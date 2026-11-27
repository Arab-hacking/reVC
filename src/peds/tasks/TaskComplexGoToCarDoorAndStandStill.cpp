#include "common.h"

#include "TaskComplexGoToCarDoorAndStandStill.h"

#include "CarEnterExit.h"
#include "Event.h"
#include "Ped.h"
#include "PedGeometryAnalyser.h"
#include "PointRoute.h"
#include "TaskComplexFollowPointRoute.h"
#include "TaskSimpleCarGoToPointNearDoorUntilDoorNotInUse.h"
#include "TaskSimpleCarWaitForDoorNotToBeInUse.h"
#include "TaskSimpleGoToPoint.h"
#include "TaskSimplePause.h"
#include "TaskSimpleStandStill.h"
#include "Vehicle.h"

CTaskComplexGoToCarDoorAndStandStill::CTaskComplexGoToCarDoorAndStandStill(CVehicle *vehicle, eMoveState moveState,
	bool bIsDriver, int32 targetSeat, float targetRadius, float slowDownDist, float maxSeekDist, int32 maxSeekTime)
	: m_Vehicle(vehicle), m_MoveState(moveState), m_bIsDriver(bIsDriver), m_TargetRadius(targetRadius),
	  m_SlowDownDist(slowDownDist), m_MaxSeekDist(maxSeekDist), m_MaxSeekTime(maxSeekTime), m_TargetDoor(0),
	  m_TargetPt(0.0f, 0.0f, 0.0f), m_TargetSeat(targetSeat), m_bTryingToEnterInWater(false),
	  m_bAchievedTargetDoor(false), m_RouteToDoor(nil)
{
	if (m_Vehicle != nil)
		m_Vehicle->RegisterReference((CEntity **)&m_Vehicle);
}

CTaskComplexGoToCarDoorAndStandStill::CTaskComplexGoToCarDoorAndStandStill(const CTaskComplexGoToCarDoorAndStandStill &other)
	: CTaskComplexGoToCarDoorAndStandStill(other.m_Vehicle, other.m_MoveState, other.m_bIsDriver, other.m_TargetSeat,
		other.m_TargetRadius, other.m_SlowDownDist, other.m_MaxSeekDist, other.m_MaxSeekTime)
{
}

CTaskComplexGoToCarDoorAndStandStill::~CTaskComplexGoToCarDoorAndStandStill()
{
	if (m_Vehicle != nil)
		m_Vehicle->CleanUpOldReference((CEntity **)&m_Vehicle);
	delete m_RouteToDoor;
}

bool
CTaskComplexGoToCarDoorAndStandStill::MakeAbortable(CPed *ped, eAbortPriority priority, const CEvent *event)
{
	if (m_bTryingToEnterInWater) {
		if (m_pSubTask != nil && m_pSubTask->GetTaskType() == TASK_SIMPLE_PAUSE) {
			if (event != nil) {
				const eEventType eventType = event->GetEventType();
				if (eventType == EVENT_IN_WATER || eventType == EVENT_IN_AIR)
					return false;
			}
		}
	}
	return m_pSubTask->MakeAbortable(ped, priority, event);
}

CTask *
CTaskComplexGoToCarDoorAndStandStill::CreateSubTask(eTaskType taskType, CPed *ped)
{
	switch (taskType) {
	case TASK_SIMPLE_GO_TO_POINT_NEAR_CAR_DOOR_UNTIL_DOOR_NOT_IN_USE:
		ped->bHasJustLeftCar = true;
		return new CTaskSimpleCarGoToPointNearDoorUntilDoorNotInUse(m_Vehicle, m_TargetDoor, m_TargetPt, m_MoveState);
	case TASK_SIMPLE_PAUSE:
		return new CTaskSimplePause(1);
	case TASK_SIMPLE_STAND_STILL:
		return new CTaskSimpleStandStill(1);
	case TASK_SIMPLE_CAR_WAIT_FOR_DOOR_NOT_TO_BE_IN_USE:
		return new CTaskSimpleCarWaitForDoorNotToBeInUse(m_Vehicle, (uint32)m_TargetDoor);
	case TASK_COMPLEX_FOLLOW_POINT_ROUTE:
		return new CTaskComplexFollowPointRoute(m_MoveState, *m_RouteToDoor,
			CTaskComplexFollowPointRoute::Mode::ONE_WAY, 0.5f, 5.0f, false, false, false);
	case TASK_SIMPLE_GO_TO_POINT:
		ped->bHasJustLeftCar = true;
		return new CTaskSimpleGoToPoint(m_MoveState, m_TargetPt, m_TargetRadius, true);
	case TASK_FINISHED:
		return nil;
	default:
		return nil;
	}
}

