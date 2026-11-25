#include "common.h"

#include "TaskSimpleGoToPoint.h"
#include "AnimBlendAssocGroup.h"
#include "AnimBlendStaticAssociation.h"
#include "AnimManager.h"
#include "General.h"
#include "Pad.h"
#include "Ped.h"
#include "PedIntelligence.h"
#include "PlayerPed.h"
#include "TaskSimpleStandStill.h"
#include "Timer.h"
#include "WeaponInfo.h"

// SA CTaskSimpleGoToPoint::CTaskSimpleGoToPoint (0x667CD0).
CTaskSimpleGoToPoint::CTaskSimpleGoToPoint(eMoveState moveState, const CVector &targetPoint, float fRadius, bool bMoveTowardsTargetPoint, bool a6)
	: CTaskSimpleGoTo(moveState, targetPoint, fRadius)
{
	m_GoToPointFlags = 0;
	gotoPointFlags.m_bMoveTowardsTargetPoint = bMoveTowardsTargetPoint;
	gotoPointFlags.m_b04 = a6;
	// SA leaves this uninitialised; reVC's CVector does not zero itself,
	// so start from the target rather than from pool garbage.
	m_vecLastPedPos = targetPoint;
}

// SA CTaskSimpleGoToPoint::MakeAbortable (0x667D60). A leisurely abort
// leaves the ped walking but stops it steering (m_b02); an immediate one
// first parks it in a stand-still.
bool
CTaskSimpleGoToPoint::MakeAbortable(CPed *ped, eAbortPriority priority, const CEvent *event)
{
	(void)event;
	QuitIK(ped);
	if(priority != ABORT_PRIORITY_URGENT){
		if(priority != ABORT_PRIORITY_IMMEDIATE){
			gotoPointFlags.m_b02 = true;
			return false;
		}
		CTaskSimpleStandStill standStillTask(0, false, false, 8.0f);
		standStillTask.ProcessPed(ped);
	}
	return true;
}

