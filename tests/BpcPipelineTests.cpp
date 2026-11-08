#include "../tools/custom-folder-verification/shim/common.h"
#include "BpcPipeline.h"
#include "BackgroundAssetQueue.h"
#include "CustomZip.h"
#include "brformats.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

static void Check(bool ok, const char *why)
{
	if(!ok){
		std::fprintf(stderr, "BpcPipelineTests FAILED: %s\n", why);
		std::abort();
	}
}

static void PutU16(std::vector<uint8_t> &b, uint16_t v)
{
	b.push_back((uint8_t)(v & 0xFFu));
	b.push_back((uint8_t)((v >> 8) & 0xFFu));
}

static void PutU32(std::vector<uint8_t> &b, uint32_t v)
{
	b.push_back((uint8_t)(v & 0xFFu));
	b.push_back((uint8_t)((v >> 8) & 0xFFu));
	b.push_back((uint8_t)((v >> 16) & 0xFFu));
	b.push_back((uint8_t)((v >> 24) & 0xFFu));
}

struct ZipTestItem {
	std::string name;
	std::vector<uint8_t> data;
	bool deflateStoredBlock;
};

// Builds an in-memory ZIP archive using stored (method 0) and/or RFC 1951 type-0
// raw deflate streams (method 8) so both stored and compressed .bpc paths run.
static std::vector<uint8_t> BuildSyntheticZip(const std::vector<ZipTestItem> &items)
{
	std::vector<uint8_t> zip;
	struct CdRec {
		std::string name;
		uint16_t method;
		uint32_t compSize;
		uint32_t uncompSize;
		uint32_t localOff;
	};
	std::vector<CdRec> cds;

	for(size_t i = 0; i < items.size(); i++){
		const ZipTestItem &it = items[i];
		std::vector<uint8_t> payload;
		uint16_t method = 0;
		if(it.deflateStoredBlock && it.data.size() <= 65535u){
			method = 8;
			uint16_t len = (uint16_t)it.data.size();
			uint16_t nlen = (uint16_t)~len;
			payload.push_back(0x01u); // BFINAL=1, BTYPE=00 (stored block inside deflate)
			PutU16(payload, len);
			PutU16(payload, nlen);
			payload.insert(payload.end(), it.data.begin(), it.data.end());
		}else{
			payload = it.data;
		}

		CdRec rec;
		rec.name = it.name;
		rec.method = method;
		rec.compSize = (uint32_t)payload.size();
		rec.uncompSize = (uint32_t)it.data.size();
		rec.localOff = (uint32_t)zip.size();
		cds.push_back(rec);

		PutU32(zip, 0x04034B50u);
		PutU16(zip, 20);
		PutU16(zip, 0);
		PutU16(zip, method);
		PutU16(zip, 0);
		PutU16(zip, 0);
		PutU32(zip, 0);
		PutU32(zip, rec.compSize);
		PutU32(zip, rec.uncompSize);
		PutU16(zip, (uint16_t)rec.name.size());
		PutU16(zip, 0);
		zip.insert(zip.end(), rec.name.begin(), rec.name.end());
		zip.insert(zip.end(), payload.begin(), payload.end());
	}

	uint32_t cdOff = (uint32_t)zip.size();
	for(size_t i = 0; i < cds.size(); i++){
		const CdRec &rec = cds[i];
		PutU32(zip, 0x02014B50u);
		PutU16(zip, 20);
		PutU16(zip, 20);
		PutU16(zip, 0);
		PutU16(zip, rec.method);
		PutU16(zip, 0);
		PutU16(zip, 0);
		PutU32(zip, 0);
		PutU32(zip, rec.compSize);
		PutU32(zip, rec.uncompSize);
		PutU16(zip, (uint16_t)rec.name.size());
		PutU16(zip, 0);
		PutU16(zip, 0);
		PutU16(zip, 0);
		PutU16(zip, 0);
		PutU32(zip, 0);
		PutU32(zip, rec.localOff);
		zip.insert(zip.end(), rec.name.begin(), rec.name.end());
	}
	uint32_t cdSize = (uint32_t)zip.size() - cdOff;

	PutU32(zip, 0x06054B50u);
	PutU16(zip, 0);
	PutU16(zip, 0);
	PutU16(zip, (uint16_t)cds.size());
	PutU16(zip, (uint16_t)cds.size());
	PutU32(zip, cdSize);
	PutU32(zip, cdOff);
	PutU16(zip, 0);
	return zip;
}

