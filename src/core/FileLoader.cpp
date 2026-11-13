#include "common.h"
#include <ctype.h>
#include "main.h"

#include "General.h"
#include "Quaternion.h"
#include "ModelInfo.h"
#include "ModelLoadDispatch.h"
#include "ModelIndices.h"
#include "TempColModels.h"
#include "VisibilityPlugins.h"
#include "FileMgr.h"
#include "HandlingMgr.h"
#include "CarCtrl.h"
#include "PedType.h"
#include "AnimManager.h"
#include "Game.h"
#include "NativeMapOptions.h"
#include "RwHelper.h"
#include "NodeName.h"
#include "TxdStore.h"
#include "PathFind.h"
#include "ObjectData.h"
#include "DummyObject.h"
#include "World.h"
#include "Zones.h"
#include "ZoneCull.h"
#include "CdStream.h"
#include <vector>
#include "FileLoader.h"
#ifdef CUSTOM_MODELS
#include "CustomModels.h"
#include "CustomWorld.h"
#ifdef LIBRW
#include "SAStaticModel.h"
#endif
#endif
#include "MemoryHeap.h"
#include "Streaming.h"
#include "ColStore.h"
#include "SACol3.h"
#include "VehicleModelInfo.h"
#include "Occlusion.h"
#include "CustomSky.h"
#include "Coronas.h"
#include "Shadows.h"
#ifdef ANDROID
#include "Pools.h"
#include "Camera.h"
#endif

char CFileLoader::ms_line[256];

const char*
GetFilename(const char *filename)
{
	char *s = strrchr((char*)filename, '\\');
	return s ? s+1 : filename;
}

void
LoadingScreenLoadingFile(const char *filename)
{
	sprintf(gString, "Loading %s", GetFilename(filename));
	LoadingScreen("Loading the Game", gString, nil);
}

void
CFileLoader::LoadLevel(const char *filename)
{
	int fd;
	RwTexDictionary *savedTxd;
	bool objectsLoaded;
	char *line;
	char txdname[64];

	savedTxd = RwTexDictionaryGetCurrent();
	objectsLoaded = false;
	if(savedTxd == nil){
		savedTxd = RwTexDictionaryCreate();
		RwTexDictionarySetCurrent(savedTxd);
	}
	fd = CFileMgr::OpenFile(filename, "r");
	assert(fd > 0);

	for(line = LoadLine(fd); line; line = LoadLine(fd)){
		if(*line == '#')
			continue;

		if(strncmp(line, "EXIT", 4) == 0)
			break;

		if(strncmp(line, "IMAGEPATH", 9) == 0){
			RwImageSetPath(line + 10);
		}else if(strncmp(line, "TEXDICTION", 10) == 0){
			PUSH_MEMID(MEMID_TEXTURES);
			strcpy(txdname, line+11);
			LoadingScreenLoadingFile(txdname);
			RwTexDictionary *txd = LoadTexDictionary(txdname);
			AddTexDictionaries(savedTxd, txd);
			RwTexDictionaryDestroy(txd);
			POP_MEMID();
		}else if(strncmp(line, "COLFILE", 7) == 0){
			LoadingScreenLoadingFile(line+10);
			LoadCollisionFile(line+10, 0);
		}else if(strncmp(line, "MODELFILE", 9) == 0){
			LoadingScreenLoadingFile(line + 10);
			LoadModelFile(line + 10);
		}else if(strncmp(line, "HIERFILE", 8) == 0){
			LoadingScreenLoadingFile(line + 9);
			LoadClumpFile(line + 9);
		}else if(strncmp(line, "IDE", 3) == 0){
			if(!CGame::bLoadViceCityMap && IsNativeVCMapFile(line + 4))
				continue;
			LoadingScreenLoadingFile(line + 4);
			LoadObjectTypes(line + 4);
		}else if(strncmp(line, "IPL", 3) == 0){
			if(!objectsLoaded){
#ifdef CUSTOM_MODELS
				// Stock IDEs/CDIMAGE records have been read. Mount the user's
				// custom/img/*.img now (including collision-only IMGs), before
				// native directory indexing; loose and common.zip IDEs follow.
				CCustomModels::MountImages();
				CCustomWorld::LoadIde();
#endif
				CCustomSky::RegisterModels();
				LoadingScreenLoadingFile("Collision");
				PUSH_MEMID(MEMID_WORLD);
				CObjectData::Initialise("DATA\\OBJECT.DAT");
				CStreaming::Init();
#ifdef CUSTOM_MODELS
				CCustomModels::EnsureVehicleTxdLoaded();
#endif
				CCustomSky::InitAfterStreaming();
				POP_MEMID();
				PUSH_MEMID(MEMID_COLLISION);
				CColStore::LoadAllCollision();
#ifdef CUSTOM_MODELS
				CCustomWorld::LoadCollisions();
#endif
				POP_MEMID();
				for(int i = 0; i < MODELINFOSIZE; i++)
					if(CModelInfo::GetModelInfo(i))
						CModelInfo::GetModelInfo(i)->ConvertAnimFileIndex();
				objectsLoaded = true;
			}
			// Keep the first-IPL initialization above even when the island is
			// disabled: the custom IDEs, streaming and collisions need it.
			if(!CGame::bLoadViceCityMap && IsNativeVCMapFile(line + 4))
				continue;
#if defined(ANDROID) && defined(CUSTOM_MODELS)
			if(CCustomModels::ReplacesNativeWorldMap()){
				const char *iplPath = line + 4;
				while(*iplPath == ' ' || *iplPath == '\t') iplPath++;
				if(IsNativeVCMapFile(iplPath) ||
				   CGeneral::faststricmp(iplPath, "DATA\\occlu.ipl") == 0 ||
				   CGeneral::faststricmp(iplPath, "DATA/occlu.ipl") == 0)
					continue;
			}
#endif
			PUSH_MEMID(MEMID_WORLD);
			LoadingScreenLoadingFile(line + 4);
			LoadScene(line + 4);
			POP_MEMID();
		}else if(strncmp(line, "SPLASH", 6) == 0){
#ifndef DISABLE_LOADING_SCREEN
			LoadSplash(GetRandomSplashScreen());
#endif
#ifndef GTA_PS2
		}else if(strncmp(line, "CDIMAGE", 7) == 0){
			if(!CdStreamAddImage(line + 8))
				REVC_LOG_ERROR("CDIMAGE could not be mounted: %s", line + 8);
#endif
		}
	}

	CFileMgr::CloseFile(fd);
	RwTexDictionarySetCurrent(savedTxd);

	int i;
	for(i = 1; i < COLSTORESIZE; i++)
		if(CColStore::GetSlot(i))
			CColStore::GetBoundingBox(i).Grow(120.0f);
	CWorld::RepositionCertainDynamicObjects();
	CColStore::RemoveAllCollision();
}

char*
CFileLoader::LoadLine(int fd)
{
	int i;
	char *line;

	if(CFileMgr::ReadLine(fd, ms_line, 256) == false)
		return nil;
	for(i = 0; ms_line[i] != '\0'; i++)
		if(ms_line[i] < ' ' || ms_line[i] == ',')
			ms_line[i] = ' ';
	for(line = ms_line; *line <= ' ' && *line != '\0'; line++);
	return line;
}

RwTexDictionary*
CFileLoader::LoadTexDictionary(const char *filename)
{
	RwTexDictionary *txd;
	RwStream *stream;

	txd = nil;
	stream = RwStreamOpen(rwSTREAMFILENAME, rwSTREAMREAD, filename);
	debug("Loading texture dictionary file %s\n", filename);
	if(stream){
		if(RwStreamFindChunk(stream, rwID_TEXDICTIONARY, nil, nil))
			txd = RwTexDictionaryGtaStreamRead(stream);
		RwStreamClose(stream, nil);
	}
	if(txd == nil)
		txd = RwTexDictionaryCreate();
	return txd;
}

struct ColHeader
{
	uint32 ident;
	uint32 size;
};

void
CFileLoader::LoadCollisionFile(const char *filename, uint8 colSlot)
{
	int fd;
	char modelname[24];
	CBaseModelInfo *mi;
	ColHeader header;
	// Per-file tally. A .col whose every record parses to no geometry is a
	// truncated or damaged file, and that is a different problem from a few
	// odd records inside an otherwise good one - the totals alone cannot tell
	// them apart.
	unsigned records = 0, noInfo = 0;
	size_t bytesWalked = 0;

	PUSH_MEMID(MEMID_COLLISION);

#ifdef CUSTOM_MODELS
	// holds the collision of one model when it comes from the custom folder
	std::vector<uint8> customColBlock;
#endif

	debug("Loading collision file %s\n", filename);
	fd = CFileMgr::OpenFile(filename, "rb");
	assert(fd > 0);

	while(CFileMgr::Read(fd, (char*)&header, sizeof(header))){
		// The record size comes straight from the file and the read lands in a
		// fixed 55 000-byte buffer, so an unchecked size writes past it: a .col
		// whose record claims to be larger than the buffer overruns the global,
		// and a body shorter than the 24-byte name below makes the copy read
		// whatever the previous record left behind. The tag was only an assert,
		// which is compiled out in release, so a corrupt or non-collision file
		// was walked to the end. Chapter 3 walks records by declared length, so
		// a record that does not fit is a truncated stream: stop there.
		if(header.ident != 'LLOC'){
			debug("collision file %s: bad record tag %08x, stopping\n",
			      filename, (unsigned)header.ident);
			break;
		}
		if(header.size < 24 || header.size > sizeof(work_buff)){
			debug("collision file %s: record size %u out of range, stopping\n",
			      filename, (unsigned)header.size);
			break;
		}
		if(CFileMgr::Read(fd, (char*)work_buff, header.size) != header.size){
			debug("collision file %s: truncated record, stopping\n", filename);
			break;
		}
		memcpy(modelname, work_buff, 24);

#ifdef CUSTOM_MODELS
		// the custom folder wins over the game's own collision file
		uint8 *colbody = work_buff;
		size_t colbodysize = header.size;
		if(CCustomModels::GetCollisionBlock(modelname, customColBlock)){
			colbody = customColBlock.data();
			colbodysize = customColBlock.size();
		}
#else
		uint8 *colbody = work_buff;
		size_t colbodysize = header.size;
#endif

		records++;
		bytesWalked += 8 + (size_t)header.size;
		mi = CModelInfo::GetModelInfo(modelname, nil);
		if(mi){
			// Read into a scratch model first. A record can be long enough to
			// pass every bounds check and still carry no geometry at all (a
			// header with five zero counters is the size the log showed for
			// every GENERIC.COL entry). Installing that empty model is worse
			// than installing nothing: GetColModel() stops being nil, so the
			// SA-static fallback that builds a box from the mesh never runs and
			// the model is drawn with nothing to stand on.
			//
			// The native model is left untouched in that case - it keeps the
			// box CTempColModels gave it at IDE load - and the name is logged,
			// because a silent drop here is the "collision is missing" symptom.
			CColModel parsed;
			parsed.level = colSlot;
			LoadCollisionModel(colbody+24, colbodysize > 24 ? colbodysize-24 : 0, parsed, modelname);
			bool empty = parsed.numSpheres == 0 && parsed.numBoxes == 0 && parsed.numTriangles == 0;
			// Drop the scratch copy's volumes now; assigning the model below
			// allocates its own, so nothing would free these.
			parsed.RemoveCollisionVolumes();
			if(empty){
				debug("colmodel %s: record has no geometry, keeping the model's own collision\n",
				      modelname);
			}else if(mi->GetColModel() && mi->DoesOwnColModel()){
				LoadCollisionModel(colbody+24, colbodysize > 24 ? colbodysize-24 : 0, *mi->GetColModel(), modelname);
			}else{
				CColModel *model = new CColModel;
				model->level = colSlot;
				LoadCollisionModel(colbody+24, colbodysize > 24 ? colbodysize-24 : 0, *model, modelname);
				mi->SetColModel(model, true);
			}
		}else{
			noInfo++;
			debug("colmodel %s can't find a modelinfo\n", modelname);
		}
	}

	// Record count per file. A .col whose records all parse to no geometry is a
	// damaged or truncated file, and that is a different problem from a few odd
	// records inside an otherwise good one - the totals alone cannot tell them
	// apart. The count is what shows it: GENERIC.COL holds thousands of models,
	// so a count far below that is the file, not the parser.
	debug("collision file %s: %u record(s), %u without a modelinfo, %u bytes walked\n",
	      filename, records, noInfo, (unsigned)bytesWalked);

	CFileMgr::CloseFile(fd);

	POP_MEMID();
}


