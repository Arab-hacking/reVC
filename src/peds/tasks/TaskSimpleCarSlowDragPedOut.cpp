#include "common.h"

#include "TaskSimpleCarSlowDragPedOut.h"

#include "AnimBlendAssociation.h"
#include "AnimManager.h"
#include "Automobile.h"
#include "CarEnterExit.h"
#include "DamageManager.h"
#include "Ped.h"
#include "TaskUtilityLineUpPedWithCar.h"
#include "Vehicle.h"
#include "VehicleAnimGroupData.h"

// The vehicles nobody can be hauled out of: the pull-out animation
// does not fit their door, so entry stops at opening it.
static bool IsNoDragOutVehicle(CVehicle *veh, int32 door)
{
	const AssocGroupId groupId = (AssocGroupId)(ANIM_GROUP_STDCARAMIMS + veh->GetAnimGroupId());
	if (groupId == ANIM_GROUP_COACHCARANIMS || groupId == ANIM_GROUP_TANKCARANIMS)
		return true;
	if (groupId == ANIM_GROUP_BUSCARANIMS && door == CAR_DOOR_RF)
		return true;
	return false;
}

CTaskSimpleCarSlowDragPedOut::CTaskSimpleCarSlowDragPedOut(CVehicle *vehicle, int32 targetDoor, CTaskUtilityLineUpPedWithCar *lineUpPedWithCarTask, bool isPedStatic)
	: m_bAnimFinished(false), m_AnimAssoc(nil), m_Vehicle(vehicle), m_TargetDoor(targetDoor),
	  m_LineUpPedWithCarTask(lineUpPedWithCarTask), m_bWasPedStatic(isPedStatic)
{
	if (m_Vehicle != nil)
		m_Vehicle->RegisterReference((CEntity **)&m_Vehicle);
}

CTaskSimpleCarSlowDragPedOut::CTaskSimpleCarSlowDragPedOut(const CTaskSimpleCarSlowDragPedOut &other)
	: CTaskSimpleCarSlowDragPedOut(other.m_Vehicle, other.m_TargetDoor, other.m_LineUpPedWithCarTask, other.m_bWasPedStatic)
{
}

CTaskSimpleCarSlowDragPedOut::~CTaskSimpleCarSlowDragPedOut()
{
	if (m_Vehicle != nil)
		m_Vehicle->CleanUpOldReference((CEntity **)&m_Vehicle);
	if (m_AnimAssoc != nil)
		m_AnimAssoc->SetFinishCallback(CDefaultAnimCallback::DefaultAnimCB, nil);
}

void
CTaskSimpleCarSlowDragPedOut::FinishAnimCarSlowDragPedOutCB(CAnimBlendAssociation *anim, void *taskPtr)
{
	CTaskSimpleCarSlowDragPedOut *self = (CTaskSimpleCarSlowDragPedOut *)taskPtr;
	self->m_bAnimFinished = true;
	self->m_AnimAssoc = nil;
	if (self->m_Vehicle != nil) {
		AssocGroupId groupId;
		AnimationId animId;
		self->ComputeAnimID(groupId, animId);
		self->m_Vehicle->ProcessOpenDoor((uint32)self->m_TargetDoor, (uint32)animId, 1.0f);

		if (self->m_Vehicle->IsAutomobile()) {
			eDoors door;
			switch (self->m_TargetDoor) {
			case CAR_DOOR_RF: door = DOOR_FRONT_RIGHT; break;
			case CAR_DOOR_RR: door = DOOR_REAR_RIGHT; break;
			case CAR_DOOR_LF: door = DOOR_FRONT_LEFT; break;
			default:          door = DOOR_REAR_LEFT; break;
			}
			((CAutomobile *)self->m_Vehicle)->Damage.SetDoorOpen(door);
		}
	}
}

void
CTaskSimpleCarSlowDragPedOut::ComputeAnimID(AssocGroupId &outGroup, AnimationId &outAnimId)
{
	outAnimId = (m_TargetDoor == CAR_DOOR_RF || m_TargetDoor == CAR_DOOR_RR)
		? ANIM_ID_CAR_PULLOUT_RHS : ANIM_ID_CAR_PULLOUT_LHS;
	outGroup = m_Vehicle->GetAnimGroup().GetGroup(outAnimId);
}

void
CTaskSimpleCarSlowDragPedOut::StartAnim(CPed *ped)
{
	AssocGroupId groupId;
	AnimationId animId;
	ComputeAnimID(groupId, animId);
	m_AnimAssoc = CAnimManager::BlendAnimation(ped->GetClump(), groupId, animId, 1000.0f);
	if (m_AnimAssoc != nil)
		m_AnimAssoc->SetFinishCallback(FinishAnimCarSlowDragPedOutCB, this);
	else
		// No clip for this vehicle/group: waiting for it hangs entry.
		m_bAnimFinished = true;
}

CPed *
CTaskSimpleCarSlowDragPedOut::GetJackedPed() const
{
	if (m_TargetDoor == CAR_DOOR_LF)
		return m_Vehicle->pDriver;
	// A door with no passenger seat (index -1) has nobody to drag out.
	const int32 idx = CCarEnterExit::ComputePassengerIndexFromCarDoor(m_Vehicle, m_TargetDoor);
	return idx >= 0 ? m_Vehicle->pPassengers[idx] : nil;
}

bool
CTaskSimpleCarSlowDragPedOut::MakeAbortable(CPed *ped, eAbortPriority priority, const CEvent *event)
{
	if (priority == ABORT_PRIORITY_IMMEDIATE) {
		if (m_AnimAssoc != nil)
			m_AnimAssoc->m_BlendDelta = -1000.0f;
		if (m_Vehicle != nil) {
			AssocGroupId groupId;
			AnimationId animId;
			ComputeAnimID(groupId, animId);
			m_Vehicle->ProcessOpenDoor((uint32)m_TargetDoor, (uint32)animId, 1.0f);
		}
		return true;
	}
	return false;
}

bool
CTaskSimpleCarSlowDragPedOut::ProcessPed(CPed *ped)
{
	if (m_bAnimFinished || m_Vehicle == nil)
		return true;

	// Coach, tank (and the bus's front seat) have no pull-out: the
	// entry stops here and the victim just gets out.
	if (IsNoDragOutVehicle(m_Vehicle, m_TargetDoor))
		return true;

	if (m_AnimAssoc == nil) {
		StartAnim(ped);
		// The reference also has the two peds shout at each other
		// here (SayJacked/SayJacking); this codebase has no
		// equivalent voice-line helpers.
	}

	if (m_AnimAssoc != nil) {
		AssocGroupId groupId;
		AnimationId animId;
		ComputeAnimID(groupId, animId);
		m_Vehicle->ProcessOpenDoor((uint32)m_TargetDoor, (uint32)m_AnimAssoc->m_AnimId, m_AnimAssoc->m_CurrentTime);
	}

	return false;
}

bool
CTaskSimpleCarSlowDragPedOut::SetPedPosition(CPed *ped)
{
	m_LineUpPedWithCarTask->ProcessPed(ped, m_Vehicle, m_AnimAssoc);
	return true;
}