static void TestVirtualBpcAddressing(void)
{
	uint32_t a0 = bpcpipeline::MakeBpcAddr(0, 0);
	Check(bpcpipeline::IsValidBpcAddr(a0), "archive 0 entry 0 valid");
	Check(bpcpipeline::BpcAddrArchive(a0) == 0 && bpcpipeline::BpcAddrEntry(a0) == 0, "decode 0:0");

	uint32_t aMax = bpcpipeline::MakeBpcAddr(254, 0x00ABCDEFu);
	Check(bpcpipeline::IsValidBpcAddr(aMax), "archive 254 valid");
	Check(bpcpipeline::BpcAddrArchive(aMax) == 254, "decode archive 254");
	Check(bpcpipeline::BpcAddrEntry(aMax) == 0x00ABCDEFu, "decode entry 0xABCDEF");

	Check(!bpcpipeline::IsValidBpcAddr(bpcpipeline::BPC_ADDR_INVALID), "0xFFFFFFFF invalid");
	Check(bpcpipeline::MakeBpcAddr(255, 1) == bpcpipeline::BPC_ADDR_INVALID, "archive 255 reserved");
	Check(bpcpipeline::MakeBpcAddr(1, 0x01000000u) == bpcpipeline::BPC_ADDR_INVALID, "24-bit entry overflow rejected");
}

static void TestManifestParsersAndPathCandidates(void)
{
	const char *configText =
		"# Android BR startup manifest\r\n"
		"CDIMAGE mesh\\world_0.bpc\r\n"
		"CDIMAGE mesh\\cars_br.bpc ; inline comment\r\n"
		"CDIMAGE mesh/world_0.bpc\r\n"
		"CDIMAGE ../evil.bpc\r\n"
		"CDIMAGE models/gta3.img\r\n"
		"IDE data\\maps\\city\\center.ide\r\n";

	std::vector<std::string> cds = bpcpipeline::ParseConfigCdImages(configText, std::strlen(configText));
	Check(cds.size() == 2, "expected 2 deduplicated valid .bpc CDIMAGE entries");
	Check(cds[0] == "mesh/world_0.bpc", "first CDIMAGE normalized");
	Check(cds[1] == "mesh/cars_br.bpc", "second CDIMAGE normalized");

	std::vector<std::string> meshCands = bpcpipeline::BuildCdImageCandidates("custom", cds[0]);
	Check(!meshCands.empty() && meshCands[0] == "custom/zip/mesh/world_0.bpc",
		"primary CDIMAGE candidate points to custom/zip/mesh/world_0.bpc");

	// Exact sample lines matching user's data/texdb.dat
	const char *texdbText =
		"TEXDB text\n"
		"TEXDB textures/BugFix\n"
		"TEXDB textures/Characters\n"
		"TEXDB textures/Roads3-0\n"
		"TEXDB textures/interiors0-9\n"
		"TEXDB textures/Characters.astc.bpc\n"
		"TEXDB ../outside\n";

	std::vector<std::string> tdbs = bpcpipeline::ParseTexDbManifest(texdbText, std::strlen(texdbText));
	Check(tdbs.size() == 5, "expected 5 deduplicated valid TEXDB entries");
	Check(tdbs[0] == "text", "bare TEXDB text preserved");
	Check(tdbs[2] == "textures/Characters", "TEXDB textures/Characters preserved");
	Check(tdbs[4] == "textures/interiors0-9", "TEXDB textures/interiors0-9 preserved");

	std::vector<std::string> bareBpc = bpcpipeline::BuildTexDbBpcCandidates("custom", tdbs[0]);
	Check(bareBpc.size() >= 2 &&
	      bareBpc[0] == "custom/zip/textures/text.astc.bpc" &&
	      bareBpc[1] == "custom/zip/text.astc.bpc",
		"bare TEXDB resolves textures/text.astc.bpc and text.astc.bpc");

	std::vector<std::string> charBpc = bpcpipeline::BuildTexDbBpcCandidates("custom", tdbs[2]);
	Check(!charBpc.empty() && charBpc[0] == "custom/zip/textures/Characters.astc.bpc",
		"TEXDB textures/Characters resolves to custom/zip/textures/Characters.astc.bpc");

	std::vector<std::string> charTmb = bpcpipeline::BuildTexDbTmbCandidates("custom", tdbs[2]);
	Check(!charTmb.empty() && charTmb[0] == "custom/zip/textures/Characters.tmb",
		"TEXDB companion .tmb resolves to custom/zip/textures/Characters.tmb");
}

