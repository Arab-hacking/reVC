# Решения, принятые при чистке reVC

Все спорные случаи и разрешённые отклонения от текста задания. Код — только на английском,
здесь — на русском, чтобы совпадало с заданием.

## 1. Решения §3.9

| Система | Решение | Почему |
|---|---|---|
| `CReplay` + `CRecordDataForGame/Chase` | удалено | одиночная фича, снапшоты пула педов и скриптовых блипов; к MP не применима |
| Радио / музыка | оставлено | не сюжет |
| `CWaterCannons` | оставлено | механика пожарной машины |
| `CCranes`, `CMovingThings`, `CBridge`, `CRopes`, `CWaterCreatures` | удалено | единственные создатели — скрипты, без них системы мертвы |
| `CPickups` | оставлено | нужно для дропа оружия при смерти |
| `CPedType` / `CPedStats` / `pedstats.dat` / `pedgrp.dat` | удалено на этапе D (D-6, §3.2) | данные отношений NPC; игроку не нужны |
| `CPathFind` / `ThePaths` / `paths/*.dat` | удалено на этапе E (сделано) | обслуживают только AI педов и машин |
| `CHeli` / `CPlane` (случайные вертолёты/самолёты, `Jumbo`) | удалено на этапе E (сделано) | их создавали `CarAI`/`CPlane::UpdatePlanes`; классы были AI-транспорт, а модель вертолёта/самолёта для игрока спавнится как `CAutomobile` с флагами handling |
| `CIniFile` / `LoadedCarGroup` / `SpecialPlateHandler` | удалено на этапе E (сделано) | читали только трафик (`trafficLights`, `cargrp`, таблички случайных машин) |
| `CTrafficLights` (отрисовка) | оставлено | мировой амбиент |
| `CPlaceName`, `CCurrentVehicle` | оставлено | косметика HUD, не сюжет |

## 2. Точки входа (§4)

### 4.1. Создание игрока
`CGame::CreatePlayer()` в `core/Game.cpp` полностью заменяет `COMMAND_CREATE_PLAYER`:
запрашивает модель игрока, вызывает `CPlayerPed::SetupPlayerPed(0)`, ставит на константу спавна
и делает `ReactivatePlayerPed(0)`. Камера обрабатывается после создания.

Выбор модели (`CGame::GetPlayerModel`): в архиве нет `player.dff`, поэтому слот 0 (`MI_PLAYER`)
не заполнен; игрок берётся из первого зарегистрированного SA-актора с SA-скелетом
(предпочтение — стандартная мужская модель). Если актера нет вообще — используется
жёстко заданный слот, и движок сообщает об отсутствии модели.

**Константа спавна**: `CGame::GetSpawnPosition` → Ocean Beach, `(79.6875, -867.25)`, курс 180°.
Точка — классическое начало Vice City; для freeroam-базы другая не нужна.

### 4.2. Смерть и респавн
`CGameLogic::Update` урезан до `WBSTATE_PLAYING → PED_DEAD → KillPlayer()` и респавна по таймеру
на ту же константу спавна. Арест, провал миссии и `CRestart` удалены.
Значения `WBSTATE_BUSTED`/`WBSTATE_FAILED_CRITICAL_MISSION` ещё живут в `PlayerInfo.h` до этапа F.

### 4.3. Спавн транспорта
Сделано на этапе E. Единственная функция — `CGame::SpawnVehicle(uint32 model, CVector pos, float heading)` в
`core/Game.cpp`: фабрика `CCarCtrl::GetNewVehicleDependingOnCarModel(model, MISSION_VEHICLE)`, `nil` при
переполнении пула, `SetPosition`/`SetOrientation(0, 0, heading)`, `STATUS_ABANDONED`, `CARLOCK_UNLOCKED`,
`CWorld::Add`. Через неё идут: `re3.cpp::SpawnCar` (дебаг-меню; `z` по `FindGroundZFor3DCoord(… + 2.0f)`, курс 3.49),
`Pad.cpp` (`VehicleCheat` и Shift+7/8), `MobileTouchControls.cpp` (панель), `AnimViewer.cpp` и
`CRemote::GivePlayerRemoteControlledCar` (последние две раньше звали фабрику/`new CAutomobile` напрямую).
`CCarCtrl` — только фабрика, других её вызовов нет. `createdBy` у фабрики оставлен: все точки спавна
передают `MISSION_VEHICLE`, а поле `VehicleCreatedBy` пишется в сейв (до этапа F).

## 3. Решения по конкретным системам

