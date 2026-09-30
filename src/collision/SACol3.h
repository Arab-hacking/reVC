#ifndef __GTA_SACOL3_H__
#define __GTA_SACOL3_H__

// the header is meant to be usable stand-alone (tests link only against it),
// so make sure the integer types reVC uses are known; typedefs of the same
// type are legal to repeat, so this doesn't collide with common.h/rwplcore.h
#include <string.h>
#include <stdint.h>
typedef uint8_t uint8;
typedef int16_t int16;
typedef uint16_t uint16;
typedef uint32_t uint32;

// Parser for the collision format GTA:San Andreas stores inside model files.
//
// SA vehicle models (and several other model types) carry a "COL3" collision
// block in the clump extension (Rockstar chunk 0x0253F2FA "CollisionModel").
// Vice City instead keeps collision in separate models/coll/*.col files, so
// reVC has to convert the SA block into its own CColModel.
//
// The parser only knows the file format and hands out pointers into the
// caller's buffer; turning those into a CColModel is the caller's job
// (see CFileLoader::LoadSAVehicleColModel). Keeping it free of engine types
// makes the format testable on its own (tools/sa-vehicle-verification).
//
// COL3 layout (all offsets in the header are relative to the byte after the
// FourCC, hence the -4 below; see gtamods.com/wiki/Collision_File):
//   0x00 'COL3'
//   0x04 uint32 size of the block
//   0x08 char   name[22]      (source file name)
//   0x1E uint16 model id
//   0x20 TBounds: vec3 min, vec3 max, vec3 center, float radius  (40 bytes)
//   0x48 uint16 numSpheres, uint16 numBoxes, uint16 numFaces,
//        uint8 numLines, uint8 unused, uint32 flags
//   0x54 6 uint32 offsets: spheres, boxes, lines, vertices, faces, planes
//   (COL3 v3: uint32 shadow mesh count + 2 offsets follow)

struct SACol3
{
	float boundingMin[3];
	float boundingMax[3];
	float boundingCenter[3];
	float boundingRadius;

	uint32 numSpheres;	// 20 bytes each: vec3 center, float radius, uint8 surface, uint8 flag
	uint32 numBoxes;	// 28 bytes each: vec3 min, vec3 max, uint8 surface, uint8 flag
	uint32 numVerts;	// int16 x,y,z (fixed point, /128)
	uint32 numFaces;	// 8 bytes each: uint16 a,b,c, uint8 material, uint8 light

	uint8 *spheres;
	uint8 *boxes;
	int16 *verts;
	uint8 *faces;
};

#define SACOL3_SPHERE_SIZE 20
#define SACOL3_BOX_SIZE    28
#define SACOL3_VERT_SIZE   6
#define SACOL3_FACE_SIZE   8

static inline bool
SACol3Parse(const uint8 *buf, uint32 size, SACol3 &col)
{
	uint32 numFaces, offSpheres, offBoxes, offVerts, offFaces;
	uint32 vertsEnd;

	if(size < 0x6C || memcmp(buf, "COL3", 4) != 0)
		return false;
	// offsets in the file are relative to the byte after the FourCC
	numFaces   = *(const uint16*)(buf+0x4C);
	offSpheres = *(const uint32*)(buf+0x54) + 4;
	offBoxes   = *(const uint32*)(buf+0x58) + 4;
	offVerts   = *(const uint32*)(buf+0x60) + 4;
	offFaces   = *(const uint32*)(buf+0x64) + 4;

	// the vertex section ends where the face section starts; that gives us the
	// vertex count, which the format itself doesn't store
	if(offVerts > offFaces || (offFaces - offVerts) % SACOL3_VERT_SIZE != 0)
		return false;
	vertsEnd = offFaces;

	col.numSpheres = *(const uint16*)(buf+0x48);
	col.numBoxes   = *(const uint16*)(buf+0x4A);
	col.numFaces   = numFaces;
	col.numVerts   = (vertsEnd - offVerts) / SACOL3_VERT_SIZE;

	if(offSpheres + col.numSpheres*SACOL3_SPHERE_SIZE > size ||
	   offBoxes + col.numBoxes*SACOL3_BOX_SIZE > size ||
	   offVerts + col.numVerts*SACOL3_VERT_SIZE > size ||
	   offFaces + col.numFaces*SACOL3_FACE_SIZE > size)
		return false;
	memcpy(col.boundingMin, buf+0x20, 12);
	memcpy(col.boundingMax, buf+0x2C, 12);
	memcpy(col.boundingCenter, buf+0x38, 12);
	col.boundingRadius = *(const float*)(buf+0x44);

	col.spheres = (uint8*)(buf + offSpheres);
	col.boxes   = (uint8*)(buf + offBoxes);
	col.verts   = (int16*)(buf + offVerts);
	col.faces   = (uint8*)(buf + offFaces);

	// face indices have to be inside the vertex array
	for(uint32 i = 0; i < col.numFaces; i++){
		const uint8 *f = col.faces + i*SACOL3_FACE_SIZE;
		if(*(const uint16*)f >= col.numVerts ||
		   *(const uint16*)(f+2) >= col.numVerts ||
		   *(const uint16*)(f+4) >= col.numVerts)
			return false;
	}
	return true;
}

#endif // __GTA_SACOL3_H__