static void TestXorCommonBpcAndConcurrentPread(void)
{
	std::vector<ZipTestItem> items(3);
	items[0].name = "data/config.dat";
	const char *cfg = "CDIMAGE mesh\\world_0.bpc\n";
	items[0].data.assign((const uint8_t*)cfg, (const uint8_t*)cfg + std::strlen(cfg));
	items[0].deflateStoredBlock = false;

	items[1].name = "data/texdb.dat";
	const char *tdb = "TEXDB textures/Roads3-0\n";
	items[1].data.assign((const uint8_t*)tdb, (const uint8_t*)tdb + std::strlen(tdb));
	items[1].deflateStoredBlock = true;

	items[2].name = "data/lods.info";
	items[2].data.resize(4099);
	for(size_t i = 0; i < items[2].data.size(); i++)
		items[2].data[i] = (uint8_t)((i * 37u + 11u) & 0xFFu);
	items[2].deflateStoredBlock = true;

	std::vector<uint8_t> plainZip = BuildSyntheticZip(items);
	std::vector<uint8_t> xorBpc = plainZip;
	// Encrypt in odd-sized chunks to verify arbitrary-offset cyclic 20-byte XOR key continuity.
	size_t off = 0;
	while(off < xorBpc.size()){
		size_t chunk = std::min<size_t>(13u, xorBpc.size() - off);
		CustomZip::ApplyBpcXor((uint64)off, xorBpc.data() + off, (uint32)chunk);
		off += chunk;
	}

	const char *xorPath = "test_common_xor.bpc";
	const char *plainPath = "test_mesh_plain.bpc";
	{
		FILE *f = std::fopen(xorPath, "wb");
		Check(f != nullptr && std::fwrite(xorBpc.data(), 1, xorBpc.size(), f) == xorBpc.size(), "write xor bpc");
		std::fclose(f);

		f = std::fopen(plainPath, "wb");
		Check(f != nullptr && std::fwrite(plainZip.data(), 1, plainZip.size(), f) == plainZip.size(), "write plain bpc");
		std::fclose(f);
	}

	CustomZip zReject;
	Check(!zReject.Open(xorPath, false), "encrypted common.bpc must not open when allowXor=false");

	CustomZip zXor;
	Check(zXor.Open(xorPath, true), "encrypted common.bpc opens when allowXor=true");
	Check(zXor.IsXorEncrypted(), "encrypted common.bpc detected as XOR");
	Check(zXor.GetNumEntries() == 3, "3 entries indexed in common.bpc");

	CustomZip zPlain;
	Check(zPlain.Open(plainPath, true), "plain .bpc opens when allowXor=true");
	Check(!zPlain.IsXorEncrypted(), "plain .bpc detected as unencrypted ZIP");

	// Multi-threaded concurrent Extract stress test on the same CustomZip instance
	std::atomic<bool> okAll(true);
	std::vector<std::thread> threads;
	for(int t = 0; t < 4; t++){
		threads.emplace_back([&zXor, &items, &okAll, t]{
			for(int iter = 0; iter < 50; iter++){
				int idx = (t + iter) % 3;
				std::vector<uint8_t> out(items[idx].data.size());
				if(!zXor.Extract(idx, out.data(), (uint32)out.size()) || out != items[idx].data){
					okAll.store(false);
					return;
				}
			}
		});
	}
	for(size_t t = 0; t < threads.size(); t++)
		threads[t].join();
	Check(okAll.load(), "concurrent multi-threaded Extract from XOR .bpc matched byte-for-byte");

	zXor.Close();
	zPlain.Close();
	std::remove(xorPath);
	std::remove(plainPath);
}

