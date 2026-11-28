#pragma once

// Black Russia .mod format — TEA decryption + RenderWare stream validation
// Extracted from src/extras/custom/br/brformats.h, cleaned for core module

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <functional>

namespace brmod {

// .mod header
static const uint32_t MOD_MAGIC   = 0xAB921033;
static const uint32_t MOD_HDR     = 0x1C;
static const uint32_t MOD_BLOCK   = 0x800;

// TEA key from libblackrussia-client.so @0x483d0c (raw) -> transformed
static const uint32_t MOD_KEY_RAW[4] = { 0x6ED9EE7A, 0x930C666B, 0x930E166B, 0x4709EE79 };
static const uint32_t MOD_KEY_XOR = 0x12913AFB;

// RenderWare chunk IDs
enum RwChunkId : uint32_t {
    rwSTRUCT        = 0x01,
    rwSTRING        = 0x02,
    rwEXTENSION     = 0x03,
    rwTEXTURE       = 0x06,
    rwMATERIAL      = 0x07,
    rwMATLIST       = 0x08,
    rwFRAMELIST     = 0x0E,
    rwGEOMETRY      = 0x0F,
    rwCLUMP         = 0x10,
    rwLIGHT         = 0x12,
    rwATOMIC        = 0x14,
    rwGEOMLIST      = 0x1A,
    rwUVANIMDICT    = 0x2B,
    rwMATFX         = 0x120,
    rw2DFX          = 0x253F2F9,
};

static const uint32_t SA_LIBID = 0x1803FFFF;  // RW 3.6.0.3

// ---- Low-level utils ----
static inline uint32_t ror32(uint32_t x, int r) { return (x >> r) | (x << (32 - r)); }
static inline uint32_t rd32(const uint8_t* p) { uint32_t v; std::memcpy(&v, p, 4); return v; }
static inline uint16_t rd16(const uint8_t* p) { uint16_t v; std::memcpy(&v, p, 2); return v; }
static inline void wr32(uint8_t* p, uint32_t v) { std::memcpy(p, &v, 4); }
static inline void wr16(uint8_t* p, uint16_t v) { std::memcpy(p, &v, 2); }

// ---- TEA decryption ----
static inline void tea_decrypt_block(uint32_t& v0, uint32_t& v1, const uint32_t key[4], uint32_t rounds = 8) {
    const uint32_t delta = 0x9E3779B9;
    uint32_t sum = delta * rounds;
    for (uint32_t r = 0; r < rounds; r++) {
        v1 -= ((key[2] + (v0 << 4)) ^ (key[3] + (v0 >> 5))) ^ (v0 + sum);
        uint32_t s2 = sum + v1;
        sum -= delta;
        v0 -= ((key[0] + (v1 << 4)) ^ s2) ^ (key[1] + (v1 >> 5));
    }
}

static inline void tea_decrypt(uint8_t* data, uint32_t len, const uint32_t key[4], uint32_t rounds = 8) {
    for (uint32_t off = 0; off + 8 <= len; off += 8) {
        uint32_t v0 = rd32(data + off), v1 = rd32(data + off + 4);
        tea_decrypt_block(v0, v1, key, rounds);
        wr32(data + off, v0); wr32(data + off + 4, v1);
    }
}

// Key variants (for compatibility with potential client updates)
struct ModKeyVariant {
    uint32_t key[4];
    uint32_t rounds;
};

static inline ModKeyVariant getModKeyVariant(int variant) {
    ModKeyVariant kv;
    int kv_idx = variant % 5;
    int rv = variant / 5;
    kv.rounds = (rv == 0) ? 8 : (rv == 1 ? 16 : 32);
    for (int i = 0; i < 4; i++) {
        uint32_t t = MOD_KEY_RAW[i];
        switch (kv_idx) {
            default: kv.key[i] = ror32(t ^ MOD_KEY_XOR, 19); break;
            case 1:  kv.key[i] = t; break;
            case 2:  kv.key[i] = t ^ MOD_KEY_XOR; break;
            case 3:  kv.key[i] = ror32(t, 19); break;
            case 4:  kv.key[i] = ror32(t ^ MOD_KEY_XOR, 13); break;
        }
    }
    return kv;
}

static const int MOD_KEY_VARIANTS = 15;

// ---- .mod detection ----
static inline bool isMod(const uint8_t* buf, size_t size) {
    return size >= MOD_HDR && rd32(buf) == MOD_MAGIC;
}

// ---- RW stream validation ----
static inline uint32_t libidToVersion(uint32_t libid) {
    if (libid & 0xFFFF0000) return (((libid >> 14) & 0x3FF00) + 0x30000) | ((libid >> 16) & 0x3F);
    return libid << 8;
}

static inline bool looksLikeRwStream(const uint8_t* p, uint32_t maxSize) {
    uint32_t id = rd32(p), sz = rd32(p + 4), lib = rd32(p + 8);
    if (id != rwCLUMP && id != rwUVANIMDICT) return false;
    if (sz < 12 || sz > 512u * 1024 * 1024) return false;
    uint32_t v = libidToVersion(lib);
    return v >= 0x30000 && v <= 0x3FFFF;
}

// Auto-detect key variant by trying to decrypt first 16 bytes
static inline int detectModKeyVariant(const uint8_t* buf, uint32_t bufSize, uint32_t maxPayload = 0) {
    if (bufSize < MOD_HDR + 16) return -1;
    if (!maxPayload) maxPayload = bufSize;
    if (looksLikeRwStream(buf + MOD_HDR, maxPayload)) return -2;  // not encrypted
    for (int v = 0; v < MOD_KEY_VARIANTS; v++) {
        uint8_t t[16]; std::memcpy(t, buf + MOD_HDR, 16);
        auto kv = getModKeyVariant(v);
        tea_decrypt(t, 16, kv.key, kv.rounds);
        if (looksLikeRwStream(t, maxPayload)) return v;
    }
    return -1;
}

// Decrypt .mod in-place. Returns payload length (0 = error).
// After call, buf[0..len) = raw RW stream (not yet normalized).
static inline uint32_t decryptModInplace(uint8_t* buf, uint32_t bufSize, std::string* warn = nullptr, int variant = 0) {
    if (!isMod(buf, bufSize)) return 0;
    uint32_t length = rd32(buf + 4);
    uint32_t nBlocks = rd32(buf + 8);
    uint32_t enc = nBlocks * MOD_BLOCK;
    if (MOD_HDR + enc > bufSize) {
        if (warn) { char t[160]; std::snprintf(t, sizeof t, "header says %u blocks (%u bytes) but buffer is %u bytes - truncating", nBlocks, enc, bufSize); *warn = t; }
        enc = bufSize - MOD_HDR;
    }
    if (variant >= 0) {
        auto kv = getModKeyVariant(variant);
        for (uint32_t off = 0; off + MOD_BLOCK <= enc; off += MOD_BLOCK) {
            tea_decrypt(buf + MOD_HDR + off, MOD_BLOCK, kv.key, kv.rounds);
        }
    }
    if (length > bufSize - MOD_HDR) length = bufSize - MOD_HDR;
    std::memmove(buf, buf + MOD_HDR, length);
    return length;
}

// Diagnostic string for failed .mod
static inline std::string modDiag(const uint8_t* buf, uint32_t bufSize) {
    char t[512]; std::string s;
    std::snprintf(t, sizeof t, "file=%u hdr: length=%u blocks=%u (=%u bytes) reserved=", bufSize, rd32(buf + 4), rd32(buf + 8), rd32(buf + 8) * MOD_BLOCK); s = t;
    for (int i = 0; i < 16; i++) { std::snprintf(t, sizeof t, "%02X", buf[12 + i]); s += t; }
    s += " enc[0..16)=";
    for (int i = 0; i < 16 && MOD_HDR + i < bufSize; i++) { std::snprintf(t, sizeof t, "%02X", buf[MOD_HDR + i]); s += t; }
    uint8_t d[16] = {0};
    size_t dn = bufSize > MOD_HDR ? bufSize - MOD_HDR : 0;
    if (dn > 16) dn = 16;
    std::memcpy(d, buf + MOD_HDR, dn);
    auto kv = getModKeyVariant(0);
    tea_decrypt(d, 16, kv.key, kv.rounds);
    s += " dec0[0..16)=";
    for (int i = 0; i < 16; i++) { std::snprintf(t, sizeof t, "%02X", d[i]); s += t; }
    return s;
}

} // namespace brmod