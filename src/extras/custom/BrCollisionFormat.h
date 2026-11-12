#ifndef REVC_BR_COLLISION_FORMAT_H
#define REVC_BR_COLLISION_FORMAT_H

// The BR .cls container is not an SA .col, and it is not a VC record either.
//
// Layout, per PUNCT ANALYSE v45 section 10.3 (17 606 records parsed from 81
// real .cls files, 0 errors):
//
//   tag[4] + u32 size + payload,   next record at 8 + size
//   payload + 0x00   char name[24]
//   payload + 0x18   the structure the client's unpacker reads as `st`
//
// CLST keeps its arrays behind pointers, counted from `st`:
//
//   st+0x00 center   st+0x0c radius   st+0x10 bbox.min   st+0x1c bbox.max
//   st+0x28 ns(i16)  st+0x2a nb(u16)  st+0x2c nt(i16)    st+0x30 flags
//   st+0x34 pSpheres st+0x38 pBoxes   st+0x40 pVertices  st+0x44 pTriangles
//
//   address of an array = st + (field - 0x1c), i.e. block + value + 4
//
// The three things that are easy to get wrong here, all confirmed on data:
//   * spheres are x,y,z,r (COLL stores r,x,y,z - the orders are not shared);
//   * vertices are 6 bytes, s16 x3 scaled by 1/128, not float3;
//   * triangles are 8 bytes (HHHBB) and end the record exactly.
//
// CLSF has no pointers: a 40-byte header (bbox.min, bbox.max, center, radius)
// followed by two counters, then one of two mutually exclusive bodies.
//
// Nothing here is passed through to the game's COLL reader unchanged: a .cls
// block is converted to the COLL v1 body that reader already validates.
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
inline int16_t I16(const uint8_t *p) { return (int16_t)U16(p); }
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
inline bool IsClsf(const uint8_t *p) { return std::memcmp(p, "CLSF", 4) == 0; }

// The first 24 bytes of the payload hold the model name. Some records carry a
// four-byte "CED2" marker at payload+20 instead of padding; when it is there the
// name field is the 20 bytes in front of it (a full 20-character name has no
// terminator, which is legal - the client stores it in a fixed field too). A
// record without the marker simply has up to 24 characters of name.
inline bool Name(const uint8_t *p, size_t n, std::string &name) {
    if(n < 8 + 24) return false;
    const uint8_t *field = p + 8;
    size_t limit = 24;
    if(std::memcmp(field + 20, "CED2", 4) == 0) limit = 20;
    size_t len = 0;
    while(len < limit && field[len] != 0) len++;
    while(len > 0 && field[len - 1] == ' ') len--;
    if(len == 0) return false;
    for(size_t k = 0; k < len; k++)
        if(field[k] < 32 || field[k] > 126 || field[k] == '/' || field[k] == '\\') return false;
    name.assign((const char *)field, len);
    return true;
}

// Walk by declared length, never by searching the payload for a tag: a float in
// a bounding box can spell "CLST" by accident. Zero padding between blocks (an
// IMG entry is sector rounded, converted archives align) is skipped; anything
// else that is not a tag is a defect and is counted, but the good blocks in
// front of it are kept.
inline size_t Index(const uint8_t *data, size_t size, std::vector<Block> &blocks) {
    size_t bad = 0;
    if(!data) return size ? 1 : 0;
    for(size_t pos = 0; pos < size;) {
        if(!(size - pos >= 4 && Tag(data + pos))) {
            size_t skip = pos;
            while(skip < size && data[skip] == 0) skip++;
            if(skip == pos) { bad++; break; }
            pos = skip;
            if(pos >= size) break;
            continue;
        }
        if(size - pos < 8) { bad++; break; }
        // Smallest payload either tag can have and still hold its header: 24
        // bytes of name plus the 0x48-byte CLST structure, or plus the 0x34
        // bytes of a CLSF down to its numBoxes counter. The corpus' smallest
        // real records are 104 (empty CLST) and 84 (empty CLSF), so this floor
        // is below every record it has to accept and only rejects a length that
        // could not describe one.
        uint32_t minLen = IsClsf(data + pos) ? (0x18u + 0x34u) : (0x18u + 0x48u);
        uint32_t length = U32(data + pos + 4);
        if(length < minLen || length > size - pos - 8) { bad++; break; }
        size_t n = (size_t)length + 8;
        std::string name;
        if(Name(data + pos, n, name)) {
            Block b = { name, pos, n };
            blocks.push_back(b);
        } else bad++;
        pos += n;
    }
    return bad;
}

