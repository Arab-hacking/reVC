# Этап 1 — аудит модуля «Персонаж, анимации, оружие»

Ветка `arena/782fc865-revc`, база `ec5b674e`, tip на момент аудита `32a1ab5`.
Работа этого этапа — **только чтение и сопоставление**. Код модуля не изменялся.
Эталон: `gta-reversed` (`/tmp/gtr`), адреса статиков — plugin-sdk (`/tmp/psdk`).

Документ — реестр того, что есть, что расходится с San Andreas и в каком порядке
это переделывать. Он же — техзадание для Блоков 0–4.

---

## 0. Резюме

Ассеты в проекте уже полностью SA (`.dff`, `.ifp`, `.txd`, `weapon.dat`,
`peds.ide`, `pedgrp.dat`, `pedstats.dat`), а **код под ними — нет**. Подсистема
осталась в состоянии «VC-логика поверх SA-данных»: переиндексация групп
анимаций сделана наполовину, оружейная таблица читает VC-формат, `CProjectile`
пуст, плавания/лазания/приседания-с-движением нет, состояний педа 64 против 65,
`eWeaponType` — VC-нумерация.

Масштаб:

| подсистема | reVC | эталон (`gta-reversed`) |
|---|---|---|
| `src/peds` | 29 файлов, 32 450 строк | `Entity/Ped` — 5 834 строки + 321 файл `Tasks/` |
| `src/animation` | 24 файла, 9 064 строки | `Animation/` — 2 264 строки (9 `.cpp`) |
| `src/weapons` | 16 файлов, 5 768 строк | `Weapon*` — 2 622 строки |
| итого модуль | 69 файлов, 47 282 строки | — |
| внешних потребителей | **90 файлов, 111 308 строк** | — |

Строк в reVC больше не потому, что функционала больше, а потому что логика
размазана по `Ped.cpp`/`PedAI.cpp`/`PedFight.cpp` без слоя задач: в SA на каждый
`CTask*` приходится отдельный файл, здесь — ветки внутри трёх «монолитов».
Отсюда главное следствие для плана: **переносить SA-код копированием нельзя**,
нужен слой состояний поверх `CPed`, и это надо делать по блокам, а не одним
коммитом.

Готово к использованию как есть (переписывать не нужно): `AnimationId.h`,
загрузчик `.ifp` (ANP2/ANP3/ANPK, сжатые кватернионы), `CPedIK` (все SA-поля,
`PitchForSlope`, `MoveLimb` с `normalize`), `CAnimBlend*` (структуры совпадают),
`CVehicleAnimGroupData` с `GetGroupForAnim`, `CPlayerPed` free-aim/lock-on/
drive-by, `CStats::GetFatAndMuscleModifier` (честная заглушка `1.0f`).

---

## 1. Методика

1. Инвентаризация `src/peds`, `src/animation`, `src/weapons`, `src/modelinfo/PedModelInfo.*`,
   `src/objects/Projectile.*`, `src/core/{Fire,Cam,Camera,PlayerInfo,Stats,FileLoader}`.
2. Пофайловое сопоставление с `/tmp/gtr/source/game_sa/{Entity/Ped,Animation,Weapon*,Models,Enums,PlayerPedData.h}`.
3. Автоматическое сравнение enum'ов: множества нормализованных имён `comm`,
   плюс явные значения через присваивание (для `AssocGroupId` — разбор
   `= expr` с разрешением ссылок на другие элементы).
4. Подсчёт употреблений по всему дереву (`grep -c`), чтобы оценить радиус
   переименований.
5. Поиск отсутствующих подсистем (`CPedIntelligence`, `CTask*`, `Ragdoll`,
   `CClothes`, swim/climb/crouch-логика).
6. Поиск мутабельных файловых `static`.

Команды проверки — в §9.

---

## 2. Реестр файлов

### 2.1 `src/peds` (29 файлов, 32 450 строк)

| файл | строк | эталон | состояние |
|---|---:|---|---|
| `Ped.h` | 1 246 | `Entity/Ped/Ped.h` (662) | VC-раскладка, 64 состояния, `VALIDATE_SIZE 0x5F4` (SA `0x79C`) |
| `Ped.cpp` | 10 235 | `Entity/Ped/Ped.cpp` (4 094) | 186 определений методов; движение/физика/`SetJump` уже SA |
| `PedAI.cpp` | 6 861 | `Entity/Ped/Ped.cpp` + `Tasks/` | вся «задачная» логика ветками |
| `PedFight.cpp` | 4 477 | `Weapon.cpp` + `Tasks/TaskSimpleFight` | бой, `InflictDamage`, drive-by |
| `PlayerPed.cpp` | 2 355 | `Entity/Ped/PlayerPed.cpp` (1 273) | управление, free-aim, lock-on |
| `Population.cpp` | 1 913 | `Entity/Ped/Population.cpp` | `pedgrp.dat` уже SA |
| `CopPed.cpp` | 874 | `Entity/Ped/CopPed.cpp` (285) | VC-поведение |
| `PedAttractor.cpp` | 802 | `Entity/Ped/PedAttractor.cpp` | ок |
| `PedIK.cpp` | 601 | `Entity/Ped/PedIK.cpp` | **SA-полный**, включая `PitchForSlope` |
| `CivilianPed.cpp` | 605 | `Entity/Ped/CivilianPed.cpp` (38) | VC-«умный» гражданский |
| `EmergencyPed.cpp` | 412 | `Entity/Ped/EmergencyPed.cpp` | ок |
| `PedType.cpp` | 388 | `PedType.cpp` (183) + `PedStats.cpp` | 22 типа / 40 стат |
| `PedDebug.cpp` | 349 | — | отладка |
| `PedChat.cpp` | 153 | — | ок |
| `Gangs.cpp` | 125 | — | ок |
| `PlayerPed.h` | 123 | `PlayerPedData.h` | нет `CPlayerPedData`, поля разложены в `CPlayerPed` |
| `PedIK.h` | 96 | — | ок |
| `PedType.h` | 181 | `PedType.h` + `PedStats.h` | VC-типы/статы |
| `Population.h` | 91 | — | ок |
| `Gangs.h` / `CopPed.h` / `EmergencyPed.h` | 60/54/42 | — | ок |
| `PedAttractor.h` | 199 | — | ок |
| `PedRoutes.*`, `PedPlacement.*`, `DummyPed.h` | 18/79/11/58/13 | — | ок |

Отсутствуют целиком: `CPedIntelligence`, `CPedGroup`/`CPedGroupPlanner`,
`CTask*` (321 `.cpp` в эталоне), `CRagdoll`, `CClothes`/`CPedClothesDesc`,
`CPedStdBonePositions` (`PedStdBonePositions.h`, 318 строк).

### 2.2 `src/animation` (24 файла, 9 064 строки)

| файл | строк | эталон | состояние |
|---|---:|---|---|
| `AnimAssocTables.h` | 3 021 | `AnimAssocAnimations.h` (1 536) | 131 массив имён клипов + 33 массива `AnimAssocDesc`; имена SA, **индексы групп VC** |
| `AnimManager.cpp` | 2 130 | `AnimManager.cpp` (937) + `AnimAssocDefinitions.cpp` (129) | загрузчик SA-готов, `animgrp.dat` из архива |
| `FrameUpdate.cpp` | 668 | — | ok |
| `RpAnimBlend.cpp` | 596 | `RpAnimBlend.cpp` (1 945) | VC-набор API |
| `AnimationId.h` | 397 | `AnimationEnums.h` | **1:1, 387 из 387** |
| `AnimBlendNode.cpp` | 318 | (141) | ок |
| `AnimBlendAssociation.cpp` | 316 | (341) | ок |
| `AnimBlendAssocGroup.cpp` | 277 | (231) | ок |
| `AnimManager.h` | 265 | — | `enum AssocGroupId` (140) + `CAnimBlock` |
| `AnimBlendSequence.cpp` | 181 | (165) | ок |
| `AnimBlendHierarchy.cpp` | 129 | (192) | ок |
| `AnimSequenceFrames.h` | 97 | — | `KeyFrame*` 0x14/0x20/0xA — SA |
| `AnimFileHeader.h` | 78 | — | ANP2/ANP3 — SA |
| `AnimBlendSequence.h` | 65 | — | ок |
| `Bones.h` / `Bones.cpp` | 81 / 63 | `eBoneTag.h` | **VC-имена, неполный enum** |
| `RpAnimBlend.h` | 51 | — | ок |
| `AnimBlendClumpData.h` | 48 | (56) | ок |
| `AnimBlendNode.h`, `AnimBlendHierarchy.h`, `AnimBlendList.h`, `AnimBlendAssocGroup.h` | 40/39/31/30 | — | ок |

### 2.3 `src/weapons` (16 файлов, 5 768 строк)

