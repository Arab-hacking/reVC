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

В SA колёса лежат в самом DFF: один меш `wheel` (у тестового ассета — на дочерней кости
`wheel` внутри `wheel_rf_dummy`), который игра сама клонирует на остальные три кости.
Без правки получалось: одно своё колесо (невидимое из-за п.2.1) + экземпляры чужой
VC-модели колеса на четырёх костях (или отсутствие колёс, если у машины нет wheel-модели).

Решение (окончательное, см. §11.2): для SA-иерархии VC-модель колеса подставляется
**только если в модели нет ни одного своего меша колеса**. Если меш есть,
`CloneSAWheelMeshes()` клонирует найденный меш колеса на пустые кости `wheel_*_dummy`
вместе с его матрицей (и предпочитает меш «своей» позиции — перед/середина/зад),
сохраняя рису-callback источника. Харнесс: **было 1 колесо из 4, стало 4 из 4**.

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
| `src/modelinfo/VehicleModelInfo.cpp` | определение SA-иерархии (`IsSAHierarchy`, `DetectSAHierarchyCB`); fallback-ветки в `SetAtomicRendererCB*`; SA-алиасы имён в `carIds[]`; сохранение своих колёс и `CloneSAWheelMeshes`; защита границ `m_materials1/2` и `m_comps`; nil-проверка атомика; решение «свои колёса или VC-модель» после обхода иерархии (§11.2); `SetEmbeddedColModel` (§11.1) |
| `src/modelinfo/VehicleModelInfo.h` | флаг `m_bHasEmbeddedColModel` + `SetEmbeddedColModel`; объявление `AddGenericWheelModel` |
| `src/collision/SACol3.h` | новый: парсер формата COL3 (коллизия внутри модели SA) без зависимостей от движка |
| `src/core/FileLoader.cpp` | откат к имени файла, если имя из DFF не совпало ни с одной моделью; `LoadSAVehicleColModel` + `ConvertSAColModel` (COL3 → `CColModel`), вызов из `FinishLoadClumpFile`, защита встроенной коллизии от перезаписи `.col` |
| `src/core/FileLoader.h` | объявление `LoadSAVehicleColModel` |
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
-- detection regression (SA vs vanilla VC naming) --
   VC naming (_hi/_lo/_vlo present)      SA-mode=0 (expect 0)  drawn: vanilla 2/4, SA 2/4  OK
   SA naming (no _hi at all)             SA-mode=1 (expect 1)  drawn: vanilla 0/4, SA 3/4  OK
   VC naming without _vlo                SA-mode=0 (expect 0)  drawn: vanilla 2/3, SA 2/3  OK

clump: frames=61 atomics=29
hierarchy: SA (no _hi parts at all) (SA-style=yes)
... (id костей, collapse, клонирование колёс) ...
   vanilla VC rules : 0 of 32 atomics drawn (hi detail), 31 not drawn at all, 0 destroyed as _lo
   with SA support  : 31 of 32 atomics drawn (hi detail) + 1 very-low-detail