#ifdef SA_VEHICLE_MODELS
// GTA:SA keeps the collision of a vehicle inside its DFF: the clump extension
// (rwID_EXTENSION) holds a Rockstar chunk ("CollisionModel") with a COL3
// collision model. VC instead keeps vehicle collision in models/coll/*.col, so
// without this an SA car would use the collision of the VC vehicle whose model
// id it replaces - the car then floats above the ground or sinks into it,
// because the volume does not match the mesh.
#define SA_RW_COLLISIONMODEL_ID MAKECHUNKID(0x0253F2, 0xFA)

// SA keeps the collision inside the clump, after the atomics. The two part
// model reader does not stop there (the atomics are the last thing it reads),
// so the position of the clump body is remembered in StartLoadClumpFile and
// the chunks are walked from there once the model is complete. Loading a model
// is a strictly sequential, non reentrant operation, so file statics are fine.
static uint32 saClumpBodyPos;
static uint32 saClumpBodySize;
static bool saClumpBodyKnown;

// Walks the chunk list from the current position up to  end looking for the
// SA collision chunk. Extension chunks are entered (that is where SA puts it)
// and the stream is left at the body of the chunk that was found, whose size
// is returned; 0 means the model has no collision.
static uint32
FindSAColChunk(RwStream *stream, uint32 end, int depth)
{
	uint32 header[3];
	uint32 type, size, body, bodyEnd, found;

	if(depth < 0)
		return 0;
	while(STREAMPOS(stream) + 12 <= end){
		if(RwStreamRead(stream, header, 12) != 12)
			return 0;
		type = header[0];
		size = header[1];
		body = STREAMPOS(stream);
		bodyEnd = body + size;
		if(size > end || bodyEnd > end)	// corrupt data, do not walk outside
			return 0;
		if(type == SA_RW_COLLISIONMODEL_ID)
			return size;
		if(type == rwID_EXTENSION){
			found = FindSAColChunk(stream, bodyEnd, depth-1);
			if(found)
				return found;
		}
		stream->seek(bodyEnd, 0);
	}
	return 0;
}

// converts SA surface ids to something VC understands; SA has far more
// materials than VC's table, and an id outside of it would be undefined.
// Everything unknown becomes car panel, which is what the body of a VC vehicle
// uses and what gives the correct friction/sound on a vehicle body.
static uint8
SASurfaceToVC(uint8 surface)
{
	if(surface > SURFACE_CONCRETE_BEACH)
		return SURFACE_CAR_PANEL;
	return surface;
}

static void
ConvertSAColModel(const SACol3 &sa, CColModel &model)
{
	uint32 i;

	model.boundingSphere.Set(sa.boundingRadius, *(const CVector*)sa.boundingCenter);
	model.boundingBox.Set(*(const CVector*)sa.boundingMin, *(const CVector*)sa.boundingMax);

	model.numSpheres = sa.numSpheres;
	if(sa.numSpheres){
		model.spheres = (CColSphere*)RwMalloc(sa.numSpheres*sizeof(CColSphere));
		REGISTER_MEMPTR(&model.spheres);
		for(i = 0; i < sa.numSpheres; i++){
			const uint8 *p = sa.spheres + i*SACOL3_SPHERE_SIZE;
			model.spheres[i].Set(*(const float*)(p+12), *(const CVector*)p,
				SASurfaceToVC(p[16]), p[17]);
		}
	}else
		model.spheres = nil;

	model.numBoxes = sa.numBoxes;
	if(sa.numBoxes){
		model.boxes = (CColBox*)RwMalloc(sa.numBoxes*sizeof(CColBox));
		REGISTER_MEMPTR(&model.boxes);
		for(i = 0; i < sa.numBoxes; i++){
			const uint8 *p = sa.boxes + i*SACOL3_BOX_SIZE;
			model.boxes[i].Set(*(const CVector*)p, *(const CVector*)(p+12),
				SASurfaceToVC(p[24]), p[25]);
		}
	}else
		model.boxes = nil;

	model.numLines = 0;
	model.lines = nil;

	if(sa.numVerts){
		model.vertices = (CompressedVector*)RwMalloc(sa.numVerts*sizeof(CompressedVector));
		REGISTER_MEMPTR(&model.vertices);
		for(i = 0; i < sa.numVerts; i++){
			const int16 *v = sa.verts + i*3;
			model.vertices[i].Set(v[0]/128.0f, v[1]/128.0f, v[2]/128.0f);
		}
	}else
		model.vertices = nil;

	model.numTriangles = sa.numFaces;
	if(sa.numFaces){
		model.triangles = (CColTriangle*)RwMalloc(sa.numFaces*sizeof(CColTriangle));
		REGISTER_MEMPTR(&model.triangles);
		for(i = 0; i < sa.numFaces; i++){
			const uint8 *p = sa.faces + i*SACOL3_FACE_SIZE;
			model.triangles[i].Set(*(const uint16*)p, *(const uint16*)(p+2),
				*(const uint16*)(p+4), SASurfaceToVC(p[6]));
		}
	}else
		model.triangles = nil;
}

bool
CFileLoader::LoadSAVehicleColModel(RwStream *stream, uint32 id, uint32 colSize)
{
	CBaseModelInfo *mi;
	CColModel *col;
	uint8 *buf;
	SACol3 sa;

	mi = CModelInfo::GetModelInfo(id);
	if(mi == nil || mi->GetModelType() != MITYPE_VEHICLE)
		return false;
	if(colSize < 0x78 || colSize > 0x10000)
		return false;
	buf = (uint8*)RwMalloc(colSize);
	if(buf == nil)
		return false;
	if(RwStreamRead(stream, buf, colSize) != colSize){
		RwFree(buf);
		return false;
	}
	if(!SACol3Parse(buf, colSize, sa)){
		RwFree(buf);
		return false;
	}
	RwFree(buf);
	col = new CColModel;
	ConvertSAColModel(sa, *col);
	((CVehicleModelInfo*)mi)->SetEmbeddedColModel(col);
	debug("SA collision model loaded from %s.dff: %d spheres, %d boxes, %d triangles\n",
		mi->GetModelName(), col->numSpheres, col->numBoxes, col->numTriangles);
	return true;
}
#endif

bool
CFileLoader::LoadCollisionFileFirstTime(uint8 *buffer, uint32 size, uint8 colSlot)
{
	uint32 modelsize;
	char modelname[24];
	CBaseModelInfo *mi;
	ColHeader *header;
	int modelIndex;

	while(size > 8){
		header = (ColHeader*)buffer;
		modelsize = header->size;
		if(header->ident != 'LLOC')
			return size-8 < CDSTREAM_SECTOR_SIZE;
		// The block layout is tag(4) + size(4) + name(24) + body, and size counts
		// everything after the first 8 bytes. A size smaller than the 24-byte
		// name makes the copy below wrap around and overwrite the whole
		// collision model with garbage; a size larger than the buffer reads past
		// the end. Chapter 3 of the collision analysis walks these records by
		// declared length, so a record that does not fit is a corrupt stream:
		// stop there instead of reading on.
		if(modelsize < 24 || modelsize > size - 8){
			debug("colmodel stream truncated: record size %u, %u bytes left\n",
			      (unsigned)modelsize, (unsigned)size);
			return false;
		}
		memcpy(modelname, buffer+8, 24);
		memcpy(work_buff, buffer+32, modelsize-24);
		size_t workbuffsize = modelsize - 24;
		size -= 32 + (modelsize-24);
		buffer += 32 + (modelsize-24);
		// 15 KB was below what a legal model can weigh: 1600 triangles (the
		// client's MAXNUMTRIS) plus their vertices is ~25 KB, so ordinary large
		// models were reported as huge. The client bounds the collision buffer
		// at 0x001B7740 (1 800 000) bytes, so use that.
		if(modelsize > 1800000)
			debug("colmodel %s is huge, size %d\n", modelname, modelsize);

		mi = CModelInfo::GetModelInfo(modelname, &modelIndex);
#ifdef SA_VEHICLE_MODELS
		if(mi && mi->GetModelType() == MITYPE_VEHICLE &&
		   ((CVehicleModelInfo*)mi)->HasEmbeddedColModel()){
			debug("colmodel %s: collision is inside the DFF, keeping it\n", modelname);
			mi = nil;
		}
#endif
		if(mi){
			CColStore::IncludeModelIndex(colSlot, modelIndex);
			CColModel *model = new CColModel;
			model->level = colSlot;
			LoadCollisionModel(work_buff, workbuffsize, *model, modelname);
			mi->SetColModel(model, true);
		}else{
			debug("colmodel %s can't find a modelinfo\n", modelname);
		}
	}
	return true;
}

bool
CFileLoader::LoadCollisionFile(uint8 *buffer, uint32 size, uint8 colSlot)
{
	uint32 modelsize;
	char modelname[24];
	CBaseModelInfo *mi;
	ColHeader *header;

	while(size > 8){
		header = (ColHeader*)buffer;
		modelsize = header->size;
		if(header->ident != 'LLOC')
			return size-8 < CDSTREAM_SECTOR_SIZE;
		// The block layout is tag(4) + size(4) + name(24) + body, and size counts
		// everything after the first 8 bytes. A size smaller than the 24-byte
		// name makes the copy below wrap around and overwrite the whole
		// collision model with garbage; a size larger than the buffer reads past
		// the end. Chapter 3 of the collision analysis walks these records by
		// declared length, so a record that does not fit is a corrupt stream:
		// stop there instead of reading on.
		if(modelsize < 24 || modelsize > size - 8){
			debug("colmodel stream truncated: record size %u, %u bytes left\n",
			      (unsigned)modelsize, (unsigned)size);
			return false;
		}
		memcpy(modelname, buffer+8, 24);
		memcpy(work_buff, buffer+32, modelsize-24);
		size_t workbuffsize = modelsize - 24;
		size -= 32 + (modelsize-24);
		buffer += 32 + (modelsize-24);
		// 15 KB was below what a legal model can weigh: 1600 triangles (the
		// client's MAXNUMTRIS) plus their vertices is ~25 KB, so ordinary large
		// models were reported as huge. The client bounds the collision buffer
		// at 0x001B7740 (1 800 000) bytes, so use that.
		if(modelsize > 1800000)
			debug("colmodel %s is huge, size %d\n", modelname, modelsize);

		mi = CModelInfo::GetModelInfo(modelname, CColStore::GetSlot(colSlot)->minIndex, CColStore::GetSlot(colSlot)->maxIndex);
#ifdef SA_VEHICLE_MODELS
		if(mi && mi->GetModelType() == MITYPE_VEHICLE &&
		   ((CVehicleModelInfo*)mi)->HasEmbeddedColModel()){
			debug("colmodel %s: collision is inside the DFF, keeping it\n", modelname);
			mi = nil;
		}
#endif
		if(mi){
			if(mi->GetColModel()){
				LoadCollisionModel(work_buff, workbuffsize, *mi->GetColModel(), modelname);
			}else{
				CColModel *model = new CColModel;
				model->level = colSlot;
				LoadCollisionModel(work_buff, workbuffsize, *model, modelname);
				mi->SetColModel(model, true);
			}
		}else{
			debug("colmodel %s can't find a modelinfo\n", modelname);
		}
	}
	return true;
}

// Reading a .col record.
//
// The five section counts are read with a signed halfword load, exactly as the
// client's unpacker reads them (`ldrsh` at 0xe04be8 and the counters that
// follow), and each one advances the read pointer by count*elementSize. The
// client trusts these numbers; a record the game wrote always has them right.
//
// The port also reads records produced by other tools, so it cannot assume that.
// An earlier attempt clamped the counts, which was wrong and crashed: the clamp
// changed how far the pointer moved, so every following section was read from
// the wrong place. A model with 48 boxes got 32 boxes read, the vertex counter
// then landed on box data and came out negative (clamped to zero) while the
// triangle counter stayed positive - the model ended up with triangles indexing
// a vertex array that was never allocated, and dereferenced null the first time
// it was used.
//
// The numbers are therefore taken at face value. What is checked is that they
// fit in the record that is actually there: the caller passes the record size,
// each section is bounds-checked against it before anything is written, and the
// first section that does not fit ends the read. Nothing is clamped and nothing
// shifts.

