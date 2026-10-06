#include "common.h"

#include "CustomCol.h"

#ifdef CUSTOM_MODELS

#include <string.h>
#include <math.h>

#include "SavehDiag.h"
#include "SurfaceTable.h"	// the surfaces Vice City knows

namespace customcol {

// Version 1 ("COLL"), the layout the game's collision reader expects:
//     fourcc, size, name[22], id, bounds[40],
//     uint32 numSpheres, TSphere[], uint32 numLines, TLine[], uint32 numBoxes,
//     TBox[], uint32 numVertices, TVertex[], uint32 numFaces, TFace[]
// Version 2/3 ("COL2"/"COL3"), what the .cls containers hold and what the
// converter of the containers produces:
//     fourcc, size, name[22], id, bounds[40], counts, flags, offsets, data
// (the offsets are counted from the end of the fourcc)
#define FCC_COLL 0x4C4C4F43
#define FCC_COL2 0x324C4F43
#define FCC_COL3 0x334C4F43

#define BLOCK_HEAD 72		// fourcc + size + name + id + bounds, both versions
#define V2_HEADER  48		// counts, flags and the offsets

#define V1_SPHERE 20		// radius, center, surface
#define V1_BOX    28		// min, max, surface
#define V1_VERTEX 12		// float x, y, z
#define V1_FACE   16		// uint32 a, b, c, surface
#define V2_SPHERE 20		// center, radius, surface
#define V2_BOX    28		// min, max, surface
#define V2_VERTEX 6			// int16 x, y, z, fixed point (1/128)
#define V2_FACE   8			// uint16 a, b, c, material, light

// a box that is inside out or a sphere the size of the map is not what the
// file says, the order of the two bounding volumes must have been the other one
static bool
SaneBounds(const float *mn, const float *mx)
{
	for(int i = 0; i < 3; i++)
		if(!(mx[i] >= mn[i]) || !(mn[i] > -1.0e6f) || !(mx[i] < 1.0e6f))
			return false;
	return true;
}

static bool
SaneRadius(float r)
{
	return r == r && r > 0.001f && r < 1000.0f;
}

// the game crashes on more than this in one collision model
#define MAX_VERTICES 32767
#define MAX_FACES    32767

static uint16
r16(const uint8 *p)
{
	return (uint16)(p[0] | (p[1] << 8));
}

static int16
r16s(const uint8 *p)
{
	return (int16)r16(p);
}

static uint32
r32(const uint8 *p)
{
	return (uint32)p[0] | ((uint32)p[1] << 8) | ((uint32)p[2] << 16) | ((uint32)p[3] << 24);
}

static float
rFloat(const uint8 *p)
{
	float f;
	memcpy(&f, p, 4);
	return f;
}

static void
w16(std::vector<uint8> &v, uint16 x)
{
	v.push_back((uint8)(x & 0xFF));
	v.push_back((uint8)(x >> 8));
}

static void
w32(std::vector<uint8> &v, uint32 x)
{
	v.push_back((uint8)(x & 0xFF));
	v.push_back((uint8)((x >> 8) & 0xFF));
	v.push_back((uint8)((x >> 16) & 0xFF));
	v.push_back((uint8)((x >> 24) & 0xFF));
}

static void
wFloat(std::vector<uint8> &v, float f)
{
	uint32 x;
	memcpy(&x, &f, 4);
	w32(v, x);
}

// One surface: the game only knows a few materials, a value behind its table
// would be read from memory the game does not own.
static uint8
GameSurface(uint8 material, Stats &stats)
{
	if(material > SURFACE_CONCRETE_BEACH){
		stats.materials++;
		return SURFACE_DEFAULT;
	}
	return material;
}

// everything of a block the converter works with
struct Block
{
	bool usable;
	bool v1;				// already in the shape the game reads
	const uint8 *name;		// 22 bytes, not zero terminated
	uint16 id;

	// bounding volumes, always in the order of the game after parsing
	bool bounds;			// true when they could be read
	float radius;
	float center[3], bmin[3], bmax[3];

	// geometry (in the layout of the file version it came from)
	uint16 nS, nB, nF;
	uint32 nV;
	const uint8 *sph, *box, *vtx, *fac;

