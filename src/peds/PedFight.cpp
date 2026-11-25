#include "common.h"

#include "main.h"
#include "RpAnimBlend.h"
#include "AnimBlendClumpData.h"
#include "AnimBlendAssociation.h"
#include "AnimBlendHierarchy.h"
#include "AnimBlendStaticAssociation.h"
#include "Camera.h"
#include "FileMgr.h"
#include "CarCtrl.h"
#include "Darkel.h"
#include "DMAudio.h"
#include "General.h"
#include "Object.h"
#include "Pad.h"
#include "Particle.h"
#include "Ped.h"
#include "PlayerPed.h"
#include "Stats.h"
#include "TempColModels.h"
#include "VisibilityPlugins.h"
#include "Vehicle.h"
#include "Automobile.h"
#include "WaterLevel.h"
#include "World.h"
#ifdef CUSTOM_MODELS
#include "CustomModels.h"
#endif
#include <vector>

// The parallel SA bank can be missing a particular reaction. Resolve on the
// target clump so the fallback remains in its own skeleton's animation bank.
static CAnimBlendAssociation *
BlendDamageReaction(RpClump *clump, AnimationId anim, float blendSpeed = 8.0f)
{
	CAnimBlendAssociation *assoc = RpAnimBlendClumpGetAssociation(clump, anim);
	if(!assoc || assoc->m_BlendDelta < 0.0f)
		assoc = CAnimManager::BlendAnimation(clump, ANIM_GROUP_DEFAULT, anim, blendSpeed);
	if(!assoc && anim != ANIM_ID_HIT_FRONT)
		assoc = CAnimManager::BlendAnimation(clump, ANIM_GROUP_DEFAULT, ANIM_ID_HIT_FRONT, blendSpeed);
	return assoc;
}
#include "Bike.h"
#include "Glass.h"
#include "SpecialFX.h"

uint16 nPlayerInComboMove;
RpClump* flyingClumpTemp;

FightMove tFightMoves[NUM_FIGHTMOVES] =
{
  { ANIM_ID_UNDEFINED,  0.0f,  0.0f,  0.0f,  0.0f,  1.0f, HITLEVEL_NULL, 0, 0 },
  { ANIM_ID_FIGHT_1, 0.2f, 8.f/30.f, 0.0f, 0.3f, 1.0f, HITLEVEL_HIGH, 1, 0 },
  { ANIM_ID_FIGHT_IDLE,  0.0f,  0.0f,  0.0f,  0.0f,  1.0f, HITLEVEL_NULL, 0, 0 },
  { ANIM_ID_FIGHTSHF,  0.0f,  0.0f,  0.0f,  0.0f,  1.0f, HITLEVEL_NULL, 0, 0 },
  { ANIM_ID_FIGHT_3, 4.f/30.f, 0.2f, 0.0f, 0.6f, 1.0f, HITLEVEL_LOW, 2, 0 },
  { ANIM_ID_FIGHT_2, 8.f/30.f, 10.f/30.f, 0.0f, 0.4f, 1.0f, HITLEVEL_HIGH, 3, 0 },
  { ANIM_ID_FIGHT_1, 4.f/30.f, 0.2f, 0.0f, 0.7f, 1.0f, HITLEVEL_HIGH, 3, 0 },
  { ANIM_ID_FIGHT_1, 4.f/30.f, 7.f/30.f, 10.f/30.f, 0.4f, 1.0f, HITLEVEL_HIGH, 1, 0 },
  { ANIM_ID_FIGHT_1, 8.f/30.f, 10.f/30.f, 0.0f, 0.5f, 1.0f, HITLEVEL_MEDIUM, 4, 0 },
  { ANIM_ID_FIGHT_3, 8.f/30.f, 10.f/30.f, 0.0f,  0.6f, 1.0f, HITLEVEL_MEDIUM, 4, 0 },
  { ANIM_ID_FIGHT_1,  8.f/30.f,  10.f/30.f,  0.0f,  0.5f,  1.0f, HITLEVEL_HIGH, 2, 0 },
  { ANIM_ID_FIGHT_2,  8.f/30.f,  10.f/30.f,  0.0f,  0.5f,  1.0f, HITLEVEL_MEDIUM, 2, 0 },
  { ANIM_ID_FIGHT_M,  8.f/30.f,  10.f/30.f,  0.0f,  0.5f,  1.0f, HITLEVEL_LOW, 2, 0 },
  { ANIM_ID_FIGHT_M,  8.f/30.f,  10.f/30.f,  0.0f,  0.5f,  1.0f, HITLEVEL_LOW, 2, 0 },
  { ANIM_ID_FIGHT_G,  8.f/30.f,  10.f/30.f,  0.0f,  0.5f,  1.0f, HITLEVEL_MEDIUM, 2, 0 },
  { ANIM_ID_FIGHT_M,  8.f/30.f,  10.f/30.f,  0.0f,  0.5f,  1.0f, HITLEVEL_MEDIUM, 2, 0 },
  { ANIM_ID_FIGHT_G,  8.f/30.f,  10.f/30.f,  0.0f,  0.5f,  1.0f, HITLEVEL_HIGH, 2, 0 },
  { ANIM_ID_FIGHT_1, 10.f/30.f, 14.f/30.f, 0.0f, 0.4f, 1.0f, HITLEVEL_GROUND, 1, 0 },
  { ANIM_ID_HIT_FRONT,  0.0f,  0.0f,  0.0f,  0.0f,  1.0f, HITLEVEL_NULL, 0, 0 },
  { ANIM_ID_HIT_BACK,  0.0f,  0.0f,  0.0f,  0.0f,  1.0f, HITLEVEL_NULL, 0, 0 },
  { ANIM_ID_HIT_R,  0.0f,  0.0f,  0.0f,  0.0f,  1.0f, HITLEVEL_NULL, 0, 0 },
  { ANIM_ID_HIT_L,  0.0f,  0.0f,  0.0f,  0.0f,  1.0f, HITLEVEL_NULL, 0, 0 },
  { ANIM_ID_KO_SHOT_STOM,  0.0f,  0.0f,  0.0f,  0.0f,  1.0f, HITLEVEL_NULL, 0, 0 },
  { ANIM_ID_KO_SHOT_STOM,  0.0f,  0.0f,  0.0f,  0.0f,  1.0f, HITLEVEL_NULL, 0, 0 },
  { ANIM_ID_KO_SHOT_FACE,  0.0f,  0.0f,  0.0f,  0.0f,  1.0f, HITLEVEL_NULL, 0, 0 },
  { ANIM_ID_HIT_WALK,  0.0f,  0.0f,  0.0f,  0.0f,  1.0f, HITLEVEL_NULL, 0, 0 },
  { ANIM_ID_FLOOR_HIT,  0.0f,  0.0f,  0.0f,  0.0f,  1.0f, HITLEVEL_NULL, 0, 0 },
  { ANIM_ID_HIT_BEHIND,  0.0f,  0.0f,  0.0f,  0.0f,  1.0f, HITLEVEL_NULL, 0, 0 },
  { ANIM_ID_FIGHT_1, 4.f/30.f, 7.f/30.f, 10.f/30.f, 0.4f, 1.0f, HITLEVEL_HIGH, 1, 0 },
  { ANIM_ID_FIGHT_2, 4.f/30.f, 7.f/30.f, 10.f/30.f, 0.4f, 1.0f, HITLEVEL_HIGH, 1, 0 },
  { ANIM_ID_FIGHT_3, 4.f / 30.f, 7.f / 30.f, 10.f / 30.f, 0.4f, 1.0f, HITLEVEL_HIGH, 1, 0 },
  { ANIM_ID_FIGHT2IDLE,  0.0f,  0.0f,  0.0f,  0.0f,  1.0f, HITLEVEL_NULL, 0, 0 }
};

static PedOnGroundState
CheckForPedsOnGroundToAttack(CPed *attacker, CPed **pedOnGround)
{
	PedOnGroundState stateToReturn;
	float angleToFace;
	CPed *currentPed = nil;
	PedState currentPedState;
	CPed *pedOnTheFloor = nil;
	CPed *deadPed = nil;
	CPed *pedBelow = nil;
	bool foundDead = false;
	bool foundOnTheFloor = false;
	bool foundBelow = false;
	float angleDiff;
	float distance;

	if (!CGame::nastyGame)
		return NO_PED;

	for (int currentPedId = 0; currentPedId < attacker->m_numNearPeds; currentPedId++) {

		currentPed = attacker->m_nearPeds[currentPedId];

		CVector posDifference = currentPed->GetPosition() - attacker->GetPosition();
		distance = posDifference.Magnitude();

		if (distance < 2.0f) {
			angleToFace = CGeneral::GetRadianAngleBetweenPoints(
				currentPed->GetPosition().x, currentPed->GetPosition().y,
				attacker->GetPosition().x, attacker->GetPosition().y);

			angleToFace = CGeneral::LimitRadianAngle(angleToFace);
			attacker->m_fRotationCur = CGeneral::LimitRadianAngle(attacker->m_fRotationCur);

			angleDiff = Abs(angleToFace - attacker->m_fRotationCur);

			if (angleDiff > PI)
				angleDiff = 2 * PI - angleDiff;

			currentPedState = currentPed->m_nPedState;

			if (currentPed->OnGroundOrGettingUp()) {
				if (distance < 2.0f && angleDiff < DEGTORAD(65.0f)) {
					if (currentPedState == PED_DEAD) {
						foundDead = 1;
						if (!deadPed)
							deadPed = currentPed;
					} else if (!currentPed->IsPedHeadAbovePos(-0.6f)) {
						foundOnTheFloor = 1;
						if (!pedOnTheFloor)
							pedOnTheFloor = currentPed;
					}
				}
			} else if ((distance < 0.8f && angleDiff < DEGTORAD(75.0f))
						|| (distance < 1.3f && angleDiff < DEGTORAD(55.0f))
						|| (distance < 1.7f && angleDiff < DEGTORAD(35.0f))
						|| (distance < 2.0f && angleDiff < DEGTORAD(30.0f))) {

				// Either this condition or below one was probably returning 4 early in development. See Fight().
				foundBelow = 1;
				pedBelow = currentPed;
				break;
			} else {
				if (angleDiff < DEGTORAD(75.0f)) {
					foundBelow = 1;
					if (!pedBelow)
						pedBelow = currentPed;
				}
			}
		}
	}

	if (foundOnTheFloor) {
		currentPed = pedOnTheFloor;
		stateToReturn = PED_ON_THE_FLOOR;
	} else if (foundDead) {
		currentPed = deadPed;
		stateToReturn = PED_DEAD_ON_THE_FLOOR;
	} else if (foundBelow) {
		currentPed = pedBelow;
		stateToReturn = PED_IN_FRONT_OF_ATTACKER;
	} else {
		currentPed = nil;
		stateToReturn = NO_PED;
	}

	if (pedOnGround)
		*pedOnGround = currentPed;

	return stateToReturn;
}

// An SA ped may have ped.ifp but no matching weapon dictionary in custom.
// Do not borrow a native VC clip: the association bank must match the clump's
// skeleton. Report the missing slot once so repeated AI attacks do not flood
// the log, and let the caller leave its attack/aim state safely.
static void
ReportUnavailableWeaponAnim(CPed *ped, AnimationId animId, AssocGroupId groupId = (AssocGroupId)NUM_ANIM_ASSOC_GROUPS)
{
	int weapon = ped->GetWeapon()->m_eWeaponType;
	int group = groupId == NUM_ANIM_ASSOC_GROUPS ?
		CWeaponInfo::GetWeaponInfo(ped->GetWeapon()->m_eWeaponType)->m_AnimToPlay : groupId;
	int anim = animId;
	static bool warned[NUM_ANIM_ASSOC_GROUPS][ANIM_ID_STRIP_G + 1] = {};
	if(group >= 0 && group < NUM_ANIM_ASSOC_GROUPS && anim >= 0 && anim <= ANIM_ID_STRIP_G){
		if(warned[group][anim])
			return;
		warned[group][anim] = true;
	}
	const char *groupName = group >= 0 && group < NUM_ANIM_ASSOC_GROUPS ?
		CAnimManager::GetAnimGroupName((AssocGroupId)group) : nil;
	const char *clipName = nil;
	if(groupName){
		AnimAssocDefinition *def = &CAnimManager::GetAssocGroupDefs()[group];
		clipName = def->AnimNames[animId - CAnimManager::GetAssocGroups()[group].m_IdOffset];
	}
	if(groupName && clipName){
		REVC_LOG_WARNING("Weapon animation unavailable: ped=%d, weapon=%d, group=%d (%s), anim=%d (%s); check custom/%s.ifp or .ani (ANP2/ANP3) for this clip and compatible SA bones",
			ped->GetModelIndex(), weapon, group, groupName, anim, clipName, groupName);
		return;
	}
	REVC_LOG_WARNING("Weapon animation unavailable: ped=%d, weapon=%d, group=%d, anim=%d; check the matching SA IFP/ANI in custom",
		ped->GetModelIndex(), weapon, group, anim);
}

void
CPed::SetPointGunAt(CEntity *to)
{
	if (to) {
		SetLookFlag(to, true, true);
		SetAimFlag(to);
		SetLookTimer(INT32_MAX);
	}

	CWeaponInfo* curWeapon = CWeaponInfo::GetWeaponInfo(GetWeapon()->m_eWeaponType);
	if (m_nPedState == PED_AIMGUN || (bIsDucking && !IsPlayer()) || m_nWaitState == WAITSTATE_PLAYANIM_DUCK || curWeapon->m_AnimToPlay == ANIM_GROUP_DEFAULT)
		return;

	if (m_nPedState != PED_ATTACK)
		SetStoredState();

	SetPedState(PED_AIMGUN);
	bIsPointingGunAt = true;
	SetMoveState(PEDMOVE_STILL);

	CAnimBlendAssociation *aimAssoc;

	if (bCrouchWhenShooting && bIsDucking && GetCrouchFireAnim(curWeapon)) {
		aimAssoc = RpAnimBlendClumpGetAssociation(GetClump(), GetCrouchFireAnim(curWeapon));
	} else {
		aimAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_FIRE);
	}

	if (!aimAssoc || aimAssoc->m_BlendDelta < 0.0f) {
		AnimationId aimAnim = bCrouchWhenShooting && bIsDucking && GetCrouchFireAnim(curWeapon) ?
			GetCrouchFireAnim(curWeapon) : ANIM_ID_FIRE;
		if (aimAnim == ANIM_ID_FIRE)
			aimAssoc = CAnimManager::AddAnimation(GetClump(), curWeapon->m_AnimToPlay, aimAnim);
		else
			aimAssoc = CAnimManager::BlendAnimation(GetClump(), curWeapon->m_AnimToPlay, aimAnim, 4.0f);
		if (!aimAssoc) {
			ReportUnavailableWeaponAnim(this, aimAnim);
			ClearPointGunAt();
			return;
		}
		aimAssoc->m_BlendAmount = 0.0f;
		aimAssoc->m_BlendDelta = 8.0f;
	}
	if (to && !IsPlayer())
		Say(SOUND_PED_ATTACK);
}

void
CPed::PointGunAt(void)
{
	CWeaponInfo *weaponInfo = CWeaponInfo::GetWeaponInfo(GetWeapon()->m_eWeaponType);
	float animLoopStart = weaponInfo->m_fAnimLoopStart;
	CAnimBlendAssociation *weaponAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_FIRE);
	if (!weaponAssoc || weaponAssoc->m_BlendDelta < 0.0f) {
		if (weaponInfo->IsFlagSet(WEAPONFLAG_CROUCHFIRE)) {
			weaponAssoc = RpAnimBlendClumpGetAssociation(GetClump(), GetCrouchFireAnim(weaponInfo));
			animLoopStart = weaponInfo->m_fAnim2LoopStart;
		}
	}

	if (weaponAssoc && weaponAssoc->m_CurrentTime > animLoopStart * 0.4f) {
		weaponAssoc->SetCurrentTime(animLoopStart);
		weaponAssoc->m_Flags &= ~ANIMATION_IS_PLAYING;

		if (bIsDucking)
			m_pedIK.m_flags &= ~CPedIK::AIMS_WITH_ARM;

		if (weaponInfo->IsFlagSet(WEAPONFLAG_CANAIM_WITHARM))
			m_pedIK.m_flags |= CPedIK::AIMS_WITH_ARM;
		else
			m_pedIK.m_flags &= ~CPedIK::AIMS_WITH_ARM;
	}
}

void
CPed::ClearPointGunAt(void)
{
	CAnimBlendAssociation *animAssoc;
	CWeaponInfo *weaponInfo;

	ClearLookFlag();
	ClearAimFlag();
	bIsPointingGunAt = false;
	if (m_nPedState == PED_AIMGUN || m_nPedState == PED_ATTACK) {
		SetPedState(PED_IDLE);
		RestorePreviousState();
	}
	weaponInfo = CWeaponInfo::GetWeaponInfo(GetWeapon()->m_eWeaponType);
	animAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_FIRE);
	if (!animAssoc || animAssoc->m_BlendDelta < 0.0f) {
		if (weaponInfo->IsFlagSet(WEAPONFLAG_CROUCHFIRE)) {
			animAssoc = RpAnimBlendClumpGetAssociation(GetClump(), GetCrouchFireAnim(weaponInfo));
		}
	}
	if (animAssoc) {
		animAssoc->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE;
		animAssoc->m_BlendDelta = -4.0f;
	}
}

void
CPed::SetAttack(CEntity *victim)
{
	CPed *victimPed = nil;
	CWeaponInfo *curWeapon = CWeaponInfo::GetWeaponInfo(GetWeapon()->m_eWeaponType);
	CAnimBlendAssociation *animAssoc;

	if (victim && victim->IsPed())
		victimPed = (CPed*)victim;

	if (m_attackTimer > CTimer::GetTimeInMilliseconds() || m_nWaitState == WAITSTATE_SURPRISE || (bIsDucking && !bCrouchWhenShooting))
		return;

	if (curWeapon->IsFlagSet(WEAPONFLAG_RELOAD) &&
		(RpAnimBlendClumpGetAssociation(GetClump(), GetReloadAnim(curWeapon)) || RpAnimBlendClumpGetAssociation(GetClump(), GetCrouchReloadAnim(curWeapon)))) {
		if (!IsPlayer() || m_nPedState != PED_ATTACK || ((CPlayerPed*)this)->HaveTargetSelected())
			bIsAttacking = false;
		else
			bIsAttacking = true;

		return;
	}

	if (GetWeapon()->m_eWeaponType == WEAPONTYPE_UNARMED || curWeapon->IsFlagSet(WEAPONFLAG_FIGHTMODE) || GetWeapon()->m_eWeaponType == WEAPONTYPE_BRASSKNUCKLE) {
		if (IsPlayer() ||
			(m_nPedState != PED_FIGHT && m_nMoveState != PEDMOVE_NONE && m_nMoveState != PEDMOVE_STILL
				&& !(m_pedStats->m_flags & STAT_SHOPPING_BAGS) && curWeapon->IsFlagSet(WEAPONFLAG_PARTIALATTACK))) {

			if (m_nPedState != PED_ATTACK) {
				SetPedState(PED_ATTACK);
				bIsAttacking = false;

				CAnimBlendAssociation *animAssoc = CAnimManager::BlendAnimation(GetClump(), curWeapon->m_AnimToPlay, ANIM_ID_FIGHT_3, 8.0f);
				if (!animAssoc) {
					ReportUnavailableWeaponAnim(this, ANIM_ID_FIGHT_3);
					RestorePreviousState();
					return;
				}
				animAssoc->m_Flags |= ANIMATION_IS_PLAYING;
				if (animAssoc->m_CurrentTime == animAssoc->m_BlendHier->m_fTotalTime)
					animAssoc->SetCurrentTime(0.0f);

				animAssoc->SetFinishCallback(FinishedAttackCB, this);
			}
		} else {
			StartFightAttack(CGeneral::GetRandomNumber());
		}
		return;
	}

	if (curWeapon->IsFlagSet(WEAPONFLAG_PARTIALATTACK) &&
		(IsPlayer() && ((CPlayerPed*)this)->MoveSpeed() >= 1.0f ||
		m_nMoveState == PEDMOVE_WALK || m_nMoveState == PEDMOVE_RUN)) {

		if (m_nPedState != PED_ATTACK) {
			SetPedState(PED_ATTACK);
			bIsAttacking = false;
			CAnimBlendAssociation* animAssoc = CAnimManager::BlendAnimation(GetClump(), curWeapon->m_AnimToPlay, ANIM_ID_FIGHT_3, 8.0f);
			if (!animAssoc) {
				ReportUnavailableWeaponAnim(this, ANIM_ID_FIGHT_3);
				RestorePreviousState();
				return;
			}
			animAssoc->m_Flags |= ANIMATION_IS_PLAYING;
			if (animAssoc->m_CurrentTime == animAssoc->m_BlendHier->m_fTotalTime)
				animAssoc->SetCurrentTime(0.0f);

			animAssoc->SetFinishCallback(FinishedAttackCB, this);
		}
		return;
	}

	if (m_pSeekTarget)
		m_pSeekTarget->CleanUpOldReference(&m_pSeekTarget);
	m_pSeekTarget = victim;
	if (m_pSeekTarget)
		m_pSeekTarget->RegisterReference((CEntity **) &m_pSeekTarget);

	if (curWeapon->IsFlagSet(WEAPONFLAG_CANAIM)) {
		CVector aimPos = GetRight() * 0.1f + GetForward() * 0.2f + GetPosition();
		aimPos += GetUp() * 0.35f;
		CEntity *obstacle = CWorld::TestSphereAgainstWorld(aimPos, 0.2f, nil, true, false, false, true, false, false);
		if (obstacle) {
			if(gaTempSphereColPoints[0].surfaceB != SURFACE_TRANSPARENT_CLOTH && gaTempSphereColPoints[0].surfaceB != SURFACE_METAL_CHAIN_FENCE &&
				gaTempSphereColPoints[0].surfaceB != SURFACE_WOOD_BENCH && gaTempSphereColPoints[0].surfaceB != SURFACE_SCAFFOLD_POLE) {
				if (!IsPlayer()) {
					bObstacleShowedUpDuringKillObjective = true;
					m_shootTimer = 0;
					SetAttackTimer(1500);
					m_shotTime = CTimer::GetTimeInMilliseconds();
				}
				return;
			}
		}

		m_pLookTarget = victim;
		if (victim) {
			m_pLookTarget->RegisterReference((CEntity **) &m_pLookTarget);
			m_pSeekTarget->RegisterReference((CEntity **) &m_pSeekTarget);
		}

		if (m_pLookTarget) {
			SetAimFlag(m_pLookTarget);
#ifdef FREE_CAM
		} else if (this != FindPlayerPed() || !((CPlayerPed*)this)->m_bFreeAimActive) {
#else
		} else {
#endif
			if (this == FindPlayerPed() && TheCamera.Cams[0].Using3rdPersonMouseCam()) {
				SetAimFlag(m_fRotationCur);
				((CPlayerPed*)this)->FPSMoveHeading() = TheCamera.Find3rdPersonQuickAimPitch();
			} else if (curWeapon->IsFlagSet(WEAPONFLAG_CANAIM_WITHARM)) {
				SetAimFlag(m_fRotationCur);
			}
		}
	}
#ifdef FIX_BUGS
	// fix aiming for flamethrower and minigun while using PC controls
	else if (curWeapon->m_AnimToPlay == ANIM_GROUP_FLAME && TheCamera.Cams[0].Using3rdPersonMouseCam() && this == FindPlayerPed())
	{
		SetAimFlag(m_fRotationCur);
		((CPlayerPed*)this)->FPSMoveHeading() = TheCamera.Find3rdPersonQuickAimPitch();
	}
#endif
	if (m_nPedState == PED_ATTACK) {
		bIsAttacking = true;
		return;
	}

	if (IsPlayer() || (!victimPed || victimPed->IsPedInControl())) {
		if (IsPlayer())
			CPad::GetPad(0)->ResetAverageWeapon();

		uint8 pointBlankStatus;
		if ((curWeapon->m_eWeaponFire == WEAPON_FIRE_INSTANT_HIT || GetWeapon()->m_eWeaponType == WEAPONTYPE_FLAMETHROWER)
			&& TheCamera.PlayerWeaponMode.Mode != CCam::MODE_M16_1STPERSON
			&& TheCamera.PlayerWeaponMode.Mode != CCam::MODE_M16_1STPERSON_RUNABOUT
			&& TheCamera.PlayerWeaponMode.Mode != CCam::MODE_SNIPER
			&& TheCamera.PlayerWeaponMode.Mode != CCam::MODE_SNIPER_RUNABOUT
			&& (pointBlankStatus = CheckForPointBlankPeds(victimPed)) != NO_POINT_BLANK_PED) {
			ClearAimFlag();

			// This condition is pointless
			if (pointBlankStatus == POINT_BLANK_FOR_WANTED_PED || !victimPed && (IsPlayer() || !m_carInObjective))
				StartFightAttack(200);
		} else {
			if (!curWeapon->IsFlagSet(WEAPONFLAG_CANAIM))
				m_pSeekTarget = nil;

			if (m_nPedState != PED_AIMGUN)
				SetStoredState();

			SetPedState(PED_ATTACK);
			SetMoveState(PEDMOVE_NONE);
			AnimationId fireAnim;
			if (bCrouchWhenShooting && bIsDucking && curWeapon->IsFlagSet(WEAPONFLAG_CROUCHFIRE)) {
				fireAnim = GetCrouchFireAnim(curWeapon);
				CAnimBlendAssociation* curMoveAssoc = RpAnimBlendClumpGetAssociation(GetClump(), fireAnim);
				if (curMoveAssoc) {
					CAnimBlendStaticAssociation *requestedMoveAssoc = CAnimManager::GetAnimAssociation(curWeapon->m_AnimToPlay, GetCrouchFireAnim(curWeapon));
					if (requestedMoveAssoc && requestedMoveAssoc->GetHashKey() != curMoveAssoc->GetHashKey())
						delete curMoveAssoc;
				}
				animAssoc = CAnimManager::BlendAnimation(GetClump(), curWeapon->m_AnimToPlay, GetCrouchFireAnim(curWeapon), 8.0f);
			} else {
				float animDelta = 8.0f;
				if (curWeapon->m_eWeaponFire == WEAPON_FIRE_MELEE)
					animDelta = 1000.0f;

				if (curWeapon->IsFlagSet(WEAPONFLAG_THROW))
					fireAnim = ANIM_ID_GRENADE_WEAPON_START_THROW;
				else if (CGame::nastyGame && (curWeapon->IsFlagSet(WEAPONFLAG_GROUND_2ND) || curWeapon->IsFlagSet(WEAPONFLAG_GROUND_3RD))) {
					PedOnGroundState pedOnGround = CheckForPedsOnGroundToAttack(this, nil);
					if (pedOnGround > PED_IN_FRONT_OF_ATTACKER || pedOnGround == NO_PED && bIsStanding && m_pCurSurface && m_pCurSurface->IsVehicle()) {
						fireAnim = GetFireAnimGround(curWeapon, false);
					} else {
						fireAnim = GetFireAnimNotDucking(curWeapon);
					}
				} else {
					fireAnim = GetFireAnimNotDucking(curWeapon);
				}

				CAnimBlendAssociation* curFireAssoc = RpAnimBlendClumpGetAssociation(GetClump(), fireAnim);
				if (curFireAssoc) {
					CAnimBlendStaticAssociation *requestedFireAssoc = CAnimManager::GetAnimAssociation(curWeapon->m_AnimToPlay, fireAnim);
					if (requestedFireAssoc && requestedFireAssoc->GetHashKey() != curFireAssoc->GetHashKey())
						delete curFireAssoc;
				}
				animAssoc = CAnimManager::BlendAnimation(GetClump(), curWeapon->m_AnimToPlay, fireAnim, animDelta);
			}

			if (!animAssoc) {
				ReportUnavailableWeaponAnim(this, fireAnim);
				bIsAttacking = false;
				RestorePreviousState();
				return;
			}
			animAssoc->m_Flags |= ANIMATION_IS_PLAYING;
			if (animAssoc->m_CurrentTime == animAssoc->m_BlendHier->m_fTotalTime)
				animAssoc->SetCurrentTime(0.0f);

			animAssoc->SetFinishCallback(FinishedAttackCB, this);
		}
		return;
	}

	if (GetWeapon()->m_eWeaponType == WEAPONTYPE_BASEBALLBAT && victimPed->m_nPedState == PED_GETUP)
		SetWaitState(WAITSTATE_SURPRISE, nil);

	SetLookFlag(victim, true, true);
	SetLookTimer(100);
}