// A .col record as the loader walks it. `size` is the number of body bytes
// available after the 32-byte header (name included).
class ColRecord
{
public:
	uint8 *p;
	size_t left;
	const char *name;

	ColRecord(uint8 *buf, size_t size, const char *n) : p(buf), left(size), name(n) {}

	bool Has(size_t n) const { return left >= n; }

	int32 ReadCount(void)
	{
		if(!Has(4)) return -1;
		int32 v = *(int16 *)p;
		p += 4;
		left -= 4;
		return v;
	}

	// Checks a count against what is actually left. A negative count, or a
	// section bigger than the record, means the record is not what it claims.
	bool SectionFits(int32 count, size_t elemSize, const char *what) const
	{
		if(count < 0){
			debug("colmodel %s: negative %s count %d\n", name, what, count);
			return false;
		}
		if((uint64)count * elemSize > (uint64)left){
			debug("colmodel %s: %s count %d needs %u bytes, %u left in the record\n",
			      name, what, count, (unsigned)((uint64)count * elemSize), (unsigned)left);
			return false;
		}
		return true;
	}

	void Skip(size_t n) { p += n; left -= n; }
};

// A collision model with triangles but no vertices, or with a triangle pointing
// past the last vertex, is not usable: every check that follows indexes the
// vertex array with those numbers (CalculateTrianglePlanes does it first, from
// the collision cache, before any entity is even drawn). Dropping the triangle
// list keeps the model's spheres and boxes, which is what the game falls back to
// when a model's detail is unusable - and it is a whole lot better than
// dereferencing null on the loading screen.
static void
DropUnusableTriangles(CColModel &model, int32 numVertices, const char *modelname)
{
	if(model.numTriangles == 0)
		return;
	if(model.vertices == nil || numVertices <= 0){
		debug("colmodel %s: %d triangles but no vertices, dropping them\n",
		      modelname, model.numTriangles);
		model.triangles = nil;
		model.numTriangles = 0;
		return;
	}
	for(int i = 0; i < model.numTriangles; i++){
		CColTriangle &t = model.triangles[i];
		if(t.a >= numVertices || t.b >= numVertices || t.c >= numVertices){
			debug("colmodel %s: triangle %d points past the last vertex (%d of %d), dropping the list\n",
			      modelname, i, (int)t.a, numVertices);
			model.triangles = nil;
			model.numTriangles = 0;
			return;
		}
	}
}

void
CFileLoader::LoadCollisionModel(uint8 *buf, size_t bufsize, CColModel &model, char *modelname)
{
	int i;
	ColRecord r(buf, bufsize, modelname);

	if(!r.Has(44)){
		debug("colmodel %s: record too short for its header (%u bytes)\n",
		      modelname, (unsigned)bufsize);
		return;
	}
	model.boundingSphere.radius = *(float*)(r.p);
	model.boundingSphere.center.x = *(float*)(r.p+4);
	model.boundingSphere.center.y = *(float*)(r.p+8);
	model.boundingSphere.center.z = *(float*)(r.p+12);
	model.boundingBox.min.x = *(float*)(r.p+16);
	model.boundingBox.min.y = *(float*)(r.p+20);
	model.boundingBox.min.z = *(float*)(r.p+24);
	model.boundingBox.max.x = *(float*)(r.p+28);
	model.boundingBox.max.y = *(float*)(r.p+32);
	model.boundingBox.max.z = *(float*)(r.p+36);
	model.numSpheres = *(int16*)(r.p+40);
	r.Skip(44);

	if(model.numSpheres > 0 && r.SectionFits(model.numSpheres, sizeof(CColSphere), "sphere")){
		model.spheres = (CColSphere*)RwMalloc(model.numSpheres*sizeof(CColSphere));
		REGISTER_MEMPTR(&model.spheres);
		for(i = 0; i < model.numSpheres; i++){
			model.spheres[i].Set(*(float*)r.p, *(CVector*)(r.p+4), r.p[16], r.p[17]);
			r.Skip(20);
		}
	}else
		model.spheres = nil;

	model.numLines = r.ReadCount();
	if(model.numLines > 0 && r.SectionFits(model.numLines, 24, "line")){
		//model.lines = (CColLine*)RwMalloc(model.numLines*sizeof(CColLine));
		REGISTER_MEMPTR(&model.lines);
		for(i = 0; i < model.numLines; i++)
			r.Skip(24);
	}
	// The game never uses collision lines for anything but the four wheels of a
	// vehicle, and those come from the DFF, so the list is dropped either way.
	model.numLines = 0;
	model.lines = nil;

	model.numBoxes = r.ReadCount();
	if(model.numBoxes > 0 && r.SectionFits(model.numBoxes, sizeof(CColBox), "box")){
		model.boxes = (CColBox*)RwMalloc(model.numBoxes*sizeof(CColBox));
		REGISTER_MEMPTR(&model.boxes);
		for(i = 0; i < model.numBoxes; i++){
			model.boxes[i].Set(*(CVector*)r.p, *(CVector*)(r.p+12), r.p[24], r.p[25]);
			r.Skip(28);
		}
	}else
		model.boxes = nil;

	int32 numVertices = r.ReadCount();
	if(numVertices > 0 && r.SectionFits(numVertices, sizeof(CompressedVector), "vertex")){
		model.vertices = (CompressedVector*)RwMalloc(numVertices*sizeof(CompressedVector));
		REGISTER_MEMPTR(&model.vertices);
		for(i = 0; i < numVertices; i++){
			model.vertices[i].Set(*(float*)r.p, *(float*)(r.p+4), *(float*)(r.p+8));
			r.Skip(12);
		}
	}else
		model.vertices = nil;

	model.numTriangles = r.ReadCount();
	if(model.numTriangles > 0 && r.SectionFits(model.numTriangles, 16, "triangle")){
		model.triangles = (CColTriangle*)RwMalloc(model.numTriangles*sizeof(CColTriangle));
		REGISTER_MEMPTR(&model.triangles);
		for(i = 0; i < model.numTriangles; i++){
			model.triangles[i].Set(*(int32*)r.p, *(int32*)(r.p+4), *(int32*)(r.p+8), r.p[12]);
			r.Skip(16);
		}
	}else
		model.triangles = nil;

	DropUnusableTriangles(model, numVertices, modelname);

	// One line per model that ended up with no usable solid geometry at all.
	// Such a model is drawn but has nothing to collide with, which is exactly
	// the "collision is missing here" a player runs into, so the name has to be
	// in the log next to the numbers that produced it.
	if(model.numSpheres == 0 && model.numBoxes == 0 && model.numTriangles == 0)
		debug("colmodel %s: no usable collision geometry (%d spheres, %d boxes, %d triangles, %d vertices), record %u bytes\n",
		      modelname, model.numSpheres, model.numBoxes, model.numTriangles, numVertices,
		      (unsigned)bufsize);
}

static void
GetNameAndLOD(const char *nodename, char *name, int *n)
{
	const char *underscore = nil;
	for(const char *s = nodename; *s != '\0'; s++){
		if(s[0] == '_' && (s[1] == 'l' || s[1] == 'L') && isdigit(s[2]))
			underscore = s;
	}
	// Every caller supplies name[24]. Imported frame names may be longer.
	size_t length = underscore ? (size_t)(underscore - nodename) : strlen(nodename);
	length = Min(length, (size_t)23);
	memcpy(name, nodename, length);
	name[length] = '\0';
	*n = underscore ? atoi(underscore + 2) : 0;
}

RpAtomic*
CFileLoader::FindRelatedModelInfoCB(RpAtomic *atomic, void *data)
{
	char *nodename, name[24];
	int n;
	RpClump *clump = (RpClump*)data;

	nodename = GetFrameNodeName(RpAtomicGetFrame(atomic));
	GetNameAndLOD(nodename ? nodename : "", name, &n);
	CBaseModelInfo *info = CModelInfo::GetModelInfo(name, nil);
	if(info && GetModelLoadKind(info->GetModelType()) == MODELLOAD_ATOMIC && n >= 0 && n < 3){
		CSimpleModelInfo *mi = (CSimpleModelInfo*)info;
		CVisibilityPlugins::SetAtomicRenderCallback(atomic, nil);
		mi->SetAtomic(n, atomic);
		RpClumpRemoveAtomic(clump, atomic);
		RpAtomicSetFrame(atomic, RwFrameCreate());
		CVisibilityPlugins::SetAtomicModelInfo(atomic, mi);
	}else{
		if(info)
			REVC_LOG_WARNING("Atomic %s: IDE model type %d or LOD %d is not a simple object; ignoring",
				name, info->GetModelType(), n);
		else
			debug("Can't find Atomic %s\n", name);
	}

	return atomic;
}

#ifdef LIBRW
void
InitClump(RpClump *clump)
{
	RpClumpForAllAtomics(clump, ConvertPlatformAtomic, nil);
}
#else
#define InitClump(clump)
#endif

struct LoadedClumpStructure {
	int atomics;
	bool invalidAtomic;
	bool skinned;
	int maxSkinBones;
};

static RpAtomic*
InspectLoadedAtomic(RpAtomic *atomic, void *data)
{
	LoadedClumpStructure *structure = (LoadedClumpStructure*)data;
	structure->atomics++;
	RpGeometry *geometry = RpAtomicGetGeometry(atomic);
	if(!geometry || !RpAtomicGetFrame(atomic)){
		structure->invalidAtomic = true;
		return atomic;
	}
	RpSkin *skin = RpSkinGeometryGetSkin(geometry);
	if(skin){
		structure->skinned = true;
		structure->maxSkinBones = Max(structure->maxSkinBones, (int)RpSkinGetNumBones(skin));
	}
	return atomic;
}

// Validate before calling the model-specific SetClump: both ped animation
// binding and vehicle hierarchy preprocessing assume a usable RW structure.
// This runs for native as well as custom DFFs and leaves native VC frame peds
// alone (an SA skin is NOT required for a VC ped).
static bool
InstallLoadedClump(RpClump *clump, CBaseModelInfo *info, const char *source, uint32 clumpVersion = 0)
{
	LoadedClumpStructure structure = { 0, false, false, 0 };
	ModelLoadKind kind = info ? GetModelLoadKind(info->GetModelType()) : MODELLOAD_UNSUPPORTED;
	bool hasRoot = clump && RpClumpGetFrame(clump);
	if(clump)
		RpClumpForAllAtomics(clump, InspectLoadedAtomic, &structure);
	RpHAnimHierarchy *hier = hasRoot && structure.skinned ? GetAnimHierarchyFromClump(clump) : nil;
	if(!IsModelClumpCompatible(kind, hasRoot, structure.atomics > 0,
	                        structure.skinned, hier != nil) || structure.invalidAtomic ||
	   (structure.skinned && (structure.maxSkinBones <= 0 ||
	                          (hier && (hier->numNodes <= 0 || structure.maxSkinBones > hier->numNodes))))){
		REVC_LOG_WARNING("DFF %s: model %s (type %d) has an incompatible clump (root=%d, atomics=%d, skin=%d, bones=%d, hierarchy=%d); refusing model",
			source, info ? info->GetModelName() : "unknown", info ? info->GetModelType() : -1,
			hasRoot, structure.atomics, structure.skinned, structure.maxSkinBones, hier ? hier->numNodes : 0);
		if(clump)
			RpClumpDestroy(clump);
		return false;
	}
	if(info->GetModelType() == MITYPE_PED)
		((CPedModelInfo*)info)->SetClumpStreamVersion(clumpVersion);
	((CClumpModelInfo*)info)->SetClump(clump);
	return true;
}

struct LoadedClumpGeometry {
	int atomics;
	bool invalid;
};