| файл | строк | эталон | состояние |
|---|---:|---|---|
| `Weapon.cpp` | 3 378 | `Weapon.cpp` (2 059) | 38 методов против 40; нет skill/HS |
| `Explosion.cpp` | 516 | (515) | VC-типы взрывов |
| `ProjectileInfo.cpp` | 437 | (115) | VC-набор снарядов, VC-границы карты |
| `BulletInfo.cpp` | 344 | (240) | ок |
| `WeaponInfo.cpp` | 330 | (456) | **VC-парсер `weapon.dat`** |
| `ShotInfo.cpp` | 149 | — | ок |
| `WeaponEffects.cpp` | 133 | (107) | **одно перекрестие, VC-раскладка** |
| `WeaponInfo.h` | 102 | (151) | VC-раскладка `0x64` против SA `0x70` |
| `Weapon.h` | 93 | — | `VALIDATE_SIZE 0x18`, у SA `0x1C` |
| `WeaponType.h` | 89 | `eWeaponType.h` | **VC-нумерация (37)** |
| `Explosion.h` | 61 | `eExplosionType.h` | 12 VC-типов |
| `ProjectileInfo.h` | 37 | `ProjectileInfo.h` | VC-поля, нет `m_pVictim`/`m_pFxSystem`/`ms_apProjectile` |
| `WeaponEffects.h` | 29 | — | `0x1C`, один объект |
| `BulletInfo.h`, `ShotInfo.h`, `BulletTravel.h` | 28/27/15 | — | ок |

Отдельно: `src/objects/Projectile.cpp` — 15 строк (только конструктор),
`Projectile.h` — 12. У SA то же самое (класс-пустышка), но вся работа лежит
в `CProjectileInfo`, а здесь `CProjectileInfo` VC-версии.

### 2.4 Смежное

| файл | строк | что важно |
|---|---:|---|
| `src/modelinfo/PedModelInfo.h/.cpp` | 76 / 366 | `PedNode` VC (18 + MAX), `m_pPedIds` VC-таблица (12 строк), hit-col 10 сфер |
| `src/modelinfo/WeaponModelInfo.h/.cpp` | 26 / 54 | `GetWeaponInfo()` без skill |
| `src/core/FileLoader.cpp:1654` | — | `LoadPedObject` — 10 полей вместо 14 |
| `src/core/PlayerInfo.h/.cpp` | 128 / 883 | нет `CPlayerPedData`, нет `m_fBreath` |
| `src/core/Stats.h/.cpp` | 184 / 1 461 | SA-статистики (305) отсутствуют |
| `src/core/Cam.cpp` / `Camera.cpp` | 5 489 / 4 203 | нет `MODE_AIMWEAPON*`, `MODE_ROCKETLAUNCHER_HS` |
| `src/renderer/Hud.cpp:234–360` | — | перекрестие VC-таблицей спрайтов |

---

## 3. Расхождения по enum'ам

### 3.1 `AnimationId` — совпадает полностью

387 явных `ANIM_ID_*` в `src/animation/AnimationId.h` против 387 в SA.
0 расхождений значений, нет «только в SA» и «только в reVC» (`ANIM_ID_MAX` —
служебный хвост эталона). **Доработки не требует.**

### 3.2 `AssocGroupId` — структурное расхождение, блокирующее

140 элементов в reVC против 142 в SA. 136 имён общих, **20 с разными
значениями**:

| имя | reVC | SA |
|---|---:|---:|
| `SHUFFLE` | 118 | 119 |
| `OLDMAN` | 119 | 120 |
| `GANG1` | 120 | 121 |
| `GANG2` | 121 | 122 |
| `OLDFATMAN` | 122 | 123 |
| `**FATMAN**` | **55** | **124** |
| `JOGGER` | 123 | 125 |
| `DRUNKMAN` | 124 | 126 |
| `BLINDMAN` | 125 | 127 |
| `SWAT` | 126 | 128 |
| `WOMAN` | 127 | 129 |
| `SHOPPING` | 128 | 130 |
| `BUSYWOMAN` | 129 | 131 |
| `SEXYWOMAN` | 130 | 132 |
| `PRO` | 131 | 133 |
| `OLDWOMAN` | 132 | 134 |
| `FATWOMAN` | 133 | 135 |
| `JOGWOMAN` | 134 | 136 |
| `OLDFATWOMAN` | 135 | 137 |
| `SKATE` | 136 | 138 |

Причина: в reVC `ASSOCGRP_FATMAN = 55` — это VC-«толстяк» из середины таблицы
игрока, а SA-шный `FATMAN` (124) стоит уже в хвосте походок. Два разных
смысла под одним именем.

Хвост SA:

```
CARS_BEGIN 88          MAN 118 (== CARS_END)
STDCARAMIMS 88         SHUFFLE 119 … SKATE 138
LOWCARAMIMS 89 …       ANIM_TOTAL_GROUPS 139
NVADAPLANEANIMS 117
CARS_END 118
```

Только в SA: `NONE = −1`, `MAN`, `FAT`, `CARS_BEGIN`, `CARS_END`, `STDCARAMIMS`.
Только в reVC: `UNARMED` (138), `PANICCHUNKY` (137), `STDCARANIMS` (88),
`LOWCARANIMS` (89 — опечатка против SA `LOWCARAMIMS`).

Дополнительно в самом enum'е reVC есть **дубль значения 89**:
`ASSOCGRP_LOWCARAMIMS = 89` и `ASSOCGRP_LOWCARANIMS = 89` стоят рядом
(`AnimManager.h:97–99`), причём первое нигде не употребляется, а второе —
употребляется (`PedAI.cpp:4703/4863`). Плюс `ASSOCGRP_STDCARANIMS` объявлен
**после** `LOWCARAMIMS`, хотя значение у него меньше (88) — таблица не
монотонна, и её больше нельзя править «по порядку».

Последствия:
* `AnimAssocTables.h` (3 021 строка) индексируется этими значениями;
* 567 употреблений `ASSOCGRP_*` в 31 файле;
* `CWeaponInfo::ms_WeaponAimOffsets` размеряется как
  `SPRAYCAN + 1 − PYTHON` (21) — от этих же значений;
* `CVehicleAnimGroupData` (новый SA-транспорт) уже ждёт SA-индексы 88…117,
  и `CARS_BEGIN`/`CARS_END` — часть контракта.

**Вывод: группы переиндексировать строго по SA. Это ломающий change для
31 файла, поэтому он идёт первым блоком, до любой логики.**

### 3.3 `eBoneTag` — неполный и с не-SA-именами

reVC `BoneTag` содержит 28 значений, SA — 303-ёмкостный диапазон:

| отсутствует в reVC | значение SA |
|---|---|
| `BONE_UNKNOWN` | −1 |
| `BONE_BROW_R` / `BONE_BROW_L` | 6 / 7 |
| `BONE_JAW` | 8 |
| `BONE_BELLY` | 201 |
| `BONE_MAX_ID` | 303 |
| `MAX_BONE_NUM` | 32 |

Имена расходятся стилем: `BONE_l_clavicle` (reVC) против
`BONE_L_CLAVICLE` (SA), `BONE_l_finger01` против `BONE_L_FINGER_01`,
`BONE_l_toe` против `BONE_L_TOE_0`. Сами значения 0…54, 301, 302 совпадают.
Переименование затронет **105 употреблений в 5 файлах**
(`Ped.cpp`, `PedAI.cpp`, `PedModelInfo.cpp`, `Bones.cpp`, `Ped.h`).

### 3.4 `PedNode` (reVC) против `ePedNode` (SA) — разные enum'ы

```
reVC PedNode (13 + MAX):        SA ePedNode (19):
PED_TORSO  0  (без кости)       NULL              0
PED_MID    1                    UPPER_TORSO       1
PED_HEAD   2                    HEAD              2
PED_UPPERARML 3 …               L_ARM 3, R_ARM 4, L_HAND 5, R_HAND 6,
PED_FOOTR  10                   L_LEG 7, R_LEG 8, L_FOOT 9, R_FOOT 10,
PED_LOWERLEGR 11                R_LOWER_LEG 11, L_LOWER_LEG 12,
PED_LOWERLEGL 12                L_LOWER_ARM 13, R_LOWER_ARM 14,
PED_FOREARML 13 …               L_CLAVICLE 15, R_CLAVICLE 16,
PED_NECK 17                     NECK 17, JAW 18, TOTAL_PED_NODES 19
```

Порядок различается начиная с 3-го элемента: **смешивать нельзя**, это не
переименование, а другой набор. Употреблений `PED_NODE_*`/`ConvertPedNode2BoneTag`
— 44.

### 3.5 `ePedPieceTypes` — другой порядок

```
reVC:  TORSO 0, MID 1, LEFTARM 2, RIGHTARM 3, LEFTLEG 4, RIGHTLEG 5, HEAD 6
SA:    LEG 0, MID 1, HEAD 2, TORSO 3, ASS 4, UPPERARM_L 5, UPPERARM_R 6,
       LEG_L 7, LEG_R 8, HEAD_COL_NODE 9
```

SA-значения — это `m_nFlags` сферы hit-col модели, то есть они зашиты в
коллизию, а не только в игровую логику. `InflictDamage` получает
`ePedPieceTypes` наружу (`Ped.h:783`, `PedFight.cpp:2339`).

### 3.6 `ePedState` — 64 против 65, нумерация сдвинута с 56-го

