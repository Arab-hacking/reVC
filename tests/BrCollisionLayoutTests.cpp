// The .cls collision records, built byte for byte the way PUNCT ANALYSE v45
// section 10.3 documents them, and converted back to the COLL body the game
// reads.
//
// The point of this file is that the fixtures are not shaped around whatever
// the converter happened to do. Each record is assembled from the field list in
// the analysis - name[24] at +0x00, the structure at +0x18, arrays addressed as
// st + (field - 0x1c), spheres x,y,z,r, vertices 6 bytes s16/128, triangles
// HHHBB ending the record - and then the converter has to produce the numbers
// that geometry implies. A converter that reads the old layout (name[20] +
// CED2, pointers as field-4, float3 vertices) cannot pass these.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../src/extras/custom/BrCollisionFormat.h"

static int failures;

static void
Check(bool cond, const char *what)
{
	if(!cond){
		printf("FAIL: %s\n", what);
		failures++;
	}
}

// --- little helpers that append at a fixed position of a growing buffer -----
static void W32(std::vector<uint8_t> &b, size_t off, uint32_t n)
{
	for(int k = 0; k < 4; k++) b[off + k] = (uint8_t)(n >> (k * 8));
}
static void W16(std::vector<uint8_t> &b, size_t off, uint16_t n)
{
	b[off] = (uint8_t)n; b[off + 1] = (uint8_t)(n >> 8);
}
static void WF(std::vector<uint8_t> &b, size_t off, float f)
{
	uint32_t n; std::memcpy(&n, &f, 4); W32(b, off, n);
}
static float RF(const std::vector<uint8_t> &b, size_t off)
{
	float f; std::memcpy(&f, &b[off], 4); return f;
}
static uint32_t R32(const std::vector<uint8_t> &b, size_t off)
{
	return brcollision::U32(&b[off]);
}

// A CLST record in the corpus layout:
//
//   +0x00 name[24]
//   +0x18 st (0x48 bytes)
//   then the arrays, in the order spheres, boxes, vertices, triangles
//
// The structure's pointer fields hold `st + (array offset - 0x18)`, which is
// exactly the value `st + (field - 0x1c)` needs to resolve to the array.
struct ClstData {
	std::vector<float> spheres;   // x,y,z,r per sphere
	std::vector<float> boxes;     // min xyz, max xyz per box
	std::vector<int16_t> verts;   // x,y,z per vertex, /128
	std::vector<uint16_t> tris;   // a,b,c per triangle
	uint8_t flags;
	size_t aabbCount;             // records with flags & 8 carry this many SAABBs
	ClstData() : flags(0), aabbCount(0) {}
};

