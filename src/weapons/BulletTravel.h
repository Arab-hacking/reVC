#ifndef REVC_BULLET_TRAVEL_H
#define REVC_BULLET_TRAVEL_H

#include <cmath>

// The world has no gameplay XY boundary for bullets. Only reject invalid
// floating-point segments; a NaN/Inf fed to sector traversal is unsafe.
template<typename Vector>
inline bool BulletSegmentIsFinite(const Vector &start, const Vector &end)
{
    return std::isfinite(start.x) && std::isfinite(start.y) && std::isfinite(start.z) &&
           std::isfinite(end.x) && std::isfinite(end.y) && std::isfinite(end.z);
}

#endif