| reVC | SA | reVC | SA |
|---|---|---|---|
| `PED_SEEK_POS` | `SEEK_POSITION` | `PED_SUN_BATHE` | `SUNBATHE` |
| `PED_FLEE_POS` | `FLEE_POSITION` | `PED_UNKNOWN` | `HANG_OUT` |
| `PED_ROCKET_MODE` | `ROCKETLAUNCHER_MODE` | `PED_ABSEIL` | `ABSEIL_FROM_HELI` |
| `PED_MAKE_CALL` | `MAKE_PHONECALL` | `PED_DIVE_AWAY` | `EVADE_DIVE` |
| `PED_AIM_GUN` | `AIMGUN` | `PED_STATES_NO_ST` | `STATES_CAN_SHOOT` |
| `PED_SEEK_IN_BOAT` | `SEEK_BOAT_POSITION` | `PED_CARJACK` | `DIE_BY_STEALTH` |
| `PED_INVESTIGATE` | `INVESTIGATE_EVENT` | `PED_DRAG_FROM_CAR` | `CARJACK` |
| `PED_STEP_AWAY` | `EVADE_STEP` | `PED_ENTER_CAR` | `DRAGGED_FROM_CAR` |
| `PED_ANSWER_MOBILE` | `ANSWER_MOBILE` | `PED_STEAL_CAR` | `ENTER_CAR` |
| | | `PED_EXIT_CAR` | `STEAL_CAR` |
| | | `PED_HANDS_UP` | `EXIT_CAR` |
| | | `PED_ARRESTED` | `HANDS_UP` |
| | | `PED_DEPLOY_STINGER` | `ARRESTED` |

**`PED_CARJACK` в reVC = `DIE_BY_STEALTH` в SA** — значения сдвинуты на 1
начиная с 56, потому что в SA вклинился `DIE_BY_STEALTH`. Это не косметика:
состояния пишутся в сейв (`Pools.cpp` пишет `sizeof(CPlayerPed)` сырым
`memcpy`), и любая переиндексация без версии сейва даёт «педа, который
арестован вместо того, чтобы умирать».

Радиус: **925 употреблений токенов `PED_*` в 43 файлах** (вне модуля —
`Camera.cpp` 30, `World.cpp` 14, `Automobile.cpp` 10, `Vehicle.cpp` 10).

### 3.7 `ePedType` — 23 против 33

reVC: `PLAYER1 0 … PLAYER4 3, CIVMALE 4, CIVFEMALE 5, COP 6, GANG1..GANG9 7..15,
EMERGENCY 16, FIREMAN 17, CRIMINAL 18, UNUSED1 19, PROSTITUTE 20, SPECIAL 21,
UNUSED2 22, NUM_PEDTYPES 23`.

SA: `NONE −1, PLAYER1 0, PLAYER2 1, PLAYER_NETWORK 2, PLAYER_UNUSED 3,
CIVMALE 4, CIVFEMALE 5, COP 6, GANG1..GANG10 7..16, DEALER 17, MEDIC 18,
FIREMAN 19, CRIMINAL 20, BUM 21, PROSTITUTE 22, SPECIAL 23, MISSION1..8 24..31,
COUNT 32`.

Совпадают 0…15 (`PLAYER1..GANG9`), **разъезжается с 16-го**:

| id | reVC | SA |
|---:|---|---|
| 2 | `PLAYER3` | `PLAYER_NETWORK` |
| 3 | `PLAYER4` | `PLAYER_UNUSED` |
| 16 | `EMERGENCY` | `GANG10` |
| 17 | `FIREMAN` | `DEALER` |
| 18 | `CRIMINAL` | `MEDIC` |
| 19 | `UNUSED1` | `FIREMAN` |
| 20 | `PROSTITUTE` | `CRIMINAL` |
| 21 | `SPECIAL` | `BUM` |
| 22 | `UNUSED2` | `PROSTITUTE` |
| 23 | — | `SPECIAL` |

Нет в SA: `PLAYER3`, `PLAYER4`, `UNUSED1`, `UNUSED2`, `EMERGENCY`
(её роль у SA делят `MEDIC 18` и `FIREMAN 19`).
Нет в reVC: `NONE`, `PLAYER_NETWORK`, `PLAYER_UNUSED`, `GANG10`, `DEALER`,
`MEDIC`, `BUM`, `MISSION1..8`, `COUNT`.

Важно: `ped.dat` уже читается из SA-архива (`PedType.cpp:71–99`), то есть
**данные SA-шные, а enum VC-шный**. Сейчас это не падает только потому, что
`FindPedFlag` ищет тип по строке имени, а не по числу; но `m_pedType`
модели, `PED_FLAG_*` и `CPedType::ms_apPedType` проиндексированы VC-числами,
и 20 из 23 слотов означают не то, что написано.

### 3.8 `ePedStats` — 40 против 43 (−1)

reVC (`PEDSTAT_*`, `0x00`): `PLAYER 0, COP 1, MEDIC 2, FIREMAN 3,
GANG1..GANG7 4..10, STREET_GUY 11 … COWARD 39` — 40 значений.

SA (`ePedStats`, `NONE = −1`): те же первые 11, затем
`GANG8 11, GANG9 12, GANG10 13`, и только потом `STREET_GUY 14 … COWARD 42`
— 43 значения.

То есть **всё, что после `GANG7`, в reVC стоит на 3 позиции выше**, чем в
SA: `STREET_GUY` 11 против 14, `COWARD` 39 против 42. Ошибка не «почти
незаметная»: `STREET_GUY` попадает на `GANG8`, а таблица `pedstats.dat`
из SA-архива уже заполнена по SA-порядку — значит, модели с `STREET_GUY`
сейчас получают характер `GANG8`.

Радиус — `m_pedStatType` у каждой модели + `CPedStats::ms_apPedStats`.

Кроме enum'а расходится и загрузчик: SA читает **11 полей**
(`name fleeDistance headingChangeRate fear temper lawfulness sexiness
attackStrength defendWeakness shootingRate defaultDecisionMaker`) в
`CPedStat` размером `0x34` с полями `m_nDefaultDecisionMaker` и
`m_flags`, а reVC-структура (`CPedStats`, `0x34`) этих двух полей
не имеет и `shootingRate` не читает.

### 3.9 `eWeaponType` — полностью другая нумерация (37 против 47)

**Арифметическая переиндексация недопустима**: множества оружия не
пересекаются по порядку.

| reVC (`WEAPONTYPE_*`) | id | SA (`WEAPON_*`) | id |
|---|---:|---|---:|
| `UNARMED` | 0 | `UNARMED` | 0 |
| `BRASSKNUCKLE` | 1 | `BRASSKNUCKLE` | 1 |
| `SCREWDRIVER` | 2 | — | — |
| `GOLFCLUB` | 3 | `GOLFCLUB` | 2 |
| `NIGHTSTICK` | 4 | `NIGHTSTICK` | 3 |
| `KNIFE` | 5 | `KNIFE` | 4 |
| `BASEBALLBAT` | 6 | `BASEBALLBAT` | 5 |
| `HAMMER`, `CLEAVER` | 7, 8 | — | — |
| `MACHETE` | 9 | — | — |
| `KATANA` | 10 | `KATANA` | 8 |
| `CHAINSAW` | 11 | `CHAINSAW` | 9 |
| `GRENADE` | 12 | `GRENADE` | 16 |
| `DETONATOR_GRENADE` | 13 | `REMOTE_SATCHEL_CHARGE` | 39 |
| `TEARGAS` | 14 | `TEARGAS` | 17 |
| `MOLOTOV` | 15 | `MOLOTOV` | 18 |
| `ROCKET` | 16 | `ROCKET` | 19 |
| `COLT45` | 17 | `PISTOL` | 22 |
| `PYTHON` | 18 | `DESERT_EAGLE` | 24 |
| `SHOTGUN` | 19 | `SHOTGUN` | 25 |
| `SPAS12_SHOTGUN` | 20 | `SPAS12_SHOTGUN` | 27 |
| `STUBBY_SHOTGUN` | 21 | `SAWNOFF_SHOTGUN` | 26 |
| `TEC9` | 22 | `TEC9` | 32 |
| `UZI` | 23 | `MICRO_UZI` | 28 |
| `SILENCED_INGRAM` | 24 | `PISTOL_SILENCED` | 23 |
| `MP5` | 25 | `MP5` | 29 |
| `M4` | 26 | `M4` | 31 |
| `RUGER` | 27 | `AK47` | 30 |
| `SNIPERRIFLE` | 28 | `SNIPERRIFLE` | 34 |
| `LASERSCOPE` | 29 | — | — |
| `ROCKETLAUNCHER` | 30 | `RLAUNCHER` | 35 |
| `FLAMETHROWER` | 31 | `FLAMETHROWER` | 37 |
| `M60` | 32 | — | — |
| `MINIGUN` | 33 | `MINIGUN` | 38 |
| `DETONATOR` | 34 | `DETONATOR` | 40 |
| `HELICANNON` | 35 | — | — |
| `CAMERA` | 36 | `CAMERA` | 43 |

Только в SA: `SHOVEL 6`, `POOL_CUE 7`, `DILDO1/2 10/11`, `VIBE1/2 12/13`,
`FLOWERS 14`, `CANE 15`, `ROCKET_HS 20`, `FREEFALL_BOMB 21`,
`COUNTRYRIFLE 33`, `RLAUNCHER_HS 36`, `SPRAYCAN 41`, `EXTINGUISHER 42`,
`NIGHTVISION 44`, `INFRARED 45`, `PARACHUTE 46`.
Служебные 47…58 (`ARMOUR … FLARE`) почти совпадают по смыслу с reVC 37…48,
но у SA сдвинуты на 10.

Радиус: **915 употреблений `WEAPONTYPE_*` в 39 файлах.**

### 3.10 Слоты оружия — 10 против 13, другой порядок

```
reVC:  UNARMED 0, MELEE 1, PROJECTILE 2, HANDGUN 3, SHOTGUN 4, SMG 5,
       RIFLE 6, HEAVY 7, SNIPER 8, OTHER 9, TOTAL_WEAPON_SLOTS 10
SA:    UNARMED 0, MELEE 1, HANDGUN 2, SHOTGUN 3, SMG 4, RIFLE 5,
       SNIPER 6, HEAVY 7, THROWN 8, SPECIAL 9, GIFT 10, PARACHUTE 11,
       DETONATOR 12, NUM_WEAPON_SLOTS 13
```