static RpAtomic*
InspectLoadedGeometry(RpAtomic *atomic, void *data)
{
	LoadedClumpGeometry *geometry = (LoadedClumpGeometry*)data;
	geometry->atomics++;
	if(!RpAtomicGetGeometry(atomic) || !RpAtomicGetFrame(atomic))
		geometry->invalid = true;
	return atomic;
}

#if defined(CUSTOM_MODELS) && defined(LIBRW)
static void
InstallCustomModel2dfxLights(CBaseModelInfo *info, const std::vector<SAStaticModel::LightEffect> &lights)
{
	if(info == nil || info->GetNum2dEffects() != 0 || lights.empty())
		return;
	const bool isStreetLamp = SAStaticModel::IsOutdoorStreetLampName(info->GetModelName());
	const size_t count = lights.size() > 32 ? 32 : lights.size();
	const int firstIdx = CModelInfo::Get2dEffectStore().allocPtr;
	if(firstIdx < 0 || firstIdx + (int)count > 32760)
		return;

	const int particleSlot = CTxdStore::FindTxdSlot("particle");
	if(particleSlot != -1){
		CTxdStore::PushCurrentTxd();
		CTxdStore::SetCurrentTxd(particleSlot);
	}

	for(size_t i = 0; i < count; i++){
		const SAStaticModel::LightEffect &fx = lights[i];
		C2dEffect *effect = CModelInfo::Get2dEffectStore().Alloc();
		if(effect == nil)
			break;
		info->Add2dEffect(effect);
		effect->pos = CVector(fx.pos[0], fx.pos[1], fx.pos[2]);
		effect->col = CRGBA(fx.r, fx.g, fx.b, fx.a ? fx.a : 200);
		effect->type = EFFECT_LIGHT;

		const bool withoutCorona = (fx.flags1 & 0x08) != 0;
		const bool atDay = (fx.flags1 & 0x20) != 0;
		const bool atNight = (fx.flags1 & 0x40) != 0;
		const bool nightOnly = !atDay || (atNight && !atDay);

		const float minCoronaDist = isStreetLamp ? 280.0f : 185.0f;
		effect->light.dist = fx.coronaDist > minCoronaDist ? fx.coronaDist : minCoronaDist;
		effect->light.range = fx.pointlightRange;
		if(isStreetLamp && effect->light.range < 18.0f)
			effect->light.range = 22.0f;
		effect->light.size = withoutCorona ? 0.0f : (fx.coronaSize > 0.0f ? fx.coronaSize : 0.85f);
		effect->light.shadowSize = fx.shadowSize;
		effect->light.shadowIntensity = fx.shadowIntensity;

		switch(fx.flashType){
		case 1:
		case 2:
			effect->light.lightType = nightOnly ? LIGHT_FLICKER_NIGHT : LIGHT_FLICKER;
			break;
		case 3:
		case 4:
			effect->light.lightType = nightOnly ? LIGHT_RANDOM_FLICKER_NIGHT : LIGHT_RANDOM_FLICKER;
			break;
		case 5:
		case 6:
			effect->light.lightType = nightOnly ? LIGHT_FLASH1_NIGHT : LIGHT_FLASH1;
			break;
		case 7:
		case 8:
			effect->light.lightType = nightOnly ? LIGHT_FLASH2_NIGHT : LIGHT_FLASH2;
			break;
		case 9:
			effect->light.lightType = nightOnly ? LIGHT_FLASH3_NIGHT : LIGHT_FLASH3;
			break;
		default:
			effect->light.lightType = nightOnly ? LIGHT_ON_NIGHT : LIGHT_ON;
			break;
		}

		effect->light.roadReflection = fx.reflection ? 1 : 0;
		effect->light.flareType = fx.flareType <= 2 ? fx.flareType : 0;

		uint8 vcFlags = 0;
		if(fx.flags1 & 0x01)
			vcFlags |= LIGHTFLAG_LOSCHECK;
		if(fx.flags1 & 0x10)
			vcFlags |= LIGHTFLAG_LONG_DIST;
		effect->light.flags = vcFlags;

		effect->light.corona = (!withoutCorona && fx.coronaName[0]) ? RwTextureRead(fx.coronaName, nil) : nil;
		if(!withoutCorona && effect->light.corona == nil && gpCoronaTexture[0]){
			effect->light.corona = gpCoronaTexture[0];
			RwTextureAddRef(effect->light.corona);
		}

		effect->light.shadow = (fx.shadowSize > 0.0f && fx.shadowName[0]) ? RwTextureRead(fx.shadowName, nil) : nil;
		if(fx.shadowSize > 0.0f && effect->light.shadow == nil && gpShadowExplosionTex){
			effect->light.shadow = gpShadowExplosionTex;
			RwTextureAddRef(effect->light.shadow);
		}
		if(effect->light.shadow == nil)
			effect->light.shadowSize = 0.0f;
	}

	if(particleSlot != -1)
		CTxdStore::PopCurrentTxd();

	// Keep static light-bearing fixtures & lampposts visible as far as their coronas
	// so coronas never float in mid-air after a short-drawDist pole disappears.
	if(info->IsBuilding() && info->GetNum2dEffects() > 0){
		CSimpleModelInfo *smi = (CSimpleModelInfo*)info;
		if(!smi->m_isBigBuilding && smi->m_lodDistances[0] > 0.0f && smi->m_lodDistances[0] < 299.0f){
			float boosted = SAStaticModel::EffectiveStaticDrawDistance(
				smi->GetModelName(), smi->m_lodDistances[0], true);
			if(boosted > smi->m_lodDistances[0])
				smi->m_lodDistances[0] = boosted;
		}
	}
}
#endif

static bool
PrepareAndInstallClump(RpClump *clump, CBaseModelInfo *info, const char *source, uint32 clumpVersion = 0)
{
	LoadedClumpGeometry geometry = { 0, false };
	if(clump)
		RpClumpForAllAtomics(clump, InspectLoadedGeometry, &geometry);
	if(!clump || !RpClumpGetFrame(clump) || !geometry.atomics || geometry.invalid){
		REVC_LOG_WARNING("DFF %s: %s has no usable root/atomic geometry", source,
			info ? info->GetModelName() : "unknown");
		if(clump)
			RpClumpDestroy(clump);
		return false;
	}
	InitClump(clump);
#if defined(CUSTOM_MODELS) && defined(LIBRW)
	if(info && info->GetNum2dEffects() == 0 &&
	   GetModelLoadKind(info->GetModelType()) == MODELLOAD_CLUMP){
		std::vector<SAStaticModel::LightEffect> clumpLights;
		SAStaticModel::ExtractClumpLights(clump, clumpLights);
		if(clumpLights.empty() && SAStaticModel::IsOutdoorStreetLampName(info->GetModelName())){
			FORLIST(lnk, clump->atomics){
				RpAtomic *a = rw::Atomic::fromClump(lnk);
				if(a && (a->object.object.flags & rw::Atomic::RENDER)){
					SAStaticModel::EstimateStreetLampLights(a, clumpLights);
					if(!clumpLights.empty())
						break;
				}
			}
		}
		InstallCustomModel2dfxLights(info, clumpLights);
	}
#endif
	return InstallLoadedClump(clump, info, source, clumpVersion);
}

void
CFileLoader::LoadModelFile(const char *filename)
{
	RwStream *stream;
	RpClump *clump;

	debug("Loading model file %s\n", filename);
	stream = RwStreamOpen(rwSTREAMFILENAME, rwSTREAMREAD, filename);
	if(RwStreamFindChunk(stream, rwID_CLUMP, nil, nil)){
		clump = RpClumpStreamRead(stream);
		if(clump){
			InitClump(clump);
			RpClumpForAllAtomics(clump, FindRelatedModelInfoCB, clump);
			RpClumpDestroy(clump);
		}
	}
	RwStreamClose(stream, nil);
}

void
CFileLoader::LoadClumpFile(const char *filename)
{
	RwStream *stream;
	RpClump *clump;
	char *nodename, name[24];
	int n;
	CBaseModelInfo *mi;

	uint32 clumpVersion = 0;

	debug("Loading model file %s\n", filename);
	stream = RwStreamOpen(rwSTREAMFILENAME, rwSTREAMREAD, filename);
	while(RwStreamFindChunk(stream, rwID_CLUMP, nil, &clumpVersion)){
		clump = RpClumpStreamRead(stream);
		if(clump){
			if(!RpClumpGetFrame(clump)){
				REVC_LOG_WARNING("DFF %s: missing clump root frame", filename);
				RpClumpDestroy(clump);
				continue;
			}
			nodename = GetFrameNodeName(RpClumpGetFrame(clump));
			GetNameAndLOD(nodename ? nodename : "", name, &n);
			mi = CModelInfo::GetModelInfo(name, nil);
#ifdef SA_VEHICLE_MODEL_NAME_FALLBACK
			if(mi == nil || !mi->IsClump()){
				// SA models and mods converted from them often keep the node
				// name of the car they were converted from, so the name in the
				// DFF may have nothing to do with the model it is used as.
				// The file name is the name the model was requested by.
				char basename[32];
				const char *base, *p;

				base = filename;
				for(p = filename; *p; p++)
					if(*p == '\\' || *p == '/')
						base = p+1;
				strncpy(basename, base, 31);
				basename[31] = '\0';
				p = strrchr(basename, '.');
				if(p)
					basename[p - basename] = '\0';
				GetNameAndLOD(basename, name, &n);
			mi = CModelInfo::GetModelInfo(name, nil);
			if(mi)
				debug("Model file %s: node name doesn't match, using file name\n", filename);
			}
#endif
			if(mi && (GetModelLoadKind(mi->GetModelType()) == MODELLOAD_CLUMP ||
			          GetModelLoadKind(mi->GetModelType()) == MODELLOAD_PED ||
			          GetModelLoadKind(mi->GetModelType()) == MODELLOAD_VEHICLE)){
				PrepareAndInstallClump(clump, mi, filename, clumpVersion);
			}else{
				if(mi)
					REVC_LOG_WARNING("DFF %s: %s is IDE type %d, not a clump model",
						filename, mi->GetModelName(), mi->GetModelType());
				RpClumpDestroy(clump);
			}
		}
	}
	RwStreamClose(stream, nil);
}

bool
CFileLoader::LoadClumpFile(RwStream *stream, uint32 id)
{
	CBaseModelInfo *mi = CModelInfo::GetModelInfo(id);
	if(!mi || (GetModelLoadKind(mi->GetModelType()) != MODELLOAD_PED &&
	           GetModelLoadKind(mi->GetModelType()) != MODELLOAD_CLUMP)){
		REVC_LOG_WARNING("DFF stream id %u: expected ped/clump model info, got type %d",
			id, mi ? mi->GetModelType() : -1);
		return false;
	}
	uint32 clumpVersion = 0;
	if(!RwStreamFindChunk(stream, rwID_CLUMP, nil, &clumpVersion))
		return false;
	RpClump *clump = RpClumpStreamRead(stream);
	if(clump == nil)
		return false;
	return PrepareAndInstallClump(clump, mi, "stream", clumpVersion);
}

bool
CFileLoader::StartLoadClumpFile(RwStream *stream, uint32 id)
{
	CBaseModelInfo *mi = CModelInfo::GetModelInfo(id);
	if(!mi || GetModelLoadKind(mi->GetModelType()) != MODELLOAD_VEHICLE){
		REVC_LOG_WARNING("DFF stream id %u: expected vehicle model info, got type %d",
			id, mi ? mi->GetModelType() : -1);
		return false;
	}
	uint32 size = 0;

	if(RwStreamFindChunk(stream, rwID_CLUMP, &size, nil)){
#ifdef SA_VEHICLE_MODELS
		// remember where the clump is; the collision inside it is read in
		// FinishLoadClumpFile, after the atomics are in
		saClumpBodyPos = STREAMPOS(stream);
		saClumpBodySize = size;
		saClumpBodyKnown = true;
#else
		(void)size;
#endif
		printf("Start loading %s\n", CModelInfo::GetModelInfo(id)->GetModelName());
		return RpClumpGtaStreamRead1(stream);
	}else{
		printf("FAILED\n");
		return false;
	}
}

