#include "common.h"

#include "TaskSimpleCarAlign.h"

#include "AnimBlendAssociation.h"
#include "AnimManager.h"
#include "CarEnterExit.h"
#include "General.h"
#include "Ped.h"
#include "TaskUtilityLineUpPedWithCar.h"
#include "Vehicle.h"
#include "VehicleAnimGroupData.h"

CTaskSimpleCarAlign::CTaskSimpleCarAlign(CVehicle *veh, const CVector &doorPos, uint32 targetDoor, CTaskUtilityLineUpPedWithCar *taskLineUp)
	: m_animHasFinished(false), m_anim(nil), m_veh(veh), m_doorPos(doorPos), m_door(targetDoor), m_lineUpTask(taskLineUp)
{
	if (m_veh != nil)
		m_veh->RegisterReference((CEntity **)&m_veh);
}

CTaskSimpleCarAlign::CTaskSimpleCarAlign(const CTaskSimpleCarAlign &other)
	: CTaskSimpleCarAlign(other.m_veh, other.m_doorPos, other.m_door, other.m_lineUpTask)
{
}

CTaskSimpleCarAlign::~CTaskSimpleCarAlign()
{
	if (m_veh != nil)
		m_veh->CleanUpOldReference((CEntity **)&m_veh);
	if (m_anim != nil)
		m_anim->SetFinishCallback(CDefaultAnimCallback::DefaultAnimCB, nil);
}

void
CTaskSimpleCarAlign::FinishAnimCarAlignCB(CAnimBlendAssociation *anim, void *data)
{
	CTaskSimpleCarAlign *self = (CTaskSimpleCarAlign *)data;
	self->m_anim = nil;
	self->m_animHasFinished = true;
}

// If the ped is facing almost exactly along the side of the car, nudge
// his heading so the align animation does not have to spin him round.
void
CTaskSimpleCarAlign::FixHeading(CPed *ped)
{
	if (m_veh == nil || !m_veh->IsAutomobile())
		return;

	CVector carLeftOrRight;
	if (m_door == CAR_DOOR_LF || m_door == CAR_DOOR_LR)
		carLeftOrRight = m_veh->GetMatrix().GetRight() * -1.0f;
	else
		carLeftOrRight = m_veh->GetMatrix().GetRight();

	const CVector pedFwd = ped->GetMatrix().GetForward();
	const float dot = DotProduct(carLeftOrRight, pedFwd);
	if (dot <= 0.0f || dot >= 0.1f)
		return;

	const CVector straightened = pedFwd - carLeftOrRight * dot;
	const float angle = CGeneral::LimitRadianAngle(
		CGeneral::GetRadianAngleBetweenPoints(straightened.x, straightened.y, 0.0f, 0.0f));
	ped->m_fRotationDest = angle;
	ped->m_fRotationCur = angle;
}

void
CTaskSimpleCarAlign::StartAnim(CPed *ped)
{
	const CVector doorOpenPos = CCarEnterExit::GetPositionToOpenCarDoor(m_veh, (int32)m_door);
	const float z = doorOpenPos.z - ped->GetPosition().z;
	AnimationId animId;
	if (m_door == CAR_DOOR_LF || m_door == CAR_DOOR_LR)
		animId = (z <= 4.4f) ? ANIM_ID_CAR_ALIGN_LHS : ANIM_ID_CAR_ALIGNHI_LHS;
	else
		animId = (z <= 4.4f) ? ANIM_ID_CAR_ALIGN_RHS : ANIM_ID_CAR_ALIGNHI_RHS;

	m_anim = CAnimManager::BlendAnimation(ped->GetClump(), m_veh->GetAnimGroup().GetGroup(animId), animId, 4.0f);
	if (m_anim != nil)
		m_anim->SetFinishCallback(FinishAnimCarAlignCB, this);
}

bool
CTaskSimpleCarAlign::MakeAbortable(CPed *ped, eAbortPriority priority, const CEvent *event)
{
	if (priority == ABORT_PRIORITY_IMMEDIATE) {
		if (m_anim != nil)
			m_anim->m_BlendDelta = -1000.0f;
		return true;
	}
	return false;
}

bool
CTaskSimpleCarAlign::ProcessPed(CPed *ped)
{
	if (m_animHasFinished || m_veh == nil)
		return true;
	if (m_anim == nil) {
		FixHeading(ped);
		StartAnim(ped);
	}
	return false;
}

bool
CTaskSimpleCarAlign::SetPedPosition(CPed *ped)
{
	m_lineUpTask->ProcessPed(ped, m_veh, m_anim);
	return true;
}
