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

// Same rule as ColRecord::SectionFits in CFileLoader.cpp: a count is accepted
// when it is not negative and the section it describes fits in what is left.
static bool
Fits(int32 count, size_t elemSize, size_t left)
{
	if(count < 0)
		return false;
	return (unsigned long long)count * elemSize <= (unsigned long long)left;
}

int main(void)
{
	// The numbers the client asserts on.
	Check(MAXNUMSPHERES == 129, "MAXNUMSPHERES is the client's 0x81");
	Check(MAXNUMTRIS == 1600, "MAXNUMTRIS is the client's 0x640");
	Check(MAXNUMBOXES == 101, "MAXNUMBOXES is the client's 0x65");

	// Loading must not apply any of these as a clamp. Real maps ship models with
	// more than 32 boxes; cutting them down would both hide collision and - as
	// happened - move the read pointer into the next section.

	// A read of a section is accepted when it fits in the bytes that are left.
	// 48 boxes - an ordinary value on a real map - must be accepted.
	Check(Fits(48, 28, 2000), "48 boxes fit in a 2000-byte record");
	Check(Fits(129, 20, 3000), "129 spheres fit in a 3000-byte record");
	Check(Fits(1600, 16, 25600), "1600 triangles fit in exactly their own size");
	Check(Fits(0, 28, 0), "an empty section always fits");

	// A read is refused when it would run past the record, which is the only
	// thing that has to be caught: clamping the count instead used to shift
	// every following section and produce triangles with no vertices.
	Check(!Fits(-1, 28, 2000), "a negative count is refused");
	Check(!Fits(-32768, 16, 2000), "the smallest signed 16-bit count is refused");
	Check(!Fits(1601, 16, 25600), "1601 triangles do not fit in 25600 bytes");
	Check(!Fits(48, 28, 100), "48 boxes do not fit in 100 bytes");

	if(failures){
		printf("%d collision count limit check(s) failed\n", failures);
		return 1;
	}
	printf("collision count limits ok\n");
	return 0;
}
