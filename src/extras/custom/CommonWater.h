#ifndef REVC_COMMON_WATER_H
#define REVC_COMMON_WATER_H

// Black Russia's data/water.dat uses GTA:SA's "processed" water quads, NOT
// Vice City's WATER.DAT rectangles or binary WATERPRO.DAT. All four vertices
// carry x/y/z, flow x/y and two wave parameters, followed by a flag. Support
// the axis-aligned, planar quads with no SA-specific flow/waves that can be
// represented by reVC's water renderer. Never pass this text to VC's loader.
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace commonwater {

struct Quad {
    float left, right, bottom, top;
    float southwest, northwest, southeast, northeast;
    float minZ, maxZ;

    bool Contains(float x, float y) const {
        return x >= left && x <= right && y >= bottom && y <= top;
    }
    float Height(float x, float y) const {
        // The four corners were checked to be coplanar. This agrees with
        // both triangles used for drawing, including sloping river sections.
        return southwest + (x-left) * (southeast-southwest) / (right-left)
                         + (y-bottom) * (northwest-southwest) / (top-bottom);
    }
    float SlopeX() const { return (southeast-southwest) / (right-left); }
    float SlopeY() const { return (northwest-southwest) / (top-bottom); }
};

struct Stats {
    size_t accepted, invalid, unsupported;
    Stats() : accepted(0), invalid(0), unsupported(0) {}
};

inline bool Number(const char *&p, float &out)
{
    while(*p == ' ' || *p == '\t' || *p == '\r') p++;
    char *end = nullptr;
    errno = 0;
    out = std::strtof(p, &end);
    if(end == p || errno == ERANGE || !std::isfinite(out) ||
       (*end && *end != ' ' && *end != '\t' && *end != '\r')) return false;
    p = end;
    return true;
}

inline bool Parse(const uint8_t *bytes, size_t length, std::vector<Quad> &out, Stats &stats)
{
    out.clear();
    stats = Stats();
    if(!bytes || !length || length > 1024 * 1024) return false;
    bool processed = false;
    size_t lines = 0;
    for(size_t pos = 0; pos < length;) {
        size_t end = pos;
        while(end < length && bytes[end] != '\n') end++;
        if(++lines > 8192) { out.clear(); return false; }
        size_t size = end-pos;
        if(size > 511 || std::memchr(bytes+pos, 0, size)) {
            stats.invalid++; pos = end + (end < length); continue;
        }
        std::string line((const char *)bytes+pos, size);
        pos = end + (end < length);
        size_t first = line.find_first_not_of(" \t\r");
        if(first == std::string::npos || line[first] == '#' || line[first] == ';') continue;
        if(!processed) {
            if(line.compare(first, 9, "processed") != 0 ||
               line.find_first_not_of(" \t\r", first+9) != std::string::npos)
                return false; // not the SA/BR water format
            processed = true;
            continue;
        }
        size_t comment = line.find_first_of("#;");
        if(comment != std::string::npos) line.resize(comment);
        const char *p = line.c_str();
        float values[28];
        bool valid = true;
        for(float &value : values)
            if(!Number(p, value)) { valid = false; break; }
        if(!valid) { stats.invalid++; continue; }
        while(*p == ' ' || *p == '\t' || *p == '\r') p++;
        // GTA:SA permits an omitted flag; the supplied archive explicitly
        // uses 1 on every row. Ignore other flags rather than mis-rendering.
        long flag = 1;
        if(*p) {
            char *flagEnd = nullptr;
            errno = 0;
            flag = std::strtol(p, &flagEnd, 10);
            if(flagEnd == p || errno == ERANGE ||
               (*flagEnd && flagEnd[0] != ' ' && flagEnd[0] != '\t' && flagEnd[0] != '\r') ||
               flag < 0 || flag > 255) { stats.invalid++; continue; }
            while(*flagEnd == ' ' || *flagEnd == '\t' || *flagEnd == '\r') flagEnd++;
            if(*flagEnd) { stats.invalid++; continue; }
        }
        if(flag != 1) { stats.unsupported++; continue; }
        bool plain = true;
        for(int i = 0; i < 4; i++) {
            if(std::fabs(values[i*7]) > 100000.0f ||
               std::fabs(values[i*7+1]) > 100000.0f ||
               std::fabs(values[i*7+2]) > 10000.0f) { valid = false; break; }
            for(int k = 3; k < 7; k++)
                if(values[i*7+k] != 0.0f) plain = false;
        }
        if(!valid) { stats.invalid++; continue; }
        if(!plain) { stats.unsupported++; continue; }
        // v0/v1 are the east edge, v2/v3 the west edge; y increases north.
        if(values[0] != values[7] || values[14] != values[21] ||
           values[1] != values[15] || values[8] != values[22] ||
           values[0] <= values[14] || values[8] <= values[1] ||
           std::fabs((values[14+2] + values[7+2]) -
                     (values[21+2] + values[2])) > 0.01f) {
            stats.unsupported++; continue;
        }
        Quad q;
        q.left = values[14]; q.right = values[0];
        q.bottom = values[1]; q.top = values[8];
        q.southwest = values[16]; q.northwest = values[23];
        q.southeast = values[2]; q.northeast = values[9];
        q.minZ = std::min(std::min(q.southwest, q.northwest),
                          std::min(q.southeast, q.northeast));
        q.maxZ = std::max(std::max(q.southwest, q.northwest),
                          std::max(q.southeast, q.northeast));
        if(out.size() >= 4096) { out.clear(); return false; }
        out.push_back(q);
        stats.accepted++;
    }
    return processed && !out.empty();
}

inline bool Sample(const std::vector<Quad> &quads, float x, float y, float &z,
                   float *slopeX = nullptr, float *slopeY = nullptr)
{
    if(!std::isfinite(x) || !std::isfinite(y)) return false;
    // Later river/pond quads override the underlying ocean at the few places
    // where they overlap in the supplied map.
    for(auto it = quads.rbegin(); it != quads.rend(); ++it)
        if(it->Contains(x, y)) {
            z = it->Height(x, y);
            if(slopeX) *slopeX = it->SlopeX();
            if(slopeY) *slopeY = it->SlopeY();
            return true;
        }
    return false;
}

inline float DistanceToWater(const std::vector<Quad> &quads, float x, float y)
{
    float distance = 250.0f;
    if(!std::isfinite(x) || !std::isfinite(y)) return distance;
    for(const Quad &q : quads) {
        float dx = std::max(std::max(q.left-x, x-q.right), 0.0f);
        float dy = std::max(std::max(q.bottom-y, y-q.top), 0.0f);
        distance = std::min(distance, std::sqrt(dx*dx + dy*dy));
    }
    return std::max(0.0f, distance - 23.0f);
}

} // namespace commonwater
#endif
