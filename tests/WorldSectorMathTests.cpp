#include <cassert>
#include "../src/core/WorldSectorMath.h"

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
}
