#pragma once

// Black Russia carmods.dat parser — NEW format (modelName modName modType modelId)

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

namespace brcarmods {

// Mod types from BR carmods.dat
enum ModType : uint8_t {
    MOD_BUMPER_F      = 0,
    MOD_BUMPER_F2     = 1,
    MOD_BUMPER_R      = 2,
    MOD_BUMPER_R2     = 3,
    MOD_BONNET        = 4,
    MOD_BOOT          = 6,
    MOD_CHASSIS       = 7,  // also 8
    MOD_EXHAUST       = 9,  // also 10
    MOD_BODY          = 11, // also 12, 13 (skirts)
    MOD_GRILLE        = 14,
    MOD_ROOF          = 15,
    MOD_UNKNOWN       = 255
};

struct ModLink {
    std::string left, right;  // e.g., "bntl_b_ov" / "bntr_b_ov"
};

struct ModEntry {
    std::string modelName;    // e.g., "bravura"
    std::string modName;      // e.g., "nto_b_l"
    ModType modType;
    int modelId;              // model ID for the mod
};

struct CarModsData {
    std::vector<ModLink> links;
    std::vector<ModEntry> mods;
    std::unordered_map<std::string, std::vector<ModEntry>> modsByModel;
};

class CarModsParser {
public:
    static CarModsData Parse(const char* data, size_t size, std::string* err = nullptr);
    static bool ParseLine(const char* line, CarModsData& data, std::string* err = nullptr);

    // Convert BR mod type to SA upgrade slot
    static int ModTypeToUpgradeSlot(ModType type);

    // Get all mods for a vehicle model
    static const std::vector<ModEntry>* GetModsForModel(const CarModsData& data, const char* modelName);
};

} // namespace brcarmods