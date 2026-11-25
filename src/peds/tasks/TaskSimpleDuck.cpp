#include "TaskSimpleDuck.h"

#include "AnimBlendAssociation.h"
#include "AnimManager.h"
#include "General.h"
#include "Ped.h"
#include "PedIntelligence.h"
#include "RpAnimBlend.h"
#include "TaskManager.h"
#include "Timer.h"
#include "WeaponInfo.h"

// SA CTaskSimpleDuck (0x691FC0). Adaptations for this codebase: the ped's
// task manager is reached through the intelligence, the use-gun task's
// ClearAnim calls are dropped (task not ported yet), and the
// gunshot-whizzed-by event that can cut a whizzing duck short is not
// ported either, so that branch of MakeAbortable is absent.

CTaskSimpleDuck::CTaskSimpleDuck(eDuckControlType duckControlType, uint16 lengthOfDuck, int16 shotWhizzingCounter)
: m_DuckControlType(duckControlType), m_LengthOfDuck(lengthOfDuck), m_ShotWhizzingCounter(shotWhizzingCounter)
{
	m_StartTime = CTimer::GetTimeInMilliseconds();
	m_DuckAnim = nil;
	m_MoveAnim = nil;
	m_bIsFinished = false;
	m_bIsAborting = false;
	m_bNeedToSetDuckFlag = true;
	m_bIsInControl = true;
	m_MoveCmd = CVector2D(0.0f, 0.0f);
	m_CountDownFrames = -1;
}

CTaskSimpleDuck::~CTaskSimpleDuck()
{
	if(m_DuckAnim){
		m_DuckAnim->SetDefaultFinishCallback();
		if(m_DuckAnim->m_BlendAmount > 0.0f && m_DuckAnim->m_BlendDelta >= 0.0f && (m_DuckAnim->m_Flags & ANIMATION_IS_PARTIAL))
			m_DuckAnim->m_BlendDelta = -8.0f;
	}

	if(m_MoveAnim){
		m_MoveAnim->SetDefaultFinishCallback();
		if(m_MoveAnim->m_BlendAmount > 0.0f && m_MoveAnim->m_BlendDelta >= 0.0f)
			m_MoveAnim->m_BlendDelta = -8.0f;
	}
}

// 0x692550
void
CTaskSimpleDuck::DeleteDuckAnimCB(CAnimBlendAssociation *assoc, void *data)
{
	CTaskSimpleDuck *self = (CTaskSimpleDuck*)data;

	assert(self != nil);
	assert(assoc != nil);

	switch(assoc->m_AnimId){
	case ANIM_ID_WEAPON_CROUCH:
	case ANIM_ID_DUCK_COWER:
		self->m_DuckAnim = nil;
		if(!self->m_MoveAnim || !self->m_bIsAborting)
			self->m_bIsFinished = true;
		break;
	case ANIM_ID_CROUCH_ROLL_L:
	case ANIM_ID_CROUCH_ROLL_R:
		self->m_MoveCmd.x = 0.0f;
		// fallthrough
	case ANIM_ID_GUNCROUCHFWD:
	case ANIM_ID_GUNCROUCHBWD:
		self->m_MoveAnim = nil;
		if(self->m_bIsAborting)
			self->m_bIsFinished = true;
		break;
	}
}

// 0x692610. VC has no spray-can weapon, so that SA branch is absent.
bool
CTaskSimpleDuck::CanPedDuck(CPed *ped)
{
	if(ped->IsPlayer()){
		switch(ped->m_nMoveState){
		case PEDMOVE_RUN:
		case PEDMOVE_SPRINT:
			return false;
		}
	}

	CWeapon *activeWep = ped->GetWeapon();
	const CWeaponInfo *weaponInfo = CWeaponInfo::GetWeaponInfo(activeWep->m_eWeaponType);

	// (SA also returns true for WEAPON_FIRE_USE weapons; this codebase's
	// weapon fire types have no such value.)
	switch(weaponInfo->m_eWeaponFire){
	case WEAPON_FIRE_MELEE:
		return true;
	}

	if(weaponInfo->IsFlagSet(WEAPONFLAG_CROUCHFIRE))
		return true;

	return false;
}

