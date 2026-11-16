#ifndef __GTA_SACOL3_H__
#define __GTA_SACOL3_H__

// the header is meant to be usable stand-alone (tests link only against it),
// so make sure the integer types reVC uses are known; typedefs of the same
// type are legal to repeat, so this doesn't collide with common.h/rwplcore.h
#include <string.h>
#include <stdint.h>
#include <cmath>
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

// Read little-endian fields without alignment assumptions. DFF extension
// chunks need not be naturally aligned.
static inline uint32 SAColRead32(const uint8 *p)
{
	return uint32(p[0]) | uint32(p[1]) << 8 | uint32(p[2]) << 16 | uint32(p[3]) << 24;
}
static inline uint16 SAColRead16(const uint8 *p) { return uint16(p[0]) | uint16(p[1]) << 8; }
static inline bool
SACol3Parse(const uint8 *buf, uint32 size, SACol3 &col)
{
	if(!buf || size < 0x6C) return false;
	const bool v3 = memcmp(buf, "COL3", 4) == 0;
	if(!v3 && memcmp(buf, "COL2", 4) != 0) return false;
	const uint32 header = v3 ? 0x78u : 0x6Cu;
	if(size < header || SAColRead32(buf+4) != size-8) return false;
	SACol3 parsed = {};
	parsed.numSpheres = SAColRead16(buf+0x48);
	parsed.numBoxes = SAColRead16(buf+0x4A);
	parsed.numFaces = SAColRead16(buf+0x4C);
	// The receiving physics structures use signed 16-bit counters.
	if(parsed.numSpheres > 32767 || parsed.numBoxes > 32767 || parsed.numFaces > 32767) return false;
	auto section = [&](uint32 field, uint32 count, uint32 width, uint32 &at) -> bool {
		if(!count) { at = 0; return true; }
		uint32 offset = SAColRead32(buf+field);
		if(offset > size-4) return false;
		at = offset+4;
		return at >= header && at <= size && count <= (size-at)/width;
	};
	uint32 spheres, boxes, faces, verts;
	if(!section(0x54, parsed.numSpheres, SACOL3_SPHERE_SIZE, spheres) ||
	   !section(0x58, parsed.numBoxes, SACOL3_BOX_SIZE, boxes) ||
	   !section(0x64, parsed.numFaces, SACOL3_FACE_SIZE, faces)) return false;
	if(parsed.numFaces){
		// SA infers the vertex count from face indices. Padding/face groups
		// between the vertex and face arrays are not extra vertices.
		uint32 highest = 0;
		for(uint32 i = 0; i < parsed.numFaces; i++)
			for(uint32 j = 0; j < 3; j++){
				uint32 index = SAColRead16(buf+faces+i*8+j*2);
				if(index > highest) highest = index;
			}
		parsed.numVerts = highest+1;
		if(parsed.numVerts > 32767 || !section(0x60, parsed.numVerts, SACOL3_VERT_SIZE, verts) ||
		   verts > faces || parsed.numVerts > (faces-verts)/SACOL3_VERT_SIZE) return false;
	}else verts = 0;
	memcpy(parsed.boundingMin, buf+0x20, 12);
	memcpy(parsed.boundingMax, buf+0x2C, 12);
	memcpy(parsed.boundingCenter, buf+0x38, 12);
	memcpy(&parsed.boundingRadius, buf+0x44, 4);
	if(!std::isfinite(parsed.boundingRadius) || parsed.boundingRadius < 0) return false;
	for(int k = 0; k < 3; k++)
		if(!std::isfinite(parsed.boundingMin[k]) || !std::isfinite(parsed.boundingMax[k]) ||
		   !std::isfinite(parsed.boundingCenter[k]) || parsed.boundingMin[k] > parsed.boundingMax[k]) return false;
	// The view is published only after all floating geometry is validated.
	for(uint32 i = 0; i < parsed.numSpheres; i++)
		for(uint32 k = 0; k < 4; k++){
			float value; memcpy(&value, buf+spheres+i*20+k*4, 4);
			if(!std::isfinite(value) || (k == 3 && value < 0)) return false;
		}
	for(uint32 i = 0; i < parsed.numBoxes; i++)
		for(uint32 k = 0; k < 3; k++){
			float mn, mx;
			memcpy(&mn, buf+boxes+i*28+k*4, 4);
			memcpy(&mx, buf+boxes+i*28+12+k*4, 4);
			if(!std::isfinite(mn) || !std::isfinite(mx) || mn > mx) return false;
		}
	parsed.spheres = parsed.numSpheres ? (uint8*)buf+spheres : nullptr;
	parsed.boxes = parsed.numBoxes ? (uint8*)buf+boxes : nullptr;
	parsed.verts = parsed.numVerts ? (int16*)(buf+verts) : nullptr;
	parsed.faces = parsed.numFaces ? (uint8*)buf+faces : nullptr;
	col = parsed;
	return true;
}

#endif // __GTA_SACOL3_H__