- **`IsPlayerOnAMission()`** → `false`. Все guard-ы «на миссии» сняты, нормальный путь выполняется всегда.
- **`CReplay::IsPlayingBack()`** — 71 guard в 23 файлах снят; поведение — как при обычной игре.
- **`ClearSpaceForMissionEntity`** — удалён: callers не осталось.
- **`bPlayerIsInTheStatium`** — удалён (сюжетный флаг).
- **Блок сохранения скриптов** в `GenericGameStorage` пишется нулевой длины, чтобы layout сейва
  остался прежним. Сам сейв-модуль уходит целиком на этапе F.
- **`SPHERE_MARKER_*`** переехали в `SpecialFX.h` (остались только мировые маркеры).
- **`ShowRadarMarker`** удалён (`CRadar` больше не рисует скриптовые маркеры).
- **`CZoneInfo`**: пороги банд (`gangPedThreshold`-массив) и `SetZone*Info`/`SetCarDensity`/
  `SetPedDensity`/`SetPedGroup` удалены — их вызывали только скриптовые команды.
- **Регистрация преступлений** (`CEventList::RegisterEvent`) удалена во всех 12 точках;
  `CPed::ScanForThreats` удалён вместе с NPC-AI (D-2).
- **Гаражная камера** удалена, камера stairs-zone (`CCullZones::CamStairsForPlayer`) сохранена —
  она нужна для лестниц и не связана с гаражами.
- **`CVehicleRecording`** удалена как скаффолд SA-порта (§6.2).
- **`AUDIOTYPE_BRIDGE`** остаётся в `audio_enums.h`: enumerate-значение пустое, но аудио-таблица
  индексируется по нему; удаление сломает индексацию. Комментарий в коде не оставлялся.
- **Диспетчеризация скорой и пожарная** удалены (D): обе жили в `CAmbulance`/`CFireTruck`-AI; на E за ними ушли
  флаги `bIsAmbulanceOnDuty`/`bIsFireTruckOnDuty`, которые к тому моменту только инициализировались.
- **`CCam::MODE_EDITOR`** (SceneEdit) удалён; максимум режима отладочной камеры понижен до `MODE_MODELVIEW`.
- **`CEmergencyPed`** удалён (D-1); вместе с ним ушли `CPopulation`/`CCivilianPed`, так что
  отдельный класс для медиков/пожарных больше не нужен — `PEDTYPE_MEDIC`/`PEDTYPE_FIREMAN`
  остаются только как значения типа.
- **SWAT-десант из вертолёта** удалён вместе с верёвками; на E удалён и `CHeli` целиком (см. §3.3).
- **`CPed::IsThisPedAnAimingPriority`** → `true` только для не-друзей / `m_pPointGunAt == this` /
  цели-убийства, иначе `false`.
- **Банды в `CPopulation`**: `AddToPopulation` больше не производит gang-типы; не-коп спавн —
  95% одиночный гражданский, 5% пара из `zoneInfo.pedGroup`. Порог банд, ветка банд и
  `isSecurityGuard` удалены. `PEDTYPE_GANG1–10` остаются в `PedType.h` до этапа D.
- **Медики/пожарные без accident-AI** — см. выше.
- **`CPed::bFannyMagnetCheat`, `CCarCtrl::bMadDriversCheat`, `CStats::ShowChaseStatOnScreen`** —
  удалены вместе с читами, которые их включали.

### 3.1 Этап D-2: ядро целей и замены кэша ближних педов

- **`m_nearPeds`/`m_numNearPeds`** (запрещены §6.2) заменены запросом `CPed::FindNearPeds(centre, radius,
  out, maxPeds)` по секторам мира. Семантика прежняя (без `this`, без педов в транспорте кроме байков,
  ≤3 мёртвых, ближайшие первыми); радиус каждая точка задаёт свой. Кэш пересобирался раз в 16 кадров
  для каждого педа — теперь опрос идёт только когда он нужен (камера, ближний бой, высадка).
- **Огнемёт** поджигает через `CWorld::SetPedsOnFire` вместо обхода `m_nearPeds` стрелка.
- **`m_pedStats`**: поле с D-1 не инициализировалось; множители силы/слабости и флаги
  `STAT_SHOPPING_BAGS`/`STAT_ONE_HIT_KNOCKDOWN` сняты, урон остался базовым.
