#pragma once

// Black Russia .btx format — u32 flags + KTX 1.1 (ASTC/uncompressed) parser
// Extracted from src/extras/custom/br/brtex.h and BpcPipeline.h

#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include <algorithm>

namespace brbtx {

// KTX 1.1 magic
static const uint8_t KTX11_MAGIC[12] = { 0xAB, 'K', 'T', 'X', ' ', '1', '1', 0xBB, 0x0D, 0x0A, 0x1A, 0x0A };

// GL enums
enum GlEnum : uint32_t {
    GL_RGB                      = 0x1907,
    GL_RGBA                     = 0x1908,
    GL_UNSIGNED_BYTE            = 0x1401,
    GL_UNSIGNED_SHORT_4_4_4_4   = 0x8033,
    GL_UNSIGNED_SHORT_5_6_5     = 0x8363,
    GL_UNSIGNED_SHORT_5_5_5_1   = 0x8034,
    GL_RGB8                     = 0x8051,
    GL_RGBA8                    = 0x8058,
    GL_RGB5_A1                  = 0x8057,
    GL_RGBA4                    = 0x8056,
    GL_RGB565                   = 0x8D62,
    GL_COMPRESSED_RGBA_ASTC_4x4_KHR       = 0x93B0,
    GL_COMPRESSED_RGBA_ASTC_12x12_KHR     = 0x93BD,
    GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4   = 0x93D0,
    GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12 = 0x93DD,
};

// ASTC block sizes (index = GL_COMPRESSED_RGBA_ASTC_4x4_KHR + index)
static const uint8_t ASTC_BLOCKS[14][2] = {
    {4,4}, {5,4}, {5,5}, {6,5}, {6,6}, {8,5}, {8,6}, {8,8},
    {10,5}, {10,6}, {10,8}, {10,10}, {12,10}, {12,12}
};

struct MipSlice {
    uint32_t width, height;
    uint32_t dataOffset;    // from start of .btx buffer (including 4-byte rfmt)
    uint32_t byteSize;      // payload size for glCompressedTexImage2D / glTexImage2D
    uint8_t unpackAlignment; // 1 or 4
};

struct BtxInfo {
    uint32_t rfmt;              // 4-byte BR header
    uint32_t width, height;
    uint32_t glInternalFormat;  // original from KTX header
    uint32_t glFormat;          // GL_RGBA / GL_RGB
    uint32_t glType;            // 0 for compressed, GL_UNSIGNED_* for uncompressed
    uint8_t  bpp;               // bytes per pixel for uncompressed (2,3,4); 0 for ASTC
    uint8_t  blockW, blockH;    // ASTC block size (4..12) or 1 for uncompressed
    bool     isCompressed;
    bool     isAstc;
    bool     headerWasSrgb;     // true if KTX header had SRGB format
    bool     hasAlpha;
    std::vector<MipSlice> mips;
};

static inline uint32_t rd32le(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static inline bool isBtx(const uint8_t* data, size_t size) {
    return size >= 4 + 64 && std::memcmp(data + 4, KTX11_MAGIC, 12) == 0;
}

// Detect ASTC block from mip0 size (BR often writes 0x93B4 but data is 4x4)
static inline bool detectAstcBlock(uint32_t w, uint32_t h, uint32_t mip0Size, uint8_t& outW, uint8_t& outH) {
    for (int i = 0; i < 14; i++) {
        uint32_t bw = ASTC_BLOCKS[i][0], bh = ASTC_BLOCKS[i][1];
        uint64_t need = (uint64_t)((w + bw - 1) / bw) * ((h + bh - 1) / bh) * 16;
        if (need == mip0Size) { outW = bw; outH = bh; return true; }
    }
    return false;
}

// Parse .btx header only (for size estimation)
static inline bool peekBtx(const uint8_t* data, size_t size, uint32_t& w, uint32_t& h, uint32_t& nmip) {
    if (!isBtx(data, size)) return false;
    const uint8_t* k = data + 4;
    w = rd32le(k + 36);
    h = rd32le(k + 40);
    nmip = rd32le(k + 56);
    if (!nmip) nmip = 1;
    return w && h && w <= 8192 && h <= 8192 && nmip <= 16;
}

// Full GPU-info parse (no CPU decompression)
static inline bool parseBtxGpu(const uint8_t* data, size_t size, BtxInfo& out, std::string* err = nullptr) {
    out = BtxInfo();
    if (!isBtx(data, size)) { if (err) *err = "not a BTX/KTX11 container"; return false; }

    out.rfmt = rd32le(data);
    const uint8_t* k = data + 4;
    const size_t kn = size - 4;

    if (rd32le(k + 12) != 0x04030201u) { if (err) *err = "unsupported big-endian KTX11"; return false; }

    out.glType           = rd32le(k + 16);
    out.glFormat         = rd32le(k + 24);
    out.glInternalFormat = rd32le(k + 28);
    out.width            = rd32le(k + 36);
    out.height           = rd32le(k + 40);
    uint32_t nmip        = rd32le(k + 56);
    uint32_t kvBytes     = rd32le(k + 60);

    if (out.width == 0 || out.height == 0 || out.width > 8192 || out.height > 8192 || nmip > 16 || 64ull + kvBytes > kn) {
        if (err) *err = "invalid KTX11 dimensions or key-value size"; return false;
    }
    if (nmip == 0) nmip = 1;

    size_t off = 64u + kvBytes;

    // Uncompressed formats
    if (out.glType != 0) {
        if (out.glType == GL_UNSIGNED_BYTE) {
            bool isRgb = (out.glFormat == GL_RGB || out.glInternalFormat == GL_RGB8 || out.glInternalFormat == GL_RGB);
            out.glInternalFormat = isRgb ? GL_RGB8 : GL_RGBA8;
            out.glFormat = isRgb ? GL_RGB : GL_RGBA;
            out.glType = GL_UNSIGNED_BYTE;
            out.bpp = isRgb ? 3 : 4;
        } else if (out.glType == GL_UNSIGNED_SHORT_4_4_4_4) {
            out.glInternalFormat = GL_RGBA4; out.glFormat = GL_RGBA; out.glType = GL_UNSIGNED_SHORT_4_4_4_4; out.bpp = 2;
        } else if (out.glType == GL_UNSIGNED_SHORT_5_6_5) {
            out.glInternalFormat = GL_RGB565; out.glFormat = GL_RGB; out.glType = GL_UNSIGNED_SHORT_5_6_5; out.bpp = 2;
        } else if (out.glType == GL_UNSIGNED_SHORT_5_5_5_1) {
            out.glInternalFormat = GL_RGB5_A1; out.glFormat = GL_RGBA; out.glType = GL_UNSIGNED_SHORT_5_5_5_1; out.bpp = 2;
        } else { if (err) *err = "unsupported uncompressed glType"; return false; }

        out.blockW = out.blockH = 1;
        out.isCompressed = false; out.isAstc = false; out.headerWasSrgb = false;

        uint32_t mw = out.width, mh = out.height;
        for (uint32_t m = 0; m < nmip; m++) {
            if (off + 4 > kn) break;
            uint32_t sz = rd32le(k + off); off += 4;
            if (off + sz > kn) break;
            uint64_t rowBytes = (uint64_t)mw * out.bpp;
            uint64_t rowPitch4 = (rowBytes + 3ull) & ~3ull;
            uint8_t align = 4;
            uint64_t needed = rowPitch4 * mh;
            if (sz < needed) { if (sz < rowBytes * mh) break; align = 1; needed = rowBytes * mh; }
            out.mips.push_back({ mw, mh, (uint32_t)(4u + off), (uint32_t)needed, align });
            off += sz; off = (off + 3u) & ~3u;
            mw = mw > 1 ? mw / 2 : 1; mh = mh > 1 ? mh / 2 : 1;
        }
        if (out.mips.empty()) { if (err) *err = "no valid uncompressed mip levels"; return false; }
        // Alpha probe on mip0
        out.hasAlpha = false;
        if (out.glFormat == GL_RGBA && out.bpp == 4) {
            const MipSlice& m0 = out.mips[0];
            uint32_t stepY = m0.height > 32 ? m0.height / 32 : 1;
            uint32_t stepX = m0.width  > 32 ? m0.width  / 32 : 1;
            for (uint32_t y = 0; y < m0.height; y += stepY) {
                const uint8_t* row = data + m0.dataOffset + (size_t)y * ((m0.width * 4 + 3) & ~3u);
                for (uint32_t x = 0; x < m0.width; x += stepX) if (row[x * 4 + 3] < 255) { out.hasAlpha = true; break; }
            }
        }
        return true;
    }

    // Compressed ASTC
    if (off + 4 > kn) { if (err) *err = "truncated before mip0"; return false; }
    uint32_t sz0 = rd32le(k + off);
    uint8_t bw = 0, bh = 0;
    if (!detectAstcBlock(out.width, out.height, sz0, bw, bh)) { if (err) *err = "unable to match ASTC block footprint"; return false; }

    out.glInternalFormat = GL_COMPRESSED_RGBA_ASTC_4x4_KHR + (bw == 4 && bh == 4 ? 0 :
                        bw == 5 && bh == 4 ? 1 :
                        bw == 5 && bh == 5 ? 2 :
                        bw == 6 && bh == 5 ? 3 :
                        bw == 6 && bh == 6 ? 4 :
                        bw == 8 && bh == 5 ? 5 :
                        bw == 8 && bh == 6 ? 6 :
                        bw == 8 && bh == 8 ? 7 :
                        bw == 10 && bh == 5 ? 8 :
                        bw == 10 && bh == 6 ? 9 :
                        bw == 10 && bh == 8 ? 10 :
                        bw == 10 && bh == 10 ? 11 :
                        bw == 12 && bh == 10 ? 12 : 13);
    out.glFormat = GL_RGBA; out.glType = 0; out.bpp = 0;
    out.blockW = bw; out.blockH = bh;
    out.isCompressed = true; out.isAstc = true;
    out.headerWasSrgb = (out.glInternalFormat >= GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4 &&
                         out.glInternalFormat <= GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12);

    uint32_t mw = out.width, mh = out.height;
    for (uint32_t m = 0; m < nmip; m++) {
        if (off + 4 > kn) break;
        uint32_t sz = rd32le(k + off); off += 4;
        if (off + sz > kn) break;
        uint64_t need = (uint64_t)((mw + bw - 1) / bw) * ((mh + bh - 1) / bh) * 16;
        if (sz < need) break;
        out.mips.push_back({ mw, mh, (uint32_t)(4u + off), (uint32_t)need, 1 });
        off += sz; off = (off + 3u) & ~3u;
        mw = mw > 1 ? mw / 2 : 1; mh = mh > 1 ? mh / 2 : 1;
    }
    if (out.mips.empty()) { if (err) *err = "no valid ASTC mip levels"; return false; }

    // Alpha probe (sample tiny mip + sparse grid on mip0)
    out.hasAlpha = false;
    // 1. Tiny mip (last)
    const MipSlice& tiny = out.mips.back();
    uint32_t tbx = (tiny.width + bw - 1) / bw, tby = (tiny.height + bh - 1) / bh;
    // Would need ASTC decompress here — skip for GPU-only parse
    // 2. Sparse grid on mip0 (placeholder — real probe in BpcPipeline)
    return true;
}

// CPU decode .btx → RGBA mips (for TXD building)
struct Texture {
    std::string name;
    uint32_t flags = 0;
    uint32_t glInternal = 0;
    int blockW = 0, blockH = 0;
    bool srgb = false;
    struct Mip { uint32_t w, h; std::vector<uint8_t> rgba; };
    std::vector<Mip> mips;
    bool hasAlpha = false;
};

static inline bool parseBtxCpu(const uint8_t* data, size_t size, Texture& tex, std::string* err = nullptr) {
    if (!isBtx(data, size)) { if (err) *err = "not BTX"; return false; }
    tex.flags = rd32le(data);
    const uint8_t* k = data + 4; size_t kn = size - 4;
    tex.glInternal = rd32le(k + 28);
    uint32_t w = rd32le(k + 36), h = rd32le(k + 40), nmip = rd32le(k + 56), kv = rd32le(k + 60);
    if (!w || !h || w > 8192 || h > 8192 || nmip > 16) { if (err) *err = "bad dimensions"; return false; }
    if (nmip == 0) nmip = 1;

    uint32_t glType = rd32le(k + 16), glFormat = rd32le(k + 24);
    enum { RAW_NONE, RAW_RGBA8, RAW_RGB8, RAW_4444, RAW_565, RAW_5551 } raw = RAW_NONE;
    if (glType == GL_UNSIGNED_BYTE) raw = (glFormat == GL_RGB || tex.glInternal == GL_RGB8 || tex.glInternal == GL_RGB) ? RAW_RGB8 : RAW_RGBA8;
    else if (glType == GL_UNSIGNED_SHORT_4_4_4_4) raw = RAW_4444;
    else if (glType == GL_UNSIGNED_SHORT_5_6_5) raw = RAW_565;
    else if (glType == GL_UNSIGNED_SHORT_5_5_5_1) raw = RAW_5551;

    size_t off = 64 + kv;
    if (raw != RAW_NONE) {
        uint32_t bpp = raw == RAW_RGBA8 ? 4 : raw == RAW_RGB8 ? 3 : 2;
        uint32_t mw = w, mh = h;
        for (uint32_t m = 0; m < nmip; m++) {
            if (off + 4 > kn) break;
            uint32_t sz = rd32le(k + off); off += 4;
            uint32_t rowBytes = mw * bpp, rowPitch = (rowBytes + 3) & ~3u;
            if (off + sz > kn) break;
            if (sz < (uint64_t)rowPitch * mh) { if (sz < (uint64_t)rowBytes * mh) break; rowPitch = rowBytes; }
            Texture::Mip mip; mip.w = mw; mip.h = mh; mip.rgba.resize((size_t)mw * mh * 4);
            for (uint32_t y = 0; y < mh; y++) {
                const uint8_t* src = k + off + (size_t)y * rowPitch; uint8_t* dst = &mip.rgba[(size_t)y * mw * 4];
                if (raw == RAW_RGBA8) std::memcpy(dst, src, (size_t)mw * 4);
                else if (raw == RAW_RGB8) for (uint32_t x = 0; x < mw; x++) { dst[x*4]=src[x*3]; dst[x*4+1]=src[x*3+1]; dst[x*4+2]=src[x*3+2]; dst[x*4+3]=0xFF; }
                else if (raw == RAW_4444) for (uint32_t x = 0; x < mw; x++) { uint16_t v = (uint16_t)(src[x*2]|(src[x*2+1]<<8)); dst[x*4]=(v>>12&15)*17; dst[x*4+1]=(v>>8&15)*17; dst[x*4+2]=(v>>4&15)*17; dst[x*4+3]=(v&15)*17; }
                else if (raw == RAW_565) for (uint32_t x = 0; x < mw; x++) { uint16_t v = (uint16_t)(src[x*2]|(src[x*2+1]<<8)); uint32_t r=(v>>11)&31,g=(v>>5)&63,b=v&31; dst[x*4]=(r<<3)|(r>>2); dst[x*4+1]=(g<<2)|(g>>4); dst[x*4+2]=(b<<3)|(b>>2); dst[x*4+3]=0xFF; }
                else if (raw == RAW_5551) for (uint32_t x = 0; x < mw; x++) { uint16_t v = (uint16_t)(src[x*2]|(src[x*2+1]<<8)); uint32_t r=(v>>11)&31,g=(v>>6)&31,b=(v>>1)&31; dst[x*4]=(r<<3)|(r>>2); dst[x*4+1]=(g<<3)|(g>>2); dst[x*4+2]=(b<<3)|(b>>2); dst[x*4+3]=(v&1)?0xFF:0; }
            }
            tex.mips.push_back(std::move(mip));
            off += sz; off = (off + 3) & ~3u;
            mw = std::max(1u, mw / 2); mh = std::max(1u, mh / 2);
        }
        if (tex.mips.empty()) { if (err) *err = "no mips decoded"; return false; }
        tex.blockW = tex.blockH = 1;
        const Texture::Mip& m0 = tex.mips[0];
        for (size_t i = 3; i < m0.rgba.size(); i += 4) if (m0.rgba[i] != 0xFF) { tex.hasAlpha = true; break; }
        return true;
    }

    // ASTC decode (requires basisu::astc::decompress from third_party)
    // This is a stub — full implementation needs astc_decomp.h
    if (err) *err = "ASTC CPU decode requires basisu::astc::decompress";
    return false;
}

// POT resampling for D3D9 compatibility
static inline uint32_t potDim(uint32_t v) {
    if (v < 4) return 4;
    uint32_t lo = 1; while (lo * 2 <= v) lo *= 2;
    uint32_t hi = lo * 2; if (lo == v) return v;
    return ((double)v / lo < (double)hi / v) ? lo : hi;
}
static inline bool isPot(uint32_t v) { return v && (v & (v - 1)) == 0; }

static inline Texture::Mip resampleMip(const Texture::Mip& src, uint32_t nw, uint32_t nh) {
    Texture::Mip d; d.w = nw; d.h = nh; d.rgba.resize((size_t)nw * nh * 4);
    for (uint32_t y = 0; y < nh; y++) {
        float fy = (y + 0.5f) * src.h / nh - 0.5f; if (fy < 0) fy = 0;
        uint32_t y0 = (uint32_t)fy, y1 = std::min(y0 + 1, src.h - 1); float wy = fy - y0;
        for (uint32_t x = 0; x < nw; x++) {
            float fx = (x + 0.5f) * src.w / nw - 0.5f; if (fx < 0) fx = 0;
            uint32_t x0 = (uint32_t)fx, x1 = std::min(x0 + 1, src.w - 1); float wx = fx - x0;
            const uint8_t* p00 = &src.rgba[((size_t)y0 * src.w + x0) * 4];
            const uint8_t* p01 = &src.rgba[((size_t)y0 * src.w + x1) * 4];
            const uint8_t* p10 = &src.rgba[((size_t)y1 * src.w + x0) * 4];
            const uint8_t* p11 = &src.rgba[((size_t)y1 * src.w + x1) * 4];
            uint8_t* o = &d.rgba[((size_t)y * nw + x) * 4];
            for (int c = 0; c < 4; c++) { float v = (p00[c]*(1-wx)+p01[c]*wx)*(1-wy)+(p10[c]*(1-wx)+p11[c]*wx)*wy; o[c]=(uint8_t)(v+0.5f); }
        }
    }
    return d;
}
static inline Texture::Mip halveMip(const Texture::Mip& s) {
    Texture::Mip d; d.w = std::max(1u, s.w/2); d.h = std::max(1u, s.h/2); d.rgba.resize((size_t)d.w*d.h*4);
    for (uint32_t y = 0; y < d.h; y++) for (uint32_t x = 0; x < d.w; x++) {
        uint32_t sx = std::min(x*2, s.w-1), sy = std::min(y*2, s.h-1), sx1 = std::min(sx+1, s.w-1), sy1 = std::min(sy+1, s.h-1);
        const uint8_t* a = &s.rgba[((size_t)sy*s.w+sx)*4], *b = &s.rgba[((size_t)sy*s.w+sx1)*4];
        const uint8_t* c = &s.rgba[((size_t)sy1*s.w+sx)*4], *e = &s.rgba[((size_t)sy1*s.w+sx1)*4];
        uint8_t* o = &d.rgba[((size_t)y*d.w+x)*4];
        for (int k = 0; k < 4; k++) o[k] = (a[k]+b[k]+c[k]+e[k]+2)/4;
    }
    return d;
}

static inline bool makePot(Texture& tex) {
    if (tex.mips.empty()) return false;
    const Texture::Mip& m0 = tex.mips[0];
    if (isPot(m0.w) && isPot(m0.h)) return false;
    uint32_t nw = potDim(m0.w), nh = potDim(m0.h);
    size_t levels = tex.mips.size();
    std::vector<Texture::Mip> out; out.push_back(resampleMip(m0, nw, nh));
    while (out.size() < std::max<size_t>(levels, 1) && (out.back().w > 4 || out.back().h > 4)) out.push_back(halveMip(out.back()));
    tex.mips.swap(out);
    return true;
}

} // namespace brbtx