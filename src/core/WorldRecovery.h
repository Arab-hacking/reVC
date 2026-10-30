#ifndef REVC_WORLD_RECOVERY_H
#define REVC_WORLD_RECOVERY_H

#include <cmath>

// Only rescue abandoned mission entities to a road that is actually in their
// part of the world. The native path graph does not cover distant custom maps;
// searching it without an XY radius teleports a fallen vehicle across the map
// (and the stock path finder can return node zero when the graph is empty).
// PathNode::GetX/GetY are deliberately used instead of Z: a falling entity
// may be far below the road even when it is directly above it horizontally.
template<typename PathNode>
inline int WorldFindNearbyRecoveryNode(float x, float y, PathNode *nodes,
                                       int first, int end, int capacity,
                                       float radius)
{
    if(!nodes || first < 0 || first >= end || end > capacity ||
       !std::isfinite(x) || !std::isfinite(y) ||
       !std::isfinite(radius) || radius <= 0.0f)
        return -1;

    int closest = -1;
    double closestDistSq = double(radius) * radius;
    for(int i = first; i < end; ++i){
        double dx = double(nodes[i].GetX()) - x;
        double dy = double(nodes[i].GetY()) - y;
        double distSq = dx*dx + dy*dy;
        if(distSq < closestDistSq){
            closest = i;
            closestDistSq = distSq;
        }
    }
    return closest;
}

#endif
