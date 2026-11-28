#include "common.h"

#include "TaskSimpleRunTimedAnim.h"
#include "AnimManager.h"
#include "Ped.h"
#include "RpAnimBlend.h"

// SA CTaskSimpleRunTimedAnim::CTaskSimpleRunTimedAnim (0x61AB70).
CTaskSimpleRunTimedAnim::CTaskSimpleRunTimedAnim(AssocGroupId groupId, AnimationId animId, float blendDelta, float unused1, uint32 durationMs, bool holdLastFrame)
	: CTaskSimpleAnim(holdLastFrame)
{
	m_animGrpId  = groupId;
	m_animId     = animId;
	m_blendDelta = blendDelta;
	m_unused1    = unused1;
	m_durationMs = durationMs;
	m_taskId     = TASK_SIMPLE_ANIM;
}

// SA CTaskSimpleRunTimedAnim::CTaskSimpleRunTimedAnim (0x61ABE0).
CTaskSimpleRunTimedAnim::CTaskSimpleRunTimedAnim(AssocGroupId groupId, AnimationId animId, float blendDelta, float unused1, uint32 durationMs, eTaskType taskType, const char *taskName, bool holdLastFrame)
	: CTaskSimpleAnim(holdLastFrame)
{
	(void)taskName;
	m_animGrpId  = groupId;
	m_animId     = animId;
	m_blendDelta = blendDelta;
	m_unused1    = unused1;
	m_durationMs = durationMs;
	m_taskId     = taskType;
}

CTaskSimpleRunTimedAnim::CTaskSimpleRunTimedAnim(const CTaskSimpleRunTimedAnim &other)
	: CTaskSimpleAnim(other.m_bHoldLastFrame)
{
	m_animGrpId  = other.m_animGrpId;
	m_animId     = other.m_animId;
	m_blendDelta = other.m_blendDelta;
	m_unused1    = other.m_unused1;
	m_durationMs = other.m_durationMs;
	m_taskId     = other.m_taskId;
}

// SA CTaskSimpleRunTimedAnim::StartAnim (0x61AC70). Note SA registers a
// delete callback (not finish): if the animation is removed early, the
// task still ends.
void
CTaskSimpleRunTimedAnim::StartAnim(CPed *ped)
{
	m_pAnim = CAnimManager::BlendAnimation(ped->GetClump(), m_animGrpId, m_animId, m_blendDelta);
	m_pAnim->SetDeleteCallback(CTaskSimpleAnim::FinishRunAnimCB, this);
}

// SA CTaskSimpleRunTimedAnim::ProcessPed (0x61ACA0).
bool
CTaskSimpleRunTimedAnim::ProcessPed(CPed *ped)
{
	if(m_bIsFinished)
		return true;

	if(m_timer.IsOutOfTime())
		MakeAbortable(ped, ABORT_PRIORITY_LEISURE, nil);

	if(m_pAnim == nil && RpAnimBlendClumpGetAssociation(ped->GetClump(), (uint32)m_animId) == nil){
		m_timer.Start((int32)m_durationMs);
		StartAnim(ped);
	}

	return m_bIsFinished;
}
