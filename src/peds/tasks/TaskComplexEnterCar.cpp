#include "common.h"

#include "TaskComplexEnterCar.h"

#include "Automobile.h"
#include "Bike.h"
#include "CarEnterExit.h"
#include "EventDraggedOutCar.h"
#include "General.h"
#include "Pad.h"
#include "Ped.h"
#include "PedGroups.h"
#include "PedIntelligence.h"
#include "PlayerInfo.h"
#include "PlayerPed.h"
#include "TaskComplexCarSlowBeDraggedOutAndStandUp.h"
#include "TaskComplexEnterBoatAsDriver.h"
#include "TaskComplexFallAndGetUp.h"
#include "TaskComplexGoToCarDoorAndStandStill.h"
#include "TaskComplexLeaveCar.h"
#include "TaskSimpleBikeJacked.h"
#include "TaskSimpleCarAlign.h"
#include "TaskSimpleCarCloseDoorFromInside.h"
#include "TaskSimpleCarCloseDoorFromOutside.h"
#include "TaskSimpleCarDriveTimed.h"
#include "TaskSimpleCarGetIn.h"
#include "TaskSimpleCarOpenDoorFromOutside.h"
#include "TaskSimpleCarOpenLockedDoorFromOutside.h"
#include "TaskSimpleCarSetPedInAsDriver.h"
#include "TaskSimpleCarSetPedInAsPassenger.h"
#include "TaskSimpleCarSetPedOut.h"
#include "TaskSimpleCarShuffle.h"
#include "TaskSimpleCarSlowDragPedOut.h"
#include "TaskSimpleCarWaitForDoorNotToBeInUse.h"
#include "TaskSimpleNone.h"
#include "TaskSimplePickUpBike.h"
#include "TaskSimpleStandStill.h"
#include "TaskSimpleWaitUntilPedIsOutCar.h"
#include "TaskUtilityLineUpPedWithCar.h"
#include "Timer.h"
#include "Train.h"
#include "Vehicle.h"
#include "VehicleAnimGroupData.h"

// Whether the door we want can be swung at all.
static bool CanUseTargetDoor(CVehicle *car, int32 door)
{
	return car->IsDoorReady((uint32)door) || car->IsDoorFullyOpen((uint32)door) ||
		(!car->IsDoorMissing((uint32)door) && !car->IsDoorFullyOpen((uint32)door) && !car->IsDoorClosed((uint32)door));
}

// The door node as a damage-manager door.
static eDoors DoorNodeToDoor(int32 doorNode)
{
	switch (doorNode) {
	case CAR_DOOR_RF: return DOOR_FRONT_RIGHT;
	case CAR_DOOR_RR: return DOOR_REAR_RIGHT;
	case CAR_DOOR_LF: return DOOR_FRONT_LEFT;
	default:          return DOOR_REAR_LEFT;
	}
}

// Mark the door as opened, the way ProcessOpenDoor leaves it.
static void OpenCarDoor(CVehicle *car, int32 door)
{
	if (car->IsAutomobile())
		((CAutomobile *)car)->Damage.SetDoorOpen(DoorNodeToDoor(door));
}

// The vehicle's entry anim group as an AssocGroupId.
static AssocGroupId VehicleEntryGroupId(CVehicle *car)
{
	return (AssocGroupId)(ANIM_GROUP_STDCARAMIMS + car->GetAnimGroupId());
}

CTaskComplexEnterCar::CTaskComplexEnterCar(CVehicle *targetVehicle, bool bAsDriver, bool bQuitAfterOpeningDoor, bool bQuitAfterDraggingPedOut, bool bCarryOnAfterFallingOff)
	: m_Car(targetVehicle), m_TargetDoor(0), m_TargetDoorOppositeToFlag(0), m_TargetSeat(-1),
	  m_DraggedPedDownTime(0), m_MoveState(PEDMOVE_RUN), m_NumGettingInSet(0), m_CamMovementChoice(0),
	  m_LineUpUtility(nil), m_bAborting(false), m_DraggedPed(nil), m_DoorFlagsSet(0),
	  m_CruiseSpeed(-1.0f), m_EnterCarStartTime(0)
{
	m_bAsDriver = bAsDriver;
	m_bQuitAfterOpeningDoor = bQuitAfterOpeningDoor;
	m_bQuitAfterDraggingPedOut = bQuitAfterDraggingPedOut;
	m_bCarryOnAfterFallingOff = bCarryOnAfterFallingOff;
	if (m_Car != nil)
		m_Car->RegisterReference((CEntity **)&m_Car);
}

CTaskComplexEnterCar::CTaskComplexEnterCar(const CTaskComplexEnterCar &other)
	: CTaskComplexEnterCar(other.m_Car, other.m_bAsDriver, other.m_bQuitAfterOpeningDoor,
		  other.m_bQuitAfterDraggingPedOut, other.m_bCarryOnAfterFallingOff)
{
	m_TargetDoor = other.m_TargetDoor;
	m_TargetSeat = other.m_TargetSeat;
	m_MoveState = other.m_MoveState;
	m_CruiseSpeed = other.m_CruiseSpeed;
}

