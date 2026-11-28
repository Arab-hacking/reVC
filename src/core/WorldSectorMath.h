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
    // Leave room for the neighbouring-sector loops (streaming, renderer,
    // collision) to add/subtract offsets without overflowing signed int.
    // This is a representation limit, not a native-map boundary.
    // windows.h defines min/max as macros in the PC build.
    const int margin = 1024;
    const int low = (std::numeric_limits<int>::min)() + margin;
    const int high = (std::numeric_limits<int>::max)() - margin;
    if(index <= low) return low;
    if(index >= high) return high;
    return (int)index;
}

#endif