- **NPC и огонь/газ/машина игрока**: бегство (`SetFlee`, `SetFindPathAndFlee`) удалено; горящий NPC
  стоит в `PED_ON_FIRE`, после тушения → `SetIdle()`, как игрок. Разгон `DRIVINGSTYLE_PLOUGH_THROUGH`
  для машины игрока при обстреле сохранён, условие цели водителя сжато до `!= OBJECTIVE_LEAVE_CAR`.
- **Gang-условие урона** в `Weapon.cpp` (`!IsGangMember() && m_nPedType != PEDTYPE_COP`) →
  `m_nPedType != PEDTYPE_COP`.
- **Чит «PEDDEBUG»** и `DebugRenderOnePedText` удалены: их тела жили в удалённом на D-1 `PedDebug.cpp`.
- **`bPedCheat3`**, `ms_vec2DFleePosition` удалены: читателей не осталось.
- **`CPed::OurPedCanSeeThisOne`** сохранён (используется автоприцелом игрока в `PlayerPed.cpp`).

### 3.2 Этап D-5/D-6: транспортное ядро игрока, `PedType`, поезд

- **Rolling door** (`PlayerPed.cpp`): блок `PED_DRIVING && m_objective != OBJECTIVE_LEAVE_CAR` → анимация
  `ROLLDOOR` удалён по §3.10 п.10 вместе с `PedAnimDoorCloseRollingCB`. У игрока в машине `ProcessControl`
  теперь выходит сразу по `PED_DRIVING`.
- **«Экипаж разбегается»** (`Vehicle.cpp::InflictDamage`): удалён. Ветка работала только для
  `RANDOM_VEHICLE` под `STATUS_SIMPLE/PHYSICS` — это NPC-трафик, которого больше нет; для машины игрока
  недостижима. `Automobile.cpp`/`Cam.cpp`-проверки `m_objective != OBJECTIVE_LEAVE_CAR` оставлены (ручник
  и камера, пока игрок выходит).
- **Навигация в `Ped.cpp`**: `ThePaths.FindNodeClosestToCoors` в `PositionPedOutOfCollision` удалён, функция
  возвращает `foundAPos` как есть; поля path-node (`m_nNumPathNodes`, `m_pathNodeTimer`, `m_pathNodeObjPool`,
  `m_pCurPathNode`, `m_pLastPathNode`) удалены — читателей нет. `#include "PathFind.h"` из `Ped.h` убран:
  теперь `src/peds/` не знает о навигации вообще.
- **Flee-ветки игрока**: blend `PEDMOVE_RUN` при бегстве всегда 1.0; в `ProcessCollision` условие
  сведено к `IsPlayer() || m_fCollisionSpeed >= 1.0f`. Guard `SetIdle` (`!= PED_MUG && != PED_FLEE_ENTITY`)
  и ветка `PED_FLEE_POSITION` в `SetExitCar` сведены к ветке игрока — эти состояния больше никто не ставит.
- **`ePedState` не сжимается**: нумерация позиционная и попадает в сейв (`SaveVersion.h`), поэтому мёртвые
  значения `PED_WANDER_*`, `PED_FLEE_*`, `PED_CHAT`, `PED_MUG`, `PED_SOLICIT`, `PED_BUY_ICE_CREAM`,
  `PED_SUNBATHE`, `PED_FLASH`, `PED_FOLLOW_ROUTE` остаются в enum и в списках `CanPedReturnToState` /
  `CanStrafeOrMouseControl` как метки; кода, который их ставит, нет. `PED_ENTER_TRAIN`/`PED_EXIT_TRAIN`
  удалены — их уже нельзя оставить, так как их читатели (`World.cpp`, `NoClip.cpp`) переписаны.
- **`PedType.{h,cpp}` удалены раньше срока E** (в §1 значилось «этап D» — сделано): `CPedType`/`CPedStats`
  читали только `ped.dat`/`pedstats.dat` и писали сейв-блок; после D-1 читателей `GetPedFlag`/`GetPedStat` нет.
  `ePedType` и `eMoveState` перенесены в `Ped.h` (их читают `Stats`, `IsFemale`, 13 файлов `PEDMOVE_*`).
  Блок `PedTypeSize` в сейве больше не пишется — формат этого форка и так несовместим.