CTaskComplexEnterCar::~CTaskComplexEnterCar()
{
	delete m_LineUpUtility;

	if (m_Car != nil) {
		m_Car->m_nNumGettingIn -= m_NumGettingInSet;
		m_Car->ClearGettingInFlags(m_DoorFlagsSet);
		m_Car->CleanUpOldReference((CEntity **)&m_Car);
	}
	if (m_DraggedPed != nil)
		m_DraggedPed->CleanUpOldReference((CEntity **)&m_DraggedPed);
}

bool
CTaskComplexEnterCar::MakeAbortable(CPed *ped, eAbortPriority priority, const CEvent *event)
{
	if (m_Car == nil)
		return true;
	if (priority == ABORT_PRIORITY_IMMEDIATE) {
		m_pSubTask->MakeAbortable(ped, priority, event);
		CTaskSimpleStandStill standStill(-1);
		standStill.ProcessPed(ped);
		if (m_TargetDoor != 0) {
			CTaskSimpleCarSetPedOut setOut(m_Car, m_TargetDoor, false);
			setOut.ProcessPed(ped);
			if (CCarEnterExit::CarHasDoorToClose(m_Car, m_TargetDoor)) {
				CTaskSimpleCarCloseDoorFromOutside closeDoor(m_Car, (uint32)m_TargetDoor, nil);
				closeDoor.MakeAbortable(ped, priority, event);
			}
		}
		return true;
	}
	if (m_pSubTask->GetTaskType() == TASK_COMPLEX_GO_TO_CAR_DOOR_AND_STAND_STILL)
		return m_pSubTask->MakeAbortable(ped, priority, event);
	return false;
}

