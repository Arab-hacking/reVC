#include "common.h"

#include "TaskSimpleGoToPointFine.h"

#include "AnimBlendAssociation.h"
#include "AnimManager.h"
#include "General.h"
#include "Ped.h"
#include "PedIntelligence.h"
#include "RpAnimBlend.h"
#include "TaskSimpleDuck.h"

// SA CTaskSimpleGoToPointFine (0x65EEB0). Adaptations: the IK look-at is
// dropped (see CTaskSimpleGoTo), SA's m_fAimingRotation is the ped's
// m_fRotationDest here, and where SA deletes a blend association this
// blends it out (a faded association removes itself, which is the same
// end state one frame later).

CTaskSimpleGoToPointFine::CTaskSimpleGoToPointFine(float moveRatio, CVector targetPoint, float fRadius, CEntity *entity)
: CTaskSimpleGoTo(PEDMOVE_WALK, targetPoint, fRadius)
{
	(void)entity;
	SetMoveRatio(moveRatio);
}

// 0x662040
CTask*
CTaskSimpleGoToPointFine::Clone() const
{
	return new CTaskSimpleGoToPointFine(m_fMoveRatio, m_vecTargetPoint, m_fRadius, nil);
}

// 0x663500
bool
CTaskSimpleGoToPointFine::MakeAbortable(CPed *ped, eAbortPriority priority, const CEvent *event)
{
	(void)priority; (void)event;
	QuitIK(ped);
	Finish(ped);
	return true;
}

// 0x65EF00
void
CTaskSimpleGoToPointFine::Finish(CPed *ped)
{
	ped->SetMoveState(PEDMOVE_STILL);
	ped->SetMoveAnim();
	ped->SetMoveState(m_moveState);
	ped->SetMoveAnim();
}

// 0x663540
bool
CTaskSimpleGoToPointFine::ProcessPed(CPed *ped)
{
	CVector dist = m_vecTargetPoint - ped->GetPosition();
	CVector2D dist2d(dist.x, dist.y);
	if(m_fRadius * m_fRadius >= dist2d.MagnitudeSqr() || HasCircledTarget(ped)){
		QuitIK(ped);
		Finish(ped);
		return true;
	}
	SetBlendedMoveAnim(ped);
	float fAngleInRadians = CGeneral::GetRadianAngleBetweenPoints(dist.x, dist.y, 0.0f, 0.0f);
	ped->m_fRotationDest = CGeneral::LimitRadianAngle(fAngleInRadians);
	SetUpIK(ped);
	return false;
}

