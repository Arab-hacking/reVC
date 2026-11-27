#include "common.h"

#include "TaskComplexCarSlowBeDraggedOutAndStandUp.h"

#include "EventDeath.h"
#include "Ped.h"
#include "PedIntelligence.h"
#include "TaskComplexCarSlowBeDraggedOut.h"
#include "TaskComplexGetUpAndStandStill.h"
#include "TaskComplexLeaveCar.h"
#include "TaskSimpleCarSetPedOut.h"
#include "Vehicle.h"
#include "VehicleAnimGroupData.h"

// The vehicles nobody can be hauled out of (as in the drag-out task
// itself): their victims just get out.
static bool IsNoDragOutVehicle(CVehicle *veh, int32 door)
{
	const AssocGroupId groupId = (AssocGroupId)(ANIM_GROUP_STDCARAMIMS + veh->GetAnimGroupId());
	if (groupId == ANIM_GROUP_COACHCARANIMS || groupId == ANIM_GROUP_TANKCARANIMS)
		return true;
	if (groupId == ANIM_GROUP_BUSCARANIMS && door == CAR_DOOR_RF)
		return true;
	return false;
}

CTaskComplexCarSlowBeDraggedOutAndStandUp::CTaskComplexCarSlowBeDraggedOutAndStandUp(CVehicle *vehicle, int32 door)
	: m_Vehicle(vehicle), m_Door(door)
{
	if (m_Vehicle != nil)
		m_Vehicle->RegisterReference((CEntity **)&m_Vehicle);
}

CTaskComplexCarSlowBeDraggedOutAndStandUp::CTaskComplexCarSlowBeDraggedOutAndStandUp(const CTaskComplexCarSlowBeDraggedOutAndStandUp &other)
	: CTaskComplexCarSlowBeDraggedOutAndStandUp(other.m_Vehicle, other.m_Door)
{
}

CTaskComplexCarSlowBeDraggedOutAndStandUp::~CTaskComplexCarSlowBeDraggedOutAndStandUp()
{
	if (m_Vehicle != nil)
		m_Vehicle->CleanUpOldReference((CEntity **)&m_Vehicle);
}

CTask *
CTaskComplexCarSlowBeDraggedOutAndStandUp::CreateLeaveCarSubTask(CPed *ped) const
{
	const AssocGroupId groupId = (AssocGroupId)(ANIM_GROUP_STDCARAMIMS + m_Vehicle->GetAnimGroupId());
	switch (groupId) {
	case ANIM_GROUP_COACHCARANIMS:
		return new CTaskComplexLeaveCar(ped->m_pMyVehicle, CAR_DOOR_RF, 0, false, true);
	case ANIM_GROUP_TANKCARANIMS:
		return new CTaskComplexLeaveCar(ped->m_pMyVehicle, CAR_DOOR_LF, 0, false, true);
	case ANIM_GROUP_BUSCARANIMS:
		if (m_Door == CAR_DOOR_RF)
			return new CTaskComplexLeaveCar(ped->m_pMyVehicle, CAR_DOOR_RF, 0, false, true);
		return nil;
	default:
		return nil;
	}
}

CTask *
CTaskComplexCarSlowBeDraggedOutAndStandUp::CreateSubTask(eTaskType taskType, CPed *ped)
{
	switch (taskType) {
	case TASK_COMPLEX_LEAVE_CAR:
		return CreateLeaveCarSubTask(ped);
	case TASK_SIMPLE_CAR_SET_PED_OUT:
		return new CTaskSimpleCarSetPedOut(ped->m_pMyVehicle, m_Door, true);
	case TASK_COMPLEX_GET_UP_AND_STAND_STILL:
		return new CTaskComplexGetUpAndStandStill();
	case TASK_COMPLEX_CAR_SLOW_BE_DRAGGED_OUT:
		return new CTaskComplexCarSlowBeDraggedOut(m_Vehicle, m_Door, false);
	default:
		return nil;
	}
}

bool
CTaskComplexCarSlowBeDraggedOutAndStandUp::MakeAbortable(CPed *ped, eAbortPriority priority, const CEvent *event)
{
	return priority == ABORT_PRIORITY_IMMEDIATE && m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_IMMEDIATE, event);
}

CTask *
CTaskComplexCarSlowBeDraggedOutAndStandUp::CreateNextSubTask(CPed *ped)
{
	switch (m_pSubTask->GetTaskType()) {
	case TASK_COMPLEX_CAR_SLOW_BE_DRAGGED_OUT:
		if (ped->m_fHealth <= 0.0f) {
			// Died on the way out: say so and stop.
			CEventDeath deathEvent(false);
			ped->GetIntelligence()->GetEventGroup().Add(&deathEvent, false);
			return nil;
		}
		return new CTaskComplexGetUpAndStandStill();
	case TASK_COMPLEX_GET_UP_AND_STAND_STILL:
	case TASK_COMPLEX_LEAVE_CAR:
	case TASK_SIMPLE_CAR_SET_PED_OUT:
	default:
		return nil;
	}
}

CTask *
CTaskComplexCarSlowBeDraggedOutAndStandUp::CreateFirstSubTask(CPed *ped)
{
	CVehicle *vehicle = ped->m_pMyVehicle;
	if (vehicle == nil)
		return nil;

	if (vehicle->IsDriver(ped))
		ped->SetRadioStation();
	// A player driver would also warn his vehicle audio here; there
	// is no such call in this codebase.

	// Coaches, tanks (and the bus's front seat) cannot be dragged
	// out of: their peds just get out.
	if (IsNoDragOutVehicle(vehicle, m_Door))
		return CreateSubTask(TASK_COMPLEX_LEAVE_CAR, ped);

	// The reference also plays a jacked sound chosen by anim group
	// (head-bang, kick, punch...); this codebase has no such audio
	// events.
	return CreateSubTask(TASK_COMPLEX_CAR_SLOW_BE_DRAGGED_OUT, ped);
}