static void TeaEncryptBlock8Rounds(uint32_t &v0, uint32_t &v1, const uint32_t k[4])
{
	uint32_t sum = 0;
	for(int r = 0; r < 8; r++){
		sum += 0x9E3779B9u;
		v0 += ((v1 << 4) + k[0]) ^ (v1 + sum) ^ ((v1 >> 5) + k[1]);
		v1 += ((v0 << 4) + k[2]) ^ (v0 + sum) ^ ((v0 >> 5) + k[3]);
	}
}

static void TestModPartialHeadTeaDecrypt(void)
{
	// Construct a payload > 0x800 bytes where ONLY the first 1 * 0x800 block is TEA-encrypted
	// and the tail after 0x800 remains plain bytes, matching STARTUP_PIPELINE.md section 2.2.
	std::vector<uint8_t> expected(0x1000, 0);
	// Minimal RW Clump header at offset 0 of payload
	uint32_t clumpId = br::rwCLUMP, clumpSz = 12, libId = br::SA_LIBID;
	uint32_t structId = br::rwSTRUCT, structSz = 0;
	std::memcpy(&expected[0], &clumpId, 4);
	std::memcpy(&expected[4], &clumpSz, 4);
	std::memcpy(&expected[8], &libId, 4);
	std::memcpy(&expected[12], &structId, 4);
	std::memcpy(&expected[16], &structSz, 4);
	std::memcpy(&expected[20], &libId, 4);
	for(size_t i = 24; i < expected.size(); i++)
		expected[i] = (uint8_t)((i * 19u + 3u) & 0xFFu);

	uint32_t key[4];
	for(int i = 0; i < 4; i++)
		key[i] = br::ror32(br::MOD_KEY_RAW[i] ^ 0x12913AFBu, 19);

	std::vector<uint8_t> mod(br::MOD_HDR + expected.size(), 0);
	uint32_t magic = br::MOD_MAGIC;
	uint32_t origLen = (uint32_t)expected.size();
	uint32_t nBlocks = 1; // encrypt ONLY the first 0x800 bytes!
	std::memcpy(&mod[0], &magic, 4);
	std::memcpy(&mod[4], &origLen, 4);
	std::memcpy(&mod[8], &nBlocks, 4);
	std::memcpy(&mod[br::MOD_HDR], expected.data(), expected.size());

	for(size_t off = 0; off < 0x800u; off += 8u){
		uint32_t v0 = br::rd32(&mod[br::MOD_HDR + off]);
		uint32_t v1 = br::rd32(&mod[br::MOD_HDR + off + 4]);
		TeaEncryptBlock8Rounds(v0, v1, key);
		br::wr32(&mod[br::MOD_HDR + off], v0);
		br::wr32(&mod[br::MOD_HDR + off + 4], v1);
	}

	uint32_t decLen = br::decryptModInplace(mod.data(), (uint32_t)mod.size());
	Check(decLen == expected.size(), "partial head TEA .mod decrypted in place");
	mod.resize(decLen);
	Check(mod == expected, "head 0x800 decrypted and unencrypted tail preserved verbatim");
}

