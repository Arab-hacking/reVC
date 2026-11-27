#include "common.h"

#include "TaskSimpleCarSetPedOut.h"

#include "CarEnterExit.h"
#include "Ped.h"
#include "PedIntelligence.h"
#include "PedType.h"
#include "TaskComplexWander.h"
#include "TaskManager.h"
#include "TaskSimpleStandStill.h"
#include "Vehicle.h"

// ProcessPed (and PositionPedOutOfCollision) are unreversed in the
// reference. The behaviour is written from the reversed sibling that
// sets a slowly-dragged ped out: free the seat, put the ped back on
// foot, and leave him somewhere the car's collision is not.

CTaskSimpleCarSetPedOut::CTaskSimpleCarSetPedOut(CVehicle *targetVehicle, int32 nTargetDoor, bool bSwitchOffEngine)
	: CTaskSimpleCarSetPedOut(targetVehicle, nTargetDoor, bSwitchOffEngine, false)
{
}

CTaskSimpleCarSetPedOut::CTaskSimpleCarSetPedOut(CVehicle *targetVehicle, int32 nTargetDoor, bool bSwitchOffEngine, bool warpingOutOfCar)
	: m_pTargetVehicle(targetVehicle), m_nTargetDoor(nTargetDoor), m_bSwitchOffEngine(bSwitchOffEngine),
	  m_bWarpingOutOfCar(warpingOutOfCar), m_bFallingOutOfCar(false), m_bKnockedOffBike(false),
	  m_nDoorFlagsToClear(0), m_nNumGettingInToClear(0)
{
	if (m_pTargetVehicle != nil)
		m_pTargetVehicle->RegisterReference((CEntity **)&m_pTargetVehicle);
}

CTaskSimpleCarSetPedOut::CTaskSimpleCarSetPedOut(const CTaskSimpleCarSetPedOut &other)
	: CTaskSimpleCarSetPedOut(other.m_pTargetVehicle, other.m_nTargetDoor, other.m_bSwitchOffEngine, other.m_bWarpingOutOfCar)
{
	m_bFallingOutOfCar = other.m_bFallingOutOfCar;
	m_bKnockedOffBike = other.m_bKnockedOffBike;
	m_nDoorFlagsToClear = other.m_nDoorFlagsToClear;
	m_nNumGettingInToClear = other.m_nNumGettingInToClear;
}

CTaskSimpleCarSetPedOut::~CTaskSimpleCarSetPedOut()
{
	if (m_pTargetVehicle != nil)
		m_pTargetVehicle->CleanUpOldReference((CEntity **)&m_pTargetVehicle);
}

void
CTaskSimpleCarSetPedOut::PositionPedOutOfCollision(CPed *ped, CVehicle *veh, int32 door)
{
	if (ped == nil || veh == nil)
		return;
	// The ped's own placement works from the vehicle and door he is
	// registered with; make sure they are the ones asked about.
	if (ped->m_pMyVehicle == veh) {
		const uint16 prevDoor = ped->m_vehDoor;
		ped->m_vehDoor = (uint16)door;
		ped->PositionPedOutOfCollision();
		ped->m_vehDoor = prevDoor;
	} else {
		ped->SetPosition(CCarEnterExit::GetPositionToOpenCarDoor(veh, door, ped));
	}
}

bool
CTaskSimpleCarSetPedOut::ProcessPed(CPed *ped)
{
	if (ped->m_pMyVehicle == nil || m_pTargetVehicle == nil)
		return true;

	// Remove ped as passenger/driver from the vehicle.
	if (m_pTargetVehicle->IsDriver(ped)) {
		m_pTargetVehicle->RemoveDriver();
		// A dragged-out driver frees a locked-down cop car, and a cop
		// taken out of his own car stops counting as its enforcer.
		if (m_pTargetVehicle->m_nDoorLock == CARLOCK_LOCKED)
			m_pTargetVehicle->m_nDoorLock = CARLOCK_UNLOCKED;
		if (ped->m_nPedType == PEDTYPE_COP && m_pTargetVehicle->IsLawEnforcementVehicle())
			m_pTargetVehicle->ChangeLawEnforcerState(0);
	} else {
		for (int32 i = 0; i < m_pTargetVehicle->m_nNumMaxPassengers && i < 8; i++) {
			if (m_pTargetVehicle->pPassengers[i] == ped)
				m_pTargetVehicle->RemovePassenger(ped);
		}
	}

	// Ped's not in a vehicle anymore.
	ped->bInVehicle = false;
	// and the vehicle reference goes with it. Before this the ped
	// still pointed at the car he just left, and the entry machinery
	// treated him as its occupant: respawned entries opened with the
	// line-up utility dragging him (the "teleport into the car"
	// report), and IsRoomForPedToLeaveCar refused the very door the
	// ped had only just vacated, sending the exit around the car.
	if (ped->m_pMyVehicle != nil) {
		ped->m_pMyVehicle->CleanUpOldReference((CEntity **)&ped->m_pMyVehicle);
		ped->m_pMyVehicle = nil;
	}

	// Set default task after setting the ped out. The player is run by
	// his own control code; mission peds wait; everybody else wanders.
	if (!ped->IsPlayer()) {
		CTask *task;
		if (ped->m_nPedType >= PEDTYPE_MISSION1 && ped->m_nPedType <= PEDTYPE_MISSION8)
			task = new CTaskSimpleStandStill(999999, true);
		else
			task = CTaskComplexWander::GetWanderTaskByPedType(ped);
		ped->GetIntelligence()->GetTaskManager().SetTask(task, TASK_PRIMARY_DEFAULT);
	}

	ped->RestartNonPartialAnims();
	ped->bUsesCollision = true;
	ped->ReplaceWeaponWhenExitingVehicle();
	if (ped->m_pMyVehicle != nil)
		ped->m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f);

	// Position ped out of the vehicle.
	PositionPedOutOfCollision(ped, m_pTargetVehicle, m_nTargetDoor);
	ped->m_fRotationCur = ped->m_fRotationDest = ped->GetHeading();

	// Reset rotation-dependent state and leave the car behind.
	ped->bHasJustLeftCar = true;
	ped->SetPedState(PED_IDLE);
	ped->SetMoveAnim();

	// Clear the getting-in accounting this task was told about.
	m_pTargetVehicle->m_nNumGettingIn -= (int8)m_nNumGettingInToClear;
	m_pTargetVehicle->ClearGettingInFlags(m_nDoorFlagsToClear);
	m_pTargetVehicle->ClearGettingOutFlags(m_nDoorFlagsToClear);

	return true;
}