inline uint8_t Surface(uint8_t s) {
    if(s <= 34) return s;
    if(s > 178) return 0;
    if(s == 80 || s == 81 || s == 83 || (s >= 115 && s <= 125) || s == 141 || s == 146 || s == 147 || s == 153)
        return 2; // SURFACE_GRASS
    if(s == 35 || s == 40 || s == 41 || (s >= 84 && s <= 89) || (s >= 96 && s <= 100) || (s >= 126 && s <= 139))
        return 3; // SURFACE_MUD_DRY
    if(s == 74 || s == 75 || s == 82 || s == 90 || s == 157)
        return 19; // SURFACE_SAND
    if(s == 36 || s == 37 || (s >= 76 && s <= 79) || s == 101 || s == 140)
        return 17; // SURFACE_GRAVEL
    if((s >= 42 && s <= 44) || s == 72 || s == 73 || s == 143 || s == 144 || s >= 172)
        return 24; // SURFACE_WOOD_SOLID
    if(s >= 45 && s <= 47)
        return 7; // SURFACE_GLASS
    if((s >= 50 && s <= 68) || s == 71 || s == 142 || (s >= 162 && s <= 168) || s == 171)
        return 16; // SURFACE_METAL_CHAIN_FENCE
    if(s == 38 || s == 39)
        return 20; // SURFACE_WATER
    if(s == 70 || s == 102 || s == 108 || s == 145)
        return 5; // SURFACE_PAVEMENT
    return 0;
}

// Length of the CLST/CLSF structure the client's unpacker fills: pointers end
// at st+0x48, and every array address is derived from st.
enum { ST_SIZE = 0x48 };

// One converted model, in the layout the game's own reader expects: the VC
// COLL v1 body (radius, centre, bbox, then counts interleaved with their
// arrays). Written by hand rather than by reusing a block, so a .cls block can
// never reach the native reader with a .cls header on it.
struct Out {
    std::vector<uint8_t> &v;
    void Header(float radius, const float center[3], const float bmin[3], const float bmax[3]) {
        PutFloat(v, radius);
        for(int k = 0; k < 3; k++) PutFloat(v, center[k]);
        for(int k = 0; k < 3; k++) PutFloat(v, bmin[k]);
        for(int k = 0; k < 3; k++) PutFloat(v, bmax[k]);
    }
    void Finish() {
        uint32_t length = (uint32_t)v.size() - 8;
        for(int j = 0; j < 4; j++) v[4 + j] = (uint8_t)(length >> (j * 8));
    }
};

inline void StartBlock(std::vector<uint8_t> &out, const std::string &name) {
    out.clear();
    out.insert(out.end(), (const uint8_t *)"COLL", (const uint8_t *)"COLL" + 4);
    Put32(out, 0);              // patched by Finish
    out.resize(32, 0);          // name[24], no raw .cls bytes in a VC header
    std::memcpy(out.data() + 8, name.data(), name.size());
}

inline bool SaneBox(const float mn[3], const float mx[3]) {
    for(int k = 0; k < 3; k++) {
        if(!std::isfinite(mn[k]) || !std::isfinite(mx[k])) return false;
        if(std::fabs(mn[k]) > 1e6f || std::fabs(mx[k]) > 1e6f) return false;
        if(mx[k] < mn[k]) return false;
    }
    return true;
}

