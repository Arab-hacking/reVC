#ifndef REVC_CUSTOM_COL_FORMAT_H
#define REVC_CUSTOM_COL_FORMAT_H

// Index model names inside ZIP .col containers. A COLL block is already in
// Vice City's v1 layout; COL2/COL3 must be converted before its body can be
// passed to CFileLoader::LoadCollisionModel. Validate the v1 layout first:
// that native reader has no length argument and trusts every count/vertex.
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace customcolformat {

struct Block {
    std::string name;
    size_t offset, size; // offset of fourcc, total block size including header
    bool vc;
};

inline uint32_t U32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
inline float F32(const uint8_t *p) {
    float f; std::memcpy(&f, p, 4); return f;
}
inline bool Tag(const uint8_t *p) {
    return std::memcmp(p, "COLL", 4) == 0 ||
           std::memcmp(p, "COL2", 4) == 0 ||
           std::memcmp(p, "COL3", 4) == 0;
}

// Contiguous records, never scan the body byte-by-byte for apparent headers.
// Index valid leading records even if a later one is corrupt. Sector padding
// consisting entirely of zero bytes is harmless.
inline size_t Index(const uint8_t *data, size_t length, std::vector<Block> &blocks) {
    size_t bad = 0, pos = 0;
    if(!data) return length ? 1 : 0;
    while(pos < length) {
        bool padding = true;
        if(length-pos < 8 || !Tag(data+pos)) {
            for(size_t k = pos; k < length; k++)
                if(data[k]) { padding = false; break; }
            if(!padding) bad++;
            break;
        }
        uint32_t size = U32(data+pos+4);
        if(size < 64 || size > length-pos-8) { bad++; break; }
        const uint8_t *name = data+pos+8;
        const uint8_t *end = (const uint8_t *)std::memchr(name, 0, 22);
        if(!end || end == name || end-name > 20) bad++;
        else {
            bool printable = true;
            for(const uint8_t *p = name; p < end; p++)
                if(*p <= 32 || *p >= 127 || *p == '/' || *p == '\\') printable = false;
            if(printable) {
                Block b = { std::string((const char *)name, end-name), pos,
                            (size_t)size+8, std::memcmp(data+pos, "COLL", 4) == 0 };
                blocks.push_back(b);
            } else bad++;
        }
        pos += (size_t)size+8;
    }
    return bad;
}

inline bool Take(size_t n, size_t &pos, uint32_t count, size_t width) {
    if(count > 32767 || pos > n || count > (n-pos)/width) return false;
    pos += (size_t)count*width;
    return true;
}
inline bool Count(const uint8_t *p, size_t n, size_t &pos, uint32_t &count) {
    if(pos > n || n-pos < 4) return false;
    count = U32(p+pos); pos += 4;
    return count <= 32767;
}

inline bool ValidVC(const uint8_t *p, size_t n) {
    if(!p || n < 92 || std::memcmp(p, "COLL", 4) != 0 || U32(p+4) != n-8)
        return false;
    for(int i = 0; i < 10; i++)
        if(!std::isfinite(F32(p+32+i*4)) || std::fabs(F32(p+32+i*4)) > 1e6f)
            return false;
    if(F32(p+32) < 0) return false;
    for(int i = 0; i < 3; i++)
        if(F32(p+60+i*4) < F32(p+48+i*4)) return false;

    size_t at = 72, begin;
    uint32_t spheres, lines, boxes, vertices, faces;
    if(!Count(p, n, at, spheres)) return false;
    begin = at;
    if(!Take(n, at, spheres, 20)) return false;
    for(uint32_t i = 0; i < spheres; i++) {
        const uint8_t *s = p+begin+i*20;
        for(int k = 0; k < 4; k++)
            if(!std::isfinite(F32(s+k*4)) || std::fabs(F32(s+k*4)) > 1e6f) return false;
        if(F32(s) < 0 || s[16] > 34) return false;
    }
    if(!Count(p, n, at, lines) || !Take(n, at, lines, 24)) return false;
    if(!Count(p, n, at, boxes)) return false;
    begin = at;
    if(!Take(n, at, boxes, 28)) return false;
    for(uint32_t i = 0; i < boxes; i++) {
        const uint8_t *b = p+begin+i*28;
        for(int k = 0; k < 6; k++)
            if(!std::isfinite(F32(b+k*4)) || std::fabs(F32(b+k*4)) > 1e6f) return false;
        for(int k = 0; k < 3; k++)
            if(F32(b+(k+3)*4) < F32(b+k*4)) return false;
        if(b[24] > 34) return false;
    }
    if(!Count(p, n, at, vertices)) return false;
    begin = at;
    if(!Take(n, at, vertices, 12)) return false;
    for(uint32_t i = 0; i < vertices*3; i++)
        if(!std::isfinite(F32(p+begin+i*4)) || std::fabs(F32(p+begin+i*4)) > 1e6f)
            return false;
    if(!Count(p, n, at, faces)) return false;
    begin = at;
    if(!Take(n, at, faces, 16)) return false;
    for(uint32_t i = 0; i < faces; i++) {
        const uint8_t *f = p+begin+i*16;
        if(U32(f) >= vertices || U32(f+4) >= vertices || U32(f+8) >= vertices || f[12] > 34)
            return false;
    }
    for(; at < n; at++) if(p[at]) return false; // optional zero alignment
    return true;
}

} // namespace customcolformat
#endif