static std::vector<uint8_t>
MakeClst(const char *name, const ClstData &d, float radiusOverride = -1.0f)
{
	// Offsets below are payload-relative: the payload starts after the 8-byte
	// tag and size. Everything the analysis lists is measured from there.
	const size_t payload = 8;
	const size_t st = 0x18, stSize = 0x48;
	size_t nS = d.spheres.size() / 4, nB = d.boxes.size() / 6;
	size_t nV = d.verts.size() / 3, nT = d.tris.size() / 3;

	size_t off = st + stSize;
	size_t sphOff = nS ? off : 0; off += nS * 20;
	size_t boxOff = nB ? off : 0; off += nB * 28;
	size_t vertOff = nV ? off : 0; off += nV * 6;
	// Records with flags bit 3 carry a block of 28-byte axis-aligned boxes
	// between the vertices and the triangles, preceded by two bytes of padding
	// and followed by a u32 count. It is acceleration data, not geometry.
	size_t aabbOff = 0, aabbCount = d.aabbCount;
	if(d.flags & 8) {
		off += 2 + aabbCount * 28 + 4;
		aabbOff = (nV ? vertOff + nV * 6 + 2 : off - aabbCount * 28 - 4);
	}
	size_t triOff = nT ? off : 0; off += nT * 8;

	std::vector<uint8_t> b(payload + off, 0);
	std::memcpy(&b[0], "CLST", 4);
	std::memcpy(&b[payload + 0x00], name, std::strlen(name));

	// Bounds come from the geometry, the way the corpus does it: the centre is
	// the middle of the box and the radius reaches its far corner.
	float mn[3] = { 1e30f, 1e30f, 1e30f }, mx[3] = { -1e30f, -1e30f, -1e30f };
	for(size_t i = 0; i < nV; i++)
		for(int k = 0; k < 3; k++){
			float v = d.verts[i * 3 + k] / 128.0f;
			if(v < mn[k]) mn[k] = v;
			if(v > mx[k]) mx[k] = v;
		}
	for(size_t i = 0; i < nS; i++)
		for(int k = 0; k < 3; k++){
			float c = d.spheres[i * 4 + k], r = d.spheres[i * 4 + 3];
			if(c - r < mn[k]) mn[k] = c - r;
			if(c + r > mx[k]) mx[k] = c + r;
		}
	for(size_t i = 0; i < nB; i++)
		for(int k = 0; k < 3; k++){
			if(d.boxes[i * 6 + k] < mn[k]) mn[k] = d.boxes[i * 6 + k];
			if(d.boxes[i * 6 + 3 + k] > mx[k]) mx[k] = d.boxes[i * 6 + 3 + k];
		}
	if(mn[0] > mx[0]) for(int k = 0; k < 3; k++){ mn[k] = -0.5f; mx[k] = 0.5f; }
	float ctr[3] = { (mn[0] + mx[0]) * 0.5f, (mn[1] + mx[1]) * 0.5f, (mn[2] + mx[2]) * 0.5f };
	float dx = (mx[0] - mn[0]) * 0.5f, dy = (mx[1] - mn[1]) * 0.5f, dz = (mx[2] - mn[2]) * 0.5f;
	float rad = std::sqrt(dx * dx + dy * dy + dz * dz);
	if(radiusOverride >= 0.0f) rad = radiusOverride;

	size_t s = payload + st;
	for(int k = 0; k < 3; k++) WF(b, s + 0x00 + k * 4, ctr[k]);
	WF(b, s + 0x0c, rad);
	for(int k = 0; k < 3; k++) WF(b, s + 0x10 + k * 4, mn[k]);
	for(int k = 0; k < 3; k++) WF(b, s + 0x1c + k * 4, mx[k]);
	W16(b, s + 0x28, (uint16_t)(int16_t)nS);
	W16(b, s + 0x2a, (uint16_t)nB);
	W16(b, s + 0x2c, (uint16_t)(int16_t)nT);
	b[s + 0x30] = d.flags;
	// Array address = st + (field - 0x1c), and st is payload+0x18, so a field
	// holding `v` resolves to payload + (v - 4): the value stored is the array's
	// payload offset plus four - the same "+4 from the fourcc" base the COL3
	// offsets use. Checked against the corpus: with it the triangles end the
	// record in all 17 594 CLST records.
	auto ptr = [&](size_t payloadOff) { return payloadOff ? (uint32_t)(payloadOff + 4) : 0u; };
	W32(b, s + 0x34, ptr(sphOff));
	W32(b, s + 0x38, ptr(boxOff));
	W32(b, s + 0x40, ptr(vertOff));
	W32(b, s + 0x44, ptr(triOff));

	for(size_t i = 0; i < nS; i++)
		for(int k = 0; k < 4; k++) WF(b, payload + sphOff + i * 20 + k * 4, d.spheres[i * 4 + k]);
	for(size_t i = 0; i < nS; i++){
		b[payload + sphOff + i * 20 + 16] = 30;    // surface
		b[payload + sphOff + i * 20 + 17] = 1;     // piece
	}
	for(size_t i = 0; i < nB; i++){
		for(int k = 0; k < 6; k++) WF(b, payload + boxOff + i * 28 + k * 4, d.boxes[i * 6 + k]);
		b[payload + boxOff + i * 28 + 24] = 30;
		b[payload + boxOff + i * 28 + 25] = 2;
	}
	for(size_t i = 0; i < nV; i++)
		for(int k = 0; k < 3; k++)
			W16(b, payload + vertOff + i * 6 + k * 2, (uint16_t)d.verts[i * 3 + k]);
	if(d.flags & 8) {
		for(size_t i = 0; i < aabbCount; i++){
			size_t o = payload + aabbOff + i * 28;
			WF(b, o + 0, -1.0f); WF(b, o + 4, -1.0f); WF(b, o + 8, -1.0f);
			WF(b, o + 12, 1.0f); WF(b, o + 16, 1.0f); WF(b, o + 20, 1.0f);
			W16(b, o + 24, 0);
			W16(b, o + 26, (uint16_t)(nT ? nT - 1 : 0));
		}
		W32(b, payload + aabbOff + aabbCount * 28, (uint32_t)aabbCount);
	}
	for(size_t i = 0; i < nT; i++){
		for(int k = 0; k < 3; k++)
			W16(b, payload + triOff + i * 8 + k * 2, d.tris[i * 3 + k]);
		b[payload + triOff + i * 8 + 6] = 30;
		b[payload + triOff + i * 8 + 7] = 0;
	}
	W32(b, 4, (uint32_t)(b.size() - 8));
	return b;
}

