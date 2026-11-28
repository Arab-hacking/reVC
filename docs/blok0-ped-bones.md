# Блок 0 — фундамент педов: кости, узлы, индексация анимаций, модели

Ветка `arena/782fc865-revc`, база `ec5b674e`, предыдущий tip `06da767`.
Коммит: `b771979`. Изменено 44 файла, +2053 / −959, добавлено 4 новых.
Эталон: `gta-reversed` (`/tmp/gtr`), адреса статиков — plugin-sdk.
Техзадание — `docs/blok6-ped-audit.md`, §8 «Блок 0».

Полный прогон: **FAIL=0** в трёх конфигурациях (по умолчанию,
`-DFIX_BUGS -DSA_VEHICLE_MODELS`, `-DCUSTOM_MODELS`), плюс `-c -O1` для всех
44 затронутых `.cpp`.

---

## 1. Зачем этот блок первый

Ассеты в проекте уже SA, а индексация под них была гибридной: `eBoneTag`
VC-шный, `AssocGroupId` — VC-таблица с приклеенными SA-группами, `ePedType`
и `ePedStats` — VC-нумерация поверх SA-файлов. Пока это так, любой следующий
блок (движение, урон по зонам, оружие) опирается на числа, которые означают
не то, что написано. Поэтому сначала — enum'ы, таблицы и загрузчик, и только
потом логика.

## 2. Кости и узлы

`src/peds/PedNode.h` (новый) — `ePedNode` на 19 значений (SA), `ePedPieceTypes`
в SA-порядке (LEG 0 … PIECE_HEAD 9) и `tPedColNodeInfo` (`0x1C`). Вынесен в
отдельный заголовок, потому что `Bones.h` и `PedModelInfo.h` тянут друг друга
через `PedType.h`; отдельный файл разрывает цикл.

`src/animation/Bones.h/.cpp`:

* `eBoneTag` целиком по SA: `BONE_UNKNOWN = -1`, `BONE_ROOT 0 … BONE_JAW 8`,
  клавикулы/плечи/кисти `21…26` и `31…36`, ноги `41…44` и `51…54`,
  `BONE_BELLY 201`, `BONE_R/L_BREAST 301/302`, `BONE_MAX_ID 303`,
  `MAX_BONE_NUM 32`;
* `ConvertBoneTag2BoneName` (0x4D56F0) — имена, которые несут SA-скелеты
  (`"Pelvis"`, `"Spine1"`, `"L Breast"`, `"Belly"`), плюс носки `L/R Toe0`;
* `ConvertPedNode2BoneTag` (0x4D58A0) — по SA-узлам, `BONE_UNKNOWN` только для
  нулевого узла и выхода за диапазон.

`src/peds/PedStdBonePositions.h` (новый) — `GetPedBoneStdPosition`, 303 записи
из `0x8D13A8` с проверкой границ. Нужен потому, что `CPed::GetBonePosition`
(0x5E4280) обязан ответить до того, как иерархия выставлена в этом кадре —
иначе первый же запрос кости до `PreRender` вернул бы прошлый кадр.

`CPed`: `bCalledPreRender` заведён (в reVC его не было), ставится в
`PreRenderAfterTest`/`PreRender` и сбрасывается в конструкторе; добавлены
`GetBonePosition`, `GetBoneMatrix`, `GetTransformedBonePosition` с
SA-развилкой «иерархия ещё не обновлена → позиция из таблицы через матрицу
сущности».

## 3. Индексация анимаций

`enum AssocGroupId` (`AnimManager.h`) переписан строго по SA — 139 значений,
`CARS_BEGIN = 88`, `STDCARAMIMS = 88`, `LOWCARAMIMS = 89`, `CARS_END = 118`,
`MAN = 118`, `SHUFFLE = 119` … `SKATE = 138`, `NUM_ANIM_ASSOC_GROUPS = 139`.
Имена `STDCARANIMS`/`LOWCARANIMS`/`UNARMED`/`PANICCHUNKY`/`PLAYER3`/`PLAYER4`
удалены — в SA таких нет.

`AnimAssocTables.h`: сверка каждой группы с `AnimAssocAnimations.h` эталона.
Закрыты 7 расхождений (`ATTRACTORS` → `StepSit_In/Out/Loop`, `MELEE_2/3/4` →
`FightB/C/D_*`, `KICK_STD` → `FIGHTkick`, `FAT`/`MUSCULAR` → `fat*`/`muscle*`),
19 VC-таблиц заменены на SA-стили (`MAN`, `SHUFFLE`, `OLDMAN`, `GANG1/2`,
`OLDFATMAN`, `FATMAN`, `JOGGER`, `DRUNKMAN`, `BLINDMAN`, `SWAT`, `WOMAN`,
`SHOPPING`, `BUSYWOMAN`, `SEXYWOMAN`, `PRO`, `OLDWOMAN`, `FATWOMAN`,
`JOGWOMAN`, `OLDFATWOMAN`, `SKATE`), `aPANICCHUNKY*` удалена.