CTask *
CTaskComplexEnterCar::CreateNextSubTask(CPed *ped)
{
	if (m_bAborting) {
		switch (m_pSubTask->GetTaskType()) {
		case TASK_SIMPLE_CAR_SET_PED_IN_AS_PASSENGER:
		case TASK_SIMPLE_CAR_SET_PED_IN_AS_DRIVER:
			return CreateSubTask(TASK_SIMPLE_CAR_SET_PED_OUT, ped);
		default:
			return CreateSubTask(TASK_FINISHED, ped);
		}
	}

	if (m_Car == nil || !CCarEnterExit::IsVehicleHealthy(m_Car))
		return CreateSubTask(TASK_FINISHED, ped);

	switch (m_pSubTask->GetTaskType()) {
	case TASK_COMPLEX_GO_TO_CAR_DOOR_AND_STAND_STILL: {
		CTaskComplexGoToCarDoorAndStandStill *tGoToDoor = (CTaskComplexGoToCarDoorAndStandStill *)m_pSubTask;

		if (!tGoToDoor->HasAchievedDoor())
			return CreateSubTask(TASK_FINISHED, ped);

		m_TargetDoor = tGoToDoor->m_TargetDoor;
		m_TargetSeat = tGoToDoor->m_TargetSeat;
		m_TargetDoorOppositeToFlag = ComputeTargetDoorOppositeToFlag();

		if (CCarEnterExit::IsCarDoorInUse(m_Car, m_TargetDoor, m_TargetDoorOppositeToFlag)) {
			ped->SetMoveState(PEDMOVE_STILL);
			if (ped->IsPlayer()) {
				((CPlayerPed *)ped)->MoveBlendRatio() = 0.0f;
				ped->SetMoveAnim();
			} else
				ped->SetMoveAnim();
			return CreateSubTask(TASK_SIMPLE_CAR_WAIT_FOR_DOOR_NOT_TO_BE_IN_USE, ped);
		}

		if (!CanUseTargetDoor(m_Car, m_TargetDoor))
			return CreateSubTask(TASK_FINISHED, ped);

		SetVehicleFlags(ped);
		PrepareVehicleForPedEnter(ped);
		PreparePedForVehicleEnter(ped);
		CreateTaskUtilityLineUpPedWithCar(ped);

		// Bikes and quads skip the align: line up and straight in.
		return (m_Car->IsBike() || m_Car->IsSubQuad())
			? CreateNextSubTask_AfterSimpleCarAlign(ped)
			: CreateSubTask(TASK_SIMPLE_CAR_ALIGN, ped);
	}
	case TASK_SIMPLE_CAR_ALIGN:
		return CreateNextSubTask_AfterSimpleCarAlign(ped);
	case TASK_SIMPLE_CAR_OPEN_LOCKED_DOOR_FROM_OUTSIDE:
		if (m_Car != nil && m_CruiseSpeed >= 0.0f)
			m_Car->AutoPilot.m_nCruiseSpeed = (uint8)m_CruiseSpeed;
		return CreateSubTask(TASK_SIMPLE_STAND_STILL, ped);
	case TASK_SIMPLE_CAR_OPEN_DOOR_FROM_OUTSIDE:
	case TASK_SIMPLE_BIKE_PICK_UP: {
		if (m_Car->m_vecMoveSpeed.MagnitudeSqr2D() > (ped->IsPlayer() ? 0.04f : 0.01f)) {
			ped->bUsesCollision = true;
			return CreateSubTask(ped->IsPlayer() ? TASK_FINISHED : TASK_COMPLEX_FALL_AND_GET_UP, ped);
		}

		if (m_bQuitAfterOpeningDoor) {
			m_bQuitAfterOpeningDoor = false;
			return CreateSubTask(TASK_FINISHED, ped);
		}

		if (m_bQuitAfterDraggingPedOut || CCarEnterExit::IsCarSlowJackRequired(m_Car, m_TargetDoor)) {
			if (m_DraggedPed != nil)
				m_DraggedPed->CleanUpOldReference((CEntity **)&m_DraggedPed);
			m_DraggedPed = CCarEnterExit::ComputeSlowJackedPed(m_Car, m_TargetDoor);
			if (m_DraggedPed != nil)
				m_DraggedPed->RegisterReference((CEntity **)&m_DraggedPed);

			if (m_bQuitAfterDraggingPedOut || (m_DraggedPed != nil && !CPedGroups::AreInSameGroup(ped, m_DraggedPed) && !m_DraggedPed->bDontDragMeOutCar)) {
				// The reference lets a player in a mid-fade cut
				// keep his car here; there is no such game-logic
				// state in this codebase.
				if (m_DraggedPed != nil)
					return CreateDragPedOutSubTask(ped);

				if (m_bQuitAfterDraggingPedOut) {
					OpenCarDoor(m_Car, m_TargetDoor);
					return CreateSubTask(TASK_FINISHED, ped);
				}
			}
		} else if (m_DraggedPed != nil && (m_bAsDriver ? m_Car->IsDriver(m_DraggedPed)
				: m_DraggedPed == m_Car->pPassengers[CCarEnterExit::ComputePassengerIndexFromCarDoor(m_Car, m_TargetDoor)])) {
			// It is still his seat after all: just close the door.
			if (CCarEnterExit::CarHasDoorToClose(m_Car, m_TargetDoor))
				return CreateSubTask(TASK_SIMPLE_CAR_CLOSE_DOOR_FROM_OUTSIDE, ped);
			return CreateSubTask(TASK_FINISHED, ped);
		}
		return CreateSubTask(TASK_SIMPLE_CAR_GET_IN, ped);
	}
	case TASK_SIMPLE_CAR_CLOSE_DOOR_FROM_INSIDE: {
		if (m_Car == nil || !CCarEnterExit::IsVehicleHealthy(m_Car) || !CCarEnterExit::IsPedHealthy(ped))
			return CreateSubTask(TASK_FINISHED, ped);
		if (!m_bAsDriver || m_TargetDoor != CAR_DOOR_RF)
			return CreateSubTask(m_bAsDriver ? TASK_SIMPLE_CAR_SET_PED_IN_AS_DRIVER : TASK_SIMPLE_CAR_SET_PED_IN_AS_PASSENGER, ped);
		if (CPed *drvr = m_Car->pDriver) {
			CEventDraggedOutCar draggedEvent(m_Car, ped, true);
			drvr->GetIntelligence()->GetEventGroup().Add(&draggedEvent, false);
			CTask *leaveTask = (drvr->m_fHealth > 0.0f)
				? (CTask *)new CTaskComplexLeaveCar(m_Car, CCarEnterExit::ComputeTargetDoorToExit(m_Car, drvr), 0, false, true)
				: (CTask *)new CTaskComplexLeaveCar(m_Car, CCarEnterExit::ComputeTargetDoorToExit(m_Car, drvr), 0);
			drvr->GetIntelligence()->AddTaskPrimaryMaybeInGroup(leaveTask, true);
		}
		return CreateSubTask((m_Car->m_pHandlingData != nil && m_Car->m_pHandlingData->m_bTandemSeats)
			? TASK_SIMPLE_CAR_SET_PED_IN_AS_DRIVER : TASK_SIMPLE_CAR_SHUFFLE, ped);
	}
	case TASK_SIMPLE_CAR_GET_IN: {
		CCarEnterExit::MakeUndraggedPassengerPedsLeaveCar(m_Car, m_DraggedPed, ped);
		bool bDontCloseDoors = false;
		if (m_bAsDriver && ped->IsPlayer() && m_Car->IsAutomobile() && !m_Car->IsRealPlane()) {
			CPad *pad = CPad::GetPad(0);
			if (pad->GetAccelerate() >= 255 || pad->GetBrake() >= 255) {
				if (CCarEnterExit::CarHasDoorToClose(m_Car, m_TargetDoor))
					OpenCarDoor(m_Car, m_TargetDoor);
				bDontCloseDoors = true;
			}
		}

		if (CCarEnterExit::CarHasDoorToClose(m_Car, m_TargetDoor) && !bDontCloseDoors)
			return CreateSubTask(TASK_SIMPLE_CAR_CLOSE_DOOR_FROM_INSIDE, ped);

		if (!m_bAsDriver)
			return CreateSubTask(TASK_SIMPLE_CAR_SET_PED_IN_AS_PASSENGER, ped);

		if (m_TargetDoor != CAR_DOOR_RF || !m_Car->IsAutomobile() ||
		    (m_Car->m_pHandlingData != nil && m_Car->m_pHandlingData->m_bTandemSeats))
			return CreateSubTask(TASK_SIMPLE_CAR_SET_PED_IN_AS_DRIVER, ped);

		if (m_Car->pDriver == nil)
			return CreateSubTask(TASK_SIMPLE_CAR_SHUFFLE, ped);

		if (CPed *drvr = m_Car->pDriver) {
			CEventDraggedOutCar draggedEvent(m_Car, ped, true);
			drvr->GetIntelligence()->GetEventGroup().Add(&draggedEvent, false);
			CTask *leaveTask = (drvr->m_fHealth > 0.0f)
				? (CTask *)new CTaskComplexLeaveCar(m_Car, CCarEnterExit::ComputeTargetDoorToExit(m_Car, drvr), 0, false, true)
				: (CTask *)new CTaskComplexLeaveCar(m_Car, CCarEnterExit::ComputeTargetDoorToExit(m_Car, drvr), 0);
			drvr->GetIntelligence()->AddTaskPrimaryMaybeInGroup(leaveTask, true);
		}
		return CreateSubTask(TASK_SIMPLE_CAR_SHUFFLE, ped);
	}
	case TASK_SIMPLE_CAR_SHUFFLE:
		return CreateSubTask(TASK_SIMPLE_CAR_SET_PED_IN_AS_DRIVER, ped);
	case TASK_SIMPLE_CAR_WAIT_FOR_DOOR_NOT_TO_BE_IN_USE:
		if (!CanUseTargetDoor(m_Car, m_TargetDoor))
			return nil;

		SetVehicleFlags(ped);
		PrepareVehicleForPedEnter(ped);
		PreparePedForVehicleEnter(ped);
		CreateTaskUtilityLineUpPedWithCar(ped);

		return (m_Car->IsBike() || m_Car->IsSubQuad())
			? CreateNextSubTask_AfterSimpleCarAlign(ped)
			: CreateSubTask(TASK_SIMPLE_CAR_ALIGN, ped);
	case TASK_SIMPLE_CAR_QUICK_DRAG_PED_OUT:
		// Dead in the reference too: IsCarQuickJackPossible never
		// creates this task.
		CCarEnterExit::MakeUndraggedPassengerPedsLeaveCar(m_Car, m_DraggedPed, ped);
		return CreateSubTask(TASK_SIMPLE_CAR_SET_PED_IN_AS_DRIVER, ped);
	case TASK_SIMPLE_CAR_SLOW_DRAG_PED_OUT:
		if (!m_bQuitAfterDraggingPedOut)
			return CreateSubTask(TASK_SIMPLE_CAR_GET_IN, ped);
		m_bQuitAfterDraggingPedOut = false;
		return CreateSubTask(m_DraggedPed != nil ? TASK_SIMPLE_WAIT_UNTIL_PED_OUT_CAR : TASK_FINISHED, ped);
	case TASK_COMPLEX_FALL_AND_GET_UP:
		return CreateSubTask(m_bCarryOnAfterFallingOff ? TASK_COMPLEX_GO_TO_CAR_DOOR_AND_STAND_STILL : TASK_FINISHED, ped);
	case TASK_SIMPLE_CAR_CLOSE_DOOR_FROM_OUTSIDE:
	case TASK_SIMPLE_CAR_SET_PED_IN_AS_DRIVER:
	case TASK_SIMPLE_CAR_SET_PED_IN_AS_PASSENGER:
	case TASK_SIMPLE_CAR_DRIVE_TIMED:
	case TASK_SIMPLE_WAIT_UNTIL_PED_OUT_CAR:
	case TASK_COMPLEX_ENTER_BOAT_AS_DRIVER:
	case TASK_SIMPLE_STAND_STILL:
	case TASK_NONE:
		return CreateSubTask(TASK_FINISHED, ped);
	case TASK_SIMPLE_UNINTERRUPTABLE:
	case TASK_SIMPLE_PAUSE:
		return nil;
	default:
		return nil;
	}
}