- **`CTrain` удалён целиком** (в §5/inventory значилось «этап E»): поезд не является системой NPC-ходьбы,
  а после удаления `PED_ENTER_TRAIN`/`SetPedPositionInTrain` и `CarCtrl::DoTrainGenerationAndRemoval`
  от него оставались бы только мёртвые структуры. Вместе с ним: `VEHICLE_TYPE_TRAIN`, `STATUS_TRAIN_*`,
  `MI_TRAIN`, `CTrainDoor`, `eTrainPositions`, поездные camera-режимы/узлы/флаги (`Process_Train_Camera_Control`,
  `LoadTrainCamNodes`, `CTrainCamNode`, `m_bTargetJustBeenOnTrain`, `m_bUseSpecialFovTrain`), `bHitByTrain`,
  `ScanSectorList_Subway`, `StartShake_Train`, `FindPlayerTrain`, поездные ветки в `PedFight.cpp`
  (урон 50/150 от поезда), `TaskSimpleClimb`, `Vehicle.cpp` (фары поезда, `CanBeDriven`, `RemovePassenger`),
  `Cam.cpp` (`StandingInTrain`, `OnTrain`, FOV поезда), `Renderer.cpp` (скан метро), `ZoneCull.cpp`,
  `CarGen.cpp`, `VisibilityPlugins`/`VehicleModelInfo` (`trainIds`, `RenderTrainHiDetail*`).
  Аудио-константы позиционных списков (`SOUND_TRAIN_DOOR_*`, `SAMPLEBANK_TRAIN`, `SFX_TRAIN_*`) оставлены.

### 3.3 Этап E: трафик, AI-транспорт, вертолёты/самолёты

- **Упавшие сущности.** Ранее принятое решение — телепортировать пустую `MISSION_VEHICLE`/педа на ближайший узел пути
  (`WorldRecovery.h`) — больше невыполнимо: графа путей нет. Теперь `CWorld::RemoveFallenCars`/`RemoveFallenPeds`
  просто удаляют пустую машину/педа ниже `MAP_Z_LOW_LIMIT` (`Remove` + `delete`); машина игрока (и пед в машине) не
  трогается — на карте ниже −100 это решает сервер/игрок. `WorldRecovery.h` и его тест удалены.
- **`VehicleCreatedBy`** остаётся как данные (читается в `Pools.cpp`, `World.cpp`, `Ped.cpp`, `Automobile.cpp`,
  `Bike.cpp`, `Boat.cpp`, `CarEnterExit.cpp`, `Vehicle.cpp::Save/Load`); `RANDOM_VEHICLE`/`PARKED_VEHICLE`/
  `PERMANENT_VEHICLE` больше никем не ставятся. Уходит вместе с сейвом на F.
- **`CAutoPilot`** остаётся структурой: поля пишутся/читаются только в `Vehicle::Save/Load`, `m_nCarMission`
  инициализируется `MISSION_NONE` в конструкторе. Позиционные `eCarMission`/`eCarDrivingStyle` не сжимаются до F.
- **`STATUS_SIMPLE`** остаётся в `eEntityStatus` (позиционное перечисление, попадает в сейв), кода с ним нет.
- **`m_pVehicleEx` переименован в `m_pSpecialColModelVehicle`.** В §6.2 он описан как «машина по соседству» для
  AI-подбора, но в этом дереве поле — указатель на машину, которая использует гидравлическую коллизию игрока
  (`CPlayerInfo::m_ColModel`, `CAutomobile::ProcessHydraulics`): ставится при инициализации спец-модели и читается,
  чтобы снять `bUsingSpecialColModel` с предыдущей машины. Это механика машины игрока — оставлена под новым именем.
- **RC-режим оставлен** (`CRemote`, `m_pRemoteVehicle`, `m_bInRemoteMode`, `BlowUpRCBuggy`, `TakeRemoteControlledCarFromPlayer`)
  по §3.10. Единственная точка входа `CRemote::GivePlayerRemoteControlledCar` в дереве вызовов не имеет (в VC её звал
  скрипт); она сохранена как API механики и переведена на `CGame::SpawnVehicle`.
- **`bIsLawEnforcer` и всё, что с ним**: флаг нигде не ставился в `true` (ставили `CCarAI`/скрипт), поэтому удалены
  `CVehicle::ChangeLawEnforcerState`, ветка `SetWantedLevelNoDrop(1)` при таране полицейской машины в `Automobile.cpp`,
  `CWorld::StopAllLawEnforcersInTheirTracks`, `CWorld::SetAllCarsCanBeDamaged`, `CPlayerInfo::MakePlayerSafe`
  (единственный вызов — `false` при респавне, отменял сценарный `true`), `HIGHSPEED_ELASTICITY_MULT_COPCAR` в
  `Physical.cpp` и режимы кинематографической камеры `OBBE_COPCAR`/`OBBE_COPCAR_WHEEL` в `Camera.cpp`.
  Бит 0 флагов машины в `Vehicle::Save/Load` теперь всегда 0 (позиции остальных битов не сдвинуты).