static std::vector<uint8_t> MakeSyntheticBtx(
	uint32_t rfmt, uint32_t glType, uint32_t glFormat, uint32_t glInternal,
	uint32_t w, uint32_t h, const std::vector<std::vector<uint8_t> > &mips)
{
	static const uint8_t kKtxMagic[12] = {
		0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
	};
	std::vector<uint8_t> b;
	PutU32(b, rfmt);
	b.insert(b.end(), kKtxMagic, kKtxMagic + 12);
	PutU32(b, 0x04030201u);             // endianness
	PutU32(b, glType);                  // glType
	PutU32(b, glType ? 1u : 1u);        // glTypeSize
	PutU32(b, glFormat);                // glFormat
	PutU32(b, glInternal);              // glInternalFormat
	PutU32(b, glFormat ? glFormat : 0x1908u); // glBaseInternalFormat
	PutU32(b, w);                       // pixelWidth
	PutU32(b, h);                       // pixelHeight
	PutU32(b, 0);                       // pixelDepth
	PutU32(b, 0);                       // numberOfArrayElements
	PutU32(b, 1);                       // numberOfFaces
	PutU32(b, (uint32_t)mips.size());   // numberOfMipmapLevels
	PutU32(b, 0);                       // bytesOfKeyValueData
	for(size_t i = 0; i < mips.size(); i++){
		PutU32(b, (uint32_t)mips[i].size());
		b.insert(b.end(), mips[i].begin(), mips[i].end());
		while((b.size() - 4u) & 3u)
			b.push_back(0);
	}
	return b;
}

static void TestDirectGpuBtxKtxParser(void)
{
	// 1. BR quirk: header declares 0x93D4 (sRGB ASTC 6x6), while mip0 size is 4x4 ASTC (8x8 -> 4 blocks = 64 bytes),
	//    and mip1 is 4x4 -> 1 block = 16 bytes, using solid opaque void-extent ASTC blocks (alpha = 0xFFFF).
	uint8_t opaqueVoidExtent[16] = {
		0xFC, 0xFD, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0x80, 0x80, 0x40, 0x40, 0x20, 0x20, 0xFF, 0xFF
	};
	std::vector<uint8_t> astcMip0(64, 0), astcMip1(16, 0);
	for(int i = 0; i < 4; i++)
		std::memcpy(&astcMip0[i * 16], opaqueVoidExtent, 16);
	std::memcpy(&astcMip1[0], opaqueVoidExtent, 16);

	std::vector<uint8_t> astcBtx = MakeSyntheticBtx(
		1u, 0u, 0u, 0x93D4u, 8u, 8u, { astcMip0, astcMip1 });

	bpcpipeline::BtxKtxGpuInfo info;
	std::string err;
	Check(bpcpipeline::ParseBtxKtxGpu(astcBtx.data(), astcBtx.size(), info, &err), err.c_str());
	Check(info.isCompressed && info.isAstc, "parsed as compressed ASTC");
	Check(info.blockW == 4 && info.blockH == 4, "deduced true 4x4 ASTC block size from mip0 bytes");
	Check(info.glInternalFormat == bpcpipeline::BTX_GL_COMPRESSED_RGBA_ASTC_4x4_KHR,
		"mapped sRGB/mismatched ASTC header to linear GL_COMPRESSED_RGBA_ASTC_4x4_KHR (0x93B0)");
	Check(info.headerWasSrgb, "recorded original sRGB ASTC header flag");
	Check(!info.hasAlpha, "opaque void-extent ASTC blocks detected as hasAlpha=false");
	Check(info.mips.size() == 2 && info.mips[0].byteSize == 64u && info.mips[1].byteSize == 16u,
		"both ASTC mip slices indexed for glCompressedTexImage2D");

	// Semi-transparent void-extent ASTC block -> hasAlpha = true
	uint8_t alphaVoidExtent[16] = {
		0xFC, 0xFD, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0x80, 0x80, 0x40, 0x40, 0x20, 0x20, 0x00, 0x80
	};
	std::memcpy(&astcMip1[0], alphaVoidExtent, 16);
	std::vector<uint8_t> alphaAstcBtx = MakeSyntheticBtx(
		1u, 0u, 0u, 0x93B0u, 8u, 8u, { astcMip0, astcMip1 });
	Check(bpcpipeline::ParseBtxKtxGpu(alphaAstcBtx.data(), alphaAstcBtx.size(), info, &err) && info.hasAlpha,
		"semi-transparent ASTC mip detected as hasAlpha=true");

	// 2. Uncompressed RGB8 with 4-byte KTX row alignment on odd width (3x2: rowBytes=9, rowPitch=12 -> 24 bytes)
	std::vector<uint8_t> rgbMip0(24, 0xAAu);
	std::vector<uint8_t> rgbBtx = MakeSyntheticBtx(
		0u, bpcpipeline::BTX_GL_UNSIGNED_BYTE, bpcpipeline::BTX_GL_RGB, bpcpipeline::BTX_GL_RGB8,
		3u, 2u, { rgbMip0 });
	Check(bpcpipeline::ParseBtxKtxGpu(rgbBtx.data(), rgbBtx.size(), info, &err), "RGB8 KTX1 parsed");
	Check(!info.isCompressed && info.bpp == 3 && !info.hasAlpha, "RGB8 bpp=3 hasAlpha=false");
	Check(info.mips.size() == 1 && info.mips[0].unpackAlignment == 4 && info.mips[0].byteSize == 24u,
		"RGB8 KTX 4-byte row alignment preserved");

	// 3. Uncompressed RGBA4444 with transparent pixel -> hasAlpha = true
	std::vector<uint8_t> rgba4444Mip0(4 * 4 * 2, 0xFFu);
	rgba4444Mip0[0] = 0xF4u; // alpha nibble = 4 (< 15)
	rgba4444Mip0[1] = 0xFFu;
	std::vector<uint8_t> rgba4444Btx = MakeSyntheticBtx(
		0u, bpcpipeline::BTX_GL_UNSIGNED_SHORT_4_4_4_4, bpcpipeline::BTX_GL_RGBA, bpcpipeline::BTX_GL_RGBA4,
		4u, 4u, { rgba4444Mip0 });
	Check(bpcpipeline::ParseBtxKtxGpu(rgba4444Btx.data(), rgba4444Btx.size(), info, &err), "RGBA4444 KTX1 parsed");
	Check(!info.isCompressed && info.glInternalFormat == bpcpipeline::BTX_GL_RGBA4 && info.hasAlpha,
		"RGBA4444 detected hasAlpha=true");
}

