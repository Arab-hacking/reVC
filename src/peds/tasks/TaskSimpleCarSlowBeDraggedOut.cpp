#include "common.h"

#include "TaskSimpleCarSlowBeDraggedOut.h"

#include "AnimBlendAssociation.h"
#include "AnimManager.h"
#include "CarEnterExit.h"
#include "Ped.h"
#include "RpAnimBlend.h"
#include "TaskUtilityLineUpPedWithCar.h"
#include "Vehicle.h"
#include "VehicleAnimGroupData.h"

CTaskSimpleCarSlowBeDraggedOut::CTaskSimpleCarSlowBeDraggedOut(CVehicle *target, int32 door, CTaskUtilityLineUpPedWithCar *lineUpUtility, bool isDraggedBySelf)
	: m_HasAnimFinished(false), m_Anim(nil), m_Vehicle(target), m_Door(door),
	  m_LineUpUtility(lineUpUtility), m_IsDraggedBySelf(isDraggedBySelf), m_FrontAnim(false)
{
	if (m_Vehicle != nil)
		m_Vehicle->RegisterReference((CEntity **)&m_Vehicle);
}

CTaskSimpleCarSlowBeDraggedOut::CTaskSimpleCarSlowBeDraggedOut(const CTaskSimpleCarSlowBeDraggedOut &other)
	: CTaskSimpleCarSlowBeDraggedOut(other.m_Vehicle, other.m_Door, other.m_LineUpUtility, other.m_IsDraggedBySelf)
{
}

CTaskSimpleCarSlowBeDraggedOut::~CTaskSimpleCarSlowBeDraggedOut()
{
	if (m_Vehicle != nil)
		m_Vehicle->CleanUpOldReference((CEntity **)&m_Vehicle);
	if (m_Anim != nil)
		m_Anim->SetFinishCallback(CDefaultAnimCallback::DefaultAnimCB, nil);
}

void
CTaskSimpleCarSlowBeDraggedOut::FinishAnimCarSlowBeDraggedOutCB(CAnimBlendAssociation *anim, void *data)
{
	CTaskSimpleCarSlowBeDraggedOut *self = (CTaskSimpleCarSlowBeDraggedOut *)data;
	self->m_HasAnimFinished = true;
	self->m_Anim = nil;
}

void
CTaskSimpleCarSlowBeDraggedOut::ComputeAnimID(AssocGroupId &outGroup, AnimationId &outAnimId)
{
	outAnimId = (m_Door == CAR_DOOR_RF || m_Door == CAR_DOOR_RR)
		? ANIM_ID_CAR_JACKEDRHS : ANIM_ID_CAR_JACKEDLHS;
	outGroup = m_Vehicle->GetAnimGroup().GetGroup(outAnimId);
}

void
CTaskSimpleCarSlowBeDraggedOut::DoOpenCarDoor(CPed *ped)
{
	AssocGroupId animGrp;
	AnimationId animId;
	ComputeAnimID(animGrp, animId);
	m_Vehicle->ProcessOpenDoor((uint32)m_Door, (uint32)animId, 1.0f);
}

void
CTaskSimpleCarSlowBeDraggedOut::StartAnim(CPed *ped)
{
	CCarEnterExit::RemoveCarSitAnim(ped);

	AssocGroupId animGroup;
	AnimationId animId;
	ComputeAnimID(animGroup, animId);
	m_Anim = CAnimManager::BlendAnimation(ped->GetClump(), animGroup, animId, 1000.0f);
	if (m_Anim != nil) {
		m_Anim->SetFinishCallback(FinishAnimCarSlowBeDraggedOutCB, this);
		m_FrontAnim = m_Anim->HasFlag(ANIMATION_IS_FRONT);
	} else {
		// No clip for this vehicle/group: waiting for it hangs exit.
		m_HasAnimFinished = true;
	}
}

bool
CTaskSimpleCarSlowBeDraggedOut::MakeAbortable(CPed *ped, eAbortPriority priority, const CEvent *event)
{
	if (priority != ABORT_PRIORITY_IMMEDIATE)
		return false;
	if (m_Anim != nil)
		m_Anim->m_BlendDelta = -1000.0f;
	if (m_Vehicle != nil)
		DoOpenCarDoor(ped);
	return true;
}

bool
CTaskSimpleCarSlowBeDraggedOut::ProcessPed(CPed *ped)
{
	if (m_Vehicle == nil)
		return true;

	if (m_HasAnimFinished) {
		// On the ground now: go down properly and stay there.
		if (RpAnimBlendClumpGetAssociation(ped->GetClump(), ANIM_ID_FLOOR_HIT_F) == nil &&
		    RpAnimBlendClumpGetAssociation(ped->GetClump(), ANIM_ID_FLOOR_HIT) == nil) {
			CAnimManager::BlendAnimation(ped->GetClump(), ANIM_GROUP_DEFAULT,
				m_FrontAnim ? ANIM_ID_FLOOR_HIT_F : ANIM_ID_FLOOR_HIT, 1000.0f);
			return false;
		}
		return true;
	}

	if (m_Anim == nil) {
		StartAnim(ped);
		return false;
	}

	if (!ped->bInVehicle)
		return true;

	// Past this point in the drag the line-up eases the ped away
	// from the door swing.
	if (m_Anim->m_CurrentTime > 1.7f)
		m_LineUpUtility->m_nDoorOpenPosType = 2;

	if (m_IsDraggedBySelf)
		DoOpenCarDoor(ped);

	return false;
}

bool
CTaskSimpleCarSlowBeDraggedOut::SetPedPosition(CPed *ped)
{
	if (m_Vehicle != nil) {
		if (!m_HasAnimFinished && (m_Anim == nil || m_Anim->m_BlendAmount == 0.0f)) {
			ped->SetPedPositionInCar();
		} else if (m_Anim != nil) {
			m_LineUpUtility->ProcessPed(ped, m_Vehicle, m_Anim);
		} else {
			AssocGroupId animGrp;
			AnimationId animId;
			ComputeAnimID(animGrp, animId);
			if (CAnimBlendAssociation *anim = RpAnimBlendClumpGetAssociation(ped->GetClump(), animId))
				m_LineUpUtility->ProcessPed(ped, m_Vehicle, anim);
		}
	}
	return true;
}