void
CPed::ClearAttack(void)
{
	if (m_nPedState != PED_ATTACK || (bIsDucking && !IsPlayer()) || m_nWaitState == WAITSTATE_PLAYANIM_DUCK)
		return;

	if (FindPlayerPed() == this && TheCamera.Using1stPersonWeaponMode()) {
		SetPointGunAt(nil);
	} else if (bIsPointingGunAt) {
		if (m_pLookTarget)
			SetPointGunAt(m_pLookTarget);
		else
			ClearPointGunAt();
	} else if (m_objective != OBJECTIVE_NONE) {
		SetIdle();
	} else {
		RestorePreviousState();
	}
}

void
CPed::ClearAttackByRemovingAnim(void)
{
	if (m_nPedState != PED_ATTACK)
		return;

	CWeaponInfo *weapon = CWeaponInfo::GetWeaponInfo(GetWeapon()->m_eWeaponType);
	CAnimBlendAssociation *weaponAssoc = RpAnimBlendClumpGetAssociation(GetClump(), GetPrimaryFireAnim(weapon));

	if (!weaponAssoc) {
		if (GetCrouchFireAnim(weapon))
			weaponAssoc = RpAnimBlendClumpGetAssociation(GetClump(), GetCrouchFireAnim(weapon));
	}
	if (!weaponAssoc) {
		if(GetFinishingAttackAnim(weapon))
			weaponAssoc = RpAnimBlendClumpGetAssociation(GetClump(), GetFinishingAttackAnim(weapon));
	}
	if (!weaponAssoc) {
		if(GetSecondFireAnim(weapon))
			weaponAssoc = RpAnimBlendClumpGetAssociation(GetClump(), GetSecondFireAnim(weapon));
	}
	if (!weaponAssoc) {
		if(Get3rdFireAnim(weapon))
			weaponAssoc = RpAnimBlendClumpGetAssociation(GetClump(), Get3rdFireAnim(weapon));
	}
	if (weaponAssoc) {
		weaponAssoc->m_BlendDelta = -8.0f;
		weaponAssoc->m_Flags &= ~ANIMATION_IS_PLAYING;
		weaponAssoc->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE;
		weaponAssoc->SetDeleteCallback(FinishedAttackCB, this);
	} else {
		ClearAttack();
	}
}

void
CPed::FinishedAttackCB(CAnimBlendAssociation *attackAssoc, void *arg)
{
	CAnimBlendAssociation *newAnim, *reloadAnimAssoc = nil;
	CPed *ped = (CPed*)arg;
	CWeaponInfo *currentWeapon = CWeaponInfo::GetWeaponInfo(ped->GetWeapon()->m_eWeaponType);

	if (ped->m_nPedState != PED_ATTACK) {
		if (ped->bIsDucking && ped->IsPedInControl()) {
			if (GetCrouchReloadAnim(currentWeapon)) {
				reloadAnimAssoc = RpAnimBlendClumpGetAssociation(ped->GetClump(), GetCrouchReloadAnim(currentWeapon));
			}
			if (GetCrouchFireAnim(currentWeapon) && attackAssoc) {
				if (attackAssoc->m_AnimId == GetCrouchFireAnim(currentWeapon) && !reloadAnimAssoc) {
					newAnim = CAnimManager::BlendAnimation(ped->GetClump(), ANIM_GROUP_DEFAULT, ANIM_ID_WEAPON_CROUCH, 8.0f);
					if (newAnim) {
						newAnim->SetCurrentTime(newAnim->m_BlendHier->m_fTotalTime);
						newAnim->m_Flags &= ~ANIMATION_IS_PLAYING;
					}
				}
			}
		}
	} else if (attackAssoc && attackAssoc->m_AnimId == ANIM_ID_GRENADE_WEAPON_START_THROW && currentWeapon->m_AnimToPlay == ANIM_GROUP_GRENADE) {
		AnimationId nextAnim = (!ped->IsPlayer() || ((CPlayerPed*)ped)->HaveTargetSelected()) && ped->IsPlayer() ?
			ANIM_ID_GRENADE_WEAPON_THROWU : ANIM_ID_GRENADE_WEAPON_THROW;
		attackAssoc->m_BlendDelta = -1000.0f;
		newAnim = CAnimManager::AddAnimation(ped->GetClump(), currentWeapon->m_AnimToPlay, nextAnim);
		if (!newAnim) {
			ReportUnavailableWeaponAnim(ped, nextAnim);
			ped->bIsAttacking = false;
			ped->RestorePreviousState();
			return;
		}
		newAnim->SetFinishCallback(FinishedAttackCB, ped);

	} else if (ped->bIsDucking && ped->bCrouchWhenShooting) {
		if (GetCrouchReloadAnim(currentWeapon)) {
			reloadAnimAssoc = RpAnimBlendClumpGetAssociation(ped->GetClump(), GetCrouchReloadAnim(currentWeapon));
		}
		if (GetCrouchFireAnim(currentWeapon) && attackAssoc) {
			if (attackAssoc->m_AnimId == GetCrouchFireAnim(currentWeapon) && !reloadAnimAssoc) {
				newAnim = CAnimManager::BlendAnimation(ped->GetClump(), ANIM_GROUP_DEFAULT, ANIM_ID_WEAPON_CROUCH, 8.0f);
				if (newAnim) {
					newAnim->SetCurrentTime(newAnim->m_BlendHier->m_fTotalTime);
					newAnim->m_Flags &= ~ANIMATION_IS_PLAYING;
				}
			}
		}

		if (!ped->bIsAttacking)
			ped->ClearAttack();

	} else if (GetSecondFireAnim(currentWeapon) && ped->bIsAttacking && currentWeapon->m_AnimToPlay != ANIM_GROUP_GRENADE) {
		AnimationId groundAnim = GetFireAnimGround(currentWeapon);
		CAnimBlendAssociation *groundAnimAssoc = RpAnimBlendClumpGetAssociation(ped->GetClump(), groundAnim);
		if (!(groundAnimAssoc && (groundAnimAssoc->m_BlendAmount > 0.95f || groundAnimAssoc->m_BlendDelta > 0.0f))) {
			AnimationId nextAnim = attackAssoc && attackAssoc->m_AnimId == ANIM_ID_FIGHT_1 ?
				GetSecondFireAnim(currentWeapon) : ANIM_ID_FIGHT_1;
			newAnim = CAnimManager::BlendAnimation(ped->GetClump(), currentWeapon->m_AnimToPlay, nextAnim, 8.0f);
			if (!newAnim) {
				ReportUnavailableWeaponAnim(ped, nextAnim);
				ped->bIsAttacking = false;
				ped->RestorePreviousState();
				return;
			}
			newAnim->SetFinishCallback(FinishedAttackCB, ped);
		}
	} else {
		if (attackAssoc && attackAssoc->m_AnimId == ANIM_ID_FIGHT_1 && currentWeapon->m_AnimToPlay == ANIM_GROUP_MELEE_1) {
			attackAssoc->m_BlendDelta = -8.0f;
			attackAssoc->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE;
			ped->ClearAttack();
			return;
		}
		if (attackAssoc) {
			if (currentWeapon->m_AnimToPlay == ANIM_GROUP_GRENADE) {
				if ((attackAssoc->m_AnimId == ANIM_ID_GRENADE_WEAPON_THROW || attackAssoc->m_AnimId == ANIM_ID_GRENADE_WEAPON_THROWU) && ped->GetWeapon()->m_nAmmoTotal > 0) {
					ped->RemoveWeaponModel(currentWeapon->m_nModelId);
					ped->AddWeaponModel(currentWeapon->m_nModelId);
				}
			}
		}

		if (!ped->bIsAttacking)
			ped->ClearAttack();
	}
}

void
CPed::FinishedReloadCB(CAnimBlendAssociation *reloadAssoc, void *arg)
{
	CPed *ped = (CPed*)arg;
	CWeaponInfo *weapon = CWeaponInfo::GetWeaponInfo(ped->GetWeapon()->m_eWeaponType);
	
	if (ped->DyingOrDead())
		return;

	if (ped->bIsDucking && ped->bCrouchWhenShooting) {
		CAnimBlendAssociation *crouchFireAssoc = nil;
		if (weapon->IsFlagSet(WEAPONFLAG_CROUCHFIRE)) {
			crouchFireAssoc = RpAnimBlendClumpGetAssociation(ped->GetClump(), GetCrouchFireAnim(weapon));
		}
		if (weapon->IsFlagSet(WEAPONFLAG_RELOAD) && reloadAssoc) {
			if (reloadAssoc->m_AnimId == GetCrouchReloadAnim(weapon) && !crouchFireAssoc) {
				CAnimBlendAssociation *crouchAssoc = CAnimManager::BlendAnimation(ped->GetClump(), ANIM_GROUP_DEFAULT, ANIM_ID_WEAPON_CROUCH, 8.0f);
				if (crouchAssoc) {
					crouchAssoc->SetCurrentTime(crouchAssoc->m_BlendHier->m_fTotalTime);
					crouchAssoc->m_Flags &= ~ANIMATION_IS_PLAYING;
				}
			}
		}
	} else if (weapon->IsFlagSet(WEAPONFLAG_RELOAD_LOOP2START) && ped->bIsAttacking) {
		CAnimBlendAssociation *fireAssoc =
			CAnimManager::BlendAnimation(ped->GetClump(), weapon->m_AnimToPlay, GetPrimaryFireAnim(weapon), 8.0f);
		if (!fireAssoc) {
			ReportUnavailableWeaponAnim(ped, GetPrimaryFireAnim(weapon));
			ped->bIsAttacking = false;
			ped->RestorePreviousState();
			return;
		}
		fireAssoc->SetFinishCallback(FinishedAttackCB, ped);
		fireAssoc->m_Flags |= ANIMATION_IS_PLAYING;
		if (fireAssoc->m_CurrentTime == reloadAssoc->m_BlendHier->m_fTotalTime)
			fireAssoc->SetCurrentTime(Max(weapon->m_fAnimLoopStart - 0.04f, 0.0f));
		else if (fireAssoc->m_CurrentTime < weapon->m_fAnimLoopStart)
			fireAssoc->SetCurrentTime(Max(weapon->m_fAnimLoopStart - 0.04f, 0.0f));
	}
}

uint8
CPed::CheckForPointBlankPeds(CPed *pedToVerify)
{
	float pbDistance = 1.1f;
	if (GetWeapon()->IsType2Handed())
		pbDistance = 1.6f;

	for (int i = 0; i < m_numNearPeds; i++)	{
		CPed *nearPed = m_nearPeds[i];

		if (!pedToVerify || pedToVerify == nearPed) {

			CVector diff = nearPed->GetPosition() - GetPosition();
			if (diff.MagnitudeSqr() < SQR(pbDistance)) {

				float neededAngle = CGeneral::GetRadianAngleBetweenPoints(
					nearPed->GetPosition().x, nearPed->GetPosition().y,
					GetPosition().x, GetPosition().y);
				neededAngle = CGeneral::LimitRadianAngle(neededAngle);
				m_fRotationCur = CGeneral::LimitRadianAngle(m_fRotationCur);

				float neededTurn = Abs(neededAngle - m_fRotationCur);

				if (neededTurn > PI)
					neededTurn = 2*PI - neededTurn;

				PedState nearPedState = nearPed->m_nPedState;

				if (nearPedState == PED_FALL || nearPedState == PED_GETUP || nearPedState == PED_DIE || nearPedState == PED_DEAD || nearPedState == PED_EVADE_DIVE)
					return NO_POINT_BLANK_PED;

				if (neededTurn < CAN_SEE_ENTITY_ANGLE_THRESHOLD) {
					if (pedToVerify == nearPed)
						return POINT_BLANK_FOR_WANTED_PED;
					else
						return POINT_BLANK_FOR_SOMEONE_ELSE;
				}
			}
		}
	}
	return NO_POINT_BLANK_PED;
}

void
CPed::Attack(void)
{
	CAnimBlendAssociation *weaponAnimAssoc;
	int32 weaponAnim;
	float weaponAnimTime;
	float animLoopEnd;
	CWeaponInfo *ourWeapon;
	bool attackShouldContinue;
	CAnimBlendAssociation *reloadAnimAssoc;
	CAnimBlendAssociation *throwAssoc;
	float delayBetweenAnimAndFire;
	float animLoopStart;
	CVector firePos;

	ourWeapon = CWeaponInfo::GetWeaponInfo(GetWeapon()->m_eWeaponType);
	weaponAnimAssoc = nil;
	attackShouldContinue = !!bIsAttacking;
	reloadAnimAssoc = nil;
	throwAssoc = nil;
	animLoopStart = ourWeapon->m_fAnimLoopStart;
	animLoopEnd = ourWeapon->m_fAnimLoopEnd;
	delayBetweenAnimAndFire = ourWeapon->m_fAnimFrameFire;
	weaponAnim = ourWeapon->m_AnimToPlay;

	if (bIsDucking) {
		if(GetCrouchFireAnim(ourWeapon) && bCrouchWhenShooting) {
			weaponAnimAssoc = RpAnimBlendClumpGetAssociation(GetClump(), GetCrouchFireAnim(ourWeapon));
			if (weaponAnimAssoc) {
				animLoopStart = ourWeapon->m_fAnim2LoopStart;
				animLoopEnd = ourWeapon->m_fAnim2LoopEnd;
				delayBetweenAnimAndFire = ourWeapon->m_fAnim2FrameFire;
			}
		}
	} else if (m_nPedType == PEDTYPE_COP && Get3rdFireAnim(ourWeapon)){
		weaponAnimAssoc = RpAnimBlendClumpGetAssociation(GetClump(), Get3rdFireAnim(ourWeapon));
		if (weaponAnimAssoc) {
			animLoopStart = 11.f/30.f;
			animLoopEnd = 19.f/30.f;
			delayBetweenAnimAndFire = 14.f/30.f;
		}
	} else {
		weaponAnimAssoc = RpAnimBlendClumpGetAssociation(GetClump(), GetPrimaryFireAnim(ourWeapon));
	}

	if (GetReloadAnim(ourWeapon)) {
		reloadAnimAssoc = RpAnimBlendClumpGetAssociation(GetClump(), GetReloadAnim(ourWeapon));
	}

	if (GetCrouchReloadAnim(ourWeapon) && !reloadAnimAssoc)  {
		reloadAnimAssoc = RpAnimBlendClumpGetAssociation(GetClump(), GetCrouchReloadAnim(ourWeapon));
	}

	if ( reloadAnimAssoc && reloadAnimAssoc->IsPlaying() ) {
		if (!IsPlayer() || ((CPlayerPed*)this)->HaveTargetSelected())
			ClearAttack();
		return;
	}

	if ( reloadAnimAssoc ) {
		reloadAnimAssoc->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE;
		if ( reloadAnimAssoc->m_BlendDelta >= 0.0f )
			reloadAnimAssoc->m_BlendDelta = -8.0f;
	}

	if (GetThrowAnim(ourWeapon))  {
		throwAssoc = RpAnimBlendClumpGetAssociation(GetClump(), GetThrowAnim(ourWeapon));
	}

	if ( CTimer::GetTimeInMilliseconds() < m_shootTimer )
		attackShouldContinue = true;

	bool meleeAttackStarted = false;
	if ( !weaponAnimAssoc ) {
		if (GetMeleeStartAnim(ourWeapon)) {
			weaponAnimAssoc = RpAnimBlendClumpGetAssociation(GetClump(), GetMeleeStartAnim(ourWeapon));
			if ( weaponAnimAssoc ) {
				if ( IsPlayer() )
					meleeAttackStarted = true;

				switch ( ourWeapon->m_AnimToPlay ) {
					case ANIM_GROUP_MELEE_1:
					case ANIM_GROUP_KNIFE_1:
					case ANIM_GROUP_BBBAT_1:
					case ANIM_GROUP_GCLUB_1:
					case ANIM_GROUP_CSAW_1:
						delayBetweenAnimAndFire = 0.2f;
						animLoopStart = 0.1f;
						break;
					default:
						break;
				}
				animLoopEnd = 99.9f;
			}
		}
	}
	if (!weaponAnimAssoc) {
		if (GetSecondFireAnim(ourWeapon)) {
			weaponAnimAssoc = RpAnimBlendClumpGetAssociation(GetClump(), GetSecondFireAnim(ourWeapon));
			if (weaponAnimAssoc) {
				animLoopStart = ourWeapon->m_fAnim2LoopStart;
				animLoopEnd = ourWeapon->m_fAnim2LoopEnd;
				delayBetweenAnimAndFire = ourWeapon->m_fAnim2FrameFire;
			}
		}
	}
	if (!weaponAnimAssoc) {
		weaponAnimAssoc = RpAnimBlendClumpGetAssociation(GetClump(), GetFireAnimGround(ourWeapon));
		if (weaponAnimAssoc) {
			animLoopStart = ourWeapon->m_fAnim2LoopStart;
			animLoopEnd = ourWeapon->m_fAnim2LoopEnd;
			delayBetweenAnimAndFire = ourWeapon->m_fAnim2FrameFire;
		}
	}

	if (!weaponAnimAssoc) {
		if (!throwAssoc) {
			if (attackShouldContinue) {
				if (ourWeapon->m_eWeaponFire != WEAPON_FIRE_PROJECTILE || !IsPlayer() || ((CPlayerPed*)this)->HaveTargetSelected()) {
					AnimationId nextFireAnim;
					if (bCrouchWhenShooting && bIsDucking && GetCrouchFireAnim(ourWeapon)) {
						nextFireAnim = GetCrouchFireAnim(ourWeapon);
					} else if(GetSecondFireAnim(ourWeapon) && CGeneral::GetRandomNumber() & 1){
						nextFireAnim = GetSecondFireAnim(ourWeapon);
					} else if(!CGame::nastyGame || ourWeapon->m_eWeaponFire != WEAPON_FIRE_MELEE ||
					          !GetFireAnimGround(ourWeapon, false) ||
						 CheckForPedsOnGroundToAttack(this, nil) < PED_ON_THE_FLOOR) {
						nextFireAnim = GetFireAnimNotDucking(ourWeapon);
					} else {
						nextFireAnim = GetFireAnimGround(ourWeapon, false);
					}
					weaponAnimAssoc = CAnimManager::BlendAnimation(GetClump(), ourWeapon->m_AnimToPlay, nextFireAnim, 8.0f);
					if (!weaponAnimAssoc) {
						ReportUnavailableWeaponAnim(this, nextFireAnim);
						bIsAttacking = false;
						RestorePreviousState();
						return;
					}
					weaponAnimAssoc->SetFinishCallback(FinishedAttackCB, this);
					weaponAnimAssoc->m_Flags |= ANIMATION_IS_PLAYING;

					if (weaponAnimAssoc->m_CurrentTime == weaponAnimAssoc->m_BlendHier->m_fTotalTime)
						weaponAnimAssoc->SetCurrentTime(0.0f);

					if (IsPlayer()) {
						((CPlayerPed*)this)->AttackButtonCounter() = 0.0f;
						((CPlayerPed*)this)->HaveTargetSelected() = false;
					}
				}
			} else
				FinishedAttackCB(nil, this);

		}
		return;
	}

	if (meleeAttackStarted && IsPlayer()) {
		if (((CPlayerPed*)this)->HaveTargetSelected() || ((CPlayerPed*)this)->MoveSpeed() < 0.5f) {
			weaponAnimAssoc->m_Flags |= ANIMATION_IS_PLAYING;
		} else {
			if (weaponAnimAssoc->m_CurrentTime > animLoopStart && weaponAnimAssoc->m_CurrentTime - weaponAnimAssoc->m_TimeStep <= animLoopStart)
				weaponAnimAssoc->m_Flags &= ~ANIMATION_IS_PLAYING;
		}
	}

	float animStart = animLoopStart * 0.4f;
	weaponAnimTime = weaponAnimAssoc->m_CurrentTime;
	if (weaponAnimTime > animStart && weaponAnimTime - weaponAnimAssoc->m_TimeStep <= animStart) {
		if (!bIsDucking && !GetFireAnimNotDucking(ourWeapon) && ourWeapon->IsFlagSet(WEAPONFLAG_CANAIM_WITHARM))
			m_pedIK.m_flags |= CPedIK::AIMS_WITH_ARM;
		else
#ifdef FREE_CAM
			if (!IsPlayer() || !((CPlayerPed*)this)->m_bFreeAimActive)
#endif
			m_pedIK.m_flags &= ~CPedIK::AIMS_WITH_ARM;
	}

	if (GetWeapon()->m_eWeaponType != WEAPONTYPE_CHAINSAW
		|| !meleeAttackStarted && delayBetweenAnimAndFire - 0.5f >= weaponAnimAssoc->m_CurrentTime
		|| weaponAnimAssoc->m_CurrentTime - weaponAnimAssoc->m_TimeStep > delayBetweenAnimAndFire) {

		if (GetWeapon()->m_eWeaponType == WEAPONTYPE_CHAINSAW) {
			DMAudio.PlayOneShot(m_audioEntityId, SOUND_WEAPON_CHAINSAW_IDLE, 0.0f);
		} else if (weaponAnimTime <= delayBetweenAnimAndFire || weaponAnimTime - weaponAnimAssoc->m_TimeStep > delayBetweenAnimAndFire || !weaponAnimAssoc->IsPlaying()) {
			if (weaponAnimAssoc->m_Speed < 1.0f)
				weaponAnimAssoc->m_Speed = 1.0f;

		} else {
			firePos = ourWeapon->m_vecFireOffset;

			if(ourWeapon->m_AnimToPlay != ANIM_GROUP_BBBAT_1 && ourWeapon->m_AnimToPlay != ANIM_GROUP_GCLUB_1) {
				if (ourWeapon->m_eWeaponFire != WEAPON_FIRE_MELEE) {
					TransformToNode(firePos, (weaponAnimAssoc->m_AnimId == ANIM_ID_FIGHT_2 && ourWeapon->m_AnimToPlay == ANIM_GROUP_MELEE_1) ? PED_NODE_RIGHT_FOOT : PED_NODE_RIGHT_HAND);
				} else {
					firePos = GetMatrix() * firePos;
				}
			} else {
				if (weaponAnimAssoc->m_AnimId == ANIM_ID_FIGHT_2)
					firePos.z = 0.7f * ourWeapon->m_fRadius - 1.0f;

				firePos = GetMatrix() * firePos;
			}
			
			GetWeapon()->Fire(this, &firePos);

			if (GetWeapon()->m_eWeaponType == WEAPONTYPE_MOLOTOV || GetWeapon()->m_eWeaponType == WEAPONTYPE_GRENADE || GetWeapon()->m_eWeaponType == WEAPONTYPE_DETONATOR_GRENADE ||
				GetWeapon()->m_eWeaponType == WEAPONTYPE_TEARGAS) {
				RemoveWeaponModel(CWeaponInfo::GetWeaponInfo(GetWeapon()->m_eWeaponType)->m_nModelId);
			}
			if (GetWeapon()->m_nAmmoTotal == 0 && ourWeapon->m_eWeaponFire != WEAPON_FIRE_MELEE && FindPlayerPed() != this) {
				SelectGunIfArmed();
			}

			if (GetWeapon()->m_eWeaponState == WEAPONSTATE_MELEE_MADECONTACT) {
				int damagerType = ENTITY_TYPE_NOTHING;
				if (m_pDamageEntity && (m_fDamageImpulse == 0.0f || !m_pDamageEntity->IsBuilding())) {
					damagerType = m_pDamageEntity->GetType();
				}
				switch (ourWeapon->m_AnimToPlay) {
					case ANIM_GROUP_MELEE_1:
						if (weaponAnimAssoc->m_AnimId == ANIM_ID_FIGHT_1 || weaponAnimAssoc->m_AnimId == ANIM_ID_FIGHT_3) 
							DMAudio.PlayOneShot(m_audioEntityId, SOUND_FIGHT_46, (damagerType | (GetWeapon()->m_eWeaponType << 8)));
						break;
					case ANIM_GROUP_KNIFE_1:
					case ANIM_GROUP_BBBAT_1:
					case ANIM_GROUP_GCLUB_1:
					case ANIM_GROUP_CSAW_1:
						DMAudio.PlayOneShot(m_audioEntityId, SOUND_WEAPON_BAT_ATTACK, (damagerType | (GetWeapon()->m_eWeaponType << 8)));
						break;
					default:
						break;
				}

				if (bIsAttacking || CTimer::GetTimeInMilliseconds() < m_shootTimer) {
					weaponAnimAssoc->m_nCallbackType = ANIM_BLEND_CALLBACK_NONE;
				}
			}

			attackShouldContinue = false;
		}
	} else {
		CVector firePos = ourWeapon->m_vecFireOffset;

		if (weaponAnimAssoc->m_AnimId == ANIM_ID_FIGHT_2)
			firePos.z = 0.7f * ourWeapon->m_fRadius - 1.0f;

		firePos = GetMatrix() * firePos;
		GetWeapon()->Fire(this, &firePos);
		if (GetWeapon()->m_eWeaponState == WEAPONSTATE_MELEE_MADECONTACT) {
			int damagerType = ENTITY_TYPE_PED;
			if (m_pDamageEntity)
				damagerType = m_pDamageEntity->GetType();

			DMAudio.PlayOneShot(m_audioEntityId, SOUND_WEAPON_CHAINSAW_MADECONTACT, (float)damagerType);
			if (IsPlayer()) {
				CPad::GetPad(0)->StartShake(240, 180);
			}
		} else {
			DMAudio.PlayOneShot(m_audioEntityId, SOUND_WEAPON_CHAINSAW_ATTACK, 0.0f);
			if (IsPlayer()) {
				CPad::GetPad(0)->StartShake(240, 90);
			}
		}
		attackShouldContinue = false;
	}

	if (ourWeapon->m_eWeaponFire == WEAPON_FIRE_INSTANT_HIT && ourWeapon->m_AnimToPlay == ANIM_GROUP_SAWNOFF) {
		weaponAnimTime = weaponAnimAssoc->m_CurrentTime;

		if (weaponAnimTime > 1.0f && weaponAnimTime - weaponAnimAssoc->m_TimeStep <= 1.0f && weaponAnimAssoc->IsPlaying()) {
			firePos = ourWeapon->m_vecFireOffset;
			TransformToNode(firePos, PED_NODE_RIGHT_HAND);

			CVector gunshellPos(
				firePos.x - 0.6f * GetForward().x,
				firePos.y - 0.6f * GetForward().y,
				firePos.z - 0.15f * GetUp().z
			);

			CVector2D gunshellRot(
				GetRight().x,
				GetRight().y
			);

			gunshellRot.Normalise();
			GetWeapon()->AddGunshell(this, gunshellPos, gunshellRot, 0.025f);
		}
	}

	if (IsPlayer()) {
		if (GetWeapon()->m_eWeaponType == WEAPONTYPE_BASEBALLBAT || GetWeapon()->m_eWeaponType == WEAPONTYPE_GOLFCLUB || GetWeapon()->m_eWeaponType == WEAPONTYPE_KATANA) {
			float loopEndWithDelay = animLoopEnd;
			if (loopEndWithDelay >= 98.0f)
				loopEndWithDelay = (14.0f / 30.0f) + delayBetweenAnimAndFire;
			if (weaponAnimAssoc->m_Flags & ANIMATION_IS_PLAYING) {
				if (weaponAnimAssoc->m_CurrentTime >= animLoopStart && weaponAnimAssoc->m_CurrentTime <= loopEndWithDelay)
					CSpecialFX::AddWeaponStreak(GetWeapon()->m_eWeaponType);
			}
		}
	}

	// Anim breakout on running
	if (IsPlayer()) {
		if (CPad::GetPad(0)->GetSprint()) {
			if (!attackShouldContinue && weaponAnimAssoc->m_CurrentTime > ourWeapon->m_fAnimBreakout) {
				weaponAnimAssoc->m_BlendDelta = -4.0f;
				FinishedAttackCB(nil, this);
				return;
			}
		}
	}

	weaponAnimTime = weaponAnimAssoc->m_CurrentTime;

	// Anim loop end, either start the loop again or finish the attack
	if (weaponAnimTime > animLoopEnd || !weaponAnimAssoc->IsPlaying() && ourWeapon->m_eWeaponFire != WEAPON_FIRE_PROJECTILE) {
		if (GetWeapon()->m_eWeaponState == WEAPONSTATE_RELOADING) {
			if (GetReloadAnim(ourWeapon) && !reloadAnimAssoc) {
				if (!CWorld::Players[CWorld::PlayerInFocus].m_bFastReload) {
					AnimationId reloadAnim = bIsDucking && GetCrouchReloadAnim(ourWeapon) ?
						GetCrouchReloadAnim(ourWeapon) : GetReloadAnim(ourWeapon);
					CAnimBlendAssociation *newReloadAssoc = CAnimManager::BlendAnimation(
						GetClump(), ourWeapon->m_AnimToPlay, reloadAnim, 8.0f);
					if (newReloadAssoc)
						newReloadAssoc->SetFinishCallback(FinishedReloadCB, this);
					else {
						ReportUnavailableWeaponAnim(this, reloadAnim);
						bIsAttacking = false;
						bIsPointingGunAt = false;
						RestorePreviousState();
						return;
					}
				}
				ClearLookFlag();
				ClearAimFlag();
				bIsAttacking = false;
				bIsPointingGunAt = false;
				m_shootTimer = CTimer::GetTimeInMilliseconds();
				DMAudio.PlayOneShot(m_audioEntityId, SOUND_WEAPON_AK47_BULLET_ECHO, GetWeapon()->m_eWeaponType);
				return;
			}
		}
		if (weaponAnimTime - 2.0f * weaponAnimAssoc->m_TimeStep <= animLoopEnd
			&& (bIsAttacking || CTimer::GetTimeInMilliseconds() < m_shootTimer)
			&& (GetWeapon()->m_eWeaponState != WEAPONSTATE_RELOADING
				|| GetWeapon()->m_eWeaponType == WEAPONTYPE_MINIGUN)) {

			PedOnGroundState pedOnGroundState;
			if (ourWeapon->m_eWeaponFire == WEAPON_FIRE_MELEE &&
				(CGame::nastyGame && ((pedOnGroundState = CheckForPedsOnGroundToAttack(this, nil)) > PED_IN_FRONT_OF_ATTACKER)
				|| GetWeapon()->m_eWeaponType == WEAPONTYPE_BASEBALLBAT && pedOnGroundState == NO_PED && bIsStanding && m_pCurSurface && m_pCurSurface->IsVehicle())) {

				AnimationId fireAnim = GetFireAnimGround(ourWeapon, false);
				if (weaponAnimAssoc->m_AnimId == fireAnim)
					weaponAnimAssoc->SetCurrentTime(0.1f);
				else {
					if (GetFireAnimGround(ourWeapon, false)) {
						weaponAnimAssoc = CAnimManager::BlendAnimation(GetClump(), ourWeapon->m_AnimToPlay, fireAnim, 8.0f);
					} else {
						fireAnim = ANIM_ID_FIGHT_1;
						weaponAnimAssoc = CAnimManager::BlendAnimation(GetClump(), ANIM_GROUP_DEFAULT, fireAnim, 8.0f);
					}
				}
				if (!weaponAnimAssoc) {
					ReportUnavailableWeaponAnim(this, fireAnim,
						fireAnim == ANIM_ID_FIGHT_1 ? ANIM_GROUP_DEFAULT : ourWeapon->m_AnimToPlay);
					bIsAttacking = false;
					RestorePreviousState();
					return;
				}
				weaponAnimAssoc->SetFinishCallback(FinishedAttackCB, this);
			} else if (GetSecondFireAnim(ourWeapon)) {
				AnimationId nextFireAnim = weaponAnimAssoc->m_AnimId == GetSecondFireAnim(ourWeapon) ?
					ANIM_ID_FIRE : GetSecondFireAnim(ourWeapon);
				weaponAnimAssoc = CAnimManager::BlendAnimation(GetClump(), ourWeapon->m_AnimToPlay, nextFireAnim, 8.0f);
				if (!weaponAnimAssoc) {
					ReportUnavailableWeaponAnim(this, nextFireAnim);
					bIsAttacking = false;
					RestorePreviousState();
					return;
				}
				weaponAnimAssoc->SetFinishCallback(FinishedAttackCB, this);
			} else {
				weaponAnimAssoc->SetCurrentTime(animLoopStart);
				weaponAnimAssoc->m_Flags |= ANIMATION_IS_PLAYING;
			}
		} else if (IsPlayer() && m_pPointGunAt && bIsAimingGun && GetWeapon()->m_eWeaponState != WEAPONSTATE_RELOADING) {
			weaponAnimAssoc->SetCurrentTime(animLoopEnd);
			weaponAnimAssoc->m_Flags &= ~ANIMATION_IS_PLAYING;
			SetPointGunAt(m_pPointGunAt);
#ifdef FREE_CAM
		} else if (IsPlayer() && ((CPlayerPed*)this)->m_bFreeAimActive && GetWeapon()->m_eWeaponState != WEAPONSTATE_RELOADING) {
			float limitedCam = CGeneral::LimitRadianAngle(-TheCamera.Orientation);
			SetLookFlag(limitedCam, true, true);
			SetAimFlag(limitedCam);
			SetLookTimer(INT32_MAX);
			SetPointGunAt(nil);
			((CPlayerPed*)this)->FPSMoveHeading() = TheCamera.Find3rdPersonQuickAimPitch();
#endif
		} else {
			ClearAimFlag();

			// Echoes of bullets, at the end of the attack. (Bug: doesn't play while reloading)
			if (weaponAnimAssoc->m_CurrentTime - weaponAnimAssoc->m_TimeStep < animLoopEnd) 
				DMAudio.PlayOneShot(m_audioEntityId, SOUND_WEAPON_AK47_BULLET_ECHO, GetWeapon()->m_eWeaponType);

			// Fun fact: removing this part leds to reloading flamethrower
			if (GetWeapon()->m_eWeaponType == WEAPONTYPE_FLAMETHROWER && weaponAnimAssoc->IsPlaying()) {
				weaponAnimAssoc->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE;
				weaponAnimAssoc->m_Flags &= ~ANIMATION_IS_PLAYING;
				weaponAnimAssoc->m_BlendDelta = -4.0f;
			}
		}
	}

	if (weaponAnimAssoc->m_CurrentTime > delayBetweenAnimAndFire)
		attackShouldContinue = false;

	bIsAttacking = attackShouldContinue;
}

