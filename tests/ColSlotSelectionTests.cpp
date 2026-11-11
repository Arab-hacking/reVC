// Collision store slot policy.
//
// CColStore::LoadCollision used to run the whole MAX_CLEANUP scan once per
// collision slot - 31 x 50 iterations with a pool lookup each, every frame,
// twice per frame - and then threw the answer away. The scan now happens once,
// and the decision itself lives here so it can be tested without a world.
//
// The margins are negative on purpose. CRect::IsPointInside takes an extra
// radius and SHRINKS the box by it, so a point has to be that far inside to
// count. Turning these into positive numbers would look like a fix and would
// destroy the anti-thrash hysteresis between load and unload.

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

// The real target pulls these in through common.h; keep this file standalone.
// Only the parts of CVector/CVector2D the policy actually touches are declared
// here: the engine versions sit on RwV3d and drag the whole renderer in.
#include <cmath>

struct CVector { float x, y, z; };

class CVector2D
{
public:
	float x, y;
	CVector2D(void) {}
	CVector2D(float x, float y) : x(x), y(y) {}
};

// Rect.cpp includes common.h, which drags the engine in. The policy never uses
// the constructors' empty-box sentinel, so a plain zero box does here.
#include "../src/math/Rect.h"
CRect::CRect(void) : left(0), bottom(0), right(0), top(0) {}
CRect::CRect(float l, float t, float r, float b) : left(l), bottom(b), right(r), top(t) {}
#define COLSLOTSELECTION_IMPLEMENTATION
#include "../src/collision/ColSlotSelection.h"

static int failures;

static void
Check(bool cond, const char *what)
{
	if(!cond){
		printf("FAIL: %s\n", what);
		failures++;
	}
}

static CRect
MakeRect(float left, float top, float right, float bottom)
{
	CRect r;
	r.left = left;
	r.right = right;
	r.top = top;
	r.bottom = bottom;
	return r;
}

int
main(void)
{
	// A 1000 x 1000 box from (0,0) to (1000,1000).
	CRect big = MakeRect(0.0f, 0.0f, 1000.0f, 1000.0f);
	CVector2D centre(500.0f, 500.0f);
	CVector2D noSecond(0.0f, 0.0f);

	// 1. Inside the box: wanted, no name or entity needed.
	Check(ColSlotWantedFor(big, "beach", centre, false, noSecond, 0, 0),
	      "point inside the box wants the slot");

	// 2. Outside the box and nothing else in range: not wanted.
	CVector2D outside(5000.0f, 5000.0f);
	Check(!ColSlotWantedFor(big, "beach", outside, false, noSecond, 0, 0),
	      "point outside the box does not want the slot");

	// 3. The yacht slot is always kept, wherever the player is.
	Check(ColSlotWantedFor(big, "yacht", outside, false, noSecond, 0, 0),
	      "the yacht slot is always wanted");

	// 4. Second position (a remote player) uses the -119 margin, so a point in
	//    the outer 119 units of the box does NOT claim it.
	CRect marginBox = MakeRect(0.0f, 0.0f, 1000.0f, 1000.0f);
	CVector2D justInside(900.0f, 500.0f);          // 100 units from the right edge
	Check(!ColSlotWantedFor(marginBox, "beach",
	                        outside, true, justInside, 0, 0),
	      "second position inside the margin does not want the slot");
	CVector2D wellInside(700.0f, 500.0f);          // 300 units from the edge
	Check(ColSlotWantedFor(marginBox, "beach",
	                       outside, true, wellInside, 0, 0),
	      "second position past the margin wants the slot");

	// 5. A mission entity within its own -80 margin claims the slot.
	CVector2D entityPos(300.0f, 300.0f);
	Check(ColSlotWantedFor(big, "beach", outside, false, noSecond, &entityPos, 1),
	      "mission entity inside the margin claims the slot");
	CVector2D entityFar(5000.0f, 5000.0f);
	Check(!ColSlotWantedFor(big, "beach", outside, false, noSecond, &entityFar, 1),
	      "mission entity outside the box does not claim it");

	// 6. Several entities, the last one matching: the loop must not stop early
	//    on a miss.
	CVector2D entities[3] = { CVector2D(5000.0f, 5000.0f),
	                          CVector2D(-5000.0f, -5000.0f),
	                          CVector2D(300.0f, 300.0f) };
	Check(ColSlotWantedFor(big, "beach", outside, false, noSecond, entities, 3),
	      "a later mission entity still claims the slot");

	// 7. The margins really do shrink: the same point is inside the plain box
	//    and outside the inset one. This is the hysteresis, stated explicitly.
	CRect box = MakeRect(0.0f, 0.0f, 1000.0f, 1000.0f);
	CVector2D nearEdge(950.0f, 500.0f);
	Check(box.IsPointInside(nearEdge), "point is inside the raw box");
	Check(!ColSlotNearby(box, nearEdge), "point is outside the -110 inset");
	Check(!ColSlotWithinRequestRadius(box, nearEdge), "point is outside the -115 inset");
	Check(!ColSlotWithinLoadedRadius(box, nearEdge), "point is outside the -115 loaded inset");

	// 8. The radii keep their documented order: requested (115) is wider than
	//    nearby (110), so a slot can be requested before it counts as needed.
	Check(COL_REQUEST_MARGIN <= COL_NEARBY_MARGIN,
	      "request radius is at least as wide as the nearby radius");
	Check(COL_HAS_LOADED_MARGIN == COL_REQUEST_MARGIN,
	      "loaded check uses the same radius as the request");

	// 9. Deep inside, every radius reports a hit.
	CVector2D deep(500.0f, 500.0f);
	Check(ColSlotNearby(box, deep), "deep inside is nearby");
	Check(ColSlotWithinRequestRadius(box, deep), "deep inside is within the request radius");
	Check(ColSlotWithinLoadedRadius(box, deep), "deep inside is within the loaded radius");

	if(failures){
		printf("%d col slot selection check(s) failed\n", failures);
		return 1;
	}
	printf("col slot selection ok\n");
	return 0;
}