// 0x65EF80
void
CTaskSimpleGoToPointFine::SetBlendedMoveAnim(CPed *ped)
{
	CAnimBlendAssociation *idleAnimAssoc = RpAnimBlendClumpGetAssociation(ped->GetClump(), ANIM_ID_IDLE);
	CAnimBlendAssociation *walkAnimAssoc = RpAnimBlendClumpGetAssociation(ped->GetClump(), ANIM_ID_WALK);
	CAnimBlendAssociation *runAnimAssoc = RpAnimBlendClumpGetAssociation(ped->GetClump(), ANIM_ID_RUN);
	CAnimBlendAssociation *sprintAnimAssoc = RpAnimBlendClumpGetAssociation(ped->GetClump(), ANIM_ID_SPRINT);
	CAnimBlendAssociation *pIdleTiredAnimAssoc = RpAnimBlendClumpGetAssociation(ped->GetClump(), ANIM_ID_IDLE_TIRED);

	if(ped->bIsDucking && ped->GetIntelligence()->GetTaskDuck(false)){
		float fMoveSpeedY = Min(m_fMoveRatio * 0.5f, 1.0f);
		CTaskSimpleDuck *duckTask = ped->GetIntelligence()->GetTaskDuck(false);
		duckTask->ControlDuckMove(CVector2D(0.0f, -fMoveSpeedY));
		return;
	}

	if(ped->bResetWalkAnims){
		if(walkAnimAssoc)
			walkAnimAssoc->SetCurrentTime(0.0f);
		if(runAnimAssoc)
			runAnimAssoc->SetCurrentTime(0.0f);
		if(sprintAnimAssoc)
			sprintAnimAssoc->SetCurrentTime(0.0f);
		ped->bResetWalkAnims = false;
	}

	if(m_fMoveRatio == 0.0f){
		if(!idleAnimAssoc)
			CAnimManager::BlendAnimation(ped->GetClump(), ped->m_animGroup, ANIM_ID_IDLE, 4.0f);

		if(walkAnimAssoc)
			walkAnimAssoc->m_BlendDelta = -8.0f;
		if(runAnimAssoc)
			runAnimAssoc->m_BlendDelta = -8.0f;
		if(sprintAnimAssoc)
			sprintAnimAssoc->m_BlendDelta = -8.0f;
		ped->m_nMoveState = PEDMOVE_STILL;
		m_moveState = PEDMOVE_STILL;
		return;
	}

	if(idleAnimAssoc){
		if(walkAnimAssoc)
			walkAnimAssoc->SetCurrentTime(0.0f);
		if(runAnimAssoc)
			runAnimAssoc->SetCurrentTime(0.0f);
		idleAnimAssoc->m_BlendDelta = -8.0f;
	}

	if(pIdleTiredAnimAssoc)
		pIdleTiredAnimAssoc->m_BlendDelta = -4.0f;

	eMoveState moveState = PEDMOVE_WALK;
	if(m_fMoveRatio > 1.0f){
		if(m_fMoveRatio > 2.0f){
			if(m_fMoveRatio > 3.0f)
				return;

			if(walkAnimAssoc)
				walkAnimAssoc->m_BlendDelta = -8.0f;
			if(!runAnimAssoc){
				runAnimAssoc = CAnimManager::AddAnimation(ped->GetClump(), ped->m_animGroup, ANIM_ID_RUN);
				runAnimAssoc->m_BlendAmount = 0.0f;
				runAnimAssoc->m_Speed = 1.0f;
			}
			runAnimAssoc->m_Flags |= ANIMATION_IS_PLAYING;
			runAnimAssoc->m_BlendDelta = 0.0f;
			runAnimAssoc->m_BlendAmount = 3.0f - m_fMoveRatio;
			if(!sprintAnimAssoc){
				sprintAnimAssoc = CAnimManager::AddAnimation(ped->GetClump(), ped->m_animGroup, ANIM_ID_SPRINT);
				sprintAnimAssoc->m_BlendAmount = 0.0f;
				sprintAnimAssoc->m_Speed = 1.0f;
			}
			sprintAnimAssoc->m_Flags |= ANIMATION_IS_PLAYING;
			sprintAnimAssoc->m_BlendDelta = 0.0f;
			moveState = PEDMOVE_SPRINT;
			sprintAnimAssoc->m_BlendAmount = m_fMoveRatio - 2.0f;
		}else{
			if(!walkAnimAssoc){
				walkAnimAssoc = CAnimManager::AddAnimation(ped->GetClump(), ped->m_animGroup, ANIM_ID_WALK);
				walkAnimAssoc->m_BlendAmount = 0.0f;
				walkAnimAssoc->m_Speed = 1.0f;
			}
			walkAnimAssoc->m_Flags |= ANIMATION_IS_PLAYING;
			walkAnimAssoc->m_BlendDelta = 0.0f;
			walkAnimAssoc->m_BlendAmount = 2.0f - m_fMoveRatio;
			if(!runAnimAssoc){
				runAnimAssoc = CAnimManager::AddAnimation(ped->GetClump(), ped->m_animGroup, ANIM_ID_RUN);
				runAnimAssoc->m_BlendAmount = 0.0f;
				runAnimAssoc->m_Speed = 1.0f;
			}
			runAnimAssoc->m_Flags |= ANIMATION_IS_PLAYING;
			runAnimAssoc->m_BlendDelta = 0.0f;
			runAnimAssoc->m_BlendAmount = m_fMoveRatio - 1.0f;
			if(sprintAnimAssoc)
				sprintAnimAssoc->m_BlendDelta = -8.0f;
			moveState = PEDMOVE_RUN;
		}
	}else{
		if(!walkAnimAssoc){
			walkAnimAssoc = CAnimManager::AddAnimation(ped->GetClump(), ped->m_animGroup, ANIM_ID_WALK);
			walkAnimAssoc->m_BlendAmount = 0.0f;
			walkAnimAssoc->m_Speed = 1.0f;
		}
		walkAnimAssoc->m_Flags |= ANIMATION_IS_PLAYING;
		walkAnimAssoc->m_BlendAmount = 1.0f;
		walkAnimAssoc->m_BlendDelta = 0.0f;
		if(runAnimAssoc)
			runAnimAssoc->m_BlendDelta = -8.0f;
		if(sprintAnimAssoc)
			sprintAnimAssoc->m_BlendDelta = -8.0f;
	}
	ped->SetMoveState(moveState);
	ped->m_nSwimmingMoveState = moveState;
	m_moveState = moveState;
}

// 0x65F330
void
CTaskSimpleGoToPointFine::SetTargetPos(CVector posn)
{
	if(m_vecTargetPoint != posn || m_fRadius != 0.5f){
		m_fRadius = 0.5f;
		m_vecTargetPoint = posn;
		m_GoToFlags &= 0xD0 | 0x20; // SA flag mask preserved as-is
	}
}

// 0x65EF30
float
CTaskSimpleGoToPointFine::BaseRatio(eMoveState moveState)
{
	switch(moveState){
	case PEDMOVE_WALK:
		return 1.0f;
	case PEDMOVE_RUN:
		return 2.0f;
	case PEDMOVE_SPRINT:
		return 3.0f;
	default:
		return 0.0f;
	}
}
