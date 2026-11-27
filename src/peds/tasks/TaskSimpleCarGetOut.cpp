#include "common.h"

#include "TaskSimpleCarGetOut.h"

#include "AnimBlendAssociation.h"
#include "AnimManager.h"
#include "CarEnterExit.h"
#include "Ped.h"
#include "TaskUtilityLineUpPedWithCar.h"
#include "UpsideDownCarCheck.h"
#include "Vehicle.h"
#include "VehicleAnimGroupData.h"

CTaskSimpleCarGetOut::CTaskSimpleCarGetOut(CVehicle *veh, uint32 door, CTaskUtilityLineUpPedWithCar *taskLineUp)
	: m_finished(false), m_vehHasDoorToOpen(false), m_anim(nil), m_isUpsideDown(false), m_veh(veh), m_door(door), m_taskLineUp(taskLineUp)
{
	if (m_veh != nil)
		m_veh->RegisterReference((CEntity **)&m_veh);
}

CTaskSimpleCarGetOut::CTaskSimpleCarGetOut(const CTaskSimpleCarGetOut &other)
	: CTaskSimpleCarGetOut(other.m_veh, other.m_door, other.m_taskLineUp)
{
}

CTaskSimpleCarGetOut::~CTaskSimpleCarGetOut()
{
	if (m_veh != nil)
		m_veh->CleanUpOldReference((CEntity **)&m_veh);
	if (m_anim != nil)
		m_anim->SetFinishCallback(CDefaultAnimCallback::DefaultAnimCB, nil);
}

void
CTaskSimpleCarGetOut::FinishAnimCarGetOutCB(CAnimBlendAssociation *anim, void *data)
{
	CTaskSimpleCarGetOut *self = (CTaskSimpleCarGetOut *)data;
	self->m_finished = true;
	self->m_anim = nil;
}

void
CTaskSimpleCarGetOut::ComputeAnimID(AssocGroupId &outGroup, AnimationId &outAnimId)
{
	// The check tracks tracked cars for a while; a car climbing out of
	// counts as upside down straight away, which is what the world
	// matrix says directly.
	if (m_veh->IsAutomobile() && m_veh->GetMatrix().GetUp().z < -0.9f)
		m_isUpsideDown = true;

	if (m_isUpsideDown) {
		const CVector right = m_veh->GetMatrix().GetRight();
		if (m_door == CAR_DOOR_LF || m_door == CAR_DOOR_LR)
			m_isUpsideDown = right.z >= -0.5f;
		else
			m_isUpsideDown = right.z <= 0.5f;
	}

	if (m_isUpsideDown) {
		outGroup = ANIM_GROUP_DEFAULT;
		switch (m_door) {
		case CAR_DOOR_RF:
		case CAR_DOOR_RR:
			outAnimId = ANIM_ID_DEFAULT_CAR_CRAWLOUTRHS_1;
			break;
		default:
			outAnimId = ANIM_ID_DEFAULT_CAR_CRAWLOUTRHS_0;
			break;
		}
		return;
	}

	switch (m_door) {
	case CAR_DOOR_RF: outAnimId = ANIM_ID_CAR_GETOUT_RHS_0; break;
	case CAR_DOOR_RR: outAnimId = ANIM_ID_CAR_GETOUT_RHS_1; break;
	case CAR_DOOR_LF: outAnimId = ANIM_ID_CAR_GETOUT_LHS_0; break;
	default:          outAnimId = ANIM_ID_CAR_GETOUT_LHS_1; break;
	}
	outGroup = m_veh->GetAnimGroup().GetGroup(outAnimId);
}

void
CTaskSimpleCarGetOut::StartAnim(const CPed *ped)
{
	CCarEnterExit::RemoveCarSitAnim(ped);

	AssocGroupId groupId;
	AnimationId animId;
	ComputeAnimID(groupId, animId);
	m_anim = CAnimManager::BlendAnimation(ped->GetClump(), groupId, animId, 1000.0f);
	if (m_anim != nil)
		m_anim->SetFinishCallback(FinishAnimCarGetOutCB, this);
}

bool
CTaskSimpleCarGetOut::MakeAbortable(CPed *ped, eAbortPriority priority, const CEvent *event)
{
	if (priority == ABORT_PRIORITY_IMMEDIATE) {
		if (m_anim != nil)
			m_anim->m_BlendDelta = -1000.0f;
		return true;
	}
	return false;
}

bool
CTaskSimpleCarGetOut::ProcessPed(CPed *ped)
{
	if (m_veh == nil)
		return false;

	if (m_finished)
		return true;

	if (m_anim != nil) {
		if (m_anim->m_BlendAmount == 1.0f)
			ped->bRenderPedInCar = true;

		if (m_vehHasDoorToOpen && !m_isUpsideDown) {
			AssocGroupId groupId;
			AnimationId animId;
			ComputeAnimID(groupId, animId);
			m_veh->ProcessOpenDoor(m_door, (uint32)m_anim->m_AnimId, m_anim->m_CurrentTime);
		}
	} else {
		StartAnim(ped);
		ped->SetPedPositionInCar();
		ped->ReplaceWeaponWhenExitingVehicle();
		m_vehHasDoorToOpen = CCarEnterExit::CarHasDoorToOpen(m_veh, m_door);
	}

	return false;
}

bool
CTaskSimpleCarGetOut::SetPedPosition(CPed *ped)
{
	if (m_anim == nil || m_anim->m_BlendAmount == 0.0f)
		ped->SetPedPositionInCar();
	else
		m_taskLineUp->ProcessPed(ped, m_veh, m_anim);
	return true;
}
