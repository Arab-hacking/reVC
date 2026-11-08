// End-to-end integration test of the Android (#ifdef ANDROID) custom/zip .bpc
// startup & streaming pipeline on the REAL CCustomModels + CustomCol + CustomZip,
// including direct .btx KTX1/ASTC -> rw::gl3::uploadKtxDirect GPU upload without .txd.
#include "common.h"
#include "CustomModels.h"
#include "CustomColFormat.h"
#include "CustomZip.h"
#include "BpcPipeline.h"
#include "brformats.h"
#include "CdStream.h"
#include "ColStore.h"
#include "FileLoader.h"
#include "ModelInfo.h"
#include "Streaming.h"
#include "Timer.h"
#include "TxdStore.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>

#ifdef main
#undef main
#endif

static std::vector<std::string> g_images;
static int g_loadedCols = 0;
static int g_directGpuUploads = 0;
static int g_lastUploadedInternalFmt = 0;
static int g_lastUploadedCompressed = 0;
static int g_txdStreamReads = 0;
static int g_linearGetModelInfoByNameCalls = 0;
static int g_liveRwTextures = 0;
static int g_simulateLoadDelayUs = 0;
static std::vector<uint8> g_lastLoadedAtomicStreamBytes;
static RwTexture *g_lastResolvedTex = nullptr;

extern "C" bool CdStreamAddImage(const char *path) { g_images.push_back(path); return true; }
extern "C" int32 CdStreamGetNumImages(void) { return (int32)g_images.size(); }
extern "C" char *CdStreamGetImageName(int cd) { return &g_images.at(cd)[0]; }

char *CColStore::GetColName(int32 slot) {
	static char world[] = "world";
	return slot == 0 ? world : nullptr;
}
bool CColStore::LoadCol(int32 slot, uint8 *bytes, int32 size) {
	assert(slot == 0 && bytes && size > 0);
	std::vector<customcolformat::Block> blocks;
	assert(customcolformat::Index(bytes, size, blocks) == 0 && !blocks.empty());
	g_loadedCols += (int)blocks.size();
	return true;
}

uint32 CTimer::m_FrameCounter = 1;
CStreamingInfo CStreaming::ms_aInfoForModel[NUMSTREAMINFO];

CBaseModelInfo::CBaseModelInfo(ModelInfoType t)
	: m_type((uint8)t), m_num2dEffects(0), m_bOwnsColModel(false),
	  m_colModel(nullptr), m_2dEffectsID(-1), m_objectId(-1), m_refCount(0), m_txdSlot(-1)
{
	m_name[0] = '\0';
}
void CBaseModelInfo::Shutdown(void) {}
void CBaseModelInfo::SetTexDictionary(const char *name) {
	int32 slot = CTxdStore::FindTxdSlot(name);
	if(slot == -1) slot = CTxdStore::AddTxdSlot(name);
	m_txdSlot = (int16)slot;
}
struct FakeModelStub : CBaseModelInfo {
	FakeModelStub() : CBaseModelInfo(MITYPE_SIMPLE) {}
	void DeleteRwObject(void) override {}
	RwObject *CreateInstance(void) override { return nullptr; }
	RwObject *CreateInstance(RwMatrix *) override { return nullptr; }
	RwObject *GetRwObject(void) override { return nullptr; }
};
static FakeModelStub g_fakeModel0;
static FakeModelStub g_fakeModel100;
static FakeModelStub g_fakeModel101;
static FakeModelStub g_fakeModel102;
static FakeModelStub g_fakeModel103;
static FakeModelStub g_fakeModel104;
CBaseModelInfo *CModelInfo::ms_modelInfoPtrs[MODELINFOSIZE];
CSimpleModelInfo *CModelInfo::AddSimpleModel(int id) {
	FakeModelStub *stub = &g_fakeModel100;
	if(id == 0) stub = &g_fakeModel0;
	else if(id == 101) stub = &g_fakeModel101;
	else if(id == 102) stub = &g_fakeModel102;
	else if(id == 103) stub = &g_fakeModel103;
	else if(id == 104) stub = &g_fakeModel104;
	ms_modelInfoPtrs[id] = stub;
	return (CSimpleModelInfo*)stub;
}
CBaseModelInfo *CModelInfo::GetModelInfo(const char *name, int *id) {
	g_linearGetModelInfoByNameCalls++;
	if(name && strcasecmp(name, "road_br_01") == 0){
		if(id) *id = 100;
		return ms_modelInfoPtrs[100];
	}
	if(name && strcasecmp(name, "road_br_02") == 0){
		if(id) *id = 101;
		return ms_modelInfoPtrs[101];
	}
	if(id) *id = -1;
	return nullptr;
}
void CModelInfo::ShutDown(void) {
	ms_modelInfoPtrs[0] = nullptr;
	ms_modelInfoPtrs[100] = nullptr;
	ms_modelInfoPtrs[101] = nullptr;
	ms_modelInfoPtrs[102] = nullptr;
	ms_modelInfoPtrs[103] = nullptr;
	ms_modelInfoPtrs[104] = nullptr;
}

namespace rw {
int32 platform = PLATFORM_GL3;
Texture *(*Texture::findCB)(const char *name) = nullptr;

Raster *Raster::create(int32 w, int32 h, int32 d, int32 fmt, int32) {
	Raster *r = (Raster*)std::calloc(1, sizeof(Raster) + 64);
	r->width = w;
	r->height = h;
	r->depth = d;
	r->format = fmt;
	return r;
}
void Raster::unlock(int32) {}

namespace gl3 {
Gl3Caps gl3Caps;
int32 nativeRasterOffset = (int32)sizeof(rw::Raster);
void setMipLevels(Raster *, int32) {}
bool32 uploadKtxDirect(Raster *raster, int32 internalFormat, int32, int32,
                       int32, bool32 isCompressed, bool32,
                       int32 numLevels, const KtxMipUploadSlice *levels)
{
	if(!raster || numLevels <= 0 || !levels || !levels[0].data)
		return 0;
	g_directGpuUploads++;
	g_lastUploadedInternalFmt = internalFormat;
	g_lastUploadedCompressed = isCompressed ? 1 : 0;
	raster->width = levels[0].width;
	raster->height = levels[0].height;
	return 1;
}
} // namespace gl3
} // namespace rw

