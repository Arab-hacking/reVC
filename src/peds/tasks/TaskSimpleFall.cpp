#include "common.h"

#include "TaskSimpleFall.h"

#include "AnimBlendAssociation.h"
#include "AnimBlendHierarchy.h"
#include "AnimManager.h"
#include "Event.h"
#include "Pad.h"
#include "Ped.h"
#include "PedIK.h"
#include "PlayerInfo.h"
#include "RpAnimBlend.h"
#include "Timer.h"

CTaskSimpleFall::CTaskSimpleFall(AnimationId nAnimId, AssocGroupId nAnimGroup, int32 nDownTime)
	: m_pAnim(nil), m_nAnimId(nAnimId), m_nAnimGroup(nAnimGroup), m_nTotalDownTime(nDownTime),
	  m_nCurrentDownTime(nDownTime), m_bIsFinished(false)
{
}

CTaskSimpleFall::CTaskSimpleFall(const CTaskSimpleFall &other)
	: CTaskSimpleFall(other.m_nAnimId, other.m_nAnimGroup, other.m_nTotalDownTime)
{
}

CTaskSimpleFall::~CTaskSimpleFall()
{
	if (m_pAnim != nil)
		m_pAnim->SetFinishCallback(CDefaultAnimCallback::DefaultAnimCB, nil);
}

void
CTaskSimpleFall::FinishFallAnimCB(CAnimBlendAssociation *blendAssoc, void *data)
{
	CTaskSimpleFall *task = (CTaskSimpleFall *)data;
	task->m_bIsFinished = true;
	task->m_pAnim = nil;
}

bool
CTaskSimpleFall::ProcessPed(CPed *ped)
{
	ped->m_pedIK.m_flags |= CPedIK::SLOPE_PITCH;
	ped->bFallenDown = true;

	if (m_bIsFinished && (ped->bIsStanding || ped->bIsDrowning)) {
		const uint32 timeStep = CTimer::GetTimeStepInMilliseconds();
		if (m_nCurrentDownTime <= (int32)timeStep) {
			m_nCurrentDownTime = 0;
			return true;
		}
		m_nCurrentDownTime -= (int32)timeStep;

		// The player can fight his way up early.
		if (m_nCurrentDownTime > m_nMaxPlayerDownTime && ped->IsPlayer() &&
		    ped->m_nPedState != PED_ARRESTED &&
		    CPad::GetPad(0)->DisablePlayerControls == 0) {
			m_nCurrentDownTime = m_nMaxPlayerDownTime - (int32)timeStep;
			return false;
		}
	} else {
		if (m_pAnim == nil && !m_bIsFinished)
			StartAnim(ped);
		ProcessFall(ped);
	}

	return false;
}

bool
CTaskSimpleFall::MakeAbortable(CPed *ped, eAbortPriority priority, const CEvent *event)
{
	CAnimBlendAssociation *pFallAnim = RpAnimBlendClumpGetAssociation(ped->GetClump(), ANIM_ID_FALL_FRONT);
	if (pFallAnim == nil)
		pFallAnim = RpAnimBlendClumpGetAssociation(ped->GetClump(), ANIM_ID_FALL_BACK);

	if (priority == ABORT_PRIORITY_IMMEDIATE) {
		CAnimBlendAssociation *pThisAnim = RpAnimBlendClumpGetAssociation(ped->GetClump(), m_nAnimId);
		if (pThisAnim != nil)
			pThisAnim->m_BlendDelta = -1000.0f;
		if (pFallAnim != nil)
			pFallAnim->m_BlendDelta = -1000.0f;
		m_bIsFinished = true;
		m_nCurrentDownTime = 0;
		return true;
	}

	if (priority == ABORT_PRIORITY_URGENT && event != nil) {
		const eEventType eventType = event->GetEventType();
		if ((eventType == EVENT_DAMAGE && ped->m_fHealth < 1.0f) ||
		    eventType == EVENT_IN_WATER || event->GetEventPriority() == 71 ||
		    eventType == EVENT_STUCK_IN_AIR) {
			float blendDelta = -2.0f;
			if (eventType == EVENT_IN_WATER || eventType == EVENT_STUCK_IN_AIR)
				blendDelta = -8.0f;

			if (pFallAnim != nil)
				pFallAnim->m_BlendDelta = blendDelta;
			if (m_pAnim != nil) {
				m_pAnim->m_BlendDelta = blendDelta;
				m_pAnim->SetFinishCallback(CDefaultAnimCallback::DefaultAnimCB, nil);
			}
			m_pAnim = nil;
			m_bIsFinished = true;
			m_nCurrentDownTime = 0;
			return true;
		}
	}

	return false;
}

