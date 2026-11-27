#include "common.h"

#include "TaskComplexLeaveCar.h"

#include "AnimManager.h"
#include "Automobile.h"
#include "Bike.h"
#include "CarEnterExit.h"
#include "DamageManager.h"
#include "Event.h"
#include "EventDamage.h"
#include "Ped.h"
#include "TaskComplexCarSlowBeDraggedOut.h"
#include "TaskComplexGetUpAndStandStill.h"
#include "TaskComplexLeaveBoat.h"
#include "TaskSimpleCarCloseDoorFromOutside.h"
#include "TaskSimpleCarDriveTimed.h"
#include "TaskSimpleCarForcePedOut.h"
#include "TaskSimpleCarGetOut.h"
#include "TaskSimpleCarJumpOut.h"
#include "TaskSimpleCarSetPedOut.h"
#include "TaskSimpleCarWaitForDoorNotToBeInUse.h"
#include "TaskSimpleCarWaitToSlowDown.h"
#include "TaskSimpleDie.h"
#include "TaskSimplePause.h"
#include "TaskSimpleTrainGetOut.h"
#include "TaskUtilityLineUpPedWithCar.h"
#include "Vehicle.h"
#include "World.h"

// Is the door being used to get in, or to get out, right now?
static void IsDoorInUse(const CVehicle *vehicle, int32 door, bool &gettingIn, bool &gettingOut)
{
	uint8 mask;
	switch (door) {
	case CAR_DOOR_RF: mask = 4; break;
	case CAR_DOOR_RR: mask = 8; break;
	case CAR_DOOR_LF: mask = 1; break;
	case CAR_DOOR_LR: mask = 2; break;
	default: return;
	}
	if (vehicle->m_nGettingInFlags & mask)
		gettingIn = true;
	if (vehicle->m_nGettingOutFlags & mask)
		gettingOut = true;
}

static uint32 NumContactWheels(const CVehicle *vehicle)
{
	switch (vehicle->m_nVehicleType) {
	case VEHICLE_TYPE_BIKE:       return ((const CBike *)vehicle)->m_nWheelsOnGround;
	case VEHICLE_TYPE_AUTOMOBILE: return ((const CAutomobile *)vehicle)->m_nWheelsOnGround;
	default: return 0;
	}
}

CTaskComplexLeaveCar::CTaskComplexLeaveCar(CVehicle *targetVehicle, int32 nTargetDoor, int32 nDelayTime, bool bSensibleLeaveCar, bool bForceGetOut)
	: m_pTargetVehicle(targetVehicle), m_nTargetDoor(nTargetDoor), m_nDelayTime(nDelayTime),
	  m_bSensibleLeaveCar(bSensibleLeaveCar), m_bForceGetOut(bForceGetOut), m_bDie(false),
	  m_pTaskUtilityLineUpPedWithCar(nil), m_nDoorFlagsSet(0), m_nNumGettingInSet(0),
	  m_nDieAnimID(ANIM_ID_KO_SHOT_FRONT_0), m_fDieAnimBlendDelta(4.0f), m_fDieAnimSpeed(1.0f),
	  m_bIsInAir(false)
{
	if (m_pTargetVehicle != nil)
		m_pTargetVehicle->RegisterReference((CEntity **)&m_pTargetVehicle);
}

// The three-argument form is the leave-and-die one.
CTaskComplexLeaveCar::CTaskComplexLeaveCar(CVehicle *targetVehicle, int32 nTargetDoor, int32 nDelayTime)
	: CTaskComplexLeaveCar(targetVehicle, nTargetDoor, nDelayTime, false, true)
{
	m_bDie = true;
}

CTaskComplexLeaveCar::CTaskComplexLeaveCar(const CTaskComplexLeaveCar &other)
	: CTaskComplexLeaveCar(other.m_pTargetVehicle, other.m_nTargetDoor, other.m_nDelayTime, other.m_bSensibleLeaveCar, other.m_bForceGetOut)
{
	m_bDie = other.m_bDie;
	m_nDieAnimID = other.m_nDieAnimID;
	m_fDieAnimBlendDelta = other.m_fDieAnimBlendDelta;
	m_fDieAnimSpeed = other.m_fDieAnimSpeed;
}

