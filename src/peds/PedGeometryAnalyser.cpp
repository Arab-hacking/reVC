#include "common.h"

#include "PedGeometryAnalyser.h"

#include "ColModel.h"
#include "ColPoint.h"
#include "ColSphere.h"
#include "Entity.h"
#include "maths.h"
#include "Ped.h"
#include "PointRoute.h"
#include "World.h"

#include <cfloat>

// How far a ped's body reaches past an obstacle's bounding box when
// routes around it are computed.
float CPedGeometryAnalyser::ms_fPedNominalRadius = 0.25f;

static CVector2D PerpRight(const CVector2D &v)
{
	return CVector2D(v.y, -v.x);
}

static CVector2D PerpLeft(const CVector2D &v)
{
	return CVector2D(-v.y, v.x);
}

static CVector2D Normalized2D(const CVector2D &v)
{
	CVector2D result(v);
	result.Normalise();
	return result;
}

static float Squared(float v)
{
	return v * v;
}

static CVector NormalizedVec(const CVector &v)
{
	CVector result(v);
	result.Normalise();
	return result;
}

void
CPedGeometryAnalyser::ComputeEntityBoundingBoxCorners(float zPos, CEntity &entity, CVector corners[4])
{
	ComputeEntityBoundingBoxCornersUncached(zPos, entity, corners);
}