void
CPed::StartFightAttack(uint8 buttonPressure)
{
	if (!IsPedInControl() || (m_attackTimer > CTimer::GetTimeInMilliseconds() && buttonPressure != 0))
		return;

	if (m_nPedState == PED_FIGHT) {
		m_fightButtonPressure = buttonPressure;
		return;
	}

	if (m_nPedState != PED_AIMGUN)
		SetStoredState();

	if (m_nWaitState != WAITSTATE_FALSE) {
		ClearWaitState();
		RestoreHeadingRate();
	}

	CAnimBlendAssociation* animAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_RUN_STOP);
	if (!animAssoc)
		animAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_RUN_STOPR);

	if (animAssoc) {
		RestoreHeadingRate();
	}
	SetMoveState(PEDMOVE_NONE);
	m_nStoredMoveState = PEDMOVE_NONE;
	bool fightWithWeapon = false;
	CAnimBlendAssociation *fightIdleAssoc;

	CWeaponInfo* weaponInfo = CWeaponInfo::GetWeaponInfo(GetWeapon()->m_eWeaponType);
	if (GetWeapon()->m_eWeaponType != WEAPONTYPE_UNARMED) {
		if (GetFightIdleWithMeleeAnim(weaponInfo)) {
			fightIdleAssoc = CAnimManager::BlendAnimation(GetClump(), weaponInfo->m_AnimToPlay, GetFightIdleWithMeleeAnim(weaponInfo), 1000.0f);
			fightWithWeapon = true;
		} else {
			fightIdleAssoc = CAnimManager::BlendAnimation(GetClump(), ANIM_GROUP_MELEE_1, ANIM_ID_FIGHT_IDLE, 1000.0f);
		}
	} else {
		fightIdleAssoc = CAnimManager::BlendAnimation(GetClump(), ANIM_GROUP_MELEE_1, ANIM_ID_FIGHT_IDLE, 1000.0f);
	}
	m_lastFightMove = FIGHTMOVE_IDLE;
	m_curFightMove = IsPlayer() ? ChooseAttackPlayer(buttonPressure, fightWithWeapon) : ChooseAttackAI(buttonPressure, fightWithWeapon);

	SetPedState(PED_FIGHT);
	m_fightButtonPressure = 0;

	if (m_curFightMove > FIGHTMOVE_NULL && m_curFightMove != FIGHTMOVE_IDLE) {
		animAssoc = CAnimManager::BlendAnimation(GetClump(), m_curFightMove < FIGHTMOVE_MELEE1 ? tFightMoves[m_curFightMove].animGroup : weaponInfo->m_AnimToPlay,
			tFightMoves[m_curFightMove].animId, 8.0f);
		if(!animAssoc){ EndFight(ENDFIGHT_FAST); return; }

		if (weaponInfo->m_AnimToPlay == ANIM_GROUP_KNIFE_1 && m_curFightMove >= FIGHTMOVE_MELEE1) {
			switch (GetWeapon()->m_eWeaponType) {
				case WEAPONTYPE_SCREWDRIVER:
				case WEAPONTYPE_KNIFE:
					animAssoc->m_Speed = 1.05f;
					break;
				case WEAPONTYPE_GOLFCLUB:
				case WEAPONTYPE_NIGHTSTICK:
				case WEAPONTYPE_BASEBALLBAT:
				case WEAPONTYPE_HAMMER:
				case WEAPONTYPE_KATANA:
					animAssoc->m_Speed = 0.8f;
					break;
				case WEAPONTYPE_CLEAVER:
				case WEAPONTYPE_MACHETE:
					animAssoc->m_Speed = 0.9f;
					break;
			}
		} else {
			// The strikes play slower than the clip was authored because the
			// wind-up is what the player has to read to time a block. That
			// makes the whole move arrive late, though, and the punch lands
			// after the arm has already started to come back. Keep the read
			// time but bring the strike itself forward.
			if (m_curFightMove == FIGHTMOVE_BACKKICK)
				animAssoc->m_Speed = 1.15f;
			else
				animAssoc->m_Speed = 0.95f;
		}
		if (IsPlayer())
			animAssoc->SetCurrentTime(0.08f);

		animAssoc->SetFinishCallback(FinishFightMoveCB, this);
	} else {
		m_nWaitTimer = CTimer::GetTimeInMilliseconds() + 2000;
	}

	m_fightState = FIGHTSTATE_NO_MOVE;
	m_takeAStepAfterAttack = false;
	bIsAttacking = true;

	if (IsPlayer())
		nPlayerInComboMove = 0;
}

void
CPed::StartFightDefend(uint8 direction, uint8 hitLevel, uint8 unk)
{
	if (m_nPedState == PED_DEAD) {
		if (CGame::nastyGame) {
			if (hitLevel == HITLEVEL_GROUND) {
				CAnimBlendAssociation *floorHitAssoc;
				if (RpAnimBlendClumpGetFirstAssociation(GetClump(), ANIMATION_IS_FRONT)) {
					floorHitAssoc = CAnimManager::BlendAnimation(GetClump(), ANIM_GROUP_DEFAULT, ANIM_ID_FLOOR_HIT_F, 8.0f);
				} else {
					floorHitAssoc = CAnimManager::BlendAnimation(GetClump(), ANIM_GROUP_DEFAULT, tFightMoves[FIGHTMOVE_HITONFLOOR].animId, 8.0f);
				}
				if (floorHitAssoc) {
					floorHitAssoc->SetCurrentTime(0.0f);
					floorHitAssoc->m_Flags |= ANIMATION_IS_PLAYING;
					floorHitAssoc->m_Flags &= ~ANIMATION_IS_FINISH_AUTO_REMOVE;
				}
			}
			if (CGame::nastyGame) {
				CVector headPos = GetNodePosition(PED_NODE_HEAD);
				for(int i = 0; i < 4; ++i) {
					CVector bloodDir(0.0f, 0.0f, 0.1f);
					CVector bloodPos = headPos - 0.2f * GetForward();
					CParticle::AddParticle(PARTICLE_BLOOD, bloodPos, bloodDir, nil, 0.0f, 0, 0, 0, 0);
				}
			}
		}
	} else if (m_nPedState == PED_FALL) {
		if (hitLevel == HITLEVEL_GROUND && !IsPedHeadAbovePos(-0.3f)) {
			CAnimBlendAssociation *floorHitAssoc = RpAnimBlendClumpGetFirstAssociation(GetClump(), ANIMATION_IS_FRONT) ?
				CAnimManager::BlendAnimation(GetClump(), ANIM_GROUP_DEFAULT, ANIM_ID_FLOOR_HIT_F, 8.0f) :
				CAnimManager::BlendAnimation(GetClump(), ANIM_GROUP_DEFAULT, ANIM_ID_FLOOR_HIT, 8.0f);
			if (floorHitAssoc) {
				floorHitAssoc->m_Flags &= ~ANIMATION_IS_FINISH_AUTO_REMOVE;
				floorHitAssoc->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE;
			}
		}
	} else if (IsPedInControl()) {
		if ((IsPlayer() && m_nPedState != PED_FIGHT && ((CPlayerPed*)this)->MoveSpeed() > 1.0f)
			|| (!IsPlayer() && m_objective == OBJECTIVE_FLEE_CHAR_ON_FOOT_TILL_SAFE)) {

			if (hitLevel != HITLEVEL_HIGH && hitLevel != HITLEVEL_LOW || (IsPlayer() || CGeneral::GetRandomNumber() & 1) && CGeneral::GetRandomNumber() & 7) {
				if (IsPlayer() || CGeneral::GetRandomNumber() & 1) {
					AnimationId shotAnim;
					switch (direction) {
						case 1:
							shotAnim = ANIM_ID_SHOT_LEFTP;
							break;
						case 2:
							shotAnim = ANIM_ID_SHOT_PARTIAL;
							break;
						case 3:
							shotAnim = ANIM_ID_SHOT_RIGHTP;
							break;
						default:
							shotAnim = ANIM_ID_SHOT_PARTIAL;
							break;
					}
					CAnimBlendAssociation *shotAssoc = BlendDamageReaction(GetClump(), shotAnim);
					if(shotAssoc){
						shotAssoc->SetCurrentTime(0.0f);
						shotAssoc->m_Flags |= ANIMATION_IS_PLAYING;
						shotAssoc->m_Flags |= ANIMATION_IS_FINISH_AUTO_REMOVE;
					}
				} else {
					int time = CGeneral::GetRandomNumberInRange(1000, 3000);
					SetWaitState(WAITSTATE_PLAYANIM_DUCK, &time);
				}
			} else {
				bool fall = true;
				AnimationId hitAnim;
				switch (direction) {
					case 1:
						hitAnim = ANIM_ID_KO_SPIN_R;
						break;
					case 2:
						if (CGeneral::GetRandomNumber() & 1) {
							fall = false;
							hitAnim = ANIM_ID_HIT_BACK;
						} else	{
							hitAnim = ANIM_ID_KO_SKID_BACK;
						}
						break;
					case 3:
						hitAnim = ANIM_ID_KO_SPIN_L;
						break;
					default:
						if (hitLevel == HITLEVEL_LOW) {
							hitAnim = ANIM_ID_KO_SHOT_STOM;
						} else if (CGeneral::GetRandomNumber() & 1) {
							fall = false;
							hitAnim = ANIM_ID_HIT_WALK;
						} else if (CGeneral::GetRandomNumber() & 1) {
							fall = false;
							hitAnim = ANIM_ID_KO_SHOT_FACE;
						} else {
							hitAnim = ANIM_ID_KO_SHOT_FACE;
						}
						break;
				}
				if (fall) {
					SetFall(500, hitAnim, false);
				} else {
					CAnimBlendAssociation *hitAssoc = BlendDamageReaction(GetClump(), hitAnim);
					if(hitAssoc){
						hitAssoc->SetCurrentTime(0.0f);
						hitAssoc->m_Flags |= ANIMATION_IS_PLAYING;
						hitAssoc->m_Flags |= ANIMATION_IS_FINISH_AUTO_REMOVE;
					}
				}
			}
			Say(SOUND_PED_DEFEND);
		} else {
			Say(SOUND_PED_DEFEND);
			switch (hitLevel) {
				case HITLEVEL_GROUND:
					m_curFightMove = FIGHTMOVE_HITONFLOOR;
					break;
				case HITLEVEL_LOW:
					if (direction == 2 && (!IsPlayer() || ((CGeneral::GetRandomNumber() & 1) && m_fHealth < 30.0f))) {
						SetFall(1000, ANIM_ID_KO_SKID_BACK, false);
						Say(SOUND_PED_DEFEND);
						return;
					} else if (direction != 2 && !IsPlayer() && (CGeneral::GetRandomNumber() & 1) && m_fHealth < 30.0f) {
						SetFall(1000, ANIM_ID_KO_SHOT_STOM, false);
						Say(SOUND_PED_DEFEND);
						return;
					}	
					m_curFightMove = FIGHTMOVE_HITBODY;
					break;
				case HITLEVEL_HIGH:
					switch (direction) {
						case 1:
							m_curFightMove = FIGHTMOVE_HITLEFT;
							break;
						case 2:
							m_curFightMove = FIGHTMOVE_HITBACK;
							break;
						case 3:
							m_curFightMove = FIGHTMOVE_HITRIGHT;
							break;
						default:
							if (unk <= 5)
								m_curFightMove = FIGHTMOVE_HITHEAD;
							else
								m_curFightMove = FIGHTMOVE_HITBIGSTEP;
							break;
					}
					break;
				default:
					switch (direction) {
						case 1:
							m_curFightMove = FIGHTMOVE_HITLEFT;
							break;
						case 2:
							m_curFightMove = FIGHTMOVE_HITBACK;
							break;
						case 3:
							m_curFightMove = FIGHTMOVE_HITRIGHT;
							break;
						default:
							if (unk <= 5)
								m_curFightMove = FIGHTMOVE_HITCHEST;
							else
								m_curFightMove = FIGHTMOVE_HITBIGSTEP;
							break;
					}
					break;
			}
			if (m_nPedState == PED_GETUP && !IsPedHeadAbovePos(0.0f))
				m_curFightMove = FIGHTMOVE_HITONFLOOR;

			if (m_nPedState == PED_FIGHT) {
				CAnimBlendAssociation *moveAssoc = BlendDamageReaction(GetClump(), tFightMoves[m_curFightMove].animId);
				if(!moveAssoc){ EndFight(ENDFIGHT_FAST); return; }
				moveAssoc->SetCurrentTime(0.0f);
				moveAssoc->SetFinishCallback(FinishFightMoveCB, this);
				if (IsPlayer())
					moveAssoc->m_Speed = 1.2f;

				m_takeAStepAfterAttack = false;
				m_fightButtonPressure = 0;

			} else if (IsPlayer() && GetWeapon()->m_eWeaponType != WEAPONTYPE_UNARMED && GetWeapon()->m_eWeaponType != WEAPONTYPE_BRASSKNUCKLE &&
					!CWeaponInfo::GetWeaponInfo(GetWeapon()->m_eWeaponType)->IsFlagSet(WEAPONFLAG_FIGHTMODE)) {
				CAnimBlendAssociation *moveAssoc = BlendDamageReaction(GetClump(), tFightMoves[m_curFightMove].animId, 4.0f);
				if(moveAssoc){
					moveAssoc->SetCurrentTime(0.0f);
					moveAssoc->m_Speed = 1.2f;
				}

			} else {
				if (m_nPedState != PED_AIMGUN && m_nPedState != PED_ATTACK)
					SetStoredState();

				if (m_nWaitState != WAITSTATE_FALSE) {
					ClearWaitState();
					RestoreHeadingRate();
				}
				SetPedState(PED_FIGHT);
				m_fightButtonPressure = 0;
				m_lastFightMove = FIGHTMOVE_IDLE;
				RpAnimBlendClumpRemoveAssociations(GetClump(), ANIMATION_IS_LOOPED);
				CAnimBlendAssociation *walkStartAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_WALK_START);
				if (walkStartAssoc) {
					walkStartAssoc->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE;
					walkStartAssoc->m_BlendDelta = -1000.0f;
				}
				CAnimBlendAssociation *walkStopAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_RUN_STOP);
				if (!walkStopAssoc)
					walkStopAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_RUN_STOPR);
				if (walkStopAssoc) {
					walkStopAssoc->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE;
					walkStopAssoc->m_BlendDelta = -1000.0f;
					RestoreHeadingRate();
				}
				SetMoveState(PEDMOVE_NONE);
				m_nStoredMoveState = PEDMOVE_NONE;
				CAnimBlendAssociation *fightIdleAssoc;

				if (GetWeapon()->m_eWeaponType != WEAPONTYPE_UNARMED) {
					CWeaponInfo *weaponInfo = CWeaponInfo::GetWeaponInfo(GetWeapon()->m_eWeaponType);
					if (GetFightIdleWithMeleeAnim(weaponInfo)) {
						fightIdleAssoc = CAnimManager::AddAnimation(GetClump(), weaponInfo->m_AnimToPlay, GetFightIdleWithMeleeAnim(weaponInfo));
					} else {
						fightIdleAssoc = CAnimManager::AddAnimation(GetClump(), ANIM_GROUP_MELEE_1, ANIM_ID_FIGHT_IDLE);
					}
				} else {
					fightIdleAssoc = CAnimManager::AddAnimation(GetClump(), ANIM_GROUP_MELEE_1, ANIM_ID_FIGHT_IDLE);
				}
				if(fightIdleAssoc)
					fightIdleAssoc->m_BlendAmount = 1.0f;
				CAnimBlendAssociation *moveAssoc = BlendDamageReaction(GetClump(), tFightMoves[m_curFightMove].animId);
				if(!moveAssoc){ EndFight(ENDFIGHT_FAST); return; }
				moveAssoc->SetFinishCallback(FinishFightMoveCB, this);
				m_fightState = FIGHTSTATE_NO_MOVE;
				m_takeAStepAfterAttack = false;
				bIsAttacking = true;
			}

			if (m_pedInObjective && m_pedInObjective->IsPlayer() && !IsPlayer())
				((CPlayerPed*)m_pedInObjective)->RemovePedFromMeleeList(this);
		}
	}
}