CTask *
CTaskComplexEnterCar::CreateDragPedOutSubTask(CPed *ped)
{
	m_DraggedPed->GetIntelligence()->AddTaskPrimaryMaybeInGroup(
		new CTaskComplexCarSlowBeDraggedOutAndStandUp(m_Car, m_TargetDoor), true);
	CEventDraggedOutCar draggedEvent(m_Car, ped, m_Car->IsDriver(m_DraggedPed));
	m_DraggedPed->GetIntelligence()->GetEventGroup().Add(&draggedEvent, false);
	// The reference also tells the victim's group its leader has
	// been taken out of the driver's seat; there is no leader event
	// in this codebase.
	return CreateSubTask(TASK_SIMPLE_CAR_SLOW_DRAG_PED_OUT, ped);
}

CTask *
CTaskComplexEnterCar::CreateNextSubTask_AfterSimpleCarAlign(CPed *ped)
{
	// The reference has the entering ped glance at the occupants
	// through the IK chain manager; there is no such manager here,
	// so the look-at does not happen.

	if (m_Car->m_vecMoveSpeed.MagnitudeSqr2D() > (ped->IsPlayer() ? 0.04f : 0.01f)) {
		ped->bUsesCollision = true;
		return CreateSubTask(ped->IsPlayer() ? TASK_FINISHED : TASK_COMPLEX_FALL_AND_GET_UP, ped);
	}

	if (m_bQuitAfterOpeningDoor) {
		if (CCarEnterExit::CarHasDoorToOpen(m_Car, m_TargetDoor)) {
			return CreateSubTask(CCarEnterExit::CarHasOpenableDoor(m_Car, m_TargetDoor, ped)
				? TASK_SIMPLE_CAR_OPEN_DOOR_FROM_OUTSIDE : TASK_SIMPLE_CAR_OPEN_LOCKED_DOOR_FROM_OUTSIDE, ped);
		}
		m_bQuitAfterOpeningDoor = false;
		return CreateSubTask(TASK_FINISHED, ped);
	}

	// IsCarQuickJackPossible is always false in the reference, so
	// there is no quick-jack branch to take here.

	if (!m_Car->IsBike() && !m_Car->IsSubQuad()) {
		if (CCarEnterExit::CarHasDoorToOpen(m_Car, m_TargetDoor)) {
			if (!CCarEnterExit::CarHasOpenableDoor(m_Car, m_TargetDoor, ped))
				return CreateSubTask(TASK_SIMPLE_CAR_OPEN_LOCKED_DOOR_FROM_OUTSIDE, ped);
			if (m_bQuitAfterDraggingPedOut || m_bQuitAfterOpeningDoor ||
			    VehicleEntryGroupId(m_Car) != ANIM_GROUP_CONVCARANIMS || m_TargetDoor != CAR_DOOR_LF || m_Car->pDriver != nil)
				return CreateSubTask(TASK_SIMPLE_CAR_OPEN_DOOR_FROM_OUTSIDE, ped);
			return CreateSubTask(TASK_SIMPLE_CAR_GET_IN, ped);
		}

		if (!CCarEnterExit::IsCarSlowJackRequired(m_Car, m_TargetDoor))
			return CreateSubTask(m_bQuitAfterDraggingPedOut ? TASK_FINISHED : TASK_SIMPLE_CAR_GET_IN, ped);

		if (m_DraggedPed != nil)
			m_DraggedPed->CleanUpOldReference((CEntity **)&m_DraggedPed);
		m_DraggedPed = CCarEnterExit::ComputeSlowJackedPed(m_Car, m_TargetDoor);
		if (m_DraggedPed != nil)
			m_DraggedPed->RegisterReference((CEntity **)&m_DraggedPed);

		// The mid-fade car-theft guard of the reference is skipped:
		// there is no skip state in this codebase's game logic.
		if (m_DraggedPed == nil || CPedGroups::AreInSameGroup(m_DraggedPed, ped) || m_DraggedPed->bDontDragMeOutCar) {
			if (m_DraggedPed == nil || !m_bAsDriver || !m_Car->IsDriver(m_DraggedPed)) {
				if (m_DraggedPed == nil || m_bAsDriver ||
				    m_DraggedPed != m_Car->pPassengers[CCarEnterExit::ComputePassengerIndexFromCarDoor(m_Car, m_TargetDoor)])
					return CreateSubTask(TASK_SIMPLE_CAR_GET_IN, ped);
			}
			return CreateSubTask(CCarEnterExit::CarHasDoorToClose(m_Car, m_TargetDoor)
				? TASK_SIMPLE_CAR_CLOSE_DOOR_FROM_OUTSIDE : TASK_FINISHED, ped);
		}
		if (m_bQuitAfterDraggingPedOut)
			return CreateSubTask(TASK_FINISHED, ped);
		return CreateDragPedOutSubTask(ped);
	}

	// Bike or quad from here.
	if (!CCarEnterExit::IsCarSlowJackRequired(m_Car, m_TargetDoor)) {
		if (m_bQuitAfterDraggingPedOut)
			return CreateSubTask(TASK_FINISHED, ped);

		if (m_Car->GetStatus() == STATUS_ABANDONED && m_Car->IsBike()) {
			const CVector &r = m_Car->GetMatrix().GetRight();
			if (r.z >= 0.5f || r.z <= -0.5f || m_Car->GetMatrix().GetUp().z <= 0.0f)
				return CreateSubTask(TASK_SIMPLE_BIKE_PICK_UP, ped);
			((CBike *)m_Car)->bIsBeingPickedUp = true;
		}

		return CreateSubTask(TASK_SIMPLE_CAR_GET_IN, ped);
	}

	if (m_DraggedPed != nil)
		m_DraggedPed->CleanUpOldReference((CEntity **)&m_DraggedPed);
	m_DraggedPed = CCarEnterExit::ComputeSlowJackedPed(m_Car, m_TargetDoor);
	if (m_DraggedPed != nil)
		m_DraggedPed->RegisterReference((CEntity **)&m_DraggedPed);

	if (m_DraggedPed != nil) {
		// Hand the rider (and his passenger, if the driver was taken)
		// the fall-off task. The reference passes the driver's door
		// for the driver and the rear door for the pillion; this
		// codebase's door numbering has no separate bike-front door,
		// so the same two serve.
		m_DraggedPed->GetIntelligence()->AddTaskPrimaryMaybeInGroup(new CTaskSimpleBikeJacked(
			m_Car, (uint32)CAR_DOOR_LF, (uint32)m_DraggedPedDownTime, ped, true), true);
		if (m_Car->IsDriver(m_DraggedPed) && m_Car->HasPassengerAtSeat(0)) {
			m_Car->pPassengers[0]->GetIntelligence()->AddTaskPrimaryMaybeInGroup(new CTaskSimpleBikeJacked(
				m_Car, (uint32)CAR_DOOR_LR, (uint32)m_DraggedPedDownTime, ped, true), true);
		}
	}

	return CreateSubTask(m_bQuitAfterDraggingPedOut ? TASK_SIMPLE_CAR_SLOW_DRAG_PED_OUT : TASK_SIMPLE_CAR_GET_IN, ped);
}

