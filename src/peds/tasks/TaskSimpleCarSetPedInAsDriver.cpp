#include "common.h"

#include "TaskSimpleCarSetPedInAsDriver.h"

#include "CarEnterExit.h"
#include "Ped.h"
#include "Vehicle.h"

// The reference leaves ProcessPed unreversed; the behaviour is the one
// every caller relies on: seat the ped directly (the entering task has
// already played the animation), and clear the getting-in accounting
// this task was told about.

CTaskSimpleCarSetPedInAsDriver::CTaskSimpleCarSetPedInAsDriver(CVehicle *targetVehicle, CTaskUtilityLineUpPedWithCar *utility)
	: m_bIsFinished(false), m_pAnim(nil), m_pTargetVehicle(targetVehicle), m_pUtility(utility),
	  m_bWarpingInToCar(false), m_nDoorFlagsToClear(0), m_nNumGettingInToClear(0)
{
	if (m_pTargetVehicle != nil)
		m_pTargetVehicle->RegisterReference((CEntity **)&m_pTargetVehicle);
}

CTaskSimpleCarSetPedInAsDriver::CTaskSimpleCarSetPedInAsDriver(CVehicle *targetVehicle, bool warpingInToCar, CTaskUtilityLineUpPedWithCar *utility)
	: CTaskSimpleCarSetPedInAsDriver(targetVehicle, utility)
{
	m_bWarpingInToCar = warpingInToCar;
}

CTaskSimpleCarSetPedInAsDriver::CTaskSimpleCarSetPedInAsDriver(const CTaskSimpleCarSetPedInAsDriver &other)
	: CTaskSimpleCarSetPedInAsDriver(other.m_pTargetVehicle, other.m_pUtility)
{
	m_bWarpingInToCar = other.m_bWarpingInToCar;
	m_nDoorFlagsToClear = other.m_nDoorFlagsToClear;
	m_nNumGettingInToClear = other.m_nNumGettingInToClear;
}

CTaskSimpleCarSetPedInAsDriver::~CTaskSimpleCarSetPedInAsDriver()
{
	if (m_pTargetVehicle != nil)
		m_pTargetVehicle->CleanUpOldReference((CEntity **)&m_pTargetVehicle);
}

bool
CTaskSimpleCarSetPedInAsDriver::ProcessPed(CPed *ped)
{
	if (m_pTargetVehicle == nil)
		return true;

	REVC_LOG_INFO("Vehicle entry task: seating ped as driver (vehicle model=%d)", m_pTargetVehicle->GetModelIndex());
	CCarEnterExit::SetPedInCarDirect(ped, m_pTargetVehicle, 0, true);

	m_pTargetVehicle->m_nNumGettingIn -= (int8)m_nNumGettingInToClear;
	m_pTargetVehicle->ClearGettingInFlags(m_nDoorFlagsToClear);

	return true;
}