// 0x61C3D0
bool
CTaskSimpleDuck::IsTaskInUseByOtherTasks() const
{
	if(m_MoveCmd.x == 0.0f && m_MoveCmd.y == 0.0f){
		if(m_DuckAnim){
			if(m_DuckAnim->m_BlendAmount >= 1.0f && !m_bIsAborting && m_ShotWhizzingCounter <= 0)
				return false;
		}
	}
	return true;
}

// 0x692340
void
CTaskSimpleDuck::AbortBecauseOfOtherDuck(CPed *ped)
{
	(void)ped;

	if(m_DuckAnim){
		m_DuckAnim->SetDefaultFinishCallback();
		m_DuckAnim = nil;
	}

	if(m_MoveAnim){
		m_MoveAnim->m_BlendDelta = -8.0f;
		if(m_DuckAnim)
			m_DuckAnim->SetDefaultFinishCallback();
		m_MoveAnim = nil;
	}

	m_bNeedToSetDuckFlag = true;
	m_bIsFinished = true;
}

// 0x692390
void
CTaskSimpleDuck::RestartTask(CPed *ped)
{
	if(m_bNeedToSetDuckFlag){
		ped->bIsDucking = true;
		m_bNeedToSetDuckFlag = false;
	}

	if(m_ShotWhizzingCounter >= 0)
		m_ShotWhizzingCounter = (int16)CGeneral::GetRandomNumberInRange(1000, 2500);
}

// 0x6923F0. moveDir: y is forward/back, x is left/right.
void
CTaskSimpleDuck::ControlDuckMove(CVector2D moveDir)
{
	m_bIsInControl = true;

	// If going full forward/back ignore left/right commands
	if(Abs(m_MoveCmd.x) == 1.0f)
		return;

	// Change direction, but limit maximum change per frame
	float maxUnits = CTimer::GetTimeStep() * 0.07f;
	float moveDeltaY = moveDir.y - m_MoveCmd.y;
	if(moveDeltaY <= maxUnits){
		if(moveDeltaY >= -maxUnits)
			m_MoveCmd.y = moveDir.y;
		else
			m_MoveCmd.y -= maxUnits;
	}else{
		m_MoveCmd.y += maxUnits;
	}

	if(Abs(moveDir.y) < 0.1f && Abs(moveDir.x) >= 0.9f){
		m_MoveCmd.y = 0.0f;
		m_MoveCmd.x = moveDir.x <= 0.0f ? -1.0f : 1.0f;
	}
}

// 0x6939F0
void
CTaskSimpleDuck::SetMoveAnim(CPed *ped)
{
	if(m_MoveCmd.x == 0.0f){
		if(m_MoveCmd.y > 0.0f){ // crouch backwards
			if(!m_MoveAnim || m_MoveAnim->m_AnimId == ANIM_ID_GUNCROUCHFWD){
				if(m_MoveAnim)
					m_MoveAnim->SetDefaultFinishCallback();
				m_MoveAnim = CAnimManager::BlendAnimation(ped->GetClump(), ANIM_GROUP_DEFAULT, ANIM_ID_GUNCROUCHBWD, 8.0f);
				m_MoveAnim->SetDeleteCallback(DeleteDuckAnimCB, this);
			}
			m_MoveAnim->SetSpeed(Max(0.6f, m_MoveCmd.y));
		}else if(m_MoveCmd.y == 0.0f){ // blend the move anim out, no more movement
			if(m_MoveAnim){
				if(m_MoveAnim->m_AnimId == ANIM_ID_GUNCROUCHFWD || m_MoveAnim->m_AnimId == ANIM_ID_GUNCROUCHBWD){
					m_MoveAnim->SetFlag(ANIMATION_IS_PLAYING, false);
					m_MoveAnim->SetBlendDelta(-4.0f);
				}
			}
		}else{ // crouch forwards
			if(!m_MoveAnim || m_MoveAnim->m_AnimId == ANIM_ID_GUNCROUCHBWD){
				if(m_MoveAnim)
					m_MoveAnim->SetDefaultFinishCallback();
				m_MoveAnim = CAnimManager::BlendAnimation(ped->GetClump(), ANIM_GROUP_DEFAULT, ANIM_ID_GUNCROUCHFWD, 8.0f);
				m_MoveAnim->SetDeleteCallback(DeleteDuckAnimCB, this);
			}
			m_MoveAnim->SetSpeed(Max(0.6f, -m_MoveCmd.y));
		}
	}else{
		bool isGunCrouch = m_MoveAnim && (m_MoveAnim->m_AnimId == ANIM_ID_GUNCROUCHFWD || m_MoveAnim->m_AnimId == ANIM_ID_GUNCROUCHBWD);
		if(!m_MoveAnim || isGunCrouch){
			if(m_MoveAnim)
				m_MoveAnim->SetDefaultFinishCallback();
			m_MoveAnim = CAnimManager::BlendAnimation(ped->GetClump(), ANIM_GROUP_DEFAULT,
				m_MoveCmd.x > 0.0f ? ANIM_ID_CROUCH_ROLL_R : ANIM_ID_CROUCH_ROLL_L, 8.0f);
			m_MoveAnim->SetFinishCallback(DeleteDuckAnimCB, this);
		}
	}
}