bool
CFileLoader::FinishLoadClumpFile(RwStream *stream, uint32 id)
{
	CBaseModelInfo *mi = CModelInfo::GetModelInfo(id);
	if(!mi || GetModelLoadKind(mi->GetModelType()) != MODELLOAD_VEHICLE){
		REVC_LOG_WARNING("DFF stream id %u: vehicle load completed with wrong model info type %d",
			id, mi ? mi->GetModelType() : -1);
		return false;
	}
	printf("Finish loading %s\n", mi->GetModelName());
	RpClump *clump = RpClumpGtaStreamRead2(stream);

	if(clump){
		if(!PrepareAndInstallClump(clump, mi, "vehicle stream")){
#ifdef SA_VEHICLE_MODELS
			saClumpBodyKnown = false;
#endif
			return false;
		}
#ifdef SA_VEHICLE_MODELS
		if(saClumpBodyKnown && mi->GetModelType() == MITYPE_VEHICLE){
			// SA keeps the collision of a vehicle inside the model, in the clump
			// extension behind the atomics. The reader above does not stop there
			// (it ends past them), so the chunks of the clump are walked again to
			// find the collision chunk; everything else in the clump is left as it
			// was and the stream is put back where the caller expects it.
			uint32 save, colSize;
			save = STREAMPOS(stream);
			stream->seek(saClumpBodyPos, 0);
			colSize = FindSAColChunk(stream, saClumpBodyPos + saClumpBodySize, 4);
			if(colSize)
				LoadSAVehicleColModel(stream, id, colSize);
			stream->seek(save, 0);
		}
		saClumpBodyKnown = false;
#endif
		return true;
	}else{
		printf("FAILED\n");
		return false;
	}
}

CSimpleModelInfo *gpRelatedModelInfo;

#if defined(CUSTOM_MODELS) && defined(LIBRW)
struct SAStaticTxdScope {
	RwTexDictionary *previous;
	bool changed;
	SAStaticTxdScope() : previous(RwTexDictionaryGetCurrent()), changed(false) {}
	void Set(RwTexDictionary *dict) { RwTexDictionarySetCurrent(dict); changed = true; }
	~SAStaticTxdScope() { if(changed) RwTexDictionarySetCurrent(previous); }
};

struct SAStaticTextureStats {
	int uvMaterials, resolved;
};

static RpAtomic *
CountSAStaticTextures(RpAtomic *atomic, void *data)
{
	SAStaticTextureStats *stats = (SAStaticTextureStats*)data;
	rw::Geometry *g = atomic->geometry;
	if((atomic->object.object.flags & rw::Atomic::RENDER) &&
	   g && (g->numTriangles > 0 || (g->meshHeader && g->meshHeader->totalIndices >= 3)) &&
	   g->numTexCoordSets > 0){
		for(int i = 0; i < g->matList.numMaterials; i++){
			stats->uvMaterials++;
			if(g->matList.materials[i] && g->matList.materials[i]->texture)
				stats->resolved++;
		}
	}
	return atomic;
}
#endif

bool
CFileLoader::LoadAtomicFile(RwStream *stream, uint32 id)
{
	CBaseModelInfo *info = CModelInfo::GetModelInfo(id);
	if(!info || GetModelLoadKind(info->GetModelType()) != MODELLOAD_ATOMIC){
		REVC_LOG_WARNING("DFF stream id %u: expected simple model info, got type %d",
			id, info ? info->GetModelType() : -1);
		return false;
	}
	RpClump *clump;

#if defined(CUSTOM_MODELS) && defined(LIBRW)
	// Only custom IDE statics and simple objects replaced by custom/img/*.img need
	// the SA clump path. Native VC simple models still use their named _lN
	// atomics and draw-distance slots unchanged.
	bool saStatic = CCustomWorld::OwnsModelSlot(id);
	SAStaticTxdScope txdScope;
	CSimpleModelInfo *mi = (CSimpleModelInfo*)CModelInfo::GetModelInfo(id);
	if(!saStatic && mi->IsBuilding() && mi->m_numAtomics == 1 &&
	   mi->m_firstDamaged == 0 && CStreaming::ms_aInfoForModel[id].m_size)
		saStatic = CCustomModels::IsAutoMountedImage(
			_GET_INDEX(CStreaming::ms_aInfoForModel[id].m_position));
#ifdef ANDROID
	if(!saStatic && CCustomModels::CanServe(id))
		saStatic = true;
#endif
	if(saStatic){
		// The streamer normally does this already. The model's own TXD is
		// selected for matched IMG pairs; otherwise the IDE assignment stays.
		// RpClumpStreamRead resolves material names at read time, not later.
		TxdDef *txd = CTxdStore::GetSlot(mi->GetTxdSlot());
		if(txd == nil || txd->texDict == nil){
			REVC_LOG_ERROR("SA static %s: selected TXD slot %d has not loaded",
				mi->GetModelName(), mi->GetTxdSlot());
			return false;
		}
		txdScope.Set(txd->texDict);
	}
#endif
	if(RwStreamFindChunk(stream, rwID_CLUMP, nil, nil)){
		clump = RpClumpStreamRead(stream);
		if(clump == nil)
			return false;
		LoadedClumpGeometry geometry = { 0, false };
		RpClumpForAllAtomics(clump, InspectLoadedGeometry, &geometry);
		if(!RpClumpGetFrame(clump) || !geometry.atomics || geometry.invalid){
			REVC_LOG_WARNING("DFF stream id %u (%s): no usable static atomics", id, info->GetModelName());
			RpClumpDestroy(clump);
			return false;
		}
		InitClump(clump);
#if defined(CUSTOM_MODELS) && defined(LIBRW)
		if(saStatic){
			SAStaticTextureStats texStats = { 0, 0 };
			RpClumpForAllAtomics(clump, CountSAStaticTextures, &texStats);
			if(texStats.uvMaterials && !texStats.resolved)
				REVC_LOG_WARNING("SA static %s: no material textures resolved from TXD %s; check the names in DFF/TXD",
					mi->GetModelName(), CTxdStore::GetTxdName(mi->GetTxdSlot()));
			std::vector<SAStaticModel::LightEffect> dffLights;
			if(mi->GetNum2dEffects() == 0)
				SAStaticModel::ExtractClumpLights(clump, dffLights);
			const char *error = nil;
			RpAtomic *atomic = SAStaticModel::TakeAtomic(clump, &error);
			if(atomic){
				CVisibilityPlugins::SetAtomicRenderCallback(atomic, nil);
#ifdef ANDROID
				if(!CCustomWorld::OwnsModelSlot(id) &&
				   (mi->m_numAtomics > 1 || mi->m_firstDamaged != 0)){
					float maxDist = mi->m_lodDistances[0];
					for(int lod = 1; lod < mi->m_numAtomics; lod++)
						if(mi->m_lodDistances[lod] > maxDist)
							maxDist = mi->m_lodDistances[lod];
					mi->m_lodDistances[0] = maxDist;
					mi->m_numAtomics = 1;
					mi->m_firstDamaged = 0;
				}
#endif
				mi->SetAtomic(0, atomic); // one draw-distance slot, all clump parts
				CVisibilityPlugins::SetAtomicModelInfo(atomic, mi);
#ifdef ANDROID
				if(mi->GetColModel() == &CTempColModels::ms_colModelBBox){
					RpGeometry *geom = RpAtomicGetGeometry(atomic);
					if(geom){
						CVector bmin(1e30f, 1e30f, 1e30f), bmax(-1e30f, -1e30f, -1e30f);
						bool haveBounds = false;
						int32 nVerts = RpGeometryGetNumVertices(geom);
						RpMorphTarget *mt = RpGeometryGetMorphTarget(geom, 0);
						const RwV3d *verts = mt ? RpMorphTargetGetVertices(mt) : nil;
						if(verts && nVerts > 0){
							for(int32 vi = 0; vi < nVerts; vi++){
								const RwV3d &v = verts[vi];
								if(!(v.x == v.x && v.y == v.y && v.z == v.z)) continue;
								if(v.x < bmin.x) bmin.x = v.x;
								if(v.y < bmin.y) bmin.y = v.y;
								if(v.z < bmin.z) bmin.z = v.z;
								if(v.x > bmax.x) bmax.x = v.x;
								if(v.y > bmax.y) bmax.y = v.y;
								if(v.z > bmax.z) bmax.z = v.z;
								haveBounds = true;
							}
						}
						if(haveBounds){
							CVector mid = (bmin + bmax) * 0.5f;
							CVector halfExt = (bmax - bmin) * 0.5f;
							float rad = halfExt.Magnitude();
							// This box is the only collision the model will ever get
							// here, so every way out of it has to leave a name behind:
							// a model that silently falls through this gate is drawn
							// with nothing to stand or shoot on, which is exactly the
							// "collision is missing here" a player runs into.
							if(!(rad > 1.5f && rad < 4000.0f))
								REVC_LOG_WARNING("collision %s: no .col, and the mesh box radius %.3f is outside 1.5..4000, so no collision box was created",
									mi->GetModelName(), rad);
							else if(!CPools::GetColModelPool() || !CPools::GetColModelPool()->EnsureFree(8))
								REVC_LOG_ERROR("collision %s: no .col, and the collision model pool cannot take another box, so no collision box was created",
									mi->GetModelName());
							else{
								CColModel *col = new CColModel();
								if(col){
									col->boundingBox.Set(bmin, bmax);
									col->boundingSphere.Set(rad, mid);
									col->level = LEVEL_GENERIC;
									mi->SetColModel(col, true);
									if(CPools::GetPtrNodePool() &&
									   CPools::GetPtrNodePool()->EnsureFree(512)){
										const CVector &camPos = TheCamera.GetPosition();
										int cx = CWorld::GetSectorIndexX(camPos.x);
										int cy = CWorld::GetSectorIndexY(camPos.y);
										std::vector<CEntity*> relink;
										for(int sy = cy - 8; sy <= cy + 8; sy++){
											for(int sx = cx - 8; sx <= cx + 8; sx++){
												CSector *sec = CWorld::GetSector(sx, sy);
												if(!sec) continue;
												for(int lk = 0; lk < 2; lk++){
													CPtrList &lst = sec->m_lists[lk == 0 ? ENTITYLIST_BUILDINGS : ENTITYLIST_BUILDINGS_OVERLAP];
													for(CPtrNode *nd = lst.first; nd; nd = nd->next){
														CEntity *ent = (CEntity*)nd->item;
														if(ent && ent->GetModelIndex() == id && !ent->bIsBIGBuilding){
															bool dup = false;
															for(size_t ri = 0; ri < relink.size(); ri++)
																if(relink[ri] == ent){ dup = true; break; }
															if(!dup) relink.push_back(ent);
														}
													}
												}
											}
										}
										for(size_t ri = 0; ri < relink.size(); ri++){
											CWorld::Remove(relink[ri]);
											CWorld::Add(relink[ri]);
										}
									}
								}
							}
						}
					}
				}
#endif
				if(mi->GetNum2dEffects() == 0){
					if(dffLights.empty() && SAStaticModel::IsOutdoorStreetLampName(mi->GetModelName()))
						SAStaticModel::EstimateStreetLampLights(atomic, dffLights);
					InstallCustomModel2dfxLights(mi, dffLights);
				}
			}else
				REVC_LOG_ERROR("SA static %s: cannot use DFF (%s)",
					mi->GetModelName(), error ? error : "unknown reason");
			RpClumpDestroy(clump);
			return atomic != nil;
		}
#endif
		gpRelatedModelInfo = (CSimpleModelInfo*)info;
		RpClumpForAllAtomics(clump, SetRelatedModelInfoCB, clump);
		RpClumpDestroy(clump);
		return gpRelatedModelInfo->GetRwObject() != nil;
	}
	return false;
}

RpAtomic*
CFileLoader::SetRelatedModelInfoCB(RpAtomic *atomic, void *data)
{
	char *nodename, name[24];
	int n;
	RpClump *clump = (RpClump*)data;

	nodename = GetFrameNodeName(RpAtomicGetFrame(atomic));
	GetNameAndLOD(nodename ? nodename : "", name, &n);
	if(n < 0 || n >= 3){
		REVC_LOG_WARNING("DFF %s: invalid static atomic LOD index %d (%s)",
			gpRelatedModelInfo->GetModelName(), n, name);
		return atomic;
	}
	CVisibilityPlugins::SetAtomicRenderCallback(atomic, nil);
	gpRelatedModelInfo->SetAtomic(n, atomic);
	RpClumpRemoveAtomic(clump, atomic);
	RpAtomicSetFrame(atomic, RwFrameCreate());
	CVisibilityPlugins::SetAtomicModelInfo(atomic, gpRelatedModelInfo);
	return atomic;
}

