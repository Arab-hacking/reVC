#ifndef REVC_CUSTOM_BPCPIPELINE_H
#define REVC_CUSTOM_BPCPIPELINE_H

// Standalone Android custom/zip (.bpc) manifest, virtual archive addressing,
// zero-copy .btx (rfmt + KTX1) GPU descriptor parser, and FreezeMonitor.
// Kept free of RenderWare and OpenGL header dependencies so it compiles cleanly
// in both the engine and standalone C++ unit tests.

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include "br/third_party/astc_decomp.h"

namespace bpcpipeline {

// 32-bit flat archive descriptor matching STARTUP_PIPELINE.md section 3.3:
//   high 8 bits  (addr >> 24)      : archive index (0..254; 0xFF reserved for INVALID)
//   low  24 bits (addr & 0xFFFFFF) : entry index inside that archive (0..16,777,215)
static const uint32_t BPC_ADDR_INVALID = 0xFFFFFFFFu;
static const uint8_t  BPC_MAX_ARCHIVES = 254u;

inline uint32_t MakeBpcAddr(uint8_t archiveId, uint32_t entryIdx)
{
	if(archiveId > BPC_MAX_ARCHIVES || entryIdx > 0x00FFFFFFu)
		return BPC_ADDR_INVALID;
	return ((uint32_t)archiveId << 24) | (entryIdx & 0x00FFFFFFu);
}

inline bool IsValidBpcAddr(uint32_t addr)
{
	return addr != BPC_ADDR_INVALID && (uint8_t)(addr >> 24) <= BPC_MAX_ARCHIVES;
}

inline uint8_t BpcAddrArchive(uint32_t addr)
{
	return (uint8_t)(addr >> 24);
}

inline uint32_t BpcAddrEntry(uint32_t addr)
{
	return addr & 0x00FFFFFFu;
}

inline std::string ToLowerAscii(const std::string &s)
{
	std::string out(s);
	for(size_t i = 0; i < out.size(); i++){
		unsigned char c = (unsigned char)out[i];
		if(c >= 'A' && c <= 'Z')
			out[i] = (char)(c - 'A' + 'a');
	}
	return out;
}

inline bool EndsWithCI(const std::string &s, const char *suffix)
{
	size_t slen = std::strlen(suffix);
	if(s.size() < slen)
		return false;
	for(size_t i = 0; i < slen; i++){
		unsigned char a = (unsigned char)s[s.size() - slen + i];
		unsigned char b = (unsigned char)suffix[i];
		if(a >= 'A' && a <= 'Z') a = (unsigned char)(a - 'A' + 'a');
		if(b >= 'A' && b <= 'Z') b = (unsigned char)(b - 'A' + 'a');
		if(a != b)
			return false;
	}
	return true;
}

inline bool StartsWithCI(const std::string &s, const char *prefix)
{
	size_t plen = std::strlen(prefix);
	if(s.size() < plen)
		return false;
	for(size_t i = 0; i < plen; i++){
		unsigned char a = (unsigned char)s[i];
		unsigned char b = (unsigned char)prefix[i];
		if(a >= 'A' && a <= 'Z') a = (unsigned char)(a - 'A' + 'a');
		if(b >= 'A' && b <= 'Z') b = (unsigned char)(b - 'A' + 'a');
		if(a != b)
			return false;
	}
	return true;
}

// Normalizes a manifest path ("mesh\\world_0.bpc" -> "mesh/world_0.bpc") and
// rejects empty paths, drive letters, and ".." traversal segments.
inline std::string SanitizeManifestRelPath(const std::string &raw)
{
	std::string out;
	size_t b = 0, e = raw.size();
	while(b < e && (raw[b] == ' ' || raw[b] == '\t' || raw[b] == '\r' || raw[b] == '\n' || raw[b] == '"'))
		b++;
	while(e > b && (raw[e - 1] == ' ' || raw[e - 1] == '\t' || raw[e - 1] == '\r' || raw[e - 1] == '\n' || raw[e - 1] == '"'))
		e--;
	for(size_t i = b; i < e; i++){
		char c = raw[i];
		if(c == '\\') c = '/';
		if(c == ':')
			return std::string();
		if(c == '/' && (out.empty() || out[out.size() - 1] == '/'))
			continue;
		out.push_back(c);
	}
	while(out.size() >= 2 && out[0] == '.' && out[1] == '/')
		out.erase(0, 2);
	while(!out.empty() && out[out.size() - 1] == '/')
		out.erase(out.size() - 1);
	if(out.empty())
		return std::string();

	size_t seg = 0;
	while(seg <= out.size()){
		size_t slash = out.find('/', seg);
		size_t len = (slash == std::string::npos) ? (out.size() - seg) : (slash - seg);
		if(len == 0 || (len == 1 && out[seg] == '.') || (len == 2 && out[seg] == '.' && out[seg + 1] == '.'))
			return std::string();
		if(slash == std::string::npos)
			break;
		seg = slash + 1;
	}
	return out;
}

inline void StripLineComment(std::string &line)
{
	for(size_t i = 0; i < line.size(); i++){
		if(line[i] == '#' || line[i] == ';'){
			line.erase(i);
			return;
		}
		if(line[i] == '/' && i + 1 < line.size() && line[i + 1] == '/'){
			line.erase(i);
			return;
		}
	}
}

inline bool ExtractDirectiveArg(const std::string &rawLine, const char *directive, std::string &outArg)
{
	std::string line(rawLine);
	StripLineComment(line);
	size_t p = 0;
	while(p < line.size() && (line[p] == ' ' || line[p] == '\t' || line[p] == '\r'))
		p++;
	size_t kwStart = p;
	while(p < line.size() && line[p] != ' ' && line[p] != '\t' && line[p] != '\r')
		p++;
	if(p == kwStart)
		return false;
	std::string kw = ToLowerAscii(line.substr(kwStart, p - kwStart));
	std::string want = ToLowerAscii(std::string(directive));
	if(kw != want)
		return false;
	while(p < line.size() && (line[p] == ' ' || line[p] == '\t'))
		p++;
	if(p >= line.size())
		return false;
	outArg = SanitizeManifestRelPath(line.substr(p));
	return !outArg.empty();
}

// Parses "CDIMAGE mesh\<archive>.bpc" entries from data/config.dat in order.
inline std::vector<std::string> ParseConfigCdImages(const char *data, size_t size)
{
	std::vector<std::string> out;
	std::set<std::string> seen;
	if(!data || size == 0)
		return out;

	size_t pos = 0;
	while(pos < size){
		size_t nl = pos;
		while(nl < size && data[nl] != '\n')
			nl++;
		std::string line(data + pos, nl - pos);
		pos = nl + 1;

		std::string rel;
		if(!ExtractDirectiveArg(line, "CDIMAGE", rel))
			continue;
		if(!EndsWithCI(rel, ".bpc"))
			continue;
		std::string key = ToLowerAscii(rel);
		if(seen.insert(key).second)
			out.push_back(rel);
	}
	return out;
}

// Parses "TEXDB <base>" entries from data/texdb.dat in declaration order.
// Strips any trailing .astc.bpc / .bpc / .tmb suffix so callers uniformly
// expand <base> -> <base>.astc.bpc and <base>.tmb.
inline std::vector<std::string> ParseTexDbManifest(const char *data, size_t size)
{
	std::vector<std::string> out;
	std::set<std::string> seen;
	if(!data || size == 0)
		return out;

	size_t pos = 0;
	while(pos < size){
		size_t nl = pos;
		while(nl < size && data[nl] != '\n')
			nl++;
		std::string line(data + pos, nl - pos);
		pos = nl + 1;

		std::string rel;
		if(!ExtractDirectiveArg(line, "TEXDB", rel))
			continue;
		if(EndsWithCI(rel, ".astc.bpc.tmb") || EndsWithCI(rel, ".etc2.bpc.tmb"))
			rel.erase(rel.size() - 13);
		else if(EndsWithCI(rel, ".bpc.tmb"))
			rel.erase(rel.size() - 8);
		else if(EndsWithCI(rel, ".astc.bpc") || EndsWithCI(rel, ".etc2.bpc") ||
		        EndsWithCI(rel, ".astc.tmb") || EndsWithCI(rel, ".etc2.tmb"))
			rel.erase(rel.size() - 9);
		else if(EndsWithCI(rel, ".bpc") || EndsWithCI(rel, ".tmb"))
			rel.erase(rel.size() - 4);
		if(rel.empty())
			continue;
		std::string key = ToLowerAscii(rel);
		if(seen.insert(key).second)
			out.push_back(rel);
	}
	return out;
}

inline void AppendUniquePath(std::vector<std::string> &list, const std::string &path)
{
	std::string key = ToLowerAscii(path);
	for(size_t i = 0; i < list.size(); i++)
		if(ToLowerAscii(list[i]) == key)
			return;
	list.push_back(path);
}

// Candidate filesystem paths for a CDIMAGE entry under <customRoot> (e.g. "custom").
// Primary layout per STARTUP_PIPELINE.md: <customRoot>/zip/mesh/<archive>.bpc.
inline std::vector<std::string> BuildCdImageCandidates(const std::string &customRoot, const std::string &relPath)
{
	std::vector<std::string> cands;
	std::string clean = SanitizeManifestRelPath(relPath);
	if(clean.empty())
		return cands;
	std::string root = customRoot;
	while(!root.empty() && (root[root.size() - 1] == '/' || root[root.size() - 1] == '\\'))
		root.erase(root.size() - 1);

	if(StartsWithCI(clean, "zip/")){
		AppendUniquePath(cands, root + "/" + clean);
	}else{
		AppendUniquePath(cands, root + "/zip/" + clean);
		if(!StartsWithCI(clean, "mesh/"))
			AppendUniquePath(cands, root + "/zip/mesh/" + clean);
		AppendUniquePath(cands, root + "/" + clean);
	}
	return cands;
}

// Candidate filesystem paths for a TEXDB entry's main .astc.bpc archive.
// Handles both "textures/Characters" and bare "text" entries from texdb.dat.
inline std::vector<std::string> BuildTexDbBpcCandidates(const std::string &customRoot, const std::string &basePath)
{
	std::vector<std::string> cands;
	std::string clean = SanitizeManifestRelPath(basePath);
	if(clean.empty())
		return cands;
	std::string root = customRoot;
	while(!root.empty() && (root[root.size() - 1] == '/' || root[root.size() - 1] == '\\'))
		root.erase(root.size() - 1);

	const bool hasDir = (clean.find('/') != std::string::npos);
	if(StartsWithCI(clean, "zip/")){
		AppendUniquePath(cands, root + "/" + clean + ".astc.bpc");
		AppendUniquePath(cands, root + "/" + clean + ".bpc");
	}else{
		if(!hasDir){
			AppendUniquePath(cands, root + "/zip/textures/" + clean + ".astc.bpc");
			AppendUniquePath(cands, root + "/zip/" + clean + ".astc.bpc");
			AppendUniquePath(cands, root + "/zip/textures/" + clean + ".bpc");
			AppendUniquePath(cands, root + "/zip/" + clean + ".bpc");
		}else{
			AppendUniquePath(cands, root + "/zip/" + clean + ".astc.bpc");
			AppendUniquePath(cands, root + "/zip/" + clean + ".bpc");
		}
	}
	return cands;
}

// Candidate filesystem paths for a TEXDB entry's companion .tmb thumbnail archive.
// Real-world BR mobile packages name the companion "<db>.<fmt>.bpc.tmb" (e.g.
// "textures/br_tex_mobile.astc.bpc.tmb" per MODEL_TEXTURE_STREAMING.md §2.1);
// also support ".bpc.tmb", ".tmb", and ".astc.tmb" fallbacks.
inline std::vector<std::string> BuildTexDbTmbCandidates(const std::string &customRoot, const std::string &basePath)
{
	std::vector<std::string> cands;
	std::string clean = SanitizeManifestRelPath(basePath);
	if(clean.empty())
		return cands;
	std::string root = customRoot;
	while(!root.empty() && (root[root.size() - 1] == '/' || root[root.size() - 1] == '\\'))
		root.erase(root.size() - 1);

	const bool hasDir = (clean.find('/') != std::string::npos);
	if(StartsWithCI(clean, "zip/")){
		AppendUniquePath(cands, root + "/" + clean + ".astc.bpc.tmb");
		AppendUniquePath(cands, root + "/" + clean + ".bpc.tmb");
		AppendUniquePath(cands, root + "/" + clean + ".tmb");
		AppendUniquePath(cands, root + "/" + clean + ".astc.tmb");
	}else if(!hasDir){
		AppendUniquePath(cands, root + "/zip/textures/" + clean + ".astc.bpc.tmb");
		AppendUniquePath(cands, root + "/zip/" + clean + ".astc.bpc.tmb");
		AppendUniquePath(cands, root + "/zip/textures/" + clean + ".bpc.tmb");
		AppendUniquePath(cands, root + "/zip/" + clean + ".bpc.tmb");
		AppendUniquePath(cands, root + "/zip/textures/" + clean + ".tmb");
		AppendUniquePath(cands, root + "/zip/" + clean + ".tmb");
		AppendUniquePath(cands, root + "/zip/textures/" + clean + ".astc.tmb");
		AppendUniquePath(cands, root + "/zip/" + clean + ".astc.tmb");
	}else{
		AppendUniquePath(cands, root + "/zip/" + clean + ".astc.bpc.tmb");
		AppendUniquePath(cands, root + "/zip/" + clean + ".bpc.tmb");
		AppendUniquePath(cands, root + "/zip/" + clean + ".tmb");
		AppendUniquePath(cands, root + "/zip/" + clean + ".astc.tmb");
	}
	return cands;
}

// ---------------------------------------------------------------------------
// Direct GPU .btx (4-byte rfmt + KTX1) parser (no CPU mip decompression, no .txd)
// ---------------------------------------------------------------------------

enum BtxGlConstants : uint32_t {
	BTX_GL_RGB                               = 0x1907u,
	BTX_GL_RGBA                              = 0x1908u,
	BTX_GL_UNSIGNED_BYTE                     = 0x1401u,
	BTX_GL_UNSIGNED_SHORT_4_4_4_4            = 0x8033u,
	BTX_GL_UNSIGNED_SHORT_5_5_5_1            = 0x8034u,
	BTX_GL_UNSIGNED_SHORT_5_6_5              = 0x8363u,
	BTX_GL_RGB8                              = 0x8051u,
	BTX_GL_RGBA4                             = 0x8056u,
	BTX_GL_RGB5_A1                           = 0x8057u,
	BTX_GL_RGBA8                             = 0x8058u,
	BTX_GL_RGB565                            = 0x8D62u,
	BTX_GL_COMPRESSED_RGBA_ASTC_4x4_KHR      = 0x93B0u,
	BTX_GL_COMPRESSED_RGBA_ASTC_12x12_KHR    = 0x93BDu,
	BTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4  = 0x93D0u,
	BTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12= 0x93DDu
};

struct BtxMipSlice {
	uint32_t width;
	uint32_t height;
	uint32_t dataOffset;       // byte offset from start of the .btx buffer
	uint32_t byteSize;         // payload size for glCompressedTexImage2D / glTexImage2D
	uint8_t  unpackAlignment;  // 1 or 4 for glPixelStorei(GL_UNPACK_ALIGNMENT, ...)
};

struct BtxKtxGpuInfo {
	uint32_t rfmt;
	uint32_t width;
	uint32_t height;
	uint32_t glInternalFormat; // linear ASTC 0x93B0..0x93BD or uncompressed sized format
	uint32_t glFormat;         // GL_RGBA / GL_RGB (for uncompressed or metadata)
	uint32_t glType;           // 0 for compressed ASTC, GL_UNSIGNED_* for uncompressed
	uint8_t  bpp;              // bytes per pixel for uncompressed (2, 3, or 4); 0 for ASTC
	uint8_t  blockW;           // ASTC block width (4..12) or 1 for uncompressed
	uint8_t  blockH;           // ASTC block height (4..12) or 1 for uncompressed
	bool     isCompressed;
	bool     isAstc;
	bool     headerWasSrgb;
	bool     hasAlpha;
	std::vector<BtxMipSlice> mips;
};

inline uint32_t ReadLE32(const uint8_t *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

inline bool ProbeAstcAlphaBlocks(const uint8_t *btxData, const BtxKtxGpuInfo &info)
{
	if(info.mips.empty() || info.blockW == 0 || info.blockH == 0)
		return false;

	std::vector<uint8_t> decoded((size_t)info.blockW * info.blockH * 4);
	// 1. Decode the smallest mip level (typically 1x1..4x4 = 1 single 16-byte ASTC block):
	//    any cutout/decal/foliage/glass texture has average alpha < 255 in its top mip.
	const BtxMipSlice &tiny = info.mips[info.mips.size() - 1];
	uint32_t tinyBx = (tiny.width + info.blockW - 1) / info.blockW;
	uint32_t tinyBy = (tiny.height + info.blockH - 1) / info.blockH;
	uint32_t tinyTotal = tinyBx * tinyBy;
	uint32_t checkTiny = tinyTotal < 4u ? tinyTotal : 4u;
	for(uint32_t b = 0; b < checkTiny; b++){
		const uint8_t *blk = btxData + tiny.dataOffset + b * 16u;
		if(!basisu::astc::decompress(decoded.data(), blk, false, info.blockW, info.blockH))
			continue;
		uint32_t bx = b % tinyBx;
		uint32_t by = b / tinyBx;
		for(uint32_t yy = 0; yy < info.blockH; yy++){
			if(by * info.blockH + yy >= tiny.height) break;
			for(uint32_t xx = 0; xx < info.blockW; xx++){
				if(bx * info.blockW + xx >= tiny.width) break;
				if(decoded[(yy * info.blockW + xx) * 4 + 3] < 255)
					return true;
			}
		}
	}

	// 2. Sparse 4x4 grid probe across mip 0 (at most 16 blocks = 256 bytes total) to catch
	//    localized alpha windows on large textures without CPU-decoding full mip chains.
	const BtxMipSlice &m0 = info.mips[0];
	uint32_t bx0 = (m0.width + info.blockW - 1) / info.blockW;
	uint32_t by0 = (m0.height + info.blockH - 1) / info.blockH;
	if(bx0 == 0 || by0 == 0)
		return false;
	const uint32_t gridX = bx0 < 4u ? bx0 : 4u;
	const uint32_t gridY = by0 < 4u ? by0 : 4u;
	for(uint32_t gy = 0; gy < gridY; gy++){
		uint32_t y = (gy * (by0 - 1)) / (gridY > 1 ? (gridY - 1) : 1);
		for(uint32_t gx = 0; gx < gridX; gx++){
			uint32_t x = (gx * (bx0 - 1)) / (gridX > 1 ? (gridX - 1) : 1);
			const uint8_t *blk = btxData + m0.dataOffset + (y * bx0 + x) * 16u;
			if(!basisu::astc::decompress(decoded.data(), blk, false, info.blockW, info.blockH))
				continue;
			for(uint32_t yy = 0; yy < info.blockH; yy++){
				if(y * info.blockH + yy >= m0.height) break;
				for(uint32_t xx = 0; xx < info.blockW; xx++){
					if(x * info.blockW + xx >= m0.width) break;
					if(decoded[(yy * info.blockW + xx) * 4 + 3] < 255)
						return true;
				}
			}
		}
	}
	return false;
}

inline bool ProbeRawAlphaSample(const uint8_t *btxData, const BtxKtxGpuInfo &info)
{
	if(info.mips.empty() || info.glFormat == BTX_GL_RGB || info.bpp == 3)
		return false;
	const BtxMipSlice &m0 = info.mips[0];
	uint32_t rowBytes = m0.width * info.bpp;
	uint32_t rowPitch = m0.unpackAlignment == 4 ? ((rowBytes + 3u) & ~3u) : rowBytes;
	uint32_t stepY = m0.height > 32u ? (m0.height / 32u) : 1u;
	uint32_t stepX = m0.width > 32u ? (m0.width / 32u) : 1u;
	for(uint32_t y = 0; y < m0.height; y += stepY){
		const uint8_t *row = btxData + m0.dataOffset + (size_t)y * rowPitch;
		if(info.glType == BTX_GL_UNSIGNED_BYTE && info.bpp == 4){
			for(uint32_t x = 0; x < m0.width; x += stepX)
				if(row[x * 4 + 3] < 255)
					return true;
		}else if(info.glType == BTX_GL_UNSIGNED_SHORT_4_4_4_4){
			for(uint32_t x = 0; x < m0.width; x += stepX){
				uint16_t v = (uint16_t)(row[x * 2] | (row[x * 2 + 1] << 8));
				if((v & 0x0Fu) != 0x0Fu)
					return true;
			}
		}else if(info.glType == BTX_GL_UNSIGNED_SHORT_5_5_5_1){
			for(uint32_t x = 0; x < m0.width; x += stepX){
				uint16_t v = (uint16_t)(row[x * 2] | (row[x * 2 + 1] << 8));
				if((v & 0x01u) == 0u)
					return true;
			}
		}
	}
	return false;
}

// Parses a .btx buffer (4-byte rfmt + KTX 1.1 container) into direct GL upload slices.
// Returns false if corrupt or if an exotic format requires CPU fallback via brtex::decode.
inline bool ParseBtxKtxGpu(const uint8_t *data, size_t size, BtxKtxGpuInfo &out, std::string *err = nullptr)
{
	static const uint8_t kKtx11Magic[12] = {
		0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
	};
	static const uint8_t kAstcBlocks[14][2] = {
		{ 4, 4 }, { 5, 4 }, { 5, 5 }, { 6, 5 }, { 6, 6 }, { 8, 5 }, { 8, 6 },
		{ 8, 8 }, { 10, 5 }, { 10, 6 }, { 10, 8 }, { 10, 10 }, { 12, 10 }, { 12, 12 }
	};

	out = BtxKtxGpuInfo();
	if(!data || size < 68 || std::memcmp(data + 4, kKtx11Magic, 12) != 0){
		if(err) *err = "not a BTX/KTX11 container";
		return false;
	}

	out.rfmt = ReadLE32(data);
	const uint8_t *k = data + 4;
	const size_t kn = size - 4;
	if(ReadLE32(k + 12) != 0x04030201u){
		if(err) *err = "unsupported big-endian KTX11";
		return false;
	}

	const uint32_t glType         = ReadLE32(k + 16);
	const uint32_t glFormat       = ReadLE32(k + 24);
	const uint32_t glInternalOrig = ReadLE32(k + 28);
	const uint32_t w              = ReadLE32(k + 36);
	const uint32_t h              = ReadLE32(k + 40);
	uint32_t nmip                 = ReadLE32(k + 56);
	const uint32_t kvBytes        = ReadLE32(k + 60);

	if(w == 0 || h == 0 || w > 8192u || h > 8192u || nmip > 16u || 64ull + kvBytes > kn){
		if(err) *err = "invalid KTX11 dimensions or key-value size";
		return false;
	}
	if(nmip == 0)
		nmip = 1;

	size_t off = 64u + (size_t)kvBytes;
	out.width = w;
	out.height = h;

	// Case 1: Uncompressed GLES-native KTX1 formats (RGBA8, RGB8, RGBA4444, RGB565, RGBA5551)
	if(glType != 0){
		if(glType == BTX_GL_UNSIGNED_BYTE){
			const bool isRgb = (glFormat == BTX_GL_RGB || glInternalOrig == BTX_GL_RGB8 || glInternalOrig == BTX_GL_RGB);
			out.glInternalFormat = isRgb ? BTX_GL_RGB8 : BTX_GL_RGBA8;
			out.glFormat         = isRgb ? BTX_GL_RGB  : BTX_GL_RGBA;
			out.glType           = BTX_GL_UNSIGNED_BYTE;
			out.bpp              = isRgb ? 3 : 4;
		}else if(glType == BTX_GL_UNSIGNED_SHORT_4_4_4_4){
			out.glInternalFormat = BTX_GL_RGBA4;
			out.glFormat         = BTX_GL_RGBA;
			out.glType           = BTX_GL_UNSIGNED_SHORT_4_4_4_4;
			out.bpp              = 2;
		}else if(glType == BTX_GL_UNSIGNED_SHORT_5_6_5){
			out.glInternalFormat = BTX_GL_RGB565;
			out.glFormat         = BTX_GL_RGB;
			out.glType           = BTX_GL_UNSIGNED_SHORT_5_6_5;
			out.bpp              = 2;
		}else if(glType == BTX_GL_UNSIGNED_SHORT_5_5_5_1){
			out.glInternalFormat = BTX_GL_RGB5_A1;
			out.glFormat         = BTX_GL_RGBA;
			out.glType           = BTX_GL_UNSIGNED_SHORT_5_5_5_1;
			out.bpp              = 2;
		}else{
			if(err) *err = "unsupported uncompressed KTX11 glType";
			return false;
		}

		out.blockW = 1;
		out.blockH = 1;
		out.isCompressed = false;
		out.isAstc = false;
		out.headerWasSrgb = false;

		uint32_t mw = w, mh = h;
		for(uint32_t m = 0; m < nmip; m++){
			if(off + 4u > kn)
				break;
			uint32_t sz = ReadLE32(k + off);
			off += 4u;
			if(off + sz > kn)
				break;
			uint64_t rowBytes = (uint64_t)mw * out.bpp;
			uint64_t rowPitch4 = (rowBytes + 3ull) & ~3ull;
			uint8_t align = 4;
			uint64_t needed = rowPitch4 * mh;
			if(sz < needed){
				if(sz < rowBytes * mh)
					break;
				align = 1;
				needed = rowBytes * mh;
			}
			BtxMipSlice slice;
			slice.width = mw;
			slice.height = mh;
			slice.dataOffset = (uint32_t)(4u + off);
			slice.byteSize = (uint32_t)needed;
			slice.unpackAlignment = align;
			out.mips.push_back(slice);

			off += sz;
			off = (off + 3u) & ~(size_t)3u;
			mw = mw > 1u ? (mw / 2u) : 1u;
			mh = mh > 1u ? (mh / 2u) : 1u;
		}
		if(out.mips.empty()){
			if(err) *err = "no valid uncompressed KTX11 mip levels";
			return false;
		}
		out.hasAlpha = ProbeRawAlphaSample(data, out);
		return true;
	}

	// Case 2: Compressed ASTC LDR (4x4 .. 12x12). Deduce the true block footprint
	// from mip 0's byte size because BR assets frequently store 0x93B4 in the header
	// while the payload is 4x4 ASTC; always map to linear GL_COMPRESSED_RGBA_ASTC_*_KHR
	// (0x93B0..0x93BD) because reVC renders without GL_FRAMEBUFFER_SRGB.
	if(off + 4u > kn){
		if(err) *err = "truncated KTX11 before mip0 size";
		return false;
	}
	const uint32_t sz0 = ReadLE32(k + off);
	int blockIdx = -1;
	for(int idx = 0; idx < 14; idx++){
		uint32_t bw = kAstcBlocks[idx][0];
		uint32_t bh = kAstcBlocks[idx][1];
		uint64_t need = (uint64_t)((w + bw - 1u) / bw) * (uint64_t)((h + bh - 1u) / bh) * 16ull;
		if(need == sz0){
			blockIdx = idx;
			break;
		}
	}
	if(blockIdx < 0){
		if(err) *err = "unable to match ASTC block footprint to mip0 size";
		return false;
	}

	const uint8_t bw = kAstcBlocks[blockIdx][0];
	const uint8_t bh = kAstcBlocks[blockIdx][1];
	out.glInternalFormat = BTX_GL_COMPRESSED_RGBA_ASTC_4x4_KHR + (uint32_t)blockIdx;
	out.glFormat         = BTX_GL_RGBA;
	out.glType           = 0;
	out.bpp              = 0;
	out.blockW           = bw;
	out.blockH           = bh;
	out.isCompressed     = true;
	out.isAstc           = true;
	out.headerWasSrgb    = (glInternalOrig >= BTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4 &&
	                        glInternalOrig <= BTX_GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12);

	uint32_t mw = w, mh = h;
	for(uint32_t m = 0; m < nmip; m++){
		if(off + 4u > kn)
			break;
		uint32_t sz = ReadLE32(k + off);
		off += 4u;
		if(off + sz > kn)
			break;
		uint64_t need = (uint64_t)((mw + bw - 1u) / bw) * (uint64_t)((mh + bh - 1u) / bh) * 16ull;
		if(sz < need)
			break;
		BtxMipSlice slice;
		slice.width = mw;
		slice.height = mh;
		slice.dataOffset = (uint32_t)(4u + off);
		slice.byteSize = (uint32_t)need;
		slice.unpackAlignment = 1;
		out.mips.push_back(slice);

		off += sz;
		off = (off + 3u) & ~(size_t)3u;
		mw = mw > 1u ? (mw / 2u) : 1u;
		mh = mh > 1u ? (mh / 2u) : 1u;
	}
	if(out.mips.empty()){
		if(err) *err = "no valid ASTC mip levels in KTX11";
		return false;
	}

	out.hasAlpha = ProbeAstcAlphaBlocks(data, out);
	return true;
}

// ---------------------------------------------------------------------------
// FreezeMonitor & per-frame main-thread GPU substitution budget tracker
// (STARTUP_PIPELINE.md sections 3.6 & 5.8)
// ---------------------------------------------------------------------------

class BpcFreezeMonitor
{
	uint64_t m_frameBudgetUs;
	uint64_t m_spikeThresholdUs;
	uint64_t m_frameSpentUs;
	uint32_t m_frameUploads;
	uint64_t m_totalUploads;
	uint64_t m_totalSpikes;
	uint64_t m_maxSingleAssetUs;
	std::string m_slowestAsset;

public:
	explicit BpcFreezeMonitor(uint64_t frameBudgetUs = 2500ull, uint64_t spikeThresholdUs = 12000ull)
		: m_frameBudgetUs(frameBudgetUs),
		  m_spikeThresholdUs(spikeThresholdUs),
		  m_frameSpentUs(0),
		  m_frameUploads(0),
		  m_totalUploads(0),
		  m_totalSpikes(0),
		  m_maxSingleAssetUs(0)
	{}

	void BeginFrame(void)
	{
		m_frameSpentUs = 0;
		m_frameUploads = 0;
	}

	// Always allow at least 1 upload per frame to guarantee forward progress,
	// then stop once the per-frame main-thread budget has been consumed.
	bool HasFrameBudget(void) const
	{
		return m_frameUploads == 0 || m_frameSpentUs < m_frameBudgetUs;
	}

	// Returns true when this single asset exceeded m_spikeThresholdUs so the
	// caller can emit a diagnostic warning to debug.log.
	bool RecordMainThreadWork(const char *assetName, uint64_t elapsedUs)
	{
		m_frameSpentUs += elapsedUs;
		m_frameUploads++;
		m_totalUploads++;
		if(elapsedUs > m_maxSingleAssetUs){
			m_maxSingleAssetUs = elapsedUs;
			m_slowestAsset = assetName ? assetName : "";
		}
		if(elapsedUs >= m_spikeThresholdUs){
			m_totalSpikes++;
			return true;
		}
		return false;
	}

	uint64_t FrameSpentUs(void) const { return m_frameSpentUs; }
	uint32_t FrameUploads(void) const { return m_frameUploads; }
	uint64_t TotalUploads(void) const { return m_totalUploads; }
	uint64_t TotalSpikes(void) const { return m_totalSpikes; }
	uint64_t MaxSingleAssetUs(void) const { return m_maxSingleAssetUs; }
	const std::string &SlowestAsset(void) const { return m_slowestAsset; }
};

} // namespace bpcpipeline

#endif // REVC_CUSTOM_BPCPIPELINE_H
