#ifndef __GTA_PEDGEOMETRYANALYSER_H__
#define __GTA_PEDGEOMETRYANALYSER_H__

#include "Vector.h"

class CPointRoute;

// Which side of an entity's bounding box a point lies on, in the entity's
// own frame: forward, left, backward, right.
enum eDirection
{
	FORWARD = 0,
	LEFT,
	BACKWARD,
	RIGHT
};

class CEntity;
class CPed;
class CColSphere;

// Bounding-box geometry queries peds use to walk around vehicles and
// other obstacles: which side of the box they are on, and a point route
// around it when the straight way is blocked.
class CPedGeometryAnalyser
{
public:
	static float ms_fPedNominalRadius;

	static void ComputeEntityBoundingBoxCorners(float zPos, CEntity &entity, CVector corners[4]);
	static void ComputeEntityBoundingBoxPlanes(float zPos, CEntity &entity, CVector outPlaneNormals[4], float outPlaneDs[4]);
	static void ComputeEntityBoundingBoxSegmentPlanes(float zPos, CEntity &entity, CVector outSegPlaneNormals[4], float outPlaneDs[4]);
	static void ComputeEntityBoundingSphere(const CPed &ped, CEntity &entity, CColSphere &out);

	static eDirection ComputeEntityHitSide(const CVector &point, CEntity &entity);
	static eDirection ComputeEntityHitSide(const CVector &point, const CVector segmentPlaneNormals[4], const float segmentPlaneDots[4]);

	static int32 ComputeRouteRoundEntityBoundingBox(const CPed &ped, CEntity &entity, const CVector &target, CPointRoute &outRoute, int32 forceDirection);
	static int32 ComputeRouteRoundEntityBoundingBox(const CPed &ped, const CVector &start, CEntity &entity, const CVector &target, CPointRoute &outRoute, int32 forceDirection);

	static bool IsEntityBlockingTarget(CEntity &entity, const CVector &point, float radius);

private:
	static bool ComputeEntityBoundingBoxCornersUncached(float zPos, CEntity &entity, CVector corners[4]);
	static void ComputeEntityBoundingBoxCentreUncached(float zPos, const CVector corners[4], CVector &center);
	static void ComputeEntityBoundingBoxPlanesUncached(float zPos, const CVector corners[4], CVector outPlaneNormals[4], float outPlaneDs[4]);
	static void ComputeEntityBoundingBoxSegmentPlanesUncached(const CVector corners[4], CVector &center, CVector outSegPlaneNormals[4], float outPlaneDs[4]);
};

#endif // __GTA_PEDGEOMETRYANALYSER_H__