static RwTexDictionary *g_currentDict = nullptr;
struct FakeDict {
	rw::TexDictionary base;
	std::vector<RwTexture*> texs;
};

RwRaster *RwRasterCreate(RwInt32 w, RwInt32 h, RwInt32 d, RwInt32 flags) {
	return rw::Raster::create(w, h, d, flags, 0);
}
RwUInt8 *RwRasterLock(RwRaster *, RwUInt8, RwInt32) {
	static RwUInt8 dummy[256];
	return dummy;
}
RwBool RwRasterDestroy(RwRaster *r) { std::free(r); return true; }

RwTexture *RwTextureCreate(RwRaster *r) {
	RwTexture *t = (RwTexture*)std::calloc(1, sizeof(RwTexture));
	t->raster = r;
	t->refCount = 1;
	g_liveRwTextures++;
	return t;
}
RwTexture *RwTextureAddRef(RwTexture *t) { if(t) t->refCount++; return t; }
RwBool RwTextureDestroy(RwTexture *t) {
	if(!t) return true;
	if(--t->refCount <= 0){
		if(t->raster) RwRasterDestroy(t->raster);
		std::free(t);
		g_liveRwTextures--;
	}
	return true;
}
RwTexture *RwTextureSetFilterMode(RwTexture *t, RwTextureFilterMode) { return t; }
RwTexture *RwTextureSetAddressing(RwTexture *t, RwTextureAddressMode) { return t; }

RwTexDictionary *RwTexDictionaryCreate(void) {
	FakeDict *d = new FakeDict();
	std::memset(&d->base, 0, sizeof(d->base));
	return (RwTexDictionary*)d;
}
RwBool RwTexDictionaryDestroy(RwTexDictionary *dict) {
	if(!dict) return true;
	FakeDict *d = (FakeDict*)dict;
	for(size_t i = 0; i < d->texs.size(); i++)
		RwTextureDestroy(d->texs[i]);
	if(g_currentDict == dict) g_currentDict = nullptr;
	delete d;
	return true;
}
RwTexDictionary *RwTexDictionarySetCurrent(RwTexDictionary *d) { g_currentDict = d; return d; }
RwTexDictionary *RwTexDictionaryGetCurrent(void) { return g_currentDict; }
RwTexture *RwTexDictionaryAddTexture(RwTexDictionary *dict, RwTexture *tex) {
	if(dict && tex) ((FakeDict*)dict)->texs.push_back(tex);
	return tex;
}
RwTexture *RwTexDictionaryFindNamedTexture(RwTexDictionary *dict, const RwChar *name) {
	if(!dict || !name) return nullptr;
	FakeDict *d = (FakeDict*)dict;
	for(size_t i = 0; i < d->texs.size(); i++)
		if(strcasecmp(d->texs[i]->name, name) == 0)
			return d->texs[i];
	return nullptr;
}
const RwTexDictionary *RwTexDictionaryForAllTextures(const RwTexDictionary *dict, RwTextureCallBack fp, void *data) {
	if(!dict || !fp) return dict;
	const FakeDict *d = (const FakeDict*)dict;
	for(size_t i = 0; i < d->texs.size(); i++)
		if(!fp(d->texs[i], data)) break;
	return dict;
}
RwTexDictionary *RwTexDictionaryGtaStreamRead(RwStream *) {
	g_txdStreamReads++;
	return nullptr;
}

RwStream *RwStreamOpen(RwStreamType type, RwStreamAccessType, const void *pData) {
	RwStream *s = (RwStream*)std::calloc(1, sizeof(RwStream));
	if(type == rwSTREAMMEMORY && pData){
		const RwMemory *mem = (const RwMemory*)pData;
		if(mem->start && mem->length > 0)
			g_lastLoadedAtomicStreamBytes.assign(mem->start, mem->start + mem->length);
	}
	return s;
}
RwBool RwStreamFindChunk(RwStream *, RwUInt32, RwUInt32 *, RwUInt32 *) { return true; }
RwBool RwStreamClose(RwStream *s, void *) { std::free(s); return true; }

bool CFileLoader::LoadAtomicFile(RwStream *, uint32) {
	if(g_simulateLoadDelayUs > 0)
		usleep((useconds_t)g_simulateLoadDelayUs);
	// Emulate RenderWare Texture::read resolving the model's texture through findCB!
	if(rw::Texture::findCB){
		RwTexture *t = rw::Texture::findCB("road_asphalt_br");
		if(t){
			RwTextureAddRef(t); // matches vendor/librw/src/texture.cpp:338
			g_lastResolvedTex = t;
		}
		return t != nullptr;
	}
	return false;
}
bool CFileLoader::LoadClumpFile(RwStream *, uint32) { return true; }
bool CFileLoader::StartLoadClumpFile(RwStream *, uint32) { return true; }
bool CFileLoader::FinishLoadClumpFile(RwStream *, uint32) { return true; }

void DebugLogPrintf(const char *, ...) {}
void re3_log(const char *, const char *, unsigned int, const char *, const char *, ...) {}
void re3_assert(const char *, const char *, unsigned int, const char *) { std::abort(); }
FILE *_fcaseopen(const char *name, const char *mode) { return std::fopen(name, mode); }

