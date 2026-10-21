#ifndef REVC_COMMON_CARCOLS_H
#define REVC_COMMON_CARCOLS_H

// The unpacked common/data/carcols.dat contains an SA-style 'col' palette,
// but no VC-compatible 'car' definitions. Only validated RGB triples are
// copied into the existing 256-entry VC colour table; handling and spawning
// data are never read from here.
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace commoncarcols {

struct RGB { uint8_t r, g, b; };

inline bool Parse(const uint8_t *bytes, size_t length, std::vector<RGB> &out)
{
    out.clear();
    if(!bytes || length == 0 || length > 64 * 1024) return false;
    bool inColors = false, found = false;
    for(size_t pos = 0; pos < length;) {
        size_t end = pos;
        while(end < length && bytes[end] != '\n') end++;
        if(end-pos > 511 || std::memchr(bytes+pos, 0, end-pos)) return false;
        std::string line((const char *)bytes+pos, end-pos);
        pos = end + (end < length);
        size_t comment = line.find('#');
        if(comment != std::string::npos) line.resize(comment);
        size_t first = line.find_first_not_of(" \t\r");
        if(first == std::string::npos) continue;
        line.erase(0, first);
        line.erase(line.find_last_not_of(" \t\r")+1);
        if(line == "col") { inColors = true; found = true; continue; }
        if(line == "end") { if(inColors) break; continue; }
        if(!inColors) continue;
        if(out.size() >= 256) return false;
        const char *p = line.c_str();
        RGB rgb = {};
        uint8_t *channels[3] = { &rgb.r, &rgb.g, &rgb.b };
        for(int i = 0; i < 3; i++) {
            char *next = nullptr;
            long value = std::strtol(p, &next, 10);
            if(next == p || value < 0 || value > 255) return false;
            *channels[i] = (uint8_t)value;
            p = next;
            while(*p == ' ' || *p == '\t' || *p == '\r') p++;
            if(i < 2) { if(*p != ',') return false; p++; }
            else if(*p) return false;
        }
        out.push_back(rgb);
    }
    if(!found || out.empty()) { out.clear(); return false; }
    return true;
}

} // namespace commoncarcols
#endif
