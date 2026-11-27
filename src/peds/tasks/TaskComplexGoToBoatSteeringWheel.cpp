#include "common.h"

#include "TaskComplexGoToBoatSteeringWheel.h"

#include "CarEnterExit.h"
#include "Ped.h"
#include "TaskComplexGoToPointAndStandStill.h"
#include "TaskSimpleStandStill.h"
#include "Timer.h"
#include "Vehicle.h"

CTaskComplexGoToBoatSteeringWheel::CTaskComplexGoToBoatSteeringWheel(CVehicle *vehicle)
	: m_Vehicle(vehicle), m_bAchievedPoint(false), m_FirstSubTaskStartTime(0)
{
	if (m_Vehicle != nil)
		m_Vehicle->RegisterReference((CEntity **)&m_Vehicle);
}

CTaskComplexGoToBoatSteeringWheel::CTaskComplexGoToBoatSteeringWheel(const CTaskComplexGoToBoatSteeringWheel &other)
	: CTaskComplexGoToBoatSteeringWheel(other.m_Vehicle)
{
}

CTaskComplexGoToBoatSteeringWheel::~CTaskComplexGoToBoatSteeringWheel()
{
	if (m_Vehicle != nil)
		m_Vehicle->CleanUpOldReference((CEntity **)&m_Vehicle);
}

CTask *
CTaskComplexGoToBoatSteeringWheel::CreateNextSubTask(CPed *ped)
{
	switch (m_pSubTask->GetTaskType()) {
	case TASK_SIMPLE_STAND_STILL:
		return CreateSubTask(TASK_FINISHED);
	case TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL:
		m_bAchievedPoint = true;
		return CreateSubTask(TASK_FINISHED);
	default:
		return nil;
	}
}

void
CTaskComplexGoToBoatSteeringWheel::ComputeTargetPos()
{
	// The boat's helm: the driver-seat dummy, in the world.
	m_TargetPos = m_Vehicle->GetMatrix() * m_Vehicle->GetDriverSeatDummyPositionOS();
}

CTask *
CTaskComplexGoToBoatSteeringWheel::CreateSubTask(eTaskType taskType)
{
	switch (taskType) {
	case TASK_SIMPLE_STAND_STILL:
		return new CTaskSimpleStandStill(-1, false, false, 8.0f);
	case TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL:
		ComputeTargetPos();
		return new CTaskComplexGoToPointAndStandStill(PEDMOVE_WALK, m_TargetPos, 0.5f, 2.0f, false, false);
	default:
		return nil;
	}
}

CTask *
CTaskComplexGoToBoatSteeringWheel::CreateFirstSubTask(CPed *ped)
{
	m_FirstSubTaskStartTime = CTimer::GetTimeInMilliseconds();
	return CreateSubTask(TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL);
}

CTask *
CTaskComplexGoToBoatSteeringWheel::ControlSubTask(CPed *ped)
{
	if (ped->IsPlayer() && m_Vehicle != nil) {
		if (CCarEnterExit::IsPlayerToQuitCarEnter(ped, m_Vehicle, m_FirstSubTaskStartTime, m_pSubTask))
			return CreateSubTask(TASK_SIMPLE_STAND_STILL);
	}

	// Keep the destination where the boat currently is.
	CTaskComplex *subTask = (CTaskComplex *)m_pSubTask;
	CTask *subSubTask = subTask->GetSubTask();
	if (subSubTask != nil && subSubTask->GetTaskType() != TASK_SIMPLE_STAND_STILL) {
		CTaskComplexGoToPointAndStandStill *goTo = (CTaskComplexGoToPointAndStandStill *)m_pSubTask;
		ComputeTargetPos();
		if ((m_TargetPos - goTo->m_vecTargetPoint).MagnitudeSqr() > 0.04f) {
			if (goTo->m_fMoveStateRadius != 2.0f || m_TargetPos != goTo->m_vecTargetPoint)
				goTo->GoToPoint(m_TargetPos, 0.5f, 2.0f);
		}
	}

	return m_pSubTask;
}