`TOTAL_WEAPON_SLOTS` употребляется в `Pools.cpp`, `Pickups.cpp`, `Replay.cpp`,
`Ped.cpp` (5 файлов). Массив `CPed::m_weapons[TOTAL_WEAPON_SLOTS]` расширяется —
это меняет `sizeof(CPed)` и требует согласования с сейвами.

### 3.11 `eWeaponFlags` — уже SA

`eWeaponFlags` (18 флагов, `CANAIM 0x1 … EXPANDS 0x40000`) уже выписан по SA.
Но `WeaponInfo.h` использует **другую, VC-шную** нумерацию
(`WEAPONFLAG_USE_GRAVITY = 1 … WEAPONFLAG_ANIMDETONATE = 1<<21`), и это
не просто сдвиг — наборы разные. Плюс `m_Flags` — `uint32` вместо union
битовых полей SA.

### 3.12 Прочее

| enum | reVC | SA | комментарий |
|---|---|---|---|
| `eMoveState` | 7: `NONE, STILL, WALK, JOG, RUN, SPRINT, THROWN` | 8: `NONE, STILL, TURN_L, TURN_R, WALK, JOG, RUN, SPRINT` | SA не имеет `THROWN`, зато имеет `TURN_L/R`; значения 5..7 сдвинуты |
| `eWeaponFire` | 5 | 6 | SA добавляет `WEAPON_FIRE_USE` |
| `eExplosionType` | 12 VC (`GRENADE, MOLOTOV, ROCKET, CAR, CAR_QUICK, BOAT, HELI, HELI2, MINE, BARREL, TANK_GRENADE, HELI_BOMB`) | 13 (`UNDEFINED -1, GRENADE, MOLOTOV, ROCKET, WEAK_ROCKET, CAR, QUICK_CAR, BOAT, AIRCRAFT, MINE, OBJECT, TANK_FIRE, SMALL, RC_VEHICLE`) | полная замена |
| `eCamMode` | 47 | 60 | нет `MODE_AIMWEAPON`, `_ATTACHED`, `_FROMCAR`, `_ATTACHCAM`, `MODE_ROCKETLAUNCHER_HS`, `_RUNABOUT_HS`, 9 `MODE_DW_*`, 4 `MODE_TWOPLAYER*` |
| `eMeleeCombo` | отсутствует | `IDLE 0 … FLOWERS 0xE, KICK_STD 0xF, PISTOL_WHIP 0x10, NUM 0x11` | нужен для `weapon.dat`-строк `case 163` |
| `eWeaponSkill` | отсутствует | `POOR, STD, PRO, COP` | основа skill-таблицы |
| `ePedRace` | отсутствует | `DEFAULT 0, BLACK 'B', WHITE 'W', ORIENTAL 'O'/'I', HISPANIC 'H'` | нужно `peds.ide` |
| `eAudioPedType` | отсутствует | `GEN 0, EMG 1, PLAYER 2, GANG 3, GFD 4, SPC 5` | нужно `peds.ide` |
| `eRadioID` | отсутствует (только `int8 radio1/radio2`) | `INVALID −1, EMERGENCY_AA … OFF, COUNT` | нужно `peds.ide` |

---

## 4. Ключевые дефекты по подсистемам

### 4.1 Кости и модели педов

1. **`Bones.h` неполон** (§3.3). Плюс `ConvertBoneTag2BoneName` отдаёт
   `"L Toe0"`/`"R Toe0"`, а SA — `"L Toe"`/`"R Toe"`; для `BONE_JAW`,
   `BONE_BROW_*`, `BONE_BELLY` имён нет вообще.
2. **`CPedModelInfo::m_pPedIds` — VC-таблица на 12 строк** (11 реальных
   `Smid/Shead/Supperarml/…/Slowerlegr` + завершающий `nil`), объявленная как
   `m_pPedIds[PED_NODE_MAX]` (18) — то есть 7 строк нулевые, а `PED_TORSO`,
   `PED_FOREARML/R`, `PED_CLAVICLEL/R`, `PED_NECK` кости не получают вообще.
   SA —
   `NUM_PED_NAME_ID_ASSOC = 13` (адрес `0x8A6268`) с именами SA-скелета
   (`Bip01 Pelvis`, `Bip01 Spine`, …). Механизм
   `CClumpModelInfo::SetFrameIds` (`ClumpModelInfo.cpp:127`) и
   `DetectSAAnimationClump` (маркеры `Normal` / `Bip01 Pelvis` / `Bip01 Spine`,
   версия ≥ `0x36000`, SA-эксклюзивные boneTag 6/7/8/201/301/302) уже есть —
   переписывать надо только таблицу имена→узел.
3. **Hit-col: 10 сфер против 12.** reVC `ColNodeInfo{name, pedNode, pieceType, x, z, radius}`
   с центром `(x, 0, z)` поверх позиции кости; SA
   `tPedColNodeInfo{m_vecCenter, m_fRadius, m_nBoneID, m_nFlags}` (адрес
   `0x8A6308`), `sphere.m_nPiece = nodeInfo.m_nFlags`, bound sphere 1.5 и
   box `±0.5, ±0.5, ±1.2`. Все три метода (`CreateHitColModelSkinned`,
   `AnimatePedColModelSkinned`, `…World`) уже с гардами «кости нет» — менять
   только данные и раскладку.
4. **`LoadPedObject` читает 10 полей вместо 14.**
   reVC: `%d %s %s %s %s %s %x %s %d %d` → `id model txd pedType pedStats animGroup carsCanDrive animFile radio1 radio2`.
   SA: `%d %s %s %s %s %s %x %x %11s %d %d %s %s %s` → плюс `flags`,
   `pedVoiceType`, `voiceMin`, `voiceMax`. Отсюда нет `m_nPedFlags`,
   `m_nRace`, `m_nPedAudioType`, `m_nVoiceMin/Max/Id`, и `radio` не
   смещается (`eRadioID(radio + 1)`).
5. **`groupAliases` в `FileLoader.cpp:1685` и `SafePedAnimGroup` в
   `CustomWorld.cpp:456`** — костыли под VC-имена (`man`, `van`, `coach`,
   `jogman`, `skater`) на фоне SA-таблицы групп. После переиндексации по SA
   имена `MAN`/`VAN`/`COACH` становятся штатными, и фолбэки надо **убирать**,
   а не расширять.
6. Нет `CClothes`/`CPedClothesDesc` — модульной внешности SA нет вообще.

### 4.2 Анимации

1. **`AssocGroupId` VC-нумерация** (§3.2) — единственное, что мешает уже
   готовому загрузчику. `AnimManager::ReadAnimAssociationDefinitions`
   (`:1380`) заполняет `ms_aAnimAssocDefinitions` по `data/animgrp.dat` из
   архива, `CreateAnimAssocGroups` строит `CAnimBlendAssocGroup` — всё это
   работает, но адресация по VC.
2. **`AnimAssocTables.h` (3 021 строка)** — имена клипов уже SA, флаги SA
   (`ANIMATION_*`, 17 штук), но таблица проиндексирована VC-значениями;
   после §3.2 нужна сверка каждой группы с `AnimAssocAnimations.h` эталона.
   Состав: **131** массив `a<Group>Animations[]` против **112** у эталона и
   **33** массива `a<Group>Descs[]`; сами определения
   (`ms_aAnimAssocDefinitions`, 140 строк) лежат в reVC в `AnimManager.cpp:31`,
   у эталона — отдельным `AnimAssocDefinitions.cpp` (129 строк).
   reVC-таблица несёт лишнее поле `numDescs` (у эталона его нет), потому что
   часть SA-групп перечисляет больше имён клипов, чем дескрипторов — при
   сверке это надо сохранить, иначе группа получит чужие флаги.
3. **`AnimAssocDefinition`** в reVC получил `numDescs` (у SA это отдельное
   поле `totalAnimations`/`numAnims`) — структура совместима, но проверять
   надо по каждой группе.
4. **`RpAnimBlend.cpp`** — 596 строк против 1 945: нет
   `RpAnimBlendClumpGetData`-API уровня SA, `ClumpExtract/GiveAssociations`,
   `SetBlendDeltas`-обвязки, `ClumpFillFrameArraySkin` (есть частично).
5. **Плавание реализовано нулевым образом**: `ANIM_ID_SWIM_BREAST`,
   `SWIM_CRAWL`, `SWIM_DIVE_UNDER`, `SWIM_GLIDE`, `SWIM_JUMPOUT`,
   `SWIM_TREAD`, `SWIM_UNDER`, `DROWN` **объявлены, но не используются
   нигде**. `PreRenderAfterTest` (`Ped.cpp:5119`) содержит комментарий-задел
   («swimming and jetpack tasks … do not exist in this fork»).
6. **IK**: `CPedIK` — SA-полный (`m_fSlopePitch`, `m_fSlopePitchLimitMult`,
   `m_fSlopeRoll`, `m_fBodyRoll`, `PitchForSlope`, `SLOPE_PITCH`, `MoveLimb`
   с `normalize`). Не хватает только потребителя — `CTaskSimpleUseGun`-цепи;
   сейчас IK дёргается вручную из `Ped.cpp:1391–1409`.

### 4.3 Логика педа

