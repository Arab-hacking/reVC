#include "common.h"

#include "TaskSimpleCarForcePedOut.h"

#include "Ped.h"
#include "TaskSimpleCarSetPedOut.h"
#include "Vehicle.h"

// ProcessPed is unreversed in the reference; it is the plain
// set-the-ped-out step, with the ped placed clear of the car.

CTaskSimpleCarForcePedOut::CTaskSimpleCarForcePedOut(CVehicle *veh, int32 door)
	: m_Veh(veh), m_Door(door)
{
	if (m_Veh != nil)
		m_Veh->RegisterReference((CEntity **)&m_Veh);
}

CTaskSimpleCarForcePedOut::CTaskSimpleCarForcePedOut(const CTaskSimpleCarForcePedOut &other)
	: CTaskSimpleCarForcePedOut(other.m_Veh, other.m_Door)
{
}

CTaskSimpleCarForcePedOut::~CTaskSimpleCarForcePedOut()
{
	if (m_Veh != nil)
		m_Veh->CleanUpOldReference((CEntity **)&m_Veh);
}

bool
CTaskSimpleCarForcePedOut::MakeAbortable(CPed *ped, eAbortPriority priority, const CEvent *event)
{
	return false;
}

bool
CTaskSimpleCarForcePedOut::ProcessPed(CPed *ped)
{
	if (m_Veh == nil)
		return true;

	CTaskSimpleCarSetPedOut setOut(m_Veh, m_Door, true);
	setOut.ProcessPed(ped);
	CTaskSimpleCarSetPedOut::PositionPedOutOfCollision(ped, m_Veh, m_Door);
	return true;
}