static void Check(bool ok, const char *why)
{
	if(!ok){
		std::fprintf(stderr, "AndroidBpcArchiveIntegrationTests FAILED: %s\n", why);
		std::abort();
	}
}

static void PutU16(std::vector<uint8> &b, uint16 v)
{
	b.push_back((uint8)(v & 0xFFu));
	b.push_back((uint8)((v >> 8) & 0xFFu));
}

static void PutU32(std::vector<uint8> &b, uint32 v)
{
	PutU16(b, (uint16)(v & 0xFFFFu));
	PutU16(b, (uint16)(v >> 16));
}

static void W32At(std::vector<uint8> &b, size_t off, uint32 v)
{
	std::memcpy(&b[off], &v, 4);
}

static void WFAt(std::vector<uint8> &b, size_t off, float f)
{
	std::memcpy(&b[off], &f, 4);
}

struct ZipEntrySpec {
	std::string name;
	std::vector<uint8> bytes;
};

static std::vector<uint8> MakeZip(const std::vector<ZipEntrySpec> &entries, bool xorEncrypt)
{
	std::vector<uint8> zip;
	std::vector<uint32> localOffsets;
	for(size_t i = 0; i < entries.size(); i++){
		const ZipEntrySpec &e = entries[i];
		localOffsets.push_back((uint32)zip.size());
		PutU32(zip, 0x04034B50u);
		PutU16(zip, 20);
		PutU16(zip, 0);
		PutU16(zip, 0);
		PutU16(zip, 0);
		PutU16(zip, 0);
		PutU32(zip, 0);
		PutU32(zip, (uint32)e.bytes.size());
		PutU32(zip, (uint32)e.bytes.size());
		PutU16(zip, (uint16)e.name.size());
		PutU16(zip, 0);
		zip.insert(zip.end(), e.name.begin(), e.name.end());
		zip.insert(zip.end(), e.bytes.begin(), e.bytes.end());
	}
	uint32 cdOff = (uint32)zip.size();
	for(size_t i = 0; i < entries.size(); i++){
		const ZipEntrySpec &e = entries[i];
		PutU32(zip, 0x02014B50u);
		PutU16(zip, 20);
		PutU16(zip, 20);
		PutU16(zip, 0);
		PutU16(zip, 0);
		PutU16(zip, 0);
		PutU16(zip, 0);
		PutU32(zip, 0);
		PutU32(zip, (uint32)e.bytes.size());
		PutU32(zip, (uint32)e.bytes.size());
		PutU16(zip, (uint16)e.name.size());
		PutU16(zip, 0);
		PutU16(zip, 0);
		PutU16(zip, 0);
		PutU16(zip, 0);
		PutU32(zip, 0);
		PutU32(zip, localOffsets[i]);
		zip.insert(zip.end(), e.name.begin(), e.name.end());
	}
	uint32 cdSize = (uint32)zip.size() - cdOff;
	PutU32(zip, 0x06054B50u);
	PutU16(zip, 0);
	PutU16(zip, 0);
	PutU16(zip, (uint16)entries.size());
	PutU16(zip, (uint16)entries.size());
	PutU32(zip, cdSize);
	PutU32(zip, cdOff);
	PutU16(zip, 0);

	if(xorEncrypt)
		CustomZip::ApplyBpcXor(0, zip.data(), (uint32)zip.size());
	return zip;
}

static std::vector<uint8> Bytes(const char *s)
{
	size_t n = std::strlen(s);
	return std::vector<uint8>((const uint8*)s, (const uint8*)s + n);
}

static std::vector<uint8> MakeVcCol(const char *modelName)
{
	std::vector<uint8> b(92, 0);
	std::memcpy(b.data(), "COLL", 4);
	W32At(b, 4, 84);
	std::memcpy(b.data() + 8, modelName, std::strlen(modelName));
	WFAt(b, 32, 2.0f);
	WFAt(b, 48, -1.0f); WFAt(b, 52, -1.0f); WFAt(b, 56, -1.0f);
	WFAt(b, 60, 1.0f);  WFAt(b, 64, 1.0f);  WFAt(b, 68, 1.0f);
	return b;
}

static void TeaEncrypt8Rounds(uint32 &v0, uint32 &v1, const uint32 k[4])
{
	uint32 sum = 0;
	for(int r = 0; r < 8; r++){
		sum += 0x9E3779B9u;
		v0 += ((v1 << 4) + k[0]) ^ (v1 + sum) ^ ((v1 >> 5) + k[1]);
		v1 += ((v0 << 4) + k[2]) ^ (v0 + sum) ^ ((v0 >> 5) + k[3]);
	}
}

static std::vector<uint8> EncryptDffToMod(std::vector<uint8> dff)
{
	size_t origLen = dff.size();
	if(dff.size() < br::MOD_BLOCK)
		dff.resize(br::MOD_BLOCK, 0);

	uint32 key[4];
	for(int i = 0; i < 4; i++)
		key[i] = br::ror32(br::MOD_KEY_RAW[i] ^ 0x12913AFBu, 19);

	std::vector<uint8> mod(br::MOD_HDR + dff.size(), 0);
	W32At(mod, 0, br::MOD_MAGIC);
	W32At(mod, 4, (uint32)origLen);
	W32At(mod, 8, 1u);
	std::memcpy(&mod[br::MOD_HDR], dff.data(), dff.size());

	for(size_t off = 0; off < br::MOD_BLOCK; off += 8){
		uint32 v0 = br::rd32(&mod[br::MOD_HDR + off]);
		uint32 v1 = br::rd32(&mod[br::MOD_HDR + off + 4]);
		TeaEncrypt8Rounds(v0, v1, key);
		br::wr32(&mod[br::MOD_HDR + off], v0);
		br::wr32(&mod[br::MOD_HDR + off + 4], v1);
	}
	return mod;
}