- **`bWaterTight`** (`CAutomobile`/`CBike`) удалён: писался только `false` (в VC ставил скрипт), поэтому утопление
  экипажа теперь не зависит от `IsPlayer()`.
- **`cTransmission::CalculateGearForSimpleCar`** удалён вместе с тестом `TestSimpleTrafficShift`: обслуживал
  `STATUS_SIMPLE`-машины. `m_fMaxCruiseVelocity` оставлен — его читает `PedFight.cpp` (сброс с байка).
- **Аттракторы в `C2dEffect`**: структуры `Attractor`/`PedAttractor` и `ATTRACTORTYPE_*` удалены, `Load2dEffect` для
  `EFFECT_ATTRACTOR`/`EFFECT_PED_ATTRACTOR` хранит только позицию и цвет; значения `EFFECT_*` позиционные — остались.
- **Мёртвые флаги педа** (`bReachedAttractorHeadingTarget`, `bTurnedAroundOnAttractor`, `bHasAlreadyUsedAttractor`,
  `bHasAlreadyStoleACar`, `bCarPassenger`, `bMiamiViceCop`, `bHasAlreadyBeenRecorded` в `CPed` и `CVehicle`) удалены:
  только инициализировались.
- **Константы винта** в `Boat.cpp`: `PLANE_MIN/STD/MAX_PROP_SPEED` → `STD/MAX_PROP_SPEED` (минимальная не читалась).
- **`codewarrior/reVC.mcp.xml`** не правился: CodeWarrior-проект не входит в §6.1, файлы там перечислены списком;
  чистится на H.
- **`CarEnterExit` урезан до 10 методов** (E-доп): 22 метода без вызывающих (SA-таск-API), статик-хелпер и 9 статических полей
  удалены; `Initialise` стал пустым и удалён. Offset drag-out (`ms_vecPedQuickDraggedOutCarAnimOffset`) всегда был
  `(0,0,0)` — единственный писатель значений был мёртвым; параметр удалён вместе с писателем, `nil` в
  `CVehicle::IsRoomForPedToLeaveCar` даёт то же поведение, что нулевой вектор.

## 4. Читы (§3.12)

Удалены целиком (вместе с комбинациями клавиш и пунктами дебаг-меню):
`PickUpChicksCheat`, `WantedLevelUpCheat`, `WantedLevelDownCheat`, `MayhemCheat`,
`EverybodyAttacksPlayerCheat`, `WeaponsForAllCheat`, `ChangePlayerCheat`, `MadCarsCheat`,
`FannyMagnetCheat`, `DoShowChaseStatCheat`, `DoChicksWithGunsCheat`.

Оставлены: `WeaponCheat1/2/3`, `HealthCheat`, `ArmourCheat`, `MoneyCheat`, `VehicleCheat`,
`FastTimeCheat`/`SlowTimeCheat`, все погодные, `StrongGripCheat`, `TrafficLightsCheat`,
`SuicideCheat`, `BlowUpCarsCheat`, `NoSeaBedCheat`, `RenderWaterLayersCheat`,
`OnlyRenderWheelsCheat`, `SpecialCarCheats`, `BackToTheFuture`, `FlyingFishCheat`,
`KangarooCheat`, `WallClimbingCheat`.

Обоснование спорных из §3.12:
- `BlackCarsCheat`/`PinkCarsCheat` **оставлены**: `gbBlackCars`/`gbPinkCars` читаются в
  `VehicleModelInfo.cpp` при выборе цвета модели, то есть действуют на любую машину, включая
  машину игрока, а не на население.
- `WallClimbingCheat` **оставлен**: `gGravityCheat` читается в `Physical.cpp` для машины игрока;
  к педам и препятствиям отношения не имеет. `WALLCLIMB_CHEAT` определён в `config.h`.
- `KANGAROO_CHEAT` **оставлен**: определён в `config.h`, действует на физику игрока.
- `CStats::CheatedCount += 1000` — все строки удалены; само поле уходит вместе с `CStats` на этапе F.

## 5. Мёртвый код (§3.13)

- `vehicles/Train.cpp` в этой ветке **не** был обёрнут в `#ifdef GTA_TRAIN` — поезда реально
  компилировались; система удалена целиком на D-6 (§3.2), а не как мёртвый код.
