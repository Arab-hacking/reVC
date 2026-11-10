#include <cassert>
#include <cstdint>
#include "../src/save/SaveBlockBounds.h"

int main()
{
    const uint32_t legacy = 201729;
    assert(FitsLegacySave(0, 0, legacy));
    assert(FitsLegacySave(150000, 51720, legacy)); // body fits exactly with padding
    assert(!FitsLegacySave(150000, 51724, legacy)); // nested size word pushes over
    assert(!FitsLegacySave(150000, 60000, legacy));
    assert(!FitsLegacySave(201722, 0, legacy));
    assert(!FitsLegacySave(1, UINT32_MAX, legacy));
}
