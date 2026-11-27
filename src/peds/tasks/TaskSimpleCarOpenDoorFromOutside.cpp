#include "common.h"

#include "TaskSimpleCarOpenDoorFromOutside.h"

#include "AnimBlendAssociation.h"
#include "AnimManager.h"
#include "Automobile.h"
#include "Pad.h"
#include "Ped.h"
#include "PlayerInfo.h"
#include "TaskUtilityLineUpPedWithCar.h"
#include "Vehicle.h"
#include "VehicleAnimGroupData.h"

CTaskSimpleCarOpenDoorFromOutside::CTaskSimpleCarOpenDoorFromOutside(CVehicle *veh, uint32 door,
	CTaskUtilityLineUpPedWithCar *lineUpTask, bool disallowPlayerDriverToExitCar)
	: m_veh(veh), m_door(door), m_lineUpTask(lineUpTask), m_anim(nil), m_finished(false),
	  m_disallowPlayerDriverToExitCar(disallowPlayerDriverToExitCar), m_doorOpenAngleRatio(0.0f)
{
	if (m_veh != nil)
		m_veh->RegisterReference((CEntity **)&m_veh);
}

CTaskSimpleCarOpenDoorFromOutside::CTaskSimpleCarOpenDoorFromOutside(const CTaskSimpleCarOpenDoorFromOutside &other)
	: CTaskSimpleCarOpenDoorFromOutside(other.m_veh, other.m_door, other.m_lineUpTask, other.m_disallowPlayerDriverToExitCar)
{
}

CTaskSimpleCarOpenDoorFromOutside::~CTaskSimpleCarOpenDoorFromOutside()
{
	if (m_veh != nil)
		m_veh->CleanUpOldReference((CEntity **)&m_veh);
	if (m_anim != nil)
		m_anim->SetFinishCallback(CDefaultAnimCallback::DefaultAnimCB, nil);
}

void
CTaskSimpleCarOpenDoorFromOutside::FinishAnimCarOpenDoorFromOutsideCB(CAnimBlendAssociation *anim, void *data)
{
	CTaskSimpleCarOpenDoorFromOutside *self = (CTaskSimpleCarOpenDoorFromOutside *)data;
	self->m_anim = nil;
	self->m_finished = true;

	AssocGroupId groupId;
	AnimationId animId;
	self->ComputeAnimID(groupId, animId);
	self->m_veh->ProcessOpenDoor(self->m_door, animId, 1.0f);
}

void
CTaskSimpleCarOpenDoorFromOutside::ComputeAnimID(AssocGroupId &outGroup, AnimationId &outAnimId)
{
	switch (m_door) {
	case CAR_DOOR_RF:
		outAnimId = ANIM_ID_CAR_OPEN_RHS;
		break;
	case CAR_DOOR_RR:
		outAnimId = ANIM_ID_CAR_OPEN_RHS_1;
		break;
	case CAR_DOOR_LF:
		outAnimId = ANIM_ID_CAR_OPEN_LHS;
		break;
	default:
		outAnimId = ANIM_ID_CAR_OPEN_LHS_1;
		break;
	}
	outGroup = m_veh->GetAnimGroup().GetGroup(outAnimId);
}

void
CTaskSimpleCarOpenDoorFromOutside::StartAnim(CPed *ped)
{
	AssocGroupId groupId;
	AnimationId animId;
	ComputeAnimID(groupId, animId);
	m_anim = CAnimManager::BlendAnimation(ped->GetClump(), groupId, animId, 4.0f);
	if (m_anim != nil)
		m_anim->SetFinishCallback(FinishAnimCarOpenDoorFromOutsideCB, this);
}

bool
CTaskSimpleCarOpenDoorFromOutside::MakeAbortable(CPed *ped, eAbortPriority priority, const CEvent *event)
{
	if (priority == ABORT_PRIORITY_IMMEDIATE) {
		if (m_anim != nil)
			m_anim->m_BlendDelta = -1000.0f;
		if (m_veh != nil) {
			AssocGroupId groupId;
			AnimationId animId;
			ComputeAnimID(groupId, animId);
			m_veh->ProcessOpenDoor(m_door, animId, 1.0f);
		}
		return true;
	}
	return false;
}

// The car's door-open ratio, by door node.
static float GetDoorAngleOpenRatio(CVehicle *veh, uint32 doorNode)
{
	if (veh == nil || !veh->IsAutomobile())
		return 0.0f;
	eDoors door;
	switch (doorNode) {
	case CAR_DOOR_RF: door = DOOR_FRONT_RIGHT; break;
	case CAR_DOOR_RR: door = DOOR_REAR_RIGHT; break;
	case CAR_DOOR_LF: door = DOOR_FRONT_LEFT; break;
	case CAR_DOOR_LR: door = DOOR_REAR_LEFT; break;
	default: return 0.0f;
	}
	return ((CAutomobile *)veh)->Doors[door].GetAngleOpenRatio();
}

bool
CTaskSimpleCarOpenDoorFromOutside::ProcessPed(CPed *ped)
{
	if (m_finished)
		return true;

	if (m_veh == nil)
		return true;

	if (m_veh->GetAnimGroup().m_specialFlags.bDontOpenDoorWhenGettingIn) {
		AssocGroupId groupId;
		AnimationId animId;
		ComputeAnimID(groupId, animId);
		m_veh->ProcessOpenDoor(m_door, animId, 1.0f);
		return true;
	}

	if (m_anim == nil) {
		m_doorOpenAngleRatio = GetDoorAngleOpenRatio(m_veh, m_door);
		StartAnim(ped);
		// In the reference, a cop opening the player's car also locks
		// the player in until the door is open; this codebase's player
		// has no can-exit flag to lock.
	}

	if (m_anim == nil)
		return true;

	const float doorOpenTiming = m_veh->GetAnimGroup().GetInOutTiming(TIMING_START)[OPEN_START];
	if (m_veh->IsAutomobile() || m_anim->m_CurrentTime >= doorOpenTiming) {
		m_veh->ProcessOpenDoor(m_door, (uint32)m_anim->m_AnimId, m_anim->m_CurrentTime);
	} else {
		// Open the door visually, from the ratio it had when we started.
		eDoors door;
		switch (m_door) {
		case CAR_DOOR_RF: door = DOOR_FRONT_RIGHT; break;
		case CAR_DOOR_RR: door = DOOR_REAR_RIGHT; break;
		case CAR_DOOR_LF: door = DOOR_FRONT_LEFT; break;
		default: door = DOOR_REAR_LEFT; break;
		}
		m_veh->OpenDoor((int32)m_door, door, (1.0f - m_anim->m_CurrentTime / doorOpenTiming) * m_doorOpenAngleRatio);
	}

	return false;
}

bool
CTaskSimpleCarOpenDoorFromOutside::SetPedPosition(CPed *ped)
{
	m_lineUpTask->ProcessPed(ped, m_veh, m_anim);
	return true;
}
