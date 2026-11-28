#include "common.h"

#include "TaskSimpleCarSetPedInAsPassenger.h"

#include "CarEnterExit.h"
#include "Ped.h"
#include "Vehicle.h"

// ProcessPed is unreversed in the reference, as with the driver twin:
// seat the ped in the seat for the door he used and clear the
// getting-in accounting this task was told about.

CTaskSimpleCarSetPedInAsPassenger::CTaskSimpleCarSetPedInAsPassenger(CVehicle *targetVehicle, int32 nTargetDoor, CTaskUtilityLineUpPedWithCar *utility)
	: CTaskSimpleCarSetPedInAsPassenger(targetVehicle, nTargetDoor, false, utility)
{
}

CTaskSimpleCarSetPedInAsPassenger::CTaskSimpleCarSetPedInAsPassenger(CVehicle *targetVehicle, int32 nTargetDoor, bool warpingInToCar, CTaskUtilityLineUpPedWithCar *utility)
	: m_bIsFinished(false), m_pAnim(nil), m_pTargetVehicle(targetVehicle), m_nTargetDoor(nTargetDoor),
	  m_pUtility(utility), m_bWarpingInToCar(warpingInToCar), m_nDoorFlagsToClear(0), m_nNumGettingInToClear(0)
{
	if (m_pTargetVehicle != nil)
		m_pTargetVehicle->RegisterReference((CEntity **)&m_pTargetVehicle);
}

CTaskSimpleCarSetPedInAsPassenger::CTaskSimpleCarSetPedInAsPassenger(const CTaskSimpleCarSetPedInAsPassenger &other)
	: CTaskSimpleCarSetPedInAsPassenger(other.m_pTargetVehicle, other.m_nTargetDoor, other.m_bWarpingInToCar, other.m_pUtility)
{
	m_nDoorFlagsToClear = other.m_nDoorFlagsToClear;
	m_nNumGettingInToClear = other.m_nNumGettingInToClear;
}

CTaskSimpleCarSetPedInAsPassenger::~CTaskSimpleCarSetPedInAsPassenger()
{
	if (m_pTargetVehicle != nil)
		m_pTargetVehicle->CleanUpOldReference((CEntity **)&m_pTargetVehicle);
}

bool
CTaskSimpleCarSetPedInAsPassenger::ProcessPed(CPed *ped)
{
	if (m_pTargetVehicle == nil)
		return true;

	const int32 seatIndex = CCarEnterExit::ComputePassengerIndexFromCarDoor(m_pTargetVehicle, m_nTargetDoor);
	CCarEnterExit::SetPedInCarDirect(ped, m_pTargetVehicle, seatIndex, false);

	m_pTargetVehicle->m_nNumGettingIn -= (int8)m_nNumGettingInToClear;
	m_pTargetVehicle->ClearGettingInFlags(m_nDoorFlagsToClear);

	return true;
}