1. **Состояния** — см. §3.6. Нет ни одного из swim-состояний SA (их и в
   SA-enum'е нет — плавание живёт в задачах, а не в состоянии, — но
   `PEDSTATE_*` сдвинут).
2. **`CStats`** — в reVC только счётчики VC-эпохи + три селектора
   `eStatModAbilities`. SA-массива из 305 статистик нет, поэтому
   `GetFatAndMuscleModifier` возвращает `1.0f` (это честно задокументировано
   в `Stats.h:166`).
3. **`m_fBreath` отсутствует**: у SA это `CPlayerPedData::m_fBreath`,
   инициализируется `CStats::GetFatAndMuscleModifier(STAT_MOD_AIR_IN_LUNG)`
   (`PlayerPed.cpp:715`), тратится при `SWIM_DIVE_UNDERWATER`, восстанавливается
   на поверхности, при 0 — `WEAPONTYPE_DROWNING`. В reVC есть только два
   бита `bIsDrowning`/`bDrownsInWater` (`Ped.h:482`) и готовая ветка урона
   `Ped.cpp:1827/1851` — состояния дыхания нет.
4. **Спринт-энергия разъехалась по двум классам**: `CPlayerPed::m_fStaminaProgress`
   (`PlayerPed.cpp:225 UseSprintEnergy`, `:625 RestoreSprintEnergy`) против
   SA `CPlayerPedData::m_fTimeCanRun` + `m_bPlayerSprintDisabled`. Работает,
   но не там, где SA.
5. **Crouch — только флаги**: `bCrouchWhenShooting`, `bCrouchWhenScared`,
   `bIsDucking`, `bNotAllowedToDuck`; движение в приседе и стрельба из
   приседа не реализованы (`ANIM_ID_CROUCHFIRE`, `CROUCHRELOAD`,
   `CROUCH_ROLL_L/R` объявлены и не используются).
6. **Паркур/лазание отсутствует**: `SetJump(highJump, climbJump)` и
   `ApplyLaunchForce` (`Ped.cpp:9662/9726`) — SA-логика прыжка, включая
   `ANIM_ID_CLIMB_JUMP`; но `TaskSimpleClimb`/`TaskComplexClimb`
   (738 строк) — нет, как и `ANIM_ID_CLIMB_IDLE/PULL/STAND/STAND_FINISH`.
7. **`eMoveState`** — 7 значений против 8, `THROWN` вместо `TURN_L/R` (§3.12).
8. **Нет `CPedIntelligence`** (`m_vehicleScanner`, `m_pedScanner`,
   `m_fSeeingRange` — комментарий в `Ped.h:590`), нет `CPedGroup`,
   нет 321 файла `Tasks/`. Всё поведение — ветки в `PedAI.cpp`.
9. **`m_wepSkills` — сырое VC-число** (`Ped.h:623`, `uint8`, `TODO`):
   `CopPed` пишет 208/176/32. API SA (`GetWeaponSkill()` /
   `GetWeaponSkill(eWeaponType)` / `SetWeaponSkill(wt, skill)` /
   `CStats::GetStatValue(GetSkillStatIndex(wt))` / `m_nReqStatLevel`
   POOR/PRO / спец-правило `PISTOL + COP → COP`) отсутствует.
10. **`InflictDamage`** (`PedFight.cpp:2900`) — VC-версия с
    `m_pedStats->m_defendWeakness`, но SA-ветки смерти уже есть; зависит от
    `ePedPieceTypes` (§3.5) и `CWeapon::GenerateDamageEvent`, которого нет.

### 4.4 Оружие, баллистика, прицеливание

1. **Парсер `weapon.dat` — VC-формат** (`WeaponInfo.cpp:155 LoadWeaponData`):
   один `sscanf` на 26 полей, без skill-индексации, без `%`-строки
   (aim offsets), без melee-секции (`case 163`), без `case '$'/'%'`
   ветвления. SA: строка `$` — 26 полей
   (`%*s %s %s %f %f %d %d %d %s %d %d %f %f %f %d %d %f %f %d %d %d %d %d %d %d %x %f %f %f %f`,
   минимум 25), `%` — 9 полей
   (`%*s %s %f %f %f %f %d %d %d %d`, ключ —
   `CAnimManager::GetAnimationGroupIdByName(name) - ANIM_GROUP_PYTHON`),
   melee — 11 полей. Плюс `SetAnimLoopInfos`: `end = start + floor(0.1f + (end-start)*50)/50 - 0.006f`
   — в reVC этого округления нет.
2. **`CWeaponInfo` — VC-раскладка `0x64`** против SA `0x70`: нет
   `m_fTargetRange` отдельно от `m_fWeaponRange`, нет `m_nSkillLevel`,
   `m_nReqStatLevel`, `m_fAccuracy`, `m_fMoveSpeed`, `m_nBaseCombo`,
   `m_nNumCombos`; флаги — своя нумерация (§3.11); нет
   `GetWeaponInfo(wt, skill)` и таблицы на 80 записей
   (`[STD 0–47][POOR 47–57][PRO 58–68][COP 69–79]`, адрес `0xC8AAB8`).
3. **`CWeapon` — `0x18` против `0x1C`**: нет `m_IsFirstPersonWeaponModeSelected`,
   `m_DontPlaceInHand` (ПНВ/тепловизор), `m_FxSystem` (огнемёт/краскопульт/
   огнетушитель). Плюс `CWeapon` не знает владельца — `Initialise(type, ammo)`
   без `CPed*`, тогда как SA `Initialise(type, ammo, owner)`.
4. **Нет методов**: `IsTypeProjectile`, `GetProjectileType`,
   `GetWeaponRange`, `TargetWeaponRangeMultiplier`, `DoWeaponEffect`,
   `StopWeaponEffect`, `SetUpPelletCol`, `GenerateDamageEvent`,
   `EvaluateTargetForHeatSeekingMissile`, `FireInstantHitFromCar2`,
   `CanBeUsedFor2Player`, `TypeHasSkillStats`, `TypeIsWeapon`,
   `GetSkillStatIndex`.
5. **`CWeaponEffects` — VC-раскладка `0x1C`**: один объект, `m_bActive`,
   `m_vecPos`, 4×`uint8` цвета, `m_fSize`, `m_fRotation`. SA — **массив из
   2 перекрестий** (`MAX_NUM_WEAPON_CROSSHAIRS = 2`, размер `0x2C`):
   `m_nTimeWhenToDeactivate`, `CRGBA m_color`, `field_1C/20`,
   `m_bClearImmediately`; API `IsLockedOn`, `MarkTarget(id, …)`,
   `ClearCrossHair(s)`, `ClearCrossHair(s)Immediately`; текстуры
   `gpCrossHairTex` (`0xC8A818`), `gpCrossHairTexFlight[2]` (`0xC8A810`),
   массив `gCrossHair` (`0xC8A838`),
   `eWeaponEffectsLockTexture{LOCK_ON, LOCK_ON_FIRE}`.
6. **`CProjectile` пуст** — 15 строк, только конструктор. SA-снаряды
   `GRENADE/TEARGAS/MOLOTOV/ROCKET/ROCKET_HS/FREEFALL_BOMB` + Satchel/Detonator
   в `CProjectileInfo` не реализованы; в reVC `AddProjectile` (`ProjectileInfo.cpp:74`)
   знает только `ROCKET/MOLOTOV/TEARGAS/GRENADE/DETONATOR_GRENADE`.
   `CProjectileInfo` в reVC — VC-поля (`m_eWeaponType, m_pSource,
   m_nExplosionTime, m_bInUse, m_vecPos`); у SA — `m_nWeaponType, m_pCreator,
   m_pVictim, m_nDestroyTime, m_bActive, m_vecLastPosn, m_pFxSystem`
   и второй массив `ms_apProjectile[32]` (`0xC89110`).
   Границы карты — VC: `PROJECTILE_BOUNDARY_{MIN,MAX}_{X,Y}` =
   `−2390/1590/−1990/1990` (`ProjectileInfo.cpp:23–26`), SA-карта больше.
7. **`eExplosionType`** — VC-набор (§3.12), нет `WEAK_ROCKET`,
   `AIRCRAFT`, `OBJECT`, `TANK_FIRE`, `SMALL`, `RC_VEHICLE`.
8. **Прицеливание**: `CPlayerPed` уже имеет free-aim (`m_bFreeAimActive`,
   под `FREE_CAM`), lock-on (`FindWeaponLockOnTarget`,
   `FindNextWeaponLockOnTarget`, `EvaluateNeighbouringTarget`,
   `EvaluateTarget`), `SetWeaponLockOnTarget`, `CanIKReachThisTarget`,
   `RotatePlayerToTrackTarget`, `DoWeaponSmoothSpray`, drive-by
   (`PedFight.cpp:4380`). **Чего нет**: камеры прицеливания SA
   (`MODE_AIMWEAPON`, `MODE_AIMWEAPON_ATTACHED`, `MODE_AIMWEAPON_FROMCAR`,
   `MODE_ATTACHCAM`, HS-варианты) — `Cam.cpp:201` держит
   `// case MODE_AIMING:` закомментированным; нет цвета/размера индикатора
   lock-on по HP цели (`CWeaponEffects` §4.4.5); нет strafe-анимаций
   (`CanStrafeOrMouseControl`, `Ped.cpp:4102` — есть, но VC-список
   состояний).
9. **Первое лицо**: `MODE_SNIPER`, `MODE_ROCKETLAUNCHER`, `MODE_M16_1STPERSON`,
   `MODE_CAMERA` есть, ПНВ/тепловизор (`NIGHTVISION`/`INFRARED`) — нет.

