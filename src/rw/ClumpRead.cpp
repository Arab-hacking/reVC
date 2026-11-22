#include "common.h"

struct rpGeometryList
{
	RpGeometry **geometries;
	int32 numGeoms;
};

struct rpAtomicBinary
{
	RwInt32 frameIndex;
	RwInt32 geomIndex;
	RwInt32 flags;
	RwInt32 unused;
};

static int32 numberGeometrys;
static int32 streamPosition;
static rpGeometryList gGeomList;
static rwFrameList gFrameList;
static RpClumpChunkInfo gClumpInfo;

// The frame read owns the frame list until the clump is finished, but the two
// halves of the read are separate calls and several things can happen in
// between: a failure inside the first half, an aborted load that calls
// RpClumpGtaCancelStream, or a second model starting to load before the first
// one finished. In every one of those cases gFrameList.frames is freed and
// numFrames keeps saying how many frames used to be there, so the second half
// walks a nil array - the crash in debug-27 is exactly that read at address 0
// inside RpClumpGtaStreamRead2. The list is tracked explicitly instead.
static bool gFrameListValid;

// Release the array only; a completed clump owns the frame hierarchy.
static void
FrameListRelease(void)
{
	if(gFrameList.frames)
		rwFrameListDeinitialize(&gFrameList);
	gFrameList.numFrames = 0;
	gFrameListValid = false;
}

// Before a clump takes ownership, cancellation must also destroy the hierarchy.
static void
FrameListDiscard(void)
{
	if(gFrameListValid && gFrameList.frames && gFrameList.numFrames > 0)
		RwFrameDestroyHierarchy(gFrameList.frames[0]);
	FrameListRelease();
}

rpGeometryList*
GeometryListStreamRead1(RwStream *stream, rpGeometryList *geomlist)
{
	int i;
	RwUInt32 size, version;
	RwInt32 numGeoms;

	numberGeometrys = 0;
	if(!RwStreamFindChunk(stream, rwID_STRUCT, &size, &version))
		return nil;
	assert(size == 4);
	if(RwStreamRead(stream, &numGeoms, 4) != 4)
		return nil;

	numberGeometrys = numGeoms/2;
	geomlist->numGeoms = numGeoms;
	if(geomlist->numGeoms > 0){
		geomlist->geometries = (RpGeometry**)RwMalloc(geomlist->numGeoms * sizeof(RpGeometry*));
		if(geomlist->geometries == nil){
			geomlist->numGeoms = 0;
			return nil;
		}
		memset(geomlist->geometries, 0, geomlist->numGeoms * sizeof(RpGeometry*));
	}else
		geomlist->geometries = nil;

	for(i = 0; i < numberGeometrys; i++){
		if(!RwStreamFindChunk(stream, rwID_GEOMETRY, nil, &version))
			return nil;
		geomlist->geometries[i] = RpGeometryStreamRead(stream);
		if(geomlist->geometries[i] == nil)
			return nil;
	}

	return geomlist;
}

rpGeometryList*
GeometryListStreamRead2(RwStream *stream, rpGeometryList *geomlist)
{
	int i;
	RwUInt32 version;

	for(i = numberGeometrys; i < geomlist->numGeoms; i++){
		if(!RwStreamFindChunk(stream, rwID_GEOMETRY, nil, &version))
			return nil;
		geomlist->geometries[i] = RpGeometryStreamRead(stream);
		if(geomlist->geometries[i] == nil)
			return nil;
	}

	return geomlist;
}

void
GeometryListDeinitialize(rpGeometryList *geomlist)
{
	int i;

	for(i = 0; i < geomlist->numGeoms; i++)
		if(geomlist->geometries[i])
			RpGeometryDestroy(geomlist->geometries[i]);

	if(geomlist->numGeoms){
		RwFree(geomlist->geometries);
		geomlist->geometries = nil;
		geomlist->numGeoms = 0;
	}
}

