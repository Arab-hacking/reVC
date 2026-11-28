#pragma once

// Black Russia .cls format — CLST/CLSF → COLL v1 (Vice City) conversion
// Extracted from src/extras/custom/BrCollisionFormat.h

#include <cstdint>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>

namespace brcls {

struct Block {
    std::string name;
    size_t offset, size;
};

static inline uint16_t U16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static inline int16_t I16(const uint8_t* p) { return (int16_t)U16(p); }
static inline uint32_t U32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static inline float F32(const uint8_t* p) { float f; std::memcpy(&f, p, 4); return f; }
static inline void Put32(std::vector<uint8_t>& out, uint32_t n) {
    out.push_back((uint8_t)n); out.push_back((uint8_t)(n>>8)); out.push_back((uint8_t)(n>>16)); out.push_back((uint8_t)(n>>24));
}
static inline void PutFloat(std::vector<uint8_t>& out, float f) { uint32_t bits; std::memcpy(&bits, &f, 4); Put32(out, bits); }
static inline bool Tag(const uint8_t* p) { return std::memcmp(p, "CLST", 4) == 0 || std::memcmp(p, "CLSF", 4) == 0; }
static inline bool IsClsf(const uint8_t* p) { return std::memcmp(p, "CLSF", 4) == 0; }

// Extract model name from payload (24 bytes, or 20 if CED2 marker at +20)
static inline bool Name(const uint8_t* p, size_t n, std::string& name) {
    if (n < 8 + 24) return false;
    const uint8_t* field = p + 8;
    size_t limit = 24;
    if (std::memcmp(field + 20, "CED2", 4) == 0) limit = 20;
    size_t len = 0;
    while (len < limit && field[len] != 0) len++;
    while (len > 0 && field[len - 1] == ' ') len--;
    if (len == 0) return false;
    for (size_t k = 0; k < len; k++) if (field[k] < 32 || field[k] > 126 || field[k] == '/' || field[k] == '\\') return false;
    name.assign((const char*)field, len);
    return true;
}

// Index all blocks in a .cls container
static inline size_t Index(const uint8_t* data, size_t size, std::vector<Block>& blocks) {
    size_t bad = 0;
    if (!data) return size ? 1 : 0;
    for (size_t pos = 0; pos < size;) {
        if (!(size - pos >= 4 && Tag(data + pos))) {
            size_t skip = pos;
            while (skip < size && data[skip] == 0) skip++;
            if (skip == pos) { bad++; break; }
            pos = skip;
            if (pos >= size) break;
            continue;
        }
        if (size - pos < 8) { bad++; break; }
        uint32_t minLen = IsClsf(data + pos) ? (0x18u + 0x34u) : (0x18u + 0x48u);
        uint32_t length = U32(data + pos + 4);
        if (length < minLen || length > size - pos - 8) { bad++; break; }
        size_t n = (size_t)length + 8;
        std::string name;
        if (Name(data + pos, n, name)) blocks.push_back({ name, pos, n });
        else bad++;
        pos += n;
    }
    return bad;
}

// Surface type mapping (BR → VC)
static inline uint8_t Surface(uint8_t s) {
    if (s <= 34) return s;
    if (s > 178) return 0;
    if (s == 80 || s == 81 || s == 83 || (s >= 115 && s <= 125) || s == 141 || s == 146 || s == 147 || s == 153) return 2;  // GRASS
    if (s == 35 || s == 40 || s == 41 || (s >= 84 && s <= 89) || (s >= 96 && s <= 100) || (s >= 126 && s <= 139)) return 3;  // MUD_DRY
    if (s == 74 || s == 75 || s == 82 || s == 90 || s == 157) return 19;  // SAND
    if (s == 36 || s == 37 || (s >= 76 && s <= 79) || s == 101 || s == 140) return 17;  // GRAVEL
    if ((s >= 42 && s <= 44) || s == 72 || s == 73 || s == 143 || s == 144 || s >= 172) return 24;  // WOOD_SOLID
    if (s >= 45 && s <= 47) return 7;  // GLASS
    if ((s >= 50 && s <= 68) || s == 71 || s == 142 || (s >= 162 && s <= 168) || s == 171) return 16;  // METAL_CHAIN_FENCE
    if (s == 38 || s == 39) return 20;  // WATER
    if (s == 70 || s == 102 || s == 108 || s == 145) return 5;  // PAVEMENT
    return 0;
}

static const size_t ST_SIZE = 0x48;  // CLST structure size

struct Out {
    std::vector<uint8_t>& v;
    void Header(float radius, const float center[3], const float bmin[3], const float bmax[3]) {
        PutFloat(v, radius);
        for (int k = 0; k < 3; k++) PutFloat(v, center[k]);
        for (int k = 0; k < 3; k++) PutFloat(v, bmin[k]);
        for (int k = 0; k < 3; k++) PutFloat(v, bmax[k]);
    }
    void Finish() {
        uint32_t length = (uint32_t)v.size() - 8;
        for (int j = 0; j < 4; j++) v[4 + j] = (uint8_t)(length >> (j * 8));
    }
};

static inline void StartBlock(std::vector<uint8_t>& out, const std::string& name) {
    out.clear();
    out.insert(out.end(), (const uint8_t*)"COLL", (const uint8_t*)"COLL" + 4);
    Put32(out, 0);
    out.resize(32, 0);
    std::memcpy(out.data() + 8, name.data(), name.size());
}

static inline bool SaneBox(const float mn[3], const float mx[3]) {
    for (int k = 0; k < 3; k++) {
        if (!std::isfinite(mn[k]) || !std::isfinite(mx[k])) return false;
        if (std::fabs(mn[k]) > 1e6f || std::fabs(mx[k]) > 1e6f) return false;
        if (mx[k] < mn[k]) return false;
    }
    return true;
}

// ---- CLST conversion ----
static inline bool ConvertClst(const uint8_t* p, size_t n, const std::string& name, std::vector<uint8_t>& out, const char** why) {
    auto no = [&](const char* r) { if (why) *why = r; out.clear(); return false; };
    if (n < 8 + 24 + ST_SIZE) return no("CLST record shorter than header");
    const uint8_t* st = p + 8 + 0x18;
    size_t stOff = (size_t)(st - p);

    for (int k = 0; k < 10; k++) if (!std::isfinite(F32(st + 4*k))) return no("bounds contain NaN/inf");
    float center[3] = { F32(st), F32(st+4), F32(st+8) };
    float radius = F32(st + 0x0c);
    float bmin[3] = { F32(st+0x10), F32(st+0x14), F32(st+0x18) };
    float bmax[3] = { F32(st+0x1c), F32(st+0x20), F32(st+0x24) };
    if (!std::isfinite(radius) || radius < 0 || radius > 1e6f) return no("radius out of range");
    if (!SaneBox(bmin, bmax)) return no("bbox invalid");

    int ns = I16(st + 0x28);
    int nb = (int)U16(st + 0x2a);
    int nt = I16(st + 0x2c);
    if (ns < 0 || nt < 0) return no("negative sphere/triangle count");

    uint32_t pSph = U32(st + 0x34), pBox = U32(st + 0x38), pVert = U32(st + 0x40), pTri = U32(st + 0x44);
    size_t arrBase = 0x1c;

    if (ns == 0 && nb == 0 && nt == 0) return no("empty collision record");

    auto addr = [&](uint32_t field, size_t count, size_t item, size_t& pos) -> bool {
        if (field == 0) { pos = 0; return count == 0; }
        if (field < arrBase) return false;
        size_t off = stOff + (size_t)(field - arrBase);
        if (off > n || count > (n - off) / item) return false;
        pos = off; return true;
    };

    size_t sphPos, boxPos;
    if (!addr(pSph, ns, 20, sphPos)) return no("sphere array OOB");
    if (!addr(pBox, nb, 28, boxPos)) return no("box array OOB");

    size_t triPos = 0, vertPos = 0, vertEnd = 0;
    uint32_t nv = 0;
    if (nt) {
        if (pTri == 0 || pTri < arrBase) return no("triangles but no tri pointer");
        triPos = stOff + (size_t)(pTri - arrBase);
        if (triPos > n || (size_t)nt > (n - triPos) / 8) return no("triangle array OOB");
        if (triPos + (size_t)nt * 8 != n) return no("triangles don't end record");
        uint32_t hi = 0;
        for (int k = 0; k < nt; k++) {
            const uint8_t* t = p + triPos + (size_t)k * 8;
            for (int j = 0; j < 3; j++) { uint32_t idx = U16(t + j*2); if (idx + 1 > hi) hi = idx + 1; }
        }
        nv = hi;
        if (pVert == 0 || pVert < arrBase) return no("triangles but no vert pointer");
        vertPos = stOff + (size_t)(pVert - arrBase);
        if (vertPos > n || (size_t)nv > (n - vertPos) / 6) return no("vertex array OOB");
        vertEnd = vertPos + (size_t)nv * 6;
        if (vertEnd > triPos) return no("vertices overlap triangles");
    }

    StartBlock(out, name);
    Out o = { out };
    o.Header(radius, center, bmin, bmax);
    Put32(out, ns);
    for (int k = 0; k < ns; k++) {
        const uint8_t* s = p + sphPos + (size_t)k * 20;
        float x = F32(s), y = F32(s+4), z = F32(s+8), r = F32(s+12);
        for (int j = 0; j < 4; j++) if (!std::isfinite(F32(s+j*4)) || std::fabs(F32(s+j*4)) > 1e6f) return no("sphere coord OOB");
        if (r < 0) return no("negative sphere radius");
        // CLST: x,y,z,r → COLL: r,x,y,z
        PutFloat(out, r); PutFloat(out, x); PutFloat(out, y); PutFloat(out, z);
        out.push_back(Surface(s[16])); out.push_back(s[17]); out.push_back(0); out.push_back(0);
    }
    Put32(out, 0);  // lines
    Put32(out, nb);
    for (int k = 0; k < nb; k++) {
        const uint8_t* b = p + boxPos + (size_t)k * 28;
        for (int j = 0; j < 6; j++) {
            float x = F32(b + j*4);
            if (!std::isfinite(x) || std::fabs(x) > 1e6f) return no("box coord OOB");
            if (j >= 3 && x < F32(b + (j-3)*4)) return no("box inverted");
            PutFloat(out, x);
        }
        out.push_back(Surface(b[24])); out.push_back(b[25]); out.push_back(0); out.push_back(0);
    }
    Put32(out, nv);
    for (uint32_t k = 0; k < nv; k++) {
        const uint8_t* v = p + vertPos + (size_t)k * 6;
        for (int j = 0; j < 3; j++) PutFloat(out, I16(v + j*2) / 128.0f);
    }
    Put32(out, nt);
    for (int k = 0; k < nt; k++) {
        const uint8_t* t = p + triPos + (size_t)k * 8;
        for (int j = 0; j < 3; j++) {
            uint32_t idx = U16(t + j*2);
            if (idx >= nv) return no("triangle indexes missing vertex");
            Put32(out, idx);
        }
        out.push_back(Surface(t[6])); out.push_back(0); out.push_back(0); out.push_back(0);
    }
    o.Finish();
    return true;
}

// ---- CLSF conversion ----
static inline bool ConvertClsf(const uint8_t* p, size_t n, const std::string& name, std::vector<uint8_t>& out, const char** why) {
    auto no = [&](const char* r) { if (why) *why = r; out.clear(); return false; };
    if (n < 8 + 0x18 + 0x34) return no("CLSF too short");
    const uint8_t* st = p + 8 + 0x18;
    float bmin[3] = { F32(st), F32(st+4), F32(st+8) };
    float bmax[3] = { F32(st+0xc), F32(st+0x10), F32(st+0x14) };
    float center[3] = { F32(st+0x18), F32(st+0x1c), F32(st+0x20) };
    float radius = F32(st+0x24);
    for (int k = 0; k < 3; k++) if (!std::isfinite(bmin[k]) || !std::isfinite(bmax[k]) || !std::isfinite(center[k])) return no("NaN/inf in bounds");
    if (!std::isfinite(radius) || radius < 0 || radius > 1e6f) return no("radius OOB");
    if (!SaneBox(bmin, bmax)) return no("bbox inverted");

    uint32_t numBoxes = U32(st + 0x30);
    uint32_t numTris = 0;
    if (numBoxes == 0) {
        if (n < 8 + 0x18 + 0x38) return no("no boxes or tris");
        numTris = U32(st + 0x34);
        if (numTris == 0) return no("empty CLSF");
    }

    if (numBoxes) {
        size_t boxPos = (size_t)(st - p) + 0x34;
        if (numBoxes > 4096 || boxPos > n || (size_t)numBoxes > (n - boxPos) / 28) return no("boxes OOB");
        StartBlock(out, name); Out o = { out };
        o.Header(radius, center, bmin, bmax);
        Put32(out, 0); Put32(out, 0); Put32(out, numBoxes);
        for (uint32_t k = 0; k < numBoxes; k++) {
            const uint8_t* b = p + boxPos + (size_t)k * 28;
            for (int j = 0; j < 6; j++) {
                float x = F32(b + j*4);
                if (!std::isfinite(x) || std::fabs(x) > 1e6f) return no("box coord OOB");
                if (j >= 3 && x < F32(b + (j-3)*4)) return no("box inverted");
                PutFloat(out, x);
            }
            out.push_back(Surface(b[24])); out.push_back(b[25]); out.push_back(0); out.push_back(0);
        }
        Put32(out, 0); Put32(out, 0); o.Finish(); return true;
    }

    // Triangles: float3 verts at 0x38, tris at end (16 bytes each)
    if (numTris > 65535) return no("too many tris");
    size_t triPos = n - (size_t)numTris * 16;
    size_t vertPos = (size_t)(st - p) + 0x38;
    if (triPos < vertPos) return no("tris don't fit");
    uint32_t nv = 0;
    for (uint32_t k = 0; k < numTris; k++) {
        const uint8_t* t = p + triPos + (size_t)k * 16;
        for (int j = 0; j < 3; j++) { uint32_t idx = U32(t + j*4); if (idx + 1 > nv) nv = idx + 1; }
    }
    if (nv == 0) return no("tris reference no verts");
    if (vertPos > n || (size_t)nv > (n - vertPos) / 12) return no("verts OOB");
    size_t vertEnd = vertPos + (size_t)nv * 12;
    if (vertEnd > triPos) return no("verts overlap tris");

    StartBlock(out, name); Out o = { out };
    o.Header(radius, center, bmin, bmax);
    Put32(out, 0); Put32(out, 0); Put32(out, 0); Put32(out, nv);
    for (uint32_t k = 0; k < nv; k++) {
        const uint8_t* v = p + vertPos + (size_t)k * 12;
        for (int j = 0; j < 3; j++) { float x = F32(v + j*4); if (!std::isfinite(x) || std::fabs(x) > 1e6f) return no("vert coord OOB"); PutFloat(out, x); }
    }
    Put32(out, numTris);
    for (uint32_t k = 0; k < numTris; k++) {
        const uint8_t* t = p + triPos + (size_t)k * 16;
        for (int j = 0; j < 3; j++) { uint32_t idx = U32(t + j*4); if (idx >= nv) return no("tri indexes missing vert"); Put32(out, idx); }
        out.push_back(Surface(t[12])); out.push_back(0); out.push_back(0); out.push_back(0);
    }
    o.Finish(); return true;
}

// Legacy CLSF (5 counters: spheres, lines, boxes, verts, faces)
static inline bool ConvertClsfV1(const uint8_t* p, size_t n, const std::string& name,
                                 float cx, float cy, float cz, float radius, const uint8_t* bounds,
                                 std::vector<uint8_t>& out) {
    if (n < 92 || std::memcmp(p, "CLSF", 4) != 0) return false;
    float mn[3], mx[3];
    for (int k = 0; k < 3; k++) { mn[k] = F32(bounds+k*4); mx[k] = F32(bounds+12+k*4); }
    if (!SaneBox(mn, mx) || !std::isfinite(radius) || radius < 0 || radius > 1e6f ||
        !std::isfinite(cx) || !std::isfinite(cy) || !std::isfinite(cz)) return false;
    size_t pos = 72;
    auto takeCount = [&](uint32_t& cnt, size_t width, const uint8_t*& ptr) -> bool {
        if (n - pos < 4) return false;
        cnt = U32(p + pos); pos += 4;
        if (cnt > 32767 || cnt > (n - pos) / width) return false;
        ptr = p + pos; pos += (size_t)cnt * width; return true;
    };
    uint32_t nS=0,nL=0,nB=0,nV=0,nF=0; const uint8_t *sph=nullptr,*lin=nullptr,*box=nullptr,*vtx=nullptr,*fac=nullptr;
    if (!takeCount(nS,20,sph) || !takeCount(nL,24,lin) || !takeCount(nB,28,box) ||
        !takeCount(nV,12,vtx) || !takeCount(nF,16,fac)) return false;
    for (size_t rem = pos; rem < n; rem++) if (p[rem] != 0) return false;
    // Validate all arrays...
    out.reserve(8+24+40+20+(size_t)nS*20+(size_t)nL*24+(size_t)nB*28+(size_t)nV*12+(size_t)nF*16);
    out.insert(out.end(), (const uint8_t*)"COLL", (const uint8_t*)"COLL"+4);
    Put32(out, 0); out.resize(32,0); std::memcpy(out.data()+8, name.data(), name.size());
    PutFloat(out, radius>0?radius:0.5f); PutFloat(out, cx); PutFloat(out, cy); PutFloat(out, cz);
    for (int k = 0; k < 6; k++) PutFloat(out, F32(bounds+k*4));
    Put32(out, nS);
    for (uint32_t k = 0; k < nS; k++) { const uint8_t* s = sph+(size_t)k*20; out.insert(out.end(), s, s+16); out.push_back(Surface(s[16])); out.push_back(s[17]); out.push_back(0); out.push_back(0); }
    Put32(out, nL); if (nL) out.insert(out.end(), lin, lin+(size_t)nL*24);
    Put32(out, nB);
    for (uint32_t k = 0; k < nB; k++) { const uint8_t* b = box+(size_t)k*28; out.insert(out.end(), b, b+24); out.push_back(Surface(b[24])); out.push_back(b[25]); out.push_back(0); out.push_back(0); }
    Put32(out, nV); if (nV) out.insert(out.end(), vtx, vtx+(size_t)nV*12);
    Put32(out, nF);
    for (uint32_t k = 0; k < nF; k++) { const uint8_t* f = fac+(size_t)k*16; out.insert(out.end(), f, f+12); out.push_back(Surface(f[12])); out.push_back(0); out.push_back(0); out.push_back(0); }
    uint32_t len = (uint32_t)out.size() - 8; for (int j = 0; j < 4; j++) out[4+j] = (uint8_t)(len>>(j*8));
    return true;
}

// Main entry: convert single CLST/CLSF record to COLL v1
static inline bool Convert(const uint8_t* p, size_t n, std::vector<uint8_t>& out, const char** why = nullptr) {
    auto no = [&](const char* r) { if (why) *why = r; out.clear(); return false; };
    if (!p) return no("null");
    if (n > 16u*1024u*1024u) return no("too large");
    if (n < 8 || !Tag(p)) return no("not CLST/CLSF");
    if (U32(p+4) != n - 8) return no("size mismatch");
    std::string name; if (!Name(p, n, name)) return no("bad name");
    if (!IsClsf(p)) return ConvertClst(p, n, name, out, why);
    if (ConvertClsf(p, n, name, out, why)) return true;
    out.clear();
    if (n >= 92 && ConvertClsfV1(p, n, name, F32(p+56), F32(p+60), F32(p+64), F32(p+68), p+32, out)) return true;
    if (n >= 92 && std::memcmp(p+28, "CED2", 4) == 0 && ConvertClsfV1(p, n, name, F32(p+32), F32(p+36), F32(p+40), F32(p+44), p+48, out)) return true;
    return false;
}

} // namespace brcls