### 4.5 Прицел/HUD

`Hud.cpp:234–360` рисует перекрестие по `CCam::MODE_*` и по VC-id
оружия (`WEAPONTYPE_COLT45 … WEAPONTYPE_RUGER`, `M60`, `MINIGUN`,
`FLAMETHROWER`, `LASERSCOPE`, `CAMERA`) спрайтами `HUD_SITEM16`,
`HUD_SITESNIPER`, `HUD_SITELASER`, `HUD_VIEWFINDER`, `gpRocketSightTex`.
После §3.9 эта таблица обязана переехать на SA-id и на
`CWeaponEffects`/`eWeaponEffectsLockTexture`.

---

## 5. Внешние потребители (правило «одной строки»)

90 файлов, 111 308 строк, вне `src/peds|animation|weapons`. По каталогам:

| каталог | файлов |
|---|---:|
| `core` | 23 |
| `control` | 18 |
| `vehicles` | 17 |
| `renderer` | 8 |
| `entities` | 7 |
| `audio` | 7 |
| `modelinfo` | 3 |
| `extras` | 3 |
| `save` | 2 |
| `objects` | 2 |
| `collision` | 2 |
| `math` | 1 |

По насыщенности — число **строк** с упоминанием
`CPed|CPlayerPed|CWeapon|CAnimBlend|AnimationId|eWeaponType|WEAPONTYPE_|BONE_|ASSOCGRP_|ANIM_ID_`
(`grep -c`, не число вхождений):

| файл | всего строк | строк с упоминанием |
|---|---:|---:|
| `audio/AudioLogic.cpp` | 10 465 | 186 |
| `audio/AudioManager.h` | 667 | 100 |
| `vehicles/Vehicle.cpp` | 4 024 | 87 |
| `core/Cam.cpp` | 5 489 | 85 |
| `vehicles/VehicleAnimGroupData.cpp` | 391 | 79 |
| `control/Replay.cpp` | 1 926 | 76 |
| `vehicles/Bike.cpp` | 2 819 | 66 |
| `vehicles/Automobile.cpp` | 6 294 | 61 |
| `extras/custom/MobileTouchControls.cpp` | 3 317 | 58 |
| `vehicles/CarEnterExit.cpp` | 991 | 49 |
| `core/Pad.cpp` | 3 983 | 40 |
| `control/Pickups.cpp` | 1 773 | 38 |
| `control/Darkel.cpp` | 439 | 31 |
| `extras/custom/CustomWorld.cpp` | 1 171 | 22 |
| `core/Pools.cpp` | 749 | 21 |
| `vehicles/Vehicle.h`, `core/Camera.cpp`, `control/SceneEdit.cpp` | 1 015 / 4 203 / 1 127 | 20 каждое |
| `vehicles/CarEnterExit.h`, `core/World.cpp`, `core/AnimViewer.cpp` | 88/2 240/433 | 19 |
| `renderer/Hud.cpp`, `modelinfo/PedModelInfo.cpp` | 2 153/366 | 16 |
| `renderer/SpecialFX.cpp`, `core/Fire.cpp` | 1 452/500 | 14 |
| `vehicles/Boat.cpp`, `control/Pickups.h` | 1 706/173 | 13 |
| `control/Script.cpp`, `vehicles/Bmx.cpp`, `entities/Physical.cpp`, `control/Phones.cpp` | 797/342/2 318/443 | 12/11/11/11 |
| `control/CarCtrl.cpp` | 3 435 | 10 |
| `core/PlayerInfo.cpp`, `core/Game.cpp`, `control/Garages.cpp` | 883/1 486/2 744 | 8 каждое |
| остальные 55 файлов | — | 1…7 |

Отдельно — **транспорт, уже переделанный под SA** и обязанный остаться
консистентным: `vehicles/CarEnterExit.{h,cpp}` (49), `vehicles/VehicleAnimGroupData.{h,cpp}`
(79+4), `vehicles/{Automobile,Bike,Boat,Bmx,QuadBike,Train}`,
`vehicles/RideAnimData.h`. Они потребляют `AssocGroupId`/`AnimationId`
напрямую, поэтому переиндексация по §3.2 их задевает механически, а не
логически — но задевает.

Скриптовый слой (`control/Script.cpp`, 797 строк) — **не интерпретатор**:
`ProcessOneCommand` (`:212`) умеет только `ProcessWeatherCommands`,
остальные опкоды логируются как «недоступны». То есть «опкоды педов»
приводить к SA не к чему — слоя нет.

---

## 6. Мутабельные файловые `static`

`grep` по `src/peds|animation|weapons` даёт **61** строку, начинающуюся с
`static`; из них 25 — объявления `static`-переменных внутри функций и на
уровне файла (остальное — `static`-функции и `static const`-таблицы, их
трогать не нужно).

Разбивка 25 объявлений: **8** — состояние, **7** — константы, **8** —
отладочные кэши, **1** — мёртвый код, **1** — ложный случай.

**Настоящее состояние, живущее дольше вызова и обязанное переехать в
экземпляр (8 переменных в 6 строках):**

| файл | строка | переменная | куда |
|---|---:|---|---|
| `peds/Ped.cpp` | 2766 | `cancelJack` | флаг `CPed` |
| `peds/Ped.cpp` | 6658 | `vecNextPathNode` | поле `CPed` |
| `peds/PlayerPed.cpp` | 1414–1415 | `changedHeadingRate`, `pointedGun` | поля `CPlayerPed` |
| `peds/PlayerPed.cpp` | 2092–2093 | `lastTime`, `lastAnim` | поля `CPlayerPed` |
| `weapons/Weapon.cpp` | 1121 | `counter` (деление темпа MINIGUN/HELICANNON) | поле `CWeapon` |
| `animation/AnimManager.cpp` | 176 | `s_dataDefinedGroup[142]` | `const`-таблица либо поле `CAnimManager` |

**VC-синглтон, подлежащий замене массивом:**
`weapons/WeaponEffects.cpp:13` — `CWeaponEffects gCrossHair;` (один объект
на всю игру). SA держит `gCrossHair[2]` (`0xC8A838`).

**Константы, которые просто не помечены `const` (7):**
`Ped.cpp:3337–3338` (`stepStartSection`, `animSections`),
`PedAI.cpp:3939` (`mult`), `PedFight.cpp:1553` (`streakDelay`),
`PedFight.cpp:2347–2348` (`particleRightLen`, `particleUpLen`),
`PlayerPed.cpp:274` (`moveAnims[]` — только читается).

**Отладочные кэши «чтобы не спамить лог», требуемые к удалению вместе с
логом (9):** `Ped.cpp:7508` (`warned`), `PedAI.cpp:3965` (`lastRefusalLog`),
`PedFight.cpp:188` (`warned[142][N]`), `PedFight.cpp:4279`/`4303`
(`warnedMissingBikePitch`/`Roll`), `PlayerPed.cpp:356–357`
(`lastMissingModel`, `lastMissingGroup`), `RpAnimBlend.cpp:412` (`warnings`).

**Ложный случай:** `weapons/Weapon.cpp:169` — `static CVector shooterSource`
под `#ifdef FIX_BUGS`; это уже локальная внутри функции, ветка `#else`
объявляет её же на стеке. Достаточно убрать `static`.

**Мёртвый код:** `weapons/Weapon.cpp:889–890` — `static float prev_heading`
записывается (`m_fRotationCur` игрока) и **ни разу не читается**; два
вхождения в файле — это объявление и присваивание. Удаляется целиком.

Плюс глобальные массивы, которые в SA — `static`-поля классов:
`aWeaponInfo[37]` (сейчас свободный глобал; у SA `CWeaponInfo::aWeaponInfo`),
`gaProjectileInfo[32]`, `CProjectileInfo::ms_apProjectile[32]`,
`CPedType::ms_apPedType[22]`, `CPedStats::ms_apPedStats[40]`,
`CPedModelInfo::m_pPedIds`.
`CPedIK::ms_torsoInfo`/`ms_headInfo`/… — уже статики класса, ок.

---

## 7. Что уже по SA и переписывать не нужно

Перечислено, чтобы Блоки 0–4 не «улучшали» готовое:

* `animation/AnimationId.h` — 387/387, полное совпадение (§3.1).
* Загрузчик `.ifp`: ANP2/ANP3/ANPK, `KRTS`/`KRT0`/`KR00`, сжатые
  кватернионы `KeyFrameCompressed` (0xA), `KeyFrameTrans` (0x20),
  `KeyFrame` (0x14), `SetBoneTag` из 44-байтового заголовка секции,
  `RemoveQuaternionFlips`/`CalcTotalTime`, детект магии
  (`AnimManager.cpp:1618/1632`), диапазонный контроль `numAnims` и слотов
  блоков (`:1755–1770`), `ccustom/zip/mesh/*.bpc` через
  `CCustomModels::ForEachAnimBank`.
* `CVehicleAnimGroupData` (30 групп, `GetGroupForAnim`, `GetInOutTimings`,
  `ComputeCriticalBlendTime`, `UsesTruckDrivingAnims` / `Kart` / `Hovercraft`).
* `CPedIK` — все SA-поля и методы (§4.2.6).
* `CAnimBlendAssociation/Hierarchy/Sequence/Node/AssocGroup/ClumpData` —
  структуры и поведение совпадают с эталоном построчно по смыслу.