static void TestMultiWorkerQueueAndFreezeMonitor(void)
{
	customcache::BackgroundAssetQueue<int> pool(64, 32, 3);
	std::atomic<int> ran(0);
	for(int i = 0; i < 24; i++){
		char key[32];
		std::snprintf(key, sizeof(key), "job:%d", i);
		Check(pool.Schedule(key, [i, &ran]{
			ran.fetch_add(1);
			return std::make_shared<int>(i * 10);
		}, (i % 4) == 0), "schedule on 3-worker pool");
	}
	int popped = 0;
	while(popped < 24){
		customcache::BackgroundAssetQueue<int>::Completed c;
		if(pool.Pop(c)){
			Check(c.value != nullptr, "multi-worker result non-null");
			popped++;
		}else{
			pool.WaitForResult();
		}
	}
	Check(ran.load() == 24, "all 24 jobs executed across worker pool");
	pool.Stop();

	bpcpipeline::BpcFreezeMonitor mon(2500ull, 12000ull);
	mon.BeginFrame();
	Check(mon.HasFrameBudget(), "fresh frame has budget");
	Check(!mon.RecordMainThreadWork("road_01", 1800ull), "1.8 ms is not a spike");
	Check(mon.HasFrameBudget(), "still under 2.5 ms budget");
	Check(!mon.RecordMainThreadWork("road_02", 900ull), "0.9 ms is not a spike");
	Check(!mon.HasFrameBudget(), "2.7 ms total exceeds 2.5 ms frame budget");
	mon.BeginFrame();
	Check(mon.HasFrameBudget(), "BeginFrame resets per-frame budget");
	Check(mon.RecordMainThreadWork("huge_building", 14500ull), "14.5 ms triggers spike log");
	Check(mon.TotalSpikes() == 1 && mon.SlowestAsset() == "huge_building", "FreezeMonitor tracked slowest asset");
}

int main(void)
{
	TestVirtualBpcAddressing();
	TestManifestParsersAndPathCandidates();
	TestXorCommonBpcAndConcurrentPread();
	TestModPartialHeadTeaDecrypt();
	TestDirectGpuBtxKtxParser();
	TestMultiWorkerQueueAndFreezeMonitor();
	std::printf("BpcPipelineTests: ALL PASSED\n");
	return 0;
}
