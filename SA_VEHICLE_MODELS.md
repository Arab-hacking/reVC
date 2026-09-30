# Поддержка моделей автомобилей GTA: San Andreas в reVC

Документ описывает переделку reVC, после которой игра корректно читает и отображает
модели машин из GTA: San Andreas (тестовый ассет — `admiral.dff` + `admiral.txd`).

Включается/выключается одним определением в `src/core/config.h`:

```c
#define SA_VEHICLE_MODELS              // сама поддержка SA-моделей
#define SA_VEHICLE_MODEL_NAME_FALLBACK // имя модели из имени файла, если оно не совпало с костью DFF
```

Если закомментировать `SA_VEHICLE_MODELS`, поведение полностью возвращается к ванильному
Vice City (все правки обёрнуты в `#ifdef`).

---

## 1. Что представляет собой SA-модель (факты по тестовому ассету)

### 1.1. `admiral.dff`

| Параметр | Значение |
|---|---|
| Формат | RenderWare 3.6.0.3, библиотека `0x1803FFFF` (эпоха SA; у моделей VC — 3.4.0.3) |
| Костей (Frame) | 61, имена в плагине NodeName (чанк `0x253F2FE` внутри Extension каждой кости) |
| Атомиков | 29 (`Struct` = {индекс кости, индекс геометрии, флаги, 0}, 16 байт) |
| Геометрий | 29, суммарно 22 088 вершин / 20 904 треугольника |
| Материалов | 111 (от 1 до 8 на геометрию) |
| Свет/камеры | нет |

Флаги геометрий: `0x00010077`, `0x000200F3`, `0x000200F2`, `0x00010076`.
**Старшие 16 бит — это количество наборов текстурных координат** (`flags >> 16`),
поэтому `0x000200F3` = «2 набора UV». Никаких native-геометрий (платформенных
вершинных буферов) в файле нет — данные лежат в платформо-независимом виде,
границы всех 29 чанков `STRUCT` сходятся байт-в-байт (проверено численно и через librw).

Иерархия (фрагмент, важное для движка):

```
glendale (root)
├── wheels → wheel_rf_dummy, wheel_rb_dummy, wheel_lb_dummy, wheel_lf_dummy
│               └── wheel_rf_dummy → "wheel" (единственный меш колеса!)
├── chassis_dummy → body, interior, boot → boot_stock, bonnet → bonnet_stock,
│                   bump_front → bump_front_stock, bump_rear → bump_rear_stock,
│                   windscreen → windscreen_stock, door_*_dummy → door_*_stock → door_*_gls,
│                   wipers_stock, roof_stock, exhaust_stock, skirts_stock, chassis_vlo
├── headlights, taillights, turnlights_*, ped_frontseat, ped_backseat, exhaust,
│   petrolcap, engine, migs_lights, ug_nitro, RHandPos, LHandPos, lights_front, lights_rear
```

Материалы:

* 76 текстурных / 35 без текстуры;
* **ванильные «магические» цвета VC используются**: 17 материалов с `(60,255,0)` (основной
  цвет машины) и 1 с `(255,0,175)` (вторичный) — то есть штатный механизм раскраски
  `CVehicleModelInfo::GetEditableMaterialListCB` + `carcols.dat` применим как есть;
* 32 материала с MatFX `ENVMAP` (отражения), 86 с SA-плагином `0x253F2F6`
  (float + имя текстуры `vehiclespecdot64` — блик), 105 с `0x253F2FC` (5 float);
* `surfaceProps` = (0.45, 1.0, 1.0) у 105 материалов и (1.0, 0.5, 0.499) у 6 —
  ненулевой specular, который ванильный шейдер машины VC уже использует.

### 1.2. `admiral.txd`

15 текстур, **все platform = 9 (D3D9)**, формат Raster `0x8500` (A8R8G8B8, несжатый),
полные мип-цепочки (5…10 уровней), `HAS_ALPHA` у трёх (`bmwcs_badge`, `bmwm5r_eng`,
`bmw_m5f90_badge`); `plate_remap` — 256×64. DXT-текстур в файле нет.

Штатный путь reVC (`src/fakerw/fake.cpp: rwNativeTextureHackRead` →
`rw::Texture::streamReadNative` → `rw::Raster::convertTexToCurrentPlatform`) читает такую
TXD без изменений: D3D9-растр конвертируется в растр текущей платформы (GL3/D3D9).

**Вывод раздела: librw (слой RenderWare) уже умеет читать SA-DFF/TXD. Ломается не
формат, а правила движка VC, которые применяются к содержимому файла.**

---

## 2. Что именно мешало ванильному reVC (и что исправлено)

### 2.1. Модель просто не рисовалась (`_hi` в именах)

VC определяет уровень детализации по имени кости: рисуются только атомики, в имени
которых есть `_hi` (или которые начинаются на `extra`), `_lo` удаляются, `_vlo` идёт в
режим very-low-detail, **всё остальное получает `RenderCallBack = nil`, то есть не
рисуется никогда**.

