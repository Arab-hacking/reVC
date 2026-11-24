# Журнал проверок

## 0. Условия проверки

Полная сборка приложения в этом окружении не выполняется: отсутствуют системные dev-пакеты
OpenAL, mpg123, GL и X11. Для статической проверки используются скрипты `.harness/`:

| Скрипт | Что делает | Результат |
|---|---|---|
| `.harness/check.sh` | компилирует каждый `.cpp` из `src/` в отдельный TU (`g++ -c -O0 -w -std=c++11 -DRW_GL3 -DLIBRW -DCMAKE_NO_AUTOLINK -DUSE_OUR_VERSIONING -DNDEBUG -DAUDIO_OAL`, все `src/**` в `-I`) | **OK: 217 FAIL: 0** после F, шаг 3b |
| `.harness/linkcheck.sh` | собирает undefined-символы TU и вычитает определения и libc/libstdc++ | **0 reVC-looking** после F, шаг 3b |
| `.harness/dangling.py src` | сканирует dangling `if`/`for`/`while` после удаления тел | **0** после F, шаг 3b |
| `.harness/wallcheck.sh` | выдаёт unused-variable warnings по указанным TU | 33 изменённых `.cpp` прогнаны; warning-only результат не сравнивался с базовой ревизией |
| `cmake -S tests -B …` | настройка standalone C++ test suite | не запущено: в окружении нет исполняемого `cmake` (`command not found`) |

Harness не заменяет приёмку настоящей сборкой обеими системами (premake + MSVC,
CMake + GCC/Clang) — см. §6.1 задания. Скрипты harness не входят в git.

Прогон по этапам:

| Этап | Коммит | TU | Результат |
|---|---|---|---|
| База | `8efb3bc` | 272 | OK 272 / FAIL 0 |
| B — скриптовый движок | `64bcef4` (все этапы B–E — один коммит, см. ниже) | 267 | OK 267 / FAIL 0 |
| C — миссионные системы и мёртвые ветки | там же | 247 | OK 247 / FAIL 0 |
| D-1 — население и NPC-классы | там же | 236 | OK 236 / FAIL 0 |
| D-2 — ядро целей транспорта | там же | 236 | OK 236 / FAIL 0 |
| D-5/D-6 — `PlayerInfo`, `Ped.cpp`, `PedType`, поезд | там же | 233 | OK 233 / FAIL 0 |
| E — трафик, AI-транспорт, вертолёты/самолёты | там же | 225 | OK 225 / FAIL 0, linkcheck чистый |

История ветки была пересоздана при пересоздании песочницы: поэтапные коммиты (`680fc1d`, `771fdeb`,
`01f7b47`, `b2709d2`) не сохранились, всё состояние B–E вошло в один коммит `64bcef4`. Числа TU
по этапам — из журналов прогонов на момент работы; перед пушем состояние перепроверено (`check.sh` 225/0,
`linkcheck.sh` без reVC-символов).

## 1. Grep-гейты §6.2

Состояние после этапа E (A–E). «0» — имя не встречается в `src/` вообще. Все этапы B–E
в этой ветке — один коммит `64bcef4` (см. §0).

| Группа | Символы с нулём | Осталось (этап) |
|---|---|---|
| Скрипты | `CTheScripts`, `CRunningScript`, `ScriptSpace`, `COMMAND_`, `main.scm`, `SIZE_MAIN_SCRIPT`, `SaveAllScripts`, `LoadAllScripts` | `CTheScripts` ×3 в `save/MemoryCard.cpp` под `PS2_MENU` (F) |
| Миссионные системы | `CDarkel`, `CPhoneInfo`, `CSetPieces`, `CSceneEdit`, `COnscreenTimer`, `CPager`, `CCutsceneMgr`, `CCutsceneObject`, `CRoadBlocks`, `CCredits`, `CPacManPickups`, `CMovingThings`, `CBridge`, `CScriptPaths`, `CRopes`, `CWaterCreatures`, `CReplay`, `CRecordDataFor`, `CEventList`, `CAccidentManager` | `CGarages`/`CRestart`/`CCranes` ×2 в `MemoryCard.cpp` под `PS2_MENU` (F) |
| Читы | `PickUpChicksCheat` | `CheatedCount` ×8 в `Stats.*` (F) |
| Педы и AI | `EmergencyPed`, `PedChat`, `IsPlayerOnAMission`, `CPopulation`, `PedAI`, `CivilianPed`, `CopPed` (класс; `pStoredCopPed` в `Cam.cpp` — камера ареста, F), `PedAttractor` (структура 2d-эффекта удалена на E), `PedRoutes`, `PedType`/`CPedType`/`CPedStats`/`m_pedStats`/`m_pedStatType`/`m_carsCanDrive`/`PEDSTAT_` | `PEDSTATS.DAT` 1 и `PEDGRP.DAT` 1 — список файлов образа в `main.cpp` (H) |
| Трафик | `CCarAI`, `CTheCarGenerators`, `ThePaths`, `CPathFind`, `GenerateRandomCars`, `AddPedInCar`, `RegisterVehicleOfInterest`, `RemoveDistantCars`, `CHeli`, `CPlane`, `SpecialPlateHandler`, `NUM_PATHNODES` | — (E) |
| Прогресс/полиция/сейвы | — | `CWanted` 39, `m_pWanted` 5, `CStats::` 316, `TheMemoryCard` 189, `WBSTATE_` 20, `BUSTEDAUDIO_` 5, `SavePlayerInfo`/`LoadPlayerInfo` 4/4, `CheatedCount` 8 (F) |
| Прочее | `eWaitState`, `m_pedInObjective`, `m_nearPeds`, `m_numNearPeds`, `m_fearFlags`, `bIsLawEnforcer` (E), `m_pVehicleEx` (переименован на E) | `OBJECTIVE_` 91 и `m_objective` 61 — только транспортные цели игрока (D-2), `eObjective` 9 (позиционный enum) |
| Ложные срабатывания `rg` | `COMMAND_` (`_APS_NEXT_COMMAND_VALUE` в `skel/win/resource.h`), `PedType` (`ePedType` в `Ped.h`), `CRestart`/`CGarages`/`CCranes`/`CTheScripts` (`MemoryCard.cpp` под `PS2_MENU`, F) | |

## 2. Поведенческие тесты §6.3

**Не выполнялись**: для них нужен работающий бинарник, которого в этом окружении нет.
Таблица подготовлена для прогона на реальной сборке; колонка «Проверено» отмечает,
что подтверждено статически.

| № | Тест | Ожидание после этапа C | Проверено |
|---|---|---|---|
| 1 | Запуск: карта грузится, игрок в мире, камера на игроке | игрок создаётся `CGame::CreatePlayer()`, камера стоит на нём | компиляция + код-ревью `Game.cpp` |
| 2 | Пустота мира: 10 минут простоя | ноль педов/машин/вертолётов/самолётов, лог скриптов пуст | статически: `CTheScripts`, `CPopulation`, `CCarAI`, `CTheCarGenerators`, `CHeli`/`CPlane` удалены; в игре — pending (§7.5) |
| 3 | Игрок: ходьба, бег, прыжок, плавание, утопление, огонь, падение, смерть → респавн | смерть → `WBSTATE_PLAYING → PED_DEAD → KillPlayer()` → респавн на `(79.6875, -867.25)` | компиляция + код-ревью `GameLogic::Update` |
| 4 | Оружие: слоты, прицел, перезарядка, взрывы, гранаты, drive-by | без изменений | компиляция |
| 5 | Транспорт: спавн из дебаг-меню, посадка/выход, взрыв, лодка, вертолёт, самолёт | все точки спавна идут через `CGame::SpawnVehicle` (этап E, §7); вертолёт/самолёт спавнятся как `CAutomobile` с флагами handling | компиляция + код-ревью; в игре — pending (§7.5) |
| 6 | HUD: только разрешённые элементы | HUD ещё не урезан (этап G) | компиляция |
| 7 | Фронтенд: пауза, настройки, язык, выход | без изменений | компиляция |
| 8 | Аудио: радио, выстрелы, шаги; нет миссионных реплик и полицейского радио | без изменений | компиляция |
| 9 | Стабильность: 30 минут, Alt-Tab, смена разрешения | — | не проверялось |
| 10 | Мёртвый код: warnings, `#if 0`, grep §6.2 | `#if 0` в игровых файлах — 0 | grep |

