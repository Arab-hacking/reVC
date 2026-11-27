#include "common.h"

#include "TaskSimpleCarSetPedSlowDraggedOut.h"

#include "CarEnterExit.h"
#include "Ped.h"
#include "PedIntelligence.h"
#include "PedPlacement.h"
#include "PedType.h"
#include "TaskComplexWander.h"
#include "TaskManager.h"
#include "TaskSimpleStandStill.h"
#include "Vehicle.h"
#include "VehicleAnimGroupData.h"

CTaskSimpleCarSetPedSlowDraggedOut::CTaskSimpleCarSetPedSlowDraggedOut(CVehicle *target, int32 door)
	: m_Vehicle(target), m_Door(door)
{
	if (m_Vehicle != nil)
		m_Vehicle->RegisterReference((CEntity **)&m_Vehicle);
}

CTaskSimpleCarSetPedSlowDraggedOut::CTaskSimpleCarSetPedSlowDraggedOut(const CTaskSimpleCarSetPedSlowDraggedOut &other)
	: CTaskSimpleCarSetPedSlowDraggedOut(other.m_Vehicle, other.m_Door)
{
}

CTaskSimpleCarSetPedSlowDraggedOut::~CTaskSimpleCarSetPedSlowDraggedOut()
{
	if (m_Vehicle != nil)
		m_Vehicle->CleanUpOldReference((CEntity **)&m_Vehicle);
}

bool
CTaskSimpleCarSetPedSlowDraggedOut::ProcessPed(CPed *ped)
{
	if (ped->m_pMyVehicle == nil || m_Vehicle == nil)
		return true;

	// Remove ped as passenger/driver from the vehicle.
	if (m_Vehicle->IsDriver(ped)) {
		m_Vehicle->RemoveDriver();
		if (m_Vehicle->m_nDoorLock == CARLOCK_LOCKED)
			m_Vehicle->m_nDoorLock = CARLOCK_UNLOCKED;
		if (ped->m_nPedType == PEDTYPE_COP && m_Vehicle->IsLawEnforcementVehicle())
			m_Vehicle->ChangeLawEnforcerState(0);
	} else {
		for (int32 i = 0; i < m_Vehicle->m_nNumMaxPassengers; i++) {
			if (m_Vehicle->pPassengers[i] == ped)
				m_Vehicle->RemovePassenger(ped);
		}
	}

	// Ped's not in a vehicle anymore.
	ped->bInVehicle = false;

	// Set default task after setting the ped out. The player is run
	// by his own control code; mission peds wait; the rest wander.
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
	ped->m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f);

	// Stand him by the door the jacker pulled him through.
	const CVector doorOffset = m_Vehicle->GetAnimGroup().ComputeAnimDoorOffsets(
		(m_Door == CAR_DOOR_RF || m_Door == CAR_DOOR_RR) ? JACK_PED_RIGHT : JACK_PED_LEFT);
	CVector offset = doorOffset - CCarEnterExit::ms_vecPedGetUpAnimOffset;
	offset.z = 0.0f;
	CVector pos = m_Vehicle->GetMatrix() * offset;
	CPedPlacement::FindZCoorForPed(&pos);
	ped->SetPosition(pos);
	ped->CalculateNewOrientation();

	// And not inside the car's collision, if it came to that.
	if (!CCarEnterExit::IsRoomForPedToLeaveCar(m_Vehicle, m_Door, &offset))
		ped->PositionPedOutOfCollision();

	ped->bHasJustLeftCar = true;
	ped->SetPedState(PED_IDLE);
	ped->SetMoveAnim();

	return true;
}
