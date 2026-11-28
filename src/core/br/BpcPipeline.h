#pragma once

// Black Russia .bpc pipeline — manifest parsing, path resolution, BTX GPU parser, FreezeMonitor
// Extracted from src/extras/custom/BpcPipeline.h

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <set>
#include <algorithm>

namespace bpcpipe {

// BPC virtual addressing: archiveId (8 bits) << 24 | entryIdx (24 bits)
static const uint32_t BPC_ADDR_INVALID = 0xFFFFFFFFu;
static const uint8_t  BPC_MAX_ARCHIVES = 254u;

static inline uint32_t MakeBpcAddr(uint8_t archiveId, uint32_t entryIdx) {
    if (archiveId > BPC_MAX_ARCHIVES || entryIdx > 0x00FFFFFFu) return BPC_ADDR_INVALID;
    return ((uint32_t)archiveId << 24) | (entryIdx & 0x00FFFFFFu);
}
static inline bool IsValidBpcAddr(uint32_t addr) {
    return addr != BPC_ADDR_INVALID && (uint8_t)(addr >> 24) <= BPC_MAX_ARCHIVES;
}
static inline uint8_t BpcAddrArchive(uint32_t addr) { return (uint8_t)(addr >> 24); }
static inline uint32_t BpcAddrEntry(uint32_t addr) { return addr & 0x00FFFFFFu; }

// String utils
static inline std::string ToLowerAscii(const std::string& s) {
    std::string out(s);
    for (size_t i = 0; i < out.size(); i++) {
        unsigned char c = (unsigned char)out[i];
        if (c >= 'A' && c <= 'Z') out[i] = (char)(c - 'A' + 'a');
    }
    return out;
}
static inline bool EndsWithCI(const std::string& s, const char* suffix) {
    size_t slen = std::strlen(suffix);
    if (s.size() < slen) return false;
    for (size_t i = 0; i < slen; i++) {
        unsigned char a = (unsigned char)s[s.size() - slen + i];
        unsigned char b = (unsigned char)suffix[i];
        if (a >= 'A' && a <= 'Z') a = (unsigned char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (unsigned char)(b - 'A' + 'a');
        if (a != b) return false;
    }
    return true;
}
static inline bool StartsWithCI(const std::string& s, const char* prefix) {
    size_t plen = std::strlen(prefix);
    if (s.size() < plen) return false;
    for (size_t i = 0; i < plen; i++) {
        unsigned char a = (unsigned char)s[i];
        unsigned char b = (unsigned char)prefix[i];
        if (a >= 'A' && a <= 'Z') a = (unsigned char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (unsigned char)(b - 'A' + 'a');
        if (a != b) return false;
    }
    return true;
}

static inline void StripLineComment(std::string& line) {
    for (size_t i = 0; i < line.size(); i++) {
        if (line[i] == '#' || line[i] == ';') { line.erase(i); return; }
        if (line[i] == '/' && i + 1 < line.size() && line[i+1] == '/') { line.erase(i); return; }
    }
}

// Normalize manifest path: backslash→forward, no drive letters, no .. traversal, no empty segments
static inline std::string SanitizeManifestRelPath(const std::string& raw) {
    std::string out;
    size_t b = 0, e = raw.size();
    while (b < e && (raw[b] == ' ' || raw[b] == '\t' || raw[b] == '\r' || raw[b] == '\n' || raw[b] == '"')) b++;
    while (e > b && (raw[e-1] == ' ' || raw[e-1] == '\t' || raw[e-1] == '\r' || raw[e-1] == '\n' || raw[e-1] == '"')) e--;
    for (size_t i = b; i < e; i++) {
        char c = raw[i];
        if (c == '\\') c = '/';
        if (c == ':') return "";
        if (c == '/' && (out.empty() || out.back() == '/')) continue;
        out.push_back(c);
    }
    while (out.size() >= 2 && out[0] == '.' && out[1] == '/') out.erase(0, 2);
    while (!out.empty() && out.back() == '/') out.pop_back();
    if (out.empty()) return "";
    size_t seg = 0;
    while (seg <= out.size()) {
        size_t slash = out.find('/', seg);
        size_t len = (slash == std::string::npos) ? (out.size() - seg) : (slash - seg);
        if (len == 0 || (len == 1 && out[seg] == '.') || (len == 2 && out[seg] == '.' && out[seg+1] == '.')) return "";
        if (slash == std::string::npos) break;
        seg = slash + 1;
    }
    return out;
}

static inline bool ExtractDirectiveArg(const std::string& rawLine, const char* directive, std::string& outArg) {
    std::string line(rawLine);
    StripLineComment(line);
    size_t p = 0;
    while (p < line.size() && (line[p] == ' ' || line[p] == '\t' || line[p] == '\r')) p++;
    size_t kwStart = p;
    while (p < line.size() && line[p] != ' ' && line[p] != '\t' && line[p] != '\r') p++;
    if (p == kwStart) return false;
    std::string kw = ToLowerAscii(line.substr(kwStart, p - kwStart));
    std::string want = ToLowerAscii(directive);
    if (kw != want) return false;
    while (p < line.size() && (line[p] == ' ' || line[p] == '\t')) p++;
    if (p >= line.size()) return false;
    outArg = SanitizeManifestRelPath(line.substr(p));
    return !outArg.empty();
}

// Parse CDIMAGE mesh\<archive>.bpc from config.dat
static inline std::vector<std::string> ParseConfigCdImages(const char* data, size_t size) {
    std::vector<std::string> out;
    std::set<std::string> seen;
    if (!data || size == 0) return out;
    size_t pos = 0;
    while (pos < size) {
        size_t nl = pos;
        while (nl < size && data[nl] != '\n') nl++;
        std::string line(data + pos, nl - pos);
        pos = nl + 1;
        std::string rel;
        if (!ExtractDirectiveArg(line, "CDIMAGE", rel)) continue;
        if (!EndsWithCI(rel, ".bpc")) continue;
        std::string key = ToLowerAscii(rel);
        if (seen.insert(key).second) out.push_back(rel);
    }
    return out;
}

// Parse TEXDB <base> from texdb.dat (strip suffixes)
static inline std::vector<std::string> ParseTexDbManifest(const char* data, size_t size) {
    std::vector<std::string> out;
    std::set<std::string> seen;
    if (!data || size == 0) return out;
    size_t pos = 0;
    while (pos < size) {
        size_t nl = pos;
        while (nl < size && data[nl] != '\n') nl++;
        std::string line(data + pos, nl - pos);
        pos = nl + 1;
        std::string rel;
        if (!ExtractDirectiveArg(line, "TEXDB", rel)) continue;
        if (EndsWithCI(rel, ".astc.bpc.tmb") || EndsWithCI(rel, ".etc2.bpc.tmb")) rel.erase(rel.size() - 13);
        else if (EndsWithCI(rel, ".bpc.tmb")) rel.erase(rel.size() - 8);
        else if (EndsWithCI(rel, ".astc.bpc") || EndsWithCI(rel, ".etc2.bpc") || EndsWithCI(rel, ".astc.tmb") || EndsWithCI(rel, ".etc2.tmb")) rel.erase(rel.size() - 9);
        else if (EndsWithCI(rel, ".bpc") || EndsWithCI(rel, ".tmb")) rel.erase(rel.size() - 4);
        if (rel.empty()) continue;
        std::string key = ToLowerAscii(rel);
        if (seen.insert(key).second) out.push_back(rel);
    }
    return out;
}

static inline void AppendUniquePath(std::vector<std::string>& list, const std::string& path) {
    std::string key = ToLowerAscii(path);
    for (size_t i = 0; i < list.size(); i++) if (ToLowerAscii(list[i]) == key) return;
    list.push_back(path);
}

// Candidate paths for CDIMAGE entry under customRoot
static inline std::vector<std::string> BuildCdImageCandidates(const std::string& customRoot, const std::string& relPath) {
    std::vector<std::string> cands;
    std::string clean = SanitizeManifestRelPath(relPath);
    if (clean.empty()) return cands;
    std::string root = customRoot;
    while (!root.empty() && (root.back() == '/' || root.back() == '\\')) root.pop_back();
    if (StartsWithCI(clean, "zip/")) {
        AppendUniquePath(cands, root + "/" + clean);
    } else {
        AppendUniquePath(cands, root + "/zip/" + clean);
        if (!StartsWithCI(clean, "mesh/")) AppendUniquePath(cands, root + "/zip/mesh/" + clean);
        AppendUniquePath(cands, root + "/" + clean);
    }
    return cands;
}

// Candidate paths for TEXDB main .astc.bpc
static inline std::vector<std::string> BuildTexDbBpcCandidates(const std::string& customRoot, const std::string& basePath) {
    std::vector<std::string> cands;
    std::string clean = SanitizeManifestRelPath(basePath);
    if (clean.empty()) return cands;
    std::string root = customRoot;
    while (!root.empty() && (root.back() == '/' || root.back() == '\\')) root.pop_back();
    bool hasDir = clean.find('/') != std::string::npos;
    if (StartsWithCI(clean, "zip/")) {
        AppendUniquePath(cands, root + "/" + clean + ".astc.bpc");
        AppendUniquePath(cands, root + "/" + clean + ".bpc");
    } else {
        if (!hasDir) {
            AppendUniquePath(cands, root + "/zip/textures/" + clean + ".astc.bpc");
            AppendUniquePath(cands, root + "/zip/" + clean + ".astc.bpc");
            AppendUniquePath(cands, root + "/zip/textures/" + clean + ".bpc");
            AppendUniquePath(cands, root + "/zip/" + clean + ".bpc");
        } else {
            AppendUniquePath(cands, root + "/zip/" + clean + ".astc.bpc");
            AppendUniquePath(cands, root + "/zip/" + clean + ".bpc");
        }
    }
    return cands;
}

// Candidate paths for TEXDB .tmb thumbnail
static inline std::vector<std::string> BuildTexDbTmbCandidates(const std::string& customRoot, const std::string& basePath) {
    std::vector<std::string> cands;
    std::string clean = SanitizeManifestRelPath(basePath);
    if (clean.empty()) return cands;
    std::string root = customRoot;
    while (!root.empty() && (root.back() == '/' || root.back() == '\\')) root.pop_back();
    bool hasDir = clean.find('/') != std::string::npos;
    if (StartsWithCI(clean, "zip/")) {
        AppendUniquePath(cands, root + "/" + clean + ".astc.bpc.tmb");
        AppendUniquePath(cands, root + "/" + clean + ".bpc.tmb");
        AppendUniquePath(cands, root + "/" + clean + ".tmb");
        AppendUniquePath(cands, root + "/" + clean + ".astc.tmb");
    } else if (!hasDir) {
        AppendUniquePath(cands, root + "/zip/textures/" + clean + ".astc.bpc.tmb");
        AppendUniquePath(cands, root + "/zip/" + clean + ".astc.bpc.tmb");
        AppendUniquePath(cands, root + "/zip/textures/" + clean + ".bpc.tmb");
        AppendUniquePath(cands, root + "/zip/" + clean + ".bpc.tmb");
        AppendUniquePath(cands, root + "/zip/textures/" + clean + ".tmb");
        AppendUniquePath(cands, root + "/zip/" + clean + ".tmb");
        AppendUniquePath(cands, root + "/zip/textures/" + clean + ".astc.tmb");
        AppendUniquePath(cands, root + "/zip/" + clean + ".astc.tmb");
    } else {
        AppendUniquePath(cands, root + "/zip/" + clean + ".astc.bpc.tmb");
        AppendUniquePath(cands, root + "/zip/" + clean + ".bpc.tmb");
        AppendUniquePath(cands, root + "/zip/" + clean + ".tmb");
        AppendUniquePath(cands, root + "/zip/" + clean + ".astc.tmb");
    }
    return cands;
}

// ---- BTX GPU parser (for direct upload) ----
enum BtxGl : uint32_t {
    BTX_GL_RGB                         = 0x1907,
    BTX_GL_RGBA                        = 0x1908,
    BTX_GL_UNSIGNED_BYTE               = 0x1401,
    BTX_GL_UNSIGNED_SHORT_4_4_4_4      = 0x8033,
    BTX_GL_UNSIGNED_SHORT_5_6_5        = 0x8363,
    BTX_GL_UNSIGNED_SHORT_5_5_5_1      = 0x8034,
    BTX_GL_RGB8                        = 0x8051,
    BTX_GL_RGBA8                       = 0x8058,
    BTX_GL_RGB5_A1                     = 0x8057,
    BTX_GL_RGBA4                       = 0x8056,
    BTX_GL_RGB565                      = 0x8D62,
    BTX_GL_COMPRESSED_RGBA_ASTC_4x4_KHR     = 0x93B0,
    BTX_GL_COMPRESSED_RGBA_ASTC_12x12_KHR   = 0x93BD,
    BTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4 = 0x93D0,
    BTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12 = 0x93DD,
};

struct BtxMipSlice {
    uint32_t width, height;
    uint32_t dataOffset;    // from start of .btx buffer
    uint32_t byteSize;
    uint8_t  unpackAlignment;
};

struct BtxKtxGpuInfo {
    uint32_t rfmt;
    uint32_t width, height;
    uint32_t glInternalFormat;
    uint32_t glFormat;
    uint32_t glType;
    uint8_t  bpp;
    uint8_t  blockW, blockH;
    bool     isCompressed;
    bool     isAstc;
    bool     headerWasSrgb;
    bool     hasAlpha;
    std::vector<BtxMipSlice> mips;
};

static inline uint32_t ReadLE32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static const uint8_t KTX11_MAGIC[12] = { 0xAB, 'K', 'T', 'X', ' ', '1', '1', 0xBB, 0x0D, 0x0A, 0x1A, 0x0A };
static const uint8_t ASTC_BLOCKS[14][2] = {
    {4,4}, {5,4}, {5,5}, {6,5}, {6,6}, {8,5}, {8,6}, {8,8},
    {10,5}, {10,6}, {10,8}, {10,10}, {12,10}, {12,12}
};

// Parse .btx for direct GPU upload (no CPU decompression)
static inline bool ParseBtxKtxGpu(const uint8_t* data, size_t size, BtxKtxGpuInfo& out, std::string* err = nullptr) {
    out = BtxKtxGpuInfo();
    if (!data || size < 68 || std::memcmp(data + 4, KTX11_MAGIC, 12) != 0) {
        if (err) *err = "not BTX/KTX11"; return false;
    }
    out.rfmt = ReadLE32(data);
    const uint8_t* k = data + 4;
    const size_t kn = size - 4;
    if (ReadLE32(k + 12) != 0x04030201u) { if (err) *err = "big-endian KTX11"; return false; }
    uint32_t glType = ReadLE32(k + 16), glFormat = ReadLE32(k + 24), glInternalOrig = ReadLE32(k + 28);
    uint32_t w = ReadLE32(k + 36), h = ReadLE32(k + 40), nmip = ReadLE32(k + 56), kvBytes = ReadLE32(k + 60);
    if (w == 0 || h == 0 || w > 8192 || h > 8192 || nmip > 16 || 64ull + kvBytes > kn) { if (err) *err = "bad KTX11 dims"; return false; }
    if (nmip == 0) nmip = 1;
    size_t off = 64u + kvBytes;
    out.width = w; out.height = h;

    if (glType != 0) {
        // Uncompressed
        if (glType == BTX_GL_UNSIGNED_BYTE) {
            bool isRgb = (glFormat == BTX_GL_RGB || glInternalOrig == BTX_GL_RGB8 || glInternalOrig == BTX_GL_RGB);
            out.glInternalFormat = isRgb ? BTX_GL_RGB8 : BTX_GL_RGBA8;
            out.glFormat = isRgb ? BTX_GL_RGB : BTX_GL_RGBA;
            out.glType = BTX_GL_UNSIGNED_BYTE;
            out.bpp = isRgb ? 3 : 4;
        } else if (glType == BTX_GL_UNSIGNED_SHORT_4_4_4_4) {
            out.glInternalFormat = BTX_GL_RGBA4; out.glFormat = BTX_GL_RGBA; out.glType = BTX_GL_UNSIGNED_SHORT_4_4_4_4; out.bpp = 2;
        } else if (glType == BTX_GL_UNSIGNED_SHORT_5_6_5) {
            out.glInternalFormat = BTX_GL_RGB565; out.glFormat = BTX_GL_RGB; out.glType = BTX_GL_UNSIGNED_SHORT_5_6_5; out.bpp = 2;
        } else if (glType == BTX_GL_UNSIGNED_SHORT_5_5_5_1) {
            out.glInternalFormat = BTX_GL_RGB5_A1; out.glFormat = BTX_GL_RGBA; out.glType = BTX_GL_UNSIGNED_SHORT_5_5_5_1; out.bpp = 2;
        } else { if (err) *err = "unsupported glType"; return false; }
        out.blockW = out.blockH = 1; out.isCompressed = false; out.isAstc = false; out.headerWasSrgb = false;
        uint32_t mw = w, mh = h;
        for (uint32_t m = 0; m < nmip; m++) {
            if (off + 4 > kn) break;
            uint32_t sz = ReadLE32(k + off); off += 4;
            if (off + sz > kn) break;
            uint64_t rowBytes = (uint64_t)mw * out.bpp;
            uint64_t rowPitch4 = (rowBytes + 3ull) & ~3ull;
            uint8_t align = 4; uint64_t needed = rowPitch4 * mh;
            if (sz < needed) { if (sz < rowBytes * mh) break; align = 1; needed = rowBytes * mh; }
            out.mips.push_back({ mw, mh, (uint32_t)(4u + off), (uint32_t)needed, align });
            off += sz; off = (off + 3u) & ~3u;
            mw = mw > 1 ? mw / 2 : 1; mh = mh > 1 ? mh / 2 : 1;
        }
        if (out.mips.empty()) { if (err) *err = "no uncompressed mips"; return false; }
        return true;
    }

    // ASTC compressed
    if (off + 4 > kn) { if (err) *err = "truncated before mip0"; return false; }
    uint32_t sz0 = ReadLE32(k + off);
    int blockIdx = -1;
    for (int i = 0; i < 14; i++) {
        uint32_t bw = ASTC_BLOCKS[i][0], bh = ASTC_BLOCKS[i][1];
        uint64_t need = (uint64_t)((w + bw - 1) / bw) * ((h + bh - 1) / bh) * 16;
        if (need == sz0) { blockIdx = i; break; }
    }
    if (blockIdx < 0) { if (err) *err = "ASTC block mismatch"; return false; }
    uint8_t bw = ASTC_BLOCKS[blockIdx][0], bh = ASTC_BLOCKS[blockIdx][1];
    out.glInternalFormat = BTX_GL_COMPRESSED_RGBA_ASTC_4x4_KHR + blockIdx;
    out.glFormat = BTX_GL_RGBA; out.glType = 0; out.bpp = 0;
    out.blockW = bw; out.blockH = bh; out.isCompressed = true; out.isAstc = true;
    out.headerWasSrgb = (glInternalOrig >= BTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4 && glInternalOrig <= BTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12);

    uint32_t mw = w, mh = h;
    for (uint32_t m = 0; m < nmip; m++) {
        if (off + 4 > kn) break;
        uint32_t sz = ReadLE32(k + off); off += 4;
        if (off + sz > kn) break;
        uint64_t need = (uint64_t)((mw + bw - 1) / bw) * ((mh + bh - 1) / bh) * 16;
        if (sz < need) break;
        out.mips.push_back({ mw, mh, (uint32_t)(4u + off), (uint32_t)need, 1 });
        off += sz; off = (off + 3u) & ~3u;
        mw = mw > 1 ? mw / 2 : 1; mh = mh > 1 ? mh / 2 : 1;
    }
    if (out.mips.empty()) { if (err) *err = "no ASTC mips"; return false; }
    return true;
}

// ---- FreezeMonitor: per-frame GPU upload budget ----
class BpcFreezeMonitor {
    uint64_t m_frameBudgetUs, m_spikeThresholdUs, m_frameSpentUs;
    uint32_t m_frameUploads;
    uint64_t m_totalUploads, m_totalSpikes, m_maxSingleAssetUs;
    std::string m_slowestAsset;
public:
    explicit BpcFreezeMonitor(uint64_t frameBudgetUs = 2500ull, uint64_t spikeThresholdUs = 12000ull)
        : m_frameBudgetUs(frameBudgetUs), m_spikeThresholdUs(spikeThresholdUs),
          m_frameSpentUs(0), m_frameUploads(0), m_totalUploads(0), m_totalSpikes(0), m_maxSingleAssetUs(0) {}
    void BeginFrame() { m_frameSpentUs = 0; m_frameUploads = 0; }
    bool HasFrameBudget() const { return m_frameUploads == 0 || m_frameSpentUs < m_frameBudgetUs; }
    bool RecordMainThreadWork(const char* assetName, uint64_t elapsedUs) {
        m_frameSpentUs += elapsedUs; m_frameUploads++; m_totalUploads++;
        if (elapsedUs > m_maxSingleAssetUs) { m_maxSingleAssetUs = elapsedUs; m_slowestAsset = assetName ? assetName : ""; }
        if (elapsedUs >= m_spikeThresholdUs) { m_totalSpikes++; return true; }
        return false;
    }
    uint64_t FrameSpentUs() const { return m_frameSpentUs; }
    uint32_t FrameUploads() const { return m_frameUploads; }
    uint64_t TotalUploads() const { return m_totalUploads; }
    uint64_t TotalSpikes() const { return m_totalSpikes; }
    uint64_t MaxSingleAssetUs() const { return m_maxSingleAssetUs; }
    const std::string& SlowestAsset() const { return m_slowestAsset; }
};

} // namespace bpcpipe