RpClump*
CFileLoader::LoadAtomicFile2Return(const char *filename)
{
	RwStream *stream;
	RpClump *clump;

	clump = nil;
	debug("Loading model file %s\n", filename);
	stream = RwStreamOpen(rwSTREAMFILENAME, rwSTREAMREAD, filename);
	if(RwStreamFindChunk(stream, rwID_CLUMP, nil, nil))
		clump = RpClumpStreamRead(stream);
	if(clump)
		InitClump(clump);
	RwStreamClose(stream, nil);
	return clump;
}

static RwTexture*
MoveTexturesCB(RwTexture *texture, void *pData)
{
	RwTexDictionaryAddTexture((RwTexDictionary*)pData, texture);
	return texture;
}

void
CFileLoader::AddTexDictionaries(RwTexDictionary *dst, RwTexDictionary *src)
{
	RwTexDictionaryForAllTextures(src, MoveTexturesCB, dst);
}

#define isLine3(l, a, b, c) ((l[0] == a) && (l[1] == b) && (l[2] == c))
#define isLine4(l, a, b, c, d) ((l[0] == a) && (l[1] == b) && (l[2] == c) && (l[3] == d))

void
CFileLoader::LoadObjectTypes(const char *filename)
{
	enum {
		NONE,
		OBJS,
		MLO,	// unused but enum still has it
		TOBJ,
		WEAP,
		HIER,
		CARS,
		PEDS,
		PATH,
		TWODFX,
		TXDP
	};
	char *line;
	int fd;
	int section;
	int pathIndex;
	int id, pathType;
	int minID, maxID;

	section = NONE;
	minID = INT32_MAX;
	maxID = -1;
	pathIndex = -1;
	debug("Loading object types from %s...\n", filename);

	fd = CFileMgr::OpenFile(filename, "rb");
	assert(fd > 0);
	for(line = CFileLoader::LoadLine(fd); line; line = CFileLoader::LoadLine(fd)){
		if(*line == '\0' || *line == '#')
			continue;

		if(section == NONE){
			if(isLine4(line, 'o','b','j','s')) section = OBJS;
			else if(isLine4(line, 't','o','b','j')) section = TOBJ;
			else if(isLine4(line, 'w','e','a','p')) section = WEAP;
			else if(isLine4(line, 'h','i','e','r')) section = HIER;
			else if(isLine4(line, 'c','a','r','s')) section = CARS;
			else if(isLine4(line, 'p','e','d','s')) section = PEDS;
			else if(isLine4(line, 'p','a','t','h')) section = PATH;
			else if(isLine4(line, '2','d','f','x')) section = TWODFX;
			else if(isLine4(line, 't','x','d','p')) section = TXDP;
		}else if(isLine3(line, 'e','n','d')){
			section = NONE;
		}else switch(section){
		case OBJS:
			id = LoadObject(line);
			if(id > maxID) maxID = id;
			if(id < minID) minID = id;
			break;
		case TOBJ:
			id = LoadTimeObject(line);
			if(id > maxID) maxID = id;
			if(id < minID) minID = id;
			break;
		case WEAP:
			LoadWeaponObject(line);
			break;
		case HIER:
			LoadClumpObject(line);
			break;
		case CARS:
			LoadVehicleObject(line);
			break;
		case PEDS:
			LoadPedObject(line);
			break;
		case PATH:
			if(pathIndex == -1){
				id = LoadPathHeader(line, pathType);
				pathIndex = 0;
			}else{
				if(pathType == 0)
					LoadPedPathNode(line, id, pathIndex);
				else if (pathType == 1)
					LoadCarPathNode(line, id, pathIndex, false);
				else if (pathType == 2)
					LoadCarPathNode(line, id, pathIndex, true);
				pathIndex++;
				if(pathIndex == 12)
					pathIndex = -1;
			}
			break;
		case TWODFX:
			Load2dEffect(line);
			break;
		case TXDP:
		{
			char childName[32], parentName[32];
			if(sscanf(line, "%31[^, \t]%*[, \t]%31[^, \t\r\n]", childName, parentName) == 2){
				int childSlot = CTxdStore::FindTxdSlot(childName);
				int parentSlot = CTxdStore::FindTxdSlot(parentName);
				if(parentSlot == -1 && CTxdStore::HasFreeSlot())
					parentSlot = CTxdStore::AddTxdSlot(parentName);
				if(childSlot >= 0 && parentSlot >= 0 && childSlot != parentSlot)
					CTxdStore::SetTxdParent(childSlot, parentSlot);
			}
			break;
		}
		}
	}
	CFileMgr::CloseFile(fd);

	for(id = minID; id <= maxID; id++){
		CSimpleModelInfo *mi = (CSimpleModelInfo*)CModelInfo::GetModelInfo(id);
		if(mi && mi->IsBuilding())
			mi->SetupBigBuilding(minID, maxID);
	}
}

void
SetModelInfoFlags(CSimpleModelInfo *mi, uint32 flags)
{
	mi->m_wetRoadReflection =	!!(flags & 1);
	mi->m_noFade =	!!(flags & 2);
	mi->m_drawLast =	!!(flags & (4|8));
	mi->m_additive =	!!(flags & 8);
	mi->m_isSubway =	!!(flags & 0x10);
	mi->m_ignoreLight =	!!(flags & 0x20);
	mi->m_noZwrite =	!!(flags & 0x40);
	mi->m_noShadows =	!!(flags & 0x80);
	mi->m_ignoreDrawDist =	!!(flags & 0x100);
	mi->m_isCodeGlass =	!!(flags & 0x200);
	mi->m_isArtistGlass =	!!(flags & 0x400);
}

int
CFileLoader::LoadObject(const char *line)
{
	int id, numObjs;
	char model[24], txd[24];
	float dist[3];
	uint32 flags;
	int damaged;
	CSimpleModelInfo *mi;

	if(sscanf(line, "%d %s %s %d", &id, model, txd, &numObjs) != 4)
		return 0;	// game returns return value

	switch(numObjs){
	case 1:
		sscanf(line, "%d %s %s %d %f %d",
			&id, model, txd, &numObjs, &dist[0], &flags);
		damaged = 0;
		break;
	case 2:
		sscanf(line, "%d %s %s %d %f %f %d",
			&id, model, txd, &numObjs, &dist[0], &dist[1], &flags);
		damaged = dist[0] < dist[1] ?	// Are distances increasing?
			0 :	// Yes, no damage model
			1;	// No, 1 is damaged
		break;
	case 3:
		sscanf(line, "%d %s %s %d %f %f %f %d",
			&id, model, txd, &numObjs, &dist[0], &dist[1], &dist[2], &flags);
		damaged = dist[0] < dist[1] ?	// Are distances increasing?
				(dist[1] < dist[2] ? 0 : 2) :	// Yes, only 2 can still be a damage model
			1;	// No, 1 and 2 are damaged
		break;
	}

	mi = CModelInfo::AddSimpleModel(id);
	mi->SetModelName(model);
	mi->SetNumAtomics(numObjs);
	mi->SetLodDistances(dist);
	SetModelInfoFlags(mi, flags);
	mi->m_firstDamaged = damaged;
	mi->SetTexDictionary(txd);
	MatchModelString(model, id);

	return id;
}

int
CFileLoader::LoadTimeObject(const char *line)
{
	int id, numObjs;
	char model[24], txd[24];
	float dist[3];
	uint32 flags;
	int timeOn, timeOff;
	int damaged;
	CTimeModelInfo *mi, *other;

	if(sscanf(line, "%d %s %s %d", &id, model, txd, &numObjs) != 4)
		return 0;	// game returns return value

	switch(numObjs){
	case 1:
		sscanf(line, "%d %s %s %d %f %d %d %d",
			&id, model, txd, &numObjs, &dist[0], &flags, &timeOn, &timeOff);
		damaged = 0;
		break;
	case 2:
		sscanf(line, "%d %s %s %d %f %f %d %d %d",
			&id, model, txd, &numObjs, &dist[0], &dist[1], &flags, &timeOn, &timeOff);
		damaged = dist[0] < dist[1] ?	// Are distances increasing?
			0 :	// Yes, no damage model
			1;	// No, 1 is damaged
		break;
	case 3:
		sscanf(line, "%d %s %s %d %f %f %f %d %d %d",
			&id, model, txd, &numObjs, &dist[0], &dist[1], &dist[2], &flags, &timeOn, &timeOff);
		damaged = dist[0] < dist[1] ?	// Are distances increasing?
				(dist[1] < dist[2] ? 0 : 2) :	// Yes, only 2 can still be a damage model
			1;	// No, 1 and 2 are damaged
		break;
	}

	mi = CModelInfo::AddTimeModel(id);
	mi->SetModelName(model);
	mi->SetNumAtomics(numObjs);
	mi->SetLodDistances(dist);
	SetModelInfoFlags(mi, flags);
	mi->m_firstDamaged = damaged;
	mi->SetTimes(timeOn, timeOff);
	mi->SetTexDictionary(txd);
	other = mi->FindOtherTimeModel();
	if(other)
		other->SetOtherTimeModel(id);
	MatchModelString(model, id);

	return id;
}

int
CFileLoader::LoadWeaponObject(const char *line)
{
	int id, numObjs;
	char model[24], txd[24], animFile[16];
	float dist;
	CWeaponModelInfo *mi;

	sscanf(line, "%d %s %s %s %d %f", &id, model, txd, animFile, &numObjs, &dist);

	mi = CModelInfo::AddWeaponModel(id);
	mi->SetModelName(model);
	mi->SetNumAtomics(1);
	mi->m_lodDistances[0] = dist;
	mi->SetTexDictionary(txd);
	mi->SetAnimFile(animFile);
	mi->SetColModel(&CTempColModels::ms_colModelWeapon);
	MatchModelString(model, id);
	return id;
}

void
CFileLoader::LoadClumpObject(const char *line)
{
	int id;
	char model[24], txd[24];
	CClumpModelInfo *mi;

	if(sscanf(line, "%d %s %s", &id, model, txd) == 3){
		mi = CModelInfo::AddClumpModel(id);
		mi->SetModelName(model);
		mi->SetTexDictionary(txd);
		mi->SetColModel(&CTempColModels::ms_colModelBBox);
	}
}

