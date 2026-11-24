# Инвентарь чистки reVC под мультиплеерную базу

Перепись по системам §3 исходного задания. Числа — фактические обращения `grep -rF`
по `src/` (без `docs/`), а не оценки из текста задания.

## 1. Сводка по дереву

| Точка | Файлов `.cpp/.h` | Строк | TU (`.cpp`) |
|---|---|---|---|
| База `8efb3bc` (ветка `arena/782fc865-revc`) | 612 | 316 624 | 271 |
| B + C + мёртвые ветки + D-1 (коммит `64bcef4`, как и всё ниже) | 546 | 282 007 | 236 |
| D-2 (ядро целей транспорта) | 546 | 280 883 | 236 |
| D-5/D-6 (`PlayerInfo`, `Ped.cpp`, `PedType`, поезд) | 540 | 277 808 | 233 |
| E (трафик, AI-транспорт, вертолёты/самолёты) | 523 | 265 562 | 225 |

Подсчёт: `find src -name '*.cpp' -o -name '*.h'` без `fakerw`, строки `xargs cat | wc -l`. Вся история
этапов B–E в этой ветке — один коммит `64bcef4`: песочница была пересоздана, поэтапные коммиты
(`680fc1d`, `771fdeb`, `01f7b47`, `b2709d2`; ранее также `c8acc16`, `88d6f55`, `742fd7e`) не сохранились,
строки таблицы — из журналов на момент работы. Этапы A–E сняли 89 файлов, 51 062 строки и 46 единиц трансляции.

## 2. Перепись по системам §3

Колонка «обращений» — сколько раз имя системы встречается в `src/` после этапа E.
Прочерк означает, что система удалена и имя не встречается вообще.

| Система (§3) | Файлы | Обращений | Состояние |
|---|---|---|---|
| `CTheScripts`, `CRunningScript`, `ScriptSpace`, `COMMAND_` | `control/Script.*`, `ScriptWeather.cpp` | 0 | удалено (B) |
| `main.scm`, `SIZE_MAIN_SCRIPT`, `SaveAllScripts`, `LoadAllScripts` | — | 0 | удалено (B) |
| `CDarkel` | `control/Darkel.*` | 0 | удалено (C) |
| `CPhoneInfo` | `control/Phones.*` | 0 | удалено (C) |
| `CSetPieces` | `control/SetPieces.*` | 0 | удалено (C) |
| `CGarages` | `control/Garages.*` | 2 (только `save/MemoryCard.cpp` под `PS2_MENU`) | удалено (C) |
| `CRestart` | `control/Restart.*` | 2 (то же) | удалено (C) |
| `CRoadBlocks` | `control/RoadBlocks.*` | 0 | удалено (C) |
| `CSceneEdit` | `control/SceneEdit.*` | 0 | удалено (C) |
| `COnscreenTimer` | `control/OnscreenTimer.*` | 0 | удалено (B) |
| `CPager` | `text/Pager.*` | 0 | удалено (C) |
| `CCutsceneMgr`, `CCutsceneObject` | `cutscene/*` | 0 | удалено (B, файлы отсутствовали в дереве) |
| `CCredits` | `renderer/Credits.*` | 0 | удалено (C) |
| `CPacManPickups` | `control/Pickups.*` (блок) | 0 | удалено (C) |
| `CCranes` | `vehicles/Cranes.*` | 2 (`save/MemoryCard.cpp`) | удалено (C) |
| `CMovingThings`, `CScriptPaths` | `renderer/Fluff.*` (блоки) | 0 | удалено (C) |
| `CBridge` | `control/Bridge.*` | 0 | удалено (C) |
| `CRopes` | `core/Ropes.*` | 0 | удалено (C) |
| `CWaterCreatures` | `renderer/WaterCreatures.*` | 0 | удалено (C) |
| `CReplay`, `CRecordDataFor*` | `control/Replay.*`, `Record.*` | 0 | удалено (C) |
| `CEventList` | `core/EventList.*` | 0 | удалено (C) |
| `CAccidentManager` | `core/Accident.*` | 0 | удалено (C) |
| `CGangs` | `peds/Gangs.*` | 0 | удалено (C) |
| `CVehicleRecording` (SA-port) | `vehicles/VehicleRecording.*` | 0 | удалено (C) |
| `CPopulation` | `peds/Population.*` | 130 | этап D |
| `PedAI` | `peds/PedAI.*` | 1 (строка ошибки внутри самого файла) | этап D |
| `CivilianPed`, `CopPed`, `EmergencyPed` | `peds/*Ped.*` | 21 / 66 / 0 | этап D (`EmergencyPed` уже удалён в C) |
| `PedChat`, `PedAttractor`, `PedRoutes` | `peds/*` | 0 / 145 / 3 | этап D |
| `PedType`, `CPedType`, `CPedStats` | `peds/PedType.*`, `PedStats` | 243 / 37 / 24 | этап D |
| `m_pedStats`, `m_pedStatType`, `m_carsCanDrive` | `Ped.h`, `PedModelInfo.h` | 62 / 15 / 5 | этап D |
| `PEDSTATS.DAT`, `PEDGRP.DAT` | загрузчики | 2 / 2 | этап D |
| `CWanted`, `m_pWanted` | `core/Wanted.*`, `PlayerPed.h` | 45 / 5 | этап F |
| `CStats::` | `core/Stats.*` | 318 | этап F |
| `save/*`, `TheMemoryCard` | `save/*` | 189 | этап F |
| `CTheCarGenerators`, `CCarAI`, `ThePaths`, `CPathFind` | `control/*`, `vehicles/*` | 0 / 0 / 0 / 0 | удалено (E) |
| `CHeli`, `CPlane`, `SpecialPlateHandler`, `LoadedCarGroup`, `CIniFile`, `WorldRecovery` | `vehicles/*`, `core/*` | 0 | удалено (E) |
| `m_fearFlags`, `m_nearPeds`, `m_numNearPeds`, `OBJECTIVE_` | `Ped.*` | 5 / 83 / 38 / 673 | этап D |
| `bIsLawEnforcer` | `Vehicle.*` | 0 | удалено (E: нигде не ставился в `true`) |
| `WBSTATE_`, `BUSTEDAUDIO_` | `core/PlayerInfo.*` | 20 / 5 | этап F |
| `m_pVehicleEx`, `m_ColModel` | `PlayerInfo.*`, `Automobile.cpp` | 0 / — | переименовано в `m_pSpecialColModelVehicle` (E, гидравлика игрока; см. `decisions.md` §3.3) |