CTask *
CTaskComplexEnterCar::CreateFirstSubTask(CPed *ped)
{
	m_EnterCarStartTime = CTimer::GetTimeInMilliseconds();

	if (m_Car == nil || m_Car->m_pCarFire != nil ||
	    !CCarEnterExit::IsVehicleHealthy(m_Car) || !CCarEnterExit::IsPedHealthy(ped) ||
	    (!m_bAsDriver && m_Car->m_nNumMaxPassengers == 0) ||
	    (m_Car->IsTrain() && ((CTrain *)m_Car)->trainFlags.bNotOnARailRoad)) {
		return CreateSubTask(TASK_FINISHED, ped);
	}

	// A group leader taking the wheel tells his group in the
	// reference; there is no leader-entered-car event here.

	if (ped->bInVehicle) {
		return CreateSubTask(ped->m_pMyVehicle == m_Car ? TASK_SIMPLE_CAR_DRIVE_TIMED : TASK_COMPLEX_LEAVE_CAR, ped);
	}

	if (m_Car->IsBoat())
		return CreateSubTask(TASK_COMPLEX_ENTER_BOAT_AS_DRIVER, ped);

	// The swimming approach to planes and boats on water is not
	// ported: peds reach those through the boat task above.
	return new CTaskComplexGoToCarDoorAndStandStill(m_Car, m_MoveState, m_bAsDriver, m_TargetSeat, 0.5f, 4.0f, 50.0f, 30000);
}

