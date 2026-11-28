#include "common.h"

#include "TaskSimpleCarWaitForDoorNotToBeInUse.h"

#include "CarEnterExit.h"
#include "Ped.h"
#include "Vehicle.h"

CTaskSimpleCarWaitForDoorNotToBeInUse::CTaskSimpleCarWaitForDoorNotToBeInUse(CVehicle *veh, uint32 doorToWaitFor1, uint32 doorToWaitFor2)
	: m_veh(veh)
{
	m_doorsToWaitFor[0] = doorToWaitFor1;
	m_doorsToWaitFor[1] = doorToWaitFor2;
	if (m_veh != nil)
		m_veh->RegisterReference((CEntity **)&m_veh);
}

CTaskSimpleCarWaitForDoorNotToBeInUse::CTaskSimpleCarWaitForDoorNotToBeInUse(const CTaskSimpleCarWaitForDoorNotToBeInUse &other)
	: CTaskSimpleCarWaitForDoorNotToBeInUse(other.m_veh, other.m_doorsToWaitFor[0], other.m_doorsToWaitFor[1])
{
}

CTaskSimpleCarWaitForDoorNotToBeInUse::~CTaskSimpleCarWaitForDoorNotToBeInUse()
{
	if (m_veh != nil)
		m_veh->CleanUpOldReference((CEntity **)&m_veh);
}

bool
CTaskSimpleCarWaitForDoorNotToBeInUse::MakeAbortable(CPed *ped, eAbortPriority priority, const CEvent *event)
{
	return true;
}

bool
CTaskSimpleCarWaitForDoorNotToBeInUse::ProcessPed(CPed *ped)
{
	return m_veh == nil || !CCarEnterExit::IsCarDoorInUse(m_veh, (int32)m_doorsToWaitFor[0], (int32)m_doorsToWaitFor[1]);
}

bool
CTaskSimpleCarWaitForDoorNotToBeInUse::SetPedPosition(CPed *ped)
{
	if (ped->InVehicle() && ped->m_pMyVehicle == m_veh)
		ped->SetPedPositionInCar();
	return true;
}
