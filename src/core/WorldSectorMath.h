#ifndef REVC_WORLD_SECTOR_MATH_H
#define REVC_WORLD_SECTOR_MATH_H

#include <cmath>
#include <limits>

// Floor, rather than truncate, so coordinates just west/south of the native
// grid are assigned to their own sector instead of aliasing sector zero.
inline int
WorldSectorIndex(float coord, float origin, float sectorSize)
{
    double index = std::floor((double(coord) - origin) / sectorSize);
    if(!std::isfinite(index)) return 0;
    // windows.h defines min/max as macros in the PC build. Parenthesizing
    // the member names prevents macro expansion without changing the limits.
    if(index <= (std::numeric_limits<int>::min)()) return (std::numeric_limits<int>::min)();
    if(index >= (std::numeric_limits<int>::max)()) return (std::numeric_limits<int>::max)();
    return (int)index;
}

#endif