RpAtomic*
ClumpAtomicStreamRead(RwStream *stream, rwFrameList *frmList, rpGeometryList *geomList)
{
	RwUInt32 size, version;
	rpAtomicBinary a;
	RpAtomic *atomic;

	numberGeometrys = 0;
	if(!RwStreamFindChunk(stream, rwID_STRUCT, &size, &version))
		return nil;
	assert(size <= sizeof(rpAtomicBinary));
	if(RwStreamRead(stream, &a, size) != size)
		return nil;

	atomic = RpAtomicCreate();
	if(atomic == nil)
		return nil;

	RpAtomicSetFlags(atomic, a.flags);

	if(frmList->numFrames){
		assert(a.frameIndex < frmList->numFrames);
		RpAtomicSetFrame(atomic, frmList->frames[a.frameIndex]);
	}

	if(geomList->numGeoms){
		assert(a.geomIndex < geomList->numGeoms);
		RpAtomicSetGeometry(atomic, geomList->geometries[a.geomIndex], 0);
	}else{
		RpGeometry *geom;
		if(!RwStreamFindChunk(stream, rwID_GEOMETRY, nil, &version)){
			RpAtomicDestroy(atomic);
			return nil;
		}
		geom = RpGeometryStreamRead(stream);
		if(geom == nil){
			RpAtomicDestroy(atomic);
			return nil;
		}
		RpAtomicSetGeometry(atomic, geom, 0);
		RpGeometryDestroy(geom);
	}

	return atomic;
}

bool
RpClumpGtaStreamRead1(RwStream *stream)
{
	RwUInt32 size, version;

	// A new start invalidates any unfinished split read, even if its header fails.
	if(gFrameListValid){
		GeometryListDeinitialize(&gGeomList);
		FrameListDiscard();
	}

	if(!RwStreamFindChunk(stream, rwID_STRUCT, &size, &version))
		return false;
	if(version >= 0x33000){
		assert(size == 12);
		if(RwStreamRead(stream, &gClumpInfo, 12) != 12)
			return false;
	}else{
		assert(size == 4);
		if(RwStreamRead(stream, &gClumpInfo, 4) != 4)
			return false;
	}

	// A previous load that never finished leaves its frames behind; dropping them
	// here keeps one model's hierarchy out of the next one's.
	if(gFrameListValid)
		FrameListRelease();
	gFrameList.numFrames = 0;

	if(!RwStreamFindChunk(stream, rwID_FRAMELIST, nil, &version))
		return false;
	if(rwFrameListStreamRead(stream, &gFrameList) == nil){
		// librw may already have freed the array on a partial read.
		gFrameList.frames = nil;
		gFrameList.numFrames = 0;
		return false;
	}
	// From here on the list is owned by this half of the read: whatever fails
	// below has to release it, and the second half may only use it if this flag
	// is still set.
	gFrameListValid = true;

	if(!RwStreamFindChunk(stream, rwID_GEOMETRYLIST, nil, &version)){
		FrameListDiscard();
		return false;
	}
	if(GeometryListStreamRead1(stream, &gGeomList) == nil){
		GeometryListDeinitialize(&gGeomList);
		FrameListDiscard();
		return false;
	}
	streamPosition = STREAMPOS(stream);
	return true;
}

RpClump*
RpClumpGtaStreamRead2(RwStream *stream)
{
	int i;
	RwUInt32 version;
	RpAtomic *atomic;
	RpClump *clump;

	// The first half has to have completed and nothing may have cancelled it
	// since. Without this the read below walks a frame list that was already
	// freed - the nil dereference this function used to take on a retried or
	// aborted load.
	if(!gFrameListValid || gFrameList.frames == nil || gFrameList.numFrames <= 0){
		REVC_LOG_WARNING("Clump stream: finished without a valid frame list (frames=%d)", gFrameList.numFrames);
		GeometryListDeinitialize(&gGeomList);
		FrameListDiscard();
		return nil;
	}

	clump = RpClumpCreate();
	if(clump == nil){
		GeometryListDeinitialize(&gGeomList);
		FrameListDiscard();
		return nil;
	}

	RwStreamSkip(stream, streamPosition - STREAMPOS(stream));

	if(GeometryListStreamRead2(stream, &gGeomList) == nil){
		GeometryListDeinitialize(&gGeomList);
		RpClumpDestroy(clump);
		FrameListDiscard();
		return nil;
	}

	RpClumpSetFrame(clump, gFrameList.frames[0]);

	for(i = 0; i < gClumpInfo.numAtomics; i++){
		if(!RwStreamFindChunk(stream, rwID_ATOMIC, nil, &version)){
			GeometryListDeinitialize(&gGeomList);
			RpClumpDestroy(clump);
			FrameListRelease();
			return nil;
		}

		atomic = ClumpAtomicStreamRead(stream, &gFrameList, &gGeomList);
		if(atomic == nil){
			GeometryListDeinitialize(&gGeomList);
			RpClumpDestroy(clump);
			FrameListRelease();
			return nil;
		}

		RpClumpAddAtomic(clump, atomic);
	}

	GeometryListDeinitialize(&gGeomList);
	FrameListRelease();
	return clump;
}

void
RpClumpGtaCancelStream(void)
{
	GeometryListDeinitialize(&gGeomList);
	FrameListDiscard();
}