## 3. Известные ограничения проверки

- Компиляция идёт с `-w`: предупреждения не ловятся. Требуется прогон с `-Wall -Wextra`
  на реальной сборке (этап H).
- `check.sh` не линкует: дубликаты символов и ошибки линковки ловятся только
  `linkcheck.sh` и только для reVC-символов. Каждый hit `linkcheck.sh`, похожий на reVC-имя,
  надо проверять `grep -n "^Class::name"` по определениям, а не по вызовам: на D-1 два
  определения были съедены соседним удалением, а журнал принял их за ложные срабатывания
  фильтра (исправлено на E, §7.4).
- Часть `#ifdef` в конфиге — реальные опции сборки (`AUDIO_OAL`, `USE_TEXTURE_POOL`,
  `SQUEEZE_PERFORMANCE` и т.п.), они задаются системой сборки, а не `config.h`,
  поэтому «никогда не компилирующимися» не считаются.

## 4. Этап D, часть 1 — удаление населения и классов NPC

| Проверка | Результат |
|---|---|
| `check.sh` (все TU) | **OK: 236 FAIL: 0** |
| Удалено файлов | 12 пар: `Population`, `PedAI`, `PedChat`, `CivilianPed`, `CopPed`, `PedAttractor`, `PedRoutes`, `PedPlacement`, `PedDebug`, `Stinger`, `EmergencyPed`, `Gangs` |
| `PedChat.cpp` | тело перенесено в `Ped.cpp` (EOF) |
| `PedAI.cpp` | функции посадки/высадки перенесены в `Ped.cpp`; файл удалён |

**Незакрытый вопрос линковки.** `CPed::SetObjective`, `ClearObjective`,
`SetObjectiveTimer`, `RestorePreviousObjective`, `ProcessObjective`,
`ReactToAttack`, `ReactToPointGun`, `RegisterThreatWithGangPeds`,
`CanBeDamagedByThisGangMember`, `SelectGunIfArmed` больше не имеют определений:
их тела жили в `PedAI.cpp` и были удалены вместе с NPC-логикой. Вызовы остались
в `PlayerInfo.cpp` (посадка игрока в машину), `Pad.cpp`, `Ped.cpp`, `PedFight.cpp`,
`Weapon.cpp`, `World.cpp`, `Cam.cpp`, `CarAI.cpp`.

Причина не удалена «мягко»: игрок в этом движке садится в машину через
`SetObjective(OBJECTIVE_ENTER_CAR_AS_DRIVER)` → `ProcessObjective()` → `SeekCar()`
→ `SetEnterCar()`. Поэтому ядро целей должно быть возвращено в `Ped.cpp` в урезанном
виде — только цели транспорта (`ENTER_CAR_AS_DRIVER/PASSENGER`, `LEAVE_CAR`,
`FLEE_CAR`, `LEAVE_CAR_AND_DIE`, `WAIT_IN_CAR`). Остальные ~45 целей (`KILL_CHAR_*`,
`FLEE_*`, `GOTO_*`, `WANDER`, `SOLICIT_*`, `MUG_CHAR`, `STEAL_ANY_CAR`, `CATCH_TRAIN`
и т.д.) — NPC-поведение и удаляются безвозвратно.

Следующий шаг: вернуть эти функции из `PedAI.cpp` (по базовой ревизии `8efb3bc`), вырезать
NPC-ветви, добить линковку (`linkcheck.sh` → 0 пропущенных проектных символов).

## 5. Этап D, часть 2 — ядро целей транспорта, чистка `Ped.h` и читателей NPC-флагов

| Проверка | Результат |
|---|---|
| `check.sh` (все TU) | **OK: 236 FAIL: 0** |
| `linkcheck.sh` | 0 пропущенных reVC-символов. После D-1 висели 3 (`SwitchDebugDisplay`, `GetDebugDisplay`, `DebugRenderOnePedText` — тела жили в удалённом `PedDebug.cpp`): удалены чит «PEDDEBUG» в `Pad.cpp`, блок в `Renderer.cpp` и объявления в `Ped.h` |
| Удалено файлов | 0 |
| `Ped.cpp` / `PedFight.cpp` / `PlayerPed.cpp` | 0 ошибок, `-w` |

### 5.1 Перенос из `PedAI.cpp` → `Ped.cpp` (тела лежали в `Ped.cpp` с D-1, здесь вырезаны NPC-ветви)

| Функция | Что вырезано / как выглядит теперь |
|---|---|
| `IsTemporaryObjective` | временные — только `LEAVE_CAR`, `LEAVE_CAR_AND_DIE`, `ENTER_CAR_AS_DRIVER`, `ENTER_CAR_AS_PASSENGER` |
| `SetObjective(eObjective)` | guard `DyingOrDead()||m_attachedTo`; `NONE` → обе цели в `NONE`; иначе если цель не та же: текущая временная → меняется только `m_prevObjective`, иначе `m_objective`; `bObjectiveCompleted = false`. Убраны `m_leader`, `SetObjectiveTimer`, ветки всех NPC-целей |
| `SetObjective(eObjective, void*)` | guards `DyingOrDead`, `entity == this`, `m_prevObjective == newObj`, дубликат (`LEAVE_*` — return, `ENTER_*` с тем же `m_carInObjective` — return), `LEAVE_*` требует `bInVehicle`; `bObjectiveCompleted = false; ClearPointGunAt()`; предыдущая цель запоминается, если текущая не временная и новая не `ENTER_*`; `LEAVE_*` → `m_carInObjective = entity` (+ для автобуса ступенчатый `m_leaveCarTimer`); `ENTER_*` → бег если стоял, ссылки `m_carInObjective`/`m_pSeekTarget`, `m_vecSeekPos = 0` |
| `ClearObjective` | обе цели в `NONE`, `m_carInObjective = nil`; в машине не за рулём → `SetObjective(LEAVE_CAR, m_pMyVehicle)`; иначе `SetIdle(); SetMoveState(STILL)`; иначе `bClearObjective = true` |
| `RestorePreviousObjective` | `m_prevObjective ↔ m_objective` (заменило `SetStoredObjective`), `bObjectiveCompleted = false` |
| `ProcessObjective` | единственный исполнитель выхода игрока: `bClearObjective` → `ClearObjective`; требует `IsPedInControl()||PED_DRIVING`; `LEAVE_CAR_AND_DIE` → лодка/автобус или `SetBeingDraggedFromCar`; `LEAVE_CAR` → `SetExitBoat`/`SetExitCar(..,0)` (игрок с зажатым газом остаётся), иначе `RestorePreviousObjective`; `ENTER_CAR_AS_PASSENGER` при полном салоне → restore + `m_pMyVehicle = nil`, далее общая ветка с `DRIVER`: сидит не там → `SetExitCar`; приехал → `bObjectiveCompleted = true; RestorePreviousState()`; перевёрнута → restore; `SetSeekCar`/`SetSeekBoatPosition`; отмена при здоровье ≤ 0 или `ENTER_CAR_MAX_DIST < dist`; хвост `if (bObjectiveCompleted) { RestorePreviousObjective(); RestorePreviousState(); ClearAimFlag(); ClearLookFlag(); }` |
| `SetSeekBoatPosition`, `SeekBoatPosition`, `IsRoomToBeCarJacked`, `AddInCarAnims`, `RemoveInCarAnims` | перенесены без изменений |
| `SeekCar` | убраны ветки поезда, solicitation, лидера и бегства |
| `SetPedPositionInTrain` | добавлен (использовался `World.cpp`) |