CTaskComplexLeaveCar::~CTaskComplexLeaveCar()
{
	if (m_pTargetVehicle != nil) {
		m_pTargetVehicle->ClearGettingOutFlags(m_nDoorFlagsSet);
		m_pTargetVehicle->m_nNumGettingIn -= m_nNumGettingInSet;
		m_pTargetVehicle->CleanUpOldReference((CEntity **)&m_pTargetVehicle);
	}
	delete m_pTaskUtilityLineUpPedWithCar;
}

bool
CTaskComplexLeaveCar::MakeAbortable(CPed *ped, eAbortPriority priority, const CEvent *event)
{
	if (m_pTargetVehicle == nil)
		return true;

	if (priority == ABORT_PRIORITY_IMMEDIATE) {
		m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_IMMEDIATE, event);
		if (ped->bInVehicle) {
			ComputeTargetDoor(ped);
			CTaskSimpleCarCloseDoorFromOutside closeDoor(m_pTargetVehicle, (uint32)m_nTargetDoor, nil);
			closeDoor.MakeAbortable(ped, ABORT_PRIORITY_IMMEDIATE, event);
			CTaskSimpleCarSetPedOut setPedOut(m_pTargetVehicle, m_nTargetDoor, m_bSensibleLeaveCar, false);
			setPedOut.ProcessPed(ped);
		}
		m_pTargetVehicle->ClearGettingOutFlags(m_nDoorFlagsSet);
		m_nDoorFlagsSet = 0;
		m_pTargetVehicle->m_nNumGettingIn -= m_nNumGettingInSet;
		m_nNumGettingInSet = 0;
		return true;
	}

	if (priority != ABORT_PRIORITY_URGENT)
		return false;

	const bool killed = event != nil && event->GetEventType() == EVENT_DAMAGE &&
		((const CEventDamage *)event)->HasKilledPed();

	switch (m_pSubTask->GetTaskType()) {
	case TASK_SIMPLE_DIE:
		return m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, event);
	case TASK_SIMPLE_CAR_WAIT_TO_SLOW_DOWN:
	case TASK_COMPLEX_GET_UP_AND_STAND_STILL:
		if (killed)
			return m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, event);
		return false;
	case TASK_SIMPLE_CAR_JUMP_OUT:
		if (event == nil)
			return false;
		if (event->GetEventType() == EVENT_IN_AIR || event->GetEventType() == EVENT_IN_WATER) {
			if (event->GetEventType() == EVENT_IN_AIR)
				m_bIsInAir = true;
			if (!m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, event))
				return false;

			float blendDelta;
			// This game has no parachute weapon, so the airborne
			// branch of the reference cannot be taken; a ped coming
			// down into water is thrown clear of the fall anim.
			if (event->GetEventType() == EVENT_IN_WATER) {
				if (!(ped->m_vecMoveSpeed.z < -0.3f))
					return false;
				blendDelta = 16.0f;
			} else {
				return false;
			}
			CAnimManager::BlendAnimation(ped->GetClump(), ANIM_GROUP_DEFAULT, ANIM_ID_FALL_FRONT, blendDelta);
			return false;
		}
		if (m_pSubTask->GetTaskType() == TASK_SIMPLE_CAR_DRIVE_TIMED)
			return m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, event);
		return false;
	case TASK_SIMPLE_CAR_DRIVE_TIMED:
		if (event == nil)
			return false;
		return m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_URGENT, event);
	default:
		return false;
	}
}

void
CTaskComplexLeaveCar::ComputeTargetDoor(CPed *ped)
{
	if (m_nTargetDoor == 0)
		m_nTargetDoor = CCarEnterExit::ComputeTargetDoorToExit(m_pTargetVehicle, ped);
}