CTask *
CTaskComplexEnterCar::ControlSubTask(CPed *ped)
{
	bool bPlayerQuit = false;
	if (ped->IsPlayer() && m_Car != nil &&
	    CCarEnterExit::IsPlayerToQuitCarEnter(ped, m_Car, m_EnterCarStartTime, m_pSubTask)) {
		bPlayerQuit = true;
		// Drop the enter objective too, or the objective layer
		// hands this task straight back next frame and the player
		// can never walk away from the car.
		ped->RestorePreviousObjective();
		// The reference also tells a quitting leader's group; there
		// is no such event here.
	}

	if (ped->IsPlayer() && !m_bQuitAfterDraggingPedOut && m_Car != nil && bPlayerQuit) {
		CPed *jacked = (m_TargetDoor > 0) ? CCarEnterExit::ComputeSlowJackedPed(m_Car, m_TargetDoor) : nil;
		if (jacked != nil && jacked->bDontDragMeOutCar)
			m_bQuitAfterOpeningDoor = true;
		else
			m_bQuitAfterDraggingPedOut = true;
	}

	if (m_pSubTask != nil && m_pSubTask->GetTaskType() == TASK_COMPLEX_GO_TO_CAR_DOOR_AND_STAND_STILL) {
		CTaskComplexGoToCarDoorAndStandStill *tGoToCarDoor = (CTaskComplexGoToCarDoorAndStandStill *)m_pSubTask;
		if (tGoToCarDoor->GetTargetDoor() != 0)
			m_TargetDoorPos = tGoToCarDoor->GetTargetPt();
	}

	if (m_Car == nil || !CCarEnterExit::IsVehicleHealthy(m_Car) || !CCarEnterExit::IsPedHealthy(ped)) {
		m_bAborting = true;
		m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_LEISURE, nil);
		return m_pSubTask;
	}

	switch (m_pSubTask->GetTaskType()) {
	case TASK_SIMPLE_CAR_WAIT_FOR_DOOR_NOT_TO_BE_IN_USE:
	case TASK_COMPLEX_GO_TO_CAR_DOOR_AND_STAND_STILL:
	case TASK_NONE:
	case TASK_COMPLEX_ENTER_BOAT_AS_DRIVER:
		if (bPlayerQuit)
			return nil;
		break;
	default:
		break;
	}

	return m_pSubTask;
}