Удалены перегрузки `SetObjective(int16,int16)`, `SetObjective(float,const CVector&)`, `SetObjective(CVector)`.

### 5.2 Удалённые вызовы (тела не восстанавливались)

| Файл | Удалено |
|---|---|
| `peds/Ped.cpp` | ветка `bFleeAfterExitingCar || bGonnaKillTheCarJacker` в колбэке закрытия двери; `BuildPedLists` + глобальные `gapTempPedList`/`gnNumTempPedList`; `SortPeds`; тело `CollideWithPed` в `ENTITY_TYPE_PED` → `break` |
| `peds/Ped.h` | `eObjective` → 5 значений; `eWaitState`, `m_nWaitState`, `m_pedInObjective`, `m_objectiveTimer`, `m_leader`, `m_threatEntity`, `m_fearFlags`, `m_pedStats`, `m_nearPeds`, `m_numNearPeds`, `bFleeAfterExitingCar`, `bGonnaKillTheCarJacker`, `ChooseAttackAI`, `CollideWithPed`, `PedSetGetInCarPositionCB`, `PlayRandomAnimationsFromAnimBlock`, debug-блок `#ifndef MASTER` (`SwitchDebugDisplay` и др.), `bPedCheat3`, `ms_vec2DFleePosition`, 3 перегрузки `SetObjective` |
| `peds/PedFight.cpp` | `ChooseAttackAI`, `CollideWithPed` (call sites → `ChooseAttackPlayer`); `ReactToAttack` ×4, `RegisterThreatWithGangPeds`, `SelectGunIfArmed`, `m_leader`/gang-блок в `KillPedWithCar`, `WAITSTATE_*` guards, `RemovePedFromMeleeList`, kill-objective disjunct в `ChooseAttackPlayer`; `m_nWaitTimer` → `m_animTimer` (общий с `m_jumpHitTimer`) |
| `peds/PlayerPed.cpp/.h` | `UpdateMeleeAttackers`, `RemovePedFromMeleeList`, `GetMeleeAttackCoords`, `FindMeleeAttackPoint`, `KeepAreaAroundPlayerClear` + поля; init `m_fleeFromPos/m_fleeFrom/m_fleeTimer`; `m_leader != this` |
| `vehicles/Vehicle.cpp` | `bFleeAfterExitingCar`, `m_objectiveTimer`, ступенчатый `time` для пассажиров горящей машины |
| `vehicles/Automobile.cpp`, `Bike.cpp` | `KeepAreaAroundPlayerClear()`, `m_leader` в ветке перевёрнутой машины |
| `weapons/Weapon.cpp` | `ReactToAttack` ×4, `ReactToPointGun`, `CanBeDamagedByThisGangMember`, `m_leader`, `IsGangMember` ×2, `SetFlee` по `m_nearPeds`, `OBJECTIVE_FLEE_ON_FOOT_TILL_SAFE`/`KILL_CHAR_ON_FOOT` в plough-through, блок «пассажиры выпрыгивают» |
| `weapons/ShotInfo.cpp` | `SetFindPathAndFlee`; цикл по `m_nearPeds` → `CWorld::SetPedsOnFire` |
| `core/Fire.cpp` | `SetFlee` ×3, `m_fleeFrom`, `m_fleeTimer`, `SetWanderPath`, `SetWaitState(FINISH_FLEE)`; NPC при пожаре переходит в `PED_ON_FIRE` на месте |
| `core/World.cpp` | `SetFlee` (слезоточивый газ), `RegisterThreatWithGangPeds` (взрыв) |
| `core/Cam.cpp`, `Camera.cpp` | чтение `m_nearPeds` → `FindNearPeds` (камера над педами, near-clip, arrest-cam) |
| `core/Pad.cpp`, `renderer/Renderer.cpp` | чит «PEDDEBUG» и отладочный вывод педов (см. линковку выше) |
| `control/CarAI.cpp` | `OBJECTIVE_KILL_CHAR_ANY_MEANS` у водителя |
| `control/CarCtrl.cpp` | бегство NPC от машины игрока (`SetFlee`, `bUsePedNodeSeek`), осталась только реакция игрока (взгляд/кулак) |

### 5.3 Замены вместо удаления (минимальные корректные)

| Было | Стало |
|---|---|
| `m_nearPeds`/`m_numNearPeds` (кэш 10 ближайших на педа, пересобиравшийся раз в 16 кадров) | `CPed::FindNearPeds(centre, radius, out, maxPeds)` — запрос по секторам мира при вызове, та же семантика (без `this`, без педов в транспорте кроме байков, ≤3 мёртвых, сортировка по расстоянию). Читатели: `Ped.cpp` ×2, `PedFight.cpp` ×4, `Weapon.cpp`, `Cam.cpp` ×3, `Camera.cpp` |
| `m_pedStats->m_fAttackStrength` / `m_fDefendWeakness` | множители сняты (поле не инициализировалось с D-1): `damageMult` без изменения, `healthImpact = damage` |
| `STAT_SHOPPING_BAGS`, `STAT_ONE_HIT_KNOCKDOWN` | дизъюнкты удалены |
| `IsGangMember()` в условии урона `Weapon.cpp` | `m_nPedType != PEDTYPE_COP` (оставшийся операнд конъюнкции) |
| огнемёт поджигал педов из `m_nearPeds` стрелка | `CWorld::SetPedsOnFire(shot.m_startPos, radius, source)` — тот же радиус, тот же `StartFire(..., 0.8f, true)` |
| NPC после пожара: `SetWanderPath` + `WAITSTATE_FINISH_FLEE` | `SetIdle()` как у игрока |

### 5.4 Grep-гейты §6.2 после D-2

| Запрещено (`rg -c` по `src/`) | 0 |
|---|---|
| `m_objectiveTimer`, `eWaitState`, `m_pedInObjective`, `m_nearPeds`, `m_numNearPeds`, `m_fearFlags`, `m_pedStats`, `SetObjectiveTimer`, `ReactToAttack`, `ReactToPointGun`, `RegisterThreatWithGangPeds`, `CanBeDamagedByThisGangMember`, `SelectGunIfArmed`, `SetStoredObjective`, `bFleeAfterExitingCar`, `bGonnaKillTheCarJacker`, `WAITSTATE_` | все **0** |