// 0x61C420
bool
CTaskSimpleDuck::StopFireGun() const
{
	return m_MoveCmd.x != 0.0f || !m_DuckAnim || m_DuckAnim->GetBlendAmount() < 1.0f || m_bIsAborting || m_ShotWhizzingCounter > 0;
}

// 0x6924B0
void
CTaskSimpleDuck::ForceStopMove()
{
	m_bIsInControl = true;
	m_MoveCmd.y = 0.0f;
}

// 0x692530
void
CTaskSimpleDuck::SetDuckTimer(uint16 time)
{
	if(m_DuckControlType != DUCK_SCRIPT_CONTROLLED)
		m_LengthOfDuck = time;
}

// 0x6924F0
bool
CTaskSimpleDuck::SetControlType(eDuckControlType controlType)
{
	if(m_bIsFinished || m_bIsAborting)
		return false;
	if(controlType == DUCK_SCRIPT_CONTROLLED)
		m_LengthOfDuck = 0;
	m_DuckControlType = controlType;
	m_StartTime = CTimer::GetTimeInMilliseconds();
	m_bIsInControl = true;
	return true;
}

// 0x692100
bool
CTaskSimpleDuck::MakeAbortable(CPed *ped, eAbortPriority priority, const CEvent *event)
{
	(void)event; // the whizzed-by-shot early-out needs CEventGunShotWhizzedBy (unported)

	switch(priority){
	case ABORT_PRIORITY_IMMEDIATE:
		// Replace/blend duck anim with idle anim
		if(m_DuckAnim){
			if(m_DuckAnim->m_Flags & ANIMATION_IS_PARTIAL)
				m_DuckAnim->m_BlendDelta = -1000.0f;
			else
				CAnimManager::BlendAnimation(ped->GetClump(), ped->m_animGroup, ANIM_ID_IDLE, 1000.0f);
			m_DuckAnim->SetDefaultFinishCallback();
			m_DuckAnim = nil;
		}

		if(m_MoveAnim){
			m_MoveAnim->m_BlendDelta = -1000.0f;
			m_MoveAnim->SetDefaultFinishCallback();
			m_MoveAnim = nil;
		}

		SetPedIsDucking(ped, false);
		return true;
	default:
		break;
	}

	if(m_MoveAnim){
		switch(m_MoveAnim->m_AnimId){
		case ANIM_ID_CROUCH_ROLL_L:
		case ANIM_ID_CROUCH_ROLL_R:
			return false;
		}
	}

	float blendDelta = (priority == ABORT_PRIORITY_URGENT) ? -8.0f : -4.0f;

	if(m_DuckAnim){
		if(m_DuckAnim->m_BlendAmount > 0.0f && m_DuckAnim->m_BlendDelta >= 0.0f){
			if(m_DuckAnim->m_Flags & ANIMATION_IS_PARTIAL)
				m_DuckAnim->m_BlendDelta = blendDelta;
			CAnimManager::BlendAnimation(ped->GetClump(), ped->m_animGroup, ANIM_ID_IDLE, -blendDelta);
			ped->m_nSwimmingMoveState = PEDMOVE_STILL;
		}

		if(priority == ABORT_PRIORITY_URGENT){
			m_DuckAnim->SetDefaultFinishCallback();
			m_DuckAnim = nil;
		}
	}

	if(m_MoveAnim){
		if(m_MoveAnim->m_BlendAmount > 0.0f && m_MoveAnim->m_BlendDelta >= 0.0f){
			if(priority == ABORT_PRIORITY_URGENT || m_MoveAnim->m_AnimId == ANIM_ID_GUNCROUCHFWD || m_MoveAnim->m_AnimId == ANIM_ID_GUNCROUCHBWD){
				m_MoveAnim->m_BlendDelta = blendDelta;
				m_MoveAnim->SetFlag(ANIMATION_IS_PLAYING, false);
			}
		}

		if(priority == ABORT_PRIORITY_URGENT){
			m_MoveAnim->SetDefaultFinishCallback();
			m_MoveAnim = nil;
		}
	}

	if(priority == ABORT_PRIORITY_URGENT){
		m_bIsFinished = true;
		SetPedIsDucking(ped, false);
		return true;
	}

	m_bIsAborting = true;
	return false;
}