// Reserve the door and an occupant slot on the way out, and quiet
// the car down if this ped was driving it.
void
CTaskComplexLeaveCar::RegisterPedLeaving(CPed *ped)
{
	const uint8 doorFlags = (uint8)CCarEnterExit::ComputeDoorFlag(m_pTargetVehicle, m_nTargetDoor, true);
	m_nDoorFlagsSet = doorFlags;
	m_pTargetVehicle->SetGettingOutFlags(doorFlags);
	m_nNumGettingInSet = 1;
	m_pTargetVehicle->m_nNumGettingIn++;

	if (m_pTargetVehicle->pDriver != nil && !m_pTargetVehicle->pDriver->IsPlayer() &&
	    ped == m_pTargetVehicle->pDriver && m_bSensibleLeaveCar) {
		m_pTargetVehicle->AutoPilot.m_nCruiseSpeed = 0;
		m_pTargetVehicle->AutoPilot.m_nCarMission = MISSION_NONE;
	}
	if (ped->IsPlayer() && ped == m_pTargetVehicle->pDriver) {
		// The reference stops the car dead under the player
		// (STATUS_FORCED_STOP, which this codebase has no status
		// for); abandoned is its "nobody is driving" state.
		m_pTargetVehicle->SetStatus(STATUS_ABANDONED);
	}
}

void
CTaskComplexLeaveCar::CreateLineUpUtility(CPed *ped)
{
	m_pTaskUtilityLineUpPedWithCar = new CTaskUtilityLineUpPedWithCar(CVector(0.0f, 0.0f, 0.0f), 0, 0, m_nTargetDoor);
}

