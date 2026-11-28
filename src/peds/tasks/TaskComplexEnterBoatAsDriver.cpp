#include "common.h"

#include "TaskComplexEnterBoatAsDriver.h"

#include "Ped.h"
#include "TaskComplexGetOnBoatSeat.h"
#include "TaskComplexGoToBoatSteeringWheel.h"
#include "TaskSimpleCarSetPedInAsDriver.h"
#include "Vehicle.h"

CTaskComplexEnterBoatAsDriver::CTaskComplexEnterBoatAsDriver(CVehicle *target)
	: m_EnterInto(target)
{
	if (m_EnterInto != nil)
		m_EnterInto->RegisterReference((CEntity **)&m_EnterInto);
}

CTaskComplexEnterBoatAsDriver::CTaskComplexEnterBoatAsDriver(const CTaskComplexEnterBoatAsDriver &other)
	: CTaskComplexEnterBoatAsDriver(other.m_EnterInto)
{
}

CTaskComplexEnterBoatAsDriver::~CTaskComplexEnterBoatAsDriver()
{
	if (m_EnterInto != nil)
		m_EnterInto->CleanUpOldReference((CEntity **)&m_EnterInto);
}

CTask *
CTaskComplexEnterBoatAsDriver::CreateSubTask(eTaskType taskType)
{
	switch (taskType) {
	case TASK_COMPLEX_GET_ON_BOAT_SEAT:
		return new CTaskComplexGetOnBoatSeat(m_EnterInto);
	case TASK_SIMPLE_CAR_SET_PED_IN_AS_DRIVER:
		return new CTaskSimpleCarSetPedInAsDriver(m_EnterInto);
	case TASK_COMPLEX_GO_TO_BOAT_STEERING_WHEEL:
		return new CTaskComplexGoToBoatSteeringWheel(m_EnterInto);
	default:
		return nil;
	}
}

CTask *
CTaskComplexEnterBoatAsDriver::CreateNextSubTask(CPed *ped)
{
	eTaskType next;
	switch (m_pSubTask->GetTaskType()) {
	case TASK_COMPLEX_GO_TO_BOAT_STEERING_WHEEL:
		next = ((CTaskComplexGoToBoatSteeringWheel *)m_pSubTask)->HasAchievedPoint()
			? TASK_COMPLEX_GET_ON_BOAT_SEAT : TASK_FINISHED;
		break;
	case TASK_COMPLEX_GET_ON_BOAT_SEAT:
		next = TASK_SIMPLE_CAR_SET_PED_IN_AS_DRIVER;
		break;
	default:
		next = TASK_FINISHED;
		break;
	}
	return CreateSubTask(next);
}

CTask *
CTaskComplexEnterBoatAsDriver::CreateFirstSubTask(CPed *ped)
{
	// Only a ped already standing on (or against) this boat takes
	// the wheel; everybody else never even starts.
	const bool aboard = ped->m_pCurrentPhysSurface == (CPhysical *)m_EnterInto ||
		ped->m_pCurSurface == (CEntity *)m_EnterInto;
	return CreateSubTask(aboard ? TASK_COMPLEX_GO_TO_BOAT_STEERING_WHEEL : TASK_FINISHED);
}