void
CPed::Fight(void)
{
	CAnimBlendAssociation *currentAssoc, *animAssoc;
	bool fightWithWeapon = false;

	eWeaponType weapon = GetWeapon()->m_eWeaponType;
	CWeaponInfo *weaponInfo = CWeaponInfo::GetWeaponInfo(weapon);

	if (weaponInfo->IsFlagSet(WEAPONFLAG_FIGHTMODE) && weapon != WEAPONTYPE_UNARMED) {
		fightWithWeapon = true;
		tFightMoves[FIGHTMOVE_MELEE1].startFireTime = weaponInfo->m_fAnimFrameFire;
		tFightMoves[FIGHTMOVE_MELEE1].endFireTime = weaponInfo->m_fAnimLoopEnd;
		tFightMoves[FIGHTMOVE_MELEE2].startFireTime = weaponInfo->m_fAnim2FrameFire;
		tFightMoves[FIGHTMOVE_MELEE2].endFireTime = weaponInfo->m_fAnim2LoopEnd;
		tFightMoves[FIGHTMOVE_MELEE3].startFireTime = weaponInfo->m_fAnim2FrameFire;
		tFightMoves[FIGHTMOVE_MELEE3].endFireTime = weaponInfo->m_fAnim2LoopEnd;
	}

	switch (m_curFightMove) {
		case FIGHTMOVE_NULL:
			return;
		case FIGHTMOVE_IDLE2NORM:
			m_curFightMove = FIGHTMOVE_NULL;
			RestorePreviousState();

			// FIX: Uninitialized
			currentAssoc = nil;
			break;
		case FIGHTMOVE_IDLE:
			currentAssoc = nil;
			break;
		default:
			currentAssoc = RpAnimBlendClumpGetAssociation(GetClump(), tFightMoves[m_curFightMove].animId);
			break;
	}

	if (m_curFightMove == FIGHTMOVE_SHUFFLE_F && !currentAssoc)
		currentAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_FIGHTSH_BWD);

	if (IsPlayer() && currentAssoc && weapon == WEAPONTYPE_KATANA) {
		if (m_curFightMove == FIGHTMOVE_MELEE1 || m_curFightMove == FIGHTMOVE_MELEE2) {
			static float streakDelay = 0.2f;

			if (tFightMoves[m_curFightMove].startFireTime - streakDelay < currentAssoc->m_CurrentTime &&
				streakDelay + tFightMoves[m_curFightMove].endFireTime > currentAssoc->m_CurrentTime) {
				CSpecialFX::AddWeaponStreak(GetWeapon()->m_eWeaponType);
			}
		}
	}

	if (!bIsAttacking && IsPlayer()) {
		if (currentAssoc) {
			currentAssoc->m_BlendDelta = -1000.0f;
			currentAssoc->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE;
			currentAssoc->m_Flags &= ~ANIMATION_IS_PLAYING;
		}
		if (m_takeAStepAfterAttack)
			EndFight(ENDFIGHT_WITH_A_STEP);
		else
			EndFight(ENDFIGHT_FAST);

	} else if (currentAssoc && m_fightState > FIGHTSTATE_MOVE_FINISHED) {
		float animTime = currentAssoc->m_CurrentTime;
		FightMove &curMove = tFightMoves[m_curFightMove];
		if (curMove.hitLevel != HITLEVEL_NULL && animTime > curMove.startFireTime && animTime <= curMove.endFireTime && m_fightState >= FIGHTSTATE_NO_MOVE) {

			if (animTime > curMove.startFireTime && animTime - currentAssoc->m_TimeStep < curMove.startFireTime &&
				(IsPlayer() || weapon != WEAPONTYPE_UNARMED)) {

				DMAudio.PlayOneShot(m_audioEntityId, SOUND_MELEE_ATTACK_START, weapon << 8);
			}

			CVector touchingNodePos(0.0f, 0.0f, 0.0f);

			switch (m_curFightMove) {
				case FIGHTMOVE_KNEE:
					TransformToNode(touchingNodePos, PED_NODE_RIGHT_LOWER_LEG);
					break;
				case FIGHTMOVE_PUNCHHOOK:
				case FIGHTMOVE_PUNCHJAB:
					TransformToNode(touchingNodePos, PED_NODE_LEFT_HAND);
					break;
				case FIGHTMOVE_LONGKICK:
				case FIGHTMOVE_ROUNDHOUSE:
				case FIGHTMOVE_FWDLEFT:
				case FIGHTMOVE_BACKRIGHT:
				case FIGHTMOVE_GROUNDKICK:
					TransformToNode(touchingNodePos, PED_NODE_RIGHT_FOOT);
					break;
				case FIGHTMOVE_FWDRIGHT:
					TransformToNode(touchingNodePos, PED_NODE_HEAD);
					break;
				case FIGHTMOVE_BACKKICK:
				case FIGHTMOVE_BACKFLIP:
					TransformToNode(touchingNodePos, PED_NODE_LEFT_FOOT);
					break;
				case FIGHTMOVE_BACKLEFT:
					TransformToNode(touchingNodePos, PED_NODE_LEFT_ARM);
					break;
				default:
					TransformToNode(touchingNodePos, PED_NODE_RIGHT_HAND);
					break;
			}

			FightStrike(touchingNodePos, fightWithWeapon);
			m_fightButtonPressure = 0;
			return;
		}

		if (curMove.hitLevel != HITLEVEL_NULL) {
			if (animTime > curMove.endFireTime && weaponInfo->m_AnimToPlay != ANIM_GROUP_KNIFE_1) {
				// The swing is over: the hand has passed the target and what is
				// left of the clip is the arm drifting back down. Played at the
				// speed the move was started with, that tail keeps the fist out
				// for a moment after the blow, which reads as a limp, slow
				// punch. Run the recovery out quickly instead, so the move ends
				// on the strike and the next one can follow. The callback still
				// fires on its own, the clip just gets there sooner.
				if (IsPlayer())
					currentAssoc->m_Speed = 1.6f;
				else
					currentAssoc->m_Speed = 1.15f;
			}

			if (IsPlayer() && !nPlayerInComboMove && !fightWithWeapon) {
				if (curMove.comboFollowOnTime > 0.0f && m_fightButtonPressure != 0 && animTime > curMove.comboFollowOnTime) {

					m_lastFightMove = m_curFightMove;
					// Notice that it increases fight move index, because we're in combo!
					animAssoc = CAnimManager::BlendAnimation(GetClump(), tFightMoves[m_curFightMove + 1].animGroup, tFightMoves[++m_curFightMove].animId, 8.0f);
					if(!animAssoc){ EndFight(ENDFIGHT_FAST); return; }
					animAssoc->SetFinishCallback(FinishFightMoveCB, this);
					// The follow-up is the middle of a thrown combination, not a
					// fresh wind-up. Starting it at 0.8 speed from ten percent in
					// made every punch of a combo as slow as the first, which is
					// what kept the whole sequence feeling sluggish.
					animAssoc->SetCurrentTime(0.1f * animAssoc->m_BlendHier->m_fTotalTime);
					animAssoc->m_Speed = tFightMoves[m_curFightMove].hitLevel == HITLEVEL_NULL ? 0.8f : 1.1f;
					m_fightButtonPressure = 0;
					nPlayerInComboMove = 1;
				}
			}
		}

	} else if (GetWeapon()->m_eWeaponType != WEAPONTYPE_UNARMED && GetWeapon()->m_eWeaponType != WEAPONTYPE_BRASSKNUCKLE && !fightWithWeapon) {
		EndFight(ENDFIGHT_FAST);

	} else if (m_fightButtonPressure != 0) {
		if (!IsPlayer())
			Say(SOUND_PED_ATTACK);

		if (m_curFightMove != FIGHTMOVE_IDLE)
			m_lastFightMove = m_curFightMove;

		m_curFightMove = IsPlayer() ? ChooseAttackPlayer(m_fightButtonPressure, fightWithWeapon) : ChooseAttackAI(m_fightButtonPressure, fightWithWeapon);

		if (m_curFightMove != FIGHTMOVE_IDLE) {

			animAssoc = CAnimManager::BlendAnimation(GetClump(), m_curFightMove < FIGHTMOVE_MELEE1 ? tFightMoves[m_curFightMove].animGroup : weaponInfo->m_AnimToPlay,
				tFightMoves[m_curFightMove].animId, 8.0f);
			if(!animAssoc){ EndFight(ENDFIGHT_FAST); return; }

			if (weaponInfo->m_AnimToPlay != ANIM_GROUP_KNIFE_1 || m_curFightMove < FIGHTMOVE_MELEE1) {
				if (m_curFightMove == FIGHTMOVE_BACKKICK)
					animAssoc->m_Speed = 1.15f;
				else
					animAssoc->m_Speed = 0.8f;
			} else {
				switch (GetWeapon()->m_eWeaponType) {
					case WEAPONTYPE_SCREWDRIVER:
					case WEAPONTYPE_KNIFE:
						animAssoc->m_Speed = 1.05f;
						break;
					case WEAPONTYPE_GOLFCLUB:
					case WEAPONTYPE_NIGHTSTICK:
					case WEAPONTYPE_BASEBALLBAT:
					case WEAPONTYPE_HAMMER:
					case WEAPONTYPE_KATANA:
						animAssoc->m_Speed = 0.8f;
						break;
					case WEAPONTYPE_CLEAVER:
					case WEAPONTYPE_MACHETE:
						animAssoc->m_Speed = 0.9f;
						break;
				}
			}

			if (m_fightState == FIGHTSTATE_MOVE_FINISHED && animAssoc->m_CurrentTime != 0.0f) {
				animAssoc->m_Flags |= ANIMATION_IS_PLAYING;
				if (!IsPlayer())
					animAssoc->SetCurrentTime(0.0f);
			}
			if (IsPlayer())
				animAssoc->SetCurrentTime(0.08f);

			animAssoc->SetFinishCallback(FinishFightMoveCB, this);
			m_fightButtonPressure = 0;
		}
		m_fightState = FIGHTSTATE_NO_MOVE;
	} else if (m_takeAStepAfterAttack && m_curFightMove != FIGHTMOVE_SHUFFLE_F
#ifndef FIX_BUGS
		&& CheckForPedsOnGroundToAttack(this, nil) == 4) {
#else
		&& CheckForPedsOnGroundToAttack(this, nil) == PED_IN_FRONT_OF_ATTACKER) {
#endif
		m_lastFightMove = m_curFightMove;
		m_curFightMove = FIGHTMOVE_SHUFFLE_F;
		animAssoc = RpAnimBlendClumpGetAssociation(GetClump(), tFightMoves[m_curFightMove].animId);

		if (animAssoc) {
			animAssoc->SetCurrentTime(0.0f);
			animAssoc->m_BlendDelta = 4.0f;
			animAssoc->m_Flags |= ANIMATION_IS_PLAYING;
			} else {
				animAssoc = CAnimManager::BlendAnimation(GetClump(), tFightMoves[m_curFightMove].animGroup, tFightMoves[m_curFightMove].animId, 32.0f);
			}
			if(!animAssoc){ EndFight(ENDFIGHT_FAST); return; }
			animAssoc->SetFinishCallback(FinishFightMoveCB, this);
			m_fightState = FIGHTSTATE_NO_MOVE;
		m_fightButtonPressure = 0;
		m_takeAStepAfterAttack = false;

	} else if (m_takeAStepAfterAttack) {
		EndFight(ENDFIGHT_FAST);

	} else if (m_curFightMove == FIGHTMOVE_IDLE) {
		if (CTimer::GetTimeInMilliseconds() > m_nWaitTimer) {
			EndFight(ENDFIGHT_NORMAL);
		}

	} else {
		m_lastFightMove = m_curFightMove;
		m_curFightMove = FIGHTMOVE_IDLE;
		if (IsPlayer())
			m_nWaitTimer = CTimer::GetTimeInMilliseconds() + 500;
		else
			m_nWaitTimer = CTimer::GetTimeInMilliseconds() + 2000;
	}
}

int32
CPed::ChooseAttackAI(uint8 buttonPressure, bool fightWithWeapon)
{
	eWeaponType weapon = GetWeapon()->m_eWeaponType;
	CWeaponInfo *weaponInfo = CWeaponInfo::GetWeaponInfo(weapon);
	if (!fightWithWeapon && weapon != WEAPONTYPE_UNARMED && weapon != WEAPONTYPE_BRASSKNUCKLE) {
		return FIGHTMOVE_PUNCH;
	}

	if (!m_pedInObjective)
		return FIGHTMOVE_IDLE;
	if (buttonPressure == 0)
		return FIGHTMOVE_IDLE;

	uint16 pedFeatures = m_pedStats->m_flags;
	bool punchOnly = !!(pedFeatures & STAT_PUNCH_ONLY);
	bool canRoundhouse = !!(pedFeatures & STAT_CAN_ROUNDHOUSE);
	bool canKneeHead = !!(pedFeatures & STAT_CAN_KNEE_HEAD);
	bool canKick = !!(pedFeatures & STAT_CAN_KICK);
	bool hasShoppingBags = !!(pedFeatures & STAT_SHOPPING_BAGS);

	CVector distVec(m_pedInObjective->GetPosition() - GetPosition());
	float dist = distVec.Magnitude();
	m_fRotationDest = CGeneral::LimitRadianAngle(distVec.Heading());
	m_fRotationCur = m_fRotationDest;

	if (fightWithWeapon) {
		if (m_pedInObjective->OnGroundOrGettingUp()) {
			if (CGame::nastyGame && dist < 1.2f && !m_pedInObjective->IsPlayer()
				&& (m_pedInObjective->m_nPedState == PED_DEAD || !m_pedInObjective->IsPedHeadAbovePos(-0.3f))) {
				if (weaponInfo->IsFlagSet(WEAPONFLAG_GROUND_2ND))
					return FIGHTMOVE_MELEE2;
				if (weaponInfo->IsFlagSet(WEAPONFLAG_GROUND_3RD))
					return FIGHTMOVE_MELEE3;

				return FIGHTMOVE_GROUNDKICK;
			} else {
				return FIGHTMOVE_IDLE;
			}
		}
		if (dist < 2.f) {
			if (m_curFightMove == FIGHTMOVE_MELEE1) {
				if (GetSecondFireAnim(weaponInfo))
					return FIGHTMOVE_MELEE2;
			}
			if (m_curFightMove == FIGHTMOVE_MELEE2) {
				if (GetFinishingAttackAnim(weaponInfo))
					return FIGHTMOVE_MELEE3;
			}
			return FIGHTMOVE_MELEE1;
		}
		return FIGHTMOVE_SHUFFLE_F;
	}
	if (!hasShoppingBags) {
		if (punchOnly) {
			if (dist < 1.4f)
				return FIGHTMOVE_PUNCH;
		} else {
			if (m_pedInObjective->OnGroundOrGettingUp()) {
				if (CGame::nastyGame && dist < 1.2f && !m_pedInObjective->IsPlayer()
					&& (m_pedInObjective->m_nPedState == PED_DEAD || !m_pedInObjective->IsPedHeadAbovePos(-0.3f))) {

					return FIGHTMOVE_GROUNDKICK;
				} else {
					return FIGHTMOVE_IDLE;
				}
			}
			if (dist < 0.95f && canKneeHead)
				return FIGHTMOVE_KNEE;
			if (dist < 1.4f)
				return FIGHTMOVE_PUNCH;
			if (dist < 2.f && canKick) {
				int nextMove = FIGHTMOVE_LONGKICK;
				if (canRoundhouse && CGeneral::GetRandomNumber() & 1)
					nextMove = FIGHTMOVE_ROUNDHOUSE;
				return nextMove;
			}
		}
		return FIGHTMOVE_SHUFFLE_F;
	}
	if (dist < 2.f)
		return FIGHTMOVE_ROUNDHOUSE;
	else
		return FIGHTMOVE_SHUFFLE_F;
}

int32
CPed::ChooseAttackPlayer(uint8 buttonPressure, bool fightWithWeapon)
{
	CWeaponInfo *weaponInfo = CWeaponInfo::GetWeaponInfo(GetWeapon()->m_eWeaponType);
	const float maxAttackDist = 2.7f;
	float weaponAttackDist = 2.0f;
	CPed *victimPed = nil;
	CPed *walkUpTo = nil;
	CPed *groundAttackDeadPed = nil;
	CPed *groundAttackAlivePed = nil;
	if (fightWithWeapon)
		weaponAttackDist = weaponInfo->m_fRange;

	bool willWalkUp = false;
	PedFightMoves choosenMove = FIGHTMOVE_IDLE;
	int numPedsWeCanReach = 0;
	if (m_takeAStepAfterAttack)
		willWalkUp = true;

	float groundAttackDeadAngle, groundAttackAliveAngle, walkAngle, victimAngle, distToVictim;

	for (int i = 0; i < m_numNearPeds; ++i) {
		CPed *nearPed = m_nearPeds[i];
		CVector distVec(nearPed->GetPosition() - GetPosition());
		float dist = distVec.Magnitude();
		if (dist < maxAttackDist) {
			float nearPedAngle = CGeneral::LimitRadianAngle(distVec.Heading());
			m_fRotationCur = CGeneral::LimitRadianAngle(m_fRotationCur);
			float neededTurn = Abs(nearPedAngle - m_fRotationCur);
			if (neededTurn > PI)
				neededTurn = TWOPI - neededTurn;

			if (!nearPed->OnGroundOrGettingUp() && nearPed->m_nWaitState != WAITSTATE_SUN_BATHE_IDLE) {
				if (!willWalkUp || neededTurn <= DEGTORAD(45.0f)) {

					if (neededTurn <= DEGTORAD(30.0f) || nearPed->m_pedInObjective == this
						&& (nearPed->m_objective == OBJECTIVE_KILL_CHAR_ON_FOOT || nearPed->m_objective == OBJECTIVE_KILL_CHAR_ANY_MEANS)) {

						if (dist < weaponAttackDist) {
							if (!victimPed
								|| nearPed->m_attackTimer < victimPed->m_attackTimer && nearPed->m_attackTimer > CTimer::GetTimeInMilliseconds() - 100) {
								victimPed = nearPed;
								victimAngle = nearPedAngle;
								distToVictim = dist;
							}
							++numPedsWeCanReach;
							
						} else {
							if (neededTurn < DEGTORAD(30.0f)) {
								walkUpTo = nearPed;
								walkAngle = nearPedAngle;
							}
						}
					}
				}
			} else if (CGame::nastyGame && dist < 1.2f && neededTurn < DEGTORAD(55.0f)) {
				if (!nearPed->DyingOrDead() || groundAttackDeadPed) {
					if (!nearPed->IsPedHeadAbovePos(-0.3f)) {
						groundAttackAlivePed = nearPed;
						groundAttackAliveAngle = nearPedAngle;
					}
				} else {
					groundAttackDeadPed = nearPed;
					groundAttackDeadAngle = nearPedAngle;
				}
				++numPedsWeCanReach;

			} else if (dist > 1.4f && dist < maxAttackDist && neededTurn < DEGTORAD(30.0f)) {
				if (!walkUpTo) {
					walkUpTo = nearPed;
					walkAngle = nearPedAngle;
				}
#ifdef FIX_BUGS
				if(dist < 2.1f)
#endif
				++numPedsWeCanReach;
			}
		}
	}

	if (victimPed) {
		float adjustedAngleDiff = victimAngle - m_fRotationCur + DEGTORAD(30.0f);
		if (adjustedAngleDiff < 0.0f)
			adjustedAngleDiff += TWOPI;

		int16 dir = Floor(adjustedAngleDiff / DEGTORAD(60.0f));

		// Just focus on who we're fighting with, don't care peds on ground
		if (numPedsWeCanReach < 2 || fightWithWeapon) {
			float angleDiff = Abs(victimAngle - m_fRotationCur);
			if (angleDiff > PI)
				angleDiff = TWOPI - angleDiff;

			if (angleDiff < DEGTORAD(60.0f))
				dir = 0; // forward
		}
		int16 randVal = CGeneral::GetRandomNumber() & 3;
		switch (dir) {
			case 0: // forward
				if (fightWithWeapon) {
					if (distToVictim < 0.95f - 0.2f && m_nPedState == PED_FIGHT) {
						choosenMove = FIGHTMOVE_KNEE;
					} else {
						if (GetWeapon()->m_eWeaponType == WEAPONTYPE_CLEAVER) {
							if (distToVictim < 0.85f * weaponInfo->m_fRange)
								choosenMove = FIGHTMOVE_MELEE1;
							else
								choosenMove = FIGHTMOVE_SHUFFLE_F;
						} else {
							float weaponRange = weaponInfo->m_fRange;
							if (distToVictim < 0.75f * weaponRange && GetWeapon()->m_eWeaponType != WEAPONTYPE_SCREWDRIVER) {
								if (m_lastFightMove == FIGHTMOVE_MELEE1 && GetFinishingAttackAnim(weaponInfo)) {
									choosenMove = FIGHTMOVE_MELEE2;
								} else if (m_lastFightMove == FIGHTMOVE_MELEE2 && GetFinishingAttackAnim(weaponInfo)) {
									choosenMove = FIGHTMOVE_MELEE3;
								} else {
									choosenMove = FIGHTMOVE_MELEE1;
								}
							} else if (distToVictim < weaponRange && GetFinishingAttackAnim(weaponInfo)) {
								choosenMove = FIGHTMOVE_MELEE3;
							} else {
								choosenMove = FIGHTMOVE_SHUFFLE_F;
							}
						}
					}
				} else if (distToVictim < 0.95f && m_nPedState == PED_FIGHT) {
					choosenMove = FIGHTMOVE_KNEE;

				} else if (distToVictim < 1.4f) {
					if (m_curFightMove == FIGHTMOVE_PUNCHJAB) {
						choosenMove = FIGHTMOVE_PUNCH;

					} else if (m_curFightMove != FIGHTMOVE_PUNCH || randVal != 1) {
						if (randVal == 2)
							choosenMove = FIGHTMOVE_PUNCH;
						else
							choosenMove = FIGHTMOVE_PUNCHJAB;
					} else {
						choosenMove = FIGHTMOVE_LONGKICK;
					}
				} else {
					choosenMove = FIGHTMOVE_LONGKICK;
				}
				break;
			case 1:
				choosenMove = FIGHTMOVE_FWDLEFT;
				break;
			case 2:
				choosenMove = FIGHTMOVE_BACKLEFT;
				break;
			case 3:
				choosenMove = FIGHTMOVE_BACKKICK;
				break;
			case 4:
				choosenMove = FIGHTMOVE_BACKRIGHT;
				break;
			default:
				choosenMove = FIGHTMOVE_FWDRIGHT;
				break;
		}

		// forward
		if (dir == 0) {
			m_fRotationDest = CGeneral::LimitRadianAngle(victimAngle);
		} else {
			m_fRotationDest = victimAngle - dir * DEGTORAD(60.0f);
			m_fRotationDest = CGeneral::LimitRadianAngle(m_fRotationDest);
		}
		
		m_fRotationCur = m_fRotationDest;
		Say(SOUND_PED_ATTACK);

	} else if (groundAttackAlivePed || groundAttackDeadPed) {
		if (fightWithWeapon && weaponInfo->IsFlagSet(WEAPONFLAG_GROUND_2ND)) {
			choosenMove = FIGHTMOVE_MELEE2;
		} else if (fightWithWeapon && weaponInfo->IsFlagSet(WEAPONFLAG_GROUND_3RD)) {
			choosenMove = FIGHTMOVE_MELEE3;
		} else {
			choosenMove = FIGHTMOVE_GROUNDKICK;
		}
		if (groundAttackAlivePed)
			m_fRotationDest = groundAttackAliveAngle;
		else
			m_fRotationDest = groundAttackDeadAngle;

		m_fRotationCur = m_fRotationDest;
		m_lookTimer = 0;
		if (groundAttackAlivePed)
			SetLookFlag(groundAttackAlivePed, 1, 0);
		else
			SetLookFlag(groundAttackDeadPed, 1, 0);

		SetLookTimer(1500u);

	} else if (walkUpTo) {
		choosenMove = FIGHTMOVE_SHUFFLE_F;
		m_fRotationCur = m_fRotationDest = walkAngle;
		m_lookTimer = 0;
		SetLookFlag(walkUpTo, true);
		SetLookTimer(1500);

	} else if (fightWithWeapon) {
		// No enemy, fight with space
		if (GetWeapon()->m_eWeaponType == WEAPONTYPE_SCREWDRIVER) {
			choosenMove = FIGHTMOVE_MELEE3;
		} else {
			if (m_lastFightMove == FIGHTMOVE_MELEE1 && GetFinishingAttackAnim(weaponInfo)) {
				choosenMove = FIGHTMOVE_MELEE2;
			} else if (m_lastFightMove == FIGHTMOVE_MELEE2 && GetFinishingAttackAnim(weaponInfo)) {
				choosenMove = FIGHTMOVE_MELEE3;
			} else {
				choosenMove = FIGHTMOVE_MELEE1;
			}
		}
	} else {
		// Max number GetRandomNumberInRange returns is max-1
#ifdef FIX_BUGS
		switch (CGeneral::GetRandomNumberInRange(0,4)) {
#else
		switch (CGeneral::GetRandomNumberInRange(0,3)) {
#endif
			case 0:
				choosenMove = FIGHTMOVE_PUNCHJAB;
				break;
			case 1:
				choosenMove = FIGHTMOVE_PUNCH;
				break;
			case 2:
				choosenMove = FIGHTMOVE_LONGKICK;
				break;
			case 3:
				choosenMove = FIGHTMOVE_KNEE;
				break;
			default:
				break;
		}
	}
	return choosenMove;
}

void
CPed::EndFight(uint8 endType)
{
	if (m_nPedState != PED_FIGHT)
		return;

	m_curFightMove = FIGHTMOVE_NULL;
	RestorePreviousState();
	CAnimBlendAssociation *animAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_FIGHT_IDLE);
	if (!animAssoc)
		animAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_FIGHT_IDLE);

	if (animAssoc)
		animAssoc->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE;

	switch (endType) {
		case ENDFIGHT_NORMAL:
			CAnimManager::BlendAnimation(GetClump(), m_animGroup, ANIM_ID_IDLE, 8.0f);
			CAnimManager::BlendAnimation(GetClump(), ANIM_GROUP_DEFAULT, ANIM_ID_FIGHT2IDLE, 8.0f);
			break;
		case ENDFIGHT_WITH_A_STEP:
			CAnimManager::BlendAnimation(GetClump(), m_animGroup, ANIM_ID_IDLE, 1.0f);
			CAnimManager::BlendAnimation(GetClump(), ANIM_GROUP_DEFAULT, ANIM_ID_WALK_START, 8.0f);
			break;
		case ENDFIGHT_FAST:
			CAnimManager::BlendAnimation(GetClump(), m_animGroup, ANIM_ID_IDLE, 8.0f);
			if(CAnimBlendAssociation *fade = CAnimManager::BlendAnimation(GetClump(), ANIM_GROUP_DEFAULT, ANIM_ID_FIGHT2IDLE, 8.0f))
				fade->m_Speed = 2.0f;
			break;
		default:
			break;
	}
	m_nWaitTimer = 0;
}

void
CPed::PlayHitSound(CPed *hitTo)
{
	// That was very complicated to reverse for me...
	// First index is our fight move ID (from 1 to 17, total 17), second is the one of we fight with (from 18 to 27, total 10).
	enum {
		S37 = SOUND_FIGHT_37,
		S38 = SOUND_FIGHT_38,
		S39 = SOUND_FIGHT_39,
		S40 = SOUND_FIGHT_40,
		S41 = SOUND_FIGHT_41,
		S42 = SOUND_FIGHT_42,
		S43 = SOUND_FIGHT_43,
		S44 = SOUND_FIGHT_44,
		S45 = SOUND_FIGHT_45,
		S46 = SOUND_FIGHT_46,
		S47 = SOUND_FIGHT_47,
		S48 = SOUND_FIGHT_48,
		NO_SND = SOUND_NO_SOUND
	};
	const uint16 hitSoundsByFightMoves[17][10] = {
	  { S37, S46, S41, S41, S46, S46, S40, S41, S43, S40 },
	  { NO_SND, NO_SND, NO_SND, NO_SND, NO_SND, NO_SND, NO_SND, NO_SND, NO_SND, NO_SND },
	  { NO_SND, NO_SND, NO_SND, NO_SND, NO_SND, NO_SND, NO_SND, NO_SND, NO_SND, NO_SND },
	  { S46, S46, S46, S46, S37, S47, S37, S38, S43, S38 },
	  { S46, S46, S46, S46, S46, S46, S40, S41, S43, S46 },
	  { S46, S46, S46, S46, S46, S46, S40, S41, S43, S40 },
	  { S46, S46, S46, S46, S46, S46, S40, S41, S43, S40 },
	  { S46, S46, S37, S46, S37, S47, S40, S47, S43, S37 },
	  { S46, S46, S46, S46, S46, S46, S43, S44, S43, S43 },
	  { S37, S46, S46, S46, S38, S47, S40, S38, S43, S46 },
	  { S46, S37, S46, S37, S39, S46, S40, S39, S43, S37 },
	  { S46, S37, S46, S46, S38, S47, S40, S38, S43, S46 },
	  { S37, S37, S46, S46, S38, S47, S48, S38, S43, S37 },
	  { S46, S46, S46, S46, S37, S46, S40, S38, S43, S46 },
	  { S46, S46, S46, S37, S39, S46, S40, S39, S43, S46 },
	  { S37, S46, S46, S46, S37, S46, S40, S37, S43, S46 },
	  { S43, S43, S43, S43, S43, S43, S43, S43, S43, S43 }
	};

	eWeaponType weapon = GetWeapon()->m_eWeaponType;
	CWeaponInfo *weaponInfo = CWeaponInfo::GetWeaponInfo(weapon);
	if (weaponInfo->m_AnimToPlay == ANIM_GROUP_KNIFE_1) {
		if (m_curFightMove >= FIGHTMOVE_MELEE1) {
			if (m_curFightMove == FIGHTMOVE_MELEE3) {
				DMAudio.PlayOneShot(m_audioEntityId, SOUND_WEAPON_BAT_ATTACK, (weapon << 8) | ENTITY_TYPE_PED);
			} else {
				DMAudio.PlayOneShot(m_audioEntityId, SOUND_WEAPON_KNIFE_ATTACK, (weapon << 8) | ENTITY_TYPE_PED);
			}
			return;
		}
	}

	// This is why first dimension is between FightMove 1 and 17.
	if (m_curFightMove <= FIGHTMOVE_NULL || m_curFightMove >= FIGHTMOVE_HITFRONT)
		return;

	uint16 soundId;

	// And this is why second dimension is between 18 and 27.
	if (hitTo->m_curFightMove > FIGHTMOVE_GROUNDKICK && hitTo->m_curFightMove < FIGHTMOVE_IDLE2NORM) {
		soundId = hitSoundsByFightMoves[m_curFightMove - FIGHTMOVE_STDPUNCH][hitTo->m_curFightMove - FIGHTMOVE_HITFRONT];

	} else {
		if (hitTo->m_nPedState == PED_DEAD || hitTo->UseGroundColModel()) {	
			soundId = hitSoundsByFightMoves[m_curFightMove - FIGHTMOVE_STDPUNCH][FIGHTMOVE_HITONFLOOR - FIGHTMOVE_HITFRONT];
		} else {
			soundId = hitSoundsByFightMoves[m_curFightMove - FIGHTMOVE_STDPUNCH][FIGHTMOVE_HITFRONT - FIGHTMOVE_HITFRONT];
		}
	}

	if (soundId != NO_SND)
		DMAudio.PlayOneShot(m_audioEntityId, soundId, (weapon << 8) | ENTITY_TYPE_PED);
}

