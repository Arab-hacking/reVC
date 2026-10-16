#ifndef REVC_CUSTOM_IMG_FORMAT_H
#define REVC_CUSTOM_IMG_FORMAT_H

// VC IMG v1: raw 2048-byte sectors in the .img, 32-byte records in a
// sibling .dir. The stream id occupies the high byte of a directory offset.
// Validate a user's DIR before handing it to the native (unbounded) reader.
// No engine dependencies; the synthetic tests exercise corrupt offsets/sizes.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace customimg {

struct Stats {
    size_t dff, txd, ignored;
    uint32_t largestSectors;
    Stats() : dff(0), txd(0), ignored(0), largestSectors(0) {}
};

inline uint32_t U32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

inline bool Extension(const uint8_t *p, size_t n, const char *ext) {
    if(n != 3) return false;
    for(size_t i = 0; i < 3; i++) {
        uint8_t c = p[i];
        if(c >= 'a' && c <= 'z') c -= 'a' - 'A';
        if(c != (uint8_t)ext[i]) return false;
    }
    return true;
}

// Reserve native TXD names before processing auto-mounted images, without
// changing the later reverse-DIR streaming precedence. No IMAGE data is read.
// A stock DIR may contain other file types; only names the TXD pool can hold
// (19 characters plus NUL) are eligible. Incomplete records are ignored.
template <class Visit>
inline void ForEachNativeTxd(const uint8_t *dir, size_t length, Visit visit) {
    if(!dir) return;
    for(size_t pos = 0; length-pos >= 32; pos += 32) {
        const uint8_t *name = dir+pos+8;
        const uint8_t *dot = (const uint8_t *)std::memchr(name, '.', 24);
        if(!dot || dot == name || dot-name > 19 || dot[4] != 0 ||
           std::memchr(name, 0, dot-name) || !Extension(dot+1, 3, "TXD"))
            continue;
        visit(std::string((const char *)name, dot-name));
    }
}

inline bool Validate(const uint8_t *dir, size_t length, uint64_t imageBytes,
                     Stats &stats, std::string &error) {
    stats = Stats();
    error.clear();
    if(!dir || !length || length % 32 || length > 16u*1024u*1024u) {
        error = "missing, truncated or oversized VC .dir"; return false;
    }
    if(imageBytes < 2048) { error = "empty IMG"; return false; }
    for(size_t pos = 0; pos < length; pos += 32) {
        const uint8_t *record = dir + pos;
        // The native reader terminates name[23] and accepts names of up to
        // 20 bytes before the dot. Don't register entries it cannot name.
        const uint8_t *dot = (const uint8_t *)std::memchr(record+8, '.', 24);
        if(!dot || dot-record-8 > 19 || dot[4] != 0) {
            stats.ignored++; continue;
        }
        if(!Extension(dot+1, 3, "DFF") && !Extension(dot+1, 3, "TXD")) {
            stats.ignored++; continue; // collision and animation are not in these IMGs
        }
        uint32_t offset = U32(record), sectors = U32(record+4);
        if(offset >= (1u<<24) || !sectors || sectors > 32768 ||
           (uint64_t)offset*2048 + (uint64_t)sectors*2048 > imageBytes) {
            error = "DIR sector offset/length outside IMG or streaming limit";
            return false;
        }
        if(sectors > stats.largestSectors) stats.largestSectors = sectors;
        if(Extension(dot+1, 3, "DFF")) stats.dff++;
        else stats.txd++;
    }
    if(!stats.dff && !stats.txd) {
        error = "no DFF/TXD records in VC .dir"; return false;
    }
    return true;
}

} // namespace customimg
#endif
