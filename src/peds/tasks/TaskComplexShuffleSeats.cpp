#include "common.h"

#include "TaskComplexShuffleSeats.h"

#include "CarEnterExit.h"
#include "Ped.h"
#include "TaskSimpleCarSetPedInAsDriver.h"
#include "TaskSimpleCarSetPedInAsPassenger.h"
#include "TaskSimpleCarSetPedOut.h"
#include "TaskSimpleCarShuffle.h"
#include "TaskUtilityLineUpPedWithCar.h"
#include "Vehicle.h"

// The seat the ped sits in, named by the door it belongs to (this
// codebase has no GetPassengerIndex, so it is a short walk).
static int32 DoorForSeatOf(CVehicle *veh, CPed *ped)
{
	if (veh->IsDriver(ped))
		return CAR_DOOR_LF;
	for (int32 i = 0; i < veh->m_nNumMaxPassengers; i++) {
		if (veh->pPassengers[i] == ped) {
			// The seat-to-door mapping of ComputePassengerIndexFromCarDoor,
			// reversed.
			switch (i) {
			case 0: return CAR_DOOR_RF;
			case 1: return CAR_DOOR_LR;
			default: return CAR_DOOR_RR;
			}
		}
	}
	return CAR_DOOR_RR;
}

CTaskComplexShuffleSeats::CTaskComplexShuffleSeats(CVehicle *veh)
	: m_Veh(veh), m_OriginDoor(0), m_TargetDoor(0), m_TaskUtilityLineUpPedWithCar(nil), m_bAbort(false)
{
	if (m_Veh != nil)
		m_Veh->RegisterReference((CEntity **)&m_Veh);
}

CTaskComplexShuffleSeats::CTaskComplexShuffleSeats(const CTaskComplexShuffleSeats &other)
	: CTaskComplexShuffleSeats(other.m_Veh)
{
}

CTaskComplexShuffleSeats::~CTaskComplexShuffleSeats()
{
	if (m_Veh != nil)
		m_Veh->CleanUpOldReference((CEntity **)&m_Veh);
	delete m_TaskUtilityLineUpPedWithCar;
}

CTask *
CTaskComplexShuffleSeats::CreateSubTask(eTaskType taskType, CPed *ped)
{
	switch (taskType) {
	case TASK_SIMPLE_CAR_SET_PED_IN_AS_DRIVER:
		m_Veh->RemovePassenger(ped);
		return new CTaskSimpleCarSetPedInAsDriver(m_Veh, m_TaskUtilityLineUpPedWithCar);
	case TASK_SIMPLE_CAR_SHUFFLE:
		return new CTaskSimpleCarShuffle(m_Veh, m_OriginDoor, m_TaskUtilityLineUpPedWithCar);
	case TASK_SIMPLE_CAR_SET_PED_IN_AS_PASSENGER:
		if (m_Veh->pDriver == ped)
			m_Veh->RemoveDriver();
		else
			m_Veh->RemovePassenger(ped);
		return new CTaskSimpleCarSetPedInAsPassenger(m_Veh, m_TargetDoor, false, m_TaskUtilityLineUpPedWithCar);
	case TASK_SIMPLE_CAR_SET_PED_OUT:
		return new CTaskSimpleCarSetPedOut(m_Veh, CCarEnterExit::ComputeTargetDoorToExit(m_Veh, ped), true);
	case TASK_FINISHED:
		if (!ped->bInVehicle)
			ped->bUsesCollision = true;
		return nil;
	default:
		return nil;
	}
}

void
CTaskComplexShuffleSeats::CreateTaskUtilityLineUpPedWithCar(CPed *ped)
{
	m_OriginDoor = DoorForSeatOf(m_Veh, ped);

	m_TaskUtilityLineUpPedWithCar = new CTaskUtilityLineUpPedWithCar(
		CCarEnterExit::GetPositionToOpenCarDoor(m_Veh, m_OriginDoor) - ped->GetPosition(),
		600, 0, m_OriginDoor);
}

bool
CTaskComplexShuffleSeats::MakeAbortable(CPed *ped, eAbortPriority priority, const CEvent *event)
{
	return priority == ABORT_PRIORITY_IMMEDIATE && m_pSubTask->MakeAbortable(ped, priority, event);
}

CTask *
CTaskComplexShuffleSeats::CreateNextSubTask(CPed *ped)
{
	eTaskType next;
	if (m_bAbort) {
		switch (m_pSubTask->GetTaskType()) {
		case TASK_SIMPLE_CAR_SET_PED_IN_AS_PASSENGER:
		case TASK_SIMPLE_CAR_SHUFFLE:
		case TASK_SIMPLE_CAR_SET_PED_IN_AS_DRIVER:
			next = TASK_SIMPLE_CAR_SET_PED_OUT;
			break;
		default:
			next = TASK_FINISHED;
			break;
		}
		return CreateSubTask(next, ped);
	}

	switch (m_pSubTask->GetTaskType()) {
	case TASK_SIMPLE_CAR_SET_PED_IN_AS_DRIVER:
	case TASK_SIMPLE_CAR_SET_PED_IN_AS_PASSENGER:
	case TASK_NONE:
		next = TASK_FINISHED;
		break;
	case TASK_SIMPLE_CAR_SHUFFLE:
		// Across: front-left <-> front-right, rear-left <-> rear-right.
		switch (m_OriginDoor) {
		case CAR_DOOR_RF: m_TargetDoor = CAR_DOOR_LF; break;
		case CAR_DOOR_LF: m_TargetDoor = CAR_DOOR_RF; break;
		case CAR_DOOR_RR: m_TargetDoor = CAR_DOOR_LR; break;
		default:          m_TargetDoor = CAR_DOOR_RR; break;
		}
		if (m_TargetDoor == CAR_DOOR_LF) {
			next = (m_Veh->pDriver == nil) ? TASK_SIMPLE_CAR_SET_PED_IN_AS_DRIVER : TASK_FINISHED;
		} else {
			const int32 idx = CCarEnterExit::ComputePassengerIndexFromCarDoor(m_Veh, m_TargetDoor);
			next = (m_Veh->pPassengers[idx] == nil) ? TASK_SIMPLE_CAR_SET_PED_IN_AS_PASSENGER : TASK_FINISHED;
		}
		break;
	default:
		next = TASK_FINISHED;
		break;
	}
	return CreateSubTask(next, ped);
}

CTask *
CTaskComplexShuffleSeats::CreateFirstSubTask(CPed *ped)
{
	if (m_Veh != nil && CCarEnterExit::IsVehicleHealthy(m_Veh) && CCarEnterExit::IsPedHealthy(ped)) {
		CreateTaskUtilityLineUpPedWithCar(ped);
		return CreateSubTask(TASK_SIMPLE_CAR_SHUFFLE, ped);
	}
	return CreateSubTask(TASK_NONE, ped);
}

CTask *
CTaskComplexShuffleSeats::ControlSubTask(CPed *ped)
{
	if (m_Veh == nil || !CCarEnterExit::IsVehicleHealthy(m_Veh) || !CCarEnterExit::IsPedHealthy(ped)) {
		m_bAbort = true;
		m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_LEISURE, nil);
	}
	return m_pSubTask;
}