CTask *
CTaskComplexLeaveCar::CreateNextSubTask(CPed *ped)
{
	switch (m_pSubTask->GetTaskType()) {
	case TASK_SIMPLE_PAUSE:
		return CreateSubTask(TASK_FINISHED, ped);
	case TASK_COMPLEX_GET_UP_AND_STAND_STILL:
		return CreateSubTask(ped->bInVehicle ? TASK_SIMPLE_CAR_SET_PED_OUT : TASK_FINISHED, ped);
	case TASK_SIMPLE_DIE:
		return CreateSubTask(TASK_FINISHED, ped);
	case TASK_COMPLEX_LEAVE_BOAT:
		return CreateSubTask(m_bDie ? TASK_SIMPLE_DIE : TASK_FINISHED, ped);
	case TASK_SIMPLE_CAR_DRIVE_TIMED: {
		if (!ped->bInVehicle)
			return CreateSubTask(TASK_FINISHED, ped);
		if (!ped->m_pMyVehicle->IsPassenger(ped) && !ped->m_pMyVehicle->IsDriver(ped)) {
			ped->bInVehicle = false;
			return CreateSubTask(TASK_FINISHED, ped);
		}
		ComputeTargetDoor(ped);
		if (m_nTargetDoor < 0)
			// No door maps to this ped's seat (he is not in the
			// target vehicle, or sits beyond the door table):
			// there is no door to walk out of, so just free the
			// seat if he is in fact sitting in this vehicle.
			return CreateSubTask(ped->bInVehicle ? TASK_SIMPLE_CAR_SET_PED_OUT : TASK_FINISHED, ped);
		bool gettingIn = false;
		bool gettingOut = false;
		IsDoorInUse(m_pTargetVehicle, m_nTargetDoor, gettingIn, gettingOut);
		if (!m_bForceGetOut) {
			if (gettingOut)
				return CreateSubTask(TASK_SIMPLE_CAR_WAIT_FOR_DOOR_NOT_TO_BE_IN_USE, ped);
			if (gettingIn)
				return CreateSubTask(TASK_FINISHED, ped);
		}
		return CreateSubTask(TASK_SIMPLE_CAR_WAIT_TO_SLOW_DOWN, ped);
	}
	case TASK_SIMPLE_CAR_CLOSE_DOOR_FROM_OUTSIDE:
		m_nDieAnimID = ANIM_ID_KO_SHOT_FRONT_0;
		return CreateSubTask(TASK_SIMPLE_CAR_SET_PED_OUT, ped);
	case TASK_SIMPLE_CAR_WAIT_TO_SLOW_DOWN: {
		if (!m_pTargetVehicle->CanPedStepOutCar(false)) {
			RegisterPedLeaving(ped);
			ped->SetPedState(PED_NONE);
			return CreateSubTask(TASK_SIMPLE_CAR_JUMP_OUT, ped);
		}

		bool giveUp = false;
		if (!CCarEnterExit::IsRoomForPedToLeaveCar(m_pTargetVehicle, m_nTargetDoor, nil)) {
			const CVehicle *vehicle = m_pTargetVehicle;
			int32 altDoor = 0;
			const bool isBikeOrTandem = vehicle->m_nVehicleType == VEHICLE_TYPE_BIKE ||
				(vehicle->m_pHandlingData != nil && vehicle->m_pHandlingData->m_bTandemSeats != 0);
			switch (m_nTargetDoor) {
			case CAR_DOOR_RF:
				if (vehicle->pDriver != nil || (vehicle->m_nGettingInFlags & 1)) giveUp = true;
				else altDoor = CAR_DOOR_LF;
				break;
			case CAR_DOOR_RR:
				if (vehicle->pPassengers[1] != nil || (vehicle->m_nGettingInFlags & 2)) giveUp = true;
				else altDoor = CAR_DOOR_LR;
				break;
			case CAR_DOOR_LF:
				altDoor = CAR_DOOR_RF;
				if (!isBikeOrTandem && (vehicle->pPassengers[0] != nil || (vehicle->m_nGettingInFlags & 4))) giveUp = true;
				break;
			case CAR_DOOR_LR:
				altDoor = CAR_DOOR_RR;
				if (!isBikeOrTandem && (vehicle->pPassengers[2] != nil || (vehicle->m_nGettingInFlags & 8))) giveUp = true;
				break;
			default:
				break;
			}
			if (!giveUp && !CCarEnterExit::IsRoomForPedToLeaveCar(m_pTargetVehicle, altDoor, nil))
				giveUp = true;
			if (!giveUp && vehicle->m_pHandlingData != nil && vehicle->m_pHandlingData->m_bForceDoorCheck != 0 &&
			    vehicle->m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE) {
				eDoors alt;
				switch (altDoor) {
				case CAR_DOOR_RF: alt = DOOR_FRONT_RIGHT; break;
				case CAR_DOOR_RR: alt = DOOR_REAR_RIGHT; break;
				case CAR_DOOR_LF: alt = DOOR_FRONT_LEFT; break;
				default:          alt = DOOR_REAR_LEFT; break;
				}
				if (!((CAutomobile *)vehicle)->Damage.IsDoorPresent(alt))
					giveUp = true;
			}
			if (!giveUp)
				m_nTargetDoor = altDoor;
		}

		RegisterPedLeaving(ped);
		ped->SetPedState(PED_NONE);
		if (giveUp) {
			CreateLineUpUtility(ped);
			return CreateSubTask(TASK_SIMPLE_CAR_FORCE_PED_OUT, ped);
		}
		if (m_bDie)
			return CreateSubTask(TASK_COMPLEX_CAR_SLOW_BE_DRAGGED_OUT, ped);
		CreateLineUpUtility(ped);
		return CreateSubTask(TASK_SIMPLE_CAR_GET_OUT, ped);
	}
	case TASK_SIMPLE_CAR_WAIT_FOR_DOOR_NOT_TO_BE_IN_USE:
		return CreateSubTask(TASK_SIMPLE_CAR_WAIT_TO_SLOW_DOWN, ped);
	case TASK_SIMPLE_CAR_GET_OUT: {
		m_pTaskUtilityLineUpPedWithCar->m_nDoorOpenPosType = 2;
		if (!m_bForceGetOut || m_pTargetVehicle->m_nDoorLock != CARLOCK_UNLOCKED) {
			if (CCarEnterExit::CarHasDoorToClose(m_pTargetVehicle, m_nTargetDoor))
				return CreateSubTask(TASK_SIMPLE_CAR_CLOSE_DOOR_FROM_OUTSIDE, ped);
		}
		if (m_pTargetVehicle->m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE &&
		    CCarEnterExit::CarHasDoorToClose(m_pTargetVehicle, m_nTargetDoor)) {
			CDamageManager &damage = ((CAutomobile *)m_pTargetVehicle)->Damage;
			eDoors door;
			switch (m_nTargetDoor) {
			case CAR_DOOR_RF: door = DOOR_FRONT_RIGHT; break;
			case CAR_DOOR_RR: door = DOOR_REAR_RIGHT; break;
			case CAR_DOOR_LF: door = DOOR_FRONT_LEFT; break;
			default:          door = DOOR_REAR_LEFT; break;
			}
			if (damage.GetDoorStatus(door) == DAMSTATE_OK || damage.GetDoorStatus(door) == DAMSTATE_DAMAGED)
				damage.SetDoorStatus(door, (eDoorStatus)(damage.GetDoorStatus(door) + 1));
		}
		return CreateSubTask(TASK_SIMPLE_CAR_SET_PED_OUT, ped);
	}
	case TASK_SIMPLE_CAR_JUMP_OUT: {
		if (m_bDie) {
			m_nDieAnimID = ANIM_ID_KO_SHOT_FRONT_0;
			m_fDieAnimBlendDelta = 1000.0f;
			m_fDieAnimSpeed = 0.5f;
			return CreateSubTask(TASK_SIMPLE_DIE, ped);
		}
		if (m_pTargetVehicle == nil || m_pTargetVehicle->m_nVehicleType == VEHICLE_TYPE_BIKE ||
		    m_pTargetVehicle->IsSubQuad())
			return CreateSubTask(TASK_FINISHED, ped);
		if (m_bIsInAir || ped->bIsDrowning)
			return CreateSubTask(TASK_SIMPLE_CAR_SET_PED_OUT, ped);
		m_pTargetVehicle->ClearGettingOutFlags(m_nDoorFlagsSet);
		m_nDoorFlagsSet = 0;
		m_pTargetVehicle->m_nNumGettingIn -= m_nNumGettingInSet;
		m_nNumGettingInSet = 0;
		return CreateSubTask(TASK_COMPLEX_GET_UP_AND_STAND_STILL, ped);
	}
	case TASK_SIMPLE_CAR_FORCE_PED_OUT:
		return CreateSubTask(TASK_SIMPLE_CAR_SET_PED_OUT, ped);
	case TASK_SIMPLE_CAR_SET_PED_OUT:
		return CreateSubTask(m_bDie ? TASK_SIMPLE_DIE : TASK_FINISHED, ped);
	case TASK_COMPLEX_CAR_SLOW_BE_DRAGGED_OUT:
		m_nDieAnimID = ANIM_ID_FLOOR_HIT;
		m_fDieAnimBlendDelta = 1000.0f;
		m_fDieAnimSpeed = 0.5f;
		if (m_pTargetVehicle != nil && m_pTargetVehicle->m_nDoorLock == CARLOCK_LOCKED)
			m_pTargetVehicle->m_nDoorLock = CARLOCK_UNLOCKED;
		return CreateSubTask(TASK_SIMPLE_DIE, ped);
	default:
		return nil;
	}
}

