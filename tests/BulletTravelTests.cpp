// These assertions must run in Release CI builds too.
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <limits>
#include "../src/weapons/BulletTravel.h"

struct Position { float x, y, z; };

int main()
{
    const Position vcCentre = {0.0f, 0.0f, 10.0f};
    const Position westOfVc = {-2401.0f, -2001.0f, 0.0f};
    const Position eastOfVc = {1601.0f, 2001.0f, 0.0f};
    const Position remoteMap = {100000.0f, -100000.0f, -500.0f};
    assert(BulletSegmentIsFinite(vcCentre, westOfVc));
    assert(BulletSegmentIsFinite(vcCentre, eastOfVc));
    assert(BulletSegmentIsFinite(westOfVc, remoteMap));
    const Position invalidX = {std::numeric_limits<float>::infinity(), 0.0f, 0.0f};
    const Position invalidY = {0.0f, std::numeric_limits<float>::quiet_NaN(), 0.0f};
    const Position invalidZ = {0.0f, 0.0f, -std::numeric_limits<float>::infinity()};
    assert(!BulletSegmentIsFinite(vcCentre, invalidX));
    assert(!BulletSegmentIsFinite(invalidY, vcCentre));
    assert(!BulletSegmentIsFinite(vcCentre, invalidZ));
}