// SA CTaskSimpleGoToPoint::ProcessPed (0x66D710).
bool
CTaskSimpleGoToPoint::ProcessPed(CPed *ped)
{
	CPlayerPed *player = ped->IsPlayer() ? (CPlayerPed*)ped : nil;
	ped->m_pedIK.m_flags |= CPedIK::SLOPE_PITCH;
	if(HasCircledTarget(ped)){
		if(!gotoPointFlags.m_b05){
			gotoPointFlags.m_b03 = true;
			QuitIK(ped);
			return true;
		}
	}
	if(ped->bHasJustLeftCar){
		if(ped->GetIntelligence())
			ped->GetIntelligence()->GetEventScanner().ScanForEventsNow(*ped, false);
		ped->bHasJustLeftCar = false;
	}else if(ped->bHasJustSoughtCover){
		if(ped->GetIntelligence())
			ped->GetIntelligence()->GetEventScanner().ScanForEventsNow(*ped, true);
		ped->bHasJustSoughtCover = false;
	}else{
		// SA hands duck movement to CTaskSimpleDuck here. The duck task is
		// not ported yet; while one is in the duck slot the ped simply
		// keeps ducking instead of taking a normal move anim.
		CTask *duckTask = nil;
		if(ped->GetIntelligence())
			duckTask = ped->GetIntelligence()->GetTaskManager().GetTaskSecondary(TASK_SECONDARY_DUCK);
		if(duckTask == nil){
			ped->SetMoveState(m_moveState);
			if(ped->IsPlayer()){
				if(CPad::GetPad(0)->DisablePlayerControls != PLAYERCONTROL_ENABLED){
					// SA also sets the player move blend ratio here;
					// that CPlayerPed helper is not ported yet.
					player->SetRealMoveAnim();
				}else{
					bool bSprinting = false;
					CWeaponInfo *weaponInfo = CWeaponInfo::GetWeaponInfo(ped->GetWeapon()->m_eWeaponType);
					if(weaponInfo == nil || !weaponInfo->IsFlagSet(WEAPONFLAG_HEAVY)){
						// SA additionally skips sprinting while holding
						// an entity and on non-sprint surfaces; the
						// hold task and surface table land later.
						if(!player->GetPlayerData()->m_bPlayerSprintDisabled){
							CAnimBlendAssocGroup *animGroup = &CAnimManager::GetAssocGroups()[ped->m_animGroup];
							CAnimBlendStaticAssociation *assoc1 = animGroup->GetAnimation(ANIM_ID_RUN);
							CAnimBlendStaticAssociation *assoc2 = animGroup->GetAnimation(ANIM_ID_SPRINT);
							if(assoc1->m_BlendHier != assoc2->m_BlendHier && player->ControlButtonSprint(SPRINT_GROUND) >= 1.0f){
								ped->SetMoveState(PEDMOVE_SPRINT);
								bSprinting = true;
							}
						}
					}
					if(!bSprinting && ped->m_nMoveState == PEDMOVE_SPRINT)
						ped->SetMoveState(PEDMOVE_RUN);
					player->SetRealMoveAnim();
				}
			}else{
				ped->SetMoveAnim();
			}
		}
		bool bAimingRotationSet = false;
		const CVector &pedPos = ped->GetPosition();
		CVector2D vecDistance = m_vecTargetPoint - pedPos;
		CVector2D vecDirection = vecDistance;
		vecDirection.Normalise();
		CVector2D vecDistanceToTargetFromLastPoint = m_vecTargetPoint - m_vecLastPedPos;
		CVector2D vecDistanceToTargetFromPoint = m_vecTargetPoint - (CTimer::GetTimeStep() * ped->m_vecMoveSpeed + pedPos);
		m_vecLastPedPos = pedPos;
		if((vecDistance.MagnitudeSqr() >= m_fRadius * m_fRadius
			|| gotoPointFlags.m_b05
			|| !ped->bIgnoreHeightCheckOnGotoPointTask && Abs(pedPos.z - m_vecTargetPoint.z) >= 2.0f
			|| gotoPointFlags.m_b04 && DotProduct2D(vecDistance, vecDistanceToTargetFromLastPoint) >= 0.0f)
			&& (gotoPointFlags.m_b04 || DotProduct2D(vecDistance, vecDistanceToTargetFromPoint) > 0.0f || gotoPointFlags.m_b05))
		{
			if(!gotoPointFlags.m_b02){
				float fAngleInRadians = CGeneral::GetRadianAngleBetweenPoints(vecDirection.x, vecDirection.y, 0.0f, 0.0f);
				ped->m_fRotationDest = CGeneral::LimitRadianAngle(fAngleInRadians);
				bAimingRotationSet = true;
			}
		}else{
			gotoPointFlags.m_b03 = true;
		}
		if(!bAimingRotationSet){
			QuitIK(ped);
			return true;
		}
	}
	if(gotoFlags.m_bTargetPointUpdated){
		QuitIK(ped);
		gotoFlags.m_bTargetPointUpdated = false;
	}
	SetUpIK(ped);
	gotoPointFlags.m_b05 = false;
	return false;
}

// SA CTaskSimpleGoToPoint::UpdatePoint (0x645700).
void
CTaskSimpleGoToPoint::UpdatePoint(const CVector &targetPosition, float fRadius, bool bDontCheckRadius)
{
	if(bDontCheckRadius || m_vecTargetPoint != targetPosition || m_fRadius != fRadius){
		m_vecTargetPoint = targetPosition;
		m_fRadius = fRadius;

		gotoFlags.m_targetCircledFlags = 0;
		gotoFlags.m_bTargetPointUpdated = true;
	}
}

// SA CTaskSimpleGoToPoint::WasTaskSuccessful.
bool
CTaskSimpleGoToPoint::WasTaskSuccessful(CPed *ped) const
{
	return gotoPointFlags.m_b03 && Abs(m_vecTargetPoint.z - ped->GetPosition().z) < 3.0f;
}