bool
CPed::FightStrike(CVector &touchedNodePos, bool fightWithWeapon)
{
	CColModel *hisCol;
	CVector attackDistance;
	float maxDistanceToBeat;
	CPed *nearPed;
	CVector extendedTouchPoint;

	CWeaponInfo *weaponInfo = CWeaponInfo::GetWeaponInfo(GetWeapon()->m_eWeaponType);
	float radius = tFightMoves[m_curFightMove].strikeRadius;
	if (fightWithWeapon)
		radius = weaponInfo->m_fRadius;

	if (m_fightState == FIGHTSTATE_JUST_ATTACKED)
		return false;

	if (this == FindPlayerPed() && fightWithWeapon && GetWeapon()->m_eWeaponType != WEAPONTYPE_UNARMED)
		CGlass::BreakGlassPhysically(touchedNodePos, radius);

	for (int i = 0; i < m_numNearPeds; i++) {
		int8 pedFound = 0;
		nearPed = m_nearPeds[i];
		if (!fightWithWeapon && GetWeapon()->m_eWeaponType != WEAPONTYPE_UNARMED && GetWeapon()->m_eWeaponType != WEAPONTYPE_BRASSKNUCKLE)
			maxDistanceToBeat = nearPed->GetBoundRadius() + radius + 0.1f;
		else
			maxDistanceToBeat = nearPed->GetBoundRadius() + radius;

		if ((nearPed->bUsesCollision || nearPed->m_nPedState == PED_DEAD) && (m_pedInObjective != FindPlayerPed() || nearPed == FindPlayerPed())) {
			// Have to animate a skinned clump because the initial col model is useless
			hisCol = ((CPedModelInfo*)CModelInfo::GetModelInfo(nearPed->GetModelIndex()))->AnimatePedColModelSkinnedWorld(nearPed->GetClump());

			CVector nearPedCentre = nearPed->GetBoundCentre();
			CVector potentialAttackDistance = nearPedCentre - touchedNodePos;

			// He can beat us
			if (sq(maxDistanceToBeat) > potentialAttackDistance.MagnitudeSqr()) {

				for (int j = 0; j < hisCol->numSpheres; j++) {
					attackDistance = hisCol->spheres[j].center;
					attackDistance -= touchedNodePos;
					CColSphere *hisPieces = hisCol->spheres;
					maxDistanceToBeat = hisPieces[j].radius + radius;

					// We can beat him too
					if (sq(maxDistanceToBeat) > attackDistance.MagnitudeSqr()) {
						FightHitPed(nearPed, touchedNodePos, attackDistance, hisPieces[j].material);
						pedFound = 1;
						break;
					}
				}
			}
			if (!pedFound && !fightWithWeapon) {
				extendedTouchPoint = touchedNodePos - GetPosition();
				if (DotProduct(touchedNodePos - GetPosition(), nearPed->GetPosition() - GetPosition()) > 0.f) {
					if (m_curFightMove == FIGHTMOVE_GROUNDKICK) {
						extendedTouchPoint += tFightMoves[FIGHTMOVE_GROUNDKICK].extendReachMultiplier * GetForward();
					} else {
						extendedTouchPoint.x *= tFightMoves[m_curFightMove].extendReachMultiplier;
						extendedTouchPoint.y *= tFightMoves[m_curFightMove].extendReachMultiplier;
					}
					pedFound = -1;
					extendedTouchPoint += GetPosition();
				}
			}
			if (pedFound == -1) {
				CVector nearPedCentre = nearPed->GetBoundCentre();
				if (sq(maxDistanceToBeat) > (nearPedCentre - extendedTouchPoint).MagnitudeSqr()) {

					for (int j = 0; j < hisCol->numSpheres; j++) {
						attackDistance = hisCol->spheres[j].center;
						attackDistance -= extendedTouchPoint;
						CColSphere* hisPieces = hisCol->spheres;
						float maxDistanceToBeat2 = hisPieces[j].radius + tFightMoves[m_curFightMove].strikeRadius;

						// We can beat him too
						if (sq(maxDistanceToBeat2) > attackDistance.MagnitudeSqr()) {
							FightHitPed(nearPed, extendedTouchPoint, attackDistance, hisPieces[j].material);
							break;
						}
					}
				}
			}
		}
	}


	if (m_fightState == FIGHTSTATE_NO_MOVE)
		m_fightState = FIGHTSTATE_1;

	m_vecHitLastPos = touchedNodePos;
	return false;
}

void
CPed::FightHitPed(CPed *victim, CVector &touchPoint, CVector &dir, int16 piece)
{
	if (victim->IsPlayer() && victim->m_nPedState == PED_GETUP)
		return;

	CWeaponInfo *weaponInfo = CWeaponInfo::GetWeaponInfo(GetWeapon()->m_eWeaponType);
	bool fightingWithWeapon = false;
	int damageMult = tFightMoves[m_curFightMove].damage * ((CGeneral::GetRandomNumber() & 1) + 2) + 1;

	if (weaponInfo->IsFlagSet(WEAPONFLAG_FIGHTMODE)) {
		fightingWithWeapon = true;
		if (m_curFightMove >= FIGHTMOVE_MELEE1) {
			damageMult = weaponInfo->m_nDamage;
			if (m_curFightMove == FIGHTMOVE_MELEE3 && GetWeapon()->m_eWeaponType != WEAPONTYPE_SCREWDRIVER)
				damageMult *= 5;
		}
	}

	if (IsPlayer()) {
		if (((CPlayerPed*)this)->AdrenalineActive())
			damageMult = 20;
	} else if (!fightingWithWeapon) {
		damageMult *= m_pedStats->m_fAttackStrength;
	}

	float oldVictimHealth = victim->m_fHealth;
	CVector bloodPos = 0.5f * dir + touchPoint;
	CVector2D diff(GetPosition() - victim->GetPosition());
	int direction = victim->GetLocalDirection(diff);

	bool brassKnucklePunch = false;
	if (GetWeapon()->m_eWeaponType == WEAPONTYPE_BRASSKNUCKLE) {
		if (m_curFightMove == FIGHTMOVE_PUNCHHOOK || m_curFightMove == FIGHTMOVE_PUNCHJAB || m_curFightMove == FIGHTMOVE_BACKLEFT ||
			m_curFightMove == FIGHTMOVE_STDPUNCH || m_curFightMove == FIGHTMOVE_PUNCH) {
			brassKnucklePunch = true;
			damageMult *= 1.5f;
		}
	}
	victim->ReactToAttack(this);

	// Mostly unused. if > 5, ANIM_HIT_BIGSTEP will be run, that's it.
	int unk2;
	if (GetWeapon()->m_eWeaponType != WEAPONTYPE_UNARMED && GetWeapon()->m_eWeaponType != WEAPONTYPE_BRASSKNUCKLE &&
		!victim->IsPlayer() && !fightingWithWeapon)
		unk2 = 101;
	else
		unk2 = damageMult;

	victim->StartFightDefend(direction, tFightMoves[m_curFightMove].hitLevel, unk2);
	PlayHitSound(victim);
	m_fightState = FIGHTSTATE_JUST_ATTACKED;
	if (CAnimBlendAssociation *attack = RpAnimBlendClumpGetAssociation(GetClump(), tFightMoves[m_curFightMove].animId))
		attack->m_Speed = 0.6f;

	if (!victim->DyingOrDead()) {
		if(fightingWithWeapon)
			victim->InflictDamage(this, GetWeapon()->m_eWeaponType, damageMult, (ePedPieceTypes)piece, direction);
		else
			victim->InflictDamage(this, WEAPONTYPE_UNARMED, damageMult * 3.0f, (ePedPieceTypes)piece, direction);
	}

	if (CGame::nastyGame && weaponInfo->m_AnimToPlay == ANIM_GROUP_KNIFE_1 && m_curFightMove >= FIGHTMOVE_MELEE1
		&& victim->GetIsOnScreen()) {

		static float particleRightLen = 0.05f;
		static float particleUpLen = 0.05f;

		// Just for particles. We will restore it below.
		dir /= (20.0f * dir.Magnitude());
		if (m_curFightMove == FIGHTMOVE_MELEE1) {
			float rightMult = -particleRightLen;
			dir += particleUpLen * GetUp() + rightMult * GetRight();

		} else if (m_curFightMove == FIGHTMOVE_MELEE2) {
			float upMult = 2.0f * particleUpLen;
			dir += particleRightLen * GetRight() + upMult * GetUp();
		}
		CParticle::AddParticle(PARTICLE_BLOOD_SPURT, bloodPos, dir, nil, 0.0f, 0, 0, 0, 0);
		CParticle::AddParticle(PARTICLE_BLOOD_SPURT, bloodPos, dir, nil, 0.0f, 0, 0, 0, 0);
		if (IsPlayer()) {
			CParticle::AddParticle(PARTICLE_BLOOD_SPURT, bloodPos, dir, nil, 0.0f, 0, 0, 0, 0);
			CParticle::AddParticle(PARTICLE_BLOOD_SPURT, bloodPos, dir, nil, 0.0f, 0, 0, 0, 0);
		}
		if (!(CGeneral::GetRandomNumber() & 3)) {
			CParticle::AddParticle(PARTICLE_TEST, bloodPos, dir, nil, 0.0f, 0, 0, 0, 0);
		}
	} else if (CGame::nastyGame && (tFightMoves[m_curFightMove].hitLevel > HITLEVEL_MEDIUM || fightingWithWeapon)
			&& victim->GetIsOnScreen()) {

		// Just for particles. We will restore it below.
		dir /= (10.0f * dir.Magnitude());
		for (int i = 0; i < 4; i++) {
			CParticle::AddParticle(PARTICLE_BLOOD, bloodPos, dir, nil, 0.0f, 0, 0, 0, 0);
		}
	}

	eWeaponType weaponType = GetWeapon()->m_eWeaponType;
	if (!fightingWithWeapon) {
		if (!victim->OnGround()) {
			float curVictimHealth = victim->m_fHealth;
			if (curVictimHealth > 0.0f
				&& (curVictimHealth < 30.0f && oldVictimHealth > 30.0f
					|| weaponType != WEAPONTYPE_UNARMED && weaponType != WEAPONTYPE_BRASSKNUCKLE && IsPlayer()
					|| victim->m_pedStats->m_flags & STAT_ONE_HIT_KNOCKDOWN || brassKnucklePunch)) {

				victim->SetFall(0, (AnimationId)(direction + ANIM_ID_KO_SKID_FRONT), 0);
				if (victim->m_nPedState == PED_FALL)
					victim->bIsStanding = false;
			}
		}
	}

	if (victim->m_nPedState == PED_DIE || !victim->bIsStanding) {
		dir = victim->GetPosition() - GetPosition();
		dir.Normalise();
		dir.z = 1.0f;
		victim->bIsStanding = false;

		float moveMult;
		if (fightingWithWeapon) {
			moveMult = Min(damageMult * 0.02f, 1.0f);
		} else if (m_curFightMove == FIGHTMOVE_GROUNDKICK) {
			moveMult = Min(damageMult * 0.6f, 4.0f);
		} else {
			if (victim->m_nPedState != PED_DIE || damageMult >= 20) {
				moveMult = damageMult;
			} else {
				moveMult = Min(damageMult * 2.0f, 14.0f);
			}
		}

		victim->ApplyMoveForce(moveMult * 0.6 * dir);
	}

	if (weaponType != WEAPONTYPE_KNIFE && weaponType != WEAPONTYPE_MACHETE
		&& weaponType != WEAPONTYPE_KATANA && weaponType != WEAPONTYPE_CHAINSAW) {

		if (victim->m_nPedType == PEDTYPE_COP)
			CEventList::RegisterEvent(SCRIPT_EVENT_ASSAULT_POLICE, EVENT_ENTITY_PED, victim, this, 2000);
		else
			CEventList::RegisterEvent(SCRIPT_EVENT_ASSAULT, EVENT_ENTITY_PED, victim, this, 2000);
	} else {
		if (victim->m_nPedType == PEDTYPE_COP)
			CEventList::RegisterEvent(SCRIPT_EVENT_ASSAULT_NASTYWEAPON_POLICE, EVENT_ENTITY_PED, victim, this, 2000);
		else
			CEventList::RegisterEvent(SCRIPT_EVENT_ASSAULT_NASTYWEAPON, EVENT_ENTITY_PED, victim, this, 2000);
	}
}

void
CPed::FinishFightMoveCB(CAnimBlendAssociation *animAssoc, void *arg)
{
	CPed *ped = (CPed*)arg;

	if (tFightMoves[ped->m_curFightMove].animId == animAssoc->m_AnimId) {
		ped->m_fightState = FIGHTSTATE_MOVE_FINISHED;
		animAssoc->m_BlendDelta = -1000.0f;
	}
}

// The engine never keys the move table on the order of the rows: it looks a
// move up by the name in the first column, so a data file may list the moves in
// any order and may leave one out without shifting every move after it. The
// built-in table is ordered by the PedFightMoves enum, so the name is what ties
// a row to its slot.
static const struct { const char *name; int move; } s_fightMoveNames[] = {
	{ "Null",           FIGHTMOVE_NULL },
	{ "StdPunch",       FIGHTMOVE_STDPUNCH },
	{ "Idle",           FIGHTMOVE_IDLE },
	{ "Shuffle_f",      FIGHTMOVE_SHUFFLE_F },
	{ "Knee",           FIGHTMOVE_KNEE },
	{ "PunchHook",      FIGHTMOVE_PUNCHHOOK },
	{ "Jab",            FIGHTMOVE_PUNCHJAB },
	{ "Punch",          FIGHTMOVE_PUNCH },
	{ "LongKick",       FIGHTMOVE_LONGKICK },
	{ "RoundHouseKick", FIGHTMOVE_ROUNDHOUSE },
	{ "FwdLeft",        FIGHTMOVE_FWDLEFT },
	{ "FwdRight",       FIGHTMOVE_FWDRIGHT },
	{ "BackKick",       FIGHTMOVE_BACKKICK },
	{ "BackFlip",       FIGHTMOVE_BACKFLIP },
	{ "BackLeft",       FIGHTMOVE_BACKLEFT },
	{ "BackRight",      FIGHTMOVE_BACKRIGHT },
	{ "RightSweep",     FIGHTMOVE_RIGHTSWEEP },
	{ "GroundKick",     FIGHTMOVE_GROUNDKICK },
	{ "HitFront",       FIGHTMOVE_HITFRONT },
	{ "HitBack",        FIGHTMOVE_HITBACK },
	{ "HitRight",       FIGHTMOVE_HITRIGHT },
	{ "HitLeft",        FIGHTMOVE_HITLEFT },
	{ "HitBody",        FIGHTMOVE_HITBODY },
	{ "HitChest",       FIGHTMOVE_HITCHEST },
	{ "HitHead",        FIGHTMOVE_HITHEAD },
	{ "HitBigStep",     FIGHTMOVE_HITBIGSTEP },
	{ "HitOnFloor",     FIGHTMOVE_HITONFLOOR },
	{ "HitBehind",      FIGHTMOVE_HITBEHIND },
	{ "Melee1",         FIGHTMOVE_MELEE1 },
	{ "Melee2",         FIGHTMOVE_MELEE2 },
	{ "Melee3",         FIGHTMOVE_MELEE3 },
	{ "Idle2Norm",      FIGHTMOVE_IDLE2NORM },
};

static int
FightMoveFromName(const char *name)
{
	for(int i = 0; i < (int)ARRAY_SIZE(s_fightMoveNames); i++)
		if(strcasecmp(name, s_fightMoveNames[i].name) == 0)
			return s_fightMoveNames[i].move;
	return -1;
}

void
CPed::LoadFightData(void)
{
	float startFireTime, endFireTime, comboFollowOnTime, strikeRadius, extendReachMultiplier;
	float animLoop = 0.0f, animChain = 0.0f;
	int damage, flags;
	char line[256], moveName[32], animName[32], hitLevel;
	int moveId = 0;

	CAnimBlendStaticAssociation *animAssoc;

	size_t bp, buflen;
	int lp, linelen;

	// The melee table is data/melee.dat and lives in the game archive. Vice City
	// kept the same table in DATA as fistfite.dat; that name is gone because the
	// move list has to describe the SA clips that are actually loaded. A build
	// without the archive reads the loose SA file.
	buflen = 0;
#ifdef CUSTOM_MODELS
	{
		std::vector<uint8> archived;
		if(CCustomModels::ReadCommonDataFile("data/melee.dat", archived) &&
		   archived.size() > 0 && archived.size() <= sizeof(work_buff)){
			memcpy(work_buff, archived.data(), archived.size());
			buflen = archived.size();
		}
	}
#endif
	if(buflen == 0){
		CFileMgr::SetDir("DATA");
		buflen = CFileMgr::LoadFile("MELEE.DAT", work_buff, sizeof(work_buff), "r");
		CFileMgr::SetDir("");
	}
	if(buflen == 0){
		REVC_LOG_ERROR("melee table: no MELEE.DAT in the archive or in DATA; keeping the built-in move table");
		return;
	}

	for (bp = 0; bp < buflen; ) {
		// read file line by line
		for (linelen = 0; work_buff[bp] != '\n' && bp < buflen; bp++) {
			// melee.dat lines are short, but the buffer must never be overrun by
			// a malformed or truncated file.
			if (linelen < (int)sizeof(line) - 1)
				line[linelen++] = work_buff[bp];
		}
		bp++;
		line[linelen] = '\0';

		// skip white space
		for (lp = 0; line[lp] <= ' ' && line[lp] != '\0'; lp++);

		if (line[lp] == '\0' ||
			line[lp] == '#')
			continue;

		sscanf(
			&line[lp],
			"%31s %f %f %f %f %f %c %31s %d %d %f %f",
			moveName,
			&startFireTime,
			&endFireTime,
			&comboFollowOnTime,
			&strikeRadius,
			&extendReachMultiplier,
			&hitLevel,
			animName,
			&damage,
			&flags,
			&animLoop,
			&animChain);

		if (strncmp(moveName, "ENDWEAPONDATA", 13) == 0)
			return;
		// A row belongs to the move it names, not to the row's position: the
		// data file is free to reorder its rows or omit one, and indexing by
		// position would then hand every later move the parameters of its
		// neighbour. A name this build does not know still gets its row, in
		// file order, so an unfamiliar table keeps the old behaviour.
		moveId = FightMoveFromName(moveName);
		if (moveId < 0){
			// This table carries rows that are not moves of this build at all
			// (Attack1/Attack2/Attack3 name clips the unarmed style does not
			// use). Giving such a row the slot its file position happens to
			// reach overwrote the real moves that follow it - GroundKick lost
			// its reach and radius, RoundHouseKick its timing - which is what
			// made the punches land short and feel weak. A row for a move we
			// have no id for is skipped outright.
			REVC_LOG_WARNING("melee table: unknown move %s, row ignored", moveName);
			continue;
		}

		tFightMoves[moveId].startFireTime = startFireTime / 30.0f;
		tFightMoves[moveId].endFireTime = endFireTime / 30.0f;
		tFightMoves[moveId].comboFollowOnTime = comboFollowOnTime / 30.0f;
		tFightMoves[moveId].strikeRadius = strikeRadius;
		tFightMoves[moveId].extendReachMultiplier = extendReachMultiplier;
		tFightMoves[moveId].damage = damage;
		tFightMoves[moveId].flags = flags;
		// The built-in table names San Andreas ids of the unarmed fighting
		// style; the lookup below overrides it when the data file names a clip
		// of its own.
		tFightMoves[moveId].animGroup = ANIM_GROUP_MELEE_1;

		switch (hitLevel) {
			case 'G':
				tFightMoves[moveId].hitLevel = HITLEVEL_GROUND;
				break;
			case 'H':
				tFightMoves[moveId].hitLevel = HITLEVEL_HIGH;
				break;
			case 'L':
				tFightMoves[moveId].hitLevel = HITLEVEL_LOW;
				break;
			case 'M':
				tFightMoves[moveId].hitLevel = HITLEVEL_MEDIUM;
				break;
			case 'N':
				tFightMoves[moveId].hitLevel = HITLEVEL_NULL;
				break;
			default:
				break;
		}

		if (strcmp(animName, "default") != 0) {
			if (strcmp(animName, "null") != 0) {
				// fistfite.dat names Vice City clips. The San Andreas melee
				// clips only exist in the melee groups, so look there first and
				// keep the group that owns the clip.
				static const AssocGroupId meleeGroups[] = {
					ANIM_GROUP_MELEE_1, ANIM_GROUP_MELEE_2, ANIM_GROUP_MELEE_3,
					ANIM_GROUP_MELEE_4, ANIM_GROUP_KICK_STD
				};
				animAssoc = nil;
				AssocGroupId foundGroup = ANIM_GROUP_NONE;
				for (int g = 0; g < (int)ARRAY_SIZE(meleeGroups) && animAssoc == nil; g++) {
					animAssoc = CAnimManager::GetAnimAssociation(meleeGroups[g], animName);
					if (animAssoc)
						foundGroup = meleeGroups[g];
				}
				if (animAssoc) {
					tFightMoves[moveId].animId = (AnimationId)animAssoc->m_AnimId;
					tFightMoves[moveId].animGroup = foundGroup;
				} else {
					animAssoc = CAnimManager::GetAnimAssociation(ANIM_GROUP_DEFAULT, animName);
					if (animAssoc) {
						tFightMoves[moveId].animId = (AnimationId)animAssoc->m_AnimId;
						tFightMoves[moveId].animGroup = ANIM_GROUP_DEFAULT;
					} else
						debug("Fight data: missing standard animation %s; keeping the built-in move mapping\n", animName);
				}
			} else {
				tFightMoves[moveId].animId = ANIM_ID_WALK;
			}
		}
	}
}

void
CPed::SetInvestigateEvent(eScriptEventType event, CVector2D pos, float distanceToCountDone, uint16 time, float angle)
{
	if (!IsPedInControl() || CharCreatedBy == MISSION_CHAR)
		return;

	SetStoredState();
	bFindNewNodeAfterStateRestore = false;
	SetPedState(PED_INVESTIGATE_EVENT);
	m_chatTimer = CTimer::GetTimeInMilliseconds() + time;
	m_eventType = event;
	m_eventOrThreat = pos;
	m_distanceToCountSeekDone = distanceToCountDone;
	m_fAngleToEvent = angle;

	if (m_eventType >= SCRIPT_EVENT_ICECREAM)
		m_lookTimer = 0;
	else
		CAnimManager::BlendAnimation(GetClump(), ANIM_GROUP_DEFAULT, ANIM_ID_HANDSCOWER, 4.0f);

}

