#include "common.h"

#include "TaskComplexCarSlowBeDraggedOut.h"

#include "Ped.h"
#include "TaskSimpleCarFallOut.h"
#include "TaskSimpleCarSetPedOut.h"
#include "TaskSimpleCarSetPedSlowDraggedOut.h"
#include "TaskSimpleCarSlowBeDraggedOut.h"
#include "TaskUtilityLineUpPedWithCar.h"
#include "Vehicle.h"

CTaskComplexCarSlowBeDraggedOut::CTaskComplexCarSlowBeDraggedOut(CVehicle *vehicle, int32 door, bool isDraggedBySelf)
	: m_Vehicle(vehicle), m_Door(door), m_IsDraggedBySelf(isDraggedBySelf), m_LineUpUtility(nil)
{
	if (m_Vehicle != nil)
		m_Vehicle->RegisterReference((CEntity **)&m_Vehicle);
}

CTaskComplexCarSlowBeDraggedOut::CTaskComplexCarSlowBeDraggedOut(const CTaskComplexCarSlowBeDraggedOut &other)
	: CTaskComplexCarSlowBeDraggedOut(other.m_Vehicle, other.m_Door, other.m_IsDraggedBySelf)
{
}

CTaskComplexCarSlowBeDraggedOut::~CTaskComplexCarSlowBeDraggedOut()
{
	if (m_Vehicle != nil)
		m_Vehicle->CleanUpOldReference((CEntity **)&m_Vehicle);
	delete m_LineUpUtility;
}

void
CTaskComplexCarSlowBeDraggedOut::CreateTaskUtilityLineUpPedWithCar(CPed *ped)
{
	delete m_LineUpUtility;
	m_LineUpUtility = new CTaskUtilityLineUpPedWithCar(CVector(0.0f, 0.0f, 0.0f), 0, 0, m_Door);
}

CTask *
CTaskComplexCarSlowBeDraggedOut::CreateSubTask(eTaskType taskType)
{
	switch (taskType) {
	case TASK_SIMPLE_CAR_FALL_OUT:
		return new CTaskSimpleCarFallOut(m_Vehicle, m_Door, m_LineUpUtility);
	case TASK_SIMPLE_CAR_SET_PED_SLOW_DRAGGED_OUT:
		return new CTaskSimpleCarSetPedSlowDraggedOut(m_Vehicle, m_Door);
	case TASK_SIMPLE_CAR_SET_PED_OUT:
		return new CTaskSimpleCarSetPedOut(m_Vehicle, m_Door, m_IsDraggedBySelf);
	case TASK_SIMPLE_CAR_SLOW_BE_DRAGGED_OUT:
		return new CTaskSimpleCarSlowBeDraggedOut(m_Vehicle, m_Door, m_LineUpUtility, m_IsDraggedBySelf);
	default:
		return nil;
	}
}

bool
CTaskComplexCarSlowBeDraggedOut::MakeAbortable(CPed *ped, eAbortPriority priority, const CEvent *event)
{
	if (priority == ABORT_PRIORITY_IMMEDIATE)
		return false;
	m_pSubTask->MakeAbortable(ped, priority, event);
	// Even aborted, he ends up out of the car.
	CTaskSimpleCarSetPedSlowDraggedOut(m_Vehicle, m_Door).ProcessPed(ped);
	return true;
}

CTask *
CTaskComplexCarSlowBeDraggedOut::CreateNextSubTask(CPed *ped)
{
	if (!ped->bInVehicle)
		return nil;

	eTaskType next;
	switch (m_pSubTask->GetTaskType()) {
	case TASK_SIMPLE_CAR_FALL_OUT:
		next = TASK_SIMPLE_CAR_SET_PED_OUT;
		break;
	case TASK_SIMPLE_CAR_SET_PED_OUT:
	case TASK_SIMPLE_CAR_SLOW_BE_DRAGGED_OUT:
		next = TASK_SIMPLE_CAR_SET_PED_SLOW_DRAGGED_OUT;
		break;
	case TASK_SIMPLE_CAR_SET_PED_SLOW_DRAGGED_OUT:
	default:
		return nil;
	}
	return CreateSubTask(next);
}

void
CTaskComplexCarSlowBeDraggedOut::PrepareVehicleForPedExit(CPed *ped)
{
	if (m_Door == CAR_DOOR_LF) {
		// A player-driven car stops dead; any other simply stops
		// being driven. The reference's forced-stop status has no
		// counterpart here, so both take the abandoned status.
		m_Vehicle->SetStatus(STATUS_ABANDONED);
	}
}

void
CTaskComplexCarSlowBeDraggedOut::PreparePedForVehicleExit(CPed *ped)
{
	ped->SetOrientation(0.0f, 0.0f, ped->GetHeading());
	ped->SetPedState(PED_NONE);
}

CTask *
CTaskComplexCarSlowBeDraggedOut::CreateFirstSubTask(CPed *ped)
{
	PrepareVehicleForPedExit(ped);
	PreparePedForVehicleExit(ped);
	CreateTaskUtilityLineUpPedWithCar(ped);

	return CreateSubTask(m_IsDraggedBySelf ? TASK_SIMPLE_CAR_FALL_OUT : TASK_SIMPLE_CAR_SLOW_BE_DRAGGED_OUT);
}
