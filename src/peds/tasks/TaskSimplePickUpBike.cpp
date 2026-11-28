#include "common.h"

#include "TaskSimplePickUpBike.h"

#include "AnimBlendAssociation.h"
#include "AnimManager.h"
#include "Bike.h"
#include "Ped.h"
#include "TaskUtilityLineUpPedWithCar.h"
#include "Vehicle.h"
#include "VehicleAnimGroupData.h"

CTaskSimplePickUpBike::CTaskSimplePickUpBike(CVehicle *veh, int32 targetDoor, CTaskUtilityLineUpPedWithCar *lineUpUtil)
	: m_targetDoor(targetDoor), m_veh(veh), m_lineUpUtil(lineUpUtil), m_anim(nil), m_hasAnimFinished(false)
{
	if (m_veh != nil)
		m_veh->RegisterReference((CEntity **)&m_veh);
}

CTaskSimplePickUpBike::CTaskSimplePickUpBike(const CTaskSimplePickUpBike &other)
	: CTaskSimplePickUpBike(other.m_veh, other.m_targetDoor, other.m_lineUpUtil)
{
}

CTaskSimplePickUpBike::~CTaskSimplePickUpBike()
{
	if (m_veh != nil)
		m_veh->CleanUpOldReference((CEntity **)&m_veh);
	if (m_anim != nil)
		m_anim->SetFinishCallback(CDefaultAnimCallback::DefaultAnimCB, nil);
}

void
CTaskSimplePickUpBike::FinishAnimPickUpBikeCB(CAnimBlendAssociation *anim, void *data)
{
	CTaskSimplePickUpBike *self = (CTaskSimplePickUpBike *)data;
	self->m_anim = nil;
	self->m_hasAnimFinished = true;
}

void
CTaskSimplePickUpBike::StartAnim(CPed *ped)
{
	const float rightZ = m_veh->GetMatrix().GetRight().z;
	AnimationId animId;
	if (m_targetDoor == CAR_DOOR_LF || m_targetDoor == CAR_DOOR_LR)
		animId = (rightZ <= 0.0f) ? ANIM_ID_CAR_ALIGNHI_LHS : ANIM_ID_CAR_ALIGN_LHS;
	else
		animId = (rightZ >= 0.0f) ? ANIM_ID_CAR_ALIGNHI_RHS : ANIM_ID_CAR_ALIGN_RHS;

	m_anim = CAnimManager::BlendAnimation(ped->GetClump(), m_veh->GetAnimGroup().GetGroup(animId), animId);
	if (m_anim != nil)
		m_anim->SetFinishCallback(FinishAnimPickUpBikeCB, this);
}

bool
CTaskSimplePickUpBike::MakeAbortable(CPed *ped, eAbortPriority priority, const CEvent *event)
{
	if (priority != ABORT_PRIORITY_IMMEDIATE)
		return false;
	if (m_anim != nil)
		m_anim->m_BlendDelta = -1000.0f;
	if (m_veh != nil && m_veh->IsBike())
		((CBike *)m_veh)->bIsBeingPickedUp = false;
	return true;
}

bool
CTaskSimplePickUpBike::ProcessPed(CPed *ped)
{
	if (m_hasAnimFinished)
		return true;

	if (m_anim == nil)
		StartAnim(ped);

	if (m_veh != nil && m_veh->IsBike()) {
		CBike *bike = (CBike *)m_veh;
		if (!bike->bIsBeingPickedUp && m_anim != nil) {
			// Half-way through the heave the bike physics takes over.
			if (m_anim->m_CurrentTime > 0.47f)
				bike->bIsBeingPickedUp = true;
		}
	}

	return false;
}

bool
CTaskSimplePickUpBike::SetPedPosition(CPed *ped)
{
	m_lineUpUtil->ProcessPed(ped, m_veh, m_anim);
	return true;
}