У SA-модели меши названы `body`, `interior`, `lights_front`, `windscreen_stock`,
`wheel`, … — ни одного `_hi`. Результат на ванильном reVC: у машины видна только
кости `chassis_vlo` (и то лишь на дальней дистанции), т.е. автомобиль фактически
невидим. Это подтверждено харнессом: **0 из 29 мешей получают hi-detail callback**.

Решение (`VehicleModelInfo.cpp`): при загрузке клэмпа определяется «SA-стиль иерархии»
(у модели есть атомики, но ни у одного нет `_hi`), и для таких моделей все атомики,
кроме `_lo`/`_vlo`, получают обычный hi-detail callback (`HiDetail`/`HiDetailAlpha`),
как в самой SA.

### 2.2. Колёса

В VC колесо — отдельная модель (`m_wheelId` из IDE), и `PreprocessHierarchy` создаёт её
экземпляр в каждой кости `wheel_*_dummy`, а `RenderWheelAtomicCB` подменяет колесо на
LOD-версию по дистанции. Если `m_wheelId == -1`, кости колёс **уничтожаются**.

В SA колёса лежат в самом DFF: один меш `wheel` (у тестового ассета — на `wheel_rf_dummy`),
который игра сама клонирует на остальные три кости. Без правки получалось: одно своё
колесо (невидимое из-за п.2.1) + экземпляры чужой VC-модели колеса на четырёх костях
(или отсутствие колёс, если у машины нет wheel-модели).

Решение: если у кости-колеса уже есть свой меш (SA), VC-модель колеса не подставляется;
кроме того, `CloneSAWheelMeshes()` клонирует найденный меш колеса на пустые кости
`wheel_*_dummy` вместе с его матрицей (и предпочитает меш «своей» позиции —
перед/середина/зад), сохраняя рису-callback источника. Харнесс: **было 1 колесо из 4,
стало 4 из 4**.

### 2.3. Имена dummy без `_dummy`

`carIds[]` в VC знает только `bonnet_dummy`, `boot_dummy`, `bump_front_dummy`,
`bump_rear_dummy`, `windscreen_dummy`. В SA и в конверсиях эти кости называются
`bonnet`, `boot`, `bump_front`, `bump_rear`, `windscreen` (без суффикса), иначе —
при отсутствии совпадения по имени кость не получает hierarchy id, не помечается
флагами (`FRONT/REAR/DRAWLAST/…`) и не обрабатывается как «collapse»-кость, из-за чего
капот/багажник/бампер/лобовое не анимируются штатными средствами VC.

Решение: в `carIds[]` добавлены альтернативные записи (те же flags), которые срабатывают
только если каноническая кость не найдена. Харнесс подтверждает: `bump_front → id 7`,
`bonnet → id 8`, `boot → id 17`, `bump_rear → id 18`, `windscreen → id 19`.

### 2.4. Переполнение массивов на моделях с большим числом материалов/частей

* `m_materials1[NUM_FIRST_MATERIALS=24]`, `m_materials2[20]` — у SA-машин (и особенно у
  конверсий с тюнингом) материалов с основным цветом может быть заметно больше 24;
  запись шла **без проверки границ** (UB/вылет).
* `m_comps[6]` — частей `extra*` у SA-моделей бывает больше шести.
* `RpClumpRemoveAtomic(clump, GetFirstObject(frame))` вызывался для костей без атомика
  (`GetFirstObject` → `nil`).

Решение: границы проверяются (лишние части остаются в клэмпе и рисуются), массивы
цветовых материалов под `SA_VEHICLE_MODELS` увеличены до 96/48.

### 2.5. Имя модели внутри DFF ≠ имя модели в игре

У тестового ассета корневая кость называется `glendale`, хотя файл (и слот модели) —
`admiral` (типичная ситуация для конверсий). При стриминге по model id это неважно, но
если `.dff` подгружается по имени файла (например, из `HIERFILE`), модель не находилась
и клэмп уничтожался. Добавлен откат на имя файла (`SA_VEHICLE_MODEL_NAME_FALLBACK`).

### 2.6. Сборка на современном GCC (сам проект)

`src/core/CdStream_posix.cpp` использует `#elifndef ANDROID` — в `-std=c++11` GCC 14
молча игнорирует эту директиву, из-за чего не определяются `RE3_SEM_OPEN/CLOSE` и
сборка падает. Заменено на `#elif !defined(ANDROID)` (переносимо и безвредно).

---

## 3. Что оставлено как есть (осознанные ограничения)

* **Плагины материала SA не применяются**: `0x253F2F6` (текстура блика
  `vehiclespecdot64`) и `0x253F2FC` игнорируются. Сам блик работает за счёт
  `surfaceProps.specular` из файла; «точечный» рисунок блика SA — нет.