- `control/Bridge.cpp` — удалён на этапе C (мост был мёртвым грузом).
- `CPacManPickups::Update` — удалён вместе с `CPacManPickups`.
- `GTA_SCENE_EDIT` определён → SceneEdit удалён как система, а не как мёртвый код.
- `MISSION_REPLAY` определён только под `PC_MENU`; его блоки в `Frontend.*` уйдут на этапе G.
- Удалены все `#if 0` в игровых файлах (Camera, Pad, Font, Streaming, Entity, Object, Ped,
  Particle, ControllerConfig, Frontend, main, re3, common.h, Pools). Остались только
  в вендорных/скелетных: `extras/custom/br/third_party/astc_decomp.cpp`, `fakerw/rwplcore.h`,
  `skel/sdl2/sdl2.cpp` — это чужой код, его не трогаем.
- Удалены пустые методы, оставшиеся от систем: `CPopulation::DealWithZoneChange`,
  `CPopulation::MoveCarsAndPedsOutOfAbandonedZones`.
- `FIX_INCOMPATIBLE_SAVES` и весь блок конвертации старых сейвов (`GetSaveType`, `FixSave`,
  `Fix*`) удалены вместе с `#define` в `config.h`: конвертация существовала только ради
  гаражей, кранов, банд, телефонов и рестартов, которых больше нет.
- Мёртвые `#ifdef`-ветки конфига (`SUPPORT_*_SCRIPT`, `USE_ADVANCED_SCRIPT_DEBUG_OUTPUT`,
  `SCRIPT_LOG_FILE_LEVEL`, `USE_DEBUG_SCRIPT_LOADER`, `DONT_FIX_REPLAY_BUGS`) удалены.
- Этап E: `CVehicle::bIsLawEnforcer`/`ChangeLawEnforcerState`, `CWorld::StopAllLawEnforcersInTheirTracks`,
  `CWorld::SetAllCarsCanBeDamaged`, `CPlayerInfo::MakePlayerSafe`, `CAutomobile`/`CBike::bWaterTight`,
  `CVehicleModelInfo::m_vehicleClass/m_frequency/m_nTimesUsed`, `Streaming.cpp`-статикам трафика, `CVehicle::bIsAmbulanceOnDuty/bIsFireTruckOnDuty`,
  `cTransmission::CalculateGearForSimpleCar`, `C2dEffect::{Attractor,PedAttractor}`, мёртвые флаги педа —
  все только инициализировались или имели читателей исключительно в удалённых системах (см. testlog §7.2).
- **`GTA_BRIDGE`** нигде не определён → удалены все 5 веток: `AUDIOTYPE_BRIDGE`,
  `CCarCtrl::ThisRoadObjectCouldMove`/`MapCouldMoveInThisArea` (map больше никогда не «двигается»,
  поэтому охраняемые блоки в `Automobile.cpp`/`Bike.cpp` выполняются безусловно),
  `CPathFind::SetLinksBridgeLights`, `CTrafficLights::ShouldCarStopForBridge`,
  поле `CPathNode::bBridgeLights`.
- **`SECUROM`** нигде не определён (DRM-заглушки) → удалены все 10 блоков вместе с
  `CWeather::ForceHurricaneWeather`, у которой не осталось ни определения, ни вызовов.
- **`MONEY_MESSAGES`** закомментирован в `config.h` → удалена ветка в `Pickups.cpp`;
  сам `CMoneyMessages` остался, его использует `Object.cpp`.
- Удалены `#if 0` в игровых файлах (Camera, Pad, Font, Streaming, Entity, Object, Ped,
  Particle, ControllerConfig, Frontend, main, re3, common.h, Pools). Остались только
  в вендорных/скелетных: `extras/custom/br/third_party/astc_decomp.cpp`, `fakerw/rwplcore.h`,
  `skel/sdl2/sdl2.cpp` — это чужой код, его не трогаем.

## 6. Стиль

- Комментарии в коде — только по-английски и только содержательные; «могильных» комментариев
  о том, «что здесь было», не осталось (проверено grep-ом по diff).
- Никаких `#if 0`, `g_EnableX`, `MP_`/`NoStory`/`Cleanup`/`Legacy`/`Old`/`Temp`/`Hack` тегов.
- Пробелы в именах не появляются: всё, что обязано остаться, переименовано по смыслу. Переименования:
  `CPlayerInfo::m_pVehicleEx` → `m_pSpecialColModelVehicle` (E), `Boat.cpp` `PLANE_*_PROP_SPEED` → `*_PROP_SPEED` (E).