static std::vector<uint8> MakeEncryptedMod(void)
{
	return EncryptDffToMod(br::buildDummyDff());
}

// Synthesize a BR meshOnly .mod (like nrock_ter498 / pcj600 / glendale / male01):
// numTriangles == 0 in RpGeometry's rwSTRUCT, with 2 materials and BinMeshPLG (0x050E)
// holding an empty 0-index split on material 0 + 3 valid triangle indices on material 1.
static std::vector<uint8> MakeMeshOnlyEncryptedMod(void)
{
	struct W {
		std::vector<uint8> b; std::vector<size_t> st;
		void u8(uint8 v) { b.push_back(v); }
		void u32(uint32 v) { for(int i = 0; i < 4; i++) b.push_back((uint8)(v >> (8 * i))); }
		void f32(float v) { uint32 u; std::memcpy(&u, &v, 4); u32(u); }
		void open(uint32 type) { u32(type); u32(0); u32(0x1803FFFFu); st.push_back(b.size()); }
		void close() { size_t s = st.back(); st.pop_back(); uint32 len = (uint32)(b.size() - s); std::memcpy(&b[s - 8], &len, 4); }
	} w;
	w.open(0x10); // Clump
	  w.open(0x01); w.u32(1); w.u32(0); w.u32(0); w.close();
	  w.open(0x0E); // Frame List
	    w.open(0x01); w.u32(1);
	      w.f32(1); w.f32(0); w.f32(0); w.f32(0); w.f32(1); w.f32(0); w.f32(0); w.f32(0); w.f32(1);
	      w.f32(0); w.f32(0); w.f32(0); w.u32(0xFFFFFFFFu); w.u32(0);
	    w.close();
	    w.open(0x03); w.close();
	  w.close();
	  w.open(0x1A); // Geometry List
	    w.open(0x01); w.u32(1); w.close();
	    w.open(0x0F); // Geometry
	      w.open(0x01);
	        w.u32(0x00000002u); // rpGEOMETRYPOSITIONS, !NATIVE
	        w.u32(0);           // numTriangles == 0 in rwSTRUCT!
	        w.u32(3);           // 3 vertices
	        w.u32(1);           // 1 morph target
	        w.f32(0); w.f32(0); w.f32(0); w.f32(1.0f); w.u32(1); w.u32(0);
	        w.f32(0); w.f32(0); w.f32(0);
	        w.f32(1); w.f32(0); w.f32(0);
	        w.f32(0); w.f32(1); w.f32(0);
	      w.close();
	      w.open(0x08); // Material List (2 materials)
	        w.open(0x01); w.u32(2); w.u32(0xFFFFFFFFu); w.u32(0xFFFFFFFFu); w.close();
	        for(int m = 0; m < 2; m++){
	          w.open(0x07);
	            w.open(0x01); w.u32(0); w.u8(255); w.u8(255); w.u8(255); w.u8(255); w.u32(0); w.u32(0); w.f32(1); w.f32(1); w.f32(1); w.close();
	            w.open(0x03); w.close();
	          w.close();
	        }
	      w.close();
	      w.open(0x03); // Extension with BinMeshPLG (0x050E)
	        w.open(0x050E);
	          w.u32(0); // triList
	          w.u32(2); // 2 splits: split 0 is empty (0 indices), split 1 has 3 indices
	          w.u32(3); // totalIndices = 3
	          w.u32(0); w.u32(0); // empty split for material 0
	          w.u32(3); w.u32(1); w.u32(0); w.u32(1); w.u32(2); // valid triangle for material 1
	        w.close();
	      w.close();
	    w.close();
	  w.close();
	  w.open(0x14); // Atomic
	    w.open(0x01); w.u32(0); w.u32(0); w.u32(5); w.u32(0); w.close();
	    w.open(0x03); w.close();
	  w.close();
	  w.open(0x03); w.close();
	w.close();
	return EncryptDffToMod(w.b);
}

// Synthesize a genuine 0-triangle stub .mod (98 vertices, 0 legacy tris, no BinMesh faces, like missile.mod).
static std::vector<uint8> MakeEmptyStubEncryptedMod(void)
{
	struct W {
		std::vector<uint8> b; std::vector<size_t> st;
		void u8(uint8 v) { b.push_back(v); }
		void u32(uint32 v) { for(int i = 0; i < 4; i++) b.push_back((uint8)(v >> (8 * i))); }
		void f32(float v) { uint32 u; std::memcpy(&u, &v, 4); u32(u); }
		void open(uint32 type) { u32(type); u32(0); u32(0x1803FFFFu); st.push_back(b.size()); }
		void close() { size_t s = st.back(); st.pop_back(); uint32 len = (uint32)(b.size() - s); std::memcpy(&b[s - 8], &len, 4); }
	} w;
	w.open(0x10);
	  w.open(0x01); w.u32(1); w.u32(0); w.u32(0); w.close();
	  w.open(0x0E);
	    w.open(0x01); w.u32(1);
	      w.f32(1); w.f32(0); w.f32(0); w.f32(0); w.f32(1); w.f32(0); w.f32(0); w.f32(0); w.f32(1);
	      w.f32(0); w.f32(0); w.f32(0); w.u32(0xFFFFFFFFu); w.u32(0);
	    w.close();
	    w.open(0x03); w.close();
	  w.close();
	  w.open(0x1A);
	    w.open(0x01); w.u32(1); w.close();
	    w.open(0x0F);
	      w.open(0x01);
	        w.u32(0x00000002u);
	        w.u32(0); w.u32(98); w.u32(1);
	        w.f32(0); w.f32(0); w.f32(0); w.f32(1.0f); w.u32(1); w.u32(0);
	        for(int i = 0; i < 98; i++){ w.f32(0); w.f32(0); w.f32(0); }
	      w.close();
	      w.open(0x08);
	        w.open(0x01); w.u32(1); w.u32(0xFFFFFFFFu); w.close();
	        w.open(0x07);
	          w.open(0x01); w.u32(0); w.u8(255); w.u8(255); w.u8(255); w.u8(255); w.u32(0); w.u32(0); w.f32(1); w.f32(1); w.f32(1); w.close();
	          w.open(0x03); w.close();
	        w.close();
	      w.close();
	      w.open(0x03); w.close();
	    w.close();
	  w.close();
	  w.open(0x14);
	    w.open(0x01); w.u32(0); w.u32(0); w.u32(5); w.u32(0); w.close();
	    w.open(0x03); w.close();
	  w.close();
	  w.open(0x03); w.close();
	w.close();
	return EncryptDffToMod(w.b);
}