void
CFileLoader::LoadVehicleObject(const char *line)
{
	int id;
	char model[24], txd[24];
	char type[8], handlingId[16], gamename[32], animFile[16], vehclass[12];
	uint32 frequency, comprules;
	int32 level, misc;
	float wheelScale, wheelScaleRear;
	CVehicleModelInfo *mi;
	char *p;

	// Клиент читает строку машины одним sscanf с 15 спецификациями (0xb6e7e0,
	// формат на 0x4231f4): ..., freq, flags, comprules, wheelModelId,
	// масштаб колеса перед, масштаб колеса зад. Двух последних спецификаций
	// VC не знал — отсюда два float вместо одного.
	// В записях без заднего размера (boat/plane — 11 полей, старый VC-формат —
	// 13) sscanf оставит переменную нетронутой, поэтому значение по умолчанию
	// задано заранее и заднее подменяется передним, если его не прочитали.
	wheelScale = 1.0f;
	wheelScaleRear = 1.0f;
	int parsed = sscanf(line, "%d %s %s %s %s %s %s %s %d %d %x %d %f %f",
		&id, model, txd,
		type, handlingId, gamename, animFile, vehclass,
		&frequency, &level, &comprules, &misc, &wheelScale, &wheelScaleRear);
	if(parsed < 14)
		wheelScaleRear = wheelScale;

	mi = CModelInfo::AddVehicleModel(id);
	mi->SetModelName(model);
	mi->SetTexDictionary(txd);
	mi->SetAnimFile(animFile);
	for(p = gamename; *p; p++)
		if(*p == '_') *p = ' ';
	strcpy(mi->m_gameName, gamename);
	mi->m_level = level;
	mi->m_compRules = comprules;

	// Порядок сравнений и присваиваемые значения повторяют IDE-парсер клиента
	// (Black Russia, функция 0xb4b0fc): там 13 типов, и «bike» — это 5, а не 9,
	// как в GTA:SA. Масштаб колеса клиент хранит двумя числами (переднее и
	// заднее), здесь — одним, потому что физика порта пока берёт только его;
	// см. VehicleModelInfo.h, eVehicleType.
	if(strcmp(type, "car") == 0){
		mi->m_wheelId = misc;
		mi->m_wheelScaleFront = wheelScale;
		mi->m_wheelScaleRear = wheelScaleRear;
		mi->m_vehicleType = VEHICLE_TYPE_CAR;
	}else if(strcmp(type, "boat") == 0){
		mi->m_vehicleType = VEHICLE_TYPE_BOAT;
	}else if(strcmp(type, "train") == 0){
		mi->m_vehicleType = VEHICLE_TYPE_TRAIN;
	}else if(strcmp(type, "heli") == 0){
		mi->m_vehicleType = VEHICLE_TYPE_HELI;
	}else if(strcmp(type, "plane") == 0){
		mi->m_planeLodId = misc;
		// у клиента plane принудительно получает 1.0 в оба поля (0xb4b29c)
		mi->m_wheelScaleFront = 1.0f;
		mi->m_wheelScaleRear = 1.0f;
		mi->m_vehicleType = VEHICLE_TYPE_PLANE;
	}else if(strcmp(type, "bike") == 0){
		mi->m_bikeSteerAngle = misc;
		mi->m_wheelScaleFront = wheelScale;
		mi->m_wheelScaleRear = wheelScaleRear;
		mi->m_vehicleType = VEHICLE_TYPE_BIKE;
	}else if(strcmp(type, "trike") == 0){
		mi->m_bikeSteerAngle = misc;
		mi->m_wheelScaleFront = wheelScale;
		mi->m_wheelScaleRear = wheelScaleRear;
		mi->m_vehicleType = VEHICLE_TYPE_TRIKE;
	}else if(strcmp(type, "mtruck") == 0){
		mi->m_wheelId = misc;
		mi->m_wheelScaleFront = wheelScale;
		mi->m_wheelScaleRear = wheelScaleRear;
		mi->m_vehicleType = VEHICLE_TYPE_MTRUCK;
	}else if(strcmp(type, "trailer") == 0){
		mi->m_wheelId = misc;
		mi->m_wheelScaleFront = wheelScale;
		mi->m_wheelScaleRear = wheelScaleRear;
		mi->m_vehicleType = VEHICLE_TYPE_TRAILER;
	}else if(strcmp(type, "quad") == 0){
		mi->m_wheelId = misc;
		mi->m_wheelScaleFront = wheelScale;
		mi->m_wheelScaleRear = wheelScaleRear;
		mi->m_vehicleType = VEHICLE_TYPE_QUAD;
	}else if(strcmp(type, "bmx") == 0){
		mi->m_bikeSteerAngle = misc;
		mi->m_wheelScaleFront = wheelScale;
		mi->m_wheelScaleRear = wheelScaleRear;
		mi->m_vehicleType = VEHICLE_TYPE_BMX;
	}else if(strcmp(type, "prtcrn") == 0){
		mi->m_bikeSteerAngle = misc;
		mi->m_wheelScaleFront = wheelScale;
		mi->m_wheelScaleRear = wheelScaleRear;
		mi->m_vehicleType = VEHICLE_TYPE_PRTCRN;
	}else if(strcmp(type, "craneof") == 0){
		mi->m_wheelId = misc;
		mi->m_wheelScaleFront = wheelScale;
		mi->m_wheelScaleRear = wheelScaleRear;
		mi->m_vehicleType = VEHICLE_TYPE_CRANEOF;
	}else
		assert(0);

	mi->m_handlingId = mod_HandlingManager.GetHandlingId(handlingId);

	if(strcmp(vehclass, "normal") == 0)
		mi->m_vehicleClass = CCarCtrl::NORMAL;
	else if(strcmp(vehclass, "poorfamily") == 0)
		mi->m_vehicleClass = CCarCtrl::POOR;
	else if(strcmp(vehclass, "richfamily") == 0)
		mi->m_vehicleClass = CCarCtrl::RICH;
	else if(strcmp(vehclass, "executive") == 0)
		mi->m_vehicleClass = CCarCtrl::EXEC;
	else if(strcmp(vehclass, "worker") == 0)
		mi->m_vehicleClass = CCarCtrl::WORKER;
	else if(strcmp(vehclass, "big") == 0)
		mi->m_vehicleClass = CCarCtrl::BIG;
	else if(strcmp(vehclass, "taxi") == 0)
		mi->m_vehicleClass = CCarCtrl::TAXI;
	else if(strcmp(vehclass, "moped") == 0)
		mi->m_vehicleClass = CCarCtrl::MOPED;
	else if(strcmp(vehclass, "motorbike") == 0)
		mi->m_vehicleClass = CCarCtrl::MOTORBIKE;
	else if(strcmp(vehclass, "leisureboat") == 0)
		mi->m_vehicleClass = CCarCtrl::LEISUREBOAT;
	else if(strcmp(vehclass, "workerboat") == 0)
		mi->m_vehicleClass = CCarCtrl::WORKERBOAT;
	else if(strcmp(vehclass, "ignore") == 0) {
		mi->m_vehicleClass = -1;
		return;
	}
	CCarCtrl::AddToCarArray(id, mi->m_vehicleClass);
	mi->m_frequency = frequency;
}

void
CFileLoader::LoadPedObject(const char *line)
{
	int id;
	char model[24], txd[24];
	char pedType[24], pedStats[24], animGroup[24], animFile[16];
	int carsCanDrive;
	CPedModelInfo *mi;
	int animGroupId;
	int radio1, radio2;

	sscanf(line, "%d %s %s %s %s %s %x %s %d %d",
	          &id, model, txd,
	          pedType, pedStats, animGroup, &carsCanDrive,
		  animFile, &radio1, &radio2);

	mi = CModelInfo::AddPedModel(id);
	mi->SetModelName(model);
	mi->SetTexDictionary(txd);
	mi->SetAnimFile(animFile);
	mi->SetColModel(&CTempColModels::ms_colModelPed1);
	mi->m_pedType = CPedType::FindPedType(pedType);
	mi->m_pedStatType = CPedStats::GetPedStatType(pedStats);
	for(animGroupId = 0; animGroupId < NUM_ANIM_ASSOC_GROUPS; animGroupId++)
		if(strcmp(animGroup, CAnimManager::GetAnimGroupName((AssocGroupId)animGroupId)) == 0)
			break;
	assert(animGroupId < NUM_ANIM_ASSOC_GROUPS);
	mi->m_animGroup = animGroupId;
	mi->m_carsCanDrive = carsCanDrive;
	mi->radio1 = radio1;
	mi->radio2 = radio2;
}

int
CFileLoader::LoadPathHeader(const char *line, int &type)
{
	int id;
	char modelname[32];

	sscanf(line, "%d %d %s", &type, &id, modelname);
	return id;
}

void
CFileLoader::LoadPedPathNode(const char *line, int id, int node)
{
	int type, next, cross, numLeft, numRight, speed, flags;
	float x, y, z, width, spawnRate;

	if(sscanf(line, "%d %d %d %f %f %f %f %d %d %d %d %f",
			&type, &next, &cross, &x, &y, &z, &width, &numLeft, &numRight,
			&speed, &flags, &spawnRate) != 12)
		spawnRate = 1.0f;

	if(id == -1)
		ThePaths.StoreDetachedNodeInfoPed(node, type, next, x, y, z,
			width, !!cross, !!(flags&1), !!(flags&4), spawnRate*15.0f);
	else
		ThePaths.StoreNodeInfoPed(id, node, type, next, x, y, z,
			width, !!cross, spawnRate*15.0f);
}

void
CFileLoader::LoadCarPathNode(const char *line, int id, int node, bool waterPath)
{
	int type, next, cross, numLeft, numRight, speed, flags;
	float x, y, z, width, spawnRate;

	if(sscanf(line, "%d %d %d %f %f %f %f %d %d %d %d %f",
			&type, &next, &cross, &x, &y, &z, &width, &numLeft, &numRight,
			&speed, &flags, &spawnRate) != 12)
		spawnRate = 1.0f;

	if(id == -1)
		ThePaths.StoreDetachedNodeInfoCar(node, type, next, x, y, z, width, numLeft, numRight,
			!!(flags&1), !!(flags&4), speed, !!(flags&2), waterPath, spawnRate * 15, false);
	else
		ThePaths.StoreNodeInfoCar(id, node, type, next, x, y, z, 0, numLeft, numRight,
			!!(flags&1), !!(flags&4), speed, !!(flags&2), waterPath, spawnRate * 15);
}


void
CFileLoader::Load2dEffect(const char *line)
{
	int id, r, g, b, a, type, ptype;
	float x, y, z;
	char corona[32], shadow[32];
	int shadowIntens, lightType, roadReflection, flare, flags, probability;
	CBaseModelInfo *mi;
	C2dEffect *effect;
	char *p;

	sscanf(line, "%d %f %f %f %d %d %d %d %d", &id, &x, &y, &z, &r, &g, &b, &a, &type);

	CTxdStore::PushCurrentTxd();
	CTxdStore::SetCurrentTxd(CTxdStore::FindTxdSlot("particle"));

	mi = CModelInfo::GetModelInfo(id);
	effect = CModelInfo::Get2dEffectStore().Alloc();
	mi->Add2dEffect(effect);
	effect->pos = CVector(x, y, z);
	effect->col = CRGBA(r, g, b, a);
	effect->type = type;

	switch(effect->type){
	case EFFECT_LIGHT:
		while(*line++ != '"');
		p = corona;
		while(*line != '"') *p++ = *line++;
		*p = '\0';
		line++;

		while(*line++ != '"');
		p = shadow;
		while(*line != '"') *p++ = *line++;
		*p = '\0';
		line++;

		sscanf(line, "%f %f %f %f %d %d %d %d %d",
			&effect->light.dist,
			&effect->light.range,
			&effect->light.size,
			&effect->light.shadowSize,
			&shadowIntens, &lightType, &roadReflection, &flare, &flags);
		effect->light.corona = RwTextureRead(corona, nil);
		effect->light.shadow = RwTextureRead(shadow, nil);
		effect->light.shadowIntensity = shadowIntens;
		effect->light.lightType = lightType;
		effect->light.roadReflection = roadReflection;
		effect->light.flareType = flare;

		if(flags & LIGHTFLAG_FOG_ALWAYS)
			flags &= ~LIGHTFLAG_FOG_NORMAL;
		effect->light.flags = flags;
		break;

	case EFFECT_PARTICLE:
		sscanf(line, "%d %f %f %f %d %d %d %d %d %d %f %f %f %f",
			&id, &x, &y, &z, &r, &g, &b, &a, &type,
			&effect->particle.particleType,
			&effect->particle.dir.x,
			&effect->particle.dir.y,
			&effect->particle.dir.z,
			&effect->particle.scale);
		break;

	case EFFECT_ATTRACTOR:
		sscanf(line, "%d %f %f %f %d %d %d %d %d %d %f %f %f %d",
			&id, &x, &y, &z, &r, &g, &b, &a, &type,
			&flags,
			&effect->attractor.dir.x,
			&effect->attractor.dir.y,
			&effect->attractor.dir.z,
			&probability);
		effect->attractor.type = flags;
#ifdef FIX_BUGS
		effect->attractor.probability = Clamp(probability, 0, 255);
#else
		effect->attractor.probability = probability;
#endif
		break;
	case EFFECT_PED_ATTRACTOR:
		sscanf(line, "%d %f %f %f %d %d %d %d %d %d %f %f %f %f %f %f",
			&id, &x, &y, &z, &r, &g, &b, &a, &type,
			&ptype,
			&effect->pedattr.queueDir.x,
			&effect->pedattr.queueDir.y,
			&effect->pedattr.queueDir.z,
			&effect->pedattr.useDir.x,
			&effect->pedattr.useDir.y,
			&effect->pedattr.useDir.z);
		effect->pedattr.type = ptype;
		break;
	}

	CTxdStore::PopCurrentTxd();
}

