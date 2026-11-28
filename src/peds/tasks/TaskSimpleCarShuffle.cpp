#include "common.h"

#include "TaskSimpleCarShuffle.h"

#include "AnimBlendAssociation.h"
#include "AnimManager.h"
#include "Ped.h"
#include "TaskUtilityLineUpPedWithCar.h"
#include "Vehicle.h"
#include "VehicleAnimGroupData.h"

CTaskSimpleCarShuffle::CTaskSimpleCarShuffle(CVehicle *car, int32 targetDoor, CTaskUtilityLineUpPedWithCar *lineUpTask)
	: m_Car(car), m_TargetDoor(targetDoor), m_LineUpUtility(lineUpTask), m_Anim(nil), m_IsFinished(false)
{
}

CTaskSimpleCarShuffle::CTaskSimpleCarShuffle(const CTaskSimpleCarShuffle &other)
	: CTaskSimpleCarShuffle(other.m_Car, other.m_TargetDoor, other.m_LineUpUtility)
{
}

CTaskSimpleCarShuffle::~CTaskSimpleCarShuffle()
{
	if (m_Anim != nil)
		m_Anim->SetFinishCallback(CDefaultAnimCallback::DefaultAnimCB, nil);
}

void
CTaskSimpleCarShuffle::FinishAnimCarShuffleCB(CAnimBlendAssociation *anim, void *data)
{
	CTaskSimpleCarShuffle *self = (CTaskSimpleCarShuffle *)data;
	self->m_Anim->SetBlendDelta(-1000.0f);
	self->m_Anim = nil;
	self->m_IsFinished = true;
}

void
CTaskSimpleCarShuffle::StartAnim(CPed *target)
{
	m_Anim = CAnimManager::BlendAnimation(target->GetClump(),
		m_Car->GetAnimGroup().GetGroup(ANIM_ID_CAR_SHUFFLE_RHS_1), ANIM_ID_CAR_SHUFFLE_RHS_1, 1000.0f);
	if (m_Anim != nil)
		m_Anim->SetFinishCallback(FinishAnimCarShuffleCB, this);
	else
		// No clip for this vehicle/group: waiting for it hangs entry.
		m_IsFinished = true;
}

bool
CTaskSimpleCarShuffle::MakeAbortable(CPed *ped, eAbortPriority priority, const CEvent *event)
{
	if (priority != ABORT_PRIORITY_IMMEDIATE)
		return false;
	if (m_Anim != nil)
		m_Anim->SetBlendDelta(-1000.0f);
	return true;
}

bool
CTaskSimpleCarShuffle::ProcessPed(CPed *ped)
{
	if (m_IsFinished || m_Car == nil)
		return true;
	if (m_Anim == nil)
		StartAnim(ped);
	return false;
}

bool
CTaskSimpleCarShuffle::SetPedPosition(CPed *ped)
{
	if (!ped->bInVehicle || (m_Anim != nil && m_Anim->GetBlendAmount() >= 0.9f))
		m_LineUpUtility->ProcessPed(ped, m_Car, m_Anim);
	else
		ped->SetPedPositionInCar();
	return true;
}