CTask *
CTaskComplexEnterCar::CreateSubTask(eTaskType taskType, CPed *ped)
{
	switch (taskType) {
	case TASK_SIMPLE_CAR_SLOW_DRAG_PED_OUT:
		// The reference also puts up the "press movement to stop"
		// help text the first time; this codebase's HUD has no
		// such string wired.
		return new CTaskSimpleCarSlowDragPedOut(m_Car, m_TargetDoor, m_LineUpUtility, m_bQuitAfterDraggingPedOut);
	case TASK_SIMPLE_WAIT_UNTIL_PED_OUT_CAR: {
		const CVector side = IsTargetDoorOnTheLeft() ? -m_Car->GetMatrix().GetRight() : m_Car->GetMatrix().GetRight();
		return new CTaskSimpleWaitUntilPedIsOutCar(m_DraggedPed, ped->GetPosition() + side);
	}
	case TASK_FINISHED: {
		if (ped == nil)
			return nil;

		if (!ped->bInVehicle && !ped->bUsesCollision) {
			ped->bUsesCollision = true;
			if (m_Car != nil && m_TargetDoor != 0)
				CTaskSimpleCarSetPedOut::PositionPedOutOfCollision(ped, m_Car, m_TargetDoor);
		}

		// The reference raises a cop-car-being-stolen event and
		// tells a driver's group he has quit here; neither event
		// exists in this codebase.

		if (m_Car != nil && m_CruiseSpeed >= 0.0f) {
			if (ped->IsPlayer() ? (!ped->bInVehicle && m_Car->pDriver != nil && m_bQuitAfterOpeningDoor)
			                    : ped->bInVehicle)
				m_Car->AutoPilot.m_nCruiseSpeed = (uint8)m_CruiseSpeed;
		}
		return nil;
	}
	case TASK_SIMPLE_CAR_SET_PED_OUT:
		return new CTaskSimpleCarSetPedOut(m_Car, m_TargetDoor, true);
	case TASK_SIMPLE_CAR_CLOSE_DOOR_FROM_OUTSIDE:
		return new CTaskSimpleCarCloseDoorFromOutside(m_Car, (uint32)m_TargetDoor, m_LineUpUtility);
	case TASK_SIMPLE_CAR_GET_IN:
		return new CTaskSimpleCarGetIn(m_Car, (uint32)m_TargetDoor, m_LineUpUtility);
	case TASK_SIMPLE_CAR_SHUFFLE:
		return new CTaskSimpleCarShuffle(m_Car, m_TargetDoor, m_LineUpUtility);
	case TASK_SIMPLE_CAR_WAIT_FOR_DOOR_NOT_TO_BE_IN_USE:
		return new CTaskSimpleCarWaitForDoorNotToBeInUse(m_Car, (uint32)m_TargetDoor, (uint32)m_TargetDoorOppositeToFlag);
	case TASK_SIMPLE_CAR_SET_PED_IN_AS_PASSENGER:
		return new CTaskSimpleCarSetPedInAsPassenger(m_Car, m_TargetDoor, false, m_LineUpUtility);
	case TASK_SIMPLE_CAR_SET_PED_IN_AS_DRIVER:
		return new CTaskSimpleCarSetPedInAsDriver(m_Car, m_LineUpUtility);
	case TASK_SIMPLE_CAR_CLOSE_DOOR_FROM_INSIDE:
		return new CTaskSimpleCarCloseDoorFromInside(m_Car, (uint32)m_TargetDoor, m_LineUpUtility);
	case TASK_COMPLEX_GO_TO_CAR_DOOR_AND_STAND_STILL:
		return new CTaskComplexGoToCarDoorAndStandStill(m_Car, m_MoveState, m_bAsDriver, m_TargetSeat, 0.5f, 4.0f, 50.0f, 30000);
	case TASK_SIMPLE_CAR_ALIGN:
		return new CTaskSimpleCarAlign(m_Car, m_TargetDoorPos, (uint32)m_TargetDoor, m_LineUpUtility);
	case TASK_SIMPLE_CAR_OPEN_DOOR_FROM_OUTSIDE:
		return new CTaskSimpleCarOpenDoorFromOutside(m_Car, (uint32)m_TargetDoor, m_LineUpUtility, m_bQuitAfterOpeningDoor);
	case TASK_SIMPLE_CAR_OPEN_LOCKED_DOOR_FROM_OUTSIDE:
		return new CTaskSimpleCarOpenLockedDoorFromOutside(m_Car, m_TargetDoor, m_LineUpUtility);
	case TASK_SIMPLE_BIKE_PICK_UP:
		return new CTaskSimplePickUpBike(m_Car, m_TargetDoor, m_LineUpUtility);
	case TASK_SIMPLE_CAR_DRIVE_TIMED:
		return new CTaskSimpleCarDriveTimed(m_Car);
	case TASK_COMPLEX_LEAVE_CAR:
		return new CTaskComplexLeaveCar(m_Car, 0, 0, true, false);
	case TASK_COMPLEX_ENTER_BOAT_AS_DRIVER:
		return new CTaskComplexEnterBoatAsDriver(m_Car);
	case TASK_COMPLEX_FALL_AND_GET_UP: {
		if (ped != nil && !ped->bInVehicle)
			ped->bUsesCollision = true;
		if (m_Car != nil) {
			CTaskSimpleCarSetPedOut::PositionPedOutOfCollision(ped, m_Car, m_TargetDoor);

			m_Car->m_nNumGettingIn -= m_NumGettingInSet;
			m_NumGettingInSet = 0;

			m_Car->ClearGettingInFlags(m_DoorFlagsSet);
			m_DoorFlagsSet = 0;

			if (m_Car->IsAutomobile() && !m_Car->IsDoorMissing((uint32)m_TargetDoor))
				OpenCarDoor(m_Car, m_TargetDoor);
		}
		return new CTaskComplexFallAndGetUp(IsTargetDoorOnTheLeft() ? ANIM_ID_KO_SPIN_L : ANIM_ID_KO_SPIN_R,
			ANIM_GROUP_DEFAULT, 2000);
	}
	case TASK_NONE:
		return new CTaskSimpleNone();
	case TASK_SIMPLE_STAND_STILL:
		return new CTaskSimpleStandStill();
	default:
		return nil;
	}
}