CTask *
CTaskComplexLeaveCar::CreateFirstSubTask(CPed *ped)
{
	// An arrested player, or a ped who is not in a car at all, sits
	// this one out. The reference's arrest test also reads a
	// being-arrested flag this codebase does not have.
	if ((ped->m_nPedState == PED_ARRESTED && ped->IsPlayer()) || !ped->bInVehicle)
		return new CTaskSimplePause(-1);

	CVehicle *vehicle = m_pTargetVehicle;
	if (vehicle->pDriver == ped) {
		// A group leader leaving the driver's seat tells his group;
		// this codebase has no leader-exited-car event, so the group
		// hears about it on its next think instead.
		if (!ped->IsPlayer())
			ped->SetRadioStation();
		// The reference also warns the audio that the player is
		// about to go on foot; no equivalent here.
	}

	if (vehicle->m_nVehicleType == VEHICLE_TYPE_BOAT)
		return new CTaskComplexLeaveBoat(vehicle, 0);
	if (vehicle->IsTrain())
		return new CTaskSimpleTrainGetOut(vehicle);
	return new CTaskSimpleCarDriveTimed(vehicle, m_nDelayTime);
}

CTask *
CTaskComplexLeaveCar::ControlSubTask(CPed *ped)
{
	if (m_pTargetVehicle == nil)
		return nil;

	if (!ped->bInVehicle &&
	    m_pSubTask->GetTaskType() != TASK_SIMPLE_CAR_SET_PED_OUT &&
	    m_pSubTask->GetTaskType() != TASK_SIMPLE_CAR_JUMP_OUT &&
	    m_pSubTask->GetTaskType() != TASK_COMPLEX_GET_UP_AND_STAND_STILL &&
	    m_pSubTask->GetTaskType() != TASK_SIMPLE_DIE) {
		return CreateSubTask(TASK_SIMPLE_CAR_SET_PED_OUT, ped);
	}

	if (!m_bSensibleLeaveCar && m_pSubTask->GetTaskType() == TASK_SIMPLE_CAR_WAIT_TO_SLOW_DOWN)
		m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_LEISURE, nil);

	return m_pSubTask;
}