CTask *
CTaskComplexGoToCarDoorAndStandStill::CreateNextSubTask(CPed *ped)
{
	switch (m_pSubTask->GetTaskType()) {
	case TASK_COMPLEX_FOLLOW_POINT_ROUTE:
		m_TargetPt = CCarEnterExit::GetPositionToOpenCarDoor(m_Vehicle, m_TargetDoor, ped);
		return CreateSubTask(TASK_SIMPLE_GO_TO_POINT, ped);
	case TASK_SIMPLE_GO_TO_POINT:
		if (((CTaskSimpleGoToPoint *)m_pSubTask)->WasTaskSuccessful(ped))
			m_bAchievedTargetDoor = true;
		return CreateSubTask(TASK_FINISHED, ped);
	case TASK_SIMPLE_GO_TO_POINT_NEAR_CAR_DOOR_UNTIL_DOOR_NOT_IN_USE:
		return CreateSubTask(TASK_SIMPLE_CAR_WAIT_FOR_DOOR_NOT_TO_BE_IN_USE, ped);
	case TASK_SIMPLE_CAR_WAIT_FOR_DOOR_NOT_TO_BE_IN_USE:
		return CreateFirstSubTask(ped);
	case TASK_SIMPLE_STAND_STILL:
		return CreateSubTask(TASK_FINISHED, ped);
	case TASK_SIMPLE_PAUSE:
		if (m_bTryingToEnterInWater) {
			if ((ped->GetPosition() - m_TargetPt).MagnitudeSqr2D() <= 0.5f * 0.5f)
				m_bAchievedTargetDoor = true;
		}
		return CreateSubTask(TASK_FINISHED, ped);
	default:
		return nil;
	}
}

CTask *
CTaskComplexGoToCarDoorAndStandStill::CreateFirstSubTask(CPed *ped)
{
	eTaskType taskType;
	if (!IsVehicleInRange(*ped)) {
		taskType = TASK_SIMPLE_STAND_STILL;
	} else if (m_bTryingToEnterInWater) {
		taskType = TASK_SIMPLE_PAUSE;
	} else {
		// The reference stands still here when the seat asked for is
		// taken by a ped marked as a priority target; this codebase's
		// peds have no such marking.
		{
			// Calculate the door to use.
			bool bCanWaitForDoorToBeFree;
			if (!CalculateTargetDoor(ped, bCanWaitForDoorToBeFree)) {
				if (!bCanWaitForDoorToBeFree)
					return CreateSubTask(TASK_SIMPLE_STAND_STILL, ped);
			}

			// If the way to the door is blocked by the car's own
			// collision, walk around the car first.
			if (CCarEnterExit::IsPathToDoorBlockedByVehicleCollisionModel(ped, m_Vehicle, m_TargetPt)) {
				ComputeRouteToDoor(*ped);
				taskType = TASK_COMPLEX_FOLLOW_POINT_ROUTE;
			} else {
				taskType = bCanWaitForDoorToBeFree
					? TASK_SIMPLE_GO_TO_POINT_NEAR_CAR_DOOR_UNTIL_DOOR_NOT_IN_USE
					: TASK_SIMPLE_GO_TO_POINT;
			}
		}
	}
	REVC_LOG_INFO("Vehicle entry task: go-to-door subtask=%d door=%d dist=%.2f",
		taskType, m_TargetDoor, (m_TargetPt - ped->GetPosition()).Magnitude2D());
	return CreateSubTask(taskType, ped);
}

CTask *
CTaskComplexGoToCarDoorAndStandStill::ControlSubTask(CPed *ped)
{
	if (m_Vehicle == nil)
		return nil;

	if (ped->m_nPedType != PEDTYPE_COP) {
		if (m_Vehicle->pDriver != nil) {
			CPed *driver = m_Vehicle->pDriver;
			if (driver->IsPlayer() && driver->m_nPedState == PED_ARRESTED)
				return CreateSubTask(TASK_SIMPLE_STAND_STILL, ped);
		}
	}

	switch (m_pSubTask->GetTaskType()) {
	case TASK_SIMPLE_GO_TO_POINT_NEAR_CAR_DOOR_UNTIL_DOOR_NOT_IN_USE:
	case TASK_SIMPLE_PAUSE:
	case TASK_SIMPLE_STAND_STILL:
	case TASK_SIMPLE_CAR_WAIT_FOR_DOOR_NOT_TO_BE_IN_USE:
		if (m_TargetDoor != 0)
			m_TargetPt = CCarEnterExit::GetPositionToOpenCarDoor(m_Vehicle, m_TargetDoor, ped);
		return m_pSubTask;
	case TASK_SIMPLE_GO_TO_POINT: {
		if (!IsVehicleInRange(*ped)) {
			m_TargetPt = CCarEnterExit::GetPositionToOpenCarDoor(m_Vehicle, m_TargetDoor, ped);
			return CreateSubTask(TASK_SIMPLE_STAND_STILL, ped);
		}

		bool bCanWaitForDoorToBeFree;
		if (!CalculateTargetDoor(ped, bCanWaitForDoorToBeFree))
			return CreateSubTask(bCanWaitForDoorToBeFree ? TASK_SIMPLE_GO_TO_POINT_NEAR_CAR_DOOR_UNTIL_DOOR_NOT_IN_USE : TASK_SIMPLE_STAND_STILL, ped);
		((CTaskSimpleGoToPoint *)m_pSubTask)->UpdatePoint(m_TargetPt, m_TargetRadius);

		return m_pSubTask;
	}
	case TASK_COMPLEX_FOLLOW_POINT_ROUTE: {
		const CVector doorOpenPos = CCarEnterExit::GetPositionToOpenCarDoor(m_Vehicle, m_TargetDoor, ped);

		if (!IsVehicleInRange(*ped)) {
			m_TargetPt = doorOpenPos;
			return CreateSubTask(TASK_SIMPLE_STAND_STILL, ped);
		}

		if (!CCarEnterExit::IsPathToDoorBlockedByVehicleCollisionModel(ped, m_Vehicle, doorOpenPos)) {
			m_TargetPt = doorOpenPos;
			return CreateSubTask(TASK_SIMPLE_GO_TO_POINT, ped);
		}

		if ((m_TargetPt - doorOpenPos).MagnitudeSqr() >= 0.1f * 0.1f) {
			m_TargetPt = doorOpenPos;
			return CreateFirstSubTask(ped);
		}

		return m_pSubTask;
	}
	default:
		return m_pSubTask;
	}
}