void
CPed::InvestigateEvent(void)
{
	CAnimBlendAssociation *animAssoc;
	AnimationId animToPlay;
	AssocGroupId animGroup;

	if (m_nWaitState == WAITSTATE_TURN180)
		return;

	if (CTimer::GetTimeInMilliseconds() > m_chatTimer) {

		if (m_chatTimer) {
			if (m_eventType < SCRIPT_EVENT_UNK)
				SetWaitState(WAITSTATE_TURN180, nil);

			m_chatTimer = 0;
		} else {
			ClearInvestigateEvent();
		}
		return;
	}

	CVector2D vecDist = m_eventOrThreat - GetPosition();
	float distSqr = vecDist.MagnitudeSqr();
	if (sq(m_distanceToCountSeekDone) >= distSqr) {

		m_fRotationDest = CGeneral::GetRadianAngleBetweenPoints(vecDist.x, vecDist.y, 0.0f, 0.0f);
		SetMoveState(PEDMOVE_STILL);

		switch (m_eventType) {
			case SCRIPT_EVENT_DEAD_PED:
			case SCRIPT_EVENT_HIT_AND_RUN:
			case SCRIPT_EVENT_HIT_AND_RUN_COP:

				if (CTimer::GetTimeInMilliseconds() > m_lookTimer) {
					animAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_ROADCROSS);

					if (animAssoc) {
						animAssoc->m_BlendDelta = -8.0f;
						animAssoc->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE;
						if (m_pEventEntity)
							SetLookFlag(m_pEventEntity, true);

						SetLookTimer(CGeneral::GetRandomNumberInRange(1500, 4000));

					} else if (CGeneral::GetRandomNumber() & 3) {
						ClearLookFlag();
						CAnimManager::BlendAnimation(GetClump(), ANIM_GROUP_DEFAULT, ANIM_ID_ROADCROSS, 4.0f);

						SetLookTimer(CGeneral::GetRandomNumberInRange(1000, 2500));
						Say(SOUND_PED_CHAT_EVENT);

					} else {
						ClearInvestigateEvent();
					}
				}
				break;
			case SCRIPT_EVENT_FIRE:
			case SCRIPT_EVENT_EXPLOSION:

				if (bHasACamera && CTimer::GetTimeInMilliseconds() > m_lookTimer) {
					animAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_IDLE_HBHB_0);
					if (!animAssoc)
						animAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_IDLE);

					if (animAssoc && animAssoc->m_AnimId == ANIM_ID_IDLE_HBHB_0) {
						CAnimManager::BlendAnimation(GetClump(), m_animGroup, ANIM_ID_IDLE, 4.0f);
						SetLookTimer(CGeneral::GetRandomNumberInRange(1000, 2500));

					} else if (CGeneral::GetRandomNumber() & 3) {
						CAnimManager::BlendAnimation(GetClump(), ANIM_GROUP_DEFAULT, ANIM_ID_IDLE_HBHB_0, 4.0f);
						SetLookTimer(CGeneral::GetRandomNumberInRange(2500, 5000));
						if (!CGame::germanGame)
							Say(SOUND_PED_CHAT_EVENT);

					} else {
						m_chatTimer = 0;
					}

				} else if (CTimer::GetTimeInMilliseconds() > m_lookTimer) {
					animAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_IDLE);
					if (!animAssoc)
						animAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_IDLE_HBHB_0);

					if (!animAssoc)
						animAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_XPRESSSCRATCH);

					if (animAssoc && animAssoc->m_AnimId == ANIM_ID_IDLE) {
						if (CGeneral::GetRandomNumber() & 1)
							animToPlay = ANIM_ID_IDLE_HBHB_0;
						else
							animToPlay = ANIM_ID_XPRESSSCRATCH;

						CAnimManager::BlendAnimation(GetClump(), ANIM_GROUP_DEFAULT, animToPlay, 4.0f);
						SetLookTimer(CGeneral::GetRandomNumberInRange(1500, 4000));

					} else if (animAssoc && animAssoc->m_AnimId == ANIM_ID_IDLE_HBHB_0) {
						animAssoc->m_BlendDelta = -8.0f;
						animAssoc->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE;
						if (CGeneral::GetRandomNumber() & 1) {
							animToPlay = ANIM_ID_IDLE;
							animGroup = m_animGroup;
						} else {
							animToPlay = ANIM_ID_XPRESSSCRATCH;
							animGroup = ANIM_GROUP_DEFAULT;
						}

						CAnimManager::BlendAnimation(GetClump(), animGroup, animToPlay, 4.0f);
						SetLookTimer(CGeneral::GetRandomNumberInRange(1000, 2500));

					} else {
						if (CGeneral::GetRandomNumber() & 1) {
							animToPlay = ANIM_ID_IDLE;
							animGroup = m_animGroup;
						} else {
							animToPlay = ANIM_ID_IDLE_HBHB_0;
							animGroup = ANIM_GROUP_DEFAULT;
						}

						CAnimManager::BlendAnimation(GetClump(), animGroup, animToPlay, 4.0f);
						SetLookTimer(CGeneral::GetRandomNumberInRange(1000, 2500));
					}
					if (!CGame::germanGame)
						Say(SOUND_PED_CHAT_EVENT);
				}
				break;
			case SCRIPT_EVENT_ICECREAM:
			case SCRIPT_EVENT_SHOPSTALL:

				m_fRotationDest = m_fAngleToEvent;
				if (CTimer::GetTimeInMilliseconds() > m_lookTimer) {

					if (m_lookTimer) {
						animAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_ROADCROSS);

						if (animAssoc) {
							animAssoc->m_BlendDelta = -8.0f;
							animAssoc->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE;
							if (m_eventType == SCRIPT_EVENT_ICECREAM)
								animToPlay = ANIM_ID_IDLE_CHAT;
							else
								animToPlay = ANIM_ID_XPRESSSCRATCH;
							CAnimManager::BlendAnimation(GetClump(), ANIM_GROUP_DEFAULT, animToPlay,4.0f);
							SetLookTimer(CGeneral::GetRandomNumberInRange(2000, 5000));

						} else {
							animAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_IDLE_CHAT);
							if (animAssoc) {
								animAssoc->m_BlendDelta = -8.0f;
								animAssoc->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE;
								ClearInvestigateEvent();
							} else {
								animAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_XPRESSSCRATCH);
								if (animAssoc) {
									animAssoc->m_BlendDelta = -8.0f;
									animAssoc->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE;
								}
								ClearInvestigateEvent();
							}
						}
					} else {
						CAnimManager::BlendAnimation(GetClump(), ANIM_GROUP_DEFAULT, ANIM_ID_ROADCROSS, 4.0f);
						SetLookTimer(CGeneral::GetRandomNumberInRange(1000, 2500));
					}
				}
				break;
			default:
				return;
		}
	} else {
		m_vecSeekPos.x = m_eventOrThreat.x;
		m_vecSeekPos.y = m_eventOrThreat.y;
		m_vecSeekPos.z = GetPosition().z;
		Seek();

		if (m_eventType < SCRIPT_EVENT_ICECREAM) {
			if (sq(5.0f + m_distanceToCountSeekDone) < distSqr) {
				SetMoveState(PEDMOVE_RUN);
				return;
			}
		}
		if (m_eventType <= SCRIPT_EVENT_EXPLOSION || m_eventType >= SCRIPT_EVENT_SHOPSTALL) {
			SetMoveState(PEDMOVE_WALK);
			return;
		}
		if (distSqr > sq(1.2f)) {
			SetMoveState(PEDMOVE_WALK);
			return;
		}

		bool willStandStill = false;
		for (int i = 0; i < m_numNearPeds; i++) {
			if ((m_eventOrThreat - m_nearPeds[i]->GetPosition()).MagnitudeSqr() < sq(0.4f)) {
				SetMoveState(PEDMOVE_STILL);
				willStandStill = true;
				break;
			}
		}

		if (!willStandStill)
			SetMoveState(PEDMOVE_WALK);
	}
}

void
CPed::ClearInvestigateEvent(void)
{
	CAnimBlendAssociation *animAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_ROADCROSS);
	if (!animAssoc)
		animAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_XPRESSSCRATCH);
	if (!animAssoc)
		animAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_IDLE_HBHB_0);
	if (!animAssoc)
		animAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_IDLE_CHAT);
	if (animAssoc) {
		animAssoc->m_BlendDelta = -8.0f;
		animAssoc->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE;
	}
	if (m_eventType > SCRIPT_EVENT_EXPLOSION)
		m_chatTimer = CTimer::GetTimeInMilliseconds() + 15000;

	bGonnaInvestigateEvent = false;
	m_pEventEntity = nil;
	ClearLookFlag();
	RestorePreviousState();
	if(m_nMoveState == PEDMOVE_NONE || m_nMoveState == PEDMOVE_STILL)
		SetMoveState(PEDMOVE_WALK);
}

// SA CPedDamageResponseCalculator::ComputeWillForceDeath (0x4AD610).
//
// SA decides lethality in two steps: a calculator applies the stats, the armour
// and a "should this hit kill outright" rule, and only then does the ped react.
// reVC's InflictDamage carried the second half only, so a headshot with a pistol
// did exactly the same damage as a shot in the hip - the part that was hit
// changed only which death animation played.
//
// The rule is deliberately narrow, the way SA's is: it applies to the guns that
// can actually do it, it spares the player, peds flagged bNoCriticalHits and the
// rifle headshots that SA lets a ped survive seven times out of eight, and a ped
// sitting in a car dies from a head hit with anything.
bool
CPed::WillForceDeathFrom(eWeaponType method, ePedPieceTypes piece, CEntity *damager, float damage)
{
	if(bNoCriticalHits)
		return false;

	CPed *damagerPed = damager && damager->IsPed() ? (CPed*)damager : nil;

	switch(method){
	case WEAPONTYPE_KATANA:
		// SA gates the katana on the attacker being the player mid-sword-combo.
		// There is no melee combo state in this fork yet, so the random third of
		// the decision stands on its own.
		if(damagerPed == nil || !damagerPed->IsPlayer())
			return false;
		return CGeneral::GetRandomNumber() % 8 == 0;

	case WEAPONTYPE_COLT45:
	case WEAPONTYPE_PYTHON:
	case WEAPONTYPE_SHOTGUN:
	case WEAPONTYPE_STUBBY_SHOTGUN:
	case WEAPONTYPE_SPAS12_SHOTGUN:
	case WEAPONTYPE_TEC9:
	case WEAPONTYPE_UZI:
	case WEAPONTYPE_SILENCED_INGRAM:
	case WEAPONTYPE_MP5:
	case WEAPONTYPE_M4:
	case WEAPONTYPE_RUGER:
	case WEAPONTYPE_SNIPERRIFLE:
	case WEAPONTYPE_LASERSCOPE:
	case WEAPONTYPE_M60:
	case WEAPONTYPE_MINIGUN:
	case WEAPONTYPE_UZI_DRIVEBY:
		if(bInVehicle && !IsPlayer())
			return piece == PED_PIECE_HEAD;

		if(!IsPlayer() && damagerPed && damagerPed->IsPlayer()){
			// A player aiming at the head with free aim lands a lethal one.
			CPlayerPedData *playerData = damagerPed->IsPlayer() ? ((CPlayerPed*)damagerPed)->GetPlayerData() : nil;
			if(playerData && (playerData->m_nTargetBone == BONE_HEAD || playerData->m_bFreeAiming))
				return piece == PED_PIECE_HEAD;
		}

		if(IsPlayer())
			return false;
		// A rifle headshot is a kill; everything else only sometimes.
		if(method == WEAPONTYPE_SNIPERRIFLE || method == WEAPONTYPE_LASERSCOPE)
			return piece == PED_PIECE_HEAD;
		return piece == PED_PIECE_HEAD && CGeneral::GetRandomNumber() % 8 != 0;

	case WEAPONTYPE_GRENADE:
	case WEAPONTYPE_DETONATOR_GRENADE:
	case WEAPONTYPE_ROCKET:
	case WEAPONTYPE_ROCKETLAUNCHER:
	case WEAPONTYPE_EXPLOSION:
		return !IsPlayer() && !bInVehicle && damage + 1.0f > m_fHealth;

	default:
		return false;
	}
}

// SA has no per-part multiplier of its own - the calculator's damage is flat and
// the part decides the lethality rule above - but the ped's hit-collision model
// already splits the body into ten pieces and nothing was reading that split.
// These factors are the ones the SA melee and gun tables imply: a head is worth
// four times a torso hit and a limb a little less.
float
CPed::GetBodyPartDamageMult(ePedPieceTypes piece)
{
	switch(piece){
	case PED_PIECE_HEAD:		return 4.0f;
	case PED_PIECE_TORSO:		return 1.0f;
	case PED_PIECE_ASS:			return 0.9f;
	case PED_PIECE_LEFT_ARM:
	case PED_PIECE_RIGHT_ARM:	return 0.6f;
	case PED_PIECE_LEFT_LEG:
	case PED_PIECE_RIGHT_LEG:	return 0.55f;
	default:					return 1.0f;
	}
}

bool
CPed::InflictDamage(CEntity *damagedBy, eWeaponType method, float damage, ePedPieceTypes pedPiece, uint8 direction)
{
	CPlayerPed *player = FindPlayerPed();
	float dieDelta = 4.0f;
	float dieSpeed = 0.0f;
	AnimationId dieAnim = ANIM_ID_KO_SHOT_FRONT_0;
	bool headShot = false;
	bool willLinger = false;
	int random;

	if (damagedBy == FindPlayerPed() && damagedBy != this && damage > 3.0f)
		++CWorld::Players[CWorld::PlayerInFocus].m_nHavocLevel;

	if (player == this) {
		if (!player->CanBeDamaged())
			return false;

		if (damagedBy && damagedBy->IsPed() && ((CPed*)damagedBy)->m_nPedType == PEDTYPE_GANG7)
			return false;

		if ((method == WEAPONTYPE_FLAMETHROWER || method == WEAPONTYPE_MOLOTOV) && CWorld::Players[CWorld::PlayerInFocus].m_bFireproof)
			return false;

		player->AnnoyPlayerPed(false);
	}

	if (DyingOrDead())
		return false;

	if (method == WEAPONTYPE_DROWNING && !bDrownsInWater)
		return false;

	if (!bUsesCollision && (!bInVehicle || m_nPedState != PED_DRIVING) && method != WEAPONTYPE_DROWNING)
		return false;

	if (bOnlyDamagedByPlayer && damagedBy != player && damagedBy != FindPlayerVehicle() &&
		method != WEAPONTYPE_DROWNING && method != WEAPONTYPE_EXPLOSION)
		return false;

	// SA decides lethality before the arithmetic (CPedDamageResponseCalculator),
	// so a gun headshot is not "more damage" but a kill by rule, and DROWNING and
	// FALL keep their flat, part-independent arithmetic.
	bool forcedDeath = false;
	if(method != WEAPONTYPE_DROWNING && method != WEAPONTYPE_FALL)
		forcedDeath = WillForceDeathFrom(method, pedPiece, damagedBy, damage);

	float healthImpact;
	if (IsPlayer())
		healthImpact = damage * 0.33f;
	else
		healthImpact = damage * m_pedStats->m_fDefendWeakness;

	// The part that was hit scales the damage, unless a rule already decided the
	// outcome. Before this the piece only chose the death animation, so a shot
	// to the head and a shot to the hip cost the same health.
	if(!forcedDeath && method != WEAPONTYPE_DROWNING && method != WEAPONTYPE_FALL)
		healthImpact *= GetBodyPartDamageMult(pedPiece);

	if(forcedDeath)
		healthImpact = m_fArmour + m_fHealth + 1.0f;

	if (!IsPlayer() &&
		(method == WEAPONTYPE_SCREWDRIVER || method == WEAPONTYPE_KNIFE || (method >= WEAPONTYPE_CLEAVER && method <= WEAPONTYPE_CHAINSAW)))
		m_bleedCounter = 200;

	bool detectDieAnim = true;
	if (m_nPedState == PED_GETUP) {
		if (!IsPedHeadAbovePos(-0.3f)) {
			if (RpAnimBlendClumpGetFirstAssociation(GetClump(), ANIMATION_IS_FRONT))
				dieAnim = ANIM_ID_FLOOR_HIT_F;
			else
				dieAnim = ANIM_ID_FLOOR_HIT;
			dieDelta *= 2.0f;
			dieSpeed = 0.5f;
			detectDieAnim = false;
		}
	} else if (m_nPedState == PED_FALL) {
		CAnimBlendAssociation *fallAssoc = RpAnimBlendClumpGetFirstAssociation(GetClump(), ANIMATION_IS_PARTIAL);
		if (!fallAssoc || fallAssoc->IsPlaying()) {
			if (fallAssoc && fallAssoc->m_BlendDelta >= 0.0f)
				dieAnim = ANIM_ID_UNDEFINED;
			else
				dieAnim = ANIM_ID_KO_SHOT_FRONT_0;
		} else {
			if (fallAssoc->m_Flags & ANIMATION_IS_FRONT)
				dieAnim = ANIM_ID_FLOOR_HIT_F;
			else
				dieAnim = ANIM_ID_FLOOR_HIT;

			dieDelta *= 2.0f;
			dieSpeed = 0.5f;
		}
		detectDieAnim = false;
	}

	if (detectDieAnim) {
		switch (method) {
			case WEAPONTYPE_UNARMED:
			case WEAPONTYPE_BRASSKNUCKLE:
				if (bMeleeProof)
					return false;

				if (m_nPedState == PED_FALL) {
					if (IsPedHeadAbovePos(-0.3f)) {
						dieAnim = ANIM_ID_UNDEFINED;
					} else {
						if (RpAnimBlendClumpGetFirstAssociation(GetClump(), ANIMATION_IS_FRONT))
							dieAnim = ANIM_ID_FLOOR_HIT_F;
						else
							dieAnim = ANIM_ID_FLOOR_HIT;
						dieDelta = dieDelta * 2.0f;
						dieSpeed = 0.5f;
					}
				} else {
					switch (direction) {
						case 0:
							dieAnim = ANIM_ID_KO_SKID_FRONT;
							break;
						case 1:
							dieAnim = ANIM_ID_KO_SPIN_R;
							break;
						case 2:
							dieAnim = ANIM_ID_KO_SKID_BACK;
							break;
						case 3:
							dieAnim = ANIM_ID_KO_SPIN_L;
							break;
						default:
							break;
					}
				}
				break;
			case WEAPONTYPE_SCREWDRIVER:
			case WEAPONTYPE_GOLFCLUB:
			case WEAPONTYPE_NIGHTSTICK:
			case WEAPONTYPE_KNIFE:
			case WEAPONTYPE_BASEBALLBAT:
			case WEAPONTYPE_HAMMER:
			case WEAPONTYPE_CLEAVER:
			case WEAPONTYPE_MACHETE:
			case WEAPONTYPE_KATANA:
			case WEAPONTYPE_CHAINSAW:
				if (bMeleeProof)
					return false;

				if (method != WEAPONTYPE_KATANA ||
					damagedBy != FindPlayerPed()
					|| FindPlayerPed()->m_nPedState != PED_FIGHT
					|| FindPlayerPed()->m_curFightMove != FIGHTMOVE_MELEE1 && FindPlayerPed()->m_curFightMove != FIGHTMOVE_MELEE2
					|| CGeneral::GetRandomNumber() & 3) {

					if (m_nPedState == PED_FALL) {
						if (IsPedHeadAbovePos(-0.3f)) {
							dieAnim = ANIM_ID_UNDEFINED;
						} else {
							if (RpAnimBlendClumpGetFirstAssociation(GetClump(), ANIMATION_IS_FRONT))
								dieAnim = ANIM_ID_FLOOR_HIT_F;
							else
								dieAnim = ANIM_ID_FLOOR_HIT;
							dieDelta = dieDelta * 2.0f;
							dieSpeed = 0.5f;
						}
					} else if (damagedBy != FindPlayerPed() || FindPlayerPed()->m_curFightMove != FIGHTMOVE_MELEE2) {
						if (damagedBy != FindPlayerPed() || FindPlayerPed()->m_curFightMove != FIGHTMOVE_MELEE3) {
							switch (direction) {
								case 0:
									dieAnim = ANIM_ID_KO_SKID_FRONT;
									break;
								case 1:
									dieAnim = ANIM_ID_KO_SPIN_R;
									break;
								case 2:
									dieAnim = ANIM_ID_KO_SKID_BACK;
									break;
								case 3:
									dieAnim = ANIM_ID_KO_SPIN_L;
									break;
								default:
									break;
							}
						} else {
							dieAnim = ANIM_ID_KO_SHOT_STOM;
						}
					} else {
						dieAnim = ANIM_ID_KO_SHOT_FACE;
					}
				} else {
					dieAnim = ANIM_ID_KO_SHOT_FACE;
					RemoveBodyPart(PED_NODE_HEAD, direction);
					headShot = true;
					willLinger = true;
				}
				break;
			case WEAPONTYPE_COLT45:
			case WEAPONTYPE_SHOTGUN:
			case WEAPONTYPE_STUBBY_SHOTGUN:
			case WEAPONTYPE_SPAS12_SHOTGUN:
			case WEAPONTYPE_TEC9:
			case WEAPONTYPE_UZI:
			case WEAPONTYPE_SILENCED_INGRAM:
			case WEAPONTYPE_MP5:
			case WEAPONTYPE_M4:
			case WEAPONTYPE_RUGER:
			case WEAPONTYPE_SNIPERRIFLE:
			case WEAPONTYPE_LASERSCOPE:
			case WEAPONTYPE_M60:
			case WEAPONTYPE_MINIGUN:
			case WEAPONTYPE_UZI_DRIVEBY:

				if (bBulletProof)
					return false;

				bool dontRemoveLimb;
				if (IsPlayer() || bNoCriticalHits)
					dontRemoveLimb = true;
				else if (method != WEAPONTYPE_M4 && method != WEAPONTYPE_RUGER && method != WEAPONTYPE_SNIPERRIFLE &&
						method != WEAPONTYPE_LASERSCOPE) {
					if (method == WEAPONTYPE_SHOTGUN)
						dontRemoveLimb = CGeneral::GetRandomNumber() & 7;
					else
						dontRemoveLimb = CGeneral::GetRandomNumber() & 15;
				} else
					dontRemoveLimb = false;

				if (dontRemoveLimb) {
					if (method == WEAPONTYPE_SHOTGUN) {
						switch (direction) {
							case 0:
								dieAnim = ANIM_ID_KO_SKID_FRONT;
								break;
							case 1:
								dieAnim = ANIM_ID_KO_SPIN_R;
								break;
							case 2:
								dieAnim = ANIM_ID_KO_SKID_BACK;
								break;
							case 3:
								dieAnim = ANIM_ID_KO_SPIN_L;
								break;
							default:
								break;
						}
					} else
						dieAnim = ANIM_ID_KO_SHOT_FRONT_0;

					willLinger = false;
				} else {
					switch (pedPiece) {
						case PED_PIECE_TORSO:
							willLinger = false;
							dieAnim = ANIM_ID_KO_SHOT_FRONT_0;
							break;
						case PED_PIECE_ASS:
							willLinger = false;
							dieAnim = ANIM_ID_KO_SHOT_STOM;
							break;
						case PED_PIECE_LEFT_ARM:
							dieAnim = ANIM_ID_KO_SHOT_FRONT_0;
							RemoveBodyPart(PED_NODE_LEFT_ARM, direction);
							willLinger = true;
							break;
						case PED_PIECE_RIGHT_ARM:
							dieAnim = ANIM_ID_KO_SHOT_FRONT_0;
							RemoveBodyPart(PED_NODE_RIGHT_ARM, direction);
							willLinger = true;
							break;
						case PED_PIECE_LEFT_LEG:
							dieAnim = ANIM_ID_KO_SHOT_FRONT_0;
							RemoveBodyPart(PED_NODE_LEFT_LEG, direction);
							willLinger = true;
							break;
						case PED_PIECE_RIGHT_LEG:
							dieAnim = ANIM_ID_KO_SHOT_FRONT_0;
							RemoveBodyPart(PED_NODE_RIGHT_LEG, direction);
							willLinger = true;
							break;
						case PED_PIECE_HEAD:
							dieAnim = ANIM_ID_KO_SHOT_FACE;
							RemoveBodyPart(PED_NODE_HEAD, direction);
							headShot = true;
							willLinger = true;
							break;
						default:
							break;
					}
				}
				break;
			case WEAPONTYPE_GRENADE:
			case WEAPONTYPE_ROCKETLAUNCHER:
			case WEAPONTYPE_EXPLOSION:
				if (bExplosionProof)
					return false;

				if (CGame::nastyGame && !IsPlayer() && !bInVehicle &&
					1.0f + healthImpact > m_fArmour + m_fHealth) {

					random = CGeneral::GetRandomNumber();
					if (random & 1)
						RemoveBodyPart(PED_NODE_LEFT_ARM, direction);
					if (random & 2)
						RemoveBodyPart(PED_NODE_RIGHT_LEG, direction);
					if (random & 4)
						RemoveBodyPart(PED_NODE_HEAD, direction);
					if (random & 8)
						RemoveBodyPart(PED_NODE_RIGHT_ARM, direction);
					if (random & 0x10)
						RemoveBodyPart(PED_NODE_LEFT_LEG, direction);
					if (bBodyPartJustCameOff)
						willLinger = true;
				}
				// fall through
			case WEAPONTYPE_MOLOTOV:
				if (bExplosionProof)
					return false;

				switch (direction) {
					case 0:
						dieAnim = ANIM_ID_KO_SKID_FRONT;
						break;
					case 1:
						dieAnim = ANIM_ID_KO_SPIN_R;
						break;
					case 2:
						dieAnim = ANIM_ID_KO_SKID_BACK;
						break;
					case 3:
						dieAnim = ANIM_ID_KO_SPIN_L;
						break;
					default:
						break;
				}
				break;
			case WEAPONTYPE_FLAMETHROWER:
				if (bFireProof)
					return false;

				dieAnim = ANIM_ID_KO_SHOT_FRONT_0;
				break;
			case WEAPONTYPE_RAMMEDBYCAR:
			case WEAPONTYPE_RUNOVERBYCAR:
				if (bCollisionProof)
					return false;

				random = CGeneral::GetRandomNumber() & 3;
				switch (random) {
					case 0:
						if ((pedPiece != PED_PIECE_LEFT_ARM || random <= 1)
							&& (pedPiece != PED_PIECE_ASS || random != 1)) {
							if (pedPiece == PED_PIECE_RIGHT_ARM && random > 1
								|| pedPiece == PED_PIECE_ASS && random == 2)
						
								dieAnim = ANIM_ID_KO_SPIN_L;
							else
								dieAnim = ANIM_ID_KO_SKID_FRONT;
						} else
							dieAnim = ANIM_ID_KO_SPIN_R;

						break;
					case 1:
						if (m_nPedState == PED_EVADE_DIVE)
							dieAnim = ANIM_ID_KD_LEFT;
						else
							dieAnim = ANIM_ID_KO_SPIN_R;
						break;
					case 2:
						if ((pedPiece != PED_PIECE_LEFT_ARM || random <= 1)
							&& (pedPiece != PED_PIECE_ASS || random != 1)) {
							if ((pedPiece != PED_PIECE_RIGHT_ARM || random <= 1)
								&& (pedPiece != PED_PIECE_ASS || random != 2)) {
								dieAnim = ANIM_ID_KO_SKID_BACK;
							} else {
								dieAnim = ANIM_ID_KD_RIGHT;
							}
						} else
							dieAnim = ANIM_ID_KD_LEFT;
						break;
					case 3:
						if (m_nPedState == PED_EVADE_DIVE)
							dieAnim = ANIM_ID_KD_RIGHT;
						else
							dieAnim = ANIM_ID_KO_SPIN_L;
						break;
					default:
						break;
				}
				if (damagedBy && pedPiece != PED_PIECE_TORSO) {
					CVehicle *vehicle = (CVehicle*)damagedBy;
					if (method == WEAPONTYPE_RAMMEDBYCAR) {
						float vehSpeed = vehicle->m_vecMoveSpeed.Magnitude();
						dieDelta = 8.0f * vehSpeed + 4.0f;
					} else {
						float vehSpeed = vehicle->m_vecMoveSpeed.Magnitude();
						dieDelta = 12.0f * vehSpeed + 4.0f;
						dieSpeed = 16.0f * vehSpeed + 1.0f;
					}
				}
				break;
			case WEAPONTYPE_DROWNING:
				dieAnim = ANIM_ID_DROWN;
				break;
			case WEAPONTYPE_FALL:
				if (bCollisionProof)
					return false;

				switch (direction) {
					case 0:
						dieAnim = ANIM_ID_KO_SKID_FRONT;
						break;
					case 1:
						dieAnim = ANIM_ID_KO_SPIN_R;
						break;
					case 2:
						dieAnim = ANIM_ID_KO_SKID_BACK;
						break;
					case 3:
						dieAnim = ANIM_ID_KO_SPIN_L;
						break;
					default:
						break;
				}
				break;
			default:
				break;
		}
	}

	if (m_fArmour != 0.0f && method != WEAPONTYPE_DROWNING) {
		if (player == this)
			CWorld::Players[CWorld::PlayerInFocus].m_nTimeLastArmourLoss = CTimer::GetTimeInMilliseconds();

		if (healthImpact < m_fArmour) {
			m_fArmour = m_fArmour - healthImpact;
			healthImpact = 0.0f;
		} else {
			healthImpact = healthImpact - m_fArmour;
			m_fArmour = 0.0f;
		}
	}

	if (healthImpact != 0.0f) {
		if (player == this)
			CWorld::Players[CWorld::PlayerInFocus].m_nTimeLastHealthLoss = CTimer::GetTimeInMilliseconds();

		m_lastWepDam = method;
		m_lastDamEntity = damagedBy;
	}

	if (method == WEAPONTYPE_FALL) {
		if (RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_DEFAULT_CAR_ROLLOUT_LHS)) {
			if (m_fHealth >= 1.0 && m_fHealth - healthImpact < 5.0f) {
				m_fHealth = Min(m_fHealth, 5.0f);
				return false;
			}
		}
	}

	if (m_fHealth - healthImpact >= 1.0f && !willLinger) {
		m_fHealth -= healthImpact;
		return false;
	}

	if (bInVehicle) {
		if (method != WEAPONTYPE_DROWNING) {
			if (m_pMyVehicle) {
				CVehicle* pVehicle = m_pMyVehicle;
				bool bDone = false;
				if (m_pMyVehicle->IsBike()) {
					m_fHealth = 0.0f;
					((CBike*)m_pMyVehicle)->KnockOffRider(method, direction, this, false);
					bDone = true;
				} else {
					if (m_pMyVehicle->GetVehicleAppearance() == VEHICLE_APPEARANCE_AUTOMOBILE) {
						if (m_pMyVehicle->pDriver == this) {
							if (m_pMyVehicle->GetStatus() == STATUS_SIMPLE) {
								m_pMyVehicle->SetStatus(STATUS_PHYSICS);
								CCarCtrl::SwitchVehicleToRealPhysics(m_pMyVehicle);
							}
							m_pMyVehicle->AutoPilot.m_nCarMission = MISSION_NONE;
							m_pMyVehicle->AutoPilot.m_nCruiseSpeed = 0;
							m_pMyVehicle->AutoPilot.m_nTempAction = TEMPACT_HANDBRAKESTRAIGHT;
							m_pMyVehicle->AutoPilot.m_nTimeTempAction = CTimer::GetTimeInMilliseconds() + 2000;
						}
					}
					if (m_pMyVehicle->CanPedExitCar(true)) {
						SetObjective(OBJECTIVE_LEAVE_CAR_AND_DIE, m_pMyVehicle);
					} else {
						m_fHealth = 0.0f;
						if (m_pMyVehicle && m_pMyVehicle->pDriver == this) {
							SetRadioStation();
							m_pMyVehicle->SetStatus(STATUS_ABANDONED);
						}
						SetDie(dieAnim, dieDelta, dieSpeed);

						if (damagedBy == FindPlayerPed() && damagedBy != this) {
							CWorld::Players[CWorld::PlayerInFocus].m_nHavocLevel += 10;
							CWorld::Players[CWorld::PlayerInFocus].m_fMediaAttention += 5.f;
						}
					}
				}
				for (int i = 0; i < ARRAY_SIZE(pVehicle->pPassengers); i++) {
					CPed* passenger = pVehicle->pPassengers[i];
					if (passenger && passenger != this && damagedBy)
						passenger->ReactToAttack(damagedBy);
				}

				CPed *driverOfVeh = pVehicle->pDriver;
				if (driverOfVeh && driverOfVeh != this && damagedBy)
					driverOfVeh->ReactToAttack(damagedBy);

				if (damagedBy == FindPlayerPed() || damagedBy && damagedBy == FindPlayerVehicle()) {
					CDarkel::RegisterKillByPlayer(this, method, headShot);
					m_threatEntity = FindPlayerPed();
				} else {
					CDarkel::RegisterKillNotByPlayer(this, method);
				}
				if (bDone)
					return true;
			}
			m_fHealth = 1.0f;
			return false;
		}
		m_fHealth = 0.0f;
		if (player == this)
			m_pMyVehicle->SetStatus(STATUS_PLAYER_DISABLED);

		SetDie(ANIM_ID_UNDEFINED, 4.0f, 0.0f);
		return true;
	} else {
		m_fHealth = 0.0f;
		SetDie(dieAnim, dieDelta, dieSpeed);

		if (damagedBy == player || damagedBy && damagedBy == FindPlayerVehicle()) {
			CDarkel::RegisterKillByPlayer(this, method, headShot);
			CWorld::Players[CWorld::PlayerInFocus].m_nHavocLevel += 10;
			CWorld::Players[CWorld::PlayerInFocus].m_fMediaAttention += 5.f;
			m_threatEntity = player;
		} else {
			CDarkel::RegisterKillNotByPlayer(this, method);
		}
		if (method == WEAPONTYPE_DROWNING) {
			bIsInTheAir = false;
			if (FindPlayerPed() == this)
				CStats::TimesDrowned++;
		}

		return true;
	}
}

