#include "common.h"

#include "TaskSimpleCarCloseDoorFromInside.h"

#include "AnimBlendAssociation.h"
#include "AnimManager.h"
#include "Ped.h"
#include "TaskUtilityLineUpPedWithCar.h"
#include "Vehicle.h"
#include "VehicleAnimGroupData.h"

CTaskSimpleCarCloseDoorFromInside::CTaskSimpleCarCloseDoorFromInside(CVehicle *veh, uint32 door, CTaskUtilityLineUpPedWithCar *lineUpTask)
	: m_veh(veh), m_door(door), m_lineUpTask(lineUpTask), m_anim(nil), m_animHasFinished(false)
{
	if (m_veh != nil)
		m_veh->RegisterReference((CEntity **)&m_veh);
}

CTaskSimpleCarCloseDoorFromInside::CTaskSimpleCarCloseDoorFromInside(const CTaskSimpleCarCloseDoorFromInside &other)
	: CTaskSimpleCarCloseDoorFromInside(other.m_veh, other.m_door, other.m_lineUpTask)
{
}

CTaskSimpleCarCloseDoorFromInside::~CTaskSimpleCarCloseDoorFromInside()
{
	if (m_veh != nil)
		m_veh->CleanUpOldReference((CEntity **)&m_veh);
	if (m_anim != nil)
		m_anim->SetDeleteCallback(CDefaultAnimCallback::DefaultAnimCB, nil);
}

void
CTaskSimpleCarCloseDoorFromInside::FinishAnimCarCloseDoorFromInsideCB(CAnimBlendAssociation *anim, void *data)
{
	CTaskSimpleCarCloseDoorFromInside *self = (CTaskSimpleCarCloseDoorFromInside *)data;

	self->m_animHasFinished = true;

	if (self->m_veh != nil)
		self->ProcessDoorOpen(nil);

	if (self->m_anim != nil)
		self->m_anim->m_BlendDelta = -1000.0f;
	self->m_anim = nil;
}

void
CTaskSimpleCarCloseDoorFromInside::ComputeAnimID(AssocGroupId &outGroup, AnimationId &outAnimId)
{
	switch (m_door) {
	case CAR_DOOR_RF: outAnimId = ANIM_ID_CAR_CLOSEDOOR_RHS_0; break;
	case CAR_DOOR_RR: outAnimId = ANIM_ID_CAR_CLOSEDOOR_RHS_1; break;
	case CAR_DOOR_LF: outAnimId = ANIM_ID_CAR_CLOSEDOOR_LHS_0; break;
	default:          outAnimId = ANIM_ID_CAR_CLOSEDOOR_LHS_1; break;
	}
	outGroup = m_veh->GetAnimGroup().GetGroup(outAnimId);
}

void
CTaskSimpleCarCloseDoorFromInside::StartAnim(CPed *ped)
{
	AssocGroupId grpId;
	AnimationId animId;
	ComputeAnimID(grpId, animId);
	m_anim = CAnimManager::BlendAnimation(ped->GetClump(), grpId, animId, 1000.0f);
	if (m_anim != nil)
		m_anim->SetFinishCallback(FinishAnimCarCloseDoorFromInsideCB, this);
}

void
CTaskSimpleCarCloseDoorFromInside::ProcessDoorOpen(CPed *ped)
{
	AssocGroupId grpId;
	AnimationId animId;
	ComputeAnimID(grpId, animId);
	m_veh->ProcessOpenDoor(m_door, animId, 1.0f);
}

bool
CTaskSimpleCarCloseDoorFromInside::MakeAbortable(CPed *ped, eAbortPriority priority, const CEvent *event)
{
	if (priority == ABORT_PRIORITY_IMMEDIATE) {
		if (m_anim != nil)
			m_anim->m_BlendDelta = -1000.0f;
		if (m_veh != nil)
			ProcessDoorOpen(ped);
		return true;
	}
	return false;
}

bool
CTaskSimpleCarCloseDoorFromInside::ProcessPed(CPed *ped)
{
	if (m_animHasFinished)
		return true;

	if (m_veh == nil)
		return true;

	if (m_veh->GetAnimGroup().m_specialFlags.bDontCloseDoorAfterGettingIn) {
		ProcessDoorOpen(ped);
		return true;
	}

	if (m_anim == nil) {
		if (ped->m_nPedState == PED_ARRESTED)
			return true;
		StartAnim(ped);
	}

	if (m_anim != nil)
		m_veh->ProcessOpenDoor(m_door, (uint32)m_anim->m_AnimId, m_anim->m_CurrentTime);

	return false;
}

bool
CTaskSimpleCarCloseDoorFromInside::SetPedPosition(CPed *ped)
{
	m_lineUpTask->ProcessPed(ped, m_veh, m_anim);
	return true;
}