* **Тюнинговые части** (`*_stock`, `*_gls`, `ug_nitro`, …) рисуются как обычные меши и
  не участвуют в системе тюнинга (в VC её и нет); части одного узла могут
  перекрываться. При необходимости глушатся переименованием.
* `lights_front` / `lights_rear` (меши свечения) рендерятся всегда, как и любой другой
  меш — VC-логика «включить свет» их не знает.
* Модель рисуется ванильным автомобильным пайплайном VC (`AttachVehiclePipe`), т.е.
  цвет кузова берётся из `carcols.dat` через «магические» цвета материалов (см. 1.1).

---

## 4. Список изменений в коде

| Файл | Что сделано |
|---|---|
| `src/core/config.h` | определения `SA_VEHICLE_MODELS`, `SA_VEHICLE_MODEL_NAME_FALLBACK` + комментарии |
| `src/modelinfo/VehicleModelInfo.h` | размеры `NUM_FIRST_MATERIALS`/`NUM_SECOND_MATERIALS` (96/48) под SA; объявление `CloneSAWheelMeshes` |
| `src/modelinfo/VehicleModelInfo.cpp` | определение SA-иерархии (`IsSAHierarchy`, `DetectSAHierarchyCB`); fallback-ветки в `SetAtomicRendererCB*`; SA-алиасы имён в `carIds[]`; сохранение своих колёс и `CloneSAWheelMeshes`; защита границ `m_materials1/2` и `m_comps`; nil-проверка атомика |
| `src/core/FileLoader.cpp` | откат к имени файла, если имя из DFF не совпало ни с одной моделью |
| `src/core/CdStream_posix.cpp` | `#elifndef ANDROID` → `#elif !defined(ANDROID)` (портируется на GCC 14) |

Проверка сборки (Linux, GCC 14, cmake + ninja, GL3/GLFW):

```sh
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -S reVC -B build
cmake --build build -j$(nproc)     # -> src/reVC, сборка завершается без ошибок
```

---

## 5. Как проверить работу на SA-модели

### 5.1. Быстрая проверка без игры (харнесс)

`tools/sa-vehicle-verification/savtest.cpp` подключается к librw и повторяет ровно те
правила, что и `CVehicleModelInfo` после патча:

```sh
g++ -std=c++11 -Ivendor/librw -Ivendor/librw/src \
    tools/sa-vehicle-verification/savtest.cpp <librw-null>/src/librw.a -lm -o savtest
./savtest admiral.dff
```

Ожидаемый результат (он же — приложенный `out_sa_vehicle_logic.txt`):

```
hierarchy: SA (no _hi parts at all) (SA-style=yes)
vanilla VC rules : 0 of 32 atomics drawn (hi detail), 31 not drawn at all
with SA support  : 31 of 32 atomics drawn (hi detail) + 1 very-low-detail
wheels with a mesh: before=1, after SA cloning=4 (cloned 3)
RESULT: PASS
```

Ниже — `satest.cpp` (проверка чтения самого файла): 61 кость, 29 атомиков, 29 геометрий,
111 материалов, 15 текстур TXD, 17 материалов с основным цветом машины.

### 5.2. Проверка в игре

1. Соберите проект и подготовьте игру (файлы VC: `models/gta3.img` и т.д.).
2. Любым IMG-редактором замените в `models/gta3.img` файлы `admiral.dff` и `admiral.txd`
   на SA-модель (`admiral.dff`/`admiral.txd` из этого репозитория/теста).
   Меняются именно файлы модели — IDE/handling/carcols трогать не нужно, они уже есть в VC.
3. Запустите игру и создайте Admiral (чит-код, отладочное меню reVC по `F1`, сейв или
   миссия).
4. Ожидаемое поведение: кузов/салон/стёкла/оптика на месте, **четыре** колеса (взяты из
   DFF), двери/капот/багажник открываются, поворотники/фары на местах (позиции берутся из
   dummy-костей), цвет кузова подставляется из `carcols.dat`, работает отражение (MatFX)
   и блик (surfaceProps).
5. Для контроля: с закомментированным `SA_VEHICLE_MODELS` та же операция даёт невидимую
   машину — это и есть исходная проблема.

---

## 6. Приложение: как это проверялось численно

* Полный разбор DFF/TXD (чанки, границы, материалы, плагины) — см. раздел 1; проверка
  геометрий: у всех 29 `parseEnd == structEnd`, суммарно 22 088 вершин.
* `savtest` — имитация `CClumpModelInfo::SetFrameIds` + `CVehicleModelInfo::PreprocessHierarchy`
  (включая `CollapseFramesCB`, обработку `ADD_WHEEL` и клонирование колёс) на реальном
  файле, с подсчётом того, что будет нарисовано при ванильных правилах и при SA-режиме.
* Полная сборка проекта с патчем (`src/reVC`, 176 целей, без ошибок) и проверка, что
  символы SA-режима присутствуют в бинарнике
  (`CVehicleModelInfo::CloneSAWheelMeshes`, `gVehicleIsSAHierarchy`).