void
CTaskComplexEnterCar::SetVehicleFlags(CPed *ped)
{
	m_DoorFlagsSet = (uint8)CCarEnterExit::ComputeDoorFlag(m_Car, m_TargetDoor, true);
	m_Car->SetGettingInFlags(m_DoorFlagsSet);

	m_NumGettingInSet = 1;
	m_Car->m_nNumGettingIn++;

	if (m_TargetDoorOppositeToFlag != 0) {
		const int32 oppositeDoorFlag = CCarEnterExit::ComputeDoorFlag(m_Car, m_TargetDoorOppositeToFlag, true);
		if (oppositeDoorFlag != m_DoorFlagsSet) {
			m_Car->SetGettingInFlags((uint8)oppositeDoorFlag);
			m_DoorFlagsSet |= (uint8)oppositeDoorFlag;
		}
	}
}

int32
CTaskComplexEnterCar::ComputeTargetDoorOppositeToFlag() const
{
	// Only a driver entering on the right also reserves the left.
	return (m_bAsDriver && m_TargetDoor == CAR_DOOR_RF) ? CAR_DOOR_LF : 0;
}

void
CTaskComplexEnterCar::PreparePedForVehicleEnter(CPed *ped)
{
	ped->bUsesCollision = false;
	CTask *tDuck = (CTask *)ped->GetIntelligence()->GetTaskDuck();
	if (tDuck != nil)
		tDuck->MakeAbortable(ped, ABORT_PRIORITY_URGENT, nil);
}

void
CTaskComplexEnterCar::PrepareVehicleForPedEnter(CPed *ped)
{
	if (m_Car->AutoPilot.m_nCruiseSpeed != 0)
		m_CruiseSpeed = (float)m_Car->AutoPilot.m_nCruiseSpeed;
	if (!ped->IsPlayer() || !CCarEnterExit::CarHasDoorToOpen(m_Car, m_TargetDoor) ||
	    CCarEnterExit::CarHasOpenableDoor(m_Car, m_TargetDoor, ped))
		m_Car->AutoPilot.m_nCruiseSpeed = 0;
}

void
CTaskComplexEnterCar::CreateTaskUtilityLineUpPedWithCar(CPed *ped)
{
	delete m_LineUpUtility;
	m_LineUpUtility = new CTaskUtilityLineUpPedWithCar(
		CCarEnterExit::GetPositionToOpenCarDoor(m_Car, m_TargetDoor, ped) - ped->GetPosition(),
		600, 0, m_TargetDoor);
}

CVector
CTaskComplexEnterCar::GetTargetPos() const
{
	if (m_TargetDoor != 0)
		return m_TargetDoorPos;
	if (m_pSubTask != nil && m_pSubTask->GetTaskType() == TASK_COMPLEX_GO_TO_CAR_DOOR_AND_STAND_STILL)
		return ((CTaskComplexGoToCarDoorAndStandStill *)m_pSubTask)->GetTargetPt();
	return CVector(0.0f, 0.0f, 0.0f);
}

CVehicle *
CTaskComplexEnterCar::GetCameraAvoidVehicle()
{
	if (m_Car != nil && m_pSubTask != nil) {
		switch (m_pSubTask->GetTaskType()) {
		case TASK_COMPLEX_LEAVE_CAR:
		case TASK_SIMPLE_CAR_OPEN_DOOR_FROM_OUTSIDE:
		case TASK_SIMPLE_CAR_OPEN_LOCKED_DOOR_FROM_OUTSIDE:
		case TASK_SIMPLE_BIKE_PICK_UP:
		case TASK_SIMPLE_STAND_STILL:
		case TASK_SIMPLE_CAR_QUICK_DRAG_PED_OUT:
		case TASK_SIMPLE_CAR_SLOW_DRAG_PED_OUT:
		case TASK_SIMPLE_CAR_SET_PED_OUT:
		case TASK_COMPLEX_ENTER_BOAT_AS_DRIVER:
		case TASK_SIMPLE_CAR_DRIVE_TIMED:
		case TASK_SIMPLE_CAR_GET_IN:
		case TASK_SIMPLE_CAR_CLOSE_DOOR_FROM_INSIDE:
		case TASK_SIMPLE_CAR_SHUFFLE:
		case TASK_SIMPLE_CAR_ALIGN:
		case TASK_SIMPLE_CAR_SET_PED_IN_AS_DRIVER:
		case TASK_SIMPLE_CAR_SET_PED_IN_AS_PASSENGER:
			return m_Car;
		default:
			break;
		}
	}
	return nil;
}
