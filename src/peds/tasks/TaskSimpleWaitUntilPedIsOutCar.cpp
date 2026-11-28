#include "common.h"

#include "TaskSimpleWaitUntilPedIsOutCar.h"

#include "General.h"
#include "Ped.h"
#include "TaskSimpleStandStill.h"

CTaskSimpleWaitUntilPedIsOutCar::CTaskSimpleWaitUntilPedIsOutCar(CPed *ped, const CVector &pos)
	: m_PedToWaitFor(ped), m_bIsNotGoingTowardsPos(true), m_Pos(pos)
{
	if (m_PedToWaitFor != nil)
		m_PedToWaitFor->RegisterReference((CEntity **)&m_PedToWaitFor);
}

CTaskSimpleWaitUntilPedIsOutCar::CTaskSimpleWaitUntilPedIsOutCar(const CTaskSimpleWaitUntilPedIsOutCar &other)
	: CTaskSimpleWaitUntilPedIsOutCar(other.m_PedToWaitFor, other.m_Pos)
{
}

CTaskSimpleWaitUntilPedIsOutCar::~CTaskSimpleWaitUntilPedIsOutCar()
{
	if (m_PedToWaitFor != nil)
		m_PedToWaitFor->CleanUpOldReference((CEntity **)&m_PedToWaitFor);
}

bool
CTaskSimpleWaitUntilPedIsOutCar::ProcessPed(CPed *ped)
{
	if (m_PedToWaitFor == nil)
		return true;

	if (m_bIsNotGoingTowardsPos) {
		m_bIsNotGoingTowardsPos = false;

		CTaskSimpleStandStill standStillTask(1);
		standStillTask.ProcessPed(ped);

		// Face the spot he was told to watch.
		ped->SetHeading(CGeneral::LimitRadianAngle(
			CGeneral::GetAngleBetweenPoints(m_Pos.x, m_Pos.y, ped->GetPosition().x, ped->GetPosition().y)));
	}

	return !m_PedToWaitFor->bInVehicle;
}