| Разрешено | Вхождений | Файлы |
|---|---|---|
| `OBJECTIVE_NONE` / `LEAVE_CAR` / `ENTER_CAR_AS_DRIVER` / `ENTER_CAR_AS_PASSENGER` / `LEAVE_CAR_AND_DIE` | 21 / 29 / 28 / 16 / 12 | другие `OBJECTIVE_*` — 0 |
| `m_objective` / `m_prevObjective` / `m_carInObjective` | 65 / 12 / 47 | |
| `SetObjective(` | 20 | `Ped.{h,cpp}`, `PedFight`, `Vehicle`, `Automobile`, `PlayerInfo`, `Pad` |
| `ClearObjective(` / `RestorePreviousObjective(` / `ProcessObjective(` / `IsTemporaryObjective(` | 5 / 19 / 3 / 4 | |
| `SeekCar(` / `SetSeekCar(` / `SeekBoatPosition(` / `SetSeekBoatPosition(` / `IsRoomToBeCarJacked(` | 9 / 3 / 7 / 4 / 3 | `SeekBoatPosition`/`SetSeekBoatPosition` ещё вызываются из `PlayerInfo.cpp` (D-5) |
| `AddInCarAnims(` / `RemoveInCarAnims(` | 4 / 5 | |
| `GetCarDoorFlag(` / `GetEnterCarDoorFlag(` / `GetBikeDoorFlag(` / `GetBikeDoorFlagInclJumpInFromFront(` | 8 / 2 / 3 / 4 | не трогались |

Поезд (`PED_ENTER_TRAIN` 3, `PED_EXIT_TRAIN` 4, `SetPedPositionInTrain` 3) закрыт не правкой
ссылок, а удалением системы целиком — см. §6.3.

### 5.5 §6.3 п.5 — посадка/высадка игрока (статическая трасса)

`Pad.cpp` (кнопка) → `CPlayerInfo::Process` → `SetObjective(OBJECTIVE_ENTER_CAR_AS_DRIVER|PASSENGER, veh)`
→ `CPed::ProcessObjective` → `SetSeekCar` → `SeekCar` → `SetEnterCar` → `CarEnterExit.cpp`;
выход → `ProcessObjective(LEAVE_CAR)` → `SetExitCar`. Вызовы в `PlayerInfo.cpp` скомпилированы
с новым ядром (0 ошибок). Полное подтверждение — после удаления `ThePaths` на этапе E.

## 6. Этап D, части 3–4 — D-5 (`PlayerInfo`), D-6 (`Ped.cpp`), `PedType`, поезд

| Проверка | Результат |
|---|---|
| `check.sh` (все TU) | **OK: 233 FAIL: 0** (236 − `PedType.cpp` − `Train.cpp` − `TrainNode.cpp`) |
| `linkcheck.sh` | 0 undefined |
| Гейты §6.2 (`rg` по запрещённому списку) | пусто |
| `rg "ThePaths\|PathFind\|CPathNode"` по `src/peds/` | пусто |

### 6.1 D-5 — посадка/высадка игрока в `PlayerInfo.cpp`

Ветка входа в машину возвращена из базовой ревизии `8efb3bc` в урезанном виде: кнопка →
`SetObjective(OBJECTIVE_ENTER_CAR_AS_DRIVER | AS_PASSENGER, carBelow)` только когда игрок стоит на земле
и не в транспорте; выход — `SetObjective(OBJECTIVE_LEAVE_CAR, veh)` только когда игрок за рулём/пассажиром
и машина достаточно медленная. NPC-ветки (поиск машины «для ограбления», `FindClosestCarSectorList`
для ботов, `SeekBoatPosition` для NPC) не возвращались. `CancelPlayerEnteringCars` осталась без изменений.

### 6.2 D-6 — чистка `Ped.cpp` / `Ped.h` / `PlayerPed.cpp` / `Vehicle.cpp`

| Что удалено | Где | Почему |
|---|---|---|
| Блок «rolling door» (`PED_DRIVING && m_objective != OBJECTIVE_LEAVE_CAR` → `ROLLDOOR`-анимация) | `PlayerPed.cpp::ProcessControl` | §3.10 п.10; заменился на `if (m_nPedState == PED_DRIVING) return;` |
| «Экипаж разбегается» при уроне (`RANDOM_VEHICLE && !IsBoat()` → `SetObjective(OBJECTIVE_LEAVE_CAR)` + `SOUND_PED_FLEE_SPRINT`) | `Vehicle.cpp::InflictDamage` | §2.6: ветка под `STATUS_SIMPLE/PHYSICS`, у машины игрока `STATUS_PLAYER` — недостижима; NPC-водителей больше нет |
| `PedAnimDoorCloseRollingCB` | `Ped.cpp`, `Ped.h` | единственный вызов — в удалённом rolling-door блоке |
| Fallback `ThePaths.FindNodeClosestToCoors` в `PositionPedOutOfCollision` | `Ped.cpp` | навигация NPC; теперь `return foundAPos;` |
| Побег с `PEDMOVE_RUN` (blend 1.0 → run) | `Ped.cpp::ProcessControl` | NPC-flee; у игрока всегда 1.0 |
| `m_nPedState == PED_WANDER_PATH` в условии столкновения | `Ped.cpp::ProcessCollision` | NPC-состояние, осталось `IsPlayer()`, либо `m_fCollisionSpeed >= 1.0f` |
| `#include "PathFind.h"`, поля `m_nNumPathNodes`, `m_pathNodeTimer`, `m_pathNodeObjPool[8]`, `m_pCurPathNode`, `m_pLastPathNode` | `Ped.h` | читателей нет ни в одном TU |
| `PED_MUG` / `PED_FLEE_ENTITY` в guard `SetIdle`, ветка `PED_FLEE_POSITION` в `SetExitCar` | `Ped.cpp` | состояния больше никто не ставит; оставлена ветка игрока (`PED_IDLE`, `PEDMOVE_STILL`) |

Списки состояний `CanPedReturnToState` / `CanStrafeOrMouseControl` и `switch` в `PlayerPed.cpp`
не трогали: `ePedState` — позиционное перечисление, нумерация попадает в сейв (см. `SaveVersion.h`),
поэтому мёртвые значения (`PED_WANDER_*`, `PED_FLEE_*`, `PED_CHAT`, `PED_MUG`, `PED_SOLICIT`, …)
остаются как метки без кода, который их ставит.

### 6.3 `PedType.h` — удалён

`CPedType`/`CPedStats` читали `ped.dat`/`pedstats.dat` (отношения NPC) и писали блок в сейв. После D-1
не оставалось ни одного читателя `CPedType::GetPedFlag`/`CPedStats::GetPedStat`. Удалены: `PedType.{h,cpp}`,
вызовы `Initialise/Shutdown/Save/Load` в `Game.cpp`, `GenericGameStorage.cpp`, `MemoryCard.cpp`,
`AnimViewer.cpp`. Оставлены и перенесены в `Ped.h`: `ePedType` (32 значения, SA-нумерация; читается
`Stats.cpp`, `Stats.h::PedsKilledOfThisType[NUM_PEDTYPES]`, `Ped.h::IsFemale`, автоприцел) и
`eMoveState`/`PEDMOVE_*` (13 файлов). Мёртвые `PED_FLAG_*`, `ePedThreatFlags`, `ePedRace` ушли вместе с файлом.
`Stats.h` теперь включает `Ped.h`. Сейв-блок `PedTypeSize` больше не пишется/не читается — формат
сейва этого форка и так помечен несовместимым (`SaveVersion.h`).

### 6.4 Поезд — `CTrain` удалён целиком