bool
CTaskSimpleFall::StartAnim(CPed *ped)
{
	// The reference first refuses peds attached to something; this
	// codebase's peds have no such attachment.
	if (m_nAnimId == ANIM_ID_NO_ANIMATION_SET) {
		m_pAnim = RpAnimBlendClumpGetAssociation(ped->GetClump(), ANIM_ID_BIKE_FALL_OFF);
		if (m_pAnim == nil)
			m_pAnim = RpAnimBlendClumpGetAssociation(ped->GetClump(), ANIM_ID_BIKE_FALLR);
		if (m_pAnim != nil)
			m_pAnim->SetFinishCallback(FinishFallAnimCB, this);
	} else {
		m_pAnim = RpAnimBlendClumpGetAssociation(ped->GetClump(), m_nAnimId);
		if (m_pAnim != nil) {
			m_pAnim->m_CurrentTime = 0.0f;
			m_pAnim->m_BlendDelta = 8.0f;
			m_pAnim->m_BlendAmount = 0.0f;
			m_pAnim->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE;
			m_pAnim->m_Flags &= ~ANIMATION_IS_FINISH_AUTO_REMOVE;
			m_pAnim->SetFinishCallback(FinishFallAnimCB, this);
		} else {
			m_pAnim = CAnimManager::BlendAnimation(ped->GetClump(), m_nAnimGroup, m_nAnimId, 8.0f);
			if (m_pAnim == nil)
				return false;
			m_pAnim->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE;
			m_pAnim->m_Flags &= ~ANIMATION_IS_FINISH_AUTO_REMOVE;
			m_pAnim->SetFinishCallback(FinishFallAnimCB, this);
			if (m_nAnimId == ANIM_ID_BIKE_FALLR)
				m_pAnim->m_CurrentTime = 0.4f;
		}
	}
	return true;
}

void
CTaskSimpleFall::ProcessFall(CPed *ped)
{
	if (ped->IsPlayer() && (ped->bKnockedUpIntoAir || ped->bKnockedOffBike) && !ped->bIsStanding) {
		CAnimBlendAssociation *anim;
		CAnimBlendAssociation *pFirstAnim = RpAnimBlendClumpGetFirstAssociation(ped->GetClump(), ANIMATION_IS_PARTIAL);

		if (pFirstAnim != nil && (pFirstAnim->m_AnimId == ANIM_ID_FALL_BACK || pFirstAnim->m_AnimId == ANIM_ID_FALL_FRONT))
			anim = pFirstAnim;
		else
			anim = RpAnimBlendClumpGetAssociation(ped->GetClump(), ANIM_ID_FALL_BACK);
		if (anim == nil)
			anim = RpAnimBlendClumpGetAssociation(ped->GetClump(), ANIM_ID_FALL_FRONT);

		if (anim != nil) {
			// Loop the tumble while he is still airborne.
			if (anim->m_BlendAmount > 0.3f && anim->m_BlendDelta >= 0.0f &&
			    anim->m_CurrentTime > 0.667f && anim->m_CurrentTime - anim->m_TimeStep <= 0.667f)
				anim->m_CurrentTime = 0.0f;
		} else if (pFirstAnim != nil && pFirstAnim->m_CurrentTime > pFirstAnim->m_BlendHier->m_fTotalTime * 0.8f) {
			CAnimManager::BlendAnimation(ped->GetClump(), ANIM_GROUP_DEFAULT,
				(pFirstAnim->m_Flags & ANIMATION_IS_FRONT) ? ANIM_ID_FALL_FRONT : ANIM_ID_FALL_BACK, 8.0f);
		}
	} else if ((ped->bKnockedUpIntoAir || ped->bKnockedOffBike) && ped->bIsStanding && !ped->bWasStanding) {
		// He has landed: hurry the fall anim to its end.
		CAnimBlendAssociation *anim = RpAnimBlendClumpGetAssociation(ped->GetClump(), ANIM_ID_FALL_BACK);
		if (anim == nil)
			anim = RpAnimBlendClumpGetAssociation(ped->GetClump(), ANIM_ID_FALL_FRONT);
		if (anim != nil) {
			ped->bKnockedUpIntoAir = false;
			anim->m_Speed = 3.0f;
		}
	}
}