static RwObject*
SetPedAtomicVisibilityCB(RwObject* object, void* data)
{
	if (data == nil)
		RpAtomicSetFlags((RpAtomic*)object, 0);
	return object;
}

static RwFrame*
RecurseFrameChildrenVisibilityCB(RwFrame* frame, void* data)
{
	RwFrameForAllObjects(frame, SetPedAtomicVisibilityCB, data);
	RwFrameForAllChildren(frame, RecurseFrameChildrenVisibilityCB, nil);
	return frame;
}

static RwObject*
CloneAtomicToFrameCB(RwObject *frame, void *data)
{
	RpAtomic *newAtomic = RpAtomicClone((RpAtomic*)frame);
	RpAtomicSetFrame(newAtomic, (RwFrame*)data);
	RpClumpAddAtomic(flyingClumpTemp, newAtomic);
	CVisibilityPlugins::SetAtomicRenderCallback(newAtomic, nil);
	return frame;
}

static RwFrame*
RecurseFrameChildrenToCloneCB(RwFrame *frame, void *data)
{
	RwFrame *newFrame = RwFrameCreate();
	RwFrameAddChild((RwFrame*)data, newFrame);
	RwFrameTransform(newFrame, RwFrameGetMatrix(frame), rwCOMBINEREPLACE);
	RwFrameForAllObjects(frame, CloneAtomicToFrameCB, newFrame);
	RwFrameForAllChildren(frame, RecurseFrameChildrenToCloneCB, newFrame);
	return newFrame;
}

void
CPed::RemoveBodyPart(ePedNode nodeId, int8 direction)
{
	RwFrame *frame;
	CVector pos;

	if(nodeId < 0 || nodeId >= TOTAL_PED_NODES || m_pFrames[nodeId] == nil)
		return;
	frame = m_pFrames[nodeId]->Frame;
	if (frame) {
		if (CGame::nastyGame) {
			if (CEntity::GetIsOnScreen()) {
				m_pedIK.GetComponentPosition(pos, nodeId);
				CParticle::AddParticle(PARTICLE_TEST, pos,
					CVector(0.0f, 0.0f, 0.0f),
					nil, 0.1f, 0, 0, 0, 0);

				for (int i = 0; i < 16; i++) {
					CParticle::AddParticle(PARTICLE_BLOOD_SMALL,
						pos,
						CVector(0.0f, 0.0f, 0.03f),
						nil, 0.0f, 0, 0, 0, 0);
				}
			}
			bBodyPartJustCameOff = true;
			m_bodyPartBleeding = nodeId;
		}
	} else {
		printf("Trying to remove ped component");
	}
}

CObject*
CPed::SpawnFlyingComponent(int pedNode, int8 direction)
{
	// VC doesn't have detachable limbs :shrug:
	return nil;
}

// III leftover and unused
void
CPed::ApplyHeadShot(eWeaponType weaponType, CVector pos, bool evenOnPlayer)
{
	CVector pos2 = CVector(
		pos.x,
		pos.y,
		pos.z + 0.1f
	);

	if (!IsPlayer() || evenOnPlayer) {
		++CStats::HeadsPopped;

		// BUG: This condition will always return true. Even fixing it won't work, because these states are unused.
		// if (m_nPedState != PED_PASSENGER || m_nPedState != PED_TAXI_PASSENGER) {
			SetDie();
		// }

		bBodyPartJustCameOff = true;
		m_nPedStateTimer = CTimer::GetTimeInMilliseconds() + 150;

		RemoveBodyPart(PED_NODE_HEAD, 0);
		CParticle::AddParticle(PARTICLE_TEST, pos2,
			CVector(0.0f, 0.0f, 0.0f), nil, 0.2f, 0, 0, 0, 0);

		if (CEntity::GetIsOnScreen()) {
			for(int i=0; i < 32; i++) {
				CParticle::AddParticle(PARTICLE_BLOOD_SMALL,
					pos2, CVector(0.0f, 0.0f, 0.03f),
					nil, 0.0f, 0, 0, 0, 0);
			}

			for (int i = 0; i < 16; i++) {
				CParticle::AddParticle(PARTICLE_DEBRIS2,
					pos2,
					CVector(0.0f, 0.0f, 0.01f),
					nil, 0.0f, 0, 0, 0, 0);
			}
		}
	}
}

bool
CPed::IsPedHeadAbovePos(float zOffset)
{
	return zOffset + GetPosition().z < GetNodePosition(PED_NODE_HEAD).z;
}

bool
CPed::PlacePedOnDryLand(void)
{
	float waterLevel = 0.0f;
	CEntity *foundEnt = nil;
	CColPoint foundCol;
	float foundColZ;

	CWaterLevel::GetWaterLevelNoWaves(GetPosition().x, GetPosition().y, GetPosition().z, &waterLevel);

	CVector potentialGround = GetPosition();
	potentialGround.z = waterLevel;

	if (!CWorld::TestSphereAgainstWorld(potentialGround, 5.0f, nil, true, false, false, false, false, false))
		return false;

	CVector potentialGroundDist = gaTempSphereColPoints[0].point - GetPosition();
	potentialGroundDist.z = 0.0f;
	potentialGroundDist.Normalise();

	CVector posToCheck = 0.5f * potentialGroundDist + gaTempSphereColPoints[0].point;
	posToCheck.z = 3.0f + waterLevel;

	if (CWorld::ProcessVerticalLine(posToCheck, waterLevel - 1.0f, foundCol, foundEnt, true, true, false, true, false, false, nil)) {
		foundColZ = foundCol.point.z;
		if (foundColZ >= waterLevel) {
			posToCheck.z = 0.8f + foundColZ;
			SetPosition(posToCheck);
			bIsStanding = true;
			bWasStanding = true;
			return true;
		}
	}

	posToCheck = 5.0f * potentialGroundDist + GetPosition();
	posToCheck.z = 3.0f + waterLevel;

	if (!CWorld::ProcessVerticalLine(posToCheck, waterLevel - 1.0f, foundCol, foundEnt, true, true, false, true, false, false, nil))
		return false;

	foundColZ = foundCol.point.z;
	if (foundColZ < waterLevel)
		return false;

	posToCheck.z = 0.8f + foundColZ;
	SetPosition(posToCheck);
	bIsStanding = true;
	bWasStanding = true;
	return true;
}

void
CPed::CollideWithPed(CPed *collideWith)
{
	CAnimBlendAssociation *animAssoc;
	AnimationId animToPlay;

	bool weAreMissionChar = CharCreatedBy == MISSION_CHAR;
	bool heIsMissionChar = collideWith->CharCreatedBy == MISSION_CHAR;
	CVector posDiff = collideWith->GetPosition() - GetPosition();
	int waitTime = 0;

	if (weAreMissionChar || !collideWith->IsPlayer() || collideWith->m_nPedState != PED_MAKE_PHONECALL) {
		if (m_nWaitState == WAITSTATE_SUN_BATHE_IDLE) {
			SetGetUp();
			return;
		}
		if (collideWith->m_nWaitState == WAITSTATE_SUN_BATHE_IDLE) {
			collideWith->SetGetUp();
			return;
		}

		bool weDontLookToHim = DotProduct(posDiff, GetForward()) > 0.0f;
		bool heLooksToUs = DotProduct(posDiff, collideWith->GetForward()) < 0.0f;

		if (m_nMoveState != PEDMOVE_NONE && m_nMoveState != PEDMOVE_STILL) {

			if ((!IsPlayer() || ((CPlayerPed*)this)->MoveSpeed() <= 1.8f)
				&& (IsPlayer() || heIsMissionChar && weAreMissionChar || m_nMoveState != PEDMOVE_RUN && m_nMoveState != PEDMOVE_SPRINT
					|| m_objective == OBJECTIVE_FOLLOW_CHAR_IN_FORMATION && m_pedInObjective == collideWith
					|| collideWith->m_objective == OBJECTIVE_FOLLOW_CHAR_IN_FORMATION && collideWith->m_pedInObjective == this		
					)) {

				if (m_objective != OBJECTIVE_FOLLOW_CHAR_IN_FORMATION && m_objective != OBJECTIVE_GOTO_CHAR_ON_FOOT) {

					if (CTimer::GetTimeInMilliseconds() > m_nPedStateTimer) {

						if (heIsMissionChar || !weAreMissionChar && collideWith->m_nMoveState != PEDMOVE_STILL) {

							if (weAreMissionChar && (m_nPedState == PED_SEEK_POSITION || m_nPedState == PED_SEEK_ENTITY)) {

								if (collideWith->m_nMoveState != PEDMOVE_STILL
									&& (!collideWith->IsPlayer() || collideWith->IsPlayer() && CPad::GetPad(0)->ArePlayerControlsDisabled())) {
									float seekPosDist = (GetPosition() - m_vecSeekPos).MagnitudeSqr2D();
									float heAndSeekPosDist = (collideWith->GetPosition() - m_vecSeekPos).MagnitudeSqr2D();

									if (seekPosDist <= heAndSeekPosDist) {
										waitTime = 1000;
										collideWith->SetWaitState(WAITSTATE_CROSS_ROAD_LOOK, &waitTime);
										collideWith->m_nPedStateTimer = CTimer::GetTimeInMilliseconds() + waitTime;
									} else {
										waitTime = 500;
										SetWaitState(WAITSTATE_CROSS_ROAD_LOOK, &waitTime);
										m_nPedStateTimer = CTimer::GetTimeInMilliseconds() + waitTime;
									}
								} else if (collideWith->m_nMoveState == PEDMOVE_STILL) {
									SetDirectionToWalkAroundObject(collideWith);
								}
							} else {
								if (FindPlayerPed() != m_pedInObjective
									|| m_objective != OBJECTIVE_KILL_CHAR_ANY_MEANS && m_objective != OBJECTIVE_KILL_CHAR_ON_FOOT
									|| collideWith == m_pedInObjective) {

									if (weAreMissionChar || m_pedStats->m_nFear <= 100 - collideWith->m_pedStats->m_nTemper
										|| (collideWith->IsPlayer() || collideWith->m_nMoveState == PEDMOVE_NONE || collideWith->m_nMoveState == PEDMOVE_STILL) &&
										(!collideWith->IsPlayer() || ((CPlayerPed*)collideWith)->MoveSpeed() <= 1.0f)) {
										SetDirectionToWalkAroundObject(collideWith);
										if (!weAreMissionChar)
											Say(SOUND_PED_CHAT);
									} else {
										SetEvasiveStep(collideWith, 2);
									}
								} else if (collideWith->m_nMoveState != PEDMOVE_STILL && GetWeapon()->IsTypeMelee()
									&& collideWith->m_pedInObjective == m_pedInObjective) {

									int colliderIndexAtPlayersKillList = -1;
									int ourIndexAtPlayersKillList = -1;
									for (int i = 0; i < ARRAY_SIZE(((CPlayerPed*)m_pedInObjective)->m_pMeleeList); i++) {
										CPed *pedInKillList = ((CPlayerPed*)m_pedInObjective)->m_pMeleeList[i];
										if (pedInKillList == this) {
											ourIndexAtPlayersKillList = i;
										} else if (pedInKillList == collideWith) {
											colliderIndexAtPlayersKillList = i;
										}
									}
									bool weAreCloserToTargetThenCollider = false;
									if ((GetPosition() - m_vecSeekPos).MagnitudeSqr2D() < (collideWith->GetPosition() - m_vecSeekPos).MagnitudeSqr2D())
										weAreCloserToTargetThenCollider = true;

									if (ourIndexAtPlayersKillList > 0 && !weAreCloserToTargetThenCollider) {
										if (colliderIndexAtPlayersKillList > 0) {
											int time = 300;
											SetWaitState(WAITSTATE_CROSS_ROAD_LOOK, &time);
											m_nPedStateTimer = CTimer::GetTimeInMilliseconds() + time;

										} else if (collideWith->m_pedInObjective == FindPlayerPed()) {
											((CPlayerPed*)m_pedInObjective)->RemovePedFromMeleeList(this);
											int time = 500;
											SetWaitState(WAITSTATE_CROSS_ROAD_LOOK, &time);
											m_nPedStateTimer = CTimer::GetTimeInMilliseconds() + time;
										}
									} else if (!weAreCloserToTargetThenCollider) {
										int time = 300;
										SetWaitState(WAITSTATE_CROSS_ROAD_LOOK, &time);
										m_nPedStateTimer = CTimer::GetTimeInMilliseconds() + time;
									} 
								} else {
									SetDirectionToWalkAroundObject(collideWith);
								}
							}
						} else {
							if (m_pedStats->m_nTemper <= m_pedStats->m_nFear
								|| GetWeapon()->m_eWeaponType != WEAPONTYPE_UNARMED
								|| weAreMissionChar
								|| collideWith->m_nPedType == PEDTYPE_CIVFEMALE
								|| collideWith->m_nPedType == m_nPedType
								|| collideWith->GetWeapon()->m_eWeaponType != WEAPONTYPE_UNARMED) {
								SetDirectionToWalkAroundObject(collideWith);
								Say(SOUND_PED_CHAT);
							} else {
								TurnBody();
								SetAttack(collideWith);
								m_fRotationCur = 0.3f + m_fRotationCur;
								m_fRotationDest = m_fRotationCur;
							}
							m_nPedStateTimer = CTimer::GetTimeInMilliseconds() + CGeneral::GetRandomNumberInRange(250, 450);
						}
					}
				} else {
					if (m_pedInObjective && (collideWith == m_pedInObjective || collideWith->m_pedInObjective == m_pedInObjective) && CTimer::GetTimeInMilliseconds() > m_nPedStateTimer) {
						if (heLooksToUs) {
							SetEvasiveStep(collideWith, 1);
							m_nPedStateTimer = CTimer::GetTimeInMilliseconds() + 3000;
						}
					} else if (weDontLookToHim && IsPedInControl()) {

						if (m_pedStats != collideWith->m_pedStats) {

							if (collideWith->m_pedStats->m_nFear <= 100 - m_pedStats->m_nTemper || collideWith->IsPlayer()
								|| CTimer::GetTimeInMilliseconds() < m_nPedStateTimer) {

								if (collideWith->IsPlayer()) {
									// He's on our right side
									if (DotProduct(posDiff,GetRight()) <= 0.0f)
										m_fRotationCur -= m_headingRate;
									else
										m_fRotationCur += m_headingRate;
								} else {
									// He's on our right side
									if (DotProduct(posDiff, collideWith->GetRight()) <= 0.0f)
										collideWith->m_fRotationCur -= collideWith->m_headingRate;
									else
										collideWith->m_fRotationCur += collideWith->m_headingRate;
								}
							} else {
								SetLookFlag(collideWith, false);
								TurnBody();
								animAssoc = CAnimManager::BlendAnimation(GetClump(), CWeaponInfo::GetWeaponInfo(GetWeapon()->m_eWeaponType)->m_AnimToPlay, ANIM_ID_FIGHT_1, 8.0f);
								if (animAssoc)
									animAssoc->m_Flags |= ANIMATION_IS_FINISH_AUTO_REMOVE;
								m_nPedStateTimer = CTimer::GetTimeInMilliseconds() + 2000;
								if (!heIsMissionChar) {
									CVector2D posDiff2D(posDiff);
									int direction = collideWith->GetLocalDirection(posDiff2D);
									collideWith->StartFightDefend(direction, HITLEVEL_HIGH, 5);
								}
							}
						}
					}
				}
			} else if (collideWith->m_pedStats->m_fDefendWeakness <= 1.5f || heIsMissionChar ||
				m_pedStats->m_fDefendWeakness <= collideWith->m_pedStats->m_fDefendWeakness) {

				// He looks us and we're not at his right side
				if (heLooksToUs && DotProduct(posDiff,collideWith->GetRight()) > 0.0f) {
					CVector moveForce = GetRight();
					moveForce.z += 0.1f;
					ApplyMoveForce(moveForce);
					if (collideWith->m_nMoveState != PEDMOVE_RUN && collideWith->m_nMoveState != PEDMOVE_SPRINT)
						animToPlay = ANIM_ID_HIT_L;
					else
						animToPlay = ANIM_ID_SHOT_LEFTP;
				} else if (heLooksToUs) {
					CVector moveForce = GetRight() * -1.0f;
					moveForce.z += 0.1f;
					ApplyMoveForce(moveForce);
					if (collideWith->m_nMoveState != PEDMOVE_RUN && collideWith->m_nMoveState != PEDMOVE_SPRINT)
						animToPlay = ANIM_ID_HIT_R;
					else
						animToPlay = ANIM_ID_SHOT_RIGHTP;
				} else {
					if (collideWith->m_nMoveState != PEDMOVE_RUN && collideWith->m_nMoveState != PEDMOVE_SPRINT)
						animToPlay = ANIM_ID_HIT_BACK;
					else
						animToPlay = ANIM_ID_SHOT_PARTIAL;
				}

				if (collideWith->IsPedInControl() && CTimer::GetTimeInMilliseconds() > collideWith->m_nPedStateTimer) {
					animAssoc = CAnimManager::BlendAnimation(collideWith->GetClump(), ANIM_GROUP_DEFAULT, animToPlay, 8.0f);
					if (animAssoc)
						animAssoc->m_Flags |= ANIMATION_IS_FINISH_AUTO_REMOVE;
					collideWith->m_nPedStateTimer = CTimer::GetTimeInMilliseconds() + 1000;
					if (m_nPedState == PED_ATTACK)
						DMAudio.PlayOneShot(m_audioEntityId, SOUND_49, 0.0f);
				}
			} else {
				// We're at his right side
				if (DotProduct(posDiff, collideWith->GetRight()) <= 0.0f) {
					CVector moveForce = GetRight() * -1.0f;
					moveForce.z += 0.1f;
					ApplyMoveForce(moveForce);
					if (heLooksToUs)
						animToPlay = ANIM_ID_KO_SPIN_L;
					else
						animToPlay = ANIM_ID_KD_RIGHT;
				} else {
					CVector moveForce = GetRight();
					moveForce.z += 0.1f;
					ApplyMoveForce(moveForce);
					if (heLooksToUs)
						animToPlay = ANIM_ID_KO_SPIN_R;
					else
						animToPlay = ANIM_ID_KD_LEFT;
				}

				if (m_nPedState == PED_ATTACK && collideWith->IsPedInControl())
					DMAudio.PlayOneShot(m_audioEntityId, SOUND_49, 0.0f);

				collideWith->SetFall(3000, animToPlay, 0);
			}
		} else {
			if (!IsPedInControl())
				return;

			if (collideWith->m_nMoveState == PEDMOVE_NONE || collideWith->m_nMoveState == PEDMOVE_STILL)
				return;

			if (m_nPedType != collideWith->m_nPedType || m_nPedType == PEDTYPE_CIVMALE || m_nPedType == PEDTYPE_CIVFEMALE) {

				if (!weAreMissionChar && heLooksToUs && m_pedStats->m_nFear > 100 - collideWith->m_pedStats->m_nTemper) {

					if (CGeneral::GetRandomNumber() & 1 && CTimer::GetTimeInMilliseconds() < m_nPedStateTimer){
						SetEvasiveStep(collideWith, 2);
						m_nPedStateTimer = CTimer::GetTimeInMilliseconds() + 3000;
					} else if (collideWith->m_nMoveState > PEDMOVE_WALK) {
						waitTime = 2000;
						SetWaitState(WAITSTATE_PLAYANIM_DUCK, &waitTime);
					}
				}
			} else if (heLooksToUs
				&& collideWith->m_nPedState != PED_EVADE_STEP
				&& m_nPedState != PED_EVADE_STEP
				&& CTimer::GetTimeInMilliseconds() > m_nPedStateTimer) {

				SetEvasiveStep(collideWith, 1);
				m_nPedStateTimer = CTimer::GetTimeInMilliseconds() + 3000;
			}
		}

		if (IsPlayer()) {
			SetLookFlag(collideWith, true);
			SetLookTimer(800);
		}
	} else {
		bool isRunning = m_nMoveState == PEDMOVE_RUN || m_nMoveState == PEDMOVE_SPRINT;
		SetFindPathAndFlee(collideWith, 5000, !isRunning);
	}
}

