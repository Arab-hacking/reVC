#include "common.h"

#include "TaskSimpleCarOpenLockedDoorFromOutside.h"

#include "AnimBlendAssociation.h"
#include "AnimManager.h"
#include "Ped.h"
#include "TaskUtilityLineUpPedWithCar.h"
#include "Vehicle.h"
#include "VehicleAnimGroupData.h"

CTaskSimpleCarOpenLockedDoorFromOutside::CTaskSimpleCarOpenLockedDoorFromOutside(CVehicle *veh, uint32 door, CTaskUtilityLineUpPedWithCar *lineUpUtil)
	: m_veh(veh), m_door(door), m_lineUpUtil(lineUpUtil), m_anim(nil), m_hasAnimFinished(false)
{
	if (m_veh != nil)
		m_veh->RegisterReference((CEntity **)&m_veh);
}

CTaskSimpleCarOpenLockedDoorFromOutside::CTaskSimpleCarOpenLockedDoorFromOutside(const CTaskSimpleCarOpenLockedDoorFromOutside &other)
	: CTaskSimpleCarOpenLockedDoorFromOutside(other.m_veh, other.m_door, other.m_lineUpUtil)
{
}

CTaskSimpleCarOpenLockedDoorFromOutside::~CTaskSimpleCarOpenLockedDoorFromOutside()
{
	if (m_veh != nil)
		m_veh->CleanUpOldReference((CEntity **)&m_veh);
	if (m_anim != nil)
		m_anim->SetFinishCallback(CDefaultAnimCallback::DefaultAnimCB, nil);
}

void
CTaskSimpleCarOpenLockedDoorFromOutside::FinishAnimCarOpenLockedDoorFromOutsideCB(CAnimBlendAssociation *anim, void *data)
{
	CTaskSimpleCarOpenLockedDoorFromOutside *self = (CTaskSimpleCarOpenLockedDoorFromOutside *)data;
	self->m_anim = nil;
	self->m_hasAnimFinished = true;
}

void
CTaskSimpleCarOpenLockedDoorFromOutside::StartAnim(CPed *ped)
{
	const AnimationId animId = (m_door == CAR_DOOR_LF) ? ANIM_ID_CAR_DOORLOCKED_LHS : ANIM_ID_CAR_DOORLOCKED_RHS;
	m_anim = CAnimManager::BlendAnimation(ped->GetClump(), m_veh->GetAnimGroup().GetGroup(animId), animId, 4.0f);
	if (m_anim != nil)
		m_anim->SetFinishCallback(FinishAnimCarOpenLockedDoorFromOutsideCB, this);
	else
		// No clip for this vehicle/group: waiting for it hangs entry.
		m_hasAnimFinished = true;
}

bool
CTaskSimpleCarOpenLockedDoorFromOutside::MakeAbortable(CPed *ped, eAbortPriority priority, const CEvent *event)
{
	if (priority != ABORT_PRIORITY_IMMEDIATE)
		return false;
	if (m_anim != nil)
		m_anim->m_BlendDelta = -1000.0f;
	return true;
}

bool
CTaskSimpleCarOpenLockedDoorFromOutside::ProcessPed(CPed *ped)
{
	if (m_hasAnimFinished)
		return true;
	if (m_anim == nil)
		StartAnim(ped);
	return false;
}

bool
CTaskSimpleCarOpenLockedDoorFromOutside::SetPedPosition(CPed *ped)
{
	m_lineUpUtil->ProcessPed(ped, m_veh, m_anim);
	return true;
}