wheels with a mesh: before=1, after SA cloning=4 (cloned 3)
RESULT: PASS
```

Первые три строки — регресс-тест на **ложное срабатывание**: на синтетических моделях
с ванильной нотацией (`_hi`/`_lo`/`_vlo`) SA-режим не включается и набор рисуемых мешей
не меняется (2 из 4 в обоих случаях), а на SA-нотации — включается.

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
   и блик (surfaceProps). Машина стоит на дороге: коллизия больше не берётся из VC-файла
   `models/coll/*.col` по индексу модели, а читается из самого DFF (см. §11.1).
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

---

## 7. Типичные ошибки сборки (Windows / premake + msbuild)

Сборка под Windows: `premake5 vs2019 --with-librw --no-full-paths` → `build/reVC.sln` →
`msbuild -m build/reVC.sln /property:Configuration=Release /property:Platform=win-amd64-librw_d3d9-oal`
(или `win-amd64-librw_gl3_glfw-oal` для OpenGL).

### 7.1. `fatal error C1083: Cannot open include file: 'shaders/obj/…inc'`

Файлы `src/extras/shaders/obj/*.inc` (25 штук) и `*.cso` (12 штук) **уже лежат в репозитории** —
это скомпилированные шейдеры (для d3d9 — HLSL, для GL — GLSL). Ошибка означает, что исходники
скопированы не полностью: папка `src/extras/shaders/obj` отсутствует.

Восстановление: `git checkout -- src/extras/shaders/obj` либо переклонировать репозиторий
(эти файлы под контролем версий, генерировать их вручную не нужно).

Если файлов нет вообще (например, вы работаете с вычищенной копией), их можно собрать самим:
* GL: `src/extras/shaders/make_glsl.sh` (вызывает `glslangValidator`), затем `makeinc_glsl.sh`;
* D3D9: `src/extras/shaders/make_hlsl.cmd` (вызывает `fxc` из DirectX SDK), затем `makeinc_hlsl.sh`.

### 7.2. `fatal error C1083: Cannot open include file: 'SAFormatConverter.h'`

`premake5.lua` подключает исходники **маской по папкам** (`files { addSrcFiles("src/rw") }`),
поэтому любой посторонний `.cpp`, положенный в `src/…`, автоматически попадает в проект и
компилируется. В апстриме (`mrxenginner/reVC`, ветка `miami`) файла `src/rw/SAFormatConverter.cpp`
**не существует** — если он у вас есть, значит в дерево добавлен сторонний файл, у которого
потерян заголовок. Действия: удалить этот `.cpp` (данный патч никаких конвертеров не требует —
SA-ассеты читаются штатным librw) либо дописать отсутствующий заголовок, после чего перегенерировать
проект (`premake5 …` заново или `cmake` при сборке через CMake).

### 7.3. Что-то ещё

Сам патч SA-моделей **не добавляет и не удаляет файлов**: правки только внутри существующих
`config.h`, `VehicleModelInfo.{h,cpp}`, `FileLoader.cpp`, `CdStream_posix.cpp`, поэтому
перегенерация проекта (premake/CMake) для него не нужна — достаточно обычной сборки.

---

## 8. Что и как проверено (журнал)

| Проверка | Результат |
|---|---|
| Разбор `admiral.dff` по формату librw (границы всех чанков) | 61 кость, 29 атомиков, 29 геометрий, 111 материалов; 0 расхождений, 22 088 вершин / 20 904 треугольника |
| Разбор `admiral.txd` | 15 текстур, platform 9 (D3D9), все A8R8G8B8, конвертация штатная |
| Логика `CVehicleModelInfo` на реальном ассете (`savtest`) | ванильно рисуется 0 из 29 мешей → с патчем 31 из 32; колёс 1 → 4; `RESULT: PASS` |
| Регресс на ложное срабатывание SA-режима (`savtest`, синтетика) | ванильная нотация не задевается: `2/4` мешей как и раньше |
| Полная сборка проекта (Linux, GCC 14, Release, GL3/GLFW/OpenAL/mpg123) | успешно, 176 целей, `src/reVC` + символы SA в бинарнике |
| Генерация проекта premake (`premake5Linux vs2019 --with-librw --no-full-paths`) | `build/reVC.sln`, `reVC.vcxproj`, `librw.vcxproj` — без ошибок |
| Include-и для конфигурации Windows D3D9 (`win-amd64-librw_d3d9-oal`) | 3571 директива `#include "…"`, **0 неразрешённых** (8 — платформенные ветки Android/PS2/Miles/EAX и системные заголовки) |
| Полнота архива vs дерево апстрима (`miami`) | все файлы на месте (0 пропущенных), 4 сабмодуля, 25 шейдерных `.inc` + 11 `.cso`, 6 Windows-DLL |
| Двухчастная загрузка SA-DFF (`vc2part`) | 14 + 15 геометрий, 29 атомиков, 0 срабатываний `assert` |
| Двухчастная загрузка SA-TXD до фикса (`vc2part`) | `numTextures=131087` → срыв чтения (FAILURE) — причина зависания (см. §10) |
| Двухчастная загрузка SA-TXD после фикса (`vc2part`) | 7 + 8 = 15 текстур, `deviceId=2`, 0 ошибок |
| Регресс VC-формата TXD (`deviceId=0`) после фикса | 15/15 текстур, поведение не изменилось |
| Полная пересборка после фикса (Linux, Release, GL3/GLFW) | успешно, 315 целей, `src/reVC` собран |
| Разбор встроенной коллизии SA (COL3) на `admiral.dff` (`sacol3_test`) | 20 сфер, 0 боксов, 26 вершин, 22 треугольника; bounds min z = 0,0, max z = 1,43, bsphere r = 2,9832 — совпало с независимым разбором |
| Негативные кейсы парсера COL3 (короткий буфер, обрезанный буфер, чужой FourCC, индекс грани вне массива) | все отвергнуты, `SACOL3 TEST: all checks passed` |
| Сборка после правок коллизии/колёс (Linux, Release, GL3/GLFW) | успешно (`src/reVC` собран) |

Проверить включения у себя: `python3 tools/sa-vehicle-verification/check_includes.py`
(или с include-папками из готового MSBuild-проекта:
`python3 tools/sa-vehicle-verification/check_includes.py --vcxproj build/reVC.vcxproj --config d3d9`).

Содержимое `tools/sa-vehicle-verification/`:
* `satest.cpp` — чтение DFF/TXD через librw (что именно лежит в файле);
* `savtest.cpp` — имитация `SetFrameIds` + `PreprocessHierarchy` + подсчёт рисуемых мешей;
* `vc2part.cpp` — прогон **двухчастных** ридеров reVC (`StartLoad…`/`FinishLoad…`) по реальному файлу;
* `out_sa_asset.txt`, `out_sa_vehicle_logic.txt` — сохранённые выводы обоих прогонов;
* `out_vc2part_txd_before_fix.txt`, `out_vc2part_txd_after_fix.txt`, `out_vc2part_dff.txt` — см. §10;
* `sacol3_test.cpp` — проверка парсера коллизии `COL3` (`src/collision/SACol3.h`) на реальном `admiral.dff` + негативные кейсы;
* `out_sacol3.txt` — сохранённый вывод этого прогона;
* `check_includes.py` — проверка полноты исходников (все локальные `#include`).

---

## 9. Готовый архив `custom.img` + `custom.dir` (для проверки в игре)

Собран упаковщиком `tools/imgtool.py` из тестовых `admiral.dff` (1 124 026 Б) и `admiral.txd`
(5 683 532 Б). Формат — «IMG v1» (как в ванильной GTA III/VC):

```
.diraname: записи по 32 байта: uint32 offset(секторы 2048), uint32 size(секторы), char name[24]
custom.img:     3 325 секторов = 6 809 600 байт
    #0  ADMIRAL.DFF  offset=0    size=549   (549*2048 = 1 124 352 Б)
    #1  ADMIRAL.TXD  offset=549  size=2776  (2776*2048 = 5 685 248 Б)
```

Совместимость с движком проверена по коду: `CDirectory::DirectoryInfo` = 32 байта,
`direntry.offset |= (imgId<<24)` при разборе и `_GET_OFFSET(a) = a & 0xFFFFFF`,
`_GET_INDEX(a) = a >> 24` при чтении (`src/core/CdStream.h`), LBA = `offset*2048`
(`lseek(..., nSectorOffset*CDSTREAM_SECTOR_SIZE)`), размер читается как `size*2048`.

### Как подключить

В `data/gta_vc.dat` (данные оригинальной игры, файл грузится последним) добавить строку:

```
CDIMAGE MODELS\CUSTOM.IMG
```

Порядок обработки образов: `CGame::Initialise` сначала добавляет `MODELS\GTA3.IMG`
(индекс 0), затем разбирает `DATA\DEFAULT.DAT`, потом `DATA\GTA_VC.DAT`. В
`CStreaming::LoadCdDirectory` образы обходятся **от последнего к первому**, а повторная
запись (`admiral.dff` уже есть в `gta3.img`) игнорируется с сообщением
«appears more than once». Значит: образ, добавленный строкой `CDIMAGE` (индекс ≥ 1),
**перекрывает** оригинальные файлы из `gta3.img`. Чтобы модель подхватилась, больше ничего
менять не нужно — `admiral` уже есть в `default.ide`, а `.txd` слот создаётся автоматически.

### Инструмент

```
python3 tools/imgtool.py pack   MODELS\CUSTOM.IMG файл1 файл2 ...   # собрать
python3 tools/imgtool.py list   MODELS\CUSTOM.IMG                  # таблица записей
python3 tools/imgtool.py unpack MODELS\CUSTOM.IMG папка            # распаковать
python3 tools/imgtool.py verify MODELS\CUSTOM.IMG файл1 файл2 ...   # сверить с оригиналами
```

Проверка этого архива: `list` — 2 записи, ошибок нет; `verify` — sha1 обоих файлов
совпадают с оригиналами; распакованные из `.img` файлы (путь «как в игре») дают тот же
результат в `tools/sa-vehicle-verification/satest`, что и исходные, `errors=0`.

---

## 10. Исправление: зависание на экране «Vice Beach» (заголовок TXD)

**Симптом.** После установки нашего SA-`admiral` через `custom.img` игра зависает (без
падения) на экране с надписью «Vice Beach» — там, где должен начаться первый ролик с
Томми на Admiral. Ванильные модели грузятся нормально.

**Причина.** Модели транспорта и большие TXD в reVC читаются **в два приёма**
(`StartLoadClumpFile`/`StartLoadTxd` → `FinishLoadClumpFile`/`FinishLoadTxd`), и заголовок
TXD при этом читался «ванильным» способом — как один `int32`:

```c
// было, src/rw/TexRead.cpp (и в RwTexDictionaryGtaStreamRead, и в ...StreamRead1)
if(RwStreamRead(stream, &numTextures, size) != size)   // size == 4
    return nil;
```

На самом деле поле — это **два `int16`**: `{ int16 numTextures; int16 deviceId; }`.
У ванильных VC-TXD `deviceId == 0`, поэтому `int32` случайно совпадал с числом текстур.
У SA-TXD `deviceId == 2` (D3D9), и старшее слово попадало в счётчик:

```
admiral.txd, байты заголовка: 0f 00 02 00
  int16 numTextures = 15, int16 deviceId = 2
  старый код читал int32 -> 131087  (0x0002000F)
```

Дальше `RwTexDictionaryGtaStreamRead1` делил это пополам (65543) и пытался прочитать
десятки тысяч текстур, пока чтение не срывалось → `StartLoadTxd` возвращал `false` → в
`CStreaming::ConvertBufferToObject` срабатывал путь ошибки:

```c
RemoveModel(streamId);
ReRequestModel(streamId);      // запрос добавляется снова
```

Так как модель машины **не грузится без TXD** (`ConvertBufferToObject` требует
`GetSlot(txdSlot)->texDict != nil`), запрос на Admiral и его TXD повторялся каждый кадр:
игра каждый кадр заново читала 5.7 МБ TXD и 1.1 МБ DFF с диска и стояла на месте — внешне
это выглядит как зависание на экране ролика.

**Проверка (до/после).** `tools/vc2part` — харнесс, который прогоняет *точно те же*
двухчастные ридеры reVC по реальному файлу (вход выровнен по 2048-байтным секторам, как
запись в IMG):

```
out_vc2part_txd_before_fix.txt   numTextures=131087 -> чтение 65543 -> FAILURE
out_vc2part_txd_after_fix.txt    numTextures=15 deviceId=2 -> 7 + 8 = 15 текстур -> SUCCESS
out_vc2part_txd_single_pass.txt  однопроходный путь (маленькие TXD): 15/15 текстур, 0 ошибок
out_vc2part_dff.txt              DFF: part1 = 14 геометрий, part2 = остальные 15 + 29 атомиков -> SUCCESS
```

DFF-путь двухчастной загрузки SA-модель проходит без изменений (все 29 атомиков,
ни одного срабатывания `assert`), ломается именно TXD.

**Что изменено.** Только `src/rw/TexRead.cpp`:

* добавлен `ReadTxdNumTextures()` — читает `int16 numTextures` + `int16 deviceId`
  (и пропускает хвост чанка, если структура больше 4 байт);
* используется и в однопроходном `RwTexDictionaryGtaStreamRead`, и в двухчастном
  `RwTexDictionaryGtaStreamRead1` (вместо `assert(size == 4)` — `assert(size >= 4)`).

Поведение для ванильных VC-TXD не меняется: при `deviceId == 0` результат совпадает со
старым (проверено копией `admiral.txd` с `deviceId = 0` — 15/15 текстур, 0 ошибок).

**Что сделать у себя.** Пересобрать проект (`premake5 vs2019` → Build) или взять
обновлённый `reVC.zip`. Проверять так: игра должна доиграть ролик с Admiral; в консоли
`reVC` пропадут повторяющиеся сообщения о загрузке `ADMIRAL.DFF`/`ADMIRAL.TXD`.

---

## 11. Коллизия и колёса SA-модели (по результатам проверки в игре)

После того как модель стала грузиться и рисоваться, обнаружились ещё две вещи, обе —
следствие того, что SA хранит часть данных **внутри** DFF, а VC — в отдельных файлах.

### 11.1. Машина висела над дорогой: коллизия берётся из модели

**Симптом.** Кузов на месте, но машина парит над асфальтом (или проваливается), столкновения
не совпадают с силуэтом.

**Причина.** В VC коллизия машины лежит отдельно — в `models/coll/*.col`, и привязывается к
модели **по индексу/имени из `default.ide`**. Когда вы заменяете `admiral.dff`, VC отдаёт
машине коллизию *своего* Admiral'а (или той модели, чьё имя занято), а она не совпадает с
геометрией SA-модели.

**Что в SA.** У SA-моделей коллизия лежит внутри DFF, в расширении клампа:

```
CLUMP
├── STRUCT / FRAMELIST / GEOMETRYLIST / ATOMICS …   ← как в VC-DFF
└── EXTENSION                                     (rwID_EXTENSION = 0x03)
    └── CollisionModel                            (Rockstar chunk 0x0253F2FA)
        └── 'COL3' блок                            (формат COL3, см. ниже)
```

Формат `COL3` (все смещения в заголовке — относительно байта после FourCC):

| Смещение | Данные |
|---|---|
| 0x00 | `'COL3'` |
| 0x04 | размер блока (не считая 8 байт заголовка) |
| 0x08 | имя файла-источника, 22 байта |
| 0x1E | id модели (`uint16`) |
| 0x20 | `TBounds`: min (12), max (12), center (12), radius (4) |
| 0x48 | `uint16` сфер, `uint16` боксов, `uint16` граней, `uint8` линий, `uint8` reserved, `uint32` flags |
| 0x54 | 6 смещений: сферы, боксы, линии, вершины, грани, плоскости |
| данные | сфера = центр (12) + радиус (4) + surface (1) + flag (1) = 20 Б; бокс = 28 Б; вершина = `int16[3]` (фикс. точка, /128); грань = `uint16 a,b,c` + material + light = 8 Б |

**Что изменено.**

* `src/collision/SACol3.h` (новый) — разбор `COL3` без зависимостей от движка: проверка
  границ всех секций, вывод указателей на сферы/боксы/вершины/грани внутрь буфера, отказ
  при индексах граней вне массива вершин. Количество вершин в формате не хранится — оно
  вычисляется как `(faces_offset - verts_offset) / 6`.
* `CFileLoader::LoadSAVehicleColModel()` (`src/core/FileLoader.cpp`) — после загрузки клампа
  (поток уже стоит сразу после атомиков) ищет `EXTENSION` → `CollisionModel`, читает блок и
  конвертирует его в родной `CColModel` (`ConvertSAColModel`): объектные сферы/боксы —
  напрямую, вершины — `CompressedVector::Set(x/128, y/128, z/128)` (совместимо и с
  `COMPRESSED_COL_VECTORS`, и без него), грани — `CColTriangle::Set`.
* Surface-идентификаторы SA шире таблицы VC (в тесте у всех 20 сфер `surface = 63`):
  всё, что выходит за `SURFACE_CONCRETE_BEACH`, отображается в `SURFACE_CAR_PANEL` — иначе
  трение и звук столкновений были бы неопределёнными.
* Коллизия ставится на модель (`CVehicleModelInfo::SetEmbeddedColModel`) и **защищается от
  перезаписи**: все три места, где reVC подхватывает `models/coll/*.col`
  (`LoadCollisionFileFirstTime`, `LoadCollisionFile(buffer)`, `LoadCollisionFile(filename)`),
  пропускают модель с `m_bHasEmbeddedColModel == true`. Загрузка `.col` по-прежнему нужна:
  она регистрирует слот ColStore и не даёт «выпасть» стримингу района.
* Модели без встроенной коллизии (обычные VC-машины, SA-конверты без `COL3`) работают как
  раньше — флаг остаётся `false`.

**Проверка.** `tools/sa-vehicle-verification/sacol3_test.cpp` (он же `out_sacol3.txt`):

```
CollisionModel chunk at 0x11234E, body 852 bytes
bounds    min (-1.0651 -2.8630 0.0000) max (1.0651 2.5238 1.4297)
bsphere   center (0.0000 -0.1696 0.7148) r 2.9832
volumes   20 spheres, 0 boxes, 26 verts, 22 faces
SACOL3 TEST: all checks passed
```

`min z = 0` — модель стоит на земле; `max z = 1.43` — высота Admiral'а. Негативные кейсы
(короткий буфер, обрезанный буфер, чужой FourCC, индекс грани вне массива) отвергаются.

### 11.2. Чужие (VC) колёса вместо своих

**Симптом.** Вместо колёс SA-модели стоят колёса ванильной VC-модели (или не хватает трёх
колёс).

**Причина.** В `PreprocessHierarchy` ветка `VEHICLE_FLAG_ADD_WHEEL` для каждой кости
`wheel_*_dummy` прикрепляла экземпляр **отдельной VC-модели колеса** (`m_wheelId` из
`default.ide`). У SA-модели свой меш колеса есть, но он лежит не на самой кости, а на
дочерней кости (у тестового `admiral.dff` — `wheel` внутри `wheel_rf_dummy`), поэтому
проверка «на кости уже есть меш» его не видела и всё равно подставляла VC-колесо; три
пустые кости `wheel_rb/lb/lf_dummy` получали только VC-колёса.

**Что изменено.**

* Обход `PreprocessHierarchy` больше не ставит колесо сразу: кости собираются в массив
  `wheelFrames[]`, а решение принимается **после** обхода, когда известно, есть ли у
  модели собственный меш колеса (рекурсивная проверка `FrameHasObject`, включая дочерние
  кости).
* Если свои колёса есть — `CloneSAWheelMeshes()` клонирует меш на пустые кости (с матрицей
  источника; первым выбирается меш «своей» позиции перед/середина/зад), рису-callback
  берётся у исходного меша (SA-иерархия получает `RenderVehicleHiDetailCB`), поэтому
  подмена на VC-колесо с его LOD не происходит.
* Если своих колёс нет вообще (SA-конверсия без меша `wheel`) — подставляется та же
  `AddGenericWheelModel()` (прежнее поведение VC), так что ванильные модели и «пустые»
  конверсии не меняются.

### Как проверить в игре

1. Заменить `models/gta3.img` (или положить рядом `models/custom.img` + `custom.dir`, см. §9)
   файлами `admiral.dff` / `admiral.txd` из SA.
2. Создать Admiral и посмотреть: машина стоит на дороге (не парит), колёса — свои, все
   четыре; столкновения совпадают с силуэтом кузова.

## 12. Радиус колеса и посадка на дорогу (исправление после первого теста в игре)

Первый игровой тест сборки из §11 показал: колёса **маленькие**, машина **парит** над
дорогой, колёса не касаются земли и машина не едет. Причина — в том, что радиус колеса в
движке VC всюду задаётся через `m_wheelScale`, а не берётся из модели.

### 12.1. Как VC считает колесо

Радиус колеса VC нигде не измеряет: колесо — отдельная модель-эталон (радиус 0.5), которая
при отрисовке масштабируется на `m_wheelScale`, поэтому **радиус = 0.5 * m_wheelScale**.
Это число попадает во всю физику:

| место | формула |
|---|---|
| `CAutomobile::SetupSuspensionLines` (Automobile.cpp:5054) | низ линии подвески `p1.z = dummyZ + lower - 0.5*S`, «верх» `p0.z = dummyZ + upper` |
| там же | `m_fHeightAboveRoad = springLen*(1-1/(4F)) - p0.z + 0.5*S` |
| там же | `m_aWheelPosition[i] = 0.5*S - m_fHeightAboveRoad` (куда ставится колесо при отрисовке) |
| `CAutomobile::PreRender` | `mat.Scale(S)` на кости колеса (15 мест) |
| `CAutomobile::SetUpWheelColModel` :4884 | сферы столкновения колёс радиусом `S/2` |
| `CAutomobile::HydraulicControl` :3413, `CAutomobile::ProcessControl` :2587 | радиус качения `0.5*S` |
| `CBike::SetupSuspensionLines` :2797, `CBike::ProcessControl` :219 | то же самое у мотоциклов |

Геометрия «посадки» получается самосогласованной только потому, что у ванильной VC-модели
кость колеса стоит **на высоте радиуса** (`dummyZ = 0.5*S`), а данные `handling.cfg`
подобраны так, что `fSuspensionLowerLimit = -springLength/(4*forceLevel)`. Второе — не
соглашение, а следствие физики: `CPhysical::ApplySpringCollisionAlt` (Physical.cpp:485)
толкает машину вверх с силой `GRAVITY*mass*forceLevel*compression*bias*2`, четыре колеса
держат вес при `compression = 1/(4*forceLevel)`, то есть статическая просадка пружины
**ровно `springLength/(4*forceLevel)`** — именно это число и зашито в формулу
`m_fHeightAboveRoad`.

### 12.2. Что именно ломалось у SA-модели

SA-модель приносит свои колёса, нарисованные в натуральную величину (у `admiral.dff`
радиус меша `r = 0.3901`, кость `wheel_rf_dummy` на `z = 0.1770`, низ модели `-0.2132`;
замер — `tools/sa-vehicle-verification/out_geodump.txt`). Но движок продолжал:

* масштабировать меш на `m_wheelScale` из `default.ide` заменяемой VC-машины (например
  0.7) → колесо рисовалось радиусом `0.3901*0.7 = 0.27` вместо `0.39` — «маленькие колёса»;
* считать радиусом `0.5*m_wheelScale = 0.35` вместо `0.39` (и ставить по нему линию
  подвески, высоту посадки `m_fHeightAboveRoad` и коллизионные сферы колёс). Кузов при
  этом оседает на `0.5*S - r` ниже правильного места (`0.35 - 0.3901 = -0.04`), а
  нарисованное колесо — меньше настоящего на `S` и висит над дорогой на
  `S*(0.5 - r) = 0.7*0.1099 ≈ 0.077` м (при `S = 0.7`), то есть «маленькие колёса, машина
  парит».

Расчёт (числа воспроизводятся в `tools/sa-vehicle-verification/sawheels_math.cpp`,
вывод — `out_sawheels_math.txt`):

```
  model / wheels                  scale rendered_r     phys_r    rest_z0   tyre_gap
  admiral.dff (SA, pre-fix)      0.7000     0.2731     0.3500     0.1730    +0.0769
  admiral.dff (SA, pre-fix)      1.0000     0.3901     0.5000     0.3230    +0.1099
  VC model + VC handling         0.7000     0.3500     0.3500     0.0000    +0.0000
```

(`tyre_gap` — насколько низ нарисованной шины висит над дорогой.)

### 12.3. Исправление

**`CVehicleModelInfo`** (`src/modelinfo/VehicleModelInfo.{h,cpp}`):

* `GetWheelMeshRadius()` — радиус меша колеса по bbox его вершин (x — вдоль оси, y/z —
  радиальные); проверяется на реальном `admiral.dff`: 0.3901.
* `CloneSAWheelMeshes()` (продолжение §11.2) теперь измеряет радиус первого найденного меша
  колеса и выставляет **`m_wheelScale = 2*r`**, плюс флаг `m_bSAWheelMesh`. Так как весь
  движок считает радиус как `0.5*m_wheelScale`, этим одним присваиванием радиус физики
  становится настоящим радиусом шины: линии подвески, `m_fHeightAboveRoad`,
  `m_aWheelPosition`, коллизионные сферы колёс, радиус качения и гидравлика.
* `GetWheelRenderScale()` — масштаб отрисовки колёс: `1.0` для моделей со своими колёсами
  (меш уже нужного размера), иначе прежний `m_wheelScale`.

**`CAutomobile::PreRender` / `CBike`** — 15 мест `mat.Scale(...)` переведены на
`wheelRenderScale`.

**`CAutomobile::SetupSuspensionLines` и `CBike::SetupSuspensionLines`** — для моделей со
своими колёсами (`HasOwnWheelMeshes()`) линия подвески строится от модели, а не от данных:

```
    sag        = springLength/(4*forceLevel)        // статическая просадка (см. 12.1)
    p0.z       = dummyZ + springLength - sag        // было dummyZ + fSuspensionUpperLimit
    p1.z       = dummyZ - 0.5*S - sag               // = низ шины модели минус sag
```

Тогда `m_fHeightAboveRoad = 0.5*S - dummyZ` — ровно та высота, на которой шины модели
стоят на дороге, а `m_aWheelPosition = dummyZ` — колесо рисуется точно на своей кости.
Для данных, подчиняющихся соотношению `lower = -springLength/(4F)` (то есть для любых
нормальных `handling.cfg`, включая VC), `p0.z` совпадает с прежним `dummyZ + upper`, так
что поведение ванильных VC-моделей не меняется вообще: ветка включается только при
`m_bSAWheelMesh`.

### 12.4. Что получается после исправления

```
  handling data (upper/lower/F)    scale rendered_r     phys_r    rest_z0   tyre_gap
  upper +0.15 lower -0.15 force 0.50:
    VC model (same data)         0.7000     0.3500     0.3500     0.0000    +0.0000
    admiral.dff (SA, fixed)      0.7802     0.3901     0.3901     0.2131    +0.0000
  upper +0.10 lower -0.20 force 0.50:
    admiral.dff (SA, fixed)      0.7802     0.3901     0.3901     0.2131    +0.0000
  upper +0.30 lower -0.10 force 0.50:
    admiral.dff (SA, fixed)      0.7802     0.3901     0.3901     0.2131    +0.0000
```

* нарисованный радиус колеса всегда равен радиусу, который использует физика (0.3901), то
  есть колесо своего настоящего размера;
* низ шины стоит ровно на дороге (`tyre_gap = 0`), просадка кузова при этом `0.2131`, а
  низ модели (`-0.2132`) оказывается на уровне дороги (расхождение 0.1 мм — округление
  bbox);
* высота посадки не зависит от `fSuspensionUpper/LowerLimit` и `forceLevel` — модель сама
  задаёт, где её колёса, данные `handling` управляют только жёсткостью и ходом подвески;
* линия контакта колёс (по ней считается `m_aWheelTimer`/`m_nWheelsOnGround`, от которого
  зависит тяга) заканчивается точно под шиной, поэтому колёса регистрируют землю и машина
  едет.

### Как проверить в игре (после правок §12)

1. Пересобрать проект (`cmake -S reVC -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DLIBRW_GL3_GFXLIB=GLFW && ninja -C build`).
2. Поставить `admiral.dff` / `admiral.txd` так же, как в прошлый раз (§9, `models/custom.img`).
3. Создать машину: она должна стоять на дороге на своих колесах нормального размера,
   проседать при торможении и нормально ехать; при разгоне колёса крутятся с правильной
   скоростью (радиус качения = радиус шины).

## 13. Коллизия из DFF (главная починка этого шага)

### Что было не так

`CAutomobile` до этого шага **никогда не получал коллизию из SA-модели**, даже
несмотря на код из §11. Причина — в двухчастном чтении моделей:

* `CFileLoader::StartLoadClumpFile` вызывает `RpClumpGtaStreamRead1`: чанки `STRUCT`,
  `FRAMELIST`, `GEOMETRYLIST` (первая половина геометрий) и запоминает позицию;
* `CFileLoader::FinishLoadClumpFile` вызывает `RpClumpGtaStreamRead2`: дочитывает оставшиеся
  геометрии и **атомики**, после чего поток стоит **внутри последнего атомика** — то
  есть там, где `RwStreamFindChunk` уже не может найти ни расширение клумпа, ни
  коллизию. Раньше поиск коллизии начинался именно с этой позиции и всегда
  проваливался, а ошибка никак не сообщалась.

Следствие: автомобиль оставался с коллизией VC из `models/coll/vehicles.col`
(она назначается модели на старте игры, до загрузки DFF). Объёмы VC-ной коллизии
рассчитаны на VC-модель, и когда машина опускается на подвеске на свою высоту
(§12), они упираются в дорогу и **держат кузов над землёй** — колёса при этом не
касаются покрытия, и машина не едет (коробка видит `m_nWheelsOnGround == 0`).

### Что сделано

* `StartLoadClumpFile` запоминает позицию и размер тела чанка `CLUMP`;
* новая функция `FindSAColChunk()` обходит чанки клумпа (с заходом внутрь
  расширений) от этой позиции и находит чанк `0x0253F2FA` (`CollisionModel`);
* `FinishLoadClumpFile` идёт по этой позиции, читает коллизию, ставит её модели
  (`CVehicleModelInfo::SetEmbeddedColModel`, который сначала удаляет прежнюю —
  VC-ную — коллизию, поэтому она и не утекает) и возвращает поток туда, где его
  ждёт потоковая система.

### Чем это проверено (без игры)

В `tools/sa-vehicle-verification/` добавлены три программы, которые запускают
**настоящий код reVC** (собираются с объектными файлами игры и headless-сборкой
librw, см. шапки файлов):

| файл | что проверяет | результат |
|------|----------------|-----------|
| `clumppos_test.cpp` | куда двухчастный читатель ставит поток | после чтения атомиков позиция внутри последнего атомика (1123126), а не перед расширением |
| `saclump_test.cpp` | полная загрузка `admiral.dff`: ветка колёс, радиус, коллизия, потоки | `ownWheelMesh=YES`, `m_wheelScale=0.7803` (радиус 0.3901), **`embedded collision: YES`** (20 сфер, 22 треугольника), у всех четырёх dummy есть свой меш колеса |
| `saphys_test.cpp` | создание `CAutomobile` (реальный `SetupSuspensionLines`) и поиск равновесия **коллизионным кодом игры** над плоской дорогой | `m_fHeightAboveRoad = 0.2132` = низ шины модели; баланс «вес = пружины» выполняется ровно на высоте 0.2132; объёмы кузова дороги не касаются ни на одной высоте |

Числа `saphys_test` для `admiral.dff` (обычные данные подвески 0.20/-0.20, force 1.25):

```
m_fHeightAboveRoad = 0.2132        (модель: центр колеса 0.1770 - радиус 0.3901)
колесо 0: m_aWheelPosition 0.1770  линия z 0.4970 .. -0.2932 (0.7901) springLen 0.4000
  z0     ratio'  F*sum(compression)  кузов касается дороги
  0.210  0.895   1.0394              0
  0.215  0.901   0.9769              0    <- равновесие
```

То есть машина останавливается там, где низ шин модели точно на дороге, и
никакая другая сила (коллизия кузова, чужая .col) её не держит.

## 14. Диагностика по флагу

Геометрия подвески и колёс зависит от того, что игра успела сделать с клумпом при
загрузке (распознана ли SA-иерархия, найдены ли `wheel_*_dummy`, есть ли у модели
свой меш колеса). Поэтому в код добавлена диагностика, включаемая переменной
окружения:

```
src/core/SavehDiag.h        -- макрос SAVEH_LOG, пишет в sa_vehicle.log (append)
```

Включается на один запуск (Windows: `set REVC_SA_LOG=1` перед `reVC.exe`).

Что попадает в `sa_vehicle.log` (в каталоге игры):

* `[SAVEH] load <модель> type <t> saHierarchy <0/1> wheelId <id> wheelFrames <n>
  ownWheelMesh <0/1> saMesh <0/1> radius <r> wheelScale <S>` — по одной строке на
  каждую загруженную модель; сразу видно, распозналась ли модель как SA и нашлись
  ли у неё свои колёса (`saMesh 0` = физика и рендер работают по ветке VC);
* `[SAVEH] suspension model <i> saMesh ... upper/lower/force/sag ... p0 ... p1 ...
  lineLen ... springLen ... h ...` + состав коллизии (`col sph/box/tri/lines`,
  `bboxz`, `embedded`);
* `[SAVEH] player z ... h ... roadZ ... gap ... wheelGap ... ratios ... wheels ...`
  каждые 32 кадра, пока игрок в машине: `wheelGap` — насколько низ шины выше дороги
  (ожидание `0.0000`), `wheels` — сколько колёс физика видит на земле (ожидание 4).

Без `REVC_SA_LOG` диагностика ничего не пишет и не открывает файлов.