void
CTaskSimpleDuck::SetPedIsDucking(CPed *ped, bool value)
{
	ped->bIsDucking = value;
	m_bNeedToSetDuckFlag = !value;
}

// 0x694390
bool
CTaskSimpleDuck::ProcessPed(CPed *ped)
{
	if(m_bIsFinished || ped->m_fHealth < 1.0f){
		if(!m_bIsAborting)
			MakeAbortable(ped);
		if(!m_bNeedToSetDuckFlag)
			ped->bIsDucking = false;
		return true;
	}

	if(m_bNeedToSetDuckFlag){
		CPedIntelligence *intelligence = ped->m_pIntelligence;
		CTask *simplest = intelligence ? intelligence->GetTaskManager().GetSimplestActiveTask() : nil;
		CTaskSimpleDuck *simplestActiveDuck = simplest && simplest->GetTaskType() == TASK_SIMPLE_DUCK
			? (CTaskSimpleDuck*)simplest : nil;
		CTaskSimpleDuck *secondaryDuck = intelligence ? intelligence->GetTaskSecondaryDuck() : nil;

		if(secondaryDuck == this && simplestActiveDuck){
			simplestActiveDuck->AbortBecauseOfOtherDuck(ped);
		}else if(simplestActiveDuck == this && secondaryDuck){
			MakeAbortable(ped);
			return false;
		}

		// SA also clears the use-gun task's anim here; not ported yet.
		SetPedIsDucking(ped, true);
	}else if(!ped->bIsDucking){
		m_bIsFinished = true;
	}

	if(m_bIsAborting)
		return false;

	if(m_bIsInControl){
		m_CountDownFrames = 4;

		if(m_LengthOfDuck && CTimer::GetTimeInMilliseconds() >= m_StartTime + m_LengthOfDuck)
			MakeAbortable(ped);

		if(m_ShotWhizzingCounter > 0)
			m_ShotWhizzingCounter = (int16)(m_LengthOfDuck - (uint16)(1000.0f * CTimer::GetTimeStep() / 60.0f));

		if(m_DuckAnim){
			if(m_DuckAnim->GetBlendAmount() > 0.99f){
				switch(m_DuckControlType){
				case DUCK_TASK_CONTROLLED:
				case DUCK_SCRIPT_CONTROLLED:
					SetMoveAnim(ped);
					break;
				}
			}
		}else{
			m_DuckAnim = CAnimManager::BlendAnimation(
				ped->GetClump(),
				ANIM_GROUP_DEFAULT,
				m_DuckControlType == DUCK_STANDALONE ? ANIM_ID_DUCK_COWER : ANIM_ID_WEAPON_CROUCH,
				4.0f
			);
			m_DuckAnim->SetFinishCallback(DeleteDuckAnimCB, this);

			CAnimBlendAssociation *weaponCrouchAnim = RpAnimBlendClumpGetAssociation(ped->GetClump(), ANIM_ID_WEAPON_CROUCH);
			if(weaponCrouchAnim){
				if(weaponCrouchAnim->m_BlendAmount > 0.0f && weaponCrouchAnim->m_BlendDelta >= 0.0f){
					if(weaponCrouchAnim->m_Flags & ANIMATION_IS_PARTIAL)
						weaponCrouchAnim->m_BlendDelta = -4.0f;
					else
						CAnimManager::BlendAnimation(ped->GetClump(), ANIM_GROUP_DEFAULT, ANIM_ID_IDLE, 4.0f);
				}
			}
		}

		if(m_DuckControlType == DUCK_TASK_CONTROLLED)
			m_bIsInControl = false;

		return false;
	}

	m_CountDownFrames--;
	if(m_CountDownFrames || !MakeAbortable(ped))
		return false;

	ped->bIsDucking = false;
	return true;
}
