#include "common.h"

#include "TaskSimpleDie.h"

#include "AnimBlendAssociation.h"
#include "AnimBlendHierarchy.h"
#include "AnimManager.h"
#include "Event.h"
#include "EventDeath.h"
#include "Ped.h"
#include "PedIK.h"
#include "PedIntelligence.h"
#include "Timer.h"

CTaskSimpleDie::CTaskSimpleDie(AssocGroupId animGroupId, AnimationId animId, float blendDelta, float animSpeed)
	: m_animGroupId(animGroupId), m_animId(animId), m_animHierarchy(nil), m_animFlags((eAnimationFlags)0),
	  m_blendDelta(blendDelta), m_animSpeed(animSpeed), m_nFlags(0), m_animAssociation(nil)
{
}

CTaskSimpleDie::CTaskSimpleDie(const char *animName, const char *animBlock, eAnimationFlags animFlags, float blendDelta, float animSpeed)
	: m_animId(ANIM_ID_KO_SHOT_FRONT_0), m_animFlags(animFlags), m_blendDelta(blendDelta), m_animSpeed(animSpeed),
	  m_nFlags(0), m_animAssociation(nil)
{
	m_animGroupId = ANIM_GROUP_DEFAULT;
	m_animHierarchy = CAnimManager::GetAnimation(animName, CAnimManager::GetAnimationBlock(animBlock));
}

CTaskSimpleDie::CTaskSimpleDie(CAnimBlendHierarchy *animHierarchy, eAnimationFlags animFlags, float blendDelta, float animSpeed)
	: m_animGroupId(ANIM_GROUP_DEFAULT), m_animId(ANIM_ID_KO_SHOT_FRONT_0), m_animHierarchy(animHierarchy),
	  m_animFlags(animFlags), m_blendDelta(blendDelta), m_animSpeed(animSpeed), m_nFlags(0), m_animAssociation(nil)
{
}

CTaskSimpleDie::CTaskSimpleDie(const CTaskSimpleDie &other)
	: CTaskSimpleDie(other.m_animGroupId, other.m_animId, other.m_blendDelta, other.m_animSpeed)
{
	m_animHierarchy = other.m_animHierarchy;
	m_animFlags = other.m_animFlags;
}

CTaskSimpleDie::~CTaskSimpleDie()
{
	if (m_animAssociation != nil)
		m_animAssociation->SetFinishCallback(CDefaultAnimCallback::DefaultAnimCB, nil);
}

void
CTaskSimpleDie::FinishAnimDieCB(CAnimBlendAssociation *association, void *data)
{
	CTaskSimpleDie *task = (CTaskSimpleDie *)data;
	task->m_dieAnimFinished = true;
	task->m_animAssociation = nil;
}

void
CTaskSimpleDie::StartAnim(CPed *ped)
{
	if (m_animHierarchy != nil)
		m_animAssociation = CAnimManager::BlendAnimation(ped->GetClump(), m_animHierarchy, (int32)m_animFlags, m_blendDelta);
	else
		m_animAssociation = CAnimManager::BlendAnimation(ped->GetClump(), m_animGroupId, m_animId, m_blendDelta);

	if (m_animAssociation == nil)
		return;
	m_animAssociation->SetFinishCallback(FinishAnimDieCB, this);
	// Strip every flag but the ones a corpse keeps.
	m_animAssociation->m_Flags = (eAnimationFlags)(m_animAssociation->m_Flags &
		(ANIMATION_CAN_EXTRACT_X_VELOCITY | ANIMATION_CAN_EXTRACT_VELOCITY | ANIMATION_IS_SYNCRONISED |
		 ANIMATION_IS_PARTIAL | ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_LOOPED | ANIMATION_IS_PLAYING));

	if (m_animSpeed > 0.0f)
		m_animAssociation->m_Speed = m_animSpeed;

	ped->ClearAll();
	ped->m_fHealth = 0.0f;
	ped->StopNonPartialAnims();
	// The reference also stamps a death time on the ped; that field
	// does not exist here (the death event carries its own).
}

CTask *
CTaskSimpleDie::Clone() const
{
	if (m_animHierarchy != nil)
		return new CTaskSimpleDie(m_animHierarchy, m_animFlags, m_blendDelta, m_animSpeed);
	return new CTaskSimpleDie(m_animGroupId, m_animId, m_blendDelta, m_animSpeed);
}

bool
CTaskSimpleDie::MakeAbortable(CPed *ped, eAbortPriority priority, const CEvent *event)
{
	if (priority == ABORT_PRIORITY_IMMEDIATE) {
		if (m_animAssociation != nil)
			m_animAssociation->m_BlendDelta = -1000.0f;
		ped->SetPedState(PED_IDLE);
		ped->SetMoveAnim();
		return true;
	}
	if (event != nil && event->GetEventType() == EVENT_DEATH)
		return true;
	return false;
}

bool
CTaskSimpleDie::ProcessPed(CPed *ped)
{
	ped->m_pedIK.m_flags |= CPedIK::SLOPE_PITCH;

	if (m_dieAnimFinished || m_animId == ANIM_ID_NO_ANIMATION_SET) {
		if (!m_alreadyDead) {
			// Standing in/on water that will take him, or so
			// obviously held down that the death can be declared
			// now. The reference's physicalFlags tests are this
			// codebase's direct bitfields; the disable-collision-
			// force test has no equivalent and is dropped.
			if (ped->bIsInWater || !ped->bUsesCollision || ped->bIsStanding) {
				CEventDeath deathEvent(m_animId == ANIM_ID_DROWN);
				ped->GetIntelligence()->GetEventGroup().Add(&deathEvent, false);
				m_alreadyDead = true;
			}
		}
	} else if (m_animAssociation == nil) {
		StartAnim(ped);
		ped->SetPedState(PED_DIE);
	}

	return false;
}