* `CPlayerPed`: free-aim, lock-on (`FindWeaponLockOnTarget` и свита),
  drive-by (`PedFight.cpp:4380`), `SetJump(highJump, climbJump)` +
  `ApplyLaunchForce` (`Ped.cpp:9662/9726`), спринт-энергия.
* `CPed::PreRenderAfterTest` — уже SA-порядок (IK slope → `UpdateRpHAnim`).
* `CPed::CanStrafeOrMouseControl`, `CanSetPedState`, `CanPedReturnToState`
  (списки состояний VC — переедут вместе с §3.6).
* `CStats::GetFatAndMuscleModifier` — честная заглушка с документированным
  предупреждением.
* `CFileLoader::LoadPedObject` — SA-архив уже читается
  (`data/peds.ide` из `.bpc`), `CPedType::LoadPedData` и
  `CPedStats::LoadPedStats` уже читают SA-таблицы из архива.
* `Population.cpp` — `pedgrp.dat` из архива, ок.

---

## 8. Поблочный план

Порядок выбран так, чтобы каждый блок оставлял дерево собираемым и
проверяемым полным прогоном (267 файлов × 3 конфигурации).

### Блок 0 — фундамент: кости, модели педов, индексация анимаций

Первый, потому что от него зависят все остальные, и потому что он ломающий.

1. `animation/Bones.h/.cpp` → SA `eBoneTag` целиком (`BONE_UNKNOWN = -1`,
   `BROW_R/L 6/7`, `JAW 8`, `BELLY 201`, `R/L_BREAST 301/302`,
   `BONE_MAX_ID = 303`, `MAX_BONE_NUM = 32`), SA-имена
   (`BONE_L_CLAVICLE` …), `ConvertBoneTag2BoneName` по таблице
   `RpAnimBlend` (включая `"L Toe"`, `"R Toe"`, `"Jaw"`);
   `ConvertPedNode2BoneTag` — по SA `ePedNode` (19 значений).
   Правка 105 употреблений в 5 файлах.

2. `animation/AnimManager.h` — `enum AssocGroupId` строго по SA
   (142 значения, `NONE = -1`, `CARS_BEGIN = 88`, `CARS_END = 118`,
   `MAN = 118`, `SHUFFLE = 119` … `SKATE = 138`, `ANIM_TOTAL_GROUPS`).
   Убрать `LOWCARANIMS`→`LOWCARAMIMS`, `STDCARANIMS`→`STDCARAMIMS`,
   `UNARMED`/`PANICCHUNKY`/`STDCARANIMS`/`LOWCARANIMS` — в SA таких имён нет;
   их роли там играют `FATMAN 124` (паника толстяка), `FAT 55` (игрок-толстяк)
   и `STDCARAMIMS 88` / `LOWCARAMIMS 89`.
   Синхронно: `AnimAssocTables.h` (3 021 строка,
   сверка каждой группы с `AnimAssocAnimations.h`), `AnimManager.cpp`
   (`ms_aAnimAssocDefinitions`), 567 употреблений `ASSOCGRP_*` в 31 файле,
   включая `CVehicleAnimGroupData` и `CCarEnterExit`.

3. `modelinfo/PedModelInfo.h/.cpp`:
   * `m_pPedIds[NUM_PED_NAME_ID_ASSOC = 13]` по SA (адрес-эталон `0x8A6268`);
   * `tPedColNodeInfo` (12 записей: `m_vecCenter`, `m_fRadius`, `m_nBoneID`,
     `m_nFlags`) вместо `ColNodeInfo` (10), `sphere.m_nPiece = m_nFlags`,
     bound sphere 1.5, box `±0.5/±0.5/±1.2`;
   * поля по SA: `m_nAnimType` (`AssocGroupId`), `m_nPedType`, `m_nStatType`,
     `m_nCarsCanDriveMask` (`uint16`), `m_nPedFlags` (`uint16`),
     `m_pHitColModel`, `m_nRadio1/2` (`eRadioID`), `m_nRace` (`ePedRace`),
     `m_nPedAudioType` (`eAudioPedType`), `m_nVoiceMin/Max/Id`.

4. `core/FileLoader.cpp:1654 LoadPedObject` — 14 полей, `eRadioID(radio+1)`,
   `CPopulation::FindPedRaceFromName`, `GetAudioPedType`/`GetVoice`.
   Заодно **удалить** `groupAliases` (`:1685`) и `SafePedAnimGroup`
   (`CustomWorld.cpp:456`) — после §3.2 имена `MAN`/`VAN`/`COACH` штатные.

5. Enum'ы: `ePedType` (32), `ePedStats` (43 + `NONE = -1`), `ePedPieceTypes`
   (SA-порядок), `ePedNode` (19), `eMoveState` (8), `eWeaponSkill`,
   `ePedRace`, `eAudioPedType`, `eRadioID`. `CPedStats` → `CPedStat` c
   `m_nDefaultDecisionMaker` и `m_flags`, 11-польный парсер.

6. `PedStdBonePositions` (318 строк эталона) — таблица костей по умолчанию.

**Приёмка:** сборка всех 3 конфигураций; ни одного `ASSOCGRP_`, `BONE_`,
`PED_NODE_` со старым значением; `DetectSAAnimationClump` распознаёт все
модели архива; hit-col педа — 12 сфер; `peds.ide` читается без фолбэков.

### Блок 1 — пед: движение, плавание, присед, паркур, урон по зонам