`Train.cpp/h`, `TrainNode.cpp/h` удалены; `VEHICLE_TYPE_TRAIN`, `STATUS_TRAIN_*`, `MI_TRAIN`,
`PED_ENTER_TRAIN`/`PED_EXIT_TRAIN`, `IsTrain()`/`IsSubTrain()`, `FindPlayerTrain`, `SetPedPositionInTrain`,
`CTrainDoor`, `eTrainPositions`, `SetAtomicRendererCB_Train`, `RenderTrainHiDetail*`, `bHitByTrain`,
`ScanSectorList_Subway`, `LoadTrainCamNodes`, `Process_Train_Camera_Control`, `CTrainCamNode`,
`MODE_CAM_ON_TRAIN_ROOF`…, `m_bTargetJustBeenOnTrain`/`CameOffTrain`, `m_bUseSpecialFovTrain`,
`m_fFovForTrain`, `m_bAboveGround/BelowGroundTrainNodesLoaded`, `StartShake_Train`, `m_bInitialNodeFound`
/`m_bInitialNoNodeStaticsSet` — удалены вместе с последними читателями. `World.cpp`-цикл посадки теперь
`if (bInVehicle || EnteringCar()) { … switch (m_nPedState) … }` без ветки поезда.
`Renderer.cpp::ScanWorld` потерял ветку «камера в метро» (сканирование `ScanSectorList_Subway`) —
подземной ветки железной дороги в сборке нет. `Pools.cpp`: тип 2 легаси-сейва больше не мапится
(осталось в комментарии). `SoundList`: `SOUND_TRAIN_DOOR_*`, `SAMPLEBANK_TRAIN`, `SFX_TRAIN_*` —
позиционные значения SA-списка, оставлены (аудио-этап). `CarGen.cpp::Process` больше не проверяет
`FindPlayerTrain()`. `main.cpp`-список файлов `PEDSTATS.DAT`/`PEDGRP.DAT`/`PED.DAT` не трогали —
это список файлов образа, чистится на этапе H вместе с остальными данными.

### 6.5 Открытые пункты после D

- `Bike.cpp:1804/1817` (`pDriver->IsPlayer() || !bWaterTight`) — сделано на E: `bWaterTight` удалился целиком
  (писался только в `false`), условие стало безусловным.
- `Weapon.cpp:756–770` — сделано на E: блок `DRIVINGSTYLE_PLOUGH_THROUGH` удалён (писал только `AutoPilot`).
- Контроль: оставшиеся `m_objective` вне peds:
  `Cam.cpp`, `PlayerInfo.cpp` ×2, `Automobile.cpp` ×2, `Vehicle.cpp` — все с разрешёнными значениями.

## 7. Этап E — трафик, AI-транспорт, вертолёты и самолёты

| Проверка | Результат |
|---|---|
| `check.sh` (все TU) | **OK: 225 FAIL: 0** (233 − `CarAI` − `CarGen` − `PathFind` − `LoadedCarGroup` − `IniFile` − `Heli` − `Plane` − `SpecialPlateHandler`) |
| `linkcheck.sh` | 225 объектов, 0 ошибок компиляции; неопределённых reVC-символов нет (392 оставшихся имени — librw/libc, вне harness) |
| `wallcheck.sh` по 40 изменённым TU против `HEAD` | новых `unused variable` нет (7 найденных по ходу убраны: `wheelRot` в `Automobile`/`Bike`, `i` в `Camera::TryToStartNewCamMode`, `processTime`/`startTime` в `Game::Process`, `i, model` и `pVehicleInfo` ×3 в `Streaming.cpp`) |
| Гейты §6.2, E-часть (`CCarAI`, `CTheCarGenerators`, `ThePaths`, `CPathFind`, `GenerateRandomCars`, `AddPedInCar`, `RegisterVehicleOfInterest`, `RemoveDistantCars`, `CHeli`, `CPlane`, `Jumbo`, `SpecialPlateHandler`, `bIsLawEnforcer`, `bIsAmbulanceOnDuty`, `bIsFireTruckOnDuty`, `NUM_PATHNODES`, `m_pVehicleEx`, `PedAttractor`) | все **0** |
| Файлы | 88 файлов: −18 (17 пар `.cpp/.h` + тест), +640 / −12 761 строк |

### 7.1 Удалённые файлы

`control/CarAI.{cpp,h}`, `control/PathFind.{cpp,h}`, `core/IniFile.{cpp,h}`, `core/WorldRecovery.h`,
`vehicles/CarGen.{cpp,h}`, `vehicles/Heli.{cpp,h}`, `vehicles/LoadedCarGroup.{cpp,h}`,
`vehicles/Plane.{cpp,h}`, `vehicles/SpecialPlateHandler.{cpp,h}`, `tests/WorldRecoveryTests.cpp`
(тестировал `WorldFindNearbyRecoveryNode` из `WorldRecovery.h`; вместе с блоком в `tests/CMakeLists.txt`).

`CCarCtrl` сведён к фабрике: `CarCtrl.{h,cpp}` содержат только `GetNewVehicleDependingOnCarModel`.
Типы `HELI`, `PLANE`, `FPLANE`, `FHELI`, для которых базовая ревизия строила `CHeli`/`CPlane`, фабрика теперь
строит как `CAutomobile` (в upstream reVC фабрика этих классов тоже не знает): модель с флагами handling
`m_bIsHeli`/`m_bIsPlane` получает физику винта/крыла из `Automobile.cpp` (`IsRealHeli()`/`IsRealPlane()`).

### 7.2 Удалённые вызовы (тела не восстанавливались)

