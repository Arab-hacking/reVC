#ifndef REVC_BR_COLLISION_FORMAT_H
#define REVC_BR_COLLISION_FORMAT_H

// The BR .cls container is NOT an SA .col: CLST and CLSF both have name[20]
// followed by CED2 and bounds in (sphere centre,radius; bbox) order. Validate
// every offset before converting one block to the old COLL layout read by VC.
// No game or RenderWare dependencies, so this is also fuzz/sanitizer testable.
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace brcollision {

struct Block {
    std::string name;
    size_t offset, size;
};

inline uint16_t U16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
inline uint32_t U32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
inline float F32(const uint8_t *p) {
    float f; std::memcpy(&f, p, 4); return f;
}
inline void Put32(std::vector<uint8_t> &out, uint32_t n) {
    out.push_back((uint8_t)n); out.push_back((uint8_t)(n >> 8));
    out.push_back((uint8_t)(n >> 16)); out.push_back((uint8_t)(n >> 24));
}
inline void PutFloat(std::vector<uint8_t> &out, float f) {
    uint32_t bits; std::memcpy(&bits, &f, 4); Put32(out, bits);
}
inline bool Tag(const uint8_t *p) {
    return std::memcmp(p, "CLST", 4) == 0 || std::memcmp(p, "CLSF", 4) == 0;
}
inline bool Header(const uint8_t *p, size_t n, std::string &name) {
    if(n < 120 || !Tag(p) || U32(p+4) != n-8 ||
       std::memcmp(p+28, "CED2", 4) != 0) return false;
    const uint8_t *nul = (const uint8_t *)std::memchr(p+8, 0, 20);
    if(!nul || nul == p+8) return false;
    for(const uint8_t *c = p+8; c < nul; c++)
        if(*c < 33 || *c > 126 || *c == '/' || *c == '\\') return false;
    name.assign((const char *)p+8, nul-(p+8));
    return true;
}

// Walk by declared lengths, not by searching the compressed mesh for a magic
// string (which can appear inside the payload). Reject truncated/invalid
// chunks; still allow good chunks before a bad tail.
inline size_t Index(const uint8_t *data, size_t size, std::vector<Block> &blocks) {
    size_t bad = 0;
    if(!data) return size ? 1 : 0;
    for(size_t pos = 0; pos < size;) {
        if(size-pos < 8) { bad++; break; }
        uint32_t length = U32(data+pos+4);
        if(length < 112 || length > size-pos-8) { bad++; break; }
        size_t n = (size_t)length+8;
        std::string name;
        if(Header(data+pos, n, name)) {
            Block b = { name, pos, n }; blocks.push_back(b);
        } else bad++;
        pos += n;
    }
    return bad;
}

inline bool Range(size_t off, size_t count, size_t item, size_t total) {
    return off >= 120 && off <= total && count <= (total-off)/item;
}
inline uint8_t Surface(uint8_t s) { return s <= 34 ? s : 0; }

// Convert exactly one validated CLST/CLSF CED2 block into [COLL, size,
// name[24], VC body]. Refuse uncertain or overlapping geometry, never pass
// raw .cls through the unsafe VC collision reader. The A/B elements are SA's
// 20-byte spheres and 28-byte boxes; group tables are only for acceleration
// and are not needed by VC's triangle collision implementation.
inline bool Convert(const uint8_t *p, size_t n, std::vector<uint8_t> &out) {
    out.clear();
    std::string name;
    if(!p || !Header(p, n, name) || n > 8u*1024u*1024u) return false;
    for(int k = 0; k < 10; k++) if(!std::isfinite(F32(p+32+4*k))) return false;
    float cx = F32(p+32), cy = F32(p+36), cz = F32(p+40);
    float radius = F32(p+44);
    if(radius < 0.0f || radius > 100000.0f) return false;
    for(int k = 0; k < 3; k++)
        if(F32(p+60+4*k) < F32(p+48+4*k) ||
           std::fabs(F32(p+48+4*k)) > 1e6f ||
           std::fabs(F32(p+60+4*k)) > 1e6f) return false;
    // A zero-radius marker can still have a valid bounding box.
    if(radius == 0.0f) {
        float dx = F32(p+60)-F32(p+48), dy = F32(p+64)-F32(p+52), dz = F32(p+68)-F32(p+56);
        radius = std::sqrt(dx*dx+dy*dy+dz*dz) * 0.5f;
    }
    uint32_t nA = U16(p+72), nB = U16(p+74), nF = U16(p+76);
    uint8_t version = p[80];
    if(nA > 32767 || nB > 32767 || nF > 32767) return false;
    uint32_t a = U32(p+84), b = U32(p+88), v = U32(p+96), f = U32(p+100);
    size_t aPos = (size_t)a+4, bPos = (size_t)b+4;
    size_t vPos = (size_t)v+4, fPos = (size_t)f+4;
    if((nA && (!a || !Range(aPos, nA, 20, n))) ||
       (nB && (!b || !Range(bPos, nB, 28, n))) ||
       (nF && (!f || !v || !Range(fPos, nF, 8, n) || vPos < 120 || vPos >= fPos)))
        return false;
    // A group count is stored immediately before faces in the full format.
    // Validate it so that group records cannot be mistaken for vertices.
    size_t endVerts = nF ? fPos : n;
    if(nF && (version & 8)) {
        if(fPos < 124) return false;
        uint32_t nGroups = U32(p+fPos-4);
        if(nGroups > 4096 || (size_t)nGroups > (fPos-4-120)/28) return false;
        endVerts = fPos-4-(size_t)nGroups*28;
        if(nGroups) {
            uint32_t next = 0;
            for(uint32_t g = 0; g < nGroups; g++) {
                const uint8_t *group = p+endVerts+g*28;
                uint32_t first = U16(group+24), last = U16(group+26);
                if(first != next || last < first || last >= nF) return false;
                next = last+1;
            }
            if(next != nF) return false;
        }
    }
    uint32_t nV = 0;
    for(uint32_t k = 0; k < nF; k++) {
        const uint8_t *face = p+fPos+(size_t)k*8;
        uint32_t hi = U16(face);
        if(U16(face+2) > hi) hi = U16(face+2);
        if(U16(face+4) > hi) hi = U16(face+4);
        if(hi >= 32767) return false;
        if(hi+1 > nV) nV = hi+1;
    }
    if(nF && (endVerts < vPos || !Range(vPos, nV, 6, endVerts))) return false;
    // Disjoint arrays. In particular do not read a group record or a face
    // as a vertex/sphere/box if an offset in the chunk is corrupt.
    size_t aEnd = aPos+(size_t)nA*20, bEnd = bPos+(size_t)nB*28;
    if(nA && nB && aPos < bEnd && bPos < aEnd) return false;
    if(nF && ((nA && aPos < endVerts && aEnd > vPos) ||
              (nB && bPos < endVerts && bEnd > vPos) ||
              (nA && aPos < n && aEnd > endVerts) ||
              (nB && bPos < n && bEnd > endVerts))) return false;

    out.reserve(8+24+40+20+(size_t)nA*20+(size_t)nB*28+(size_t)nV*12+(size_t)nF*16);
    out.insert(out.end(), (const uint8_t *)"COLL", (const uint8_t *)"COLL"+4);
    Put32(out, 0); // length filled below
    out.resize(32, 0); // name[24], no raw CED2 bytes in a VC header
    std::memcpy(out.data()+8, name.data(), name.size());
    PutFloat(out, radius);
    PutFloat(out, cx); PutFloat(out, cy); PutFloat(out, cz);
    for(int k = 0; k < 6; k++) PutFloat(out, F32(p+48+k*4));
    Put32(out, nA);
    for(uint32_t k = 0; k < nA; k++) {
        const uint8_t *s = p+aPos+(size_t)k*20;
        for(int j = 0; j < 4; j++)
            if(!std::isfinite(F32(s+j*4)) || std::fabs(F32(s+j*4)) > 1e6f) {
                out.clear(); return false;
            }
        if(F32(s+12) < 0.0f) { out.clear(); return false; }
        PutFloat(out, F32(s+12));
        for(int j = 0; j < 3; j++) PutFloat(out, F32(s+j*4));
        out.push_back(Surface(s[16])); out.push_back(s[17]); out.push_back(0); out.push_back(0);
    }
    Put32(out, 0); // lines, not present in CED2
    Put32(out, nB);
    for(uint32_t k = 0; k < nB; k++) {
        const uint8_t *box = p+bPos+(size_t)k*28;
        for(int j = 0; j < 6; j++) {
            float x = F32(box+j*4);
            if(!std::isfinite(x) || std::fabs(x) > 1e6f ||
               (j >= 3 && x < F32(box+(j-3)*4))) {
                out.clear(); return false;
            }
            PutFloat(out, x);
        }
        out.push_back(Surface(box[24])); out.push_back(box[25]); out.push_back(0); out.push_back(0);
    }
    Put32(out, nV);
    for(uint32_t k = 0; k < nV; k++) {
        const uint8_t *vert = p+vPos+(size_t)k*6;
        for(int j = 0; j < 3; j++) PutFloat(out, (int16_t)U16(vert+j*2) / 128.0f);
    }
    Put32(out, nF);
    for(uint32_t k = 0; k < nF; k++) {
        const uint8_t *face = p+fPos+(size_t)k*8;
        for(int j = 0; j < 3; j++) Put32(out, U16(face+j*2));
        out.push_back(Surface(face[6])); out.push_back(0); out.push_back(0); out.push_back(0);
    }
    uint32_t length = (uint32_t)out.size()-8;
    for(int j = 0; j < 4; j++) out[4+j] = (uint8_t)(length >> (j*8));
    return true;
}

} // namespace brcollision
#endif
