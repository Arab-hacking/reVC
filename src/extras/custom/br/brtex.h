// brtex.h — текстуры Black Russia (.btx = u32 flags + KTX11/ASTC) -> RenderWare TXD (SA, D3D9 DXT1/DXT5)
#pragma once
#include <cstdint>
#include <cstring>
#include <vector>
#include <functional>
#include <cmath>
#include <string>
#include <algorithm>
#include "brformats.h"
#include "third_party/astc_decomp.h"
#include "third_party/stb_dxt.h"

namespace brtex {

static inline uint32_t rd32(const uint8_t* p) { uint32_t v; memcpy(&v, p, 4); return v; }
static inline void put32(std::vector<uint8_t>& o, uint32_t v) { o.insert(o.end(), (uint8_t*)&v, (uint8_t*)&v + 4); }
static inline void put16(std::vector<uint8_t>& o, uint16_t v) { o.insert(o.end(), (uint8_t*)&v, (uint8_t*)&v + 2); }

struct Mip { uint32_t w, h; std::vector<uint8_t> rgba; };
struct Texture {
    std::string name;            // имя текстуры (без расширения)
    uint32_t flags = 0;          // u32 из заголовка .btx (2 = ?, у части файлов)
    uint32_t glInternal = 0;     // 0x93B4 = COMPRESSED_SRGB8_ALPHA8_ASTC_4x4, 0x93B0 = RGBA_ASTC_4x4 ...
    int blockW = 0, blockH = 0;
    bool srgb = false;
    std::vector<Mip> mips;
    bool hasAlpha = false;
};

static inline bool isBtx(const uint8_t* p, size_t n) {
    static const uint8_t KTX1[12] = { 0xAB,'K','T','X',' ','1','1',0xBB,'\r','\n',0x1A,'\n' };
    return n >= 4 + 64 && memcmp(p + 4, KTX1, 12) == 0;
}

// GL enum ASTC -> размер блока / sRGB
static inline bool astcFormat(uint32_t gl, int& bw, int& bh, bool& srgb) {
    static const struct { uint32_t base; int w, h; } tbl[] = {
        {0x93B0,4,4},{0x93B1,5,4},{0x93B2,5,5},{0x93B3,6,5},{0x93B4,6,6},{0x93B5,8,5},{0x93B6,8,6},{0x93B7,8,8},
        {0x93B8,10,5},{0x93B9,10,6},{0x93BA,10,8},{0x93BB,10,10},{0x93BC,12,10},{0x93BD,12,12} };
    // ВНИМАНИЕ: BR пишет 0x93B4 но данные 4x4 (см. проверку по размеру мипа ниже) — определяем блок по размеру данных
    for (auto& t : tbl) { if (gl == t.base) { bw = t.w; bh = t.h; srgb = false; return true; } if (gl == t.base + 0x20) { bw = t.w; bh = t.h; srgb = true; return true; } }
    return false;
}

// Разбор .btx. Блок ASTC определяется по фактическому размеру первого мипа (у BR enum не совпадает с данными).
static inline bool parseBtx(const uint8_t* p, size_t n, Texture& t, std::string* err = nullptr) {
    if (!isBtx(p, n)) { if (err) *err = "not a BTX/KTX11"; return false; }
    t.flags = rd32(p);
    const uint8_t* k = p + 4; size_t kn = n - 4;
    uint32_t endian = rd32(k + 12); if (endian != 0x04030201) { if (err) *err = "big-endian KTX"; return false; }
    t.glInternal = rd32(k + 28);
    uint32_t w = rd32(k + 36), h = rd32(k + 40), nmip = rd32(k + 56), kv = rd32(k + 60);
    if (!w || !h || w > 8192 || h > 8192 || nmip > 16) { if (err) *err = "bad dimensions"; return false; }
    if (nmip == 0) nmip = 1;
    int bw = 4, bh = 4; bool srgb = true;
    size_t off = 64 + kv;
    uint32_t glType = rd32(k + 16), glFormat = rd32(k + 24);
    // Несжатые форматы (встречаются у BR: RGBA8, RGB8, RGBA4444, RGB565, RGBA5551)
    enum { RAW_NONE, RAW_RGBA8, RAW_RGB8, RAW_4444, RAW_565, RAW_5551 } raw = RAW_NONE;
    if (glType == 0x1401 /*UNSIGNED_BYTE*/) raw = (glFormat == 0x1907 /*GL_RGB*/ || t.glInternal == 0x8051 || t.glInternal == 0x1907) ? RAW_RGB8 : RAW_RGBA8;
    else if (glType == 0x8033 /*UNSIGNED_SHORT_4_4_4_4*/) raw = RAW_4444;
    else if (glType == 0x8363 /*UNSIGNED_SHORT_5_6_5*/ || glType == 0x8364 /*5_6_5_REV*/) raw = RAW_565;
    else if (glType == 0x8034 /*UNSIGNED_SHORT_5_5_5_1*/) raw = RAW_5551;
    if (raw != RAW_NONE) {
        uint32_t bpp = raw == RAW_RGBA8 ? 4 : raw == RAW_RGB8 ? 3 : 2;
        uint32_t mw = w, mh = h;
        for (uint32_t m = 0; m < nmip; m++) {
            if (off + 4 > kn) break;
            uint32_t sz = rd32(k + off); off += 4;
            uint32_t rowBytes = mw * bpp, rowPitch = (rowBytes + 3) & ~3u;   // KTX: строки выровнены на 4 (GL_UNPACK_ALIGNMENT)
            if (off + sz > kn) break;
            if (sz < (uint64_t)rowPitch * mh) { if (sz < (uint64_t)rowBytes * mh) break; rowPitch = rowBytes; }   // без выравнивания строк
            Mip mip; mip.w = mw; mip.h = mh; mip.rgba.resize((size_t)mw * mh * 4);
            for (uint32_t y = 0; y < mh; y++) {
                const uint8_t* src = k + off + (size_t)y * rowPitch; uint8_t* dst = &mip.rgba[(size_t)y * mw * 4];
                switch (raw) {
                case RAW_RGBA8: memcpy(dst, src, (size_t)mw * 4); break;
                case RAW_RGB8:  for (uint32_t x = 0; x < mw; x++) { dst[x * 4] = src[x * 3]; dst[x * 4 + 1] = src[x * 3 + 1]; dst[x * 4 + 2] = src[x * 3 + 2]; dst[x * 4 + 3] = 0xFF; } break;
                case RAW_4444:  for (uint32_t x = 0; x < mw; x++) { uint16_t v = (uint16_t)(src[x * 2] | (src[x * 2 + 1] << 8));
                                    dst[x * 4] = (uint8_t)(((v >> 12) & 15) * 17); dst[x * 4 + 1] = (uint8_t)(((v >> 8) & 15) * 17); dst[x * 4 + 2] = (uint8_t)(((v >> 4) & 15) * 17); dst[x * 4 + 3] = (uint8_t)((v & 15) * 17); } break;
                case RAW_565:   for (uint32_t x = 0; x < mw; x++) { uint16_t v = (uint16_t)(src[x * 2] | (src[x * 2 + 1] << 8));
                                    uint32_t r = (v >> 11) & 31, g = (v >> 5) & 63, b = v & 31;
                                    dst[x * 4] = (uint8_t)((r << 3) | (r >> 2)); dst[x * 4 + 1] = (uint8_t)((g << 2) | (g >> 4)); dst[x * 4 + 2] = (uint8_t)((b << 3) | (b >> 2)); dst[x * 4 + 3] = 0xFF; } break;
                case RAW_5551:  for (uint32_t x = 0; x < mw; x++) { uint16_t v = (uint16_t)(src[x * 2] | (src[x * 2 + 1] << 8));
                                    uint32_t r = (v >> 11) & 31, g = (v >> 6) & 31, b = (v >> 1) & 31;
                                    dst[x * 4] = (uint8_t)((r << 3) | (r >> 2)); dst[x * 4 + 1] = (uint8_t)((g << 3) | (g >> 2)); dst[x * 4 + 2] = (uint8_t)((b << 3) | (b >> 2)); dst[x * 4 + 3] = (v & 1) ? 0xFF : 0; } break;
                default: break;
                }
            }
            t.mips.push_back(std::move(mip));
            off += sz; off = (off + 3) & ~(size_t)3;
            mw = std::max(1u, mw / 2); mh = std::max(1u, mh / 2);
        }
        if (t.mips.empty()) { if (err) { char tb[160]; snprintf(tb, sizeof tb, "no mip levels (raw glType=0x%X glFormat=0x%X glInternal=0x%X %ux%u)", glType, glFormat, t.glInternal, w, h); *err = tb; } return false; }
        t.blockW = t.blockH = 1;
        const Mip& m0 = t.mips[0];
        for (size_t i = 3; i < m0.rgba.size(); i += 4) if (m0.rgba[i] != 0xFF) { t.hasAlpha = true; break; }
        return true;
    }
    astcFormat(t.glInternal, bw, bh, srgb);
    // первый мип: подбираем блок по размеру
    if (off + 4 > kn) { if (err) *err = "truncated"; return false; }
    uint32_t sz0 = rd32(k + off);
    {
        static const int bs[][2] = { {4,4},{5,4},{5,5},{6,5},{6,6},{8,5},{8,6},{8,8},{10,5},{10,6},{10,8},{10,10},{12,10},{12,12} };
        bool found = false;
        for (auto& b : bs) { uint32_t need = ((w + b[0] - 1) / b[0]) * ((h + b[1] - 1) / b[1]) * 16; if (need == sz0) { bw = b[0]; bh = b[1]; found = true; break; } }
        if (!found) { if (err) { char tb[160]; snprintf(tb, sizeof tb, "unsupported format: glType=0x%X glFormat=0x%X glInternal=0x%X %ux%u mip0=%u bytes", rd32(k + 16), rd32(k + 24), t.glInternal, w, h, sz0); *err = tb; } return false; }
    }
    t.blockW = bw; t.blockH = bh; t.srgb = srgb;
    uint32_t mw = w, mh = h;
    for (uint32_t m = 0; m < nmip; m++) {
        if (off + 4 > kn) break;
        uint32_t sz = rd32(k + off); off += 4;
        if (off + sz > kn) break;
        uint32_t bx = (mw + bw - 1) / bw, by = (mh + bh - 1) / bh;
        if (sz < bx * by * 16) break;
        Mip mip; mip.w = mw; mip.h = mh; mip.rgba.assign((size_t)mw * mh * 4, 0);
        std::vector<uint8_t> blk(bw * bh * 4);
        for (uint32_t y = 0; y < by; y++) for (uint32_t x = 0; x < bx; x++) {
            const uint8_t* src = k + off + (y * bx + x) * 16;
            if (!basisu::astc::decompress(blk.data(), src, srgb, bw, bh)) memset(blk.data(), 0xFF, blk.size());
            for (int yy = 0; yy < bh; yy++) { uint32_t py = y * bh + yy; if (py >= mh) break;
                for (int xx = 0; xx < bw; xx++) { uint32_t px = x * bw + xx; if (px >= mw) break;
                    memcpy(&mip.rgba[((size_t)py * mw + px) * 4], &blk[(yy * bw + xx) * 4], 4); } }
        }
        t.mips.push_back(std::move(mip));
        off += sz; off = (off + 3) & ~(size_t)3;
        mw = std::max(1u, mw / 2); mh = std::max(1u, mh / 2);
    }
    if (t.mips.empty()) { if (err) *err = "no mip levels decoded"; return false; }
    // альфа?
    const Mip& m0 = t.mips[0];
    for (size_t i = 3; i < m0.rgba.size(); i += 4) if (m0.rgba[i] != 0xFF) { t.hasAlpha = true; break; }
    return true;
}

// ---------------------------------------------------------------------------
// RW TXD (SA / D3D9). Текстуры DXT1 (без альфы) / DXT5 (с альфой), мипы включаются, размеры >= 4 и кратные 4 —
// иначе (мелкие хвосты мип-цепочки) обрезаем цепочку.
// ---------------------------------------------------------------------------
static const uint32_t SA_LIBID = 0x1803FFFF;
enum { rwSTRUCT = 1, rwEXTENSION = 3, rwTEXDICT = 0x16, rwTEXNATIVE = 0x15 };

static inline void chunk(std::vector<uint8_t>& out, uint32_t id, const std::vector<uint8_t>& body) {
    put32(out, id); put32(out, (uint32_t)body.size()); put32(out, SA_LIBID); out.insert(out.end(), body.begin(), body.end());
}

static inline void encodeDxt(const Mip& m, bool dxt5, std::vector<uint8_t>& out) {
    uint32_t bx = (m.w + 3) / 4, by = (m.h + 3) / 4;
    uint8_t px[64];
    for (uint32_t y = 0; y < by; y++) for (uint32_t x = 0; x < bx; x++) {
        for (int yy = 0; yy < 4; yy++) for (int xx = 0; xx < 4; xx++) {
            uint32_t sx = std::min(x * 4 + xx, m.w - 1), sy = std::min(y * 4 + yy, m.h - 1);
            memcpy(&px[(yy * 4 + xx) * 4], &m.rgba[((size_t)sy * m.w + sx) * 4], 4);
        }
        uint8_t blk[16]; stb_compress_dxt_block(blk, px, dxt5 ? 1 : 0, STB_DXT_NORMAL);
        out.insert(out.end(), blk, blk + (dxt5 ? 16 : 8));
    }
}

// v6.4.3: D3D9/RW SA принимает только текстуры со сторонами-степенями двойки (и DXT требует кратности 4).
// У BR много NPOT-текстур (255x255, 500x541, 1254x1254...) -> RwTexDictionaryStreamRead падал, TXD отвергался,
// модель вечно перезапрашивалась. Пересэмплируем в ближайшую степень двойки и строим мип-цепочку заново.
static inline uint32_t potDim(uint32_t v) {
    if (v < 4) return 4;
    uint32_t lo = 1; while (lo * 2 <= v) lo *= 2;
    uint32_t hi = lo * 2; if (lo == v) return v;
    return ((double)v / lo < (double)hi / v) ? lo : hi;   // ближайшая в логарифмическом смысле
}
static inline bool isPot(uint32_t v) { return v && (v & (v - 1)) == 0; }
static inline Mip resampleMip(const Mip& src, uint32_t nw, uint32_t nh) {
    Mip d; d.w = nw; d.h = nh; d.rgba.resize((size_t)nw * nh * 4);
    for (uint32_t y = 0; y < nh; y++) {
        float fy = (y + 0.5f) * src.h / nh - 0.5f; if (fy < 0) fy = 0; uint32_t y0 = (uint32_t)fy; uint32_t y1 = std::min(y0 + 1, src.h - 1); float wy = fy - y0;
        for (uint32_t x = 0; x < nw; x++) {
            float fx = (x + 0.5f) * src.w / nw - 0.5f; if (fx < 0) fx = 0; uint32_t x0 = (uint32_t)fx; uint32_t x1 = std::min(x0 + 1, src.w - 1); float wx = fx - x0;
            const uint8_t* p00 = &src.rgba[((size_t)y0 * src.w + x0) * 4]; const uint8_t* p01 = &src.rgba[((size_t)y0 * src.w + x1) * 4];
            const uint8_t* p10 = &src.rgba[((size_t)y1 * src.w + x0) * 4]; const uint8_t* p11 = &src.rgba[((size_t)y1 * src.w + x1) * 4];
            uint8_t* o = &d.rgba[((size_t)y * nw + x) * 4];
            for (int c = 0; c < 4; c++) { float v = (p00[c] * (1 - wx) + p01[c] * wx) * (1 - wy) + (p10[c] * (1 - wx) + p11[c] * wx) * wy; o[c] = (uint8_t)(v + 0.5f); }
        }
    }
    return d;
}
static inline Mip halveMip(const Mip& s) {
    Mip d; d.w = std::max(1u, s.w / 2); d.h = std::max(1u, s.h / 2); d.rgba.resize((size_t)d.w * d.h * 4);
    for (uint32_t y = 0; y < d.h; y++) for (uint32_t x = 0; x < d.w; x++) {
        uint32_t sx = std::min(x * 2, s.w - 1), sy = std::min(y * 2, s.h - 1), sx1 = std::min(sx + 1, s.w - 1), sy1 = std::min(sy + 1, s.h - 1);
        const uint8_t* a = &s.rgba[((size_t)sy * s.w + sx) * 4]; const uint8_t* b = &s.rgba[((size_t)sy * s.w + sx1) * 4];
        const uint8_t* c = &s.rgba[((size_t)sy1 * s.w + sx) * 4]; const uint8_t* e = &s.rgba[((size_t)sy1 * s.w + sx1) * 4];
        uint8_t* o = &d.rgba[((size_t)y * d.w + x) * 4];
        for (int k = 0; k < 4; k++) o[k] = (uint8_t)((a[k] + b[k] + c[k] + e[k] + 2) / 4);
    }
    return d;
}
// Приводит текстуру к POT: true, если пришлось менять
static inline bool makePot(Texture& t) {
    if (t.mips.empty()) return false;
    const Mip& m0 = t.mips[0];
    if (isPot(m0.w) && isPot(m0.h)) return false;
    uint32_t nw = potDim(m0.w), nh = potDim(m0.h);
    size_t levels = t.mips.size();
    std::vector<Mip> out; out.push_back(resampleMip(m0, nw, nh));
    while (out.size() < std::max<size_t>(levels, 1) && (out.back().w > 4 || out.back().h > 4)) out.push_back(halveMip(out.back()));
    t.mips.swap(out);
    return true;
}

// Одна текстура -> тело Texture Native (D3D9)
static inline std::vector<uint8_t> buildTexNative(const Texture& tIn, bool useDxt) {
    Texture tmp; const Texture* pt = &tIn;
    if (!tIn.mips.empty() && !(isPot(tIn.mips[0].w) && isPot(tIn.mips[0].h))) { tmp = tIn; makePot(tmp); pt = &tmp; }
    const Texture& t = *pt;
    std::vector<uint8_t> s;
    bool dxt5 = t.hasAlpha;
    uint32_t nm = 0; for (auto& m : t.mips) { if (useDxt && (m.w < 4 || m.h < 4)) break; nm++; }
    if (!nm) nm = 1;
    put32(s, 9);                                   // platform D3D9
    // filter/addressing: filter = mip-linear (0x06) если есть мипы, else linear 0x02; wrap U/V = 1 (wrap)
    uint32_t filt = nm > 1 ? 0x06 : 0x02; s.push_back((uint8_t)filt); s.push_back(0x11); s.push_back(0); s.push_back(0);
    char name[32] = {0}, mask[32] = {0}; strncpy(name, t.name.c_str(), 31);
    s.insert(s.end(), name, name + 32); s.insert(s.end(), mask, mask + 32);
    uint32_t rasterFormat = 0;
    if (useDxt) rasterFormat = (dxt5 ? 0x0300 : 0x0100) | (nm > 1 ? 0x8000 : 0);   // 0x0100 = 1555 (DXT1), 0x0300 = 8888 (DXT5)
    else        rasterFormat = 0x0500 | (nm > 1 ? 0x8000 : 0);                       // 8888
    put32(s, rasterFormat);
    if (useDxt) { const char* fcc = dxt5 ? "DXT5" : "DXT1"; s.insert(s.end(), fcc, fcc + 4); } else put32(s, 21 /*D3DFMT_A8R8G8B8*/);
    put16(s, (uint16_t)t.mips[0].w); put16(s, (uint16_t)t.mips[0].h);
    s.push_back(useDxt ? (dxt5 ? 32 : 16) : 32);   // depth
    s.push_back((uint8_t)nm);                      // num levels
    s.push_back(4);                                // raster type = texture
    s.push_back(useDxt ? 0x08 | (dxt5 ? 0x01 : 0) : 0x01 /* hasAlpha */);   // flags: 0x08 = compressed, 0x01 = has alpha
    if (useDxt && !dxt5) s.back() = 0x08;
    for (uint32_t i = 0; i < nm; i++) {
        const Mip& m = t.mips[i];
        std::vector<uint8_t> px;
        if (useDxt) encodeDxt(m, dxt5, px);
        else { px.resize(m.rgba.size()); for (size_t j = 0; j < m.rgba.size(); j += 4) { px[j] = m.rgba[j + 2]; px[j + 1] = m.rgba[j + 1]; px[j + 2] = m.rgba[j]; px[j + 3] = m.rgba[j + 3]; } }
        put32(s, (uint32_t)px.size()); s.insert(s.end(), px.begin(), px.end());
    }
    std::vector<uint8_t> body; chunk(body, rwSTRUCT, s); chunk(body, rwEXTENSION, {});
    std::vector<uint8_t> out; chunk(out, rwTEXNATIVE, body);
    return out;
}

// Верхняя оценка размера Texture Native для текстуры w x h с nmip уровнями (DXT5, все уровни >= 4px)
static inline uint64_t estimateTexNativeSize(uint32_t w, uint32_t h, uint32_t nmip, bool useDxt = true) {
    if (!(isPot(w) && isPot(h))) { w = potDim(w); h = potDim(h); if (nmip < 12) nmip = 12; }   // NPOT -> POT, мипы строятся заново
    uint64_t s = 12 + 12 + 88 + 12;
    for (uint32_t i = 0; i < nmip; i++) {
        if (useDxt && (w < 4 || h < 4)) break;
        s += 4 + (useDxt ? (uint64_t)((w + 3) / 4) * ((h + 3) / 4) * 16 : (uint64_t)w * h * 4);
        w = w > 1 ? w / 2 : 1; h = h > 1 ? h / 2 : 1;
    }
    return s;
}
static inline uint64_t estimateTxdSize(const std::vector<uint64_t>& texSizes) { uint64_t s = 12 + 16 + 12; for (auto v : texSizes) s += v; return s; }
// Только заголовок .btx: w/h/nmip без декодирования (нужно >= 68 байт)
static inline bool peekBtx(const uint8_t* p, size_t n, uint32_t& w, uint32_t& h, uint32_t& nmip) {
    if (!isBtx(p, n)) return false;
    const uint8_t* k = p + 4; w = rd32(k + 36); h = rd32(k + 40); nmip = rd32(k + 56); if (!nmip) nmip = 1;
    return w && h && w <= 8192 && h <= 8192 && nmip <= 16;
}

// ---------------------------------------------------------------------------
// pbrTerrain (BR): маска RGBA (веса слоёв) + 4 тайловых слоя -> одна запечённая диффуза на чанк земли (SA не умеет сплаттинг).
//   size — сторона композита (512 по умолчанию), tiles — сколько раз тайл укладывается по стороне чанка (умножается на landscapeScale).
//   Маска без альфы (A=255 везде): вес 4-го слоя = 1 - R - G - B, иначе A берётся из маски; веса нормируются.
// load(name, tex) — загрузка .btx по нормализованному имени. Возвращает false, если нет ни маски, ни одного слоя.
// ---------------------------------------------------------------------------
static inline bool bakeTerrain(const br::TerrainRecipe& r, const std::function<bool(const std::string&, Texture&)>& load, Texture& out, uint32_t size = 512, float tiles = 0.f) {
    Texture mask; bool haveMask = !r.tex.empty() && load(r.tex, mask) && !mask.mips.empty();
    Texture lay[4]; bool have[4] = { false, false, false, false }; int nLay = 0;
    for (int i = 0; i < 4; i++) if (!r.layers[i].empty() && load(r.layers[i], lay[i]) && !lay[i].mips.empty()) { have[i] = true; nLay++; }
    if (!nLay) { if (!haveMask) return false; out = mask; out.name = r.tex; return true; }   // слоёв нет — хоть маску (лучше, чем ничего)
    if (size < 16) size = 16; if (size > 2048) size = 2048;
    // tiles <= 0 -> авто: шейдер BR берёт UV слоёв как мировые XY / 5 м, т.е. на чанк шириной extent приходится extent/5 повторов
    if (tiles <= 0.f) tiles = r.extent > 1.f ? r.extent / 5.f : 8.f;
    if (tiles > size / 4.f) tiles = size / 4.f;
    // слои: уменьшаем до размера одного повтора (size / tiles), чтобы не алиасило
    const Mip* lm[4] = { nullptr, nullptr, nullptr, nullptr }; std::vector<Mip> tmp; tmp.reserve(8);
    for (int i = 0; i < 4; i++) if (have[i]) {
        float rep = tiles * (r.scale[i] > 0.01f ? r.scale[i] : 1.f); uint32_t want = std::max<uint32_t>(4, (uint32_t)(size / rep));
        Mip m = lay[i].mips[0]; while (m.w > want * 2 && m.h > want * 2) m = halveMip(m);   // не ниже want (нужен ещё бильинейный шаг)
        tmp.push_back(std::move(m)); lm[i] = &tmp.back();
    }
    const Mip* mk = haveMask ? &mask.mips[0] : nullptr; bool maskHasAlpha = haveMask && mask.hasAlpha;
    out = Texture(); out.name = r.tex.empty() ? r.layers[0] : r.tex; Mip o; o.w = o.h = size; o.rgba.resize((size_t)size * size * 4);
    auto sampleWrap = [](const Mip& m, float u, float v, float px[4]) {
        u -= floorf(u); v -= floorf(v); float fx = u * m.w - 0.5f, fy = v * m.h - 0.5f;
        int x0 = (int)floorf(fx), y0 = (int)floorf(fy); float wx = fx - x0, wy = fy - y0;
        auto at = [&](int x, int y) { x = ((x % (int)m.w) + (int)m.w) % (int)m.w; y = ((y % (int)m.h) + (int)m.h) % (int)m.h; return &m.rgba[((size_t)y * m.w + x) * 4]; };
        const uint8_t* a = at(x0, y0); const uint8_t* b = at(x0 + 1, y0); const uint8_t* c = at(x0, y0 + 1); const uint8_t* d = at(x0 + 1, y0 + 1);
        for (int k = 0; k < 4; k++) px[k] = (a[k] * (1 - wx) + b[k] * wx) * (1 - wy) + (c[k] * (1 - wx) + d[k] * wx) * wy;
    };
    for (uint32_t y = 0; y < size; y++) for (uint32_t x = 0; x < size; x++) {
        float u = (x + 0.5f) / size, v = (y + 0.5f) / size, w[4] = { 0, 0, 0, 0 };
        if (mk) { float m[4]; sampleWrap(*mk, u, v, m); w[0] = m[0] / 255.f; w[1] = m[1] / 255.f; w[2] = m[2] / 255.f; w[3] = maskHasAlpha ? m[3] / 255.f : std::max(0.f, 1.f - w[0] - w[1] - w[2]); }
        else { w[0] = 1; }
        float sum = 0; for (int i = 0; i < 4; i++) { if (!have[i]) w[i] = 0; sum += w[i]; }
        if (sum <= 1e-4f) { for (int i = 0; i < 4; i++) if (have[i]) { w[i] = 1; sum = 1; break; } }
        float acc[3] = { 0, 0, 0 };
        for (int i = 0; i < 4; i++) if (w[i] > 0) {
            float rep = tiles * (r.scale[i] > 0.01f ? r.scale[i] : 1.f), px[4]; sampleWrap(*lm[i], u * rep, v * rep, px);
            for (int k = 0; k < 3; k++) acc[k] += px[k] * w[i] / sum;
        }
        uint8_t* d = &o.rgba[((size_t)y * size + x) * 4]; for (int k = 0; k < 3; k++) d[k] = (uint8_t)std::min(255.f, acc[k] + 0.5f); d[3] = 255;
    }
    out.mips.push_back(std::move(o));
    while (out.mips.back().w > 4 && out.mips.back().h > 4) out.mips.push_back(halveMip(out.mips.back()));
    out.hasAlpha = false; out.blockW = out.blockH = 1;
    return true;
}

static inline std::vector<uint8_t> buildTxd(const std::vector<Texture>& texs, bool useDxt = true) {
    std::vector<uint8_t> body, st;
    put16(st, (uint16_t)texs.size()); put16(st, 0);   // count, deviceId (0 = любой)
    chunk(body, rwSTRUCT, st);
    for (auto& t : texs) { auto n = buildTexNative(t, useDxt); body.insert(body.end(), n.begin(), n.end()); }
    chunk(body, rwEXTENSION, {});
    std::vector<uint8_t> out; chunk(out, rwTEXDICT, body);
    return out;
}

// ---------------------------------------------------------------------------
// Имена текстур из DFF: обходим дерево, в каждом rwTEXTURE (0x06) две rwSTRING (0x02): name, mask
// ---------------------------------------------------------------------------
static inline void collectTextureNames(const uint8_t* p, size_t n, std::vector<std::string>& out, int depth = 0) {
    size_t off = 0;
    while (off + 12 <= n) {
        uint32_t id = rd32(p + off), sz = rd32(p + off + 4);
        if (sz > n - off - 12) sz = (uint32_t)(n - off - 12);      // терпимо к неверным размерам (BR)
        const uint8_t* body = p + off + 12;
        if (id == 0x06 && sz >= 12 + 12) {
            // struct(4) + string name + string mask
            size_t o = 0; int idx = 0;
            while (o + 12 <= sz) {
                uint32_t cid = rd32(body + o), csz = rd32(body + o + 4);
                if (csz > sz - o - 12) break;
                if (cid == 0x02) {
                    std::string s((const char*)body + o + 12, strnlen((const char*)body + o + 12, csz));
                    if (idx == 0 && !s.empty()) { std::string l = s; for (auto& c : l) c = (char)tolower((unsigned char)c);
                        size_t dot = l.find_last_of('.');
                        if (dot != std::string::npos && dot > 0) { std::string e = l.substr(dot + 1); if (e == "png" || e == "dds" || e == "tga" || e == "bmp" || e == "jpg" || e == "jpeg" || e == "btx") l.resize(dot); }
                        if (std::find(out.begin(), out.end(), l) == out.end()) out.push_back(l); }
                    idx++;
                }
                o += 12 + csz;
            }
        } else if (id == 0x120 && sz >= 4) {
            // текстуры внутри RpMatFX (envmap/bump/dual и BR-PBR тип 10): для типа 10 берём только ту, что станет диффузной
            std::vector<br::MatFxTex> mt; uint32_t type = br::matfxTextures(body, sz, mt);
            const br::MatFxTex* pick = nullptr;
            if (type == 9) { for (auto& t : mt) { if (t.key == "landscapeMask") pick = &t; } }
            if (type == 10) {
                for (auto& t : mt) { if (t.key == "diffuseTex") pick = &t; }
                if (!pick) { for (auto& t : mt) { if (t.key == "albedoTex") pick = &t; } }
                if (!pick) { for (auto& t : mt) { if (t.key == "baseColorTex" || t.key == "colorTex") pick = &t; } }
            }
            for (auto& t : mt) { if ((type == 10 || type == 9) && &t != pick) continue; collectTextureNames(body + t.off, t.len, out, depth + 1); }
        } else if (depth < 32) {
            switch (id) { case 0x03: case 0x07: case 0x08: case 0x0E: case 0x0F: case 0x10: case 0x14: case 0x1A: case 0x2B: case 0x05: case 0x12:
                collectTextureNames(body, sz, out, depth + 1); break; default: break; }
        }
        off += 12 + sz;
    }
}

} // namespace brtex
