#ifndef REVC_CUSTOM_IMG_FORMAT_H
#define REVC_CUSTOM_IMG_FORMAT_H

// VC IMG v1: raw 2048-byte sectors in the .img, 32-byte records in a
// sibling .dir. The stream id occupies the high byte of a directory offset.
// Validate a user's DIR before handing it to the native (unbounded) reader.
// No engine dependencies; the synthetic tests exercise corrupt offsets/sizes.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>

namespace customimg {

struct Stats {
    size_t dff, txd, col, cls, ignored, badCollision;
    uint32_t largestSectors;
    Stats() : dff(0), txd(0), col(0), cls(0), ignored(0),
              badCollision(0), largestSectors(0) {}
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

// Index names the native DIR scanner can actually register. Both extensions
// must use the same stem (at most 19 characters, the TXD slot name limit).
// A zero-length entry has no streamable payload, and a truncated/embedded-NUL
// name must not accidentally pair with a different model's texture.
template <class Visit>
inline void ForEachModelOrTxd(const uint8_t *dir, size_t length, Visit visit) {
    if(!dir) return;
    for(size_t pos = 0; length-pos >= 32; pos += 32) {
        const uint8_t *record = dir+pos;
        const uint8_t *name = record+8;
        const uint8_t *dot = (const uint8_t *)std::memchr(name, '.', 24);
        if(!dot || dot == name || dot-name > 19 || dot[4] != 0 ||
           std::memchr(name, 0, dot-name) || U32(record+4) == 0)
            continue;
        bool dff = Extension(dot+1, 3, "DFF");
        bool txd = Extension(dot+1, 3, "TXD");
        // IMG offsets occupy only the low 24 bits of a streaming position.
        // The mounted user DIR is validated, but stock DIRs also use this
        // helper and must not provide an out-of-range fallback position.
        uint32_t offset = U32(record), sectors = U32(record+4);
        if((dff || txd) && offset < (1u<<24) && sectors <= 32768)
            visit(std::string((const char *)name, dot-name), txd, offset, sectors);
    }
}

// The native directory reader walks IMG indices from highest to lowest and
// keeps the first occurrence of each filename. Follow that precedence
// independently for DFF and TXD: matching stems may be in different IMGs,
// but the TXD with the highest-priority registered name must win.
class PairIndex {
    struct Sources {
        int dff, txd;
        uint32_t dffPosition, dffSectors;
        Sources() : dff(-1), txd(-1), dffPosition(0), dffSectors(0) {}
    };
    std::map<std::string, Sources> sources;
    static std::string Key(const char *name) {
        std::string key(name ? name : "");
        for(size_t i = 0; i < key.size(); i++)
            if(key[i] >= 'A' && key[i] <= 'Z') key[i] += 'a' - 'A';
        return key;
    }
public:
    void Clear() { sources.clear(); }
    void AddDirectory(int image, const uint8_t *dir, size_t length) {
        if(image < 0 || image >= 256) return;
        ForEachModelOrTxd(dir, length, [this, image](const std::string &name, bool txd,
                                                      uint32_t sector, uint32_t sectors) {
            Sources &s = sources[Key(name.c_str())];
            if(txd) {
                if(image > s.txd) s.txd = image;
            } else if(image > s.dff) {
                // Within one IMG keep its first DFF, just like native DIR
                // streaming. A higher image number wins between IMGs.
                s.dff = image;
                s.dffPosition = (uint32_t(image)<<24) | sector;
                s.dffSectors = sectors;
            }
        });
    }
    bool FindDffRecord(const char *model, uint32_t &position, uint32_t &sectors) const {
        if(model == nullptr) return false;
        std::map<std::string, Sources>::const_iterator it = sources.find(Key(model));
        if(it == sources.end() || it->second.dff < 0) return false;
        position = it->second.dffPosition;
        sectors = it->second.dffSectors;
        return true;
    }
    int FindDffImage(const char *model) const {
        if(model == nullptr) return -1;
        std::map<std::string, Sources>::const_iterator it = sources.find(Key(model));
        return it == sources.end() ? -1 : it->second.dff;
    }
    // Return the winning TXD image and (optionally) the winning DFF image.
    // Callers decide which images are eligible, e.g. only user-mounted IMGs.
    int FindTxdImage(const char *model, int *dffImage = nullptr) const {
        if(dffImage) *dffImage = -1;
        if(model == nullptr) return -1;
        std::map<std::string, Sources>::const_iterator it = sources.find(Key(model));
        if(it == sources.end() || it->second.dff < 0 || it->second.txd < 0)
            return -1;
        if(dffImage) *dffImage = it->second.dff;
        return it->second.txd;
    }
};

// Collision-only IMG v1 pairs are valid too. COL/CLS records are checked with
// the same bounds as models; their payloads are converted separately into VC
// COLL and must never be passed to the stock COL slot streamer as SA bytes.
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
        // 19 bytes before the dot. Don't register entries it cannot name.
        const uint8_t *name = record+8;
        const uint8_t *dot = (const uint8_t *)std::memchr(name, '.', 24);
        if(!dot || dot == name || dot-name > 19 || dot[4] != 0 ||
           std::memchr(name, 0, dot-name)) {
            stats.ignored++; continue;
        }
        bool dff = Extension(dot+1, 3, "DFF"), txd = Extension(dot+1, 3, "TXD");
        bool col = Extension(dot+1, 3, "COL"), cls = Extension(dot+1, 3, "CLS");
        if(!dff && !txd && !col && !cls) {
            stats.ignored++; continue; // IFP/MOD/BTX are not IMG resources
        }
        uint32_t offset = U32(record), sectors = U32(record+4);
        if(offset >= (1u<<24) || !sectors || sectors > 32768 ||
           (uint64_t)offset*2048 + (uint64_t)sectors*2048 > imageBytes) {
            // A malformed collision entry must not hide usable DFF/TXD or
            // other COL/CLS entries in the same IMG. Reject the whole image
            // only for malformed native-streamed DFF/TXD records.
            if(col || cls) { stats.badCollision++; continue; }
            error = "DIR sector offset/length outside IMG or streaming limit";
            return false;
        }
        if(sectors > stats.largestSectors) stats.largestSectors = sectors;
        if(dff) stats.dff++;
        else if(txd) stats.txd++;
        else if(col) stats.col++;
        else stats.cls++;
    }
    if(!stats.dff && !stats.txd && !stats.col && !stats.cls) {
        error = "no DFF/TXD/COL/CLS records in VC .dir"; return false;
    }
    return true;
}