// The COLL block the converter writes: tag, size, name[24] = 32 bytes, then the
// 40-byte bounds (radius, centre, bbox), then the five counts interleaved with
// their arrays - the same walk CFileLoader::LoadCollisionModel does.
static size_t ColSection(const std::vector<uint8_t> &g, int which, uint32_t &count)
{
	size_t at = 72;
	uint32_t c = R32(g, at); at += 4;
	if(which == 0){ count = c; return at; }
	at += (size_t)c * 20;
	c = R32(g, at); at += 4;
	if(which == 1){ count = c; return at; }
	at += (size_t)c * 24;
	c = R32(g, at); at += 4;
	if(which == 2){ count = c; return at; }
	at += (size_t)c * 28;
	c = R32(g, at); at += 4;
	if(which == 3){ count = c; return at; }
	at += (size_t)c * 12;
	c = R32(g, at); at += 4;
	count = c;
	return at;
}

static void
TestClstWithTriangles(void)
{
	ClstData d;
	// A quad in the xy plane, 2 m across, plus the two triangles that cover it.
	d.verts = { 0, 0, 0,  128, 0, 0,  128, 128, 0,  0, 128, 0 };
	d.tris = { 0, 1, 2,  0, 2, 3 };
	std::vector<uint8_t> rec = MakeClst("tri_model", d);

	std::vector<brcollision::Block> blocks;
	Check(brcollision::Index(rec.data(), rec.size(), blocks) == 0, "CLST record indexes cleanly");
	Check(blocks.size() == 1 && blocks[0].name == "tri_model", "CLST record keeps its name");

	std::vector<uint8_t> g;
	const char *why = nullptr;
	Check(brcollision::Convert(rec.data(), rec.size(), g, &why), "CLST with triangles converts");
	if(g.empty()){
		printf("  reason: %s\n", why ? why : "?");
		return;
	}
	Check(std::memcmp(g.data(), "COLL", 4) == 0, "converted block is a COLL block");
	Check(R32(g, 4) == g.size() - 8, "declared size covers the converted block");
	Check(std::strcmp((const char *)g.data() + 8, "tri_model") == 0, "converted block keeps the name");

	uint32_t n;
	size_t verts = ColSection(g, 3, n);
	Check(n == 4, "maxIndex+1 vertices, not the whole array");
	// x,y,z = 0,0,0 for the first; 1,0,0 for the second; (1,1,0) for the third.
	Check(std::fabs(RF(g, verts + 0) - 0.0f) < 1e-6f, "vertex 0 x");
	Check(std::fabs(RF(g, verts + 12) - 1.0f) < 1e-6f, "vertex 1 x is 128/128");
	Check(std::fabs(RF(g, verts + 24 + 4) - 1.0f) < 1e-6f, "vertex 2 y is 128/128");
	size_t tris = ColSection(g, 4, n);
	Check(n == 2, "both triangles kept");
	Check(tris + 2 * 16 == g.size(), "triangles end the converted block");
	Check(R32(g, tris + 0) == 0 && R32(g, tris + 4) == 1 && R32(g, tris + 8) == 2,
	      "triangle indices survive conversion");

	// The bounding sphere must still cover the geometry.
	float radius = RF(g, 32);
	Check(std::fabs(radius - std::sqrt(2.0f) * 0.5f) < 1e-4f, "radius is half the box diagonal");
}

