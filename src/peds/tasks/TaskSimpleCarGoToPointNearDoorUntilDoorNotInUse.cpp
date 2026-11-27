#include "common.h"

#include "TaskSimpleCarGoToPointNearDoorUntilDoorNotInUse.h"

#include "CarEnterExit.h"
#include "Ped.h"
#include "TaskSimpleCarWaitForDoorNotToBeInUse.h"
#include "TaskSimpleGoToPoint.h"
#include "TaskSimpleStandStill.h"
#include "Vehicle.h"

CTaskSimpleCarGoToPointNearDoorUntilDoorNotInUse::CTaskSimpleCarGoToPointNearDoorUntilDoorNotInUse(
	CVehicle *vehicle, int32 targetDoor, const CVector &targetPt, eMoveState moveState)
	: m_TaskGoToPoint(new CTaskSimpleGoToPoint(moveState, targetPt)),
	  m_TaskWaitForDoor(new CTaskSimpleCarWaitForDoorNotToBeInUse(vehicle, (uint32)targetDoor))
{
}

CTaskSimpleCarGoToPointNearDoorUntilDoorNotInUse::CTaskSimpleCarGoToPointNearDoorUntilDoorNotInUse(
	const CTaskSimpleCarGoToPointNearDoorUntilDoorNotInUse &other)
	: CTaskSimpleCarGoToPointNearDoorUntilDoorNotInUse(other.m_TaskWaitForDoor->m_veh,
		(int32)other.m_TaskWaitForDoor->m_doorsToWaitFor[0], other.m_TaskGoToPoint->m_vecTargetPoint,
		other.m_TaskGoToPoint->m_moveState)
{
}

CTaskSimpleCarGoToPointNearDoorUntilDoorNotInUse::~CTaskSimpleCarGoToPointNearDoorUntilDoorNotInUse()
{
	delete m_TaskGoToPoint;
	delete m_TaskWaitForDoor;
}

bool
CTaskSimpleCarGoToPointNearDoorUntilDoorNotInUse::ProcessPed(CPed *ped)
{
	m_TaskGoToPoint->UpdatePoint(ComputeTarget(ped));
	if (!m_TaskGoToPoint->ProcessPed(ped))
		return m_TaskWaitForDoor->ProcessPed(ped);
	CTaskSimpleStandStill standStill(-1);
	standStill.ProcessPed(ped);
	return true;
}

CVector
CTaskSimpleCarGoToPointNearDoorUntilDoorNotInUse::ComputeTarget(CPed *ped) const
{
	CVehicle *veh = m_TaskWaitForDoor->m_veh;
	const CVector vehPos = veh->GetPosition();
	return vehPos + (CCarEnterExit::GetPositionToOpenCarDoor(veh, (int32)m_TaskWaitForDoor->m_doorsToWaitFor[0], ped) - vehPos) * 2.0f;
}
