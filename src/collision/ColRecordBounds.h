#ifndef REVC_COL_RECORD_BOUNDS_H
#define REVC_COL_RECORD_BOUNDS_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cmath>

// VC COLL body (after tag/length/name). Validate the complete record before
// mutating a live model. Counts are never clamped: that would shift all arrays.
namespace colrecord {
inline uint32_t U32(const uint8_t *p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
struct Layout {
    uint32_t count[5];
    size_t offset[5];
};
inline bool Validate(const uint8_t *p, size_t n, Layout &out) {
    if(!p || n < 60) return false;
    for(size_t i = 0; i < 10; ++i) {
        float f; std::memcpy(&f, p + i*4, 4);
        if(!std::isfinite(f)) return false;
    }
    const size_t widths[] = {20, 24, 28, 12, 16};
    size_t at = 40;
    for(int section = 0; section < 5; ++section) {
        if(n - at < 4) return false;
        uint32_t count = U32(p + at); at += 4;
        if(count > 32767 || count > (n - at) / widths[section]) return false;
        out.count[section] = count;
        out.offset[section] = at;
        at += size_t(count) * widths[section];
    }
    // All geometry coordinates/radii must be finite before they reach the
    // physics code (or a compressed-vector float-to-integer conversion).
    const size_t floats[] = {4, 6, 6, 3};
    for(int section = 0; section < 4; ++section)
        for(uint32_t i = 0; i < out.count[section]; ++i)
            for(size_t j = 0; j < floats[section]; ++j) {
                float f;
                std::memcpy(&f, p + out.offset[section] + size_t(i)*widths[section] + j*4, 4);
                if(!std::isfinite(f)) return false;
            }
    for(uint32_t i = 0; i < out.count[4]; ++i) {
        const uint8_t *face = p + out.offset[4] + size_t(i)*16;
        if(U32(face) >= out.count[3] || U32(face+4) >= out.count[3] || U32(face+8) >= out.count[3])
            return false;
    }
    return true;
}
} // namespace colrecord
#endif
