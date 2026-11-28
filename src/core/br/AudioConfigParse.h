#pragma once

// Black Russia audio config parsers — vehicleAudioSettings.cfg + .bpcmeta

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

namespace braudio {

// vehicleAudioSettings.cfg entry (14 columns)
struct VehicleAudioSettings {
    std::string modelName;    // e.g., "landstal"
    int engineSample;         // sample ID for engine
    int hornSample;           // sample ID for horn
    float pitch;              // pitch multiplier
    int unk1;                 // usually 0
    float volume;             // volume multiplier
    int unk2;                 // usually 15
    float unk3;               // e.g., 1.189...
    int audioType;            // 1/2/3/4
    int vehicleClass;         // 1-13
    int flags;                // bitfield
    int maxDistance;          // 60
    float doppler;            // 0.0
};

struct BpcMetaEntry {
    std::string bankName;     // e.g., "Bank_001"
    std::string soundName;    // e.g., "sound_001.mp3"
    uint32_t offset;
    uint32_t size;
};

struct AudioConfigData {
    std::vector<VehicleAudioSettings> vehicleSettings;
    std::unordered_map<std::string, VehicleAudioSettings> byModel;
    std::unordered_map<std::string, std::vector<BpcMetaEntry>> bankIndex;  // bankName -> entries
};

class AudioConfigParser {
public:
    // Parse vehicleAudioSettings.cfg (text, tab-separated)
    static AudioConfigData ParseVehicleAudio(const char* data, size_t size, std::string* err = nullptr);

    // Parse .bpcmeta (binary TLV format)
    // Format: [u32 count] then for each: [u32 nameLen][name][u32 offset][u32 size]
    // Or: sequence of entries with name strings
    static AudioConfigData ParseBpcMeta(const uint8_t* data, size_t size, std::string* err = nullptr);

    // Get vehicle audio settings by model name
    static const VehicleAudioSettings* GetVehicleAudio(const AudioConfigData& data, const char* modelName);

    // Get sound bank entries
    static const std::vector<BpcMetaEntry>* GetBankEntries(const AudioConfigData& data, const char* bankName);
};

} // namespace braudio