	// version 1 blocks are copied over as they are
	const uint8 *copyFrom;
	size_t copySize;
};

static bool
ParseBlock(const uint8 *p, size_t n, Block &b)
{
	uint32 fourcc, size, offS, offB, offV, offF;
	const uint8 *t;

	memset(&b, 0, sizeof(b));
	if(n < BLOCK_HEAD)
		return false;
	fourcc = r32(p);
	if(fourcc != FCC_COLL && fourcc != FCC_COL2 && fourcc != FCC_COL3)
		return false;
	size = r32(p + 4);
	if(size < 24 + 40 || size > n - 8)
		return false;

	b.name = p + 8;
	b.id = r16(p + 8 + 22);
	b.usable = true;

	// The bounding volumes are stored in two different orders: the game and
	// the newer versions start with the box, the first version starts with
	// the sphere. Which one a block carries is decided by which of the two
	// orders makes a box that contains something (a box that is inside out is
	// invisible to the game), so a file written by either tool is understood.
	t = p + 32;
	if(fourcc == FCC_COLL){
		b.radius = rFloat(t);
		memcpy(b.center, t + 4, 12);
		memcpy(b.bmin, t + 16, 12);
		memcpy(b.bmax, t + 28, 12);
		b.v1 = true;
		b.bounds = true;
		// bounds and body both need nothing done to them
		b.copyFrom = p + 32;
		b.copySize = 8 + size - 32;
		return true;
	}

	float boxMin[3], boxMax[3], boxCenter[3], boxRadius;
	float sphRadius, sphCenter[3], sphMin[3], sphMax[3];
	memcpy(boxMin, t, 12);
	memcpy(boxMax, t + 12, 12);
	// box first: the sphere follows it as radius then center, the way both
	// the game and brformats lay it out
	boxRadius = rFloat(t + 24);
	memcpy(boxCenter, t + 28, 12);
	sphRadius = rFloat(t);
	memcpy(sphCenter, t + 4, 12);
	memcpy(sphMin, t + 16, 12);
	memcpy(sphMax, t + 28, 12);

	if(SaneBounds(boxMin, boxMax) && SaneRadius(boxRadius)){
		memcpy(b.bmin, boxMin, 12);
		memcpy(b.bmax, boxMax, 12);
		memcpy(b.center, boxCenter, 12);
		b.radius = boxRadius;
		b.bounds = true;
	}else if(SaneBounds(sphMin, sphMax) && SaneRadius(sphRadius)){
		memcpy(b.bmin, sphMin, 12);
		memcpy(b.bmax, sphMax, 12);
		memcpy(b.center, sphCenter, 12);
		b.radius = sphRadius;
		b.bounds = true;
	}else{
		// nothing usable in there: built from the geometry instead
		memcpy(b.bmin, boxMin, 12);
		memcpy(b.bmax, boxMax, 12);
		memcpy(b.center, boxCenter, 12);
		b.radius = boxRadius;
	}

	if(size < 40 + V2_HEADER)
		return false;
	const uint8 *h = p + BLOCK_HEAD;
	b.nS = r16(h + 0);
	b.nB = r16(h + 2);
	b.nF = r16(h + 4);
	offS = r32(h + 12);
	offB = r32(h + 16);
	offV = r32(h + 24);
	offF = r32(h + 28);

	// The offsets are counted from the fourcc plus four, the way the game
	// reads them: offset 116 of a block whose header ends after 120 bytes
	// means the first byte behind the header. Unlike the first version of the
	// format, the arrays carry no count of their own - the counts are in the
	// header - so an offset points straight at the data.
	uint32 limit = size + 4;		// everything the block holds, counted that way
	if(b.nS && (!offS || (size_t)offS + (size_t)b.nS*V2_SPHERE > limit))
		return false;
	if(b.nB && (!offB || (size_t)offB + (size_t)b.nB*V2_BOX > limit))
		return false;
	if(b.nF && (!offF || (size_t)offF + (size_t)b.nF*V2_FACE > limit))
		return false;
	if(offV && (size_t)offV + V2_VERTEX > limit)
		return false;
	if(b.nF && !offV)
		return false;			// faces without vertices

	// The vertex array is the only one whose length is not known before it is
	// read, so the offset of the faces is the one that can sit on the wrong
	// byte: the container converter starts every array on a four byte border
	// but counts the offsets from the fourcc plus four, which leaves the face
	// offset up to three bytes short when the vertices padded the array.
	if(b.nF && offF){
		uint32 aligned = (offF + 3) & ~3u;
		uint32 want = b.nF * V2_FACE;
		if(aligned != offF && offF + want != limit && aligned + want == limit)
			offF = aligned;
	}

	b.sph = offS ? p + 4 + offS : nil;
	b.box = offB ? p + 4 + offB : nil;
	b.vtx = offV ? p + 4 + offV : nil;
	b.fac = offF ? p + 4 + offF : nil;

	// The newer versions do not store the number of vertices. The space
	// between the vertex array and the one behind it is the limit, the faces
	// say which of the vertices are really there. The game itself counts the
	// vertices of a shadow mesh the same way.
	if(b.vtx){
		uint32 avail;
		if(offF && offF > offV)
			avail = (offF - offV) / V2_VERTEX;
		else
			avail = (limit - offV) / V2_VERTEX;
		uint32 hi = 0, i;
		for(i = 0; i < b.nF; i++){
			const uint8 *f = b.fac + (size_t)i*V2_FACE;
			uint32 v[3];
			v[0] = r16(f); v[1] = r16(f + 2); v[2] = r16(f + 4);
			for(int k = 0; k < 3; k++)
				if(v[k] + 1 > hi)
					hi = v[k] + 1;
		}
		if(hi > avail)
			return false;		// the faces want vertices that are not there
		b.nV = hi;
	}
	if(b.nV > MAX_VERTICES || b.nF > MAX_FACES)
		return false;
	return true;
}

// A collision model whose bounding volumes are missing or upside down would be
// invisible to the game, so they are built from the geometry in that case.
static void
FixBounds(const Block &b, float radius[1], float center[3], float bmin[3], float bmax[3])
{
	uint32 i;

	*radius = b.radius;
	memcpy(center, b.center, 12);
	memcpy(bmin, b.bmin, 12);
	memcpy(bmax, b.bmax, 12);
	if(b.bounds)
		return;

	float mn[3] = { 1e30f, 1e30f, 1e30f }, mx[3] = { -1e30f, -1e30f, -1e30f };
	for(i = 0; i < b.nS; i++){
		const uint8 *s = b.sph + (size_t)i*V2_SPHERE;
		float c[3], r = rFloat(s + 12);
		memcpy(c, s, 12);
		for(int k = 0; k < 3; k++){
			if(c[k] - r < mn[k]) mn[k] = c[k] - r;
			if(c[k] + r > mx[k]) mx[k] = c[k] + r;
		}
	}
	for(i = 0; i < b.nB; i++){
		const uint8 *s = b.box + (size_t)i*V2_BOX;
		for(int k = 0; k < 3; k++){
			if(rFloat(s + k*4) < mn[k]) mn[k] = rFloat(s + k*4);
			if(rFloat(s + 12 + k*4) > mx[k]) mx[k] = rFloat(s + 12 + k*4);
		}
	}
	for(i = 0; i < b.nV; i++){
		const uint8 *v = b.vtx + (size_t)i*V2_VERTEX;
		for(int k = 0; k < 3; k++){
			float f = r16s(v + k*2) / 128.0f;
			if(f < mn[k]) mn[k] = f;
			if(f > mx[k]) mx[k] = f;
		}
	}
	if(mn[0] > mx[0])
		return;				// no geometry at all, nothing to build

	memcpy(bmin, mn, 12);
	memcpy(bmax, mx, 12);
	center[0] = (mn[0] + mx[0]) * 0.5f;
	center[1] = (mn[1] + mx[1]) * 0.5f;
	center[2] = (mn[2] + mx[2]) * 0.5f;
	float dx = (mx[0] - mn[0]) * 0.5f, dy = (mx[1] - mn[1]) * 0.5f, dz = (mx[2] - mn[2]) * 0.5f;
	*radius = sqrtf(dx*dx + dy*dy + dz*dz);
}

static void
WriteBlock(std::vector<uint8> &out, const Block &b, Stats &stats)
{
	size_t start = out.size();
	uint32 i;

	w32(out, FCC_COLL);
	w32(out, 0);					// patched below
	for(i = 0; i < 22; i++)
		out.push_back(b.name[i]);	// the name is not zero terminated in the file
	w16(out, b.id);

	if(b.v1){
		out.insert(out.end(), b.copyFrom, b.copyFrom + b.copySize);
		uint32 size = (uint32)(out.size() - start - 8);
		out[start+4] = (uint8)(size & 0xFF);
		out[start+5] = (uint8)((size >> 8) & 0xFF);
		out[start+6] = (uint8)((size >> 16) & 0xFF);
		out[start+7] = (uint8)((size >> 24) & 0xFF);
		return;
	}

	float radius, center[3], bmin[3], bmax[3];
	FixBounds(b, &radius, center, bmin, bmax);
	wFloat(out, radius);
	for(i = 0; i < 3; i++) wFloat(out, center[i]);
	for(i = 0; i < 3; i++) wFloat(out, bmin[i]);
	for(i = 0; i < 3; i++) wFloat(out, bmax[i]);

	// spheres: the game has the radius first, the newer versions have it last
	w32(out, b.nS);
	for(i = 0; i < b.nS; i++){
		const uint8 *s = b.sph + (size_t)i*V2_SPHERE;
		wFloat(out, rFloat(s + 12));
		out.insert(out.end(), s, s + 12);
		out.push_back(GameSurface(s[16], stats));
		out.push_back(0); out.push_back(0); out.push_back(0);
		stats.spheres++;
	}

	// lines (never used by the containers, but the section is part of the format)
	w32(out, 0);

	w32(out, b.nB);
	for(i = 0; i < b.nB; i++){
		const uint8 *s = b.box + (size_t)i*V2_BOX;
		out.insert(out.end(), s, s + 24);
		out.push_back(GameSurface(s[24], stats));
		out.push_back(0); out.push_back(0); out.push_back(0);
		stats.boxes++;
	}

	w32(out, b.nV);
	for(i = 0; i < b.nV; i++){
		const uint8 *v = b.vtx + (size_t)i*V2_VERTEX;
		for(int k = 0; k < 3; k++)
			wFloat(out, r16s(v + k*2) / 128.0f);
		stats.vertices++;
	}

	// The number of faces has to be known before they are written: faces that
	// point at a vertex which is not there would make the game read whatever
	// follows the collision model.
	uint32 valid = 0;
	for(i = 0; i < b.nF; i++){
		const uint8 *f = b.fac + (size_t)i*V2_FACE;
		if(r16(f) < b.nV && r16(f + 2) < b.nV && r16(f + 4) < b.nV)
			valid++;
	}
	stats.dropped += b.nF - valid;
	if(valid > MAX_FACES){
		stats.dropped += valid - MAX_FACES;
		valid = MAX_FACES;
	}
	w32(out, valid);
	uint32 written = 0;
	for(i = 0; i < b.nF && written < valid; i++){
		const uint8 *f = b.fac + (size_t)i*V2_FACE;
		if(r16(f) >= b.nV || r16(f + 2) >= b.nV || r16(f + 4) >= b.nV)
			continue;
		w32(out, r16(f));
		w32(out, r16(f + 2));
		w32(out, r16(f + 4));
		out.push_back(GameSurface(f[6], stats));
		out.push_back(0); out.push_back(0); out.push_back(0);
		stats.faces++;
		written++;
	}

	uint32 size = (uint32)(out.size() - start - 8);
	out[start+4] = (uint8)(size & 0xFF);
	out[start+5] = (uint8)((size >> 8) & 0xFF);
	out[start+6] = (uint8)((size >> 16) & 0xFF);
	out[start+7] = (uint8)((size >> 24) & 0xFF);
}

std::vector<uint8>
ToGameFormat(const std::vector<uint8> &col, Stats &stats)
{
	std::vector<uint8> out;
	size_t pos = 0;

	memset(&stats, 0, sizeof(stats));
	while(pos + 8 <= col.size()){
		const uint8 *p = col.data() + pos;
		uint32 fourcc = r32(p);
		if(fourcc != FCC_COLL && fourcc != FCC_COL2 && fourcc != FCC_COL3){
			// the blocks are stored one after another; a gap means the file
			// is not what it should be
			pos++;
			continue;
		}
		uint32 size = r32(p + 4);
		size_t avail = col.size() - pos;
		if(size > avail - 8)
			size = (uint32)(avail - 8);

		Block b;
		if(ParseBlock(p, avail, b)){
			WriteBlock(out, b, stats);
			stats.blocks++;
		}else
			stats.bad++;

		pos += 8 + size;
	}
	return out;
}

}

#endif // CUSTOM_MODELS