| Система | Где вычищалось | Что осталось |
|---|---|---|
| `CCarAI` (миссии `MISSION_*`, `DRIVINGSTYLE_*`, `SteerAI*`, `Add*Occupants`, `TellOccupantsToLeaveCar`, `FindPoliceCar*`) | `AutoPilot.{h,cpp}` (`eCarMission` и поля остались данными; `AutoPilot.cpp` удалён на F-3b как сериализационный), `Vehicle.cpp`, `Automobile.cpp`, `Boat.cpp`, `Bike.cpp`, `Weapon.cpp` (разгон `PLOUGH_THROUGH`), `Streaming.cpp` (`StreamVehiclesAndPeds` лишился цикла по моделям), `AudioLogic.cpp` (`STATUS_SIMPLE` ×4) | `AutoPilot.h` хранит структуру до F-8: конструктор транспорта и сброс мёртвого водителя в `PedFight.cpp` используют её поля |
| `CTheCarGenerators` / `CPathFind` / `ThePaths` | `Game.cpp` (`Init`, `Process`, `ReInit`, `Shutdown`, `LoadPathFindData`), `re3.cpp` (дебаг-переменные), `Zones.{h,cpp}` (`CZoneInfo`, `ZoneInfoArray`, `TotalNumberOfZoneInfos`, `zoneinfoDay/Night`), `FileLoader.cpp` (`LoadPathFindData`, секции ZONE/CULL/OCCL/PICK/PATH в `LoadScene`), `Streaming.cpp` (`LoadAllRequestedModels`-независимые статики `ms_currentPedGrp` и т.п.), `Zones.cpp`, `Renderer.cpp`, `main.cpp` (список `paths.ipl`/`navig.zon`/`info.zpl`), `Pad.cpp` (`gbShowPedPaths/CarPaths/CarPathsLinks`), `World.cpp` (`RemoveFallenCars` → удаление), `Ped.cpp` (`PositionPedOutOfCollision` без `FindNodeClosestToCoors`), `GenericGameStorage.cpp`/`MemoryCard.cpp` (блоки `CTheZones::Save/Load` без path-данных) | ничего |
| Random heli/plane (`CHeli`, `CPlane`, `ProcessJumbo*`, `AIRTRAIN`, `CPlaneTrails`, `CPlaneBanners`, `SpecialHeliPreRender`, `InitHelis/Planes`, `UpdateHelis/Planes`) | `Game.cpp`, `Renderer.cpp`, `Weapon.cpp` (4 × `TestBulletCollision`), `ProjectileInfo.cpp`, `BulletInfo.cpp`, `main.cpp`, `AudioLogic.cpp` + `AudioManager.h`, `Fluff.{h,cpp}`, `Vehicle.cpp` (`IsVehiclePointerValid`), `re3.cpp`, `Streaming.cpp` (запрос `MI_CHOPPER` по розыску), `Boat.cpp` (`Predator`-блок), `Automobile.cpp` | `eHeliNodes`/`ePlaneNodes` в `VehicleModelInfo.h`, `m_bIsHeli`/`m_bIsPlane`, `IsRealHeli()`-физика в `Automobile.cpp`, `CSmokeTrails`/`CEscalators` в `Fluff.cpp` (читаются `Pad.cpp` и аудио), `CEscalator`-тесты в `tests/CMakeLists.txt` |
| `SpecialPlateHandler` | `Boat.{cpp,h}` (`m_nPoliceShoutTimer`), `Vehicle.cpp` | ничего |
| `RegisterVehicleOfInterest` / `RemoveDistantCars` / `RemoveCarsIfThePoolGetsFull` | `Vehicle.h`, `World.cpp`, `Streaming.cpp` | ничего |
| `CIniFile` (ini-настройки TrafficLights/`TimeInfo`) | `Game.cpp`, `re3.cpp`, `main.cpp` | ничего |
| `bIsAmbulanceOnDuty` / `bIsFireTruckOnDuty` | `Vehicle.{h,cpp}` (только инициализация `false`, читателей нет) | ничего |
| Константы `NUM_PATHNODES`, `NUM_CARPATHLINKS`, `NUM_MAPOBJECTS`, `NUM_PATHCONNECTIONS`, `NUMEVENTS`, `NUM_ACCIDENTS` | `config.h` (0 внешних ссылок) | ничего |
| `m_vehicleClass` / `m_frequency` / `m_nTimesUsed` (`CVehicleModelInfo`) | `VehicleModelInfo.{h,cpp}`, `FileLoader.cpp` (поля `vehclass`/`frequency` читаются только чтобы сохранить позиции в строке `vehicles.ide`), `CustomWorld.cpp` | ничего |
| `bWaterTight` (`CAutomobile`, `CBike`) | поля, инициализация и условия `IsPlayer() \|\| !bWaterTight` в `ProcessBuoyancy`-ветке утопления (писался только в `false`) | ничего |
| `bIsLawEnforcer` + `CVehicle::ChangeLawEnforcerState` | `Vehicle.{h,cpp}` (в т.ч. бит 0 флагов в `Save/Load` — теперь всегда 0), `Automobile.cpp` (вызов `SetWantedLevelNoDrop(1)` при таране полицейской машины — флаг никогда не ставился в `true`), `Bike.cpp`, `Boat.cpp`, `Wanted.cpp` (подсчёт полицейских машин по флагу; подсчёт по модели остался — F), `Pools.cpp`, `Physical.cpp` (`HIGHSPEED_ELASTICITY_MULT_COPCAR`), `World.cpp` (`StopAllLawEnforcersInTheirTracks` — удалена), `Camera.cpp` (`OBBE_COPCAR`, `OBBE_COPCAR_WHEEL` — режимы кинематографической камеры на полицейскую машину, удалены вместе с записями в `SequenceOfCarCams`) | `IsLawEnforcementVehicle()` (по модели) |
| `CPlayerInfo::MakePlayerSafe` | `PlayerInfo.{h,cpp}`, `GameLogic.cpp` (единственный вызов — `MakePlayerSafe(false)` при респавне, отменял сценарный `true`), `CWorld::SetAllCarsCanBeDamaged` (вызывалась только из него) | ничего |
| `cTransmission::CalculateGearForSimpleCar` + `TestSimpleTrafficShift` | `cTransmission.{h,cpp}`, `tests/TransmissionTests.cpp` (метод обслуживал `STATUS_SIMPLE`-машины) | `m_fMaxCruiseVelocity` (читает `PedFight.cpp`), `m_fCurVelocity` |
| Аттракторы в `C2dEffect` (`struct Attractor`, `struct PedAttractor`, `ATTRACTORTYPE_*`) | `2dEffect.h`, `FileLoader.cpp::Load2dEffect` (ветки `EFFECT_ATTRACTOR`/`EFFECT_PED_ATTRACTOR` теперь хранят только позицию и цвет) | значения `EFFECT_ATTRACTOR`/`EFFECT_PED_ATTRACTOR` — позиционные в файле эффектов |
| Мёртвые флаги педа: `bReachedAttractorHeadingTarget`, `bTurnedAroundOnAttractor`, `bHasAlreadyUsedAttractor`, `bHasAlreadyStoleACar`, `bCarPassenger`, `bMiamiViceCop`, `bHasAlreadyBeenRecorded` (+ тот же флаг в `CVehicle`) | `Ped.{h,cpp}`, `Vehicle.{h,cpp}` (только инициализация) | ничего |

### 7.3 Замены и решения (детали — `decisions.md` §3.3, §4.3)

- **Единый спавн (§4.3).** `CGame::SpawnVehicle(model, pos, heading)` в `Game.cpp`: фабрика
  `CCarCtrl::GetNewVehicleDependingOnCarModel(model, MISSION_VEHICLE)` → `SetPosition/SetOrientation`,
  `STATUS_ABANDONED`, `CARLOCK_UNLOCKED`, `CWorld::Add`, `nil` при переполнении пула. Через него идут:
  `re3.cpp::SpawnCar` (дебаг-меню), `Pad.cpp` (`VehicleCheat`, Shift+7/8), `MobileTouchControls.cpp`,
  `AnimViewer.cpp` (просмотрщик моделей; раньше звал фабрику напрямую) и `CRemote::GivePlayerRemoteControlledCar`
  (раньше `new CAutomobile`). Фабрика больше нигде не вызывается.
- **`m_pVehicleEx` → `m_pSpecialColModelVehicle`.** В задании поле описано как «машина по соседству для AI-подбора»;
  в этом дереве `CPlayerInfo::m_pVehicleEx` — указатель на машину, использующую гидравлическую `m_ColModel`
  игрока (`Automobile.cpp`, `ProcessHydraulics`), ставится только там и читается при смене машины игрока.
  Механика игрока — оставлена, имя переименовано по смыслу, чтобы гейт §6.2 был пуст.
- **Упавшие сущности.** `CWorld::RemoveFallenCars`/`RemoveFallenPeds`: пустая (без водителя-игрока) машина или пед ниже
  `MAP_Z_LOW_LIMIT` удаляются (`Remove` + `delete`); машина игрока не трогается. Раньше телепорт на ближайший узел
  пути (`WorldRecovery.h`) — путей нет, см. `decisions.md` §3.3.
- **`VehicleCreatedBy`** (`RANDOM/MISSION/PARKED/PERMANENT`) остаётся как runtime-данные: читается в `World.cpp`,
  `Ped.cpp`, `Automobile.cpp`, `Bike.cpp`, `Boat.cpp` и `Vehicle.cpp`; сериализация, которая его читала/писала,
  удалена на F-3b.
- **Константы винта лодки** `PLANE_MIN/STD/MAX_PROP_SPEED` в `Boat.cpp` переименованы в `STD/MAX_PROP_SPEED`
  (минимальная не использовалась — удалена).