1. `ePedState` по SA (65 значений, `DIE_BY_STEALTH` вклинивается перед
   `CARJACK`); миграция 925
   употреблений в 43 файлах; версия сейва (сырой `memcpy` в `Pools.cpp:641`
   запрещает менять `sizeof(CPed)` без bump'а).
2. `CPlayerPedData` (или эквивалент): `m_fBreath`, `m_fTimeCanRun`,
   `m_fMoveBlendRatio`, `m_nWetness`, `m_nWaterCoverPerc`, `m_fWaterHeight`,
   `m_bRequireHandleBreath`, `m_bPlayerSprintDisabled`. `CPlayerPed` худеет
   до SA-набора.
3. Плавание: `CTaskSimpleSwim`-эквивалент поверх `CPed` —
   `eSwimState{TREAD, SPRINT, SPRINTING, DIVE_UNDERWATER, UNDERWATER_SPRINTING,
   BACK_TO_SURFACE}`, `SWIM_DIVE_UNDER_ANGLE = -35`, `SWIM_STOP_TIME = 15`,
   анимации `ANIM_ID_SWIM_*`, `m_fBreath`/утопление, `CWaterLevel::GetWaterLevel`,
   вето на slope-lean в `PreRenderAfterTest`, splash-FX.
4. Присед: движение в `PEDMOVE_*` при `bIsDucking`, `ANIM_ID_CROUCHFIRE`,
   `CROUCHRELOAD`, `CROUCH_ROLL_L/R`, `bCrouchWhenShooting` как решение,
   а не как флаг.
5. Паркур: `CTaskSimpleClimb`/`TaskComplexClimb`-эквивалент
   (`ANIM_ID_CLIMB_IDLE/PULL/STAND/STAND_FINISH/JUMP/JUMP_B`), проверка
   коллизии, `SetJump` уже готов.
6. Урон по зонам: `ePedPieceTypes` из hit-col сфер
   (`m_nPiece == m_nFlags`), `InflictDamage` с множителями по зоне,
   `CWeapon::GenerateDamageEvent`.
7. `CStats`: SA-массив статистик (305) или минимум —
   `GetStatValue`/`SetStatValue` для `PISTOL..TEC9`, `FAT`, `MUSCLE`,
   `STAMINA`, `LUNG_CAPACITY`, `MAX_HEALTH`; `GetFatAndMuscleModifier`
   перестаёт быть заглушкой ровно настолько, насколько реверсирован
   `0x559AF0`.

**Приёмка:** пед плавает/ныряет/всплывает и тонет по `m_fBreath`; приседает
и стреляет в приседе; карабкается на бордюр и стену; урон по голове
отличается от урона в торс; сейв не ломается.

### Блок 2 — оружие: `weapon.dat` SA, снаряды, перекрестие

1. `weapons/WeaponType.h` → SA `eWeaponType` (47 + служебные 48…58);
   **явная таблица маппинга** старых VC-id там, где они ещё нужны
   (сейвы, `weapons.ide`), а не арифметика. 915 употреблений в 39 файлах,
   включая `Hud.cpp` и пикапы.
2. Слоты → `eWeaponSlot` (13), `m_weapons[13]`, bump сейва.
3. `weapons/WeaponInfo.h/.cpp` → SA `CWeaponInfo` (`0x70`),
   `aWeaponInfo[80]` с раскладкой `[STD 0–47][POOR 47–57][PRO 58–68][COP 69–79]`,
   `GetWeaponInfo(wt, skill)`, `GetWeaponInfoIndex`, `TypeHasSkillStats`,
   `GetSkillStatIndex`, `m_nAimOffsetIndex`, `GetTargetHeadRange`,
   `GetWeaponReloadTime`.
4. Парсер: `case '$'` (26 полей), `case '%'` (9 полей → `ms_WeaponAimOffsets`),
   `case 163` (melee, 11 полей → `m_nBaseCombo`/`m_nNumCombos`/флаги),
   `SetAnimLoopInfos` с округлением SA, `null`-префиксы,
   `m_nAimOffsetIndex = m_eAnimGroup - ANIM_GROUP_PYTHON`,
   `SetWeaponInfo` только для `eWeaponSkill::STD` и не для `DETONATOR`.
5. `eWeaponFlags` — union битовых полей SA (`bCanAim … bRangeIncreasesOverTime`),
   замена `WEAPONFLAG_*`.
6. `weapons/Weapon.h/.cpp` → `0x1C` (`m_IsFirstPersonWeaponModeSelected`,
   `m_DontPlaceInHand`, `m_FxSystem`), `Initialise(type, ammo, owner)`,
   недостающие методы (`IsTypeProjectile`, `GetProjectileType`,
   `GetWeaponRange`, `TargetWeaponRangeMultiplier`, `DoWeaponEffect`,
   `StopWeaponEffect`, `SetUpPelletCol`, `GenerateDamageEvent`,
   `EvaluateTargetForHeatSeekingMissile`, `FireInstantHitFromCar2`,
   `CanBeUsedFor2Player`), `eWeaponSkill`-зависимая ветка стрельбы.
7. `CProjectile`/`CProjectileInfo` по SA: `m_pCreator`, `m_pVictim`,
   `m_nDestroyTime`, `m_bActive`, `m_vecLastPosn`, `m_pFxSystem`,
   `ms_apProjectile[32]`; набор снарядов
   `GRENADE/TEARGAS/MOLOTOV/ROCKET/ROCKET_HS/FREEFALL_BOMB` +
   Satchel/Detonator; границы по SA-карте вместо VC-констант.
8. `eExplosionType` (13), HS-ракета (`EvaluateTargetForHeatSeekingMissile`),
   огнемёт/краскопульт/огнетушитель как режимы (а не как отдельные VC-ветки),
   бензопила (`CHAINSAW`-ветка `InflictDamage` уже есть).
9. `CWeaponEffects` → массив `gCrossHair[2]` (`0x2C`), `MarkTarget(id, …)`,
   `IsLockedOn`, `ClearCrossHair(s)(Immediately)`,
   `eWeaponEffectsLockTexture`, текстуры `gpCrossHairTex`/`gpCrossHairTexFlight[2]`.
10. `renderer/Hud.cpp:234–360` — перекрестие и прицел по SA-id и по
    `CWeaponEffects`.

**Приёмка:** `weapon.dat` SA читается целиком (80 записей, aim offsets
заполнены, melee-комбо приходят); снаряды шести типов летают и взрываются;
индикатор lock-on меняет цвет по HP; ПНВ/тепловизор включаются.

### Блок 3 — камера, прицеливание, IK

1. `eCamMode` → SA (60): `MODE_AIMWEAPON`, `MODE_AIMWEAPON_ATTACHED`,
   `MODE_AIMWEAPON_FROMCAR`, `MODE_ATTACHCAM`,
   `MODE_ROCKETLAUNCHER_HS`, `MODE_ROCKETLAUNCHER_RUNABOUT_HS`,
   `MODE_TWOPLAYER*`/`MODE_DW_*` — по необходимости (сетевых режимов нет,
   но enum приводится целиком ради совместимости).
2. `core/Cam.cpp` — `Process_AimWeapon` (третье лицо с поворотом корпуса),
   `Process_AimWeaponFromCar`, `Process_AttachCam`; убрать
   закомментированный `case MODE_AIMING:`.
3. Связка IK: `CPedIK::PointGunAtPosition` / `PointGunInDirection` /
   `RotateTorsoForArm` + `m_nAimOffsetIndex` из `weapon.dat`
   (сейчас `Ped.cpp:1391–1409` дёргает IK вручную).
4. Free aim от третьего лица с crosshair по `m_f3rdPersonCHairMultX/Y`,
   strafe на `CanStrafeOrMouseControl` (SA-список состояний),
   drive-by по SA-условиям, первое лицо (`SNIPER`, `RLAUNCHER`, `M16`,
   `CAMERA`, `NIGHTVISION`, `INFRARED`).
5. Wanted/звук/частицы, завязанные на прицел (`AudioLogic.cpp` 186
   вхождений — только точки подключения, без переписывания аудио).

**Приёмка:** прицеливание в третьем лице поворачивает корпус через IK,
мушка совпадает с точкой попадания; drive-by стреляет в SA-секторах;
снайперка/РПГ/фотоаппарат/ПНВ работают в первом лице.

### Блок 4 — каскадная зачистка и чистый API

1. Убрать мутабельные `static` (§6): 8 «настоящих» состояний — в
   `CPed`/`CWeapon`/`CPlayerPed`, 7 констант — в `constexpr`, 9 отладочных
   кэшей — вместе с логом, `CWeaponEffects`-синглтон — в `gCrossHair[2]`.
2. Чистый C++ API (геттеры/сеттеры, без `nil`-полей наружу):
   `GetPedState/SetPedState`, `GetMoveState/SetMoveState`,
   `GetWeaponSkill/SetWeaponSkill`, `GetBreath/SetBreath`,
   `IsDucking/SetDucking`, `GetAnimGroup`, `GetAimTarget`,
   `IsProjectileActive` и т. п.; `CProjectileInfo` — без публичных
   массивов.
3. Проход по 90 внешним файлам в порядке §5 — привести к новым enum'ам и к
   API, а не к публичным полям; приоритет по насыщенности
   (`AudioLogic`, `AudioManager.h`, `Vehicle`, `Cam`, `VehicleAnimGroupData`,
   `Replay`, `Bike`, `Automobile`, `MobileTouchControls`, `CarEnterExit`,
   `Pad`, `Pickups`, `Darkel`, `CustomWorld`, `Pools`).
4. Ревизия `extras/custom/*` — убрать SA-фолбэки, ставшие штатными путями
   (`SafePedAnimGroup`, `groupAliases`, подмены VC-скелетов
   `FindSAReplacementModel`/`m_saReplacementModel` — VC-моделей больше нет,
   механизм замещения можно выпилить вместе с `m_bSAAnimationModel`).
5. Сейвы: версия и раскладка (`Pools.cpp`, `GenericGameStorage.cpp`,
   `MemoryCard.cpp`), потому что меняются `sizeof(CPed)`, набор состояний,
   слоты оружия и `CWeaponInfo`.

**Приёмка:** ни одной мутабельной файловой `static` в модуле; ни одного
VC-id оружия/группы/кости; полный прогон 267 × 3 = FAIL 0; объектники
`-c -O1` по всем изменённым `.cpp`.

---

## 9. Проверка (используется после каждого блока)

```sh
INCS=$(find src vendor/librw vendor/librw/src -type d | sed 's/^/-I/')
g++ -std=c++17 -fsyntax-only -Wall -DLIBRW -DRW_GL3 $INCS \
    -I/tmp/glfw-3.3.4/include -Isrc <file>
# конфигурации: по умолчанию; -DFIX_BUGS -DSA_VEHICLE_MODELS; -DCUSTOM_MODELS
# объектники: g++ ... -c -O1 -o /tmp/objs/<name>.o <file>
# полный прогон: /tmp/check.sh /tmp/check2.sh /tmp/check3.sh   (267 файлов, ~100 с каждый)
```

`-fsyntax-only` не ловит ODR и шаблоны — финальная проверка всегда `-c -O1`.

---

## 10. Приложение — адреса, подтверждённые по plugin-sdk / эталону

| символ | адрес | размер |
|---|---|---|
| `CWeaponInfo::aWeaponInfo` | `0xC8AAB8` | 80 × `0x70` |
| `CWeaponInfo::ms_WeaponAimOffsets` | `0xC8A8A8` | 21 × `0x18` |
| `CWeaponEffects::gCrossHair` | `0xC8A838` | 2 × `0x2C` |
| `CWeaponEffects::gpCrossHairTex` | `0xC8A818` | — |
| `CWeaponEffects::gpCrossHairTexFlight[2]` | `0xC8A810` | — |
| `CPedModelInfo::m_pPedIds` | `0x8A6268` | 13 × `0xC` |
| `CPedModelInfo::m_pColNodeInfos` | `0x8A6308` | 12 |
| `CPedStats::ms_apPedStats` | `0xC0BBEC` | 43 |
| `CProjectileInfo::gaProjectileInfo` | `0xC891A8` | 32 × `0x24` |
| `CProjectileInfo::ms_apProjectile` | `0xC89110` | 32 |
| `CPedStdBonePositions` | `0x8D13A8` | `BONE_MAX_ID` |

Ключевые адреса функций SA, по которым сверялась логика:
`CWeaponInfo::LoadWeaponData 0x5BE670`, `CAnimManager::Initialise 0x5BF6B0`,
`CAnimManager::LoadAnimFile 0x4D47F0`, `CAnimManager::LoadAnimFiles 0x4D5620`,
`CPed::CanStrafeOrMouseControl 0x5DF090`, `CPed::CanSetPedState 0x5DF030`,
`CPed::PreRenderAfterTest 0x5E65A0`, `CPedType::LoadPedData 0x608B30`,
`CPedType::FindPedType 0x608790`, `CPedStats::LoadPedStats 0x5BB890`,
`CPedStats::GetPedStatType 0x6088D0`, `CFileLoader::LoadPedObject 0x5B7420`,
`CPedModelInfo::CreateHitColModelSkinned 0x4C6D90`,
`CPedModelInfo::AnimatePedColModelSkinned 0x4C6F70`,
`CClumpModelInfo::SetFrameIds 0x4C5460`, `CStats::GetFatAndMuscleModifier 0x559AF0`,
`CProjectileInfo::AddProjectile 0x737C80`, `CProjectileInfo::Update 0x738B20`,
`CProjectileInfo::IsProjectileInRange 0x739860`.
