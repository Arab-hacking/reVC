#include "common.h"

#include "TaskSimpleCarFallOut.h"

#include "AnimBlendAssociation.h"
#include "AnimManager.h"
#include "Automobile.h"
#include "CarEnterExit.h"
#include "DamageManager.h"
#include "Ped.h"
#include "TaskUtilityLineUpPedWithCar.h"
#include "Vehicle.h"
#include "VehicleAnimGroupData.h"

CTaskSimpleCarFallOut::CTaskSimpleCarFallOut(CVehicle *target, int32 door, CTaskUtilityLineUpPedWithCar *lineUpUtility)
	: m_HasFinished(false), m_Anim(nil), m_Vehicle(target), m_Door(door), m_LineUpUtility(lineUpUtility)
{
	if (m_Vehicle != nil)
		m_Vehicle->RegisterReference((CEntity **)&m_Vehicle);
}

CTaskSimpleCarFallOut::CTaskSimpleCarFallOut(const CTaskSimpleCarFallOut &other)
	: CTaskSimpleCarFallOut(other.m_Vehicle, other.m_Door, other.m_LineUpUtility)
{
}

CTaskSimpleCarFallOut::~CTaskSimpleCarFallOut()
{
	if (m_Vehicle != nil)
		m_Vehicle->CleanUpOldReference((CEntity **)&m_Vehicle);
	if (m_Anim != nil)
		m_Anim->SetFinishCallback(CDefaultAnimCallback::DefaultAnimCB, nil);
}

void
CTaskSimpleCarFallOut::FinishAnimFallOutCB(CAnimBlendAssociation *anim, void *data)
{
	CTaskSimpleCarFallOut *self = (CTaskSimpleCarFallOut *)data;
	self->m_HasFinished = true;
	self->m_Anim = nil;

	if (self->m_Vehicle != nil && self->m_Vehicle->IsAutomobile()) {
		eDoors door;
		switch (self->m_Door) {
		case CAR_DOOR_RF: door = DOOR_FRONT_RIGHT; break;
		case CAR_DOOR_RR: door = DOOR_REAR_RIGHT; break;
		case CAR_DOOR_LF: door = DOOR_FRONT_LEFT; break;
		default:          door = DOOR_REAR_LEFT; break;
		}
		((CAutomobile *)self->m_Vehicle)->Damage.SetDoorClosed(door);
	}
}

void
CTaskSimpleCarFallOut::ComputeAnimID(AssocGroupId &outGroup, AnimationId &outAnimId)
{
	outAnimId = (m_Door == CAR_DOOR_LF || m_Door == CAR_DOOR_LR) ? ANIM_ID_CAR_FALLOUT_LHS : ANIM_ID_CAR_FALLOUT_RHS;
	outGroup = m_Vehicle->GetAnimGroup().GetGroup(outAnimId);
}

void
CTaskSimpleCarFallOut::StartAnim(CPed *ped)
{
	CCarEnterExit::RemoveCarSitAnim(ped);

	AssocGroupId animGrp;
	AnimationId animId;
	ComputeAnimID(animGrp, animId);
	m_Anim = CAnimManager::BlendAnimation(ped->GetClump(), animGrp, animId, 1000.0f);
	if (m_Anim != nil)
		m_Anim->SetFinishCallback(FinishAnimFallOutCB, this);
}

void
CTaskSimpleCarFallOut::DoOpenCarDoor(CPed *ped)
{
	AssocGroupId animGrp;
	AnimationId animId;
	ComputeAnimID(animGrp, animId);
	m_Vehicle->ProcessOpenDoor((uint32)m_Door, (uint32)animId, 1.0f);
}

bool
CTaskSimpleCarFallOut::MakeAbortable(CPed *ped, eAbortPriority priority, const CEvent *event)
{
	if (priority != ABORT_PRIORITY_IMMEDIATE)
		return false;
	if (m_Anim != nil) {
		m_Anim->m_BlendDelta = -1000.0f;
		m_Anim = nil;
	}
	if (m_Vehicle != nil)
		DoOpenCarDoor(ped);
	return true;
}

bool
CTaskSimpleCarFallOut::ProcessPed(CPed *ped)
{
	if (m_Vehicle == nil || m_HasFinished)
		return true;
	if (m_Anim == nil) {
		StartAnim(ped);
		return false;
	}
	if (ped->bInVehicle)
		DoOpenCarDoor(ped);
	return !ped->bInVehicle;
}

bool
CTaskSimpleCarFallOut::SetPedPosition(CPed *ped)
{
	if (m_Vehicle != nil) {
		if (m_HasFinished || (m_Anim != nil && m_Anim->GetBlendAmount() != 0.0f))
			m_LineUpUtility->ProcessPed(ped, m_Vehicle, m_Anim);
		else
			ped->SetPedPositionInCar();
	}
	return true;
}