static std::vector<uint8> MakeAstcBtx(void)
{
	static const uint8 kKtxMagic[12] = {
		0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
	};
	static const uint8 kVoidBlock[16] = {
		0xFC, 0xFD, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0x80, 0x80, 0x40, 0x40, 0x20, 0x20, 0xFF, 0xFF
	};
	std::vector<uint8> b;
	PutU32(b, 1u);
	b.insert(b.end(), kKtxMagic, kKtxMagic + 12);
	PutU32(b, 0x04030201u);
	PutU32(b, 0u);
	PutU32(b, 1u);
	PutU32(b, 0u);
	PutU32(b, 0x93D0u); // sRGB ASTC 4x4 in KTX1 -> mapped to linear 0x93B0 on GPU
	PutU32(b, 0x1908u);
	PutU32(b, 4u);
	PutU32(b, 4u);
	PutU32(b, 0u);
	PutU32(b, 0u);
	PutU32(b, 1u);
	PutU32(b, 1u);
	PutU32(b, 0u);
	PutU32(b, 16u);
	b.insert(b.end(), kVoidBlock, kVoidBlock + 16);
	return b;
}

static void WriteFileBytes(const std::string &path, const std::vector<uint8> &bytes)
{
	FILE *f = std::fopen(path.c_str(), "wb");
	Check(f != nullptr && std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size(), path.c_str());
	std::fclose(f);
}

static void CollectWorldFile(const char *path, const unsigned char *data, size_t size, void *ctx)
{
	std::map<std::string, std::string> *m = (std::map<std::string, std::string>*)ctx;
	(*m)[path] = std::string((const char*)data, size);
}