static void
TestClstSphereOrder(void)
{
	// One sphere off the origin. If the converter reads CLST as r,x,y,z (the
	// COLL order) the radius comes out as x and the centre as (y,z,r), which
	// puts the sphere somewhere else entirely.
	ClstData d;
	d.spheres = { 10.0f, 20.0f, 30.0f, 2.5f };
	std::vector<uint8_t> rec = MakeClst("sphere_model", d);

	std::vector<uint8_t> g;
	Check(brcollision::Convert(rec.data(), rec.size(), g), "sphere-only CLST converts");
	if(g.empty()) return;
	uint32_t n;
	size_t sph = ColSection(g, 0, n);
	Check(n == 1, "one sphere converted");
	// The COLL body stores radius first.
	Check(std::fabs(RF(g, sph + 0) - 2.5f) < 1e-6f, "sphere radius is the fourth float of CLST");
	Check(std::fabs(RF(g, sph + 4) - 10.0f) < 1e-6f, "sphere x");
	Check(std::fabs(RF(g, sph + 8) - 20.0f) < 1e-6f, "sphere y");
	Check(std::fabs(RF(g, sph + 12) - 30.0f) < 1e-6f, "sphere z");
}

static void
TestClstBoxes(void)
{
	ClstData d;
	d.boxes = { -1.0f, -2.0f, -3.0f, 1.0f, 2.0f, 3.0f,
	             4.0f,  5.0f,  6.0f, 7.0f, 8.0f, 9.0f };
	std::vector<uint8_t> rec = MakeClst("box_model", d);
	std::vector<uint8_t> g;
	Check(brcollision::Convert(rec.data(), rec.size(), g), "box-only CLST converts");
	if(g.empty()) return;
	uint32_t n;
	size_t box = ColSection(g, 2, n);
	Check(n == 2, "both boxes converted");
	Check(std::fabs(RF(g, box + 0) - (-1.0f)) < 1e-6f, "box 0 min.x");
	Check(std::fabs(RF(g, box + 28 + 12) - 7.0f) < 1e-6f, "box 1 max.x");
}

static void
TestEmptyClstIsRefused(void)
{
	// 6 205 records in the corpus look exactly like this. Converting them would
	// install a model whose collision is empty and non-null, which is the
	// "player stands in nothing" case; the reason has to name it.
	ClstData d;
	std::vector<uint8_t> rec = MakeClst("empty_model", d);
	Check(rec.size() == 8 + 0x18 + 0x48, "an empty CLST is tag, size, name and the bare structure");
	std::vector<uint8_t> g;
	const char *why = nullptr;
	Check(!brcollision::Convert(rec.data(), rec.size(), g, &why), "empty CLST is not converted");
	Check(g.empty(), "nothing written for an empty CLST");
	Check(why != nullptr && std::strstr(why, "no spheres") != nullptr,
	      "the reason names the missing geometry");
}

static void
TestTruncatedTrianglesAreRefused(void)
{
	ClstData d;
	d.verts = { 0, 0, 0,  128, 0, 0,  0, 128, 0 };
	d.tris = { 0, 1, 2 };
	std::vector<uint8_t> rec = MakeClst("short_model", d);
	// Make the declared size one byte shorter than the record: the triangles no
	// longer end it, which is the check that catches a wrong pointer base.
	rec.resize(rec.size() - 1);
	W32(rec, 4, (uint32_t)(rec.size() - 8));
	std::vector<uint8_t> g;
	Check(!brcollision::Convert(rec.data(), rec.size(), g), "triangles that do not end the record are refused");
}

static void
TestTriangleIndexBeyondVerticesIsRefused(void)
{
	ClstData d;
	d.verts = { 0, 0, 0,  128, 0, 0,  0, 128, 0 };
	d.tris = { 0, 1, 2,  0, 1, 9 };      // 9 needs ten vertices
	std::vector<uint8_t> rec = MakeClst("bad_index", d);
	std::vector<uint8_t> g;
	Check(!brcollision::Convert(rec.data(), rec.size(), g),
	      "a triangle that needs vertices which are not there is refused");
}

static std::vector<uint8_t>
MakeClsfBoxes(const char *name, int numBoxes)
{
	std::vector<uint8_t> b(8 + 0x18 + 0x34 + (size_t)numBoxes * 28, 0);
	std::memcpy(&b[0], "CLSF", 4);
	std::memcpy(&b[8], name, std::strlen(name));
	size_t h = 8 + 0x18;      // the structure starts at payload+0x18, as for CLST
	WF(b, h + 0x00, -2.0f); WF(b, h + 0x04, -2.0f); WF(b, h + 0x08, -2.0f);
	WF(b, h + 0x0c,  2.0f); WF(b, h + 0x10,  2.0f); WF(b, h + 0x14,  2.0f);
	WF(b, h + 0x18,  0.0f); WF(b, h + 0x1c,  0.0f); WF(b, h + 0x20,  0.0f);
	WF(b, h + 0x24,  3.4641016f);
	W32(b, h + 0x30, (uint32_t)numBoxes);
	for(int i = 0; i < numBoxes; i++){
		size_t bo = h + 0x34 + (size_t)i * 28;
		WF(b, bo + 0, -1.0f); WF(b, bo + 4, -1.0f); WF(b, bo + 8, -1.0f);
		WF(b, bo + 12, 1.0f); WF(b, bo + 16, 1.0f); WF(b, bo + 20, 1.0f);
		b[bo + 24] = 30;
	}
	W32(b, 4, (uint32_t)(b.size() - 8));
	return b;
}

static std::vector<uint8_t>
MakeClsfTriangles(const char *name)
{
	// Vertices are float3 at +0x38; the main triangle array ends the record.
	// One "fast" entry of 16 bytes sits between them, as in the corpus.
	const uint32_t nv = 3, nt = 1;
	size_t vertOff = 8 + 0x18 + 0x38;
	size_t fastOff = vertOff + nv * 12;
	size_t triOff = fastOff + 16;
	std::vector<uint8_t> b(triOff + nt * 16, 0);
	std::memcpy(&b[0], "CLSF", 4);
	std::memcpy(&b[8], name, std::strlen(name));
	size_t h = 8 + 0x18;
	WF(b, h + 0x00, 0.0f); WF(b, h + 0x04, 0.0f); WF(b, h + 0x08, 0.0f);
	WF(b, h + 0x0c, 10.0f); WF(b, h + 0x10, 4.0f); WF(b, h + 0x14, 0.0f);
	WF(b, h + 0x18, 5.0f); WF(b, h + 0x1c, 2.0f); WF(b, h + 0x20, 0.0f);
	WF(b, h + 0x24, 5.3851648f);
	W32(b, h + 0x30, 0);            // numBoxes
	W32(b, h + 0x34, nt);           // numTriangles
	WF(b, vertOff + 0,  0.0f); WF(b, vertOff + 4,  0.0f); WF(b, vertOff + 8,  0.0f);
	WF(b, vertOff + 12, 10.0f); WF(b, vertOff + 16, 0.0f); WF(b, vertOff + 20, 0.0f);
	WF(b, vertOff + 24, 0.0f); WF(b, vertOff + 28, 4.0f); WF(b, vertOff + 32, 0.0f);
	W32(b, triOff + 0, 0); W32(b, triOff + 4, 1); W32(b, triOff + 8, 2);
	W32(b, triOff + 12, 30u << 24);
	W32(b, 4, (uint32_t)(b.size() - 8));
	return b;
}

static void
TestClsfBoxes(void)
{
	std::vector<uint8_t> rec = MakeClsfBoxes("clsf_boxes", 3);
	std::vector<brcollision::Block> blocks;
	Check(brcollision::Index(rec.data(), rec.size(), blocks) == 0, "CLSF record indexes cleanly");
	Check(blocks.size() == 1 && blocks[0].name == "clsf_boxes", "CLSF record keeps its name");

	std::vector<uint8_t> g;
	const char *why = nullptr;
	Check(brcollision::Convert(rec.data(), rec.size(), g, &why), "CLSF with boxes converts");
	if(g.empty()){
		printf("  reason: %s\n", why ? why : "?");
		return;
	}
	uint32_t n;
	ColSection(g, 2, n);
	Check(n == 3, "all three CLSF boxes converted");
	Check(std::fabs(RF(g, 32) - 3.4641016f) < 1e-4f, "CLSF header radius carried over");
}

static void
TestClsfTriangles(void)
{
	std::vector<uint8_t> rec = MakeClsfTriangles("clsf_tris");
	std::vector<uint8_t> g;
	const char *why = nullptr;
	Check(brcollision::Convert(rec.data(), rec.size(), g, &why), "CLSF with triangles converts");
	if(g.empty()){
		printf("  reason: %s\n", why ? why : "?");
		return;
	}
	uint32_t n;
	size_t verts = ColSection(g, 3, n);
	Check(n == 3, "CLSF float3 vertices counted from the indices");
	Check(std::fabs(RF(g, verts + 12) - 10.0f) < 1e-6f, "CLSF vertex 1 x (float3, not /128)");
	size_t tris = ColSection(g, 4, n);
	Check(n == 1, "CLSF main triangle converted");
	Check(tris + 16 == g.size(), "converted CLSF triangles end the block");
}

static void
TestEmptyClsfIsRefused(void)
{
	std::vector<uint8_t> rec = MakeClsfBoxes("clsf_empty", 0);
	// A real empty CLSF is 84 bytes: the counters are there, nothing follows.
	rec.resize(84);
	W32(rec, 4, (uint32_t)(rec.size() - 8));
	// A real empty CLSF is 84 bytes: the counters are there, nothing follows.
	rec.resize(84);
	W32(rec, 4, (uint32_t)(rec.size() - 8));
	std::vector<uint8_t> g;
	const char *why = nullptr;
	Check(!brcollision::Convert(rec.data(), rec.size(), g, &why), "empty CLSF is not converted");
	Check(why != nullptr && std::strstr(why, "no boxes") != nullptr,
	      "the reason names the missing geometry");
}

static void
TestContainerWalk(void)
{
	// Several records back to back, the second one padded, and a zero tail -
	// what an IMG entry actually hands over.
	ClstData a;
	a.verts = { 0, 0, 0,  128, 0, 0,  0, 128, 0 };
	a.tris = { 0, 1, 2 };
	std::vector<uint8_t> one = MakeClst("first_model", a);
	std::vector<uint8_t> two = MakeClsfBoxes("second_model", 2);
	std::vector<uint8_t> file = one;
	file.insert(file.end(), two.begin(), two.end());
	while(file.size() % 16) file.push_back(0);
	file.resize(file.size() + 64, 0);

	std::vector<brcollision::Block> blocks;
	size_t bad = brcollision::Index(file.data(), file.size(), blocks);
	Check(bad == 0, "padding after the last record is not an error");
	Check(blocks.size() == 2, "both records are indexed");
	if(blocks.size() == 2){
		Check(blocks[0].name == "first_model" && blocks[1].name == "second_model",
		      "names come from the 24-byte field at +8, not from CED2");
		Check(blocks[1].offset == one.size(), "second record follows the first");
	}

	// A record whose declared size is smaller than its own name field is not a
	// record; the walk has to stop rather than read the next one's bytes.
	std::vector<uint8_t> broken = MakeClst("broken", a);
	W32(broken, 4, 4);
	std::vector<brcollision::Block> none;
	Check(brcollision::Index(broken.data(), broken.size(), none) > 0, "a truncated record is reported");
	Check(none.empty(), "and no block is produced from it");
}


static void
TestNameWithCed2Marker(void)
{
	// The 20-byte name plus the four-byte marker is a real variant: the name has
	// no terminator and the marker sits where the padding would be.
	ClstData d;
	d.verts = { 0, 0, 0,  128, 0, 0,  0, 128, 0 };
	d.tris = { 0, 1, 2 };
	std::vector<uint8_t> rec = MakeClst("12345678901234567890", d);
	std::memcpy(&rec[8 + 20], "CED2", 4);

	std::vector<brcollision::Block> blocks;
	Check(brcollision::Index(rec.data(), rec.size(), blocks) == 0, "record with a CED2 marker indexes");
	Check(blocks.size() == 1 && blocks[0].name == "12345678901234567890",
	      "the marker is not part of a 20-character name");
	std::vector<uint8_t> g;
	Check(brcollision::Convert(rec.data(), rec.size(), g), "record with a CED2 marker converts");
	Check(!g.empty() && std::strcmp((const char *)g.data() + 8, "12345678901234567890") == 0,
	      "the converted block carries that same name");

	// Without the marker the same bytes are just a 24-character name, and a
	// shorter name is unaffected either way.
	std::vector<uint8_t> plain = MakeClst("short_name", d);
	std::vector<brcollision::Block> b2;
	Check(brcollision::Index(plain.data(), plain.size(), b2) == 0 &&
	      b2.size() == 1 && b2[0].name == "short_name", "a short name is read as-is");
}


static void
TestClstWithAabbBlock(void)
{
	// 5 154 of the 17 594 CLST records have flags bit 3 set, which puts a block
	// of 28-byte axis-aligned boxes between the vertices and the triangles. The
	// converter does not need that block (it is a candidate list, and the port
	// builds its own), but it must not trip over it: the vertices still end
	// before it and the triangles still end the record.
	ClstData d;
	d.flags = 0x0a;
	d.aabbCount = 2;
	d.verts = { 0, 0, 0,  128, 0, 0,  128, 128, 0,  0, 128, 0 };
	d.tris = { 0, 1, 2,  0, 2, 3 };
	std::vector<uint8_t> rec = MakeClst("aabb_model", d);

	std::vector<brcollision::Block> blocks;
	Check(brcollision::Index(rec.data(), rec.size(), blocks) == 0, "record with an AABB block indexes");
	std::vector<uint8_t> g;
	const char *why = nullptr;
	Check(brcollision::Convert(rec.data(), rec.size(), g, &why), "record with an AABB block converts");
	if(g.empty()){
		printf("  reason: %s\n", why ? why : "?");
		return;
	}
	uint32_t n;
	size_t verts = ColSection(g, 3, n);
	Check(n == 4, "vertices are still counted from the triangle indices");
	Check(std::fabs(RF(g, verts + 24 + 4) - 1.0f) < 1e-6f, "vertex data is not shifted by the AABB block");
	size_t tris = ColSection(g, 4, n);
	Check(n == 2, "both triangles converted with the AABB block present");
	Check(tris + 2 * 16 == g.size(), "and they still end the converted block");

	// The block is only acceleration data, so the geometry must be identical to
	// the same record without it.
	ClstData plain = d;
	plain.flags = 0x02;
	plain.aabbCount = 0;
	std::vector<uint8_t> flat = MakeClst("aabb_model", plain);
	std::vector<uint8_t> h;
	Check(brcollision::Convert(flat.data(), flat.size(), h), "the same record without the block converts");
	Check(g == h, "the AABB block does not change the converted geometry");
}

int
main(void)
{
	TestClstWithTriangles();
	TestClstSphereOrder();
	TestClstBoxes();
	TestEmptyClstIsRefused();
	TestTruncatedTrianglesAreRefused();
	TestTriangleIndexBeyondVerticesIsRefused();
	TestClsfBoxes();
	TestClsfTriangles();
	TestEmptyClsfIsRefused();
	TestContainerWalk();
	TestNameWithCed2Marker();
	TestClstWithAabbBlock();

	if(failures){
		printf("%d br collision layout check(s) failed\n", failures);
		return 1;
	}
	printf("br collision layout ok\n");
	return 0;
}
