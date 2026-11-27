#include "common.h"

#include "TaskComplexLeaveBoat.h"

#include "TaskSimpleCarSetPedOut.h"
#include "Vehicle.h"

CTaskComplexLeaveBoat::CTaskComplexLeaveBoat(CVehicle *vehicle, uint32 delay)
	: m_veh(vehicle), m_delayTimeMs(delay)
{
	if (m_veh != nil)
		m_veh->RegisterReference((CEntity **)&m_veh);
}

CTaskComplexLeaveBoat::CTaskComplexLeaveBoat(const CTaskComplexLeaveBoat &other)
	: CTaskComplexLeaveBoat(other.m_veh, other.m_delayTimeMs)
{
}

CTaskComplexLeaveBoat::~CTaskComplexLeaveBoat()
{
	if (m_veh != nil)
		m_veh->CleanUpOldReference((CEntity **)&m_veh);
}

CTask *
CTaskComplexLeaveBoat::CreateSubTask(eTaskType taskType)
{
	switch (taskType) {
	case TASK_SIMPLE_CAR_SET_PED_OUT:
		return new CTaskSimpleCarSetPedOut(m_veh, CAR_DOOR_LF, true);
	default:
		return nil;
	}
}

CTask *
CTaskComplexLeaveBoat::CreateFirstSubTask(CPed *ped)
{
	return CreateSubTask(TASK_SIMPLE_CAR_SET_PED_OUT);
}
