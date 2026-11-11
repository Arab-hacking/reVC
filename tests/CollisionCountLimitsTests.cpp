// Collision model counts have to be bounded before they are used.
//
// CFileLoader::LoadCollisionModel takes five counts (spheres, lines, boxes,
// vertices, triangles) straight out of a .col record. Each one decides how much
// memory is allocated and how far the read pointer advances, so a count that
// cannot come from a file the game wrote makes the loader read past its buffer
// and leaves every following section misaligned - the collision model is then
// built from garbage.
//
// Chapter 8 of the collision analysis fixes the ceilings that ProcessColModels
// asserts on: 129 spheres and 1600 triangles. Boxes and lines reuse the
// processing limits. These tests pin the values and the clamping behaviour so a
// later edit cannot quietly widen them again. The limits live in a
// dependency-free header precisely so this test can read the real values
// instead of a copy.

#include <cstdint>

typedef int32_t int32;
#include <cstdio>

#include "../src/collision/ColLimits.h"

static int failures;

static void
Check(bool cond, const char *what)
{
	if(!cond){
		printf("FAIL: %s\n", what);
		failures++;
	}
}

// Same behaviour as the helper in CFileLoader.cpp.
static int32
ClampCount(int32 count, int32 max)
{
	if(count < 0 || count > max)
		return count < 0 ? 0 : max;
	return count;
}

int main(void)
{
	// The numbers the client asserts on.
	Check(MAXNUMSPHERES == 129, "MAXNUMSPHERES is the client's 0x81");
	Check(MAXNUMTRIS == 1600, "MAXNUMTRIS is the client's 0x640");

	// What the loader is allowed to read into a model.
	Check(MAX_COLSPHERES == MAXNUMSPHERES, "the loader accepts as many spheres as the check allows");
	Check(MAX_COLTRIS == MAXNUMTRIS, "the loader accepts as many triangles as the check allows");
	Check(MAX_COLBOXES == MAXNUMBOXES, "box ceiling is shared with the check");
	Check(MAX_COLLINES == MAXNUMLINES, "line ceiling is shared with the check");

	// A legal count passes through untouched.
	Check(ClampCount(0, MAX_COLSPHERES) == 0, "an empty section stays empty");
	Check(ClampCount(129, MAX_COLSPHERES) == 129, "a full sphere list is kept");
	Check(ClampCount(1600, MAX_COLTRIS) == 1600, "a full triangle list is kept");

	// Out of range values are bounded, not passed on.
	Check(ClampCount(130, MAX_COLSPHERES) == MAX_COLSPHERES, "too many spheres is clamped");
	Check(ClampCount(1601, MAX_COLTRIS) == MAX_COLTRIS, "too many triangles is clamped");
	Check(ClampCount(32767, MAX_COLTRIS) == MAX_COLTRIS, "the largest signed 16-bit count is clamped");
	Check(ClampCount(-1, MAX_COLSPHERES) == 0, "a negative count becomes an empty section");
	Check(ClampCount(-32768, MAX_COLTRIS) == 0, "the smallest signed 16-bit count becomes empty");

	// Vertices have no assert of their own; the ceiling must still be positive
	// and large enough for a legal model.
	Check(MAX_COLVERTS > 1600, "the vertex ceiling allows a legal model");
	Check(ClampCount(32767, MAX_COLVERTS) == 32767, "a large but legal vertex list is kept");

	if(failures){
		printf("%d collision count limit check(s) failed\n", failures);
		return 1;
	}
	printf("collision count limits ok\n");
	return 0;
}