// ---- CLST -------------------------------------------------------------
//
// A CLST record is empty when flags, ns, nb and nt are all zero (6 205 of the
// 17 594 records in the corpus). Such a record carries a bounding box and
// nothing else. It is *not* a reason to fail: the game installs it, and the
// port has to keep behaving the same way, so it is converted to the COLL body
// with every count at zero and the caller decides what to do with it. The one
// thing that must never happen is a record that is reported as converted while
// its geometry was dropped on the floor, so a record that says it has n
// triangles and cannot produce them is refused with a reason.
inline bool ConvertClst(const uint8_t *p, size_t n, const std::string &name,
                        std::vector<uint8_t> &out, const char **why) {
    auto no = [&](const char *r) { if(why) *why = r; out.clear(); return false; };
    if(n < 8 + 24 + ST_SIZE) return no("CLST record shorter than its 24-byte name and 0x48-byte structure");
    const uint8_t *st = p + 8 + 0x18;
    const size_t stOff = (size_t)(st - p);          // 0x20 for a .cls block

    for(int k = 0; k < 10; k++)
        if(!std::isfinite(F32(st + 4 * k))) return no("bounds contain NaN or infinity");
    float center[3] = { F32(st + 0x00), F32(st + 0x04), F32(st + 0x08) };
    float radius = F32(st + 0x0c);
    float bmin[3] = { F32(st + 0x10), F32(st + 0x14), F32(st + 0x18) };
    float bmax[3] = { F32(st + 0x1c), F32(st + 0x20), F32(st + 0x24) };
    if(!std::isfinite(radius) || radius < 0.0f || radius > 1e6f)
        return no("bounding radius out of range");
    if(!SaneBox(bmin, bmax)) return no("bounding box is inside out or out of range");

    int ns = I16(st + 0x28);
    int nb = (int)U16(st + 0x2a);
    int nt = I16(st + 0x2c);
    // Bit 3 of the flags says the record also carries a block of axis-aligned
    // boxes used to narrow down which triangles to test: 5 154 of the records
    // have it, between the vertices and the triangles. It is acceleration data,
    // not collision geometry, and the port recomputes its own candidate list,
    // so it is deliberately not converted - the geometry is the same either way.
    uint8_t flags = st[0x30];
    (void)flags;
    if(ns < 0 || nt < 0) return no("negative sphere or triangle count");
    // Addresses are derived from st, so a pointer value is a byte offset into
    // the record; 0 means "no array".
    uint32_t pSph = U32(st + 0x34), pBox = U32(st + 0x38);
    uint32_t pVert = U32(st + 0x40), pTri = U32(st + 0x44);
    const size_t arrBase = 0x1c;

    // 6 205 of the 17 594 records in the corpus carry a bounding box and no
    // geometry at all. That is what the data says, not damage. Installing such
    // a record is what leaves a model owning an empty collision model, so
    // GetColModel() stops being nil and the box-from-the-mesh fallback never
    // runs - the model is then drawn with nothing to stand on. Refusing it here
    // is what makes the caller keep the model's own collision, and the reason
    // string is what names it in the log.
    if(ns == 0 && nb == 0 && nt == 0)
        return no("record has no spheres, boxes or triangles");

    // Spheres and boxes are addressed the same way as the vertices.
    auto addr = [&](uint32_t field, size_t count, size_t item, size_t &pos) -> bool {
        if(field == 0) { pos = 0; return count == 0; }
        if(field < arrBase) return false;
        size_t off = stOff + (size_t)(field - arrBase);
        if(off > n || count > (n - off) / item) return false;
        pos = off;
        return true;
    };

    size_t sphPos = 0, boxPos = 0;
    if(!addr(pSph, (size_t)ns, 20, sphPos)) return no("sphere array points outside the record");
    if(!addr(pBox, (size_t)nb, 28, boxPos)) return no("box array points outside the record");

    // Triangles end the record exactly; the vertex count is not stored, it is
    // maxIndex+1 over the triangles themselves.
    size_t triPos = 0, vertPos = 0, vertEnd = 0;
    uint32_t nv = 0;
    if(nt) {
        if(pTri == 0 || pTri < arrBase) return no("record with triangles has no triangle pointer");
        triPos = stOff + (size_t)(pTri - arrBase);
        if(triPos > n || (size_t)nt > (n - triPos) / 8)
            return no("triangle array does not fit the record");
        // Triangles end the record in all 17 594 CLST records of the corpus; a
        // record where they do not has been read with the wrong pointer base.
        if(triPos + (size_t)nt * 8 != n)
            return no("triangle array does not end the record");
        uint32_t hi = 0;
        for(int k = 0; k < nt; k++) {
            const uint8_t *t = p + triPos + (size_t)k * 8;
            for(int j = 0; j < 3; j++) {
                uint32_t idx = U16(t + j * 2);
                if(idx + 1 > hi) hi = idx + 1;
            }
        }
        nv = hi;
        if(pVert == 0 || pVert < arrBase)
            return no("record with triangles has no vertex pointer");
        vertPos = stOff + (size_t)(pVert - arrBase);
        if(vertPos > n || (size_t)nv > (n - vertPos) / 6)
            return no("vertex array does not fit before the triangles");
        vertEnd = vertPos + (size_t)nv * 6;
        if(vertEnd > triPos) return no("vertex array overlaps the triangles");
    }

    StartBlock(out, name);
    Out o = { out };
    o.Header(radius, center, bmin, bmax);
    Put32(out, (uint32_t)ns);
    for(int k = 0; k < ns; k++) {
        const uint8_t *s = p + sphPos + (size_t)k * 20;
        float x = F32(s + 0), y = F32(s + 4), z = F32(s + 8), r = F32(s + 12);
        for(int j = 0; j < 4; j++) {
            float val = F32(s + j * 4);
            if(!std::isfinite(val) || std::fabs(val) > 1e6f) return no("sphere coordinate out of range");
        }
        if(r < 0.0f) return no("negative sphere radius");
        // CLST is x,y,z,r; the COLL body wants r,x,y,z.
        PutFloat(out, r); PutFloat(out, x); PutFloat(out, y); PutFloat(out, z);
        out.push_back(Surface(s[16])); out.push_back(s[17]); out.push_back(0); out.push_back(0);
    }
    Put32(out, 0);               // lines: the format has no line array
    Put32(out, (uint32_t)nb);
    for(int k = 0; k < nb; k++) {
        const uint8_t *b = p + boxPos + (size_t)k * 28;
        for(int j = 0; j < 6; j++) {
            float x = F32(b + j * 4);
            if(!std::isfinite(x) || std::fabs(x) > 1e6f) return no("box coordinate out of range");
            if(j >= 3 && x < F32(b + (j - 3) * 4)) return no("box is inside out");
            PutFloat(out, x);
        }
        out.push_back(Surface(b[24])); out.push_back(b[25]); out.push_back(0); out.push_back(0);
    }
    Put32(out, nv);
    for(uint32_t k = 0; k < nv; k++) {
        const uint8_t *v = p + vertPos + (size_t)k * 6;
        for(int j = 0; j < 3; j++) PutFloat(out, I16(v + j * 2) / 128.0f);
    }
    Put32(out, (uint32_t)nt);
    for(int k = 0; k < nt; k++) {
        const uint8_t *t = p + triPos + (size_t)k * 8;
        for(int j = 0; j < 3; j++) {
            uint32_t idx = U16(t + j * 2);
            if(idx >= nv) return no("triangle indexes a vertex that is not there");
            Put32(out, idx);
        }
        out.push_back(Surface(t[6])); out.push_back(0); out.push_back(0); out.push_back(0);
    }
    o.Finish();
    return true;
}