CTask *
CTaskComplexLeaveCar::CreateSubTask(eTaskType taskType, CPed *ped)
{
	switch (taskType) {
	case TASK_SIMPLE_PAUSE:
		return new CTaskSimplePause(-1);
	case TASK_COMPLEX_GET_UP_AND_STAND_STILL:
		return new CTaskComplexGetUpAndStandStill();
	case TASK_SIMPLE_DIE:
		return new CTaskSimpleDie(ANIM_GROUP_DEFAULT, m_nDieAnimID, m_fDieAnimBlendDelta, m_fDieAnimSpeed);
	case TASK_COMPLEX_LEAVE_BOAT:
		return new CTaskComplexLeaveBoat(m_pTargetVehicle, 0);
	case TASK_SIMPLE_CAR_DRIVE_TIMED:
		return new CTaskSimpleCarDriveTimed(m_pTargetVehicle, m_nDelayTime);
	case TASK_SIMPLE_CAR_CLOSE_DOOR_FROM_OUTSIDE:
		return new CTaskSimpleCarCloseDoorFromOutside(m_pTargetVehicle, (uint32)m_nTargetDoor, m_pTaskUtilityLineUpPedWithCar);
	case TASK_SIMPLE_CAR_WAIT_TO_SLOW_DOWN: {
		if (m_bDie)
			return new CTaskSimpleCarWaitToSlowDown(m_pTargetVehicle, CTaskSimpleCarWaitToSlowDown::DONT_WAIT);
		if (m_bSensibleLeaveCar) {
			const bool isPlayer = ped->IsPlayer();
			return new CTaskSimpleCarWaitToSlowDown(m_pTargetVehicle,
				isPlayer ? CTaskSimpleCarWaitToSlowDown::SLOW_ENOUGH_TO_STEP_OR_JUMP
				         : CTaskSimpleCarWaitToSlowDown::SLOW_ENOUGH_TO_STEP);
		}
		return new CTaskSimpleCarWaitToSlowDown(m_pTargetVehicle, CTaskSimpleCarWaitToSlowDown::DONT_WAIT);
	}
	case TASK_SIMPLE_CAR_WAIT_FOR_DOOR_NOT_TO_BE_IN_USE:
		return new CTaskSimpleCarWaitForDoorNotToBeInUse(m_pTargetVehicle, (uint32)m_nTargetDoor, 0);
	case TASK_SIMPLE_CAR_GET_OUT:
		return new CTaskSimpleCarGetOut(m_pTargetVehicle, (uint32)m_nTargetDoor, m_pTaskUtilityLineUpPedWithCar);
	case TASK_SIMPLE_CAR_JUMP_OUT:
		return new CTaskSimpleCarJumpOut(m_pTargetVehicle, (uint32)m_nTargetDoor, m_pTaskUtilityLineUpPedWithCar);
	case TASK_SIMPLE_CAR_FORCE_PED_OUT:
		return new CTaskSimpleCarForcePedOut(m_pTargetVehicle, m_nTargetDoor);
	case TASK_SIMPLE_CAR_SET_PED_OUT:
		return new CTaskSimpleCarSetPedOut(m_pTargetVehicle, m_nTargetDoor, m_bSensibleLeaveCar);
	case TASK_COMPLEX_CAR_SLOW_BE_DRAGGED_OUT:
		return new CTaskComplexCarSlowBeDraggedOut(m_pTargetVehicle, m_nTargetDoor, true);
	default:
		return nil;
	}
}
