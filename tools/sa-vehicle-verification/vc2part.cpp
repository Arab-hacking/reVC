/*
 * vc2part.cpp - simulates reVC's *two-part* streaming readers (the ones the game
 * actually uses for vehicles and for big TXDs) on a given file, with asserts turned
 * into diagnostics. Purpose: find out where an SA-format asset breaks the
 * Start/Finish load path used in CStreaming::ConvertBufferToObject /
 * FinishLoadingLargeFile.
 *
 *   start : CFileLoader::StartLoadClumpFile -> RpClumpGtaStreamRead1   (src/rw/ClumpRead.cpp)
 *           CTxdStore::StartLoadTxd        -> RwTexDictionaryGtaStreamRead1 (src/rw/TexRead.cpp)
 *   finish: CFileLoader::FinishLoadClumpFile -> RpClumpGtaStreamRead2
 *           CTxdStore::FinishLoadTxd        -> RwTexDictionaryGtaStreamRead2
 *
 * Build: g++ -std=c++11 -I<librw> -I<librw>/src vc2part.cpp <lwbuild>/src/librw.a -lm -o vc2part
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <rw.h>

using namespace rw;

/* ---- diagnostics ---- */
static int gFail = 0;
#define CHECK(cond, fmt, ...) do { \
	if (!(cond)) { printf("   ** FAIL(assert-equivalent): " fmt "\n", ##__VA_ARGS__); gFail++; } \
	} while (0)
#define NOTE(fmt, ...) printf("   . " fmt "\n", ##__VA_ARGS__)

/* ---- copied from reVC src/rw/ClumpRead.cpp (logic identical, asserts -> CHECK) ---- */
struct rpGeometryList { Geometry **geometries; int32 numGeoms; };
struct rpAtomicBinary { int32 frameIndex, geomIndex, flags, unused; };

static int32 numberGeometrys;
static uint32 streamPosition;
static rpGeometryList gGeomList;
static FrameList_ gFrameList;
struct RpClumpChunkInfo { uint32 numAtomics, numLights, numCameras; };
static RpClumpChunkInfo gClumpInfo;

static bool GeometryListStreamRead1(Stream *stream, rpGeometryList *geomlist)
{
	uint32 size, version; int32 numGeoms;
	numberGeometrys = 0;
	if (!findChunk(stream, ID_STRUCT, &size, &version)) { NOTE("GL1: no STRUCT chunk"); return false; }
	CHECK(size == 4, "GL1: struct size %u != 4", size);
	if (stream->read8(&numGeoms, 4) != 4) { NOTE("GL1: numGeoms read failed"); return false; }
	numberGeometrys = numGeoms/2;
	geomlist->numGeoms = numGeoms;
	NOTE("GL1: numGeoms=%d -> reading first %d in this pass (VC splits 50/50)", numGeoms, numberGeometrys);
	if (geomlist->numGeoms > 0) {
		geomlist->geometries = (Geometry**)malloc(geomlist->numGeoms * sizeof(Geometry*));
		memset(geomlist->geometries, 0, geomlist->numGeoms * sizeof(Geometry*));
	} else geomlist->geometries = nil;
	for (int i = 0; i < numberGeometrys; i++) {
		uint32 v;
		if (!findChunk(stream, ID_GEOMETRY, nil, &v)) { NOTE("GL1: geometry[%d] chunk not found", i); return false; }
		geomlist->geometries[i] = Geometry::streamRead(stream);
		if (geomlist->geometries[i] == nil) { NOTE("GL1: geometry[%d] read FAILED (librw returned nil)", i); return false; }
	}
	return true;
}

static bool GeometryListStreamRead2(Stream *stream, rpGeometryList *geomlist)
{
	uint32 version;
	NOTE("GL2: reading remaining geometries %d..%d", numberGeometrys, geomlist->numGeoms-1);
	for (int i = numberGeometrys; i < geomlist->numGeoms; i++) {
		if (!findChunk(stream, ID_GEOMETRY, nil, &version)) { NOTE("GL2: geometry[%d] chunk not found", i); return false; }
		geomlist->geometries[i] = Geometry::streamRead(stream);
		if (geomlist->geometries[i] == nil) { NOTE("GL2: geometry[%d] read FAILED", i); return false; }
	}
	return true;
}

static void GeometryListDeinitialize(rpGeometryList *geomlist)
{
	for (int i = 0; i < geomlist->numGeoms; i++)
		if (geomlist->geometries[i]) geomlist->geometries[i]->destroy();
	if (geomlist->numGeoms) { free(geomlist->geometries); geomlist->numGeoms = 0; }
}

static Atomic *ClumpAtomicStreamRead(Stream *stream)
{
	uint32 size, version; rpAtomicBinary a;
	if (!findChunk(stream, ID_STRUCT, &size, &version)) { NOTE("ATOMIC: no STRUCT"); return nil; }
	CHECK(size <= sizeof(rpAtomicBinary), "ATOMIC: struct size %u > rpAtomicBinary %zu", size, sizeof(rpAtomicBinary));
	if (stream->read8(&a, size) != size) { NOTE("ATOMIC: struct read failed"); return nil; }
	Atomic *atomic = Atomic::create();
	if (!atomic) return nil;
	atomic->setFlags(a.flags);
	if (gFrameList.numFrames) {
		CHECK(a.frameIndex >= 0 && a.frameIndex < gFrameList.numFrames,
		      "ATOMIC: frameIndex %d out of range 0..%d", a.frameIndex, gFrameList.numFrames);
		if (a.frameIndex < 0 || a.frameIndex >= gFrameList.numFrames) { atomic->destroy(); return nil; }
		atomic->setFrame(gFrameList.frames[a.frameIndex]);
	}
	if (gGeomList.numGeoms) {
		CHECK(a.geomIndex >= 0 && a.geomIndex < gGeomList.numGeoms,
		      "ATOMIC: geomIndex %d out of range 0..%d (geoms loaded so far: %d)",
		      a.geomIndex, gGeomList.numGeoms, numberGeometrys);
		if (a.geomIndex < 0 || a.geomIndex >= gGeomList.numGeoms) { atomic->destroy(); return nil; }
		atomic->setGeometry(gGeomList.geometries[a.geomIndex], 0);
	} else {
		Geometry *geom;
		if (!findChunk(stream, ID_GEOMETRY, nil, &version)) { atomic->destroy(); return nil; }
		geom = Geometry::streamRead(stream);
		if (!geom) { atomic->destroy(); return nil; }
		atomic->setGeometry(geom, 0);
		geom->destroy();
	}
	return atomic;
}

static bool RpClumpGtaStreamRead1(Stream *stream, const char *tag)
{
	uint32 size, version;
	if (!findChunk(stream, ID_STRUCT, &size, &version)) { NOTE("%s: no clump STRUCT", tag); return false; }
	NOTE("%s: clump chunk version=0x%X structSize=%u", tag, version, size);
	if (version >= 0x33000) {
		CHECK(size == 12, "%s: clump struct size %u != 12 (SA uses 12, VC expects 12 for v>=0x33000)", tag, size);
		if (stream->read8(&gClumpInfo, 12) != 12) return false;
	} else {
		CHECK(size == 4, "%s: clump struct size %u != 4 (old format)", tag, size);
		if (stream->read8(&gClumpInfo, 4) != 4) return false;
	}
	NOTE("%s: numAtomics=%u numLights=%u numCameras=%u", tag, gClumpInfo.numAtomics, gClumpInfo.numLights, gClumpInfo.numCameras);
	if (!findChunk(stream, ID_FRAMELIST, nil, &version)) { NOTE("%s: no FRAMELIST", tag); return false; }
	NOTE("%s: framelist version=0x%X", tag, version);
	if (gFrameList.streamRead(stream) == nil) { NOTE("%s: framelist read failed", tag); return false; }
	NOTE("%s: frames=%d", tag, gFrameList.numFrames);
	if (!findChunk(stream, ID_GEOMETRYLIST, nil, &version)) {
		NOTE("%s: no GEOMETRYLIST", tag);
		gFrameList.numFrames = 0;
		return false;
	}
	if (!GeometryListStreamRead1(stream, &gGeomList)) { gFrameList.numFrames = 0; return false; }
	streamPosition = stream->tell();
	NOTE("%s: OK, part-1 stream position = %u", tag, streamPosition);
	return true;
}

static Clump *RpClumpGtaStreamRead2(Stream *stream, const char *tag)
{
	Clump *clump;
	clump = Clump::create();
	if (clump == nil) return nil;
	NOTE("%s: part2 enters at pos %u, saved pos %u -> skip %d", tag, stream->tell(), streamPosition,
	     (int)(streamPosition - stream->tell()));
	stream->seek(streamPosition - stream->tell());
	if (!GeometryListStreamRead2(stream, &gGeomList)) { GeometryListDeinitialize(&gGeomList); gFrameList.numFrames = 0; clump->destroy(); return nil; }
	clump->setFrame(gFrameList.frames[0]);
	for (uint32 i = 0; i < gClumpInfo.numAtomics; i++) {
		uint32 version;
		if (!findChunk(stream, ID_ATOMIC, nil, &version)) { NOTE("%s: atomic[%u] not found", tag, i); GeometryListDeinitialize(&gGeomList); clump->destroy(); return nil; }
		Atomic *atomic = ClumpAtomicStreamRead(stream);
		if (atomic == nil) { NOTE("%s: atomic[%u] read failed", tag, i); GeometryListDeinitialize(&gGeomList); clump->destroy(); return nil; }
		clump->addAtomic(atomic);
	}
	NOTE("%s: OK, atomics added = %u", tag, gClumpInfo.numAtomics);
	GeometryListDeinitialize(&gGeomList);
	gFrameList.numFrames = 0;
	return clump;
}

/* ---- TXD two-part reader, copied from reVC src/rw/TexRead.cpp ---- */
static int32 numberTextures = -1;
static uint32 texStreamPosition;

static Texture *RwTextureGtaStreamRead(Stream *stream)
{
	uint32 size, version; Texture *tex;
	if (!findChunk(stream, ID_TEXTURENATIVE, &size, &version)) { NOTE("TEX: no TEXTURENATIVE chunk"); return nil; }
	tex = Texture::streamReadNative(stream);
	if (!tex) NOTE("TEX: Texture::streamReadNative returned nil");
	return tex;
}

static TexDictionary *RwTexDictionaryGtaStreamRead1(Stream *stream, const char *tag)
{
	uint32 size, version; int32 numTextures; Texture *tex;
	numberTextures = 0;
	if (!findChunk(stream, ID_STRUCT, &size, &version)) { NOTE("%s: txd no STRUCT", tag); return nil; }
	CHECK(size >= 4, "%s: txd struct size %u < 4", tag, size);
	{	/* FIXED: header = {int16 numTextures; int16 deviceId} - SA deviceId != 0 */
		int16 cnt, devId;
		if (stream->read8(&cnt, 2) != 2 || stream->read8(&devId, 2) != 2) { NOTE("%s: txd header read failed", tag); return nil; }
		if (size > 4) stream->seek(size - 4);
		numTextures = cnt;
		NOTE("%s: txd header: numTextures=%d deviceId=%d", tag, cnt, devId);
	}
	TexDictionary *texDict = TexDictionary::create();
	if (texDict == nil) return nil;
	numberTextures = numTextures/2;
	NOTE("%s: numTextures=%d -> reading first %d now", tag, numTextures, numberTextures);
	while (numTextures > numberTextures) {
		numTextures--;
		tex = RwTextureGtaStreamRead(stream);
		if (tex == nil) { NOTE("%s: texture read failed in part1", tag); texDict->destroy(); return nil; }
		texDict->add(tex);
	}
	numberTextures = numTextures;
	texStreamPosition = stream->tell();
	return texDict;
}

static TexDictionary *RwTexDictionaryGtaStreamRead(Stream *stream, const char *tag)
{
	uint32 size, version; int32 numTextures;
	if (!findChunk(stream, ID_STRUCT, &size, &version)) { NOTE("%s: no STRUCT", tag); return nil; }
	CHECK(size >= 4, "%s: struct size %u < 4", tag, size);
	{
		int16 cnt, devId;
		if (stream->read8(&cnt, 2) != 2 || stream->read8(&devId, 2) != 2) { NOTE("%s: header read failed", tag); return nil; }
		if (size > 4) stream->seek(size - 4);
		numTextures = cnt;
		NOTE("%s (single pass): numTextures=%d deviceId=%d", tag, cnt, devId);
	}
	TexDictionary *texDict = TexDictionary::create();
	if (texDict == nil) return nil;
	while (numTextures--) {
		Texture *tex = RwTextureGtaStreamRead(stream);
		if (tex == nil) { NOTE("%s: texture read failed", tag); texDict->destroy(); return nil; }
		texDict->add(tex);
	}
	NOTE("%s: OK single-pass", tag);
	return texDict;
}

static TexDictionary *RwTexDictionaryGtaStreamRead2(Stream *stream, TexDictionary *texDict, const char *tag)
{
	Texture *tex;
	NOTE("%s: part2 pos %u saved %u -> skip %d", tag, stream->tell(), texStreamPosition,
	     (int)(texStreamPosition - stream->tell()));
	stream->seek(texStreamPosition - stream->tell());
	while (numberTextures--) {
		tex = RwTextureGtaStreamRead(stream);
		if (tex == nil) { NOTE("%s: texture read failed in part2", tag); texDict->destroy(); return nil; }
		texDict->add(tex);
	}
	NOTE("%s: OK txd loaded", tag);
	return texDict;
}

/* ---- reVC plugin registration (NodeName on Frame) - copied from satest.cpp ---- */
#define VENDOR_ROCKSTAR 0x0253F2
#define MAKECHUNKID(vendor, id) (((vendor & 0xFFFFFF) << 8) | (id & 0xFF))
enum { ID_NODENAME = MAKECHUNKID(VENDOR_ROCKSTAR, 0xFE) };
static int32 gNodeNameOffset = -1;
static void *NodeNameCtor(void *object, int32 offset, int32) { if (gNodeNameOffset > 0) *(char*)((char*)object + offset) = '\0'; return object; }
static void *NodeNameDtor(void *, int32, int32) { return nil; }
static void *NodeNameCopy(void *dst, void *src, int32 offset, int32) { strncpy((char*)((char*)dst + offset), (char*)((char*)src + offset), 23); return nil; }
static Stream *NodeNameRead(Stream *s, int32 length, void *object, int32 offset, int32) {
	if (length > 23) length = 23;
	s->read8((char*)object + offset, length);
	((char*)object)[offset + length] = '\0';
	return s;
}
static Stream *NodeNameWrite(Stream *s, int32 length, void *object, int32 offset, int32) { s->write8((char*)object + offset, length); return s; }
static int32 NodeNameSize(void *object, int32 offset, int32) { return (int32)strlen((char*)object + offset); }
static const char *FrameName(Frame *f) { return (gNodeNameOffset < 0 || f == nil) ? "<nil>" : (char*)f + gNodeNameOffset; }

static uint8 *LoadFilePadded(const char *path, uint32 *outSize, uint32 *outRaw)
{
	FILE *f = fopen(path, "rb");
	if (!f) { printf("cannot open %s\n", path); exit(1); }
	fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
	uint32 pad = (n + 2047) & ~2047;           /* the game reads whole CD sectors */
	uint8 *buf = (uint8*)calloc(1, pad);       /* padding zeros, exactly like the .img entry */
	if (fread(buf, 1, n, f) != (size_t)n) { printf("short read\n"); exit(1); }
	fclose(f);
	*outSize = pad; *outRaw = (uint32)n;
	return buf;
}

int main(int argc, char **argv)
{
	if (argc < 3) { printf("usage: vc2part <file.dff|file.txd> dff|txd|txd1\n"); return 1; }
	const char *path = argv[1];
	bool isTxd = strcmp(argv[2], "txd") == 0 || strcmp(argv[2], "txd1") == 0;
	bool singlePass = strcmp(argv[2], "txd1") == 0;

	if (!Engine::init()) { printf("Engine::init failed\n"); return 1; }
	if (!Engine::open(nil)) { printf("Engine::open failed\n"); return 1; }
	if (!Engine::start()) { printf("Engine::start failed\n"); return 1; }
	gNodeNameOffset = Frame::registerPlugin(24, ID_NODENAME, NodeNameCtor, NodeNameDtor, NodeNameCopy);
	Frame::registerPluginStream(ID_NODENAME, NodeNameRead, NodeNameWrite, NodeNameSize);
	if (gNodeNameOffset <= 0) { printf("NodeName plugin registration failed\n"); return 1; }

	uint32 size, raw; uint8 *buf = LoadFilePadded(path, &size, &raw);
	printf("== %s: raw %u bytes, sector-padded %u bytes (cdSize=%u sectors) ==\n", path, raw, size, size/2048);
	printf("-- PASS 1 (StartLoad...) --\n");

	StreamMemory mem1; mem1.open(buf, size);
	if (singlePass) {
		if (!findChunk(&mem1, ID_TEXDICTIONARY, nil, nil)) { printf("no TEXDICTIONARY chunk\n"); return 1; }
		printf("-- SINGLE-PASS (LoadTxd, small txds) --\n");
		TexDictionary *td = RwTexDictionaryGtaStreamRead(&mem1, "TXD");
		if (td) { int n = 0; FORLIST(lnk, td->textures) { Texture *t = LLLinkGetData(lnk, Texture, inDict); n++; NOTE("texture %d: '%s' %dx%d", n-1, t->name, t->raster ? t->raster->width : -1, t->raster ? t->raster->height : -1); } printf("-- result: SUCCESS, textures=%d, failures=%d --\n", n, gFail); td->destroy(); return 0; }
		printf("-- result: FAILURE, failures=%d --\n", gFail); return 3;
	}

	bool ok1;
	if (!isTxd) {
		if (!findChunk(&mem1, ID_CLUMP, nil, nil)) { printf("no CLUMP chunk found\n"); return 1; }
		ok1 = RpClumpGtaStreamRead1(&mem1, "DFF");
	} else {
		if (!findChunk(&mem1, ID_TEXDICTIONARY, nil, nil)) { printf("no TEXDICTIONARY chunk\n"); return 1; }
		ok1 = RwTexDictionaryGtaStreamRead1(&mem1, "TXD") != nil;
	}
	printf("-- P1 result: %s  (failures so far: %d) --\n", ok1 ? "SUCCESS" : "FAILURE", gFail);
	if (!ok1) { printf("\n=> the game would call RemoveModel+ReRequestModel (retry) and re-request this model.\n"); return 2; }

	printf("-- PASS 2 (FinishLoad...) over a fresh read of the same entry --\n");
	StreamMemory mem2; mem2.open(buf, size);
	int ret = 0;
	if (!isTxd) {
		Clump *clump = RpClumpGtaStreamRead2(&mem2, "DFF");
		if (clump) {
			int n = 0;
			FORLIST(lnk, clump->atomics) { Atomic *a = Atomic::fromClump(lnk); n++; NOTE("atomic %d: frame='%s' tris=%d verts=%d", n-1, FrameName(a->getFrame()), a->geometry ? a->geometry->numTriangles : -1, a->geometry ? a->geometry->numVertices : -1); }
			printf("-- P2 result: SUCCESS, atomics=%d --\n", n);
			clump->destroy();
		} else { printf("-- P2 result: FAILURE --\n"); ret = 3; }
	} else {
		/* re-create part1 dict, then part2 on the fresh stream (exactly like the game) */
		StreamMemory mem1b; mem1b.open(buf, size);
		findChunk(&mem1b, ID_TEXDICTIONARY, nil, nil);
		TexDictionary *td = RwTexDictionaryGtaStreamRead1(&mem1b, "TXD");
		if (td) {
			TexDictionary *td2 = RwTexDictionaryGtaStreamRead2(&mem2, td, "TXD");
			if (td2) { int n = 0; FORLIST(lnk, td2->textures) { Texture *t = LLLinkGetData(lnk, Texture, inDict); n++; NOTE("texture %d: '%s' %dx%d", n-1, t->name, t->raster ? t->raster->width : -1, t->raster ? t->raster->height : -1); } printf("-- P2 result: SUCCESS, textures=%d --\n", n); td2->destroy(); }
			else { printf("-- P2 result: FAILURE --\n"); ret = 3; }
		} else { printf("-- P2 skipped (part1 failed) --\n"); ret = 3; }
	}
	printf("== total assert-equivalent failures: %d ==\n", gFail);
	return ret;
}
