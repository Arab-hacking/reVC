#include "AudioConfigParse.h"
#include <cctype>
#include <sstream>

namespace braudio {

static inline void trim(std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace((unsigned char)s[start])) start++;
    size_t end = s.size();
    while (end > start && std::isspace((unsigned char)s[end-1])) end--;
    s = s.substr(start, end - start);
}

static inline std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        trim(item);
        if (!item.empty()) out.push_back(item);
    }
    return out;
}

AudioConfigData AudioConfigParser::ParseVehicleAudio(const char* data, size_t size, std::string* err) {
    AudioConfigData result;
    if (!data || size == 0) return result;

    std::string_view view(data, size);
    size_t pos = 0;

    while (pos < view.size()) {
        size_t nl = view.find('\n', pos);
        if (nl == std::string_view::npos) nl = view.size();
        std::string line(view.substr(pos, nl - pos));
        pos = nl + 1;

        trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;

        auto parts = split(line, '\t');
        if (parts.size() < 14) continue;

        VehicleAudioSettings vas;
        vas.modelName = parts[0];
        vas.engineSample = std::stoi(parts[1]);
        vas.hornSample = std::stoi(parts[2]);
        vas.pitch = std::stof(parts[3]);
        vas.unk1 = std::stoi(parts[4]);
        vas.volume = std::stof(parts[5]);
        vas.unk2 = std::stoi(parts[6]);
        vas.unk3 = std::stof(parts[7]);
        vas.audioType = std::stoi(parts[8]);
        vas.vehicleClass = std::stoi(parts[9]);
        vas.flags = std::stoi(parts[10]);
        vas.maxDistance = std::stoi(parts[11]);
        vas.doppler = std::stof(parts[12]);

        result.vehicleSettings.push_back(vas);
        result.byModel[vas.modelName] = vas;
    }
    return result;
}

AudioConfigData AudioConfigParser::ParseBpcMeta(const uint8_t* data, size_t size, std::string* err) {
    AudioConfigData result;
    if (!data || size < 4) { if (err) *err = "bpcmeta too small"; return result; }

    // bpcmeta format: entries with name, offset, size
    // BR format: sequence of records with null-terminated strings
    size_t pos = 0;
    while (pos + 4 <= size) {
        // Try to read a string (bank/sound name)
        size_t strStart = pos;
        while (pos < size && data[pos] != 0) pos++;
        if (pos >= size) break;
        std::string name((const char*)data + strStart, pos - strStart);
        pos++; // skip null

        if (pos + 8 > size) break;
        uint32_t offset = (uint32_t)data[pos] | ((uint32_t)data[pos+1] << 8) | ((uint32_t)data[pos+2] << 16) | ((uint32_t)data[pos+3] << 24);
        uint32_t sz = (uint32_t)data[pos+4] | ((uint32_t)data[pos+5] << 8) | ((uint32_t)data[pos+6] << 16) | ((uint32_t)data[pos+7] << 24);
        pos += 8;

        // Parse bank name from entry name (e.g., "Bank_001/sound_001.mp3")
        size_t slash = name.find('/');
        std::string bankName = (slash != std::string::npos) ? name.substr(0, slash) : name;

        BpcMetaEntry entry = { bankName, name, offset, sz };
        result.bankIndex[bankName].push_back(entry);
    }
    return result;
}

const VehicleAudioSettings* AudioConfigParser::GetVehicleAudio(const AudioConfigData& data, const char* modelName) {
    auto it = data.byModel.find(modelName);
    return it != data.byModel.end() ? &it->second : nullptr;
}

const std::vector<BpcMetaEntry>* AudioConfigParser::GetBankEntries(const AudioConfigData& data, const char* bankName) {
    auto it = data.bankIndex.find(bankName);
    return it != data.bankIndex.end() ? &it->second : nullptr;
}

} // namespace braudio