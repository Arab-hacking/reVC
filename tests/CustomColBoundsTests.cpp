// The TBounds block of a COL2/COL3 record stores the box (min, max) followed by
// the bounding sphere in one of two orders:
//
//   San Andreas / the mobile engine : center (24..35), radius (36..39)
//   brformats and most converters    : radius (24..27), center (28..39)
//
// The converter used to pick the order with #ifdef ANDROID, so the exact same
// .col/.cls was converted correctly on one platform and with a centre of
// (radius, center.x, center.y) and a radius taken from the low word of center.z
// on the other. It now recognises the layout from the data itself; this test
// pins both directions down.

// The real test target compiles with common.h (which defines uint8) through
// the game headers; keep this file self-contained for the standalone build.
typedef unsigned char uint8;

#include "CustomCol.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#define CHECK(cond) do { \
	if(!(cond)){ \
		std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
		std::abort(); \
	} \
} while(0)

static void W32(std::vector<uint8> &b, size_t pos, uint32_t n) {
	for(int k = 0; k < 4; k++) b[pos + k] = (uint8)(n >> (8 * k));
}
static void WF(std::vector<uint8> &b, size_t pos, float x) {
	uint32_t n; std::memcpy(&n, &x, 4); W32(b, pos, n);
}

// A COL3 record with one box. The bounds carry the same numbers in either
// order, so a converter that guesses has to fall back on which reading makes a
// sphere that sits at the centre of the box (radius 38.73 around (10,10,5)).
enum { BOUNDS_SA_ORDER, BOUNDS_RADIUS_FIRST };

static std::vector<uint8> MakeCol3(int order) {
	std::vector<uint8> b(120 + 28, 0);
	std::memcpy(&b[0], "COL3", 4);
	W32(b, 4, (uint32_t)(b.size() - 8));
	std::memcpy(&b[8], "col3_bridge", 11);

	const size_t tb = 32;
	// Zero the record first: the offsets are read from a fixed header, and
	// leaving the sphere/box counts at 0 keeps the geometry check out of the
	// way so only the TBounds parsing decides the result.
	WF(b, tb + 0, -10.0f); WF(b, tb + 4, -20.0f); WF(b, tb + 8, -5.0f);
	WF(b, tb + 12, 30.0f); WF(b, tb + 16, 40.0f); WF(b, tb + 20, 15.0f);
	if(order == BOUNDS_SA_ORDER) {
		WF(b, tb + 24, 10.0f); WF(b, tb + 28, 10.0f); WF(b, tb + 32, 5.0f);
		WF(b, tb + 36, 38.73f);
	} else {
		WF(b, tb + 24, 38.73f);
		WF(b, tb + 28, 10.0f); WF(b, tb + 32, 10.0f); WF(b, tb + 36, 5.0f);
	}

	b[74] = 1;                    // numBoxes = 1
	W32(b, 88, 120u - 4u);        // box array offset, counted from fourcc + 4
	WF(b, 120 + 0, -10.0f); WF(b, 120 + 4, -20.0f); WF(b, 120 + 8, -5.0f);
	WF(b, 120 + 12, 30.0f); WF(b, 120 + 16, 40.0f); WF(b, 120 + 20, 15.0f);
	return b;
}

// Vice City COLL body: radius @ +8 (block offset 32 = fourcc 4 + size 4),
// center @ +12, min @ +24, max @ +36.
static void CheckConverted(const char *label, const std::vector<uint8> &body) {
	float rad = 0.0f, cx = 0.0f, cy = 0.0f, cz = 0.0f;
	std::memcpy(&rad, &body[32], 4);
	std::memcpy(&cx,  &body[36], 4);
	std::memcpy(&cy,  &body[40], 4);
	std::memcpy(&cz,  &body[44], 4);
	if(std::fabs(rad - 38.73f) > 0.05f ||
	   std::fabs(cx - 10.0f) > 0.05f ||
	   std::fabs(cy - 10.0f) > 0.05f ||
	   std::fabs(cz -  5.0f) > 0.05f){
		std::fprintf(stderr, "%s: rad=%f center=(%f,%f,%f)\n", label, rad, cx, cy, cz);
		std::abort();
	}
}

int main() {
	{
		customcol::Stats st;
		std::vector<uint8> out = customcol::ToGameFormat(MakeCol3(BOUNDS_SA_ORDER), st);
		CHECK(st.blocks == 1);
		CHECK(out.size() >= 48);
		CheckConverted("SA-order TBounds (center @24, radius @36)", out);
	}
	{
		customcol::Stats st;
		std::vector<uint8> out = customcol::ToGameFormat(MakeCol3(BOUNDS_RADIUS_FIRST), st);
		CHECK(st.blocks == 1);
		CHECK(out.size() >= 48);
		CheckConverted("radius-first TBounds (radius @24, center @28)", out);
	}
	// Bounds that cannot be read at all (NaN box and sphere) must not survive
	// into the converted record: the game trusts the radius and the box, and a
	// NaN there makes the model invisible or the collision query unstable.
	{
		std::vector<uint8> bad = MakeCol3(BOUNDS_SA_ORDER);
		for(size_t o = 32; o < 72; o += 4) {
			const float nan = std::nanf("");
			std::memcpy(&bad[o], &nan, 4);
		}
		customcol::Stats st;
		std::vector<uint8> out = customcol::ToGameFormat(bad, st);
		CHECK(st.blocks == 1);
		float rad = 0.0f, minx = 0.0f, maxx = 0.0f;
		std::memcpy(&rad,  &out[32], 4);
		std::memcpy(&minx, &out[48], 4);
		std::memcpy(&maxx, &out[60], 4);
		CHECK(rad == rad && rad > 0.0f && rad < 100000.0f);
		CHECK(minx == minx && maxx == maxx && maxx >= minx);
	}

	std::puts("custom COL TBounds layout autodetect tests passed");
	return 0;
}
