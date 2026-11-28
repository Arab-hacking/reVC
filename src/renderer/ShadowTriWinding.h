#ifndef __GTA_SHADOWTRIWINDING_H__
#define __GTA_SHADOWTRIWINDING_H__

namespace ShadowTriWinding
{

// In GTA VC (CColTrianglePlane::Set), normal = CrossProduct(vc - va, vb - va),
// so upward-facing collision triangles (normal.z > 0) wind A -> B -> C clockwise in XY,
// where interior points satisfy CrossProduct2D(P - start, end - start) > 0.
// Custom collision triangles with normal.z < 0 wind counter-clockwise in XY and reject
// 100% of shadow vertices unless B and C are swapped first.
template <typename Vec3T>
inline void OrientShadowTriangleCCW(float normalZ, Vec3T &pointB, Vec3T &pointC)
{
	if(normalZ < 0.0f){
		Vec3T tmp = pointB;
		pointB = pointC;
		pointC = tmp;
	}
}

} // namespace ShadowTriWinding

#endif // __GTA_SHADOWTRIWINDING_H__