// The four corners of the entity's collision bounding box, projected onto
// the horizontal plane at zPos and inflated by the ped's radius. Corner
// order follows the entity's dominant axis, so consecutive corners form
// the box's edges.
bool
CPedGeometryAnalyser::ComputeEntityBoundingBoxCornersUncached(float zPos, CEntity &entity, CVector corners[4])
{
	CVector min(FLT_MAX, FLT_MAX, FLT_MAX);
	CVector max(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	const CMatrix &matrix = entity.GetMatrix();
	const CColModel *entityCM = entity.GetColModel();
	if (!entityCM)
		return false;

	if (entity.IsObject() && (entityCM->boundingBox.max.z - entityCM->boundingBox.min.z) > 6.0f
		&& (entityCM->numBoxes != 0 || entityCM->numSpheres != 0)) {
		// Tall objects are walked around using only the collision volumes
		// that intersect the ped's height band.
		const float minAllowedZ = zPos - 1.0f;
		const float maxAllowedZ = zPos + 1.0f;
		for (int32 i = 0; i < entityCM->numBoxes; i++) {
			const CVector &bmin = entityCM->boxes[i].min;
			const CVector &bmax = entityCM->boxes[i].max;
			const float bminZ = (matrix * CVector(0.0f, 0.0f, bmin.z)).z;
			const float bmaxZ = (matrix * CVector(0.0f, 0.0f, bmax.z)).z;
			if (bmaxZ < minAllowedZ || bminZ > maxAllowedZ)
				continue;
			min.x = Min(min.x, bmin.x);
			min.y = Min(min.y, bmin.y);
			min.z = Min(min.z, bmin.z);
			max.x = Max(max.x, bmax.x);
			max.y = Max(max.y, bmax.y);
			max.z = Max(max.z, bmax.z);
		}
		for (int32 i = 0; i < entityCM->numSpheres; i++) {
			const CColSphere &sphere = entityCM->spheres[i];
			const CVector bmin = sphere.center - CVector(sphere.radius, sphere.radius, sphere.radius);
			const CVector bmax = sphere.center + CVector(sphere.radius, sphere.radius, sphere.radius);
			const float bminZ = (matrix * CVector(0.0f, 0.0f, bmin.z)).z;
			const float bmaxZ = (matrix * CVector(0.0f, 0.0f, bmax.z)).z;
			if (bmaxZ < minAllowedZ || bminZ > maxAllowedZ)
				continue;
			min.x = Min(min.x, bmin.x);
			min.y = Min(min.y, bmin.y);
			min.z = Min(min.z, bmin.z);
			max.x = Max(max.x, bmax.x);
			max.y = Max(max.y, bmax.y);
			max.z = Max(max.z, bmax.z);
		}
	} else {
		min = entityCM->boundingBox.min;
		max = entityCM->boundingBox.max;
	}

	min -= CVector(ms_fPedNominalRadius, ms_fPedNominalRadius, ms_fPedNominalRadius);
	max += CVector(ms_fPedNominalRadius, ms_fPedNominalRadius, ms_fPedNominalRadius);
	const CVector halfExtent = (max - min) * 0.5f;

	// Project the box onto the plane along its dominant axis.
	const CVector2D center = CVector2D(matrix * ((min + max) * 0.5f));
	const struct {
		CVector principal;
		float extP;
		CVector2D axisA;
		float extA;
		CVector2D axisB;
		float extB;
	} dominant[3] = {
		{ matrix.GetForward(), halfExtent.y, CVector2D(matrix.GetRight()), halfExtent.x, CVector2D(matrix.GetUp()), halfExtent.z },
		{ matrix.GetUp(), halfExtent.z, CVector2D(matrix.GetRight()), halfExtent.x, CVector2D(matrix.GetForward()), halfExtent.y },
		{ matrix.GetRight(), halfExtent.x, CVector2D(matrix.GetForward()), halfExtent.y, CVector2D(matrix.GetUp()), halfExtent.z },
	};
	const float weightX = 2.0f * halfExtent.x * CVector2D(matrix.GetRight()).MagnitudeSqr();
	const float weightY = 2.0f * halfExtent.y * CVector2D(matrix.GetForward()).MagnitudeSqr();
	const float weightZ = 2.0f * halfExtent.z * CVector2D(matrix.GetUp()).MagnitudeSqr();

	const int32 which = (weightY > weightX && weightY > weightZ) ? 0 : (weightX <= weightZ ? 1 : 2);
	const CVector2D u = Normalized2D(CVector2D(dominant[which].principal));
	const CVector2D v = PerpRight(u);
	const float extentU = Abs(DotProduct2D(u, dominant[which].axisA)) * dominant[which].extA
		+ Abs(DotProduct2D(u, dominant[which].axisB)) * dominant[which].extB;
	const float extentV = Abs(DotProduct2D(v, dominant[which].axisA)) * dominant[which].extA
		+ Abs(DotProduct2D(v, dominant[which].axisB)) * dominant[which].extB;
	const CVector2D offsetU = u * extentU;
	const CVector2D offsetV = v * extentV;
	const CVector2D ptA = center + CVector2D(dominant[which].principal) * dominant[which].extP;
	const CVector2D ptB = center - CVector2D(dominant[which].principal) * dominant[which].extP;
	corners[0] = CVector(ptA.x + offsetU.x - offsetV.x, ptA.y + offsetU.y - offsetV.y, zPos);
	corners[1] = CVector(ptB.x - offsetU.x - offsetV.x, ptB.y - offsetU.y - offsetV.y, zPos);
	corners[2] = CVector(ptB.x - offsetU.x + offsetV.x, ptB.y - offsetU.y + offsetV.y, zPos);
	corners[3] = CVector(ptA.x + offsetU.x + offsetV.x, ptA.y + offsetU.y + offsetV.y, zPos);
	return true;
}

void
CPedGeometryAnalyser::ComputeEntityBoundingBoxCentreUncached(float zPos, const CVector corners[4], CVector &center)
{
	center.x = 0.0f;
	center.y = 0.0f;
	center.z = zPos;
	center.x = (corners[0].x + corners[1].x + corners[2].x + corners[3].x) * 0.25f;
	center.y = (corners[0].y + corners[1].y + corners[2].y + corners[3].y) * 0.25f;
}

void
CPedGeometryAnalyser::ComputeEntityBoundingBoxPlanes(float zPos, CEntity &entity, CVector outPlaneNormals[4], float outPlaneDs[4])
{
	CVector corners[4];
	ComputeEntityBoundingBoxCornersUncached(zPos, entity, corners);
	ComputeEntityBoundingBoxPlanesUncached(zPos, corners, outPlaneNormals, outPlaneDs);
}

// One outward-facing plane per bounding-box edge: n . P + d = 0.
void
CPedGeometryAnalyser::ComputeEntityBoundingBoxPlanesUncached(float zPos, const CVector corners[4], CVector outPlaneNormals[4], float outPlaneDs[4])
{
	(void)zPos;
	for (int32 i = 0; i < 4; i++) {
		const CVector2D prev(corners[(i + 3) % 4]);
		const CVector2D curr(corners[i]);
		const CVector2D normal = Normalized2D(PerpRight(curr - prev));
		outPlaneNormals[i] = CVector(normal.x, normal.y, 0.0f);
		outPlaneDs[i] = -DotProduct2D(normal, prev);
	}
}

void
CPedGeometryAnalyser::ComputeEntityBoundingBoxSegmentPlanes(float zPos, CEntity &entity, CVector outSegPlaneNormals[4], float outPlaneDs[4])
{
	CVector corners[4];
	CVector center;
	ComputeEntityBoundingBoxCornersUncached(zPos, entity, corners);
	ComputeEntityBoundingBoxCentreUncached(zPos, corners, center);
	ComputeEntityBoundingBoxSegmentPlanesUncached(corners, center, outSegPlaneNormals, outPlaneDs);
}

// One plane per corner, through the corner and facing away from the
// centre; used to tell which side of the box a point approaches from.
void
CPedGeometryAnalyser::ComputeEntityBoundingBoxSegmentPlanesUncached(const CVector corners[4], CVector &center, CVector outSegPlaneNormals[4], float outPlaneDs[4])
{
	const CVector2D center2D(center);
	for (int32 i = 0; i < 4; i++) {
		const CVector2D corner2D(corners[i]);
		const CVector2D normal2D = PerpLeft(corner2D - center2D);
		outSegPlaneNormals[i] = CVector(normal2D.x, normal2D.y, 0.0f);
		outPlaneDs[i] = -DotProduct2D(normal2D, corner2D);
	}
}

void
CPedGeometryAnalyser::ComputeEntityBoundingSphere(const CPed &ped, CEntity &entity, CColSphere &out)
{
	CVector corners[4];
	CVector center;
	const float zPos = ped.GetPosition().z;
	ComputeEntityBoundingBoxCornersUncached(zPos, entity, corners);
	ComputeEntityBoundingBoxCentreUncached(zPos, corners, center);

	float radiusSq = 0.0f;
	for (int32 i = 0; i < 4; i++)
		radiusSq = Max(radiusSq, (corners[i] - center).MagnitudeSqr2D());

	out.Set(Sqrt(radiusSq), center);
}

eDirection
CPedGeometryAnalyser::ComputeEntityHitSide(const CVector &point, CEntity &entity)
{
	CVector planeNormals[4];
	float planeDs[4];
	ComputeEntityBoundingBoxSegmentPlanes(point.z, entity, planeNormals, planeDs);
	return ComputeEntityHitSide(point, planeNormals, planeDs);
}

eDirection
CPedGeometryAnalyser::ComputeEntityHitSide(const CVector &point, const CVector segmentPlaneNormals[4], const float segmentPlaneDots[4])
{
	for (int32 curr = 0; curr < 4; curr++) {
		const int32 prev = (curr + 3) % 4;
		if (DotProduct(segmentPlaneNormals[prev], point) + segmentPlaneDots[prev] >= 0.0f
			&& DotProduct(segmentPlaneNormals[curr], point) + segmentPlaneDots[curr] < 0.0f)
			return (eDirection)curr;
	}
	return FORWARD;
}

// Is the entity's bounding box covering the point (within radius)?
bool
CPedGeometryAnalyser::IsEntityBlockingTarget(CEntity &entity, const CVector &point, float radius)
{
	CVector bbPlaneNormals[4];
	float bbPlaneDs[4];
	ComputeEntityBoundingBoxPlanes(entity.GetPosition().z, entity, bbPlaneNormals, bbPlaneDs);

	const CVector dir = point - entity.GetPosition();
	if (Abs(dir.z) > 3.0f)
		return false;
	if (Squared(entity.GetBoundRadius() + radius) < CVector2D(dir).MagnitudeSqr())
		return false;

	for (int32 i = 0; i < 4; i++) {
		if (DotProduct(bbPlaneNormals[i], point) + bbPlaneDs[i] > 0.0f)
			return false; // point lies in front of the plane, outside the box
	}
	return true;
}

int32
CPedGeometryAnalyser::ComputeRouteRoundEntityBoundingBox(const CPed &ped, CEntity &entity, const CVector &target, CPointRoute &outRoute, int32 forceDirection)
{
	return ComputeRouteRoundEntityBoundingBox(ped, ped.GetPosition(), entity, target, outRoute, forceDirection);
}

// Work out a route of points that takes the ped around the entity's
// bounding box from start to target. Returns 0 when no detour is needed,
// 1 for the left-hand route and 2 for the right-hand one.
int32
CPedGeometryAnalyser::ComputeRouteRoundEntityBoundingBox(const CPed &ped, const CVector &from0, CEntity &entity, const CVector &to0, CPointRoute &outRoute, int32 forceDirection)
{
	outRoute.Clear();

	const float prevPedNominalRadius = ms_fPedNominalRadius;
	ms_fPedNominalRadius = 0.175f;
	const float zPos = ped.GetPosition().z;

	CVector bbCorners[4];
	ComputeEntityBoundingBoxCorners(zPos, entity, bbCorners);

	CVector bbPlaneNormals[4];
	float bbPlaneDs[4];
	ComputeEntityBoundingBoxPlanes(zPos, entity, bbPlaneNormals, bbPlaneDs);

	ms_fPedNominalRadius = prevPedNominalRadius;

	CVector from = from0;
	CVector to = to0;

	// If both start and target are in front of the same plane, no detour.
	for (int32 pl = 0; pl < 4; pl++) {
		if (DotProduct(bbPlaneNormals[pl], from) + bbPlaneDs[pl] > 0.0f
			&& DotProduct(bbPlaneNormals[pl], to) + bbPlaneDs[pl] > 0.0f)
			return 0;
	}

	// Same approach side: no detour either.
	if (ComputeEntityHitSide(to, bbPlaneNormals, bbPlaneDs) == ComputeEntityHitSide(from, bbPlaneNormals, bbPlaneDs))
		return 0;

	const CVector moveDir = NormalizedVec(to - from);

	// Push start and target out onto the planes they cross.
	CVector fromOnPlane = from;
	CVector toOnPlane = to;
	{
		bool hasPlaneWithNoCrossings = false;
		for (int32 pl = 0; pl < 4; pl++) {
			const float EDGE_THRESHOLD = 0.2f;
			const float toDot = DotProduct(bbPlaneNormals[pl], toOnPlane) + bbPlaneDs[pl];
			const float fromDot = DotProduct(bbPlaneNormals[pl], fromOnPlane) + bbPlaneDs[pl];
			const bool toAbove = toDot > EDGE_THRESHOLD;
			const bool toBelow = toDot < -EDGE_THRESHOLD;
			const bool fromAbove = fromDot > EDGE_THRESHOLD;
			const bool fromBelow = fromDot < -EDGE_THRESHOLD;

			if (fromBelow && toAbove) {
				const float t = -(DotProduct(bbPlaneNormals[pl], from) + bbPlaneDs[pl]) / DotProduct(bbPlaneNormals[pl], moveDir);
				toOnPlane = from + moveDir * t;
			} else if (fromAbove && toBelow) {
				const float t = -(DotProduct(bbPlaneNormals[pl], from) + bbPlaneDs[pl]) / DotProduct(bbPlaneNormals[pl], moveDir);
				fromOnPlane = from + moveDir * t;
			} else if (!fromBelow && !toBelow) {
				hasPlaneWithNoCrossings = true;
				if (Abs(toDot) <= EDGE_THRESHOLD)
					toOnPlane += bbPlaneNormals[pl] * (EDGE_THRESHOLD - toDot);
				if (Abs(fromDot) <= EDGE_THRESHOLD)
					fromOnPlane += bbPlaneNormals[pl] * (EDGE_THRESHOLD - fromDot);
			}
		}

		// If some plane is never crossed, only the bounding sphere can
		// still be in the way.
		if (!hasPlaneWithNoCrossings) {
			const CVector2D dir2D = CVector2D(from) - CVector2D(to);
			const float distSq = dir2D.MagnitudeSqr();
			if (distSq == 0.0f)
				return 0; // start == target
			CColSphere sp;
			ComputeEntityBoundingSphere(ped, entity, sp);
			CVector dirVec(dir2D.x, dir2D.y, 0.0f);
			dirVec.Normalise();
			if (!sp.IntersectRay(from, dirVec, fromOnPlane, toOnPlane))
				return 0; // misses the sphere too: go straight
		}
	}

	// Second, stricter pass: stick the points exactly onto the box.
	int32 fromPlane = -1;
	int32 toPlane = -1;
	{
		const CVector dir = NormalizedVec(toOnPlane - fromOnPlane);
		for (int32 pl = 0; pl < 4; pl++) {
			const float fromDot = DotProduct(bbPlaneNormals[pl], fromOnPlane) + bbPlaneDs[pl];
			const float toDot = DotProduct(bbPlaneNormals[pl], toOnPlane) + bbPlaneDs[pl];
			if (toDot > 0.0f && fromDot < 0.0f) {
				toPlane = pl;
				const float t = -(DotProduct(bbPlaneNormals[pl], fromOnPlane) + bbPlaneDs[pl]) / DotProduct(bbPlaneNormals[pl], dir);
				toOnPlane = fromOnPlane + dir * t;
			}
			if (fromDot > 0.0f && toDot < 0.0f) {
				fromPlane = pl;
				const float t = -(DotProduct(bbPlaneNormals[pl], fromOnPlane) + bbPlaneDs[pl]) / DotProduct(bbPlaneNormals[pl], dir);
				fromOnPlane = fromOnPlane + dir * t;
			}
		}
	}
	if (fromPlane == -1 || toPlane == -1)
		return 0;

	// The two candidate routes run along the box corners between the
	// entry and exit planes, one each way round.
	CPointRoute routeL;
	CPointRoute routeR;
	{
		routeL.Add(from);
		for (int32 i = (fromPlane - 1 + 4) % 4; ; i = (i + 3) % 4) {
			if (i == (toPlane - 1 + 4) % 4)
				break;
			routeL.Add(bbCorners[i]);
		}
		routeL.Add(to);

		routeR.Add(from);
		for (int32 i = fromPlane; ; i = (i + 1) % 4) {
			if (i == toPlane)
				break;
			routeR.Add(bbCorners[i]);
		}
		routeR.Add(to);
	}

	// Trim the endpoints: the caller walks to `to` itself afterwards.
	struct TrimRoute {
		static int32 Apply(int32 ret, const CPointRoute &result, const CPed &ped, CEntity &entity, const CVector &from, CPointRoute &outRoute)
		{
			outRoute = result;
			if (CPedGeometryAnalyser::ComputeEntityHitSide(from, entity) == CPedGeometryAnalyser::ComputeEntityHitSide(outRoute[0], entity))
				outRoute.PopFront();
			outRoute.PopBack();
			(void)ped;
			return ret;
		}
	};

	switch (forceDirection) {
	case 0: {
		// No forced side: check both routes for blockers and take the
		// shortest clear one.
		CVector bbCenter;
		ComputeEntityBoundingBoxCentreUncached(zPos, bbCorners, bbCenter);
		const CVector2D bbCenter2D(bbCenter);

		struct ProcessedRoute {
			uint32 lastProcessedPoint;
			CEntity *blockedByEntity;
			ProcessedRoute() : lastProcessedPoint(1), blockedByEntity(nil) {}
		};
		struct RouteProcessor {
			static bool CheckLineBlocked(const CVector2D &origin, const CVector2D &target, float zPos, CEntity *&blockedBy, CEntity &entity, const CPed &ped)
			{
				CColPoint cp;
				if (!CWorld::ProcessLineOfSight(CVector(origin.x, origin.y, zPos), CVector(target.x, target.y, zPos), cp, blockedBy,
						true, true, true, true, false, false, false, false))
					return false;
				if (blockedBy == &entity || blockedBy == (CEntity *)&ped) {
					blockedBy = nil;
					return false;
				}
				return true;
			}

			static ProcessedRoute Process(const CPointRoute &route, float zPos, CEntity &entity, const CPed &ped, const CVector2D &bbCenter2D)
			{
				ProcessedRoute res;
				if (route.GetSize() < 2)
					return res;
				for (res.lastProcessedPoint = 1; (int32)res.lastProcessedPoint < route.GetSize(); res.lastProcessedPoint++) {
					const CVector &curr = route[(int32)res.lastProcessedPoint];
					const CVector &prev = route[(int32)res.lastProcessedPoint - 1];
					CVector dir = curr - prev;
					dir.Normalise();

					if (!CheckLineBlocked(CVector2D(curr), CVector2D(prev), zPos, res.blockedByEntity, entity, ped)) {
						const CVector2D curr2D(curr);
						CVector2D offset2D = CVector2D(dir.x, dir.y) * 0.5f;
						if (DotProduct2D(curr2D, curr2D - bbCenter2D) < 0.0f)
							offset2D = offset2D * -1.0f;
						if (!CheckLineBlocked(CVector2D(prev) + offset2D, curr2D + offset2D, zPos, res.blockedByEntity, entity, ped))
							continue; // not blocked: keep going
					}

					// Blocked by something. A ped walking our way is fine.
					if (res.blockedByEntity && res.blockedByEntity->IsPed()) {
						CPed *hitPed = (CPed *)res.blockedByEntity;
						if (hitPed->m_nMoveState != PEDMOVE_STILL) {
							if (DotProduct(dir, hitPed->GetMatrix().GetForward()) > 0.0f)
								continue;
						}
					}
					break;
				}
				return res;
			}
		};

		const ProcessedRoute processedL = RouteProcessor::Process(routeL, zPos, entity, ped, bbCenter2D);
		const ProcessedRoute processedR = RouteProcessor::Process(routeR, zPos, entity, ped, bbCenter2D);
		const bool clearL = processedL.blockedByEntity == nil;
		const bool clearR = processedR.blockedByEntity == nil;

		if (clearL) {
			if (clearR)
				return TrimRoute::Apply(routeL.GetLength() < routeR.GetLength() ? 1 : 2,
					routeL.GetLength() < routeR.GetLength() ? routeL : routeR, ped, entity, from, outRoute);
			return TrimRoute::Apply(1, routeL, ped, entity, from, outRoute);
		}
		if (clearR)
			return TrimRoute::Apply(2, routeR, ped, entity, from, outRoute);
		if (processedL.lastProcessedPoint == 1 && processedR.lastProcessedPoint > 1)
			return TrimRoute::Apply(2, routeR, ped, entity, from, outRoute);
		if (processedL.lastProcessedPoint > 1 && processedR.lastProcessedPoint == 1)
			return TrimRoute::Apply(1, routeL, ped, entity, from, outRoute);
		if (processedL.blockedByEntity == processedR.blockedByEntity)
			return TrimRoute::Apply(routeL.GetLength() < routeR.GetLength() ? 1 : 2,
				routeL.GetLength() < routeR.GetLength() ? routeL : routeR, ped, entity, from, outRoute);
		return TrimRoute::Apply(
			processedR.blockedByEntity->GetBoundRadius() > processedL.blockedByEntity->GetBoundRadius() ? 1 : 2,
			processedR.blockedByEntity->GetBoundRadius() > processedL.blockedByEntity->GetBoundRadius() ? routeL : routeR,
			ped, entity, from, outRoute);
	}
	case 1:
		return TrimRoute::Apply(1, routeL, ped, entity, from, outRoute);
	case 2:
		return TrimRoute::Apply(2, routeR, ped, entity, from, outRoute);
	default:
		return 0;
	}
}