bool
CTaskComplexGoToCarDoorAndStandStill::IsVehicleInRange(const CPed &ped)
{
	return (ped.GetPosition() - m_Vehicle->GetPosition()).MagnitudeSqr() <= m_MaxSeekDist * m_MaxSeekDist;
}

// Project a point onto the bounding-box plane of the side it is on.
static CVector ProjectOnToHitSidePlane(const CVector &pos, CEntity &entity, const CVector planeNormals[4], const float planeDs[4])
{
	const int32 side = (int32)CPedGeometryAnalyser::ComputeEntityHitSide(pos, entity);
	return pos - planeNormals[side] * (DotProduct(planeNormals[side], pos) + planeDs[side]);
}

// Work out the points that take the ped around the car to the door.
void
CTaskComplexGoToCarDoorAndStandStill::ComputeRouteToDoor(const CPed &ped)
{
	if (m_RouteToDoor == nil)
		m_RouteToDoor = new CPointRoute();
	m_RouteToDoor->Clear();

	const CVector pedPos = ped.GetPosition();

	CVector vehBBPlaneNormals[4];
	float vehBBplaneDs[4];
	CPedGeometryAnalyser::ComputeEntityBoundingBoxPlanes(pedPos.z, *m_Vehicle, vehBBPlaneNormals, vehBBplaneDs);

	const CVector pedPosOnBBPlane = ProjectOnToHitSidePlane(pedPos, *m_Vehicle, vehBBPlaneNormals, vehBBplaneDs);
	const CVector doorPosOnBBPlane = ProjectOnToHitSidePlane(m_TargetPt, *m_Vehicle, vehBBPlaneNormals, vehBBplaneDs);

	CPointRoute routeAroundVeh;
	CPedGeometryAnalyser::ComputeRouteRoundEntityBoundingBox(ped, pedPosOnBBPlane, *m_Vehicle, doorPosOnBBPlane, routeAroundVeh, 0);

	m_RouteToDoor->AddUnlessFull(pedPosOnBBPlane);
	for (int32 i = 0; i < routeAroundVeh.GetSize(); i++)
		m_RouteToDoor->AddUnlessFull(routeAroundVeh[i]);
	m_RouteToDoor->AddUnlessFull(doorPosOnBBPlane);
}

// Calculate the door to use and its position.
bool
CTaskComplexGoToCarDoorAndStandStill::CalculateTargetDoor(CPed *ped, bool &bCanWaitForDoorToBeFree)
{
	bCanWaitForDoorToBeFree = false;
	if (m_TargetSeat != 0) {
		m_TargetDoor = m_TargetSeat;
		m_TargetPt = CCarEnterExit::GetPositionToOpenCarDoor(m_Vehicle, m_TargetSeat, ped);
	} else {
		if (m_bIsDriver) {
			if (!CCarEnterExit::GetNearestCarDoor(ped, m_Vehicle, m_TargetPt, m_TargetDoor))
				return false;
		} else {
			if (!CCarEnterExit::GetNearestCarPassengerDoor(ped, m_Vehicle, &m_TargetPt, &m_TargetDoor, true, true, true)) {
				bCanWaitForDoorToBeFree = CCarEnterExit::GetNearestCarPassengerDoor(ped, m_Vehicle, &m_TargetPt, &m_TargetDoor, true, false, true);
				return false;
			}
		}
	}
	return true;
}
