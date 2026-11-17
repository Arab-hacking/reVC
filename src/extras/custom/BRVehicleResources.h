#pragma once

// Portable metadata only. Native BR pointers/offsets are not a file ABI.
#include <cstdint>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <string>
#include <sstream>
#include <vector>
#include <map>

namespace brvehicle {
enum TextureRole { Other, Palette, Toner, LightsOff, LightsOn, IndicatorLeft,
    IndicatorRight, PlateFront, PlateBack };
inline TextureRole Role(const char *name)
{
    if(!name) return Other;
    if(std::strncmp(name, "remaptoner", 10) == 0) return Toner;
    if(std::strncmp(name, "remap", 5) == 0) return Palette;
    if(std::strcmp(name, "vehiclelights128") == 0) return LightsOff;
    if(std::strcmp(name, "vehiclelightson128") == 0) return LightsOn;
    if(std::strcmp(name, "v_t_left") == 0) return IndicatorLeft;
    if(std::strcmp(name, "v_t_right") == 0) return IndicatorRight;
    if(std::strcmp(name, "plate_remap") == 0) return PlateFront;
    if(std::strcmp(name, "platem_remap") == 0) return PlateBack;
    return Other;
}
struct Upgrade {
    int32_t id, vehicle, slot, model;
    std::string dummy;
    std::vector<std::string> stockParts;
};
inline bool Number(const std::string &s, int32_t &out)
{
    if(s.empty() || s[0] < '0' || s[0] > '9') return false;
    errno = 0;
    char *end;
    long v = std::strtol(s.c_str(), &end, 10);
    if(errno || *end || v < 0 || v > INT32_MAX) return false;
    out = static_cast<int32_t>(v);
    return true;
}
inline bool Name(const std::string &s)
{
    if(s.empty() || s.size() > 63) return false;
    for(char c : s)
        if(!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
             (c >= '0' && c <= '9') || c == '_')) return false;
    return true;
}
// Actual supplied table: optional stock names, multiple stock names, commas,
// UTF-8 BOM and sparse IDs. Do not assume exactly six columns or dense IDs.
inline bool ParseUpgrades(const uint8_t *data, size_t size, std::vector<Upgrade> &out)
{
    out.clear();
    if(!data || !size || size > 1024*1024 || std::memchr(data, 0, size)) return false;
    size_t pos = size >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF ? 3 : 0;
    std::vector<Upgrade> parsed;
    std::map<int32_t, size_t> ids;
    while(pos < size){
        size_t end = pos;
        while(end < size && data[end] != '\n') ++end;
        if(end-pos > 1023) return false;
        std::string line(reinterpret_cast<const char*>(data+pos), end-pos);
        pos = end + (end < size);
        size_t comment = line.find('#');
        if(comment != std::string::npos) line.resize(comment);
        for(char &c : line) if(c == ',') c = ' ';
        std::istringstream stream(line);
        std::vector<std::string> fields;
        std::string field;
        while(stream >> field){
            if(fields.size() == 21) return false;
            fields.push_back(field);
        }
        if(fields.empty()) continue;
        if(fields.size() < 5 || parsed.size() >= 16384) return false;
        Upgrade u = {};
        if(!Number(fields[0],u.id) || !Number(fields[1],u.vehicle) ||
           !Number(fields[2],u.slot) || !Number(fields[4],u.model) ||
           u.slot > 42 || !Name(fields[3])) return false;
        u.dummy = fields[3];
        for(size_t i = 5; i < fields.size(); ++i){
            if(!Name(fields[i])) return false;
            if(fields[i] != "null") u.stockParts.push_back(fields[i]);
        }
        auto old = ids.find(u.id);
        if(old != ids.end()){
            const Upgrade &previous = parsed[old->second];
            if(previous.vehicle != u.vehicle || previous.slot != u.slot ||
               previous.model != u.model || previous.dummy != u.dummy ||
               previous.stockParts != u.stockParts) return false;
            continue; // exact duplicates in the shipped table are harmless
        }
        ids[u.id] = parsed.size();
        parsed.push_back(u);
    }
    if(parsed.empty()) return false;
    out.swap(parsed);
    return true;
}
} // namespace brvehicle