Оставшиеся 2 обращения к `CGarages`/`CRestart`/`CCranes`/`CTheScripts` находятся в
`src/save/MemoryCard.cpp` целиком под `#ifdef PS2_MENU`; файл уходит на этапе F.

## 3. Граф «система → потребители» по факту

Расхождения с §3, найденные переписью:

- **`Cranes`** был подключён не только к скриптам: `CPedAI` использовал `CCranes::IsThisCarBeingCarried` для запрета выхода из машины. Вызов удалён вместе с системой; ограничение на выход из машины теперь не накладывается.
- **`CEventList`** был подключён к 12 файлам (`Fire.cpp`, `World.cpp`, `Explosion.cpp`, `BulletInfo.cpp`, `Automobile.cpp`, `Weapon.cpp`, `Ped.cpp`, `PedFight.cpp` и др.). Все точки регистрации удалены, `CPed::ScanForThreats` теперь работает только по ближним педам.
- **`CGangs`** использовался `Population.cpp` (порог банд, оружие банд, `GetWillAttackPlayerWithCops`), `Zones.*` (массивы порогов банд), `Streaming.cpp` и `CarCtrl.cpp` (бандитские машины). Все четыре потребителя почищены.
- **`CReplay`** имел 71 точку обращения в 23 файлах — в основном guard-проверки `IsPlayingBack()` в `Process*`. Все guard-ы сняты: нормальный путь выполняется всегда.
- **`CVehicleRecording`** — не VC-система, а скаффолд SA-порта; удалена как «строительные леса» по §6.2.
- **`CTrain`** удалён на этапе D-6 (вместе с `TrainNode`, `CTrainDoor`, поездной камерой и `VEHICLE_TYPE_TRAIN`): `vehicles/Train.cpp` в этой ветке **не** был обёрнут в `#ifdef GTA_TRAIN`, поэтому поезд присутствовал в сборке и удалён как живая система, а не как мёртвый код.
- **`CCutsceneMgr`** в дереве отсутствовал уже на базе — катсценный менеджер этого форка не содержит.

## 4. Удалённые файлы

**Этап B** (`8efb3bc` → `64bcef4`): `control/OnscreenTimer.{cpp,h}`, `control/Script.{cpp,h}`,
`control/ScriptWeather.cpp`, `vehicles/StuckCarCheck.{cpp,h}`, `vehicles/UpsideDownCarCheck.{cpp,h}`.

**Этап C** (там же): `control/{Bridge,Darkel,Garages,Phones,Record,Replay,Restart,RoadBlocks,SceneEdit,SetPieces}.{cpp,h}`,
`core/{Accident,EventList,Ropes}.{cpp,h}`, `peds/{EmergencyPed,Gangs}.{cpp,h}`,
`renderer/{Credits,WaterCreatures}.{cpp,h}`, `text/Pager.{cpp,h}`,
`vehicles/{Cranes,VehicleRecording}.{cpp,h}`.

**Этап D-1** (там же): `peds/{CivilianPed,CopPed,EmergencyPed,PedAI,PedAttractor,PedChat,PedDebug,PedPlacement,PedRoutes,Population}.{cpp,h}`,
`objects/{Stinger}.{cpp,h}` (шипы полицейских).

**Этап D-5/D-6** (там же): `peds/PedType.{cpp,h}`, `vehicles/{Train,TrainNode}.{cpp,h}`.

**Этап E**: `control/CarAI.{cpp,h}`, `control/PathFind.{cpp,h}`, `core/IniFile.{cpp,h}`, `core/WorldRecovery.h`,
`vehicles/{CarGen,LoadedCarGroup,Heli,Plane,SpecialPlateHandler}.{cpp,h}`, `tests/WorldRecoveryTests.cpp`.

## 5. Таблица переносов `PedAI` / `PedFight`

Заполняется на этапе D. Состояние на `8efb3bc`: файлы `peds/PedAI.cpp` и `peds/PedFight.cpp`
ещё в дереве, переносов не было. Переносы сделаны на этапах D-1/D-2 (таблица — в `testlog.md` §5.1).

| Метод | Судьба | Куда |
|---|---|---|
| — | — | — |

## 6. Повисшие поля и функции, удалённые вслед за системами

| Поле / функция | Причина |
|---|---|
| `CPed::m_pRopeEntity`, `m_fAbseilPos`, `PED_ABSEIL_FROM_HELI` | верёвочный десант SWAT (`CRopes`, `CCopPed::ProcessHeliSwat`) |
| `CCopPed::m_nRoadblockVeh` | питался только `CRoadBlocks` |
| `eCopType::COP_HELI_SWAT` | единственный потребитель — удалённый `ProcessHeliSwat` |
| `CPed::PED_FACE_PHONE`, `PED_MAKE_PHONECALL`, `PED_ANSWER_PHONE`-ветки | `CPhoneInfo` |
| `CPed::PED_INVESTIGATE_EVENT`, `m_fAngleToEvent`, `CheckForExplosions/GunShots/DeadPeds` | `CEventList` |
| `CPed::bFannyMagnetCheat` | чит, работавший только на педах |
| `CPopulation::ms_bGivePedsWeapons`, `m_AllRandomPedsThisType`, `DealWithZoneChange`, `MoveCarsAndPedsOutOfAbandonedZones` | сценарные/пустые хвосты `CPopulation` |
| `CCarCtrl::bMadDriversCheat` | чит, работавший только на трафике |
| `CStats::ShowChaseStatOnScreen` | чит `DoShowChaseStatCheat` |
| `CVehicle::m_nPacManPickupsCarried`, `PickupType` PacMan-значения | `CPacManPickups` |
| `CZoneInfo`: пороги банд, `SetZone*Info`/`SetCarDensity`/`SetPedDensity`/`SetPedGroup` | их вызывали только скриптовые команды |
| `CPed::bHeldHostageInCar`-ветка с `if (false)` | мёртвый код |
| `CVehicle::bIsLawEnforcer`, `ChangeLawEnforcerState`, `CWorld::StopAllLawEnforcersInTheirTracks`, `CWorld::SetAllCarsCanBeDamaged`, `CPlayerInfo::MakePlayerSafe`, `Camera.cpp` `OBBE_COPCAR*` | флаг нигде не ставился (E) |
| `CAutomobile`/`CBike::bWaterTight` | писался только `false` (E) |
| `CVehicleModelInfo::m_vehicleClass`, `m_frequency`, `m_nTimesUsed` | `CTheCarGenerators`/`LoadedCarGroup` (E) |
| `CVehicle::bIsAmbulanceOnDuty`, `bIsFireTruckOnDuty` | диспетчеризация скорой/пожарных (E) |
| `cTransmission::CalculateGearForSimpleCar` | `STATUS_SIMPLE`-машины (E) |
| `C2dEffect::{Attractor,PedAttractor}`, `ATTRACTORTYPE_*` | `CPedAttractorManager` (E) |
| `CPed::bReachedAttractorHeadingTarget`, `bTurnedAroundOnAttractor`, `bHasAlreadyUsedAttractor`, `bHasAlreadyStoleACar`, `bCarPassenger`, `bMiamiViceCop`, `bHasAlreadyBeenRecorded`; `CVehicle::bHasAlreadyBeenRecorded` | только инициализация (E) |