void
CFileLoader::LoadScene(const char *filename)
{
	enum {
		NONE,
		INST,
		ZONE,
		CULL,
		OCCL,
		PICK,
		PATH,
	};
	char *line;
	int fd;
	int section;
	int pathType, pathIndex;

	section = NONE;
	pathIndex = -1;
	debug("Creating objects from %s...\n", filename);

	fd = CFileMgr::OpenFile(filename, "rb");
	assert(fd > 0);
	for(line = CFileLoader::LoadLine(fd); line; line = CFileLoader::LoadLine(fd)){
		if(*line == '\0' || *line == '#')
			continue;

		if(section == NONE){
			if(isLine4(line, 'i','n','s','t')) section = INST;
			else if(isLine4(line, 'z','o','n','e')) section = ZONE;
			else if(isLine4(line, 'c','u','l','l')) section = CULL;
			else if(isLine4(line, 'p','i','c','k')) section = PICK;
			else if(isLine4(line, 'p','a','t','h')) section = PATH;
			else if(isLine4(line, 'o','c','c','l')) section = OCCL;
		}else if(isLine3(line, 'e','n','d')){
			section = NONE;
		}else switch(section){
		case INST:
			LoadObjectInstance(line);
			break;
		case ZONE:
			LoadZone(line);
			break;
		case CULL:
			LoadCullZone(line);
			break;
		case OCCL:
			LoadOcclusionVolume(line);
			break;
		case PICK:
			// unused
			LoadPickup(line);
			break;
		case PATH:
			if(pathIndex == -1){
				LoadPathHeader(line, pathType);
				pathIndex = 0;
			}else{
				if(pathType == 0)
					LoadPedPathNode(line, -1, pathIndex);
				else if (pathType == 1)
					LoadCarPathNode(line, -1, pathIndex, false);
				else if (pathType == 2)
					LoadCarPathNode(line, -1, pathIndex, true);
				pathIndex++;
				if(pathIndex == 12)
					pathIndex = -1;
			}
			break;
		}
	}
	CFileMgr::CloseFile(fd);

	debug("Finished loading IPL\n");
}

void
CFileLoader::LoadObjectInstance(const char *line)
{
	int id;
	char name[24];
	RwV3d trans, scale, axis;
	float angle;
	CSimpleModelInfo *mi;
	RwMatrix *xform;
	CEntity *entity;
	float area;

	if(sscanf(line, "%d %s %f %f %f %f %f %f %f %f %f %f %f",
	          &id, name, &area,
	          &trans.x, &trans.y, &trans.z,
	          &scale.x, &scale.y, &scale.z,
	          &axis.x, &axis.y, &axis.z, &angle) != 13){
		if(sscanf(line, "%d %s %f %f %f %f %f %f %f %f %f %f",
		          &id, name,
		          &trans.x, &trans.y, &trans.z,
		          &scale.x, &scale.y, &scale.z,
		          &axis.x, &axis.y, &axis.z, &angle) != 12)
			return;
		area = 0;
	}

	mi = (CSimpleModelInfo*)CModelInfo::GetModelInfo(id);
	if(mi == nil)
		return;
	assert(mi->IsSimple());

	if(!CStreaming::IsObjectInCdImage(id))
		debug("Not in cdimage %s\n", mi->GetModelName());

	angle = -RADTODEG(2.0f * Acos(angle));
	xform = RwMatrixCreate();
	RwMatrixRotate(xform, &axis, angle, rwCOMBINEREPLACE);
	RwMatrixTranslate(xform, &trans, rwCOMBINEPOSTCONCAT);

	if(mi->GetObjectID() == -1){
		if(ThePaths.IsPathObject(id)){
			entity = new CTreadable;
			ThePaths.RegisterMapObject((CTreadable*)entity);
		}else
			entity = new CBuilding;
		entity->SetModelIndexNoCreate(id);
		entity->GetMatrix() = CMatrix(xform);
#if defined(ANDROID) && defined(CUSTOM_MODELS)
		entity->m_level = CCustomModels::ReplacesNativeWorldMap()
			? LEVEL_GENERIC
			: CTheZones::GetLevelFromPosition(&entity->GetPosition());
#else
		entity->m_level = CTheZones::GetLevelFromPosition(&entity->GetPosition());
#endif
		entity->m_area = area;
		if(mi->IsBuilding()){
			if(mi->m_isBigBuilding)
				entity->SetupBigBuilding();
			if(mi->m_isSubway)
				entity->bIsSubway = true;
		}
		if(mi->GetLargestLodDistance() < 2.0f)
			entity->bIsVisible = false;
		CWorld::Add(entity);

		CColModel *col = entity->GetColModel();
		if(col->numSpheres || col->numBoxes || col->numTriangles){
			if(col->level != 0)
				CColStore::GetBoundingBox(col->level).ContainRect(entity->GetBoundRect());
		}else
			entity->bUsesCollision = false;

		if(entity->GetPosition().z + col->boundingBox.min.z < 6.0f)
			entity->bUnderwater = true;
	}else{
		entity = new CDummyObject;
		entity->SetModelIndexNoCreate(id);
		entity->GetMatrix() = CMatrix(xform);
		// m_level has to be set before CWorld::Add(): Add() files a big building
		// into ms_bigBuildingsList[m_level], so assigning it afterwards left the
		// dummy in the list slot of whatever level it had at construction and the
		// per-level removal passes then never found it again.
#if defined(ANDROID) && defined(CUSTOM_MODELS)
		entity->m_level = CCustomModels::ReplacesNativeWorldMap()
			? LEVEL_GENERIC
			: CTheZones::GetLevelFromPosition(&entity->GetPosition());
#else
		entity->m_level = CTheZones::GetLevelFromPosition(&entity->GetPosition());
#endif
		entity->m_area = area;
		CWorld::Add(entity);
		if(IsGlass(entity->GetModelIndex()) && !mi->m_isArtistGlass)
			entity->bIsVisible = false;
	}

	RwMatrixDestroy(xform);
}

void
CFileLoader::LoadZone(const char *line)
{
	char name[24];
	int type, level;
	float minx, miny, minz;
	float maxx, maxy, maxz;

	if(sscanf(line, "%s %d %f %f %f %f %f %f %d", name, &type, &minx, &miny, &minz, &maxx, &maxy, &maxz, &level) == 9)
		CTheZones::CreateZone(name, (eZoneType)type, minx, miny, minz, maxx, maxy, maxz, (eLevelName)level);
}

void
CFileLoader::LoadCullZone(const char *line)
{
	CVector pos;
	float minx, miny, minz;
	float maxx, maxy, maxz;
	int flags;
	int wantedLevelDrop = 0;

	sscanf(line, "%f %f %f %f %f %f %f %f %f %d %d",
		&pos.x, &pos.y, &pos.z,
		&minx, &miny, &minz,
		&maxx, &maxy, &maxz,
		&flags, &wantedLevelDrop);
	CCullZones::AddCullZone(pos, minx, maxx, miny, maxy, minz, maxz, flags, wantedLevelDrop);
}

// unused
void
CFileLoader::LoadPickup(const char *line)
{
	int id;
	float x, y, z;

	sscanf(line, "%d %f %f %f", &id, &x, &y, &z);
}

void
CFileLoader::LoadOcclusionVolume(const char *line)
{
	float x, y, z;
	float width, length, height;
	float angle;

	sscanf(line, "%f %f %f %f %f %f %f",
		&x, &y, &z,
		&width, &length, &height,
		&angle);
	COcclusion::AddOne(x, y, z + height/2.0f, width, length, height, angle);
}


// unused
void
CFileLoader::ReloadPaths(const char *filename)
{
	enum {
		NONE,
		PATH,
	};
	char *line;
	int section = NONE;
	int id, pathType, pathIndex = -1;
	debug("Reloading paths from %s...\n", filename);

	int fd = CFileMgr::OpenFile(filename, "r");
	assert(fd > 0);
	for (line = CFileLoader::LoadLine(fd); line; line = CFileLoader::LoadLine(fd)) {
		if (*line == '\0' || *line == '#')
			continue;

		if (section == NONE) {
			if (isLine4(line, 'p','a','t','h')) {
				section = PATH;
				ThePaths.AllocatePathFindInfoMem(4500);
			}
		} else if (isLine3(line, 'e','n','d')) {
			section = NONE;
		} else {
			switch (section) {
				case PATH:
					if (pathIndex == -1) {
						id = LoadPathHeader(line, pathType);
						pathIndex = 0;
					} else {
						if(pathType == 0)
							LoadPedPathNode(line, id, pathIndex);
						else if (pathType == 1)
							LoadCarPathNode(line, id, pathIndex, false);
						else if (pathType == 2)
							LoadCarPathNode(line, id, pathIndex, true);
						pathIndex++;
						if (pathIndex == 12)
							pathIndex = -1;
					}
					break;
				default:
					break;
			}
		}
	}
	CFileMgr::CloseFile(fd);
}

void
CFileLoader::ReloadObjectTypes(const char *filename)
{
	enum {
		NONE,
		OBJS,
		TOBJ,
		TWODFX
	};
	char *line;
	int section = NONE;
	CModelInfo::ReInit2dEffects();
	debug("Reloading object types from %s...\n", filename);

	CFileMgr::ChangeDir("\\DATA\\MAPS\\");
	int fd = CFileMgr::OpenFile(filename, "r");
	assert(fd > 0);
	CFileMgr::ChangeDir("\\");
	for (line = CFileLoader::LoadLine(fd); line; line = CFileLoader::LoadLine(fd)) {
		if (*line == '\0' || *line == '#')
			continue;

		if (section == NONE) {
			if (isLine4(line, 'o','b','j','s')) section = OBJS;
			else if (isLine4(line, 't','o','b','j')) section = TOBJ;
			else if (isLine4(line, '2','d','f','x')) section = TWODFX;
		} else if (isLine3(line, 'e','n','d')) {
			section = NONE;
		} else {
			switch (section) {
				case OBJS:
				case TOBJ:
					ReloadObject(line);
					break;
				case TWODFX:
					Load2dEffect(line);
					break;
				default:
					break;
			}
		}
	}
	CFileMgr::CloseFile(fd);
}

void
CFileLoader::ReloadObject(const char *line)
{
	int id, numObjs;
	char model[24], txd[24];
	float dist[3];
	uint32 flags;
	CSimpleModelInfo *mi;

	if(sscanf(line, "%d %s %s %d", &id, model, txd, &numObjs) != 4)
		return;

	switch(numObjs){
	case 1:
		sscanf(line, "%d %s %s %d %f %d",
			&id, model, txd, &numObjs, &dist[0], &flags);
		break;
	case 2:
		sscanf(line, "%d %s %s %d %f %f %d",
			&id, model, txd, &numObjs, &dist[0], &dist[1], &flags);
		break;
	case 3:
		sscanf(line, "%d %s %s %d %f %f %f %d",
			&id, model, txd, &numObjs, &dist[0], &dist[1], &dist[2], &flags);
		break;
	}

	mi = (CSimpleModelInfo*) CModelInfo::GetModelInfo(id);
	if (
#ifdef FIX_BUGS
		mi &&
#endif
	    mi->GetModelType() == MITYPE_SIMPLE && !strcmp(mi->GetModelName(), model) && mi->m_numAtomics == numObjs) {
		mi->SetLodDistances(dist);
		SetModelInfoFlags(mi, flags);
	} else {
		printf("Can't reload %s\n", model);
	}
}

// unused mobile function - crashes
void
CFileLoader::ReLoadScene(const char *filename)
{
	char *line;
	CFileMgr::ChangeDir("\\DATA\\");
	int fd = CFileMgr::OpenFile(filename, "r");
	assert(fd > 0);
	CFileMgr::ChangeDir("\\");

	for (line = CFileLoader::LoadLine(fd); line; line = CFileLoader::LoadLine(fd)) {
		if (*line == '#')
			continue;

		if (strncmp(line, "EXIT", 4) == 0)
			break;

		if (strncmp(line, "IDE", 3) == 0) {
			LoadObjectTypes(line + 4);
		}
	}
	CFileMgr::CloseFile(fd);
}