int main(void)
{
	std::memset(&rw::gl3::gl3Caps, 0, sizeof(rw::gl3::gl3Caps));
	rw::gl3::gl3Caps.gles = 1;
	rw::gl3::gl3Caps.glversion = 30;
	rw::gl3::gl3Caps.astcSupported = 1;

	char tmpTemplate[] = "/tmp/revc_android_bpc_XXXXXX";
	char *tmpRoot = mkdtemp(tmpTemplate);
	Check(tmpRoot != nullptr, "mkdtemp");

	char oldCwd[4096];
	Check(getcwd(oldCwd, sizeof(oldCwd)) != nullptr, "getcwd");
	Check(chdir(tmpRoot) == 0, "chdir tmpRoot");

	mkdir("custom", 0755);
	mkdir("custom/zip", 0755);
	mkdir("custom/zip/mesh", 0755);
	mkdir("custom/zip/textures", 0755);

	const char *cfgDat =
		"CDIMAGE mesh\\world_0.bpc\n"
		"IDE data\\maps\\city\\center.ide\n"
		"IPL data\\maps\\city\\center.ipl\n";
	const char *texdbDat =
		"TEXDB textures/Roads3-0\n";
	const char *lodsInfo = "road_br_01,lod_road_br_01\n";
	const char *centerIde = "objs\n9000, road_br_01, road_asphalt_br, 1, 200, 0\nend\n";
	const char *centerIpl = "inst\n9000, road_br_01, 0, 10.0, 20.0, 3.0, 1, 1, 1, 0, 0, 0, 1\nend\n";
	const char *pedAniDummy = "ANP3ped_test_block\0\0\0\0\0\0\0\0\0\0\x01\0\0\0\0\0\0\0";

	WriteFileBytes("custom/zip/common.bpc", MakeZip({
		{ "data/config.dat", Bytes(cfgDat) },
		{ "data/texdb.dat", Bytes(texdbDat) },
		{ "data/lods.info", Bytes(lodsInfo) },
		{ "data/maps/city/center.ide", Bytes(centerIde) },
		{ "data/maps/city/center.ipl", Bytes(centerIpl) },
		{ "anim/ped.ani", std::vector<uint8>((const uint8*)pedAniDummy, (const uint8*)pedAniDummy + 36) }
	}, true /* XOR-encrypted! */));

	WriteFileBytes("custom/zip/mesh/world_0.bpc", MakeZip({
		{ "player.mod", MakeEncryptedMod() },
		{ "road_br_01.mod", MakeEncryptedMod() },
		{ "road_br_02.mod", MakeEncryptedMod() },
		{ "road_br_03.mod", MakeEncryptedMod() },
		{ "nrock_ter498.mod", MakeMeshOnlyEncryptedMod() },
		{ "missile.mod", MakeEmptyStubEncryptedMod() },
		{ "world.col", MakeVcCol("road_br_01") }
	}, false));
	WriteFileBytes("custom/zip/mesh/unlisted.bpc", MakeZip({
		{ "secret_model.mod", MakeEncryptedMod() }
	}, false));

	WriteFileBytes("custom/zip/textures/Roads3-0.astc.bpc", MakeZip({
		{ "road_asphalt_br.btx", MakeAstcBtx() },
		{ "brsky.btx", MakeAstcBtx() }
	}, false));
	// Real-world BR companion thumbnail archive naming (<db>.astc.bpc.tmb)
	WriteFileBytes("custom/zip/textures/Roads3-0.astc.bpc.tmb", MakeZip({
		{ "road_asphalt_br.btx", MakeAstcBtx() }
	}, false));
	WriteFileBytes("custom/zip/textures/Secret.astc.bpc", MakeZip({
		{ "secret_tex.btx", MakeAstcBtx() }
	}, false));

	g_images.clear();
	g_images.push_back("models/gta3.img");
	g_images.push_back("models/txd.img");
	CTxdStore::Initialise();
	CCustomModels::Initialise();
	CCustomModels::MountImages();
	Check(CCustomModels::IsActive(), "CCustomModels active with Android .bpc layout");

	bool fromCommon = false;
	std::vector<uint8> lodBytes;
	Check(CCustomModels::ReadDataFile("data/lods.info", lodBytes, &fromCommon) && fromCommon,
		"read data/lods.info from XOR common.bpc");
	Check(std::string((const char*)lodBytes.data(), lodBytes.size()) == lodsInfo, "lods.info matches byte-for-byte");

	std::map<std::string, std::string> ides, ipls;
	CCustomModels::ForEachWorldFile(true, CollectWorldFile, &ides);
	CCustomModels::ForEachWorldFile(false, CollectWorldFile, &ipls);
	Check(ides.size() == 1 && ipls.size() == 1, "IDE and IPL read from XOR common.bpc");

	std::vector<uint8> animBytes;
	Check(CCustomModels::LoadAnimFileFromCustom("ANIM\\PED.IFP", animBytes),
		"ANIM\\PED.IFP resolved to anim/ped.ani inside XOR common.bpc");

	Check(CCustomModels::HasModelFile("road_br_01"), "manifest-listed road_br_01.mod mounted");
	Check(CCustomModels::HasModelFile("road_br_02"), "manifest-listed road_br_02.mod mounted");
	Check(!CCustomModels::HasModelFile("secret_model"), "unlisted mesh/unlisted.bpc strictly ignored");

	Check(CCustomModels::HasImageTxd("road_asphalt_br"), "manifest-listed road_asphalt_br.btx mounted");
	Check(CCustomModels::HasImageTxd("brsky"), "brsky.btx sky texture mounted");
	Check(!CCustomModels::HasImageTxd("secret_tex"), "unlisted textures/Secret.astc.bpc strictly ignored");

	// Verify .bpc collisions survive MountImages() and load cleanly into CColStore
	Check(CCustomModels::CanServe(STREAM_OFFSET_COL) && CCustomModels::Load(STREAM_OFFSET_COL),
		"CanServe/Load(STREAM_OFFSET_COL) serves city_block.col from mesh/world_0.bpc");
	Check(g_loadedCols == 1, "CColStore::LoadCol received 1 validated VC COLL block");
	std::vector<uint8> colBlock;
	Check(CCustomModels::GetCollisionBlock("road_br_01", colBlock),
		"GetCollisionBlock('road_br_01') returns per-model collision block after MountImages()");

	// Native MODELS/TXD.IMG slots (m_size != 0) must pass straight through to stock CdStream
	// in O(1) without calling CModelInfo::GetModelInfo(const char*) / FindModelForTxd!
	int32 nativeTxdSlot = CTxdStore::AddTxdSlot("hmoca");
	CStreaming::ms_aInfoForModel[STREAM_OFFSET_TXD + nativeTxdSlot].m_position = 10u + (1u << 24);
	CStreaming::ms_aInfoForModel[STREAM_OFFSET_TXD + nativeTxdSlot].m_size = 8u;
	Check(!CCustomModels::CanServe(STREAM_OFFSET_TXD + nativeTxdSlot),
		"native MODELS/TXD.IMG slot (m_size != 0) returns CanServe=false immediately");

	// Verify async streaming preparation + direct GPU .btx upload on model load WITHOUT .txd
	int32 txdSlot = CTxdStore::AddTxdSlot("roads_br_txd");
	CSimpleModelInfo *mi = CModelInfo::AddSimpleModel(100);
	mi->SetModelName("road_br_01");
	((CBaseModelInfo*)mi)->SetTexDictionary("roads_br_txd");

	// Placeholder TXD slot "rip" (m_size == 0, no rip.btx, no rip.txd) owned by second .mod
	int32 ripSlot = CTxdStore::AddTxdSlot("rip");
	CSimpleModelInfo *mi2 = CModelInfo::AddSimpleModel(101);
	mi2->SetModelName("road_br_02");
	((CBaseModelInfo*)mi2)->SetTexDictionary("rip");

	CCustomModels::Initialise(); // rebuild O(1) TXD slot index after IDE registration

	// Load TXD slots first: must create empty dictionaries without RwTexDictionaryGtaStreamRead
	Check(CCustomModels::CanServe(STREAM_OFFSET_TXD + txdSlot), "CanServe(TXD slot) true for model-owned slot");
	Check(CCustomModels::CanServe(STREAM_OFFSET_TXD + ripSlot), "CanServe('rip') true for m_size==0 placeholder slot");
	Check(CCustomModels::Load(STREAM_OFFSET_TXD + txdSlot), "Load(TXD slot) creates empty dictionary");
	Check(CCustomModels::Load(STREAM_OFFSET_TXD + ripSlot), "Load('rip') creates empty dictionary");
	Check(g_txdStreamReads == 0 && g_directGpuUploads == 0,
		"ZERO .txd stream reads and ZERO premature uploads on empty group TXD slots");
	Check(g_linearGetModelInfoByNameCalls == 0,
		"strictly ZERO linear CModelInfo::GetModelInfo(name) calls in CanServe/Load");

	// 1) Load model 100 directly (before any TexDB prefetch) so HasPreparedRawBtx is
	// deterministically false: BpcDirectTexFindCB must upload the 8x8 .astc.bpc.tmb
	// preview immediately (#1) and schedule an async in-place .astc.bpc upgrade (#2).
	Check(CCustomModels::CanServe(100), "CanServe(100) resolves road_br_01.mod");
	g_lastResolvedTex = nullptr;
	CTxdStore::SetCurrentTxd(txdSlot);
	Check(CCustomModels::Load(100), "CCustomModels::Load(100) loaded TEA .mod and resolved .btx directly");
	RwTexture *tex1 = g_lastResolvedTex;
	Check(tex1 != nullptr, "BpcDirectTexFindCB resolved road_asphalt_br texture for model 100");
	Check(g_directGpuUploads == 1, "rw::gl3::uploadKtxDirect invoked immediately for .astc.bpc.tmb preview");
	Check(g_lastUploadedCompressed == 1 && g_lastUploadedInternalFmt == 0x93B0,
		"ASTC 4x4 KTX1 uploaded directly as GL_COMPRESSED_RGBA_ASTC_4x4_KHR (0x93B0)");
	Check(g_txdStreamReads == 0, "strictly ZERO .txd conversions throughout entire model + texture load");

	// Verify RwTexture live .astc.bpc.tmb -> .astc.bpc upgrade and refCount accounting:
	// While PendingBtxUpgrade is queued, refCount is 3 (TexDB registry + material + upgrade);
	// once customTexDbWorker finishes and PumpBackgroundResults runs, the raster is
	// upgraded in place (2nd uploadKtxDirect) and refCount settles to 2 (TexDB registry + material).
	for(int spin = 0; spin < 200 && tex1->refCount > 2; spin++){
		usleep(2000);
		CCustomModels::PumpBackgroundResults();
	}
	Check(g_directGpuUploads == 2, ".astc.bpc.tmb preview uploaded immediately, then upgraded in place from .astc.bpc");
	Check(tex1->refCount == 2,
		"after live .btx upgrade, refCount==2 (1 TexDB registry + 1 material)");

	// 2) Async worker .mod preparation + second model (101) in a DIFFERENT TXD slot ("rip")
	// referencing the SAME "road_asphalt_br": must reuse the exact same RwTexture* from
	// the global TexDB registry with ZERO extra GPU uploads!
	Check(CCustomModels::CanServe(101), "CanServe(101) resolves road_br_02.mod");
	CCustomModels::PrefetchStream(101, true);
	for(int spin = 0; spin < 200 && !CCustomModels::IsStreamPrepared(101, true); spin++){
		usleep(2000);
		CCustomModels::PumpBackgroundResults();
	}
	Check(CCustomModels::IsStreamPrepared(101, true), "async TEA .mod preparation finished on worker pool");
	g_lastResolvedTex = nullptr;
	CTxdStore::SetCurrentTxd(ripSlot);
	Check(CCustomModels::Load(101), "CCustomModels::Load(101) loaded second model in 'rip' slot");
	RwTexture *tex2 = g_lastResolvedTex;
	Check(tex2 == tex1, "second model in different TXD slot ('rip') shares exact same RwTexture*");
	Check(g_directGpuUploads == 2, "ZERO duplicate GPU uploads when sharing .btx across multiple TXD slots");
	Check(tex1->refCount == 3, "shared RwTexture refCount==3 (1 TexDB registry + 2 materials)");
	Check(g_linearGetModelInfoByNameCalls == 0,
		"strictly ZERO linear CModelInfo::GetModelInfo(name) calls across entire session");

	RwTextureDestroy(tex2); // release model 101 simulated material ref -> refCount==2
	RwTextureDestroy(tex1); // release model 100 simulated material ref -> refCount==1 (TexDB registry)

	// 3) Regression test for debug-13.log SIGSEGV at librevc.so+0x292500:
	//    - Reserved "player.mod" in .bpc must NOT be indexed (MI_PLAYER = 0 stays native GTA3.IMG).
	//    - Mesh-only BR models (numTriangles==0 in rwSTRUCT, faces in BinMeshPLG 0x050E, even with
	//      an empty material split) must be accepted and have empty splits compacted away.
	//    - Genuine 0-face stubs (98 verts, 0 legacy tris, 0 BinMesh tris) must still be rejected.
	//    - Exhausting s_bpcFreezeMonitor's 8 ms per-frame budget on the same frame must NEVER block
	//      native IMG models (!CanServe, such as MI_PLAYER = 0) nor custom .bpc models inside
	//      CCustomModels::ScopedBlockingLoad (CStreaming::LoadAllRequestedModels), while still
	//      throttling non-blocking per-frame custom .bpc loads until the next frame!
	Check(!CCustomModels::HasModelFile("player"), "reserved player.mod skipped at .bpc index time");
	CSimpleModelInfo *miPlayer = CModelInfo::AddSimpleModel(0);
	miPlayer->SetModelName("player");
	((CBaseModelInfo*)miPlayer)->SetTexDictionary("hmoca");

	CSimpleModelInfo *miMeshOnly = CModelInfo::AddSimpleModel(102);
	miMeshOnly->SetModelName("nrock_ter498");
	((CBaseModelInfo*)miMeshOnly)->SetTexDictionary("roads_br_txd");

	CSimpleModelInfo *miStub = CModelInfo::AddSimpleModel(103);
	miStub->SetModelName("missile");
	((CBaseModelInfo*)miStub)->SetTexDictionary("hmoca");

	CSimpleModelInfo *miBudget = CModelInfo::AddSimpleModel(104);
	miBudget->SetModelName("road_br_03");
	((CBaseModelInfo*)miBudget)->SetTexDictionary("roads_br_txd");

	Check(!CCustomModels::CanServe(0), "MI_PLAYER (0) is not served by .bpc");
	Check(CCustomModels::CanServe(102), "meshOnly nrock_ter498.mod (0 legacy tris, BinMeshPLG faces) accepted");
	CCustomModels::PrefetchStream(103, true);
	for(int spin = 0; spin < 200 && !CCustomModels::IsStreamPrepared(103, true); spin++){
		usleep(2000);
		CCustomModels::PumpBackgroundResults();
	}
	Check(!CCustomModels::CanServe(103) && !CCustomModels::Load(103),
		"genuine 0-face missile.mod stub (98 verts, 0 tris) rejected after background validation");
	Check(CCustomModels::CanServe(104), "road_br_03.mod accepted");

	// Prepare road_br_03 on worker before budget exhaustion test
	CCustomModels::PrefetchStream(104, true);
	for(int spin = 0; spin < 200 && !CCustomModels::IsStreamPrepared(104, true); spin++){
		usleep(2000);
		CCustomModels::PumpBackgroundResults();
	}
	Check(CCustomModels::IsStreamPrepared(104, true), "road_br_03 prepared on worker");

	// Load nrock_ter498 with a 15 ms simulated main-thread conversion to exhaust the 12 ms frame budget
	// on the current CTimer::m_FrameCounter, and verify empty BinMesh split compaction!
	g_simulateLoadDelayUs = 15000;
	g_lastResolvedTex = nullptr;
	g_lastLoadedAtomicStreamBytes.clear();
	CTxdStore::SetCurrentTxd(txdSlot);
	Check(CCustomModels::Load(102), "meshOnly nrock_ter498.mod loaded cleanly");
	g_simulateLoadDelayUs = 0;
	if(g_lastResolvedTex) RwTextureDestroy(g_lastResolvedTex);
	Check(!g_lastLoadedAtomicStreamBytes.empty(), "captured normalized nrock_ter498 DFF stream");
	{
		std::string why;
		Check(br::validateClumpForGame(g_lastLoadedAtomicStreamBytes.data(),
		      g_lastLoadedAtomicStreamBytes.size(), why),
			"normalized meshOnly DFF passes validateClumpForGame");
		// Verify BinMeshPLG (0x050E) had its empty split compacted from nSplit=2 down to nSplit=1!
		bool foundCompactedBinMesh = false;
		const uint8 *d = g_lastLoadedAtomicStreamBytes.data();
		size_t n = g_lastLoadedAtomicStreamBytes.size();
		for(size_t off = 0; off + 24 <= n; off += 4){
			if(br::rd32(d + off) == 0x050Eu && br::rd32(d + off + 4) == 32u &&
			   br::rd32(d + off + 12) == 0u && br::rd32(d + off + 16) == 1u &&
			   br::rd32(d + off + 20) == 3u){
				foundCompactedBinMesh = true;
				break;
			}
		}
		Check(foundCompactedBinMesh, "empty 0-index BinMesh split compacted away (nSplit 2 -> 1)");
	}

	// Now frame budget on CTimer::GetFrameCounter() == 1 is exhausted (> 12 ms used):
	// - Native MI_PLAYER (0) MUST still return IsStreamPrepared(0, false) == true!
	// - Non-blocking per-frame custom model 104 MUST be throttled (IsStreamPrepared(104, false) == false).
	// - Advancing CTimer::SetFrameCounter() restores per-frame budget; exhausting it again and entering
	//   ScopedBlockingLoad (LoadAllRequestedModels) overrides the throttle immediately!
	Check(CCustomModels::IsStreamPrepared(0, false),
		"native MI_PLAYER (0) NEVER throttled by exhausted s_bpcFreezeMonitor frame budget");
	Check(!CCustomModels::IsStreamPrepared(104, false),
		"per-frame custom .bpc model throttled when frame budget exhausted outside LoadAllRequestedModels");
	CTimer::SetFrameCounter(CTimer::GetFrameCounter() + 1);
	Check(CCustomModels::IsStreamPrepared(104, false),
		"advancing CTimer::GetFrameCounter() restores per-frame .bpc model budget");

	// Exhaust the new frame's budget again and verify ScopedBlockingLoad overrides it on the SAME frame!
	g_simulateLoadDelayUs = 15000;
	g_lastResolvedTex = nullptr;
	CTxdStore::SetCurrentTxd(txdSlot);
	Check(CCustomModels::Load(104), "road_br_03.mod loaded cleanly");
	g_simulateLoadDelayUs = 0;
	if(g_lastResolvedTex) RwTextureDestroy(g_lastResolvedTex);
	Check(!CCustomModels::IsStreamPrepared(102, false),
		"second frame budget exhausted for non-blocking per-frame load");
	{
		CCustomModels::ScopedBlockingLoad blockingScope;
		Check(CCustomModels::IsStreamPrepared(102, false),
			"ScopedBlockingLoad overrides frame budget throttle inside LoadAllRequestedModels");
	}

	CTxdStore::RemoveTxd(ripSlot);
	CTxdStore::RemoveTxd(txdSlot);
	CModelInfo::ShutDown();
	CCustomModels::Shutdown();
	Check(g_liveRwTextures == 0, "all TexDB RwTextures and rasters cleanly destroyed on Shutdown()");
	CTxdStore::GameShutdown();
	CTxdStore::Shutdown();
	Check(chdir(oldCwd) == 0, "restore cwd");
	std::string rmCmd = std::string("rm -rf ") + tmpRoot;
	int rc = std::system(rmCmd.c_str());
	(void)rc;

	std::printf("AndroidBpcArchiveIntegrationTests: ALL PASSED\n");
	return 0;
}