// ---- CLSF -------------------------------------------------------------
//
// 12 records, all of them in BRWinter.cls. The header is 40 bytes read straight
// off the payload, with no pointer indirection:
//
//   payload+0x00 bbox.min   +0x0c bbox.max   +0x18 center   +0x24 radius
//   payload+0x28 u32 (0 in all 12)           +0x2c u32 (0 in all 12)
//   payload+0x30 numBoxes   -- body starts at 0x34, 28-byte boxes, 8-byte aligned
//   payload+0x34 numTriangles  -- body starts at 0x38, float3 vertices,
//                                 a 16-byte "fast" list, then the 16-byte
//                                 triangles that end the record
//
// Both counters are zero in 6 of the 12: an empty 84-byte record.
//
// The "fast" 16-byte list is left alone. It holds the same fields as the main
// triangles but does not take part in numTriangles, and one record in the
// corpus is not enough to prove what it is for; the main array is the model.
inline bool ConvertClsf(const uint8_t *p, size_t n, const std::string &name,
                        std::vector<uint8_t> &out, const char **why) {
    auto no = [&](const char *r) { if(why) *why = r; out.clear(); return false; };
    // The smallest record in the corpus is 84 bytes: 24 name + 40 header + the
    // four bytes of numBoxes. numTriangles is only present when the record is
    // longer than that.
    if(n < 8 + 0x18 + 0x34) return no("CLSF record shorter than its header");
    const uint8_t *st = p + 8 + 0x18;
    float bmin[3] = { F32(st + 0x00), F32(st + 0x04), F32(st + 0x08) };
    float bmax[3] = { F32(st + 0x0c), F32(st + 0x10), F32(st + 0x14) };
    float center[3] = { F32(st + 0x18), F32(st + 0x1c), F32(st + 0x20) };
    float radius = F32(st + 0x24);
    for(int k = 0; k < 3; k++) {
        if(!std::isfinite(bmin[k]) || !std::isfinite(bmax[k]) ||
           !std::isfinite(center[k])) return no("bounds contain NaN or infinity");
    }
    if(!std::isfinite(radius) || radius < 0.0f || radius > 1e6f)
        return no("bounding radius out of range");
    if(!SaneBox(bmin, bmax)) return no("bounding box is inside out or out of range");
    // The two bodies are mutually exclusive: either +0x30 is numBoxes and the
    // boxes start at +0x34 (so +0x34 is the first box, not a counter), or
    // numBoxes is zero and +0x34 is numTriangles. Reading both as counters
    // makes a box record look like it also has triangles.
    uint32_t numBoxes = U32(st + 0x30);
    uint32_t numTris = 0;
    if(numBoxes == 0) {
        if(n < 8 + 0x18 + 0x38) return no("record has no boxes or triangles");
        numTris = U32(st + 0x34);
        if(numTris == 0) return no("record has no boxes or triangles");
    }

    if(numBoxes) {
        size_t boxPos = (size_t)(st - p) + 0x34;
        if(numBoxes > 4096 || boxPos > n || (size_t)numBoxes > (n - boxPos) / 28)
            return no("CLSF box array does not fit the record");
        StartBlock(out, name);
        Out o = { out };
        o.Header(radius, center, bmin, bmax);
        Put32(out, 0);           // spheres
        Put32(out, 0);           // lines
        Put32(out, numBoxes);
        for(uint32_t k = 0; k < numBoxes; k++) {
            const uint8_t *b = p + boxPos + (size_t)k * 28;
            for(int j = 0; j < 6; j++) {
                float x = F32(b + j * 4);
                if(!std::isfinite(x) || std::fabs(x) > 1e6f)
                    return no("CLSF box coordinate out of range");
                if(j >= 3 && x < F32(b + (j - 3) * 4)) return no("CLSF box is inside out");
                PutFloat(out, x);
            }
            out.push_back(Surface(b[24])); out.push_back(b[25]); out.push_back(0); out.push_back(0);
        }
        Put32(out, 0);           // vertices
        Put32(out, 0);           // triangles
        o.Finish();
        return true;
    }

    // Triangles: float3 vertices at 0x38, main array ending the record.
    if(numTris > 65535) return no("CLSF triangle count out of range");
    size_t triPos = n - (size_t)numTris * 16;
    size_t vertPos = (size_t)(st - p) + 0x38;
    if(triPos < vertPos) return no("CLSF triangles do not fit behind the vertices");
    uint32_t nv = 0;
    for(uint32_t k = 0; k < numTris; k++) {
        const uint8_t *t = p + triPos + (size_t)k * 16;
        for(int j = 0; j < 3; j++) {
            uint32_t idx = U32(t + j * 4);
            if(idx + 1 > nv) nv = idx + 1;
        }
    }
    if(nv == 0) return no("CLSF triangles index no vertices");
    if(vertPos > n || (size_t)nv > (n - vertPos) / 12)
        return no("CLSF vertex count does not fit before the triangles");
    size_t vertEnd = vertPos + (size_t)nv * 12;
    // Between the vertices and the main triangles sits the 16-byte fast list.
    // It must not overlap either array; where it is empty this is an identity.
    if(vertEnd > triPos) return no("CLSF vertices overlap the triangles");

    StartBlock(out, name);
    Out o = { out };
    o.Header(radius, center, bmin, bmax);
    Put32(out, 0);               // spheres
    Put32(out, 0);               // lines
    Put32(out, 0);               // boxes
    Put32(out, nv);
    for(uint32_t k = 0; k < nv; k++) {
        const uint8_t *v = p + vertPos + (size_t)k * 12;
        for(int j = 0; j < 3; j++) {
            float x = F32(v + j * 4);
            if(!std::isfinite(x) || std::fabs(x) > 1e6f)
                return no("CLSF vertex coordinate out of range");
            PutFloat(out, x);
        }
    }
    Put32(out, numTris);
    for(uint32_t k = 0; k < numTris; k++) {
        const uint8_t *t = p + triPos + (size_t)k * 16;
        for(int j = 0; j < 3; j++) {
            uint32_t idx = U32(t + j * 4);
            if(idx >= nv) return no("CLSF triangle indexes a vertex that is not there");
            Put32(out, idx);
        }
        out.push_back(Surface(t[12])); out.push_back(0); out.push_back(0); out.push_back(0);
    }
    o.Finish();
    return true;
}

// Convert exactly one CLST or CLSF record into a [COLL, size, name[24], COLL v1
// body]. `p` is the record including its 8-byte tag and size.
inline bool Convert(const uint8_t *p, size_t n, std::vector<uint8_t> &out, const char **why = nullptr) {
    auto no = [&](const char *r) { if(why) *why = r; out.clear(); return false; };
    out.clear();
    if(!p) return no("no data");
    if(n > 16u * 1024u * 1024u) return no("record larger than 16 MB");
    if(n < 8 || !Tag(p)) return no("not a CLST or CLSF record");
    if(U32(p + 4) != n - 8) return no("declared size does not match the record");
    std::string name;
    if(!Name(p, n, name)) return no("record has no usable model name");
    return IsClsf(p) ? ConvertClsf(p, n, name, out, why)
                      : ConvertClst(p, n, name, out, why);
}
} // namespace brcollision
#endif