void
CPed::KillPedWithCar(CVehicle *car, float impulse)
{
	CVehicleModelInfo *vehModel;
	CColModel *vehColModel;
	uint8 damageDir;
	ePedNode nodeToDamage;
	eWeaponType killMethod;

	if (m_nPedState == PED_FALL || m_nPedState == PED_DIE) {
		if (!m_pCollidingEntity || car->GetStatus() == STATUS_PLAYER)
			m_pCollidingEntity = car;
		return;
	}

	if (m_nPedState == PED_DEAD)
		return;

	if (m_pCurSurface) {
		if (m_pCurSurface->IsVehicle() && (((CVehicle*)m_pCurSurface)->IsBoat()|| IsPlayer()))
			return;
	}

	CVector distVec = GetPosition() - car->GetPosition();

	if ((impulse > 12.0f || car->GetModelIndex() == MI_TRAIN) && !IsPlayer()) {
		nodeToDamage = PED_NODE_NULL;
		killMethod = WEAPONTYPE_RAMMEDBYCAR;
		uint8 randVal = CGeneral::GetRandomNumber() & 3;

		if (car == FindPlayerVehicle()) {
			float carSpeed = car->m_vecMoveSpeed.Magnitude();
			uint8 shakeFreq;
			if (100.0f * carSpeed * 2000.0f / car->m_fMass + 80.0f <= 250.0f) {
				shakeFreq = 100.0f * carSpeed * 2000.0f / car->m_fMass + 80.0f;
			} else {
				shakeFreq = 250.0f;
			}
			CPad::GetPad(0)->StartShake(40000 / shakeFreq, shakeFreq);
		}
		bIsStanding = false;
		damageDir = GetLocalDirection(-m_vecMoveSpeed);
		vehModel = (CVehicleModelInfo *)CModelInfo::GetModelInfo(car->GetModelIndex());
		vehColModel = vehModel->GetColModel();
		float carRightAndDistDotProd = DotProduct(distVec, car->GetRight());

		if (car->GetModelIndex() == MI_TRAIN) {
			killMethod = WEAPONTYPE_RUNOVERBYCAR;
			nodeToDamage = PED_NODE_HEAD;
			m_vecMoveSpeed = 0.9f * car->m_vecMoveSpeed;
			m_vecMoveSpeed.z = 0.0f;
			if (damageDir == 1 || damageDir == 3)
				damageDir = 2;
			if (CGame::nastyGame)
				DMAudio.PlayOneShot(m_audioEntityId, SOUND_SPLATTER, 0.0f);

		// Car doesn't look to us
		} else if (DotProduct(car->m_vecMoveSpeed, car->GetForward()) >= 0.0f){

			if (0.99f * vehColModel->boundingBox.max.x < Abs(carRightAndDistDotProd)) {

				// We're at the right of the car
				if (carRightAndDistDotProd <= 0.0f)
					nodeToDamage = PED_NODE_LEFT_ARM;
				else
					nodeToDamage = PED_NODE_RIGHT_ARM;

				if (Abs(DotProduct(distVec, car->GetForward())) < 0.85f * vehColModel->boundingBox.max.y) {
					killMethod = WEAPONTYPE_RUNOVERBYCAR;
					m_vecMoveSpeed = 0.9f * car->m_vecMoveSpeed;
					m_vecMoveSpeed.z = 0.0f;
					if (damageDir == 1 || damageDir == 3)
						damageDir = 2;
					if (CGame::nastyGame)
						DMAudio.PlayOneShot(m_audioEntityId, SOUND_SPLATTER, 0.0f);

				}
			} else {
				float carFrontAndDistDotProd = DotProduct(distVec, car->GetForward());

				// carFrontAndDistDotProd <= 0.0 car looks to us
				if ((carFrontAndDistDotProd <= 0.1 || randVal <= 1) && randVal != 0) {
					killMethod = WEAPONTYPE_RUNOVERBYCAR;
					nodeToDamage = PED_NODE_HEAD;
					m_vecMoveSpeed = 0.9f * car->m_vecMoveSpeed;
					m_vecMoveSpeed.z = 0.0f;
					if (damageDir == 1 || damageDir == 3)
						damageDir = 2;

					if (CGame::nastyGame)
						DMAudio.PlayOneShot(m_audioEntityId, SOUND_SPLATTER, 0.0f);

				} else {
					nodeToDamage = PED_NODE_UPPER_TORSO;
					float vehColMaxY = vehColModel->boundingBox.max.y;
					float vehColMinY = vehColModel->boundingBox.min.y;
					float vehColMaxZ = vehColModel->boundingBox.max.z;
					float carFrontZ = car->GetForward().z;
					float carHighestZ, carLength;

					if (carFrontZ < -0.2f) {
						// Highest point of car's back
						carHighestZ = (car->GetMatrix() * CVector(0.0f, vehColMinY, vehColMaxZ)).z;
						carLength = vehColMaxY - vehColMinY;

					} else if (carFrontZ > 0.1f) {
						// Highest point of car's front
						carHighestZ = (car->GetMatrix() * CVector(0.0f, vehColMaxY, vehColMaxZ)).z;
						float highestZDist = carHighestZ - GetPosition().z;
						if (highestZDist > 0.0f) {
							GetMatrix().GetPosition().z += 0.5f * highestZDist;
							carHighestZ += highestZDist * 0.25f;
						}
						carLength = vehColMaxY;

					} else {
						// Highest point of car's front
						carHighestZ = (car->GetMatrix() * CVector(0.0f, vehColMaxY, vehColMaxZ)).z;
						carLength = vehColMaxY;
					}

					float pedJumpSpeedToReachHighestZ = (carHighestZ - GetPosition().z) / (carLength / car->m_vecMoveSpeed.Magnitude());

					// TODO: What are we doing down here?
					float unknown = ((CGeneral::GetRandomNumber() % 256) * 0.002 + 1.5) * pedJumpSpeedToReachHighestZ;

					// After this point distVec isn't really distVec.
					distVec = car->m_vecMoveSpeed;
					distVec.Normalise();
					distVec *= 0.2 * unknown;

					if (damageDir != 1 && damageDir != 3)
						distVec.z += unknown;
					else
						distVec.z += 1.5f * unknown;

					m_vecMoveSpeed = distVec;
					damageDir += 2;
					if (damageDir > 3)
						damageDir = damageDir - 4;

					if (car->IsAutomobile()) {
						CObject *bonnet = ((CAutomobile*)car)->RemoveBonnetInPedCollision();

						if (bonnet) {
							if (CGeneral::GetRandomNumber() & 1) {
								bonnet->m_vecMoveSpeed += Multiply3x3(car->GetMatrix(), CVector(0.1f, 0.0f, 0.5f));
							} else {
								bonnet->m_vecMoveSpeed += Multiply3x3(car->GetMatrix(), CVector(-0.1f, 0.0f, 0.5f));
							}
							CVector forceDir = car->GetUp() * 10.0f;
							bonnet->ApplyTurnForce(forceDir, car->GetForward());
						}
					}
				}
			}
		}

		if (car->pDriver) {
			CEventList::RegisterEvent((m_nPedType == PEDTYPE_COP ? SCRIPT_EVENT_HIT_AND_RUN_COP : SCRIPT_EVENT_HIT_AND_RUN), EVENT_ENTITY_PED, this, car->pDriver, 1000);
		}

		ePedPieceTypes pieceToDamage;
		switch (nodeToDamage) {
			case PED_NODE_HEAD:
				pieceToDamage = PED_PIECE_HEAD;
				break;
			case PED_NODE_LEFT_ARM:
				pieceToDamage = PED_PIECE_LEFT_ARM;
				break;
			case PED_NODE_RIGHT_ARM:
				pieceToDamage = PED_PIECE_RIGHT_ARM;
				break;
			default:
				pieceToDamage = PED_PIECE_ASS;
				break;
		}
		InflictDamage(car, killMethod, 1000.0f, pieceToDamage, damageDir);

		if (DyingOrDead()
			&& bIsPedDieAnimPlaying && !m_pCollidingEntity) {
			m_pCollidingEntity = car;
		}
		if (nodeToDamage == PED_NODE_UPPER_TORSO)
			bKnockedUpIntoAir = true;
		else
			bKnockedUpIntoAir = false;

		distVec.Normalise();

		distVec *= Min(car->m_fMass / 1400.0f, 1.0f);
		car->ApplyMoveForce(distVec * -100.0f);
		Say(SOUND_PED_DEFEND);

	} else if (m_vecDamageNormal.z < -0.8f && impulse > 3.0f
		|| impulse > 6.0f && (!IsPlayer() || impulse > 10.0f)) {

		bIsStanding = false;
		uint8 fallDirection = GetLocalDirection(-car->m_vecMoveSpeed);
		float damage;
		if (IsPlayer() && car->GetModelIndex() == MI_TRAIN)
			damage = 150.0f;
		else
			damage = 30.0f;
	
		InflictDamage(car, WEAPONTYPE_RAMMEDBYCAR, damage, PED_PIECE_TORSO, fallDirection);
		SetFall(1000, (AnimationId)(fallDirection + ANIM_ID_KO_SKID_FRONT), true);

		if (OnGround() && !m_pCollidingEntity &&
			(!IsPlayer() || bHasHitWall || car->GetModelIndex() == MI_TRAIN || m_vecDamageNormal.z < -0.8f)) {

			m_pCollidingEntity = car;
		}

		bKnockedUpIntoAir = false;
		if (car->GetModelIndex() != MI_TRAIN && !bHasHitWall) {
			m_vecMoveSpeed = car->m_vecMoveSpeed * 0.75f;
		}
		m_vecMoveSpeed.z = 0.0f;
		distVec.Normalise();
		distVec *= Min(car->m_fMass / 1400.0f, 1.0f);
		car->ApplyMoveForce(distVec * -60.0f);
		Say(SOUND_PED_DEFEND);
	}

	if (IsGangMember()) {
		CPed *driver = car->pDriver;
		if (driver && driver->IsPlayer()
#ifdef FIX_BUGS
			&& (CharCreatedBy != MISSION_CHAR || bRespondsToThreats) && (!m_leader || m_leader != driver)
#endif
			) {
			RegisterThreatWithGangPeds(driver);
		}
	}
}

void
CPed::DriveVehicle(void)
{
	if (bOffscreen)
		return;

	CVehicle *veh = m_pMyVehicle;
	if (veh->IsBike()) {
		CBike *bike = (CBike*)veh;
		float blendDelta = 1.0f;
		float targetUDLean = 0.0f;
		CAnimBlendAssociation *leftAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_BIKE_LEFT);
		CAnimBlendAssociation *rightAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_BIKE_RIGHT);
		CAnimBlendAssociation *stillAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_BIKE_STILL);
		CAnimBlendAssociation *fwdAssoc, *backAssoc;
		if (IsPlayer()) {
			fwdAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_BIKE_FWD);
			backAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_BIKE_BACK);
		}
		CAnimBlendAssociation *walkbackAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_BIKE_PUSHES);
		CAnimBlendAssociation *drivebyAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_BIKE_DRIVEBYLHS);
		if (!drivebyAssoc)
			drivebyAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_BIKE_DRIVEBYRHS);
		if (!drivebyAssoc)
			drivebyAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_BIKE_DRIVEBYFT);

		float velocityFwdDotProd = DotProduct(bike->m_vecMoveSpeed, bike->GetForward());
		if (m_vecTurnSpeed.MagnitudeSqr() > 0.09f) {
			bike->KnockOffRider(WEAPONTYPE_FALL, 2, this, false);
			if (bike->pPassengers[0])
				bike->KnockOffRider(WEAPONTYPE_FALL, 2, bike->pPassengers[0], false);
			return;
		}
		if (!drivebyAssoc && Abs(velocityFwdDotProd) < 0.02f) {
			if (!stillAssoc || stillAssoc->m_BlendAmount < 1.0 && stillAssoc->m_BlendDelta <= 0.0) {
				stillAssoc = CAnimManager::BlendAnimation(GetClump(), bike->m_bikeAnimType, ANIM_ID_BIKE_STILL, 2.0f);
			}
		} else {
			if (velocityFwdDotProd >= 0.0f) {
				if (stillAssoc && stillAssoc->m_BlendDelta >= 0.0f)
					stillAssoc->m_BlendDelta = -4.0f;
				if (walkbackAssoc && walkbackAssoc->m_BlendDelta >= 0.0f)
					walkbackAssoc->m_BlendDelta = -4.0f;
			} else {
				float maxReverseSpeed = bike->m_pHandlingData->m_Transmission.m_fMaxReverseVelocity;
				if (3.5f * maxReverseSpeed > velocityFwdDotProd && (bike->m_nWheelsOnGround || bike->GetUp().z < -0.5f)) {
					bike->KnockOffRider(WEAPONTYPE_FALL, 2, this, false);
					if (bike->pPassengers[0])
						bike->KnockOffRider(WEAPONTYPE_FALL, 2, bike->pPassengers[0], false);
					return;
				}
				if (bike->m_fGasPedal >= 0.0 || velocityFwdDotProd <= maxReverseSpeed * 1.5) {
					if (IsPlayer() && velocityFwdDotProd < maxReverseSpeed * 1.5)
						targetUDLean = -1.0f;

					if (stillAssoc && stillAssoc->m_BlendDelta >= 0.0f)
						stillAssoc->m_BlendDelta = -4.0f;

					if (walkbackAssoc && walkbackAssoc->m_BlendDelta >= 0.0f) {
						walkbackAssoc->m_BlendDelta = -4.0f;
					}
				} else if (!walkbackAssoc || walkbackAssoc->m_BlendAmount < 1.0f && walkbackAssoc->m_BlendDelta <= 0.0f) {
					walkbackAssoc = CAnimManager::BlendAnimation(GetClump(), bike->m_bikeAnimType, ANIM_ID_BIKE_PUSHES, 4.0f);
				}
			}
		}
		if (stillAssoc)
			blendDelta -= Min(1.0f, CTimer::GetTimeStepNonClipped() * 0.02f * stillAssoc->m_BlendDelta + stillAssoc->m_BlendAmount);

		if (drivebyAssoc)
			blendDelta -= Min(blendDelta, CTimer::GetTimeStepNonClipped() * 0.02f * drivebyAssoc->m_BlendDelta + drivebyAssoc->m_BlendAmount);

		if (walkbackAssoc)
			blendDelta -= Min(blendDelta, CTimer::GetTimeStepNonClipped() * 0.02f * walkbackAssoc->m_BlendDelta + walkbackAssoc->m_BlendAmount);

		float targetLRLean, timeBlend, neededAngForWheelie, stoppieAng;

		// Smooth the lean amount
		if (targetUDLean == -1.0f) {
			targetLRLean = 0.0f;
			timeBlend = Pow(0.86f, CTimer::GetTimeStep());
		} else {
			targetLRLean = Clamp(bike->m_fLeanLRAngle / bike->m_pBikeHandlingData->m_fFullAnimLean, -1.0f, 1.0f);
			timeBlend = Pow(0.86f, CTimer::GetTimeStep());
		}

		bike->m_fPedLeanAmountLR = bike->m_fPedLeanAmountLR * timeBlend + (1.0 - timeBlend) * targetLRLean;

		if (!IsPlayer()) {
			targetUDLean = 0.0f;

		} else if (targetUDLean > -1.0f) {
			targetUDLean = bike->m_fLeanInput;
			bike->bWheelieCam = false;
			neededAngForWheelie = 1.0f;
			if (bike->m_aWheelTimer[0] != 0.0f || bike->m_aWheelTimer[1] != 0.0f || bike->GetForward().z <= 0.0f ||
				(0.0f == bike->m_aWheelTimer[2] && 0.0f == bike->m_aWheelTimer[3])) {

				if (0.0f == bike->m_aWheelTimer[2] && 0.0f == bike->m_aWheelTimer[3] &&
					(bike->GetForward().z < 0.0f && (bike->m_aWheelTimer[0] != 0.0f || bike->m_aWheelTimer[1] != 0.0f))) {

					stoppieAng = bike->m_pBikeHandlingData->m_fStoppieAng;
					if (stoppieAng - bike->GetForward().z > 0.6f * stoppieAng)
						bike->bWheelieCam = true;
				}
			} else {
				float wheelieAng = bike->m_pBikeHandlingData->m_fWheelieAng;
				neededAngForWheelie = wheelieAng - bike->GetForward().z;
				if (neededAngForWheelie < wheelieAng / 2.f)
					bike->bWheelieCam = true;
			}
			if (neededAngForWheelie >= 0.15f) {
				if (bike->m_fBrakePedal <= 0.5f || velocityFwdDotProd <= 0.01f) {
					if (bike->m_fGasPedal > 0.5f && targetUDLean <= 0.0f && 0.3f * bike->m_pHandlingData->m_Transmission.m_fMaxCruiseVelocity > velocityFwdDotProd) {
						targetUDLean = Min(0.1f, targetUDLean);
					}
				} else {
					targetUDLean = Max(0.1f, targetUDLean);
				}
			} else {
				targetUDLean = Max(0.25f, targetUDLean);
			}
			float targetLRLeanABS = Abs(targetLRLean);
			if (targetLRLeanABS > 0.3f) {
				// Yes, UD
				targetUDLean *= Max(0.0f, 1.0f - (targetLRLeanABS - 0.3f) * 50.f / 13.f);
			}
		}
		if (IsPlayer()) {
			float timeBlend = Pow(0.89f, CTimer::GetTimeStep());
			bike->m_fPedLeanAmountUD = (timeBlend * bike->m_fPedLeanAmountUD) + ((1.0f - timeBlend) * targetUDLean);
		} else {
			bike->m_fPedLeanAmountUD = 0.0f;
		}

		float fwdBackLeanAmount, leftRightLeanAmount;
		if (Abs(bike->m_fPedLeanAmountLR) <= 0.56f && IsPlayer()) {

			if (Abs(bike->m_fPedLeanAmountUD) <= 0.56f) {
				CVector2D smoothedLean(bike->m_fPedLeanAmountLR, bike->m_fPedLeanAmountUD);
				float smoothLeanMag = smoothedLean.Magnitude();
				if (smoothLeanMag <= 0.01f) {
					fwdBackLeanAmount = Abs(smoothedLean.y);
					leftRightLeanAmount = Abs(smoothedLean.x);
				} else {
					fwdBackLeanAmount = Abs(smoothedLean.y / smoothLeanMag);
					leftRightLeanAmount = Abs(smoothedLean.x / smoothLeanMag);
				}
			} else {
				fwdBackLeanAmount = 1.0f;
				leftRightLeanAmount = 0.0f;
			}
		} else {
			fwdBackLeanAmount = 0.0f;
			leftRightLeanAmount = 1.0f;
		}
		float fwdBackBlend = fwdBackLeanAmount * blendDelta;
		float leftRightBlend = leftRightLeanAmount * blendDelta;
		if (IsPlayer()) {
			if (!fwdAssoc)
				fwdAssoc = CAnimManager::AddAnimation(GetClump(), bike->m_bikeAnimType, ANIM_ID_BIKE_FWD);
			if (!backAssoc)
				backAssoc = CAnimManager::AddAnimation(GetClump(), bike->m_bikeAnimType, ANIM_ID_BIKE_BACK);

			// SA bike banks may not provide both VC lean animations.
			if (!fwdAssoc || !backAssoc) {
				static bool warnedMissingBikePitch = false;
				if (!warnedMissingBikePitch) {
					REVC_LOG_WARNING("Bike pitch animation missing (model=%d, group=%d); skipping pitch",
						bike->GetModelIndex(), bike->m_bikeAnimType);
					warnedMissingBikePitch = true;
				}
			} else if (bike->m_fPedLeanAmountUD < 0.0f) {
				backAssoc->m_BlendAmount = fwdBackBlend;
				backAssoc->SetCurrentTime(-(bike->m_fPedLeanAmountUD * backAssoc->m_BlendHier->m_fTotalTime));
				backAssoc->m_Flags &= ~ANIMATION_IS_PLAYING;
				fwdAssoc->m_BlendAmount = 0.0f;
			} else {
				fwdAssoc->m_BlendAmount = fwdBackBlend;
				fwdAssoc->SetCurrentTime(bike->m_fPedLeanAmountUD* fwdAssoc->m_BlendHier->m_fTotalTime);
				fwdAssoc->m_Flags &= ~ANIMATION_IS_PLAYING;
				backAssoc->m_BlendAmount = 0.0f;
			}
		}
		if (!leftAssoc)
			leftAssoc = CAnimManager::AddAnimation(GetClump(), bike->m_bikeAnimType, ANIM_ID_BIKE_LEFT);
		if (!rightAssoc)
			rightAssoc = CAnimManager::AddAnimation(GetClump(), bike->m_bikeAnimType, ANIM_ID_BIKE_RIGHT);

		if (!leftAssoc || !rightAssoc) {
			static bool warnedMissingBikeRoll = false;
			if (!warnedMissingBikeRoll) {
				REVC_LOG_WARNING("Bike roll animation missing (model=%d, group=%d); skipping roll",
					bike->GetModelIndex(), bike->m_bikeAnimType);
				warnedMissingBikeRoll = true;
			}
		} else if (bike->m_fPedLeanAmountLR < 0.0f) {
			leftAssoc->m_BlendAmount = leftRightBlend;
			leftAssoc->SetCurrentTime(-(bike->m_fPedLeanAmountLR * leftAssoc->m_BlendHier->m_fTotalTime));
			leftAssoc->m_Flags &= ~ANIMATION_IS_PLAYING;
			rightAssoc->m_BlendAmount = 0.0f;
		} else {
			rightAssoc->m_BlendAmount = leftRightBlend;
			rightAssoc->SetCurrentTime(bike->m_fPedLeanAmountLR* rightAssoc->m_BlendHier->m_fTotalTime);
			rightAssoc->m_Flags &= ~ANIMATION_IS_PLAYING;
			leftAssoc->m_BlendAmount = 0.0f;
		}
		if (velocityFwdDotProd > 0.3f) {
			RwV3d Xaxis = { 1.0f, 0.0f, 0.0f };
			RwV3d Yaxis = { 0.0f, 1.0f, 0.0f };
			// The head bone is missing on a skeleton that does not carry it.
			RtQuat *head = FrameQuat(PED_NODE_HEAD);
			if (head) {
				RtQuatRotate(head, &Xaxis, CGeneral::GetRandomNumberInRange(-6.0f * velocityFwdDotProd, 6.0f * velocityFwdDotProd), rwCOMBINEPOSTCONCAT);
				RtQuatRotate(head, &Yaxis, CGeneral::GetRandomNumberInRange(-6.0f * velocityFwdDotProd, 6.0f * velocityFwdDotProd), rwCOMBINEPOSTCONCAT);
			}
			bDontAcceptIKLookAts = true;
		}
		return;
	}

	if (!IsPlayer())
		return;

	float steerAngle = m_pMyVehicle->m_fSteerAngle;
	CAnimBlendAssociation* lDriveAssoc;
	CAnimBlendAssociation* rDriveAssoc;
	CAnimBlendAssociation* lbAssoc;
	CAnimBlendAssociation* sitAssoc;
	if (m_pMyVehicle->IsBoat() && !(m_pMyVehicle->m_pHandlingData->m_nModelFlags & VEHICLE_HANDLING_MODEL_SIT_IN_BOAT)) {
		sitAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_DRIVE_BOAT);

		if (!sitAssoc || sitAssoc->m_BlendAmount < 1.0f) {
			return;
		}

		lDriveAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_DRIVE_BOAT_L);
		rDriveAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_DRIVE_BOAT_R);
		lbAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_DRIVE_BOAT_BACK);
	} else if (m_pMyVehicle->bLowVehicle) {
		sitAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_CAR_LSIT);

		if (!sitAssoc || sitAssoc->m_BlendAmount < 1.0f) {
			return;
		}

		lDriveAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_DRIVE_LO_L);
		lbAssoc = nil;
		rDriveAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_DRIVE_LO_R);
	} else {
		sitAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_CAR_SIT);

		if (!sitAssoc || sitAssoc->m_BlendAmount < 1.0f) {
			return;
		}

		lDriveAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_DRIVE_L);
		rDriveAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_DRIVE_R);
		lbAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_CAR_LB);
	}

	if (lbAssoc &&
		TheCamera.Cams[TheCamera.ActiveCam].Mode == CCam::MODE_1STPERSON
		&& TheCamera.Cams[TheCamera.ActiveCam].DirectionWasLooking == LOOKING_LEFT) {
		lbAssoc->m_BlendDelta = -1000.0f;
	}

	CAnimBlendAssociation* driveByAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_DRIVEBY_L);
	if (!driveByAssoc)
		driveByAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_DRIVEBY_R);
	if (!driveByAssoc)
		driveByAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_DRIVEBYL_L);
	if (!driveByAssoc)
		driveByAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_DRIVEBYL_R);

	if (m_pMyVehicle->bLowVehicle || m_pMyVehicle->m_fGasPedal >= 0.0f || driveByAssoc ||
		m_pMyVehicle->GetVehicleAppearance() == VEHICLE_APPEARANCE_HELI || m_pMyVehicle->GetVehicleAppearance() == VEHICLE_APPEARANCE_PLANE) {
		if (steerAngle == 0.0f || driveByAssoc) {
			if (lDriveAssoc)
				lDriveAssoc->m_BlendAmount = 0.0f;
			if (rDriveAssoc)
				rDriveAssoc->m_BlendAmount = 0.0f;

		} else if (steerAngle <= 0.0f) {
			if (lDriveAssoc)
				lDriveAssoc->m_BlendAmount = 0.0f;

			if (rDriveAssoc)
				rDriveAssoc->m_BlendAmount = Clamp(steerAngle * -100.0f / 61.0f, 0.0f, 1.0f);
			else if (m_pMyVehicle->IsBoat() && !(m_pMyVehicle->m_pHandlingData->m_nModelFlags & VEHICLE_HANDLING_MODEL_SIT_IN_BOAT))
				CAnimManager::AddAnimation(GetClump(), ANIM_GROUP_DEFAULT, ANIM_ID_DRIVE_BOAT_R);
			else if (m_pMyVehicle->bLowVehicle)
				CAnimManager::AddAnimation(GetClump(), ANIM_GROUP_DEFAULT, ANIM_ID_DRIVE_LO_R);
			else
				CAnimManager::AddAnimation(GetClump(), ANIM_GROUP_DEFAULT, ANIM_ID_DRIVE_R);

		} else {
			if (rDriveAssoc)
				rDriveAssoc->m_BlendAmount = 0.0f;

			if (lDriveAssoc)
				lDriveAssoc->m_BlendAmount = Clamp(steerAngle * 100.0f / 61.0f, 0.0f, 1.0f);
			else if (m_pMyVehicle->IsBoat() && !(m_pMyVehicle->m_pHandlingData->m_nModelFlags & VEHICLE_HANDLING_MODEL_SIT_IN_BOAT))
				CAnimManager::AddAnimation(GetClump(), ANIM_GROUP_DEFAULT, ANIM_ID_DRIVE_BOAT_L);
			else if (m_pMyVehicle->bLowVehicle)
				CAnimManager::AddAnimation(GetClump(), ANIM_GROUP_DEFAULT, ANIM_ID_DRIVE_LO_L);
			else
				CAnimManager::AddAnimation(GetClump(), ANIM_GROUP_DEFAULT, ANIM_ID_DRIVE_L);
		}

		if (lbAssoc)
			lbAssoc->m_BlendDelta = -4.0f;
	} else {

		if ((TheCamera.Cams[TheCamera.ActiveCam].Mode != CCam::MODE_1STPERSON
			|| TheCamera.Cams[TheCamera.ActiveCam].DirectionWasLooking != LOOKING_LEFT)
			&& (!lbAssoc || lbAssoc->m_BlendAmount < 1.0f && lbAssoc->m_BlendDelta <= 0.0f)) {

			if (m_pMyVehicle->IsBoat() && !(m_pMyVehicle->m_pHandlingData->m_nModelFlags & VEHICLE_HANDLING_MODEL_SIT_IN_BOAT))
				CAnimManager::BlendAnimation(GetClump(), ANIM_GROUP_DEFAULT, ANIM_ID_DRIVE_BOAT_BACK, 4.0f);
			else
			{
				// Lean-out is part of the vehicle's own driving set, keyed by
				// the handling line's animation group.
				CAnimManager::BlendAnimation(GetClump(), (AssocGroupId)m_pMyVehicle->GetAnimGroupId(), ANIM_ID_CAR_LB, 4.0f);
			}
		}
	}
}

void
CPed::RemoveWeaponAnims(int unused, float animDelta)
{	
	CAnimBlendAssociation *weaponAssoc;
	//CWeaponInfo::GetWeaponInfo(unused);

	weaponAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_FIRE);
	if (weaponAssoc) {
		weaponAssoc->m_BlendDelta = animDelta;
		weaponAssoc->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE;
	}
	weaponAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_FIRE);
	if (weaponAssoc) {
		weaponAssoc->m_BlendDelta = animDelta;
		weaponAssoc->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE;
	}
	weaponAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_FIRE);
	if (weaponAssoc) {
		weaponAssoc->m_BlendDelta = animDelta;
		weaponAssoc->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE;
	}
	weaponAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_RELOAD);
	if (weaponAssoc) {
		weaponAssoc->m_BlendDelta = animDelta;
		weaponAssoc->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE;
	}
	weaponAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_FIGHT_IDLE);
	if (weaponAssoc) {
		weaponAssoc->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE;
		if (weaponAssoc->m_Flags & ANIMATION_IS_PARTIAL)
			weaponAssoc->m_BlendDelta = animDelta;
		else
			CAnimManager::BlendAnimation(GetClump(), m_animGroup, ANIM_ID_IDLE, -animDelta);
	}
}
