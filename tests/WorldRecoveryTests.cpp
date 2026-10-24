#include <cassert>
#include <cmath>
#include <limits>
#include "../src/core/WorldRecovery.h"

struct RoadNode {
    float x, y, z;
    float GetX() { return x; }
    float GetY() { return y; }
};

int main()
{
    RoadNode nodes[] = {
        {0.0f, 0.0f, 0.0f},
        {100.0f, 50.0f, 0.0f},
        {-4100.0f, 120.0f, -500.0f},
        {9000.0f, -12000.0f, 10000.0f}
    };
    // A local path can rescue an abandoned mission vehicle even after a
    // large Z fall, including on either side of VC's old sector grid.
    assert(WorldFindNearbyRecoveryNode(105.0f, 60.0f, nodes, 0, 2, 4, 200.0f) == 1);
    assert(WorldFindNearbyRecoveryNode(-4090.0f, 130.0f, nodes, 2, 4, 4, 200.0f) == 2);
    assert(WorldFindNearbyRecoveryNode(9002.0f, -12003.0f, nodes, 2, 4, 4, 200.0f) == 3);
    // Never resurrect a mission car/ped in Vice City when it fell on a
    // distant custom map or when the matching native path graph is empty.
    assert(WorldFindNearbyRecoveryNode(100000.0f, 0.0f, nodes, 0, 4, 4, 200.0f) == -1);
    assert(WorldFindNearbyRecoveryNode(0.0f, 0.0f, nodes, 2, 2, 4, 200.0f) == -1);
    assert(WorldFindNearbyRecoveryNode(0.0f, 0.0f, nodes, 0, 4, 3, 200.0f) == -1);
    assert(WorldFindNearbyRecoveryNode<RoadNode>(0.0f, 0.0f, nullptr, 0, 4, 4, 200.0f) == -1);
    assert(WorldFindNearbyRecoveryNode(std::numeric_limits<float>::infinity(), 0.0f,
                                       nodes, 0, 4, 4, 200.0f) == -1);
    assert(WorldFindNearbyRecoveryNode(0.0f, std::numeric_limits<float>::quiet_NaN(),
                                       nodes, 0, 4, 4, 200.0f) == -1);
}
