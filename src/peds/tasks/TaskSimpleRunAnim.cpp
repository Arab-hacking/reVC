#include "common.h"

#include "TaskSimpleRunAnim.h"
#include "AnimManager.h"
#include "Ped.h"
#include "RpAnimBlend.h"

// SA CTaskSimpleRunAnim::CTaskSimpleRunAnim (0x61A7A0).
CTaskSimpleRunAnim::CTaskSimpleRunAnim(AssocGroupId animGroup, AnimationId animId, float fBlendDelta, int32 nTaskType, const char *taskName, bool bHoldLastFrame)
	: CTaskSimpleAnim(bHoldLastFrame)
{
	(void)taskName; // SA only uses it for debug naming
	m_nAnimGroup  = animGroup;
	m_nAnimId     = animId;
	m_fBlendDelta = fBlendDelta;
	m_nTaskType   = (uint16)nTaskType;
}

// SA CTaskSimpleRunAnim::CTaskSimpleRunAnim (0x61A810).
CTaskSimpleRunAnim::CTaskSimpleRunAnim(AssocGroupId animGroup, AnimationId animId, float fBlendDelta, bool bHoldLastFrame)
	: CTaskSimpleAnim(bHoldLastFrame)
{
	m_nAnimGroup  = animGroup;
	m_nAnimId     = animId;
	m_fBlendDelta = fBlendDelta;
	m_nTaskType   = TASK_SIMPLE_ANIM;
}

// SA CTaskSimpleRunAnim::Clone (0x61E700). As in SA, the clone comes back
// as a plain TASK_SIMPLE_ANIM; derived tasks override Clone to keep their
// own type.
CTask*
CTaskSimpleRunAnim::Clone() const
{
	return new CTaskSimpleRunAnim(m_nAnimGroup, m_nAnimId, m_fBlendDelta, m_bHoldLastFrame);
}

// SA CTaskSimpleRunAnim::ProcessPed (0x61A8B0).
bool
CTaskSimpleRunAnim::ProcessPed(CPed *ped)
{
	if(m_bIsFinished)
		return true;

	if(m_pAnim == nil && RpAnimBlendClumpGetAssociation(ped->GetClump(), (uint32)m_nAnimId) == nil)
		StartAnim(ped);

	return m_bIsFinished;
}

// SA CTaskSimpleRunAnim::StartAnim (0x61A920).
void
CTaskSimpleRunAnim::StartAnim(CPed *ped)
{
	m_pAnim = CAnimManager::BlendAnimation(ped->GetClump(), m_nAnimGroup, m_nAnimId, m_fBlendDelta);
	m_pAnim->SetFinishCallback(CTaskSimpleAnim::FinishRunAnimCB, this);
}