// Enumerate sector slices, not filenames on the host filesystem. This helper
// rechecks the DIR record even after Validate so it is safe with untrusted or
// subsequently changed metadata. IMG order determines collision precedence.
template <class Visit>
inline void ForEachCollisionEntry(const uint8_t *dir, size_t length,
                                  uint64_t imageBytes, Visit visit) {
    if(!dir || length % 32) return;
    for(size_t pos = 0; length-pos >= 32; pos += 32) {
        const uint8_t *record = dir+pos;
        const uint8_t *name = record+8;
        const uint8_t *dot = (const uint8_t *)std::memchr(name, '.', 24);
        if(!dot || dot == name || dot-name > 19 || dot[4] != 0 ||
           std::memchr(name, 0, dot-name)) continue;
        bool col = Extension(dot+1, 3, "COL"), cls = Extension(dot+1, 3, "CLS");
        if(!col && !cls) continue;
        uint32_t sector = U32(record), sectors = U32(record+4);
        uint64_t start = (uint64_t)sector*2048, bytes = (uint64_t)sectors*2048;
        if(sector >= (1u<<24) || !sectors || sectors > 32768 ||
           start > imageBytes || bytes > imageBytes-start) continue;
        visit(std::string((const char *)name, dot-name), cls, sector, sectors);
    }
}

} // namespace customimg
#endif