`AnimManager.cpp`: `ms_aAnimAssocDefinitions` обрезан до 118 записей, которые
SA компилирует (остальные id 118–138 заводит `data/animgrp.dat`), имена блоков
приведены к SA (`melee_2` → `fight_b`, `attractors` → `attractors`,
`fatman` → `fat`). `aAnimGroupNames[139]` переписан в порядке id 0–138;
`GetAnimGroupName` отдаёт имя из определения, а для ещё не определённой группы
— из этой таблицы, поэтому поиск по имени работает до чтения `animgrp.dat`.

## 4. Типы, статистика, модели

`ePedType` — SA (32): `NONE` нет, но есть `PLAYER_NETWORK`, `PLAYER_UNUSED`,
`GANG10`, `DEALER`, `MEDIC`, `BUM`, `MISSION1..8`. `PED_FLAG_*` = `1 << id`.

`ePedStats` — 43 + `PEDSTAT_NONE = -1`; `CPedStat` (`0x34`) с SA-полями,
включая `m_nDefaultDecisionMaker`; парсер `pedstats.dat` читает 11 колонок.
`GetPedStatType` (0x6088D0) на неизвестное имя отвечает `PEDSTAT_NONE`, а не
`SENSIBLE_GUY`, как в SA: молча выданная личность прячет опечатку в `peds.ide`,
и все вызывающие всё равно подставляют свой fallback.

`ePedRace` + `CPopulation::FindPedRaceFromName` (0x5B6D40).

`CPedModelInfo` — SA-раскладка: `m_animGroup` (`AssocGroupId`),
`m_pedType`, `m_pedStatType`, `m_carsCanDrive` (`uint16`), `m_nPedFlags`
(`uint16`), `m_hitColModel`, `radio1/2`, `m_nRace`, `m_nPedAudioType`,
`m_nVoiceMin/Max/Id`; `m_pPedIds` — 13 записей `0x84D300`; hit-col — 12 сфер
из `0x8A6308` (которая кость, какая часть тела, центр, радиус), так что
`m_nPiece` сферы и есть `ePedPieceTypes`.

`src/audio/PedVoice.{h,cpp}` (новый) — `eAudioPedType` (6 + `UNK`),
`CPedVoice::GetAudioPedType` (0x4E3C60), `GetVoice` (0x4E3CD0) и пять таблиц
голосов SA (GEN 210, EMG 47, PLY 21, GNG 53, GFD 19).

## 5. Загрузчики

`CFileLoader::LoadPedObject` (0x5B7420) читает 14 полей SA
(`%d %s %s %s %s %s %x %x %s %d %d %s %s %s`), маски — hex, статы и группа
ищутся по имени, голос резолвится через `CPedVoice`, гонка — через
`FindPedRaceFromName`. Отдельного `groupAliases` больше нет: после §3 имена
`MAN`/`VAN`/`COACH` штатные.

`CustomWorld.cpp`: `SafePedAnimGroup` стал тем же поиском по имени, что и в
штатном загрузчике; из алиасов остались только написания, которых в таблице
нет (`skater`, `jogman`, `unarmed` последним — VC-наследство архивов).
Строка модели получает ещё `m_nRace`, чтобы голос архивного педа не остался
`RACE_DEFAULT`.

## 6. Население

`CPopulation::UpdatePedCount` (0x611570) переписан по SA: счётчики по типу,
`CRIMINAL` считается как `CIVMALE`, `PROSTITUTE` — как `CIVFEMALE`, десять
банд. `ms_nNumDummy` (VC-заглушка без смысла) заменён на `ms_nNumGang10`,
`CReplay` хранит его под новым именем. Три агрегата
(`ms_nTotalCivPeds`/`ms_nTotalGangPeds`/`ms_nTotalPeds`) больше не считаются
вручную в двух местах, а приходят из `UpdatePedCounts()`, как в SA.

## 7. Что осталось за блоком

* `ePedState` (65 значений) — это уже Блок 1, вместе с bump'ом версии сейва:
  сырой `memcpy` в `Pools.cpp` запрещает менять `sizeof(CPed)` молча.
* Таблицы `CGangs` остались VC-шными (9 банд), хотя `ePedType` знает
  `GANG10`; счётчики его считают, квоты и модели для него появятся вместе с
  переиндексацией `pedgrp.dat`-потребителей.
* `eRadioID` не заводился: радиостанции в форке пока VC-нумерованные, а
  `radio + 1` из SA-формата применялся бы к чужой нумерации. Значение из
  `peds.ide` сохраняется как есть, сдвиг сделается вместе с радио-слоем.
* `CPlayerPedData`, плавание, присед, паркур, урон по зонам — Блок 1.