- **RC-режим** (`CRemote`, `m_pRemoteVehicle`, `m_bInRemoteMode`, `BlowUpRCBuggy`) оставлен по §3.10;
  `CRemote::GivePlayerRemoteControlledCar` — единственная точка входа в этот режим, в дереве вызовов у неё нет
  (в VC её звал скрипт). Записано в `decisions.md` §3.3.

### 7.4 Восстановленные определения (дефект, внесённый на D-1)

`linkcheck.sh` с самого D-1 показывал два неопределённых reVC-символа — `CControllerState::CheckForInput()` и
`CStreaming::LoadAllRequestedModels(bool)`; журнал ошибочно принимал их за ложные срабатывания фильтра (проверка шла
по вызовам, а не по определениям). Фактически правки этапа D-1 удалили их вместе с соседними функциями:
`CheckForInput` — вместе с `DoShowChaseStatCheat` (`Pad.cpp`), `LoadAllRequestedModels` — вместе с `#if 0`-блоком
`ONE_THREAD_PER_CHANNEL` (`Streaming.cpp`; удалилась и активная ветка `#else`). Оба восстановлены из `8efb3bc`
(`#if 0`-вариант не восстанавливался). Аудит: из 4 615 определений методов базы 1 061 отсутствует, ни одно из
отсутствующих не осталось объявленным в заголовке — других «съеденных» функций нет.

### 7.5 Поведенческий тест E (§6.3 п.2, п.5) — **pending**, нужен бинарник

1. Запуск: карта грузится без `paths.ipl`/`navig.zon`/`info.zon`/`info.zpl`, игрок в мире, камера на игроке.
2. 10 минут простоя в трёх точках карты: в пулах `CPools::GetVehiclePool()`/`GetPedPool()` нет ничего, кроме
   созданного игрой; в небе нет вертолётов/самолётов (раньше `CPlane::UpdatePlanes` создавал `DEADDODO`/`CHOPPER`
   через 10 с полёта игрока на самолёте).
3. Дебаг-меню → `Spawn` → любая машина: появляется перед игроком, стоит на земле (`z` по `FindGroundZFor3DCoord` +2),
   садится/выходит (`EnterCar`/`ExitCar`, `QuitEnteringCar`), ездит; лодка на воде; `MAVERICK`/`DODO` из списка —
   как автомобиль с физикой винта/крыла (`IsRealHeli()`/`IsRealPlane()`).
4. Читы `VEHICLE`-списка и Shift+7/8 (спавн следующей/предыдущей модели) — та же точка входа.
5. Мобильная панель (`MobileTouchControls`) — спавн кнопкой: машина появляется, для байка ставится `bIsStanding`.
6. Утопление в машине: у `Automobile`/`Bike` экипаж получает `WEAPONTYPE_DROWNING`/сбрасывается с байка независимо от
   `IsPlayer()` (раньше — только игрок или при `!bWaterTight`, что всегда `true`).
7. Кинематографическая камера (режим `CAM_ZOOM_CINEMATIC` в машине): режимы переключаются, камера полицейской
   машины больше не выбирается.

### 7.6 Открытые пункты после E

- `CStreaming::StreamVehiclesAndPeds` (`Streaming.cpp`, вызывается из `Update`) теперь содержит только стриминг
  SWAT/FBI/армии по `CWanted::Are*Required` — уходит на этапе F вместе с `Wanted.cpp`.
- `Wanted.cpp::CountPolice`-подсчёт машин по `IsPoliceVehicleModel` — F.
- `Cam.cpp` `pStoredCopPed`/`ARRESTCAM_*` — камера ареста, F.
- Раскладка сейва: бит 0 флагов машины свободен, битовые поля `CPed`/`CVehicle` были сжаты на E; весь сейв
  удалён на F-3b, вместе с `SAVE_VERSION`.
- `codewarrior/reVC.mcp.xml` (CodeWarrior-проект) всё ещё перечисляет файлы, удалённые на B–E (`Script.cpp`,
  `Population.cpp`, `CarAI.cpp`, `Heli.cpp`…). Это не одна из систем сборки §6.1 (premake/CMake), файлы там
  перечислены списком — почистить на H вместе с `main.cpp`-списком `PEDSTATS.DAT`/`PEDGRP.DAT`.

### 7.7. E-доп: `CarEnterExit` урезан (`vehicles/CarEnterExit.{cpp,h}`)

Модуль — SA-класс `CCarEnterExit`, в этом дереве у 22 из 33 методов не было вызывающих
(в SA их зовёт таск-система, которой здесь нет; поток игрока идёт через `CPed`-стейт-машину).

| Поле | Значение |
|---|---|
| строки `.cpp` до / после | 983 → 399 (−584: 22 метода, статик-хелпер `GetVehicleAnimGroupFor`, 9 статических полей, `Initialise`) |
| методов до / после | 33 → 10 (`Initialise` стал пустым и удалён вместе с вызовом из `Game.cpp:448`) |
| живые точки входа | `Ped.cpp:8217` `ComputeDoorFlag`, `:8222`/`:8233` `IsCarDoorReady`, `:8374` `ComputeDoorFlag`, `:9002` `GetNearestCarDoor`, `:9013` `GetNearestCarPassengerDoor` |
| публичный API в `.h` | `CarHasOpenableDoor`, `IsCarDoorReady`, `ComputeDoorFlag`, `ComputeSlowJackedPed`, `GetNearestCarDoor`, `GetNearestCarPassengerDoor`, `GetPositionToOpenCarDoor` ×2, `IsPathToDoorBlockedByVehicleCollisionModel`, `IsRoomForPedToLeaveCar` |
| удалены (22) | `AddInCarAnim`, `CarHasDoorToClose`, `CarHasDoorToOpen`, `CarHasPartiallyOpenDoor`, `ComputeOppositeDoorFlag`, `ComputePassengerIndexFromCarDoor`, `ComputeTargetDoorToEnterAsPassenger`, `ComputeTargetDoorToExit`, `IsCarDoorInUse`, `IsCarQuickJackPossible`, `IsCarSlowJackRequired`, `IsClearToDriveAway`, `IsPedHealthy`, `IsVehicleHealthy`, `IsVehicleStealable`, `MakeUndraggedDriverPedLeaveCar`, `MakeUndraggedPassengerPedsLeaveCar`, `QuitEnteringCar` (статический; `CPed::QuitEnteringCar` жив), `RemoveCarSitAnim`, `RemoveGetInAnims`, `SetAnimOffsetForEnterOrExitVehicle` (статический; `CPed::SetAnimOffsetForEnterOrExitVehicle` жив), `SetPedInCarDirect` |
| статические поля | `ms_fMaxSpeed_CanDragPedOut`, `ms_fMaxSpeed_PlayerCanDragPedOut`, `ms_bPedOffsetsCalculated`, `ms_vecPedGetUpAnimOffset`, `ms_vecPedBedL/RAnimOffset`, `ms_vecPedDeskAnimOffset`, `ms_vecPedChairAnimOffset`, `ms_vecPedQuickDraggedOutCarAnimOffset` — удалены: писатель значимых offset'ов был мёртвым, speed-константы не читались |
| `GetNearestCarDoor` | указатели `driverDraggedOutOffset`/`psgrDraggedOutOffset` и 9 передач третьего аргумента в `IsRoomForPedToLeaveCar` убраны; ветка `else if(bIsBus \|\| bLowVehicle)`, состоявшая только из обнуления указателей, удалена; `CCarEnterExit::IsRoomForPedToLeaveCar(vehicle, door)` передаёт в `CVehicle::IsRoomForPedToLeaveCar` `nil` — то же поведение, что и раньше (указатель всегда ссылался на нулевой вектор) |
| `#include` | оставлены `common.h`, `CarEnterExit.h`, `Vehicle.h`, `Automobile.h`, `Ped.h`, `ModelInfo.h`, `VehicleModelInfo.h`, `World.h`, `Collision.h`, `Timer.h`; остальные сняты — компилятор их не требует |
| проверки | `check.sh` для `Ped.cpp`, `CarEnterExit.cpp`, `Game.cpp` — OK 3 / FAIL 0; `-Wall` по `CarEnterExit.cpp` — 0 unused (165 оставшихся — статические таблицы `AnimAssocTables.h`, приходящие транзитивно через `VehicleModelInfo.h`, были и до правки); метод §2.6 (повисшие `if`) — 1 (`PlayerPed.cpp:1832`, чинится следующим шагом) |

