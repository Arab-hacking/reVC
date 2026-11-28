#include "CarModsParse.h"
#include <cctype>
#include <sstream>

namespace brcarmods {

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

CarModsData CarModsParser::Parse(const char* data, size_t size, std::string* err) {
    CarModsData result;
    if (!data || size == 0) return result;

    std::string_view view(data, size);
    size_t pos = 0;
    std::string section;

    while (pos < view.size()) {
        size_t nl = view.find('\n', pos);
        if (nl == std::string_view::npos) nl = view.size();
        std::string line(view.substr(pos, nl - pos));
        pos = nl + 1;

        trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;

        // Check for section headers
        if (line == "link" || line == "mods" || line == "end") {
            section = line;
            continue;
        }

        if (section == "link") {
            auto parts = split(line, ',');
            if (parts.size() >= 2) {
                result.links.push_back({ parts[0], parts[1] });
            }
        } else if (section == "mods") {
            auto parts = split(line, ',');
            if (parts.size() >= 4) {
                ModType type = ModType::MOD_UNKNOWN;
                try { type = static_cast<ModType>(std::stoi(parts[2])); } catch (...) {}
                int modelId = 0;
                try { modelId = std::stoi(parts[3]); } catch (...) {}
                ModEntry entry = { parts[0], parts[1], type, modelId };
                result.mods.push_back(entry);
                result.modsByModel[parts[0]].push_back(entry);
            }
        }
    }
    return result;
}

bool CarModsParser::ParseLine(const char* line, CarModsData& data, std::string* err) {
    if (!line) return false;
    std::string s(line);
    trim(s);
    if (s.empty() || s[0] == '#' || s[0] == ';') return false;

    // This is simplified - real parser would track section state
    return false;
}

int CarModsParser::ModTypeToUpgradeSlot(ModType type) {
    // Map BR mod types to SA upgrade slots (0-15)
    switch (type) {
        case MOD_BUMPER_F: return 0;
        case MOD_BUMPER_F2: return 1;
        case MOD_BUMPER_R: return 2;
        case MOD_BUMPER_R2: return 3;
        case MOD_BONNET: return 4;
        case MOD_BOOT: return 6;
        case MOD_CHASSIS: return 7;
        case MOD_EXHAUST: return 9;
        case MOD_BODY: return 10;
        case MOD_GRILLE: return 14;
        case MOD_ROOF: return 15;
        default: return -1;
    }
}

const std::vector<ModEntry>* CarModsParser::GetModsForModel(const CarModsData& data, const char* modelName) {
    auto it = data.modsByModel.find(modelName);
    return it != data.modsByModel.end() ? &it->second : nullptr;
}

} // namespace brcarmods