#include <cassert>
#include <climits>
#include <cmath>
#include <limits>
// Mirror windows.h's function-like macros to catch PC-only preprocessing
// failures in this standalone test on Linux as well.
#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif
#include "../src/core/WorldSectorMath.h"
#undef min
#undef max

int main()
{
    const float origin = -4000.0f;
    const float size = 50.0f;
    assert(WorldSectorIndex(-4000.0f, origin, size) == 0);
    assert(WorldSectorIndex(-4000.1f, origin, size) == -1);
    assert(WorldSectorIndex(-4050.0f, origin, size) == -1);
    assert(WorldSectorIndex(-4050.1f, origin, size) == -2);
    assert(WorldSectorIndex(3999.9f, origin, size) == 159);
    assert(WorldSectorIndex(4000.0f, origin, size) == 160);
    assert(WorldSectorIndex(8450.0f, origin, size) == 249);
    assert(WorldSectorIndex(1.0e30f, origin, size) == INT_MAX);
    assert(WorldSectorIndex(-1.0e30f, origin, size) == INT_MIN);
}