### 7.8. Регрессы удаления (D-1/D-5) — исправлены

Оба одного класса: при удалении тела метода оставалась управляющая строка. Метод проверки (`.harness/dangling.py`):
по всем `.cpp` — строка `if/for/while/else if … )` без `{`, затем одна или несколько пустых строк, затем
непустая строка с отступом не больше, чем у условия. До правки — ровно 1 хит, после — **0**.

| Место | Было | Стало |
|---|---|---|
| `peds/PlayerPed.cpp:1832` | `if (Wanted()->GetWantedLevel() > 0)` с пустым телом (его тело `FindNewAttackPoints()`/`UpdateMeleeAttackers()` удалено на D-1) | условие удалено целиком |
| `peds/PedFight.cpp:3086` | `if (GetVehicleAppearance() == AUTOMOBILE) { }` — пустой блок: на D-5 вместе с `CCarCtrl::SwitchVehicleToRealPhysics` ушёл и сброс автопилота умирающего водителя | `if (… == AUTOMOBILE && pDriver == this) { AutoPilot.m_nCarMission = MISSION_NONE; m_nCruiseSpeed = 0; m_nTempAction = TEMPACT_HANDBRAKESTRAIGHT; m_nTimeTempAction = now + 2000; }` — без `STATUS_SIMPLE`/`SwitchVehicleToRealPhysics` (их больше нет). На F блок уходит вместе с `CAutoPilot` (§2.4 патча) — тогда зафиксировать поведение машины с мёртвым водителем |

`check.sh` для обоих файлов — OK 2 / FAIL 0.

### 7.9. Этап F, §5 шаг 3 — удаление подсистемы сейвов (3a + 3b)

#### Удалённые `.cpp` — число исходных строк до удаления

| Файл | Строк |
|---|---:|
| `save/MemoryCard.cpp` (3a, PS2) | 3 070 |
| `core/Frontend_PS2.cpp` (3a) | 3 019 |
| `control/AutoPilot.cpp` | 93 |
| `peds/PlayerPedData.cpp` | 152 |
| `save/Date.cpp` | 91 |
| `save/GenericGameStorage.cpp` | 827 |
| `save/PCSave.cpp` | 218 |
| `core/MenuScreens.cpp` (неиспользуемая legacy-таблица без CFO) | 355 |
| **Итого** | **7 825** |

3a также удалил `FrontendTriggers.h`, `Frontend_PS2.h`, `MemoryCard.h`; в семи оставшихся
исходниках было удалено 544 строки PS2-веток. 3b удалил семь оставшихся файлов заголовков
сейв-подсистемы: `Date.h`, `GenericGameStorage.h`, `PCSave.h`, `SaveBlockBounds.h`,
`SaveBuf.h`, `SaveVersion.h`, `SavedPoolValidation.h`.

#### Что удалено и что осталось

- Удалены все `Save`/`Load`-методы мира, зон, радарных блипов, стриминга, пулов, pickup-ов,
  частиц, педов/игрока, транспорта и оружия; удалены `SavePlayerInfo`/`LoadPlayerInfo`,
  `Timer` friend-объявления `GenericSave`/`GenericLoad`, `nSaveStructSize`, save-only
  `PlayerPedData.cpp`, и `SetupDamageAfterLoad` (единственный вызов был в `CAutomobile::Load`).
  `CTheZones`, `CRadar`, `CParticleObject`, `CPools` и `CStreaming` сохранены как подсистемы,
  их игровые методы не менялись.
- Удалены `GenericGameStorage`, `PCSave`, `Date`, весь PS2 MemoryCard-фронтенд и его
  fade/load state; в `Game.cpp` сохранён `InitialiseWhenRestarting`, но без загрузочной
  ветки. `ReInitGameObjectVariables` по-прежнему создаёт игрока и обрабатывает камеру.
- Удалены frontend-страницы выбора/подтверждения save/load/delete, действия для слотов,
  рисование информации о слотах, `ProcessFileActions`, save-zone activation, `Slots[]` и
  `PcSaveHelper`. Страница `STATS` и её пункт меню не тронуты (F, §5 шаг 7). Для mission
  replay сохранён отдельный пустой переход `MENUPAGE_MISSION_REPLAY_TRANSITION`; он заменил
  прежний общий экран «loading» и не содержит save/load-функций. `MENUACTION_YES/NO` оставлены
  только для подтверждения нового старта игры.
- Удалён неиспользуемый `MenuScreens.cpp`: проект настроен на `CUSTOM_FRONTEND_OPTIONS`,
  рабочая таблица — `MenuScreensCustom.cpp`.
- `TopLineEmptyFile` — sentinel файла пользовательских настроек `gta_vc.set`, а не символ
  сейвов. Его определение перемещено в `FileMgr.cpp/.h`; оба settings-валидатора сравнивают
  длину через `strlen`, не включая завершающий NUL.
- Удалены 5 связанных тестов (`AndroidFreeroamSpawnTests`, `PhoneInfoNoMapCrashTests`,
  `SaveBlockBoundsTests`, `SavedPoolValidationTests`, `ScriptSaveValidationTests`) вместе
  с их целями CMake; `premake5.lua` больше не перечисляет `src/save/`.
- `CZONE_SAVE_SIZE`, неиспользуемый бит 0 vehicle save flags, все `COMPATIBLE_SAVES` /
  `SAVE_VERSION` и заголовки wire-format удалены вместе с сериализацией. Позиционные
  игровые enum-ы не уплотнялись на этом шаге.

#### Проверки и гейты после шага 3b

| Проверка | До 3b (после 3a) | После 3b |
|---|---:|---:|
| `bash .harness/check.sh` | 223 / 0 ошибок | **217 / 0 ошибок** |
| `bash .harness/linkcheck.sh` | 0 пропущенных reVC-символов | **0 пропущенных reVC-символов**; 217 объектных файлов, 0 ошибок компиляции |
| `python3 .harness/dangling.py src` | 0 | **0** |
| Ссылки на save subsystem / `SaveBuf` / `COMPATIBLE_SAVES` / `SAVE_VERSION` / `SavePlayerInfo` / `LoadPlayerInfo` в `src/` | были | **0** |
| save/load/delete frontend actions/pages и `SAVESLOT_` | были | **0** (кроме нового старта игры и отдельного mission-replay перехода) |

Поведенческие проверки §7.5 и §7.9 не выполнялись: для них нужен работающий игровой
бинарник. В игре остаётся проверить старт нового мира, pause/options, отсутствие save/load/stats
экрана в меню (stats screen уходит на шаг 7), mission replay если он включён, а также смерть/
респавн/деньги/машину с мёртвым водителем; статус — **pending**, не выдаётся за подтверждённый.
