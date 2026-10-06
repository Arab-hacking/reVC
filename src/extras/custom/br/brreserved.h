// brreserved.h — имена моделей, которые в GTA SA заняты движком/скриптами: peds.ide (0..311), default.ide (оружие,
// cutscene-объекты), vehicles.ide (400..611), veh_mods.ide (1000..1193), деревья 615..661, special/cutscene-педы, player/csplay.
// BR-архивы br_skins_01/br_cars_01/br_common/br_map_0x содержат .mod с ТАКИМИ ЖЕ именами; modloader подхватывает любой X.dff,
// чьё имя совпадает с зарегистрированной моделью, и подменяет им родную. Для игрока это player.mod — 8-КБ заглушка:
// RpClumpStreamRead её отвергает, CPed::SetModelIndex получает NULL clump -> EIP 0x749B7B. Поэтому такие .mod не публикуем.
// Сгенерировано из eModelID (fastman92, unmodified SA data) + имена, реально импортированные modloader в логе пользователя.
#pragma once
#include <string>
#include <cstring>
#include <algorithm>
namespace brres {
// v6.5: по просьбе пользователя блокируется ТОЛЬКО player (player.mod из br_common.zip — 8-КБ заглушка, роняет игру по 0x749B7B).
// Скины педов и машины BR теперь публикуются и подменяют модели SA.
static const char* const kReserved[] = {
    "player",
};
static const size_t kReservedCount = sizeof(kReserved) / sizeof(kReserved[0]);
// name — в нижнем регистре, без расширения (список отсортирован по strcmp)
static inline bool isReserved(const std::string& name) {
    const char* const* e = kReserved + kReservedCount;
    const char* const* it = std::lower_bound(kReserved, e, name.c_str(), [](const char* a, const char* b) { return strcmp(a, b) < 0; });
    return it != e && name == *it;
}
} // namespace brres
