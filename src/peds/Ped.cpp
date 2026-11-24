#include "common.h"
#include "World.h"
#include "Camera.h"

#include "main.h"
#include "Pools.h"
#include "Particle.h"
#include "RpAnimBlend.h"
#include "AnimBlendClumpData.h"
#include "Bones.h"
#include "Ped.h"
#include "VehicleAnimGroupData.h"
#include "CarEnterExit.h"
#include "VehicleAnimGroupData.h"
#include "CarEnterExit.h"
#include "TaskSimpleSwim.h"
#include "TaskSimpleClimb.h"
#include "AnimBlendAssociation.h"
#include "Fire.h"
#include "DMAudio.h"
#include "General.h"
#include "Stats.h"
#include "VisibilityPlugins.h"
#include "cHandlingDataMgr.h"
#include "Radar.h"
#include "Shadows.h"
#include "Weather.h"
#include "ZoneCull.h"
#include "Pad.h"
#include "TrafficLights.h"
#include "WaterLevel.h"
#include "Timecycle.h"
#include "ParticleObject.h"
#include "Floater.h"
#include "Range2D.h" 
#include "Streaming.h"
#include "GameLogic.h"
#include "Bike.h"
#include "WindModifiers.h"
#include "CutsceneShadow.h"
#include "Clock.h"
#include "Wanted.h"

bool CPed::bNastyLimbsCheat;

void *CPed::operator new(size_t sz) throw() { return CPools::GetPedPool()->New();  }
void *CPed::operator new(size_t sz, int handle) throw() { return CPools::GetPedPool()->New(handle); }
void CPed::operator delete(void *p, size_t sz) throw() { CPools::GetPedPool()->Delete((CPed*)p); }
void CPed::operator delete(void *p, int handle) throw() { CPools::GetPedPool()->Delete((CPed*)p); }

float gfTommyFatness = 1.0f;

CPed::CPed(uint32 pedType) : m_pedIK(this)
{
#ifdef USE_CUTSCENE_SHADOW_FOR_PED
	m_pRTShadow = nil;
#endif
	// Until the first PreRender poses this ped, bone queries answer from the
	// standard table rather than from an unposed hierarchy.
	bCalledPreRender = false;
	m_vecAnimMoveDelta.x = 0.0f;
	m_vecAnimMoveDelta.y = 0.0f;
	m_fHealth = 100.0f;
	m_fArmour = 0.0f;
	m_nWaterCirclesTime = 0;
	m_nRaindropsTime = 0;
	m_nSwimmingMoveState = PEDMOVE_STILL;
	m_pClimbTask = nil;
	m_pSwimTask = nil;
	m_fSwimStopTime = 0.0f;
	m_nPedType = pedType;
	m_lastSoundStart = 0;
	m_soundStart = 0;
	m_lastQueuedSound = SOUND_NO_SOUND;
	m_queuedSound = SOUND_NO_SOUND;
	m_canTalk = true;

	m_type = ENTITY_TYPE_PED;
	bPedPhysics = true;
	bUseCollisionRecords = true;

	m_objective = OBJECTIVE_NONE;
	m_prevObjective = OBJECTIVE_NONE;
	CharCreatedBy = RANDOM_CHAR;
	m_carInObjective = nil;
	bInVehicle = false;
	m_pMyVehicle = nil;
	m_pVehicleAnim = nil;
	m_vecOffsetSeek.x = 0.0f;
	m_vecOffsetSeek.y = 0.0f;
	m_vecOffsetSeek.z = 0.0f;
	m_nPedStateTimer = 0;
	m_actionX = 0.0f;
	m_actionY = 0.0f;
	m_leaveCarTimer = 0;
	m_getUpTimer = 0;
	m_attackTimer = 0;
	m_lookTimer = 0;
	m_shootTimer = 0;
	m_carJackTimer = 0;
	m_duckAndCoverTimer = 0;
	m_moved = CVector2D(0.0f, 0.0f);
	m_fRotationCur = 0.0f;
	m_headingRate = 15.0f;
	m_fRotationDest = 0.0f;
	m_vehDoor = CAR_DOOR_LF;
	m_walkAroundType = 0;
	m_pCurrentPhysSurface = nil;
	m_vecOffsetFromPhysSurface = CVector(0.0f, 0.0f, 0.0f);
	m_pSeekTarget = nil;
	m_vecSeekPos = CVector(0.0f, 0.0f, 0.0f);
	m_wepSkills = 0;
	m_pCollidingEntity = nil;
	m_nPedState = PED_IDLE;
	m_nLastPedState = PED_NONE;
	m_nMoveState = PEDMOVE_STILL;
#ifdef FIX_BUGS
	m_nPrevMoveState = PEDMOVE_NONE;
#endif
	m_nStoredMoveState = PEDMOVE_NONE;
	m_pFire = nil;
	m_pPointGunAt = nil;
	m_pLookTarget = nil;
	m_fLookDirection = 0.0f;
	m_pCurSurface = nil;

	m_fMass = 70.0f;
	m_fTurnMass = 100.0f;
	m_fAirResistance = 0.4f / m_fMass;
	m_fElasticity = 0.05f;

	m_ceaseAttackTimer = 0;
	m_bodyPartBleeding = -1;

	bIsStanding = false;
	bWasStanding = false;
	bIsAttacking = false;
	bIsPointingGunAt = false;
	bIsLooking = false;
	bKeepTryingToLook = false;
	bIsRestoringLook = false;
	bIsAimingGun = false;

	bIsRestoringGun = false;
	bCanPointGunAtTarget = false;
	bIsTalking = false;
	bIsInTheAir = false;
	bIsLanding = false;
	bIsRunning = false;
	bHitSomethingLastFrame = false;
	bVehEnterDoorIsBlocked = false;

	bCanPedEnterSeekedCar = false;
	bRespondsToThreats = true;
	bRenderPedInCar = true;
	bChangedSeat = false;
	bUpdateAnimHeading = false;
	bBodyPartJustCameOff = false;
	bIsShooting = false;

	bPedIsBleeding = false;
	bStopAndShoot = false;
	bIsPedDieAnimPlaying = false;
	bObjectiveCompleted = false;

	bKindaStayInSamePlace = false;
	bBeingChasedByPolice = false;
	bNotAllowedToDuck = false;
	bCrouchWhenShooting = false;
	bIsDucking = false;
	bGetUpAnimStarted = false;
	bDoBloodyFootprints = false;

	bIsLeader = false;
	bDontDragMeOutCar = false;
	bWillBeQuickJacked = false;
	bCancelEnteringCar = false;
	bObstacleShowedUpDuringKillObjective = false;
	bDuckAndCover = false;

	bStillOnValidPoly = false;
	bAllowMedicsToReviveMe = true;
	bResetWalkAnims = false;
	bOnBoat = false;
	bFadeOut = false;

	bKnockedUpIntoAir = false;
	bHitSteepSlope = false;
	bCullExtraFarAway = false;
	bClearObjective = false;
	bTryingToReachDryLand = false;
	bCollidedWithMyVehicle = false;
	bRichFromMugging = false;
	bChrisCriminal = false;

	bShakeFist = false;
	bNoCriticalHits = false;
	bVehExitWillBeInstant = false;
	bFallenDown = false;
	bDontAcceptIKLookAts = false;
#ifdef KANGAROO_CHEAT
	m_ped_flagI80 = false;
#endif

	bMoneyHasBeenGivenByScript = false;
	bHasBeenPhotographed = false;

	bIsDrowning = false;
	bDrownsInWater = true;
	bWaitForLeaderToComeCloser = false;
	bIsPlayerFriend = true;
	bHeadStuckInCollision = false;
	bDeadPedInFrontOfCar = false;

	m_gangFlags = ~0;
	
	bStayInCarOnJack = false;

	bDontFight = false;
	bDoomAim = true;
	bCanBeShotInVehicle = true;
	bPushedAlongByCar = false;
	bRemoveMeWhenIGotIntoCar = false;
	bIgnoreThreatsBehindObjects = false;

	bNeverEverTargetThisPed = false;
	bCrouchWhenScared = false;
	bKnockedOffBike = false;
	b158_8 = false;
	bCollectBusFare = false;
	bDonePositionOutOfCollision = false;
	bCanAttackPlayerWithCops = false;

	m_audioEntityId = DMAudio.CreateEntity(AUDIOTYPE_PHYSICAL, this);
	DMAudio.SetEntityStatus(m_audioEntityId, TRUE);
	m_maxWeaponTypeAllowed = WEAPONTYPE_UNARMED;
	m_currentWeapon = WEAPONTYPE_UNARMED;
	m_storedWeapon = WEAPONTYPE_UNIDENTIFIED;
	m_delayedWeapon = WEAPONTYPE_UNIDENTIFIED;

	for(int i = 0; i < TOTAL_WEAPON_SLOTS; i++) {
		CWeapon &weapon = GetWeapon(i);
		weapon.m_eWeaponType = WEAPONTYPE_UNARMED;
		weapon.m_eWeaponState = WEAPONSTATE_READY;
		weapon.m_nAmmoInClip = 0;
		weapon.m_nAmmoTotal = 0;
		weapon.m_nTimer = 0;
	}

	m_curFightMove = m_lastFightMove = FIGHTMOVE_IDLE;
	GiveWeapon(WEAPONTYPE_UNARMED, 0, true);
	m_wepAccuracy = 60;
	m_lastWepDam = -1;
	m_lastDamEntity = nil;
	m_attachedTo = nil;
	m_attachWepAmmo = 0;
	m_collPoly.valid = false;
	m_fCollisionSpeed = 0.0f;
	m_wepModelID = -1;
	uint16 random = CGeneral::GetRandomNumber();
	m_nPedMoney = random % 25;
	if (m_nPedMoney == 23)
		m_nPedMoney = 400;
	m_bleedCounter = 0;
	m_pWeaponModel = nil;
	m_delayedSoundID = -1;
	m_delayedSoundTimer = 0;
	m_lastComment = UINT32_MAX;
}

CPed::~CPed(void)
{
#ifdef USE_CUTSCENE_SHADOW_FOR_PED
	if ( m_pRTShadow ) delete m_pRTShadow;
#endif
	// The ped owns both tasks outright, so they have to die with it - a ped
	// deleted mid-stroke would otherwise leave the swim holding a clump that
	// the pool has already handed to the next ped.
	delete m_pSwimTask;
	m_pSwimTask = nil;
	delete m_pClimbTask;
	m_pClimbTask = nil;
	CWorld::Remove(this);
	CRadar::ClearBlipForEntity(BLIP_CHAR, CPools::GetPedPool()->GetIndex(this));
	if (InVehicle()){
		uint8 door_flag = GetCarDoorFlag(m_vehDoor);
		if (m_pMyVehicle->pDriver == this)
			m_pMyVehicle->pDriver = nil;
		else {
			// FIX: Passenger counter now being decreased after removing ourself from vehicle.
			m_pMyVehicle->RemovePassenger(this);
		}
		if (m_nPedState == PED_EXIT_CAR || m_nPedState == PED_DRAGGED_FROM_CAR)
			m_pMyVehicle->m_nGettingOutFlags &= ~door_flag;
		bInVehicle = false;
		m_pMyVehicle = nil;
	} else if (EnteringCar()) {
		QuitEnteringCar();
	}
	if (m_pFire)
		m_pFire->Extinguish();

	ClearWeapons();
	DMAudio.DestroyEntity(m_audioEntityId);
}

void
CPed::Initialise(void)
{
	debug("Initialising CPed...\n");
	LoadFightData();
	SetAnimOffsetForEnterOrExitVehicle();
	debug("CPed ready\n");
}

void
CPed::SetModelIndex(uint32 mi)
{
	REVC_LOG_INFO("[spawn] CPed::SetModelIndex(%u) enter, registered=%d", mi,
		CModelInfo::GetModelInfo((int)mi) != nil);
	CEntity::SetModelIndex(mi);
	REVC_LOG_INFO("[spawn] CPed::SetModelIndex after CEntity, clump=%p", (void*)GetClump());
	if(GetClump() == nil){
		CStreaming::RequestModel(mi, STREAMFLAGS_PRIORITY | STREAMFLAGS_KEEP_IN_MEMORY);
		CStreaming::LoadAllRequestedModels(true);
		if(!CStreaming::HasModelLoaded(mi))
			CStreaming::LoadAllRequestedModels(false);
		CEntity::SetModelIndex(mi);
	}
	CPedModelInfo *modelInfo = (CPedModelInfo *)CModelInfo::GetModelInfo(GetModelIndex());
	// The ped animation set is SA only, so a VC skeleton would never animate.
	// Swap such a ped for an SA model instead of leaving it frozen. A candidate
	// can only be judged once its clump is resident, so each one is loaded
	// before it is accepted.
	int32 modelIndexTried = -1;
	for(int32 attempt = 0; modelInfo && !modelInfo->UsesSAAnimations() && attempt < 4; attempt++){
		int32 saModel = CPedModelInfo::FindSAReplacementModel(GetModelIndex());
		if(saModel < 0 || (uint32)saModel == GetModelIndex() || saModel == modelIndexTried)
			break;
		modelIndexTried = saModel;
		CStreaming::RequestModel(saModel, STREAMFLAGS_PRIORITY | STREAMFLAGS_KEEP_IN_MEMORY);
		CStreaming::LoadAllRequestedModels(true);
		if(!CStreaming::HasModelLoaded(saModel))
			continue;
		CPedModelInfo *saInfo = (CPedModelInfo *)CModelInfo::GetModelInfo(saModel);
		if(saInfo == nil || saInfo->GetRwObject() == nil || !saInfo->UsesSAAnimations())
			continue;
		REVC_LOG_INFO("Ped model %s has a VC skeleton and no VC animation path; using SA model %s instead",
			modelInfo->GetModelName(), saInfo->GetModelName());
		// Remember the choice on the VC slot: the replacement is then fixed for
		// the lifetime of the game instead of being searched again, and every
		// instance of this model keeps one body.
		modelInfo->m_saReplacementModel = saModel;
		CEntity::SetModelIndex(saModel);
		modelInfo = saInfo;
	}
	if(modelInfo){
		m_animGroup = (AssocGroupId) modelInfo->m_animGroup;
	}
	if(GetClump() == nil){
		REVC_LOG_ERROR("CPed::SetModelIndex(%u): clump is nil for model %s",
			mi, modelInfo ? modelInfo->GetModelName() : "unknown");
		return;
	}
	RpAnimBlendClumpInit(GetClump());
	RpAnimBlendClumpFillFrameArray(GetClump(), m_pFrames);
	// Associations are built against a skeleton-specific reference clump. This
	// call also retries groups that had no native/SA reference during startup.
	CAnimManager::CreateAnimAssocGroups(GetModelIndex());
	CAnimManager::AddAnimation(GetClump(), m_animGroup, ANIM_ID_IDLE);

	if (!CanUseTorsoWhenLooking())
		m_pedIK.m_flags |= CPedIK::LOOKAROUND_HEAD_ONLY;

	(*RPANIMBLENDCLUMPDATA(m_rwObject))->velocity2d = &m_vecAnimMoveDelta;

	if(modelInfo->GetHitColModel() == nil)
		modelInfo->CreateHitColModelSkinned(GetClump());

	UpdateRpHAnim();
#ifdef USE_CUTSCENE_SHADOW_FOR_PED
	if (!m_pRTShadow)
	{
		m_pRTShadow = new CCutsceneShadow;
		m_pRTShadow->Create(m_rwObject, 10, 1, 1, 1);
		//m_pRTShadow->Create(m_rwObject, 8, 0, 0, 0);
	}
#endif
}

void
CPed::DeleteRwObject()
{
	CEntity::DeleteRwObject();
}

bool
CPed::OurPedCanSeeThisOne(CEntity *target, bool shootablesDoBlock)
{
	CColPoint colpoint;
	CEntity *ent;

	CVector2D dist = CVector2D(target->GetPosition()) - CVector2D(GetPosition());

	// Check if target is behind ped
	if (DotProduct2D(dist, CVector2D(GetForward())) < 0.0f)
		return false;

	// Check if target is too far away
	if (dist.Magnitude() >= 40.0f)
		return false;

	// Check line of sight from head
	return !CWorld::ProcessLineOfSight(GetPosition() + CVector(0.f, 0.f, 1.f), target->GetPosition() + CVector(0.f, 0.f, 1.f),
		colpoint, ent, true, false, false, shootablesDoBlock, false, false, false, shootablesDoBlock);
}

// Quick sort by distance to centre
static void
SortNearPeds(CPed **list, const CVector &centre, int min, int max)
{
	if (min >= max)
		return;

	CVector leftDiff, rightDiff;
	CVector middleDiff = centre - list[(max + min) / 2]->GetPosition();
	float middleDist = middleDiff.Magnitude();

	int left = max;
	int right = min;
	while(right <= left){
		float rightDist, leftDist;
		do {
			rightDiff = centre - list[right]->GetPosition();
			rightDist = rightDiff.Magnitude();
		} while (middleDist > rightDist && ++right);

		do {
			leftDiff = centre - list[left]->GetPosition();
			leftDist = leftDiff.Magnitude();
		} while (middleDist < leftDist && left--);

		if (right <= left) {
			CPed *ped = list[right];
			list[right] = list[left];
			list[left] = ped;
			right++;
			left--;
		}
	}
	SortNearPeds(list, centre, min, left);
	SortNearPeds(list, centre, right, max);
}

// Peds within radius (2D) of centre, nearest first. This ped is skipped, and so are
// peds inside vehicles (bike riders excepted); at most three dead peds are included.
// Returns the number of peds written to out, the remaining entries are set to nil.
int16
CPed::FindNearPeds(const CVector &centre, float radius, CPed **out, int16 maxPeds)
{
	CPed *list[50];
	int16 numFound = 0;
	int deadsRegistered = 0;
	CRect rect(centre.x - radius,
		centre.y - radius,
		centre.x + radius,
		centre.y + radius);
	int xstart = CWorld::GetSectorIndexX(rect.left);
	int ystart = CWorld::GetSectorIndexY(rect.top);
	int xend = CWorld::GetSectorIndexX(rect.right);
	int yend = CWorld::GetSectorIndexY(rect.bottom);

	assert(maxPeds <= int16(ARRAY_SIZE(list)));

	for(int y = ystart; y <= yend; y++) {
		for(int x = xstart; x <= xend; x++) {
			for (CPtrNode *pedPtrNode = CWorld::GetSector(x,y)->m_lists[ENTITYLIST_PEDS].first; pedPtrNode; pedPtrNode = pedPtrNode->next) {
				CPed *ped = (CPed*)pedPtrNode->item;
				if (ped == this || ped->bInVehicle && !(ped->m_pMyVehicle && ped->m_pMyVehicle->IsBike()))
					continue;
				if (radius <= (ped->GetPosition() - centre).Magnitude2D())
					continue;
				if (ped->m_nPedState == PED_DEAD && ++deadsRegistered > 3)
					continue;

				// List is full: keep the nearest maxPeds so far, then carry on
				if (numFound == ARRAY_SIZE(list)) {
					SortNearPeds(list, centre, 0, numFound - 1);
					numFound = maxPeds;
				}
				list[numFound++] = ped;
			}
		}
	}

	SortNearPeds(list, centre, 0, numFound - 1);
	numFound = Min(numFound, maxPeds);
	for (int16 i = 0; i < maxPeds; i++)
		out[i] = i < numFound ? list[i] : nil;
	return numFound;
}

void
CPed::SetMoveState(eMoveState state)
{
	m_nMoveState = state;
}

// SA CPed::SetMoveAnimSpeed (0x5DEC10). Walk, jog, run and sprint are all
// authored at one stride rate, so SA scales each ped's playback speed by a
// per-ped random factor (mission peds always 1.0) to break the lock-step, and
// by the slope pitch so a ped climbing a hill keeps the same cadence. reVC had
// this expression inlined twice inside SetMoveAnim and dropped the slope term,
// because there was no CPedIK slope state to read.
void
CPed::SetMoveAnimSpeed(CAnimBlendAssociation *assoc)
{
	float pitchFactor = Clamp(m_pedIK.m_fSlopePitch, -0.3f, 0.3f);
	if (CharCreatedBy == MISSION_CHAR)
		assoc->speed = pitchFactor + 1.0f;
	else
		assoc->speed = pitchFactor + 1.2f - m_randomSeed * (1.0f / MYRAND_MAX) * 0.4f;
}

void
CPed::SetMoveAnim(void)
{
	if (m_nStoredMoveState == m_nMoveState || !IsPedInControl() || m_attachedTo)
		return;

	// A swimmer's legs belong to the stroke and a climber's to the climb. The
	// ground blend would override both: the walk cycle does not exist in
	// ASSOCGRP_SWIM, so asking for it mid-stroke replaced the stroke with a
	// standing ped's idle and the swimmer appeared to walk on the water.
	if (m_pSwimTask || m_pClimbTask || (bIsDucking && bCrouchWhenShooting))
		return;

	if (m_nMoveState == PEDMOVE_NONE) {
		m_nStoredMoveState = PEDMOVE_NONE;
		return;
	}

	AssocGroupId animGroupToUse = m_animGroup;

	CAnimBlendAssociation *animAssoc = RpAnimBlendClumpGetFirstAssociation(GetClump(), ASSOC_BLOCK);
	if (!animAssoc) {
		animAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_FIGHT_IDLE);
		if (!animAssoc)
			animAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_FIGHT_IDLE);

		if (animAssoc && m_nPedState == PED_FIGHT)
			return;

		if (animAssoc) {
			CAnimBlendAssociation *idleAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_IDLE);
			if (!idleAssoc || idleAssoc->blendDelta <= 0.0f) {
				animAssoc->flags |= ASSOC_DELETEFADEDOUT;
				animAssoc = CAnimManager::BlendAnimation(GetClump(), animGroupToUse, ANIM_ID_IDLE, 8.0f);
			}
		}
	}
	if (!animAssoc) {
		animAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_IDLE_TIRED);

		if (animAssoc) {
			CAnimBlendAssociation *idleAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_IDLE);
			if (!idleAssoc || idleAssoc->blendDelta <= 0.0f) {
				animAssoc->flags |= ASSOC_DELETEFADEDOUT;
				animAssoc = CAnimManager::BlendAnimation(GetClump(), animGroupToUse, ANIM_ID_IDLE, 4.0f);
			}
		}
	}
	if (!animAssoc) {
		m_nStoredMoveState = m_nMoveState;
		if (m_nMoveState == PEDMOVE_WALK || m_nMoveState == PEDMOVE_RUN || m_nMoveState == PEDMOVE_SPRINT) {
			for (CAnimBlendAssociation *assoc = RpAnimBlendClumpGetFirstAssociation(GetClump(), ASSOC_PARTIAL);
				assoc; assoc = RpAnimBlendGetNextAssociation(assoc, ASSOC_PARTIAL)) {

				if (!(assoc->flags & ASSOC_FADEOUTWHENDONE)) {
					assoc->blendDelta = -2.0f;
					assoc->flags |= ASSOC_DELETEFADEDOUT;
				}
			}

			ClearAimFlag();
			ClearLookFlag();
		}

		switch (m_nMoveState) {
			case PEDMOVE_STILL:
				animAssoc = CAnimManager::BlendAnimation(GetClump(), animGroupToUse, ANIM_ID_IDLE, 4.0f);
				break;
			case PEDMOVE_WALK:
				animAssoc = CAnimManager::BlendAnimation(GetClump(), animGroupToUse, ANIM_ID_WALK, 1.0f);
				break;
			case PEDMOVE_RUN:
				animAssoc = CAnimManager::BlendAnimation(GetClump(), animGroupToUse, ANIM_ID_RUN, 1.0f);
				break;
			case PEDMOVE_SPRINT:
				animAssoc = CAnimManager::BlendAnimation(GetClump(), animGroupToUse, ANIM_ID_SPRINT, 1.0f);
				break;
			default:
				break;
		}

		if (animAssoc)
			SetMoveAnimSpeed(animAssoc);
	}
}

void
CPed::StopNonPartialAnims(void)
{
	CAnimBlendAssociation *assoc;

	for (assoc = RpAnimBlendClumpGetFirstAssociation(GetClump()); assoc; assoc = RpAnimBlendGetNextAssociation(assoc)) {
		if (!assoc->IsPartial())
			assoc->flags &= ~ASSOC_RUNNING;
	}
}

void
CPed::RestartNonPartialAnims(void)
{
	CAnimBlendAssociation *assoc;

	for (assoc = RpAnimBlendClumpGetFirstAssociation(GetClump()); assoc; assoc = RpAnimBlendGetNextAssociation(assoc)) {
		if (!assoc->IsPartial())
			assoc->SetRun();
	}
}

void
CPed::SetStoredState(void)
{
	if (m_nLastPedState != PED_NONE || !CanPedReturnToState())
		return;

	if (m_nPedState != PED_IDLE) {
		m_nLastPedState = m_nPedState;
		if (m_nMoveState >= m_nPrevMoveState)
			m_nPrevMoveState = m_nMoveState;
	} else if (m_nMoveState == PEDMOVE_WALK || m_nMoveState == PEDMOVE_RUN ||
	          m_nMoveState == PEDMOVE_SPRINT) {
		// A ped that runs or sprints is in PED_IDLE: the movement lives in
		// m_nMoveState, and the state machine only says "on foot, standing".
		// Skipping the save for PED_IDLE dropped the run entirely, so landing
		// from a jump went through RestorePreviousState -> SetIdle ->
		// SetMoveState(PEDMOVE_STILL), which is why a run turned into an
		// instant stop. Keep the move state even when the ped state is idle.
		if (m_nMoveState >= m_nPrevMoveState)
			m_nPrevMoveState = m_nMoveState;
	}
}

void
CPed::RestorePreviousState(void)
{
	if (!CanSetPedState() || m_nPedState == PED_FALL)
		return;

	if (m_nPedState == PED_GETUP && !bGetUpAnimStarted)
		return;

	if (InVehicle()) {
		SetPedState(PED_DRIVING);
		m_nLastPedState = PED_NONE;
	} else {
		if (m_nLastPedState == PED_NONE) {
			// SetIdle settles the ped into PEDMOVE_STILL. A player who jumped
			// while running was running under PED_IDLE, so its move state was
			// saved by SetStoredState but its ped state was not; putting the
			// move state back is what keeps the run after the landing rather
			// than snapping to a standstill.
			const int32 savedMove = m_nPrevMoveState;
			SetIdle();
			if (savedMove == PEDMOVE_WALK || savedMove == PEDMOVE_RUN || savedMove == PEDMOVE_SPRINT) {
				SetMoveState((eMoveState)savedMove);
				m_nPrevMoveState = (eMoveState)savedMove;
				// SetMoveAnim refuses to do anything while the stored state
				// already matches the wanted one, so clear it and let the run
				// clip be picked up again on the next frame.
				m_nStoredMoveState = PEDMOVE_NONE;
			}
			return;
		}

		switch (m_nLastPedState) {
			case PED_IDLE:
				SetIdle();
				break;
			default:
				SetPedState(m_nLastPedState);
				SetMoveState((eMoveState) m_nPrevMoveState);
				// Same reason: the stored state still names the clip that was
				// playing before the jump, so SetMoveAnim would skip the rebuild.
				m_nStoredMoveState = PEDMOVE_NONE;
				break;
		}
		m_nLastPedState = PED_NONE;
	}
}

bool
CPed::CanUseTorsoWhenLooking(void)
{
	// gta-reversed CPed::CanUseTorsoWhenLooking vetoes the torso turn only while
	// driving or being dragged out of a car, and while ducking. reVC also
	// vetoed it for every female animation group, which is why women never
	// turned their torso to look at something - the head swung on its own and
	// the body stayed square to the direction of travel.
	if(m_nPedState == PED_DRIVING || m_nPedState == PED_DRAGGED_FROM_CAR)
		return false;
	if(bIsDucking)
		return false;
	return true;
}

void
CPed::SetLookFlag(float direction, bool keepTryingToLook, bool cancelPrevious)
{
	if (m_lookTimer < CTimer::GetTimeInMilliseconds() || cancelPrevious) {
		bIsLooking = true;
		bIsRestoringLook = false;
		m_fLookDirection = direction;
		m_pLookTarget = nil;
		m_lookTimer = 0;
		bKeepTryingToLook = keepTryingToLook;
		if (CanUseTorsoWhenLooking()) {
			m_pedIK.m_flags &= ~CPedIK::LOOKAROUND_HEAD_ONLY;
		}
	}
}

void
CPed::SetLookFlag(CEntity *target, bool keepTryingToLook, bool cancelPrevious)
{
	if (m_lookTimer < CTimer::GetTimeInMilliseconds() || cancelPrevious) {
		bIsLooking = true;
		bIsRestoringLook = false;
		m_pLookTarget = target;
		m_pLookTarget->RegisterReference((CEntity**)&m_pLookTarget);
		m_fLookDirection = 999999.0f;
		m_lookTimer = 0;
		bKeepTryingToLook = keepTryingToLook;
		if (CanUseTorsoWhenLooking()) {
			m_pedIK.m_flags &= ~CPedIK::LOOKAROUND_HEAD_ONLY;
		}
	}
}

void
CPed::ClearLookFlag(void) {
	if (bIsLooking) {
		bIsLooking = false;
		bIsRestoringLook = true;
		bShakeFist = false;

		if (CanUseTorsoWhenLooking())
			m_pedIK.m_flags &= ~CPedIK::LOOKAROUND_HEAD_ONLY;

		if (IsPlayer())
			m_lookTimer = CTimer::GetTimeInMilliseconds() + 2000;
		else
			m_lookTimer = CTimer::GetTimeInMilliseconds() + 4000;

		if (m_nPedState == PED_LOOK_HEADING || m_nPedState == PED_LOOK_ENTITY) {
			ClearLook();
		}
	}
}

void
CPed::MoveHeadToLook(void)
{
	CVector lookPos;

	if (m_lookTimer && CTimer::GetTimeInMilliseconds() > m_lookTimer) {
		ClearLookFlag();
	} 

	if (bIsLooking || bIsRestoringLook)
		if (!CanUseTorsoWhenLooking())
			m_pedIK.m_flags |= CPedIK::LOOKAROUND_HEAD_ONLY;

	if (m_pLookTarget) {
		if (m_pLookTarget->IsPed()) {
			((CPed*)m_pLookTarget)->m_pedIK.GetComponentPosition(lookPos, PED_NODE_UPPER_TORSO);
		} else {
			lookPos = m_pLookTarget->GetPosition();
		}

		if (!m_pedIK.LookAtPosition(lookPos)) {
			if (!bKeepTryingToLook) {
				ClearLookFlag();
			}
			return;
		}

		if (!bShakeFist || bIsAimingGun || bIsRestoringGun)
			return;

		if (m_nPedState == PED_ANSWER_MOBILE)
			return;

		if (m_lookTimer - CTimer::GetTimeInMilliseconds() >= 1000)
			return;

		bool handFreeToMove = false;
		AnimationId animToPlay = ANIM_ID_UNDEFINED;

		if (!GetWeapon()->IsType2Handed() && GetWeapon()->m_eWeaponType != WEAPONTYPE_ROCKETLAUNCHER)
			handFreeToMove = true;

		if (IsPlayer() && handFreeToMove) {

			if (m_pLookTarget->IsPed()) {
				animToPlay = ANIM_ID_FUCKU;
			} else {
				if (m_pLookTarget->GetModelIndex() == MI_POLICE)
					animToPlay = ANIM_ID_FUCKU;
			}
		} else if (handFreeToMove && (CGeneral::GetRandomNumber() & 1)) {
			animToPlay = ANIM_ID_FUCKU;
		}

		if (animToPlay != ANIM_ID_UNDEFINED) {
			CAnimBlendAssociation *newAssoc = CAnimManager::BlendAnimation(GetClump(), ASSOCGRP_DEFAULT, animToPlay, 4.0f);

			if (newAssoc) {
				newAssoc->flags |= ASSOC_FADEOUTWHENDONE;
				newAssoc->flags |= ASSOC_DELETEFADEDOUT;
			}
		}
		bShakeFist = false;
	} else if (m_fLookDirection == 999999.0f) {
		ClearLookFlag();
	} else if (!m_pedIK.LookInDirection(m_fLookDirection, 0.0f)) {
		if (!bKeepTryingToLook)
			ClearLookFlag();
	}
}

void
CPed::RestoreHeadPosition(void)
{
	if(!CanUseTorsoWhenLooking())
		m_pedIK.m_flags |= CPedIK::LOOKAROUND_HEAD_ONLY;

	if (m_pedIK.RestoreLookAt()) {
		bIsRestoringLook = false;
		if(CanUseTorsoWhenLooking())
			m_pedIK.m_flags &= ~CPedIK::LOOKAROUND_HEAD_ONLY;
	}
}

void
CPed::SetAimFlag(float angle)
{
	bIsAimingGun = true;
	bIsRestoringGun = false;
	m_fLookDirection = angle;
	m_lookTimer = 0;
	m_pLookTarget = nil;
	m_pSeekTarget = nil;

	if (bIsDucking)
		m_pedIK.m_flags &= ~CPedIK::AIMS_WITH_ARM;

	if (CWeaponInfo::GetWeaponInfo(GetWeapon()->m_eWeaponType)->IsFlagSet(WEAPONFLAG_CANAIM_WITHARM))
		m_pedIK.m_flags |= CPedIK::AIMS_WITH_ARM;
	else
		m_pedIK.m_flags &= ~CPedIK::AIMS_WITH_ARM;
}

void
CPed::SetAimFlag(CEntity *to)
{
	bIsAimingGun = true;
	bIsRestoringGun = false;
	if (m_pLookTarget)
		m_pLookTarget->CleanUpOldReference(&m_pLookTarget);
	m_pLookTarget = to;
	m_pLookTarget->RegisterReference((CEntity **) &m_pLookTarget);
	if (m_pSeekTarget)
		m_pSeekTarget->CleanUpOldReference(&m_pSeekTarget);
	m_pSeekTarget = to;
	m_pSeekTarget->RegisterReference((CEntity **) &m_pSeekTarget);
	m_lookTimer = 0;
}

void
CPed::ClearAimFlag(void)
{
	if (bIsAimingGun) {
		bIsAimingGun = false;
		bIsRestoringGun = true;
		m_pedIK.m_flags &= ~CPedIK::AIMS_WITH_ARM;
		m_lookTimer = 0;
	}

	if (IsPlayer()) {
		((CPlayerPed*)this)->FPSMoveHeading() = 0.0f;
#ifdef FREE_CAM
		((CPlayerPed*)this)->m_bFreeAimActive = false;
#endif
	}
}

// SA passes the blend amount of the weapon's fire animation as `normalize` to
// CPedIK::PointGunInDirection / PointGunAtPosition. VC had no such argument, so
// reVC has to look the association up the same way SetPointGunAt picks it.
float
CPed::GetAimAnimBlendAmount(void)
{
	CWeaponInfo *curWeapon = CWeaponInfo::GetWeaponInfo(GetWeapon()->m_eWeaponType);
	CAnimBlendAssociation *aimAssoc = nil;
	if (bCrouchWhenShooting && bIsDucking && GetCrouchFireAnim(curWeapon))
		aimAssoc = RpAnimBlendClumpGetAssociation(GetClump(), GetCrouchFireAnim(curWeapon));
	else
		aimAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_FIRE);

	// -1 means "no blend", which makes MoveLimb step towards the target the way
	// SA does when the fire animation is not playing yet.
	return aimAssoc ? aimAssoc->blendAmount : -1.0f;
}

void
CPed::AimGun(void)
{
	CVector vector;

	if (IsPlayer() && bIsDucking)
		m_pedIK.m_flags &= ~CPedIK::AIMS_WITH_ARM;

	if (m_pSeekTarget) {
		if (m_pSeekTarget->IsPed()) {
			((CPed*)m_pSeekTarget)->m_pedIK.GetComponentPosition(vector, PED_NODE_UPPER_TORSO);
		} else {
			vector = m_pSeekTarget->GetPosition();
		}

		if (!IsPlayer())
			Say(SOUND_PED_ATTACK);

		// SA resolves the aim against the weapon's fire animation blend, so the
		// torso turn fades in with the clip instead of snapping to full on the
		// first frame. Without an association the blend is 0, the plain stepwise
		// MoveLimb path, which is what VC always used.
		// SA's PointGunAtPosition returns void and reports success through
		// CPedIK::bGunReachedTarget, which VC carries as GUN_POINTED_SUCCESSFULLY.
		m_pedIK.PointGunAtPosition(vector, GetAimAnimBlendAmount());
		bCanPointGunAtTarget = (m_pedIK.m_flags & CPedIK::GUN_POINTED_SUCCESSFULLY) != 0;

		// VC aims some weapons with the arm chain rather than the torso. SA moves
		// that into CTaskSimpleUseGun's IK chain manager, which reVC has no
		// equivalent of, so the arm pass stays here - otherwise AIMS_WITH_ARM
		// would be set and never acted on, and the arm would hang at the side
		// while the torso swung round.
		if (m_pedIK.m_flags & CPedIK::AIMS_WITH_ARM) {
			float aimYaw = CGeneral::GetRadianAngleBetweenPoints(vector.x, vector.y, GetPosition().x, GetPosition().y);
			float aimPitch = CGeneral::GetRadianAngleBetweenPoints(
				vector.z, Distance2D(GetPosition(), vector), GetPosition().z, 0.0f);
			if (m_pedIK.PointGunInDirectionUsingArm(aimYaw, aimPitch)) {
				m_pedIK.RotateTorsoForArm(vector);
				bCanPointGunAtTarget = true;
			}
		}
		if (m_pLookTarget != m_pSeekTarget) {
			SetLookFlag(m_pSeekTarget, true, true);
		}

	} else {
		// Same shape as CTaskSimpleUseGun: aim straight ahead, using the player's
		// look pitch as the vertical distance and -1 for "no blend" so MoveLimb
		// takes its stepping path.
		bCanPointGunAtTarget = m_pedIK.PointGunInDirection(
			m_fLookDirection,
			IsPlayer() ? ((CPlayerPed*)this)->FPSMoveHeading() : 0.0f,
			false,
			GetAimAnimBlendAmount());
	}
}

void
CPed::RestoreGunPosition(void)
{
	if (bIsLooking) {
		m_pedIK.m_flags &= ~CPedIK::LOOKAROUND_HEAD_ONLY;
		bIsRestoringGun = false;
	} else if (m_pedIK.RestoreGunPosn()) {
		bIsRestoringGun = false;
	} else {
		if (IsPlayer())
			((CPlayerPed*)this)->FPSMoveHeading() = 0.0f;
	}
}

bool
CPed::CanWeRunAndFireWithWeapon(void)
{
	return CWeaponInfo::GetWeaponInfo(GetWeapon()->m_eWeaponType)->IsFlagSet(WEAPONFLAG_CANAIM_WITHARM);
}

void
CPed::CalculateNewVelocity(void)
{
	if (IsPedInControl()) {
		float headAmount = DEGTORAD(m_headingRate) * CTimer::GetTimeStep();
		m_fRotationCur = CGeneral::LimitRadianAngle(m_fRotationCur);
		float limitedRotDest = CGeneral::LimitRadianAngle(m_fRotationDest);

		if (m_fRotationCur - PI > limitedRotDest) {
				limitedRotDest += 2 * PI;
		} else if(PI + m_fRotationCur < limitedRotDest) {
			limitedRotDest -= 2 * PI;
		}

		float neededTurn = limitedRotDest - m_fRotationCur;
		if (neededTurn <= headAmount) {
			if (neededTurn > (-headAmount))
				m_fRotationCur += neededTurn;
			else
				m_fRotationCur -= headAmount;
		} else {
			m_fRotationCur += headAmount;
		}
	}

	CVector2D forward(Sin(m_fRotationCur), Cos(m_fRotationCur));

	m_moved.x = CrossProduct2D(m_vecAnimMoveDelta, forward); // (m_vecAnimMoveDelta.x * Cos(m_fRotationCur)) + -Sin(m_fRotationCur) * m_vecAnimMoveDelta.y;
	m_moved.y = DotProduct2D(m_vecAnimMoveDelta, forward);  // m_vecAnimMoveDelta.y* Cos(m_fRotationCur) + (m_vecAnimMoveDelta.x * Sin(m_fRotationCur));

	if (CTimer::GetTimeStep() >= 0.01f) {
		m_moved = m_moved * (1 / CTimer::GetTimeStep());
	} else {
		m_moved = m_moved * (1 / 100.0f);
	}

	// Crouch clips extract signed local movement, including lateral roll motion.
	// The standing mouse-strafe adjustment below would rotate that displacement
	// a second time and make backwards movement and rolls slide.
	if (bIsDucking && bCrouchWhenShooting)
		return;

	if ((!TheCamera.Cams[TheCamera.ActiveCam].GetWeaponFirstPersonOn() && !TheCamera.Cams[0].Using3rdPersonMouseCam())
		|| FindPlayerPed() != this || !CanStrafeOrMouseControl()) {

		if (FindPlayerPed() == this)
			FindPlayerPed()->m_fWalkAngle = 0.0f;
		return;
	}

	float walkAngle = WorkOutHeadingForMovingFirstPerson(m_fRotationCur);
	float pedSpeed = m_moved.Magnitude();
	float localWalkAngle = CGeneral::LimitRadianAngle(walkAngle - m_fRotationCur);

	if (localWalkAngle < -0.5f * PI) {
		localWalkAngle += PI;
	} else if (localWalkAngle > 0.5f * PI) {
		localWalkAngle -= PI;
	}

	// Interestingly this part is responsible for diagonal walking.
	if (localWalkAngle > -DEGTORAD(50.0f) && localWalkAngle < DEGTORAD(50.0f)) {
		TheCamera.Cams[TheCamera.ActiveCam].m_fPlayerVelocity = pedSpeed;
		m_moved = CVector2D(-Sin(walkAngle), Cos(walkAngle)) * pedSpeed;
	}

	CAnimBlendAssociation *idleAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_IDLE);
	CAnimBlendAssociation *fightAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_FIGHT_IDLE);
	if(!fightAssoc)
		fightAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_IDLE_TIRED);

	if(!fightAssoc)
		fightAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_FIGHT_IDLE);

	if ((!idleAssoc || idleAssoc->blendAmount < 0.5f) && !fightAssoc && !bIsDucking) {
		LimbOrientation newUpperLegs;
		newUpperLegs.yaw = localWalkAngle;

		if (newUpperLegs.yaw < -DEGTORAD(100.0f)) {
			newUpperLegs.yaw += PI;
		} else if (newUpperLegs.yaw > DEGTORAD(100.0f)) {
			newUpperLegs.yaw -= PI;
		}

		if (newUpperLegs.yaw > -DEGTORAD(50.0f) && newUpperLegs.yaw < DEGTORAD(50.0f)) {
			newUpperLegs.pitch = 0.1f;
			RwV3d Xaxis = { 1.0f, 0.0f, 0.0f };
			RwV3d Zaxis = { 0.0f, 0.0f, 1.0f };
			// The leg bones only exist on a skeleton that carries them.
			RtQuat *legL = FrameQuat(PED_NODE_LEFT_LEG);
			RtQuat *legR = FrameQuat(PED_NODE_RIGHT_LEG);
			if (legL && legR) {
				RtQuatRotate(legL, &Zaxis, RADTODEG(newUpperLegs.pitch), rwCOMBINEPOSTCONCAT);
				RtQuatRotate(legL, &Xaxis, RADTODEG(newUpperLegs.yaw), rwCOMBINEPOSTCONCAT);
				RtQuatRotate(legR, &Zaxis, RADTODEG(newUpperLegs.pitch), rwCOMBINEPOSTCONCAT);
				RtQuatRotate(legR, &Xaxis, RADTODEG(newUpperLegs.yaw), rwCOMBINEPOSTCONCAT);
			}
			bDontAcceptIKLookAts = true;
		}
	}
}

float
CPed::WorkOutHeadingForMovingFirstPerson(float offset)
{
	if (!IsPlayer())
		return 0.0f;

	CPad *pad0 = CPad::GetPad(0);
	float leftRight = pad0->GetPedWalkLeftRight();
	float upDown = pad0->GetPedWalkUpDown();
	float &angle = ((CPlayerPed*)this)->m_fWalkAngle;

	if (upDown != 0.0f) {
		angle = CGeneral::GetRadianAngleBetweenPoints(0.0f, 0.0f, -leftRight, upDown);
	} else {
		if (leftRight < 0.0f)
			angle = HALFPI;
		else if (leftRight > 0.0f)
			angle = -HALFPI;
	}

	return CGeneral::LimitRadianAngle(offset + angle);
}

void
CPed::UpdatePosition(void)
{
	if (!bIsStanding || m_attachedTo)
		return;

	CVector2D velocityChange;

	SetHeading(m_fRotationCur);
	if (m_pCurrentPhysSurface) {
		CVector2D velocityOfSurface;
		if (!IsPlayer() && m_pCurrentPhysSurface->IsVehicle() && ((CVehicle*)m_pCurrentPhysSurface)->IsBoat()) {

			// It seems R* didn't like m_vecOffsetFromPhysSurface for boats
			CVector offsetToSurface = GetPosition() - m_pCurrentPhysSurface->GetPosition();
			offsetToSurface.z -= FEET_OFFSET;

			CVector surfaceMoveVelocity = m_pCurrentPhysSurface->m_vecMoveSpeed;
			CVector surfaceTurnVelocity = CrossProduct(m_pCurrentPhysSurface->m_vecTurnSpeed, offsetToSurface);

			// Also we use that weird formula instead of friction if it's boat
			float slideMult = -m_pCurrentPhysSurface->m_vecTurnSpeed.MagnitudeSqr();
			velocityOfSurface = slideMult * offsetToSurface * CTimer::GetTimeStep() + (surfaceTurnVelocity + surfaceMoveVelocity);
			m_vecMoveSpeed.z = slideMult * offsetToSurface.z * CTimer::GetTimeStep() + (surfaceTurnVelocity.z + surfaceMoveVelocity.z);
		} else {
			velocityOfSurface = m_pCurrentPhysSurface->GetSpeed(m_vecOffsetFromPhysSurface);
		}
		// Reminder: m_moved is displacement from walking/running.
		velocityChange = m_moved + velocityOfSurface - m_vecMoveSpeed;
		m_fRotationCur += m_pCurrentPhysSurface->m_vecTurnSpeed.z * CTimer::GetTimeStep();
		m_fRotationDest += m_pCurrentPhysSurface->m_vecTurnSpeed.z * CTimer::GetTimeStep();
	} else if (m_nSurfaceTouched == SURFACE_STEEP_CLIFF && (m_vecDamageNormal.x != 0.0f || m_vecDamageNormal.y != 0.0f)) {
		// Ped got damaged by steep slope
		m_vecMoveSpeed = CVector(0.0f, 0.0f, -0.001f);
		// some kind of
		CVector2D reactionForce = m_vecDamageNormal;
		reactionForce.Normalise();

		velocityChange = 0.02f * reactionForce + m_moved;

		float reactionAndVelocityDotProd = DotProduct2D(reactionForce, velocityChange);
		// they're in same direction
		if (reactionAndVelocityDotProd < 0.0f) {
			velocityChange -= reactionAndVelocityDotProd * reactionForce;
		}
	} else {
		velocityChange = m_moved - m_vecMoveSpeed;
	}
	
	// Take time step into account
	if (m_pCurrentPhysSurface && (!m_pCurrentPhysSurface->bInfiniteMass || m_pCurrentPhysSurface->m_phy_flagA08)) {
		float speedChange = velocityChange.Magnitude();
		float changeMult = speedChange;
		if (m_nPedState == PED_DIE && m_pCurrentPhysSurface->IsVehicle()) {
			changeMult = 0.002f * CTimer::GetTimeStep();
		} else if (!(m_pCurrentPhysSurface->IsVehicle() && ((CVehicle*)m_pCurrentPhysSurface)->IsBoat())) {
			changeMult = 0.01f * CTimer::GetTimeStep();
		}

		if (speedChange > changeMult) {
			velocityChange = velocityChange * (changeMult / speedChange);
		}
	}
	m_vecMoveSpeed.x += velocityChange.x;
	m_vecMoveSpeed.y += velocityChange.y;
}

void
CPed::CalculateNewOrientation(void)
{
	if (!IsPedInControl())
		return;

	SetHeading(m_fRotationCur);
}

void
CPed::ClearAll(void)
{
	if (!IsPedInControl() && m_nPedState != PED_DEAD)
		return;

	SetPedState(PED_NONE);
	SetMoveState(PEDMOVE_NONE);
	m_pSeekTarget = nil;
	m_vecSeekPos = CVector(0.0f, 0.0f, 0.0f);
	bUsesCollision = true;
	ClearPointGunAt();
	bIsPointingGunAt = false;
	bRenderPedInCar = true;
	bKnockedUpIntoAir = false;
	bKnockedOffBike = false;
	m_pCollidingEntity = nil;
}

// SA CPed::ProcessBuoyancy creates CTaskComplexInWater as soon as the ped's
// origin is 0.6 under the surface, and CPed::ProcessControl destroys it once
// the ped can stand again. reVC has no task manager, so the same state lives
// in m_pSwimTask and is stepped from here.
void
CPed::UpdateSwim(void)
{
	if (bInVehicle) {
		if (m_pSwimTask) {
			delete m_pSwimTask;
			m_pSwimTask = nil;
		}
		return;
	}

	float waterLevel = 0.0f;
	bool hasWater = CWaterLevel::GetWaterLevel(GetPosition(), &waterLevel, false);

	// A ped in the air, an arrested ped and a corpse never swim: SA creates the
	// task from the buoyancy pass, which those states skip.
	bool canSwim = hasWater && !DyingOrDead() && m_nPedState != PED_ARRESTED
	               && m_nPedState != PED_JUMP && m_nPedState != PED_FALL;

	if (m_pSwimTask == nil) {
		if (canSwim && GetPosition().z + 1.0f + 0.6f < waterLevel) {
			m_pSwimTask = new CTaskSimpleSwim;
			// Straight into the underwater stroke when the drop was deep enough
			// that the ped never broke the surface on the way in.
			m_nSwimmingMoveState = PEDMOVE_STILL;
			SetPedState(PED_IDLE);
			m_vecMoveSpeed.z = 0.0f;
		}
		return;
	}

	// The strand exit: no water at all, or standing on the bed with the origin
	// above the swim line. SA gives a ped standing in the shallows 15 steps to
	// find deeper water before it drops the task.
	if (!hasWater) {
		delete m_pSwimTask;
		m_pSwimTask = nil;
		m_nSwimmingMoveState = PEDMOVE_STILL;
		return;
	}

	if (bIsStanding && !(GetPosition().z + 1.0f + 0.6f < waterLevel)) {
		// SA's SWIM_STOP_TIME is 15 frames, which CTimer's frame-scaled step
		// measures directly.
		m_fSwimStopTime += CTimer::GetTimeStep();
		if (m_fSwimStopTime > 15.0f) {
			delete m_pSwimTask;
			m_pSwimTask = nil;
			m_nSwimmingMoveState = PEDMOVE_STILL;
			SetMoveAnim();
			return;
		}
	} else {
		m_fSwimStopTime = 0.0f;
	}

	if (DyingOrDead()) {
		delete m_pSwimTask;
		m_pSwimTask = nil;
		m_nSwimmingMoveState = PEDMOVE_STILL;
		return;
	}

	// While swimming, the ped stays in PED_IDLE: the stroke is an animation,
	// not a state, so the rest of the ped code keeps working - a swimmer can
	// still be shot at, drown and die without the water code fighting it.
	if (!DyingOrDead() && m_nPedState != PED_ARRESTED)
		m_pSwimTask->ProcessPed(this);
}

// SA's climb entry, as CPed::ProcessControl does it: on the frame the player
// presses jump against a wall, ask the collision scan whether there is a ledge
// to grab, cancel the jump if there is, and hand the ped to the climb task.
bool
CPed::TryToClimb(void)
{
	if (m_pClimbTask)
		return true;

	if (!IsPedInControl() || bInVehicle || bIsInWater || bIsInTheAir || bIsLanding)
		return false;

	CVector climbPos;
	float climbHeading = 0.0f;
	eSurfaceType surface = SURFACE_DEFAULT;

	CEntity *entity = CTaskSimpleClimb::TestForClimb(this, climbPos, climbHeading, surface, false);
	if (entity == nil)
		return false;

	m_pClimbTask = new CTaskSimpleClimb(entity, climbPos, climbHeading, surface, CLIMB_GRAB, true);
	if (m_pClimbTask == nil)
		return false;

	m_nPedState = PED_IDLE;
	bIsStanding = false;
	bIsInTheAir = false;
	bIsLanding = false;
	m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f);
	return true;
}

void
CPed::ProcessBuoyancy(void)
{
	float buoyancyLevel = 1.1f;
	// Per ped; see m_nWaterCirclesTime. These used to be statics, so every ped
	// in the water shared one pair of timers.
	CRGBA color;

	if (bInVehicle)
		return;

	CVector buoyancyPoint;
	CVector buoyancyImpulse;

	if (DyingOrDead())
		buoyancyLevel = 1.8f;

	if (mod_Buoyancy.ProcessBuoyancy(this, GRAVITY * m_fMass * buoyancyLevel, &buoyancyPoint, &buoyancyImpulse)) {
		bTouchingWater = true;
		CEntity *entity;
		CColPoint point;
		if (CWorld::ProcessVerticalLine(GetPosition(), GetPosition().z - 3.0f, point, entity, false, true, false, false, false, false, nil)
			&& entity->IsVehicle() && ((CVehicle*)entity)->IsBoat() && !entity->bRenderScorched) {
			bIsInWater = false;
			return;
		}
		color.r = (0.5f * CTimeCycle::GetDirectionalRed() + CTimeCycle::GetAmbientRed()) * 127.5f;
		color.g = (0.5f * CTimeCycle::GetDirectionalBlue() + CTimeCycle::GetAmbientBlue()) * 127.5f;
		color.b = (0.5f * CTimeCycle::GetDirectionalGreen() + CTimeCycle::GetAmbientGreen()) * 127.5f;
		color.a = CGeneral::GetRandomNumberInRange(48.0f, 96.0f);
		bIsInWater = true;
		ApplyMoveForce(buoyancyImpulse);
		if (!DyingOrDead()) {
			if (bTryingToReachDryLand) {
				if (buoyancyImpulse.z / m_fMass > GRAVITY * 0.4f * CTimer::GetTimeStep()) {
					bTryingToReachDryLand = false;
					CVector pos = GetPosition();
					if (PlacePedOnDryLand()) {
						if (m_fHealth > 20.0f)
							InflictDamage(nil, WEAPONTYPE_DROWNING, 15.0f, PED_PIECE_TORSO, false);

						if (bIsInTheAir) {
							RpAnimBlendClumpSetBlendDeltas(GetClump(), ASSOC_PARTIAL, -1000.0f);
							bIsInTheAir = false;
						}
						pos.z = pos.z - 0.8f;
						CParticleObject::AddObject(POBJECT_PED_WATER_SPLASH, pos, CVector(0.0f, 0.0f, 0.0f), 0.0f, 50, color, true);
						m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f);
						SetPedState(PED_IDLE);
						return;
					}
				}
			}
		}
		float speedMult = 0.0f;
		if (buoyancyImpulse.z / m_fMass > GRAVITY * CTimer::GetTimeStep()
			|| mod_Buoyancy.m_waterlevel > GetPosition().z + 0.6f) {
			speedMult = pow(0.9f, CTimer::GetTimeStep());
			m_vecMoveSpeed.x *= speedMult;
			m_vecMoveSpeed.y *= speedMult;
			m_vecMoveSpeed.z *= speedMult;
			bIsStanding = false;
			// SA drowns a ped from the swim task's breath meter, not from the
			// buoyancy pass: a ped merely bobbing with his head under for one
			// frame is not drowning. The ped is left to CTaskSimpleSwim, which
			// knows whether the player is actually out of air, and to the AI
			// peds, which do not hold their breath at all.
			if (m_pSwimTask == nil && !IsPlayer()) {
				bIsDrowning = true;
				InflictDamage(nil, WEAPONTYPE_DROWNING, 3.0f * CTimer::GetTimeStep(), PED_PIECE_TORSO, 0);
			}
		}
		if (buoyancyImpulse.z / m_fMass > GRAVITY * 0.25f * CTimer::GetTimeStep()) {
			if (speedMult == 0.0f) {
				speedMult = pow(0.9f, CTimer::GetTimeStep());
			}
			m_vecMoveSpeed.x *= speedMult;
			m_vecMoveSpeed.y *= speedMult;
			if (m_vecMoveSpeed.z >= -0.1f) {
				if (m_vecMoveSpeed.z < -0.04f)
					m_vecMoveSpeed.z = -0.02f;
			} else {
				m_vecMoveSpeed.z = -0.01f;
				DMAudio.PlayOneShot(m_audioEntityId, SOUND_SPLASH, 0.0f);
				CVector aBitForward = 2.2f * m_vecMoveSpeed + GetPosition();
				float level = 0.0f;
				if (CWaterLevel::GetWaterLevel(aBitForward, &level, false))
					aBitForward.z = level;

				CParticleObject::AddObject(POBJECT_PED_WATER_SPLASH, aBitForward, CVector(0.0f, 0.0f, 0.1f), 0.0f, 200, color, true);
				m_nRaindropsTime = CTimer::GetTimeInMilliseconds() + 80;
				m_nWaterCirclesTime = CTimer::GetTimeInMilliseconds() + 100;
			}
		}
		if (m_nWaterCirclesTime && CTimer::GetTimeInMilliseconds() >= m_nWaterCirclesTime) {
			CVector pos = GetPosition();
			float level = 0.0f;
			if (CWaterLevel::GetWaterLevel(pos, &level, false))
				pos.z = level;

			if (pos.z != 0.0f) {
				m_nWaterCirclesTime = 0;
				for(int i = 0; i < 4; i++) {
					pos.x += CGeneral::GetRandomNumberInRange(-0.75f, 0.75f);
					pos.y += CGeneral::GetRandomNumberInRange(-0.75f, 0.75f);
					CParticle::AddParticle(PARTICLE_RAIN_SPLASH_BIGGROW, pos, CVector(0.0f, 0.0f, 0.0f), nil, 0.0f, color, 0, 0, 0, 0);
				}
			}
		}
		if (m_nRaindropsTime && CTimer::GetTimeInMilliseconds() >= m_nRaindropsTime) {
			CVector pos = GetPosition();
			float level = 0.0f;
			if (CWaterLevel::GetWaterLevel(pos, &level, false))
				pos.z = level;

			if (pos.z >= 0.0f) {
				pos.z += 0.25f;
				m_nRaindropsTime = 0;
				CParticleObject::AddObject(POBJECT_SPLASHES_AROUND, pos, CVector(0.0f, 0.0f, 0.0f), 4.5f, 1500, CRGBA(0,0,0,0), true);
			}
		}
	} else
		bTouchingWater = false;
}

void
CPed::ProcessControl(void)
{
	CColPoint foundCol;
	CEntity *foundEnt = nil;

	if (CTimer::GetFrameCounter() + m_randomSeed % 32 == 0)
		PruneReferences();

	int alpha = CVisibilityPlugins::GetClumpAlpha(GetClump());
	if (!bFadeOut) {
		if (alpha < 255) {
			alpha += 16;
			if (alpha > 255)
				alpha = 255;
		}
	} else {
		alpha -= 8;
		if (alpha < 0)
			alpha = 0;
	}

	CVisibilityPlugins::SetClumpAlpha(GetClump(), alpha);
	bIsShooting = false;
	bDonePositionOutOfCollision = false;
	bIsInWater = false;
	bIsDrowning = false;
	ProcessBuoyancy();
	UpdateSwim();

	// A climb owns the ped until it finishes: the world and the ped's own state
	// machine both have to stay out of the way, exactly as they do while the
	// ped is in a vehicle.
	if (m_pClimbTask) {
		if (m_pClimbTask->ProcessPed(this)) {
			delete m_pClimbTask;
			m_pClimbTask = nil;
		}
		CPhysical::ProcessControl();
		return;
	}

	if (m_nPedState != PED_ARRESTED) {
		if (m_nPedState == PED_DEAD) {
			DeadPedMakesTyresBloody();
			if (CGame::nastyGame && !bIsInWater) {
				uint32 remainingBloodyFpTime = CTimer::GetTimeInMilliseconds() - m_bloodyFootprintCountOrDeathTime;
				float timeDependentDist;
				if (remainingBloodyFpTime >= 2000) {
					if (remainingBloodyFpTime <= 7000)
						timeDependentDist = (remainingBloodyFpTime - 2000) / 5000.0f * 0.75f;
					else
						timeDependentDist = 0.75f;
				} else {
					timeDependentDist = 0.0f;
				}

				CPed *nearPeds[10];
				int16 numNearPeds = FindNearPeds(GetPosition(), timeDependentDist, nearPeds, ARRAY_SIZE(nearPeds));
				for (int i = 0; i < numNearPeds; ++i) {
					CPed *nearPed = nearPeds[i];
					if (!nearPed->DyingOrDead()) {
						CVector dist = nearPed->GetPosition() - GetPosition();
						if (dist.MagnitudeSqr() < sq(timeDependentDist)) {
							nearPed->m_bloodyFootprintCountOrDeathTime = 200;
							nearPed->bDoBloodyFootprints = true;
							if (nearPed->IsPlayer()) {
								if (!nearPed->bIsLooking && nearPed->m_nPedState != PED_ATTACK) {
									int16 camMode = TheCamera.Cams[TheCamera.ActiveCam].Mode;
									if (camMode != CCam::MODE_SNIPER
										&& camMode != CCam::MODE_CAMERA
										&& camMode != CCam::MODE_ROCKETLAUNCHER
										&& camMode != CCam::MODE_M16_1STPERSON
										&& camMode != CCam::MODE_1STPERSON
										&& camMode != CCam::MODE_HELICANNON_1STPERSON
										&& !TheCamera.Cams[TheCamera.ActiveCam].GetWeaponFirstPersonOn()) {

										nearPed->SetLookFlag(this, true);
										nearPed->SetLookTimer(500);
									}
								}
							}
						}
					}
				}

				if (remainingBloodyFpTime > 2000) {
					CVector bloodPos = GetPosition();
					if (remainingBloodyFpTime - 2000 >= 5000) {
						if (!m_deadBleeding) {
							CShadows::AddPermanentShadow(SHADOWTYPE_DARK, gpBloodPoolTex, &bloodPos,
								0.75f, 0.0f, 0.0f, -0.75f, 255, 255, 0, 0, 4.0f, 40000, 1.0f);
							m_deadBleeding = true;
						}
					} else {
						CShadows::StoreStaticShadow(
							(uintptr)this + 17, SHADOWTYPE_DARK, gpBloodPoolTex, &bloodPos,
							(remainingBloodyFpTime - 2000) / 5000.0f * 0.75f, 0.0f,
							0.0f, (remainingBloodyFpTime - 2000) / 5000.0f * -0.75f,
							255, 255, 0, 0, 4.0f, 1.0f, 40.0f, false, 0.0f);
					}
				}
			}
			if (ServiceTalkingWhenDead())
				ServiceTalking();

			if (bIsInWater) {
				bIsStanding = false;
				bWasStanding = false;
				CPhysical::ProcessControl();
			}
			return;
		}

		bWasStanding = false;
		if (bIsStanding) {
			if (!CWorld::bForceProcessControl) {
				if (m_pCurrentPhysSurface && m_pCurrentPhysSurface->bIsInSafePosition) {
					bWasPostponed = true;
					return;
				}
			}
		}

		if (m_fHealth <= 1.0f && m_nPedState <= PED_STATES_NO_AI && !bIsInTheAir && !bIsLanding)
			SetDie();

		if (bIsStanding)
			bPushedAlongByCar = false;

		bCollidedWithMyVehicle = false;

		CEntity *collidingEnt = m_pDamageEntity;
		if (!bUsesCollision || ((!collidingEnt || m_fDamageImpulse <= 0.0f) && (!IsPlayer() || !bIsStuck)) || m_nPedState == PED_DIE) {	
			bHitSomethingLastFrame = false;
			if (m_nPedStateTimer <= 500 && bIsInTheAir) {
				if (m_nPedStateTimer)
					m_nPedStateTimer--;
			} else if (m_nPedStateTimer < 1001) {
				m_nPedStateTimer = 0;
			}
		} else {
			if (collidingEnt) {
				switch (collidingEnt->GetType())
				{
				case ENTITY_TYPE_BUILDING:
				case ENTITY_TYPE_OBJECT:
					bHitSomethingLastFrame = true;
					break;
				case ENTITY_TYPE_VEHICLE:
				{
					CVehicle* collidingVeh = ((CVehicle*)collidingEnt);
					float collidingVehSpeedSqr = collidingVeh->m_vecMoveSpeed.MagnitudeSqr();

					if (collidingVeh == m_pMyVehicle)
						bCollidedWithMyVehicle = true;

					float oldHealth = m_fHealth;
					bool playerSufferSound = false;

					if (collidingVehSpeedSqr <= 1.0f / 400.0f) {
						if (IsPedInControl() && (!IsPlayer() || m_objective == OBJECTIVE_ENTER_CAR_AS_DRIVER)) {
							if ((collidingVeh != m_pCurrentPhysSurface || IsPlayer()) && !bVehEnterDoorIsBlocked) {
								if (m_nPedState == PED_SEEK_CAR) {
									SetDirectionToWalkAroundObject(collidingVeh);
									CWorld::Players[CWorld::PlayerInFocus].m_nLastBumpPlayerCarTimer = m_nPedStateTimer;
								} else {
									SetDirectionToWalkAroundObject(collidingVeh);
								}
							}
					} else if (IsPlayer() && !bIsInTheAir) {

							if (IsPedInControl() && ((CPlayerPed*)this)->MoveSpeed() == 0.0f
								&& !bIsLooking && CTimer::GetTimeInMilliseconds() > m_lookTimer && collidingVeh->pDriver) {

								SetLookFlag(collidingVeh, true);
								SetLookTimer(1300);

								eWeaponType weaponType = GetWeapon()->m_eWeaponType;
								uint32 weaponSlot = CWeaponInfo::GetWeaponInfo(weaponType)->m_nWeaponSlot;
								if (weaponType == WEAPONTYPE_UNARMED
									|| weaponSlot == 3
									|| weaponSlot == 5
									|| weaponSlot == 1) {
									bShakeFist = true;
								}
							} else {
								SetLookFlag(collidingVeh, true);
								SetLookTimer(500);
							}
						}
					} else {
						float adjustedImpulse = m_fDamageImpulse;
						if (IsPlayer()) {
							if (bIsStanding) {
								float forwardVecAndDamageDirDotProd = DotProduct(m_vecAnimMoveDelta.y * GetForward(), m_vecDamageNormal);
								if (forwardVecAndDamageDirDotProd < 0.0f) {
									adjustedImpulse = forwardVecAndDamageDirDotProd * m_fMass + m_fDamageImpulse;
									if (adjustedImpulse < 0.0f)
										adjustedImpulse = 0.0f;
								}
							}
						}
						if (m_fMass / 20.0f < adjustedImpulse)
							DMAudio.PlayOneShot(collidingVeh->m_audioEntityId, SOUND_CAR_PED_COLLISION, adjustedImpulse);

						if (IsPlayer()) {
							if (adjustedImpulse > 20.0f)
								adjustedImpulse = 20.0f;

							if (adjustedImpulse > 5.0f) {
								if (adjustedImpulse <= 13.0f)
									playerSufferSound = true;
								else
									Say(SOUND_PED_DAMAGE);
							}

							CColModel *collidingCol = CModelInfo::GetColModel(collidingVeh->m_modelIndex);
							CVector colMinVec = collidingCol->boundingBox.min;
							CVector colMaxVec = collidingCol->boundingBox.max;

							CVector vehColCenterDist = collidingVeh->GetMatrix() * ((colMinVec + colMaxVec) * 0.5f) - GetPosition();

							// TLVC = To look vehicle center

							float angleToVehFront = collidingVeh->GetForward().Heading();
							float angleDiffFromLookingFrontTLVC = angleToVehFront - vehColCenterDist.Heading();
							angleDiffFromLookingFrontTLVC = CGeneral::LimitRadianAngle(angleDiffFromLookingFrontTLVC);

							// I don't know why do we use that
							float vehTopRightHeading = Atan2(colMaxVec.x - colMinVec.x, colMaxVec.y - colMinVec.y);

							CVector vehDist = GetPosition() - collidingVeh->GetPosition();
							vehDist.Normalise();

							float vehRightVecAndSpeedDotProd;

							if (Abs(angleDiffFromLookingFrontTLVC) >= vehTopRightHeading && Abs(angleDiffFromLookingFrontTLVC) < PI - vehTopRightHeading) {
								if (angleDiffFromLookingFrontTLVC <= 0.0f) {
									vehRightVecAndSpeedDotProd = DotProduct(collidingVeh->GetRight(), collidingVeh->m_vecMoveSpeed);

									// vehRightVecAndSpeedDotProd < 0.1f = Vehicle being overturned or spinning to it's right?
									if (collidingVehSpeedSqr > 1.0f / 100.0f && vehRightVecAndSpeedDotProd < 0.1f) {

										// Car's right faces towards us and isn't coming directly to us
										if (DotProduct(collidingVeh->GetRight(), GetForward()) < 0.0f
											&& DotProduct(vehDist, collidingVeh->m_vecMoveSpeed) > 0.0f) {
											SetEvasiveStep(collidingVeh, 1);
										}
									}
								} else {
									vehRightVecAndSpeedDotProd = DotProduct(-1.0f * collidingVeh->GetRight(), collidingVeh->m_vecMoveSpeed);

									if (collidingVehSpeedSqr > 1.0f / 100.0f && vehRightVecAndSpeedDotProd < 0.1f) {
										if (DotProduct(collidingVeh->GetRight(), GetForward()) > 0.0f
											&& DotProduct(vehDist, collidingVeh->m_vecMoveSpeed) > 0.0f) {
											SetEvasiveStep(collidingVeh, 1);
										}
									}
								}
							} else {
								vehRightVecAndSpeedDotProd = DotProduct(vehDist, collidingVeh->m_vecMoveSpeed);
							}

							if (vehRightVecAndSpeedDotProd <= 0.1f) {
								if (m_nPedState != PED_FIGHT) {
									SetLookFlag(collidingVeh, true);
									SetLookTimer(700);
								}
							} else {
								bIsStanding = false;
								CVector2D collidingEntMoveDir = -collidingVeh->m_vecMoveSpeed;
								int dir = GetLocalDirection(collidingEntMoveDir);
								SetFall(1000, (AnimationId)(dir + ANIM_ID_KO_SKID_FRONT), false);

								InflictDamage(collidingVeh, WEAPONTYPE_RAMMEDBYCAR, 20.0f, PED_PIECE_TORSO, dir);
								Say(SOUND_PED_DAMAGE);
							}
						} else {
							KillPedWithCar(collidingVeh, m_fDamageImpulse);
						}
						
						if (m_pCollidingEntity != collidingEnt)
							bPushedAlongByCar = true;
					}
					if (m_fHealth < oldHealth && playerSufferSound)
						Say(SOUND_PED_HIT);

					break;
				}
				case ENTITY_TYPE_PED:
					break;
				default:
					break;
				}
			}
			CVector forceDir;
			if (!bIsInTheAir && m_nPedState != PED_JUMP && m_fDamageImpulse > 0.0f) {
				forceDir = m_vecDamageNormal;
				forceDir.z = 0.0f;
				if (!bIsStanding) {
					forceDir *= 4.0f;
				} else {
					forceDir *= 0.5f;
				}

				ApplyMoveForce(forceDir);
			}
			if ((bIsInTheAir && !DyingOrDead()) || (!bIsStanding && !bWasStanding && m_nPedState == PED_FALL)) {
				if (m_nPedStateTimer > 0 && m_nPedStateTimer <= 1000) {
					forceDir = GetPosition() - m_vecHitLastPos;
				} else {
					m_nPedStateTimer = 0;
					m_vecHitLastPos = GetPosition();
					forceDir = CVector(0.0f, 0.0f, 0.0f);
				}

				CVector offsetToCheck;
				m_nPedStateTimer++;

				float adjustedTs = Max(CTimer::GetTimeStep(), 0.01f);

				CPad *pad0 = CPad::GetPad(0);
				if ((m_nPedStateTimer <= 50.0f / (4.0f * adjustedTs) || m_nPedStateTimer * 0.01f <= forceDir.MagnitudeSqr())
					&& (m_nCollisionRecords <= 1 || m_nPedStateTimer <= 50.0f / (2.0f * adjustedTs) || m_nPedStateTimer * 1.0f / 250.0f <= Abs(forceDir.z))) {

					if (m_nCollisionRecords == 1 && m_aCollisionRecords[0] != nil && m_aCollisionRecords[0]->IsBuilding()
						&& m_nPedStateTimer > 50.0f / (2.0f * adjustedTs) && m_nPedStateTimer * 1.0f / 250.0f > Abs(forceDir.z)) {
						offsetToCheck.x = -forceDir.y;
						offsetToCheck.z = 1.0f;
						offsetToCheck.y = forceDir.x;
						offsetToCheck.Normalise();

						CVector posToCheck = GetPosition() + offsetToCheck;

						// These are either obstacle or ground to land, I don't know which one.
						float obstacleForFlyingZ, obstacleForFlyingOtherDirZ;
						CColPoint obstacleForFlying, obstacleForFlyingOtherDir;

						// Check is there any room for being knocked up in reverse direction of force
						if (CWorld::ProcessVerticalLine(posToCheck, -20.0f, obstacleForFlying, foundEnt, true, false, false, false, false, false, nil)) {
							obstacleForFlyingZ = obstacleForFlying.point.z;
						} else {
							obstacleForFlyingZ = 500.0f;
						}
						
						posToCheck = GetPosition() - offsetToCheck;

						// Now check for direction of force this time
						if (CWorld::ProcessVerticalLine(posToCheck, -20.0f, obstacleForFlyingOtherDir, foundEnt, true, false, false, false, false, false, nil)) {
							obstacleForFlyingOtherDirZ = obstacleForFlyingOtherDir.point.z;
						} else {
							obstacleForFlyingOtherDirZ = 501.0f;
						}
						int16 flyDir = 0;
						float feetZ = GetPosition().z - FEET_OFFSET;
#ifdef FIX_BUGS
						if (obstacleForFlyingZ > feetZ && obstacleForFlyingZ < 500.0f)
							flyDir = 1;
						else if (obstacleForFlyingOtherDirZ > feetZ && obstacleForFlyingOtherDirZ < 501.0f)
							flyDir = 2;
#else
						if ((obstacleForFlyingZ > feetZ && obstacleForFlyingOtherDirZ < 500.0f) || (obstacleForFlyingZ > feetZ && obstacleForFlyingOtherDirZ > feetZ))
							flyDir = 1;
						else if (obstacleForFlyingOtherDirZ > feetZ && obstacleForFlyingZ < 499.0f)
							flyDir = 2;
#endif

						if (flyDir > 0 && !bHeadStuckInCollision) {
							GetMatrix().SetTranslateOnly(flyDir == 2 ? obstacleForFlyingOtherDir.point : obstacleForFlying.point);
							GetMatrix().GetPosition().z += FEET_OFFSET;
							GetMatrix().UpdateRW();
							SetLanding();
							bIsStanding = true;
						}
						if (obstacleForFlyingZ < obstacleForFlyingOtherDirZ) {
							offsetToCheck *= -1.0f;
						}
						offsetToCheck.z = 1.0f;
						forceDir = 4.0f * offsetToCheck;
						forceDir.z = 4.0f;
						ApplyMoveForce(forceDir);

						// What was that for?? It pushes player inside of collision sometimes and kills him.
#ifdef FIX_BUGS
						if (!IsPlayer())
#endif
						GetMatrix().GetPosition() += 0.25f * offsetToCheck;

						m_fRotationCur = CGeneral::GetRadianAngleBetweenPoints(offsetToCheck.x, offsetToCheck.y, 0.0f, 0.0f);
						m_fRotationCur = CGeneral::LimitRadianAngle(m_fRotationCur);
						m_fRotationDest = m_fRotationCur;
						SetHeading(m_fRotationCur);

						if (m_nPedState != PED_FALL && !bIsPedDieAnimPlaying) {
							SetFall(1000, ANIM_ID_KO_SKID_BACK, true);
						}
						bIsInTheAir = false;
					} else if (m_vecDamageNormal.z > 0.4f) {
						if (m_nPedState == PED_JUMP) {
							if (m_animTimer <= 2000) {
								if (m_animTimer < 1000)
									m_animTimer += CTimer::GetTimeStepInMilliseconds();
							} else {
								m_animTimer = 0;
							}
						}
						forceDir = m_vecDamageNormal;
						forceDir.z = 0.0f;
						forceDir.Normalise();
						if (m_nPedState != PED_JUMP || m_animTimer >= 300) {
							ApplyMoveForce(2.0f * forceDir);
						} else {
							ApplyMoveForce(-4.0f * forceDir);
						}
					}
				} else if ((CTimer::GetFrameCounter() + m_randomSeed % 256 + 3) & 7) {
					if (IsPlayer() && m_nPedState != PED_JUMP && pad0->JumpJustDown()) {
						int16 padWalkX = pad0->GetPedWalkLeftRight();
						int16 padWalkY = pad0->GetPedWalkUpDown();
						if (Abs(padWalkX) > 0.0f || Abs(padWalkY) > 0.0f) {
							m_fRotationDest = CGeneral::GetRadianAngleBetweenPoints(0.0f, 0.0f, -padWalkX, padWalkY);
							m_fRotationDest -= TheCamera.Orientation;
							m_fRotationDest = CGeneral::LimitRadianAngle(m_fRotationDest);
							m_fRotationCur = m_fRotationDest;
							SetHeading(m_fRotationCur);
						}
						SetJump();
						m_nPedStateTimer = 0;
						m_vecHitLastPos = GetPosition();

						// Why? forceDir is unused after this point.
						forceDir = CVector(0.0f, 0.0f, 0.0f);
					} else if (IsPlayer()) {
						int16 padWalkX = pad0->GetPedWalkLeftRight();
						int16 padWalkY = pad0->GetPedWalkUpDown();
						if (Abs(padWalkX) > 0.0f || Abs(padWalkY) > 0.0f) {
							m_fRotationDest = CGeneral::GetRadianAngleBetweenPoints(0.0f, 0.0f, -padWalkX, padWalkY);
							m_fRotationDest -= TheCamera.Orientation;
							m_fRotationDest = CGeneral::LimitRadianAngle(m_fRotationDest);
							m_fRotationCur = m_fRotationDest;
							SetHeading(m_fRotationCur);
						}
						CAnimBlendAssociation *jumpAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_JUMP_GLIDE);

						if (!jumpAssoc)
							jumpAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_FALL_GLIDE);

						if (jumpAssoc) {
							jumpAssoc->blendDelta = -3.0f;
							jumpAssoc->flags |= ASSOC_DELETEFADEDOUT;
						}
						if (m_nPedState == PED_JUMP)
							m_nPedState = PED_IDLE;
					} else {
						CAnimBlendAssociation *jumpAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_JUMP_GLIDE);

						if (!jumpAssoc)
							jumpAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_FALL_GLIDE);

						if (jumpAssoc) {
							jumpAssoc->blendDelta = -3.0f;
							jumpAssoc->flags |= ASSOC_DELETEFADEDOUT;
						}
					}
				} else {
					offsetToCheck = GetPosition();
					offsetToCheck.z += 0.5f;

					if (CWorld::ProcessVerticalLine(offsetToCheck, GetPosition().z - FEET_OFFSET, foundCol, foundEnt, true, true, false, true, false, false, nil)) {
						if (!bHeadStuckInCollision || FEET_OFFSET + foundCol.point.z < GetPosition().z) {
							GetMatrix().GetPosition().z = FEET_OFFSET + foundCol.point.z;
							GetMatrix().UpdateRW();
							if (bHeadStuckInCollision)
								bHeadStuckInCollision = false;
						}
						SetLanding();
						bIsStanding = true;
					}
				}
			} else if (m_nPedStateTimer < 1001) {
				m_nPedStateTimer = 0;
			}
		}

		if (bIsDucking)
			Duck();

		if (!bIsStanding && m_vecMoveSpeed.z > 0.25) {
			float airResistance = Pow(0.95f, CTimer::GetTimeStep());

			m_vecMoveSpeed *= airResistance;
		}
		if (IsPlayer() || !bIsStanding || m_vecMoveSpeed.x != 0.0f || m_vecMoveSpeed.y != 0.0f || m_vecMoveSpeed.z != 0.0f
			|| (m_nMoveState != PEDMOVE_NONE && m_nMoveState != PEDMOVE_STILL)
			|| m_vecAnimMoveDelta.x != 0.0f || m_vecAnimMoveDelta.y != 0.0f
			|| m_nPedState == PED_JUMP
			|| bIsInTheAir
			|| m_pCurrentPhysSurface) {

			CPhysical::ProcessControl();
		} else {
			bHasContacted = false;
			bIsInSafePosition = false;
			bWasPostponed = false;
			bHasHitWall = false;
			m_nCollisionRecords = 0;
			bHasCollided = false;
			m_nDamagePieceType = 0;
			m_fDamageImpulse = 0.0f;
			m_pDamageEntity = nil;
			m_vecTurnFriction = CVector(0.0f, 0.0f, 0.0f);
			m_vecMoveFriction = CVector(0.0f, 0.0f, 0.0f);
		}

		if (m_nPedState != PED_DIE || bIsPedDieAnimPlaying) {
			RequestDelayedWeapon();
			PlayFootSteps();
			if (m_nPedState != PED_DEAD) {
				CalculateNewVelocity();
				CalculateNewOrientation();
			}
			UpdatePosition();
			if (IsPedInControl() && !bIsStanding && !m_pDamageEntity) {
				if (m_attachedTo) {
					bIsInTheAir = false;
				} else if (CheckIfInTheAir()) {
					SetInTheAir();
					bHeadStuckInCollision = false;
				}
			}
			if (bHeadStuckInCollision) {
				CVector posToCheck = GetPosition();
				posToCheck.z += 0.9f;
				if (!CWorld::TestSphereAgainstWorld(posToCheck, 0.2f, this, true, true, false, true, false, false))
					bHeadStuckInCollision = false;
			}
			ProcessObjective();
			if (!bIsAimingGun) {
				if (bIsRestoringGun)
					RestoreGunPosition();
			} else {
				AimGun();
			}

			if (bIsLooking) {
				MoveHeadToLook();
			} else if (bIsRestoringLook) {
				RestoreHeadPosition();
			}

			if (bIsInTheAir)
				InTheAir();

			if (bUpdateAnimHeading) {
				if (m_nPedState != PED_GETUP && m_nPedState != PED_FALL) {
					m_fRotationCur -= HALFPI;
					m_fRotationDest = m_fRotationCur;
					bUpdateAnimHeading = false;
				}
			}

#ifdef CANCELLABLE_CAR_ENTER
			static bool cancelJack = false;
			if (IsPlayer()) {
				if (EnteringCar() && m_pVehicleAnim) {
					CPad *pad = CPad::GetPad(0);

					if (!pad->ArePlayerControlsDisabled()) {
						int vehAnim = m_pVehicleAnim->animId;

						int16 padWalkX = pad->GetPedWalkLeftRight();
						int16 padWalkY = pad->GetPedWalkUpDown();
						if (Abs(padWalkX) > 0.0f || Abs(padWalkY) > 0.0f) {
							if (vehAnim == ANIM_ID_CAR_OPEN_LHS || vehAnim == ANIM_ID_CAR_OPEN_RHS || vehAnim == ANIM_ID_CAR_OPEN_LHS || vehAnim == ANIM_ID_CAR_OPEN_LHS ||
								vehAnim == ANIM_ID_CAR_OPEN_LHS_1 || vehAnim == ANIM_ID_CAR_OPEN_RHS_1) {

								if (!m_pMyVehicle->pDriver) {
									cancelJack = false;
									bCancelEnteringCar = true;
								} else
									cancelJack = true;
							} else if (vehAnim == ANIM_ID_CAR_OPEN_LHS && m_pVehicleAnim->GetTimeLeft() > 0.75f) {
								cancelJack = true;
							} else if (vehAnim == ANIM_ID_CAR_PULLOUT_LHS || vehAnim == ANIM_ID_CAR_PULLOUT_LHS || vehAnim == ANIM_ID_CAR_PULLOUT_RHS || vehAnim == ANIM_ID_CAR_PULLOUT_RHS) {
								bCancelEnteringCar = true;
								cancelJack = false;
							}
						}
						if (cancelJack && vehAnim == ANIM_ID_CAR_OPEN_LHS && m_pVehicleAnim->GetTimeLeft() > 0.75f && m_pVehicleAnim->GetTimeLeft() < 0.78f) {
							cancelJack = false;
							QuitEnteringCar();
							RestorePreviousObjective();
						}
						if (cancelJack && (vehAnim == ANIM_ID_CAR_PULLOUT_LHS || vehAnim == ANIM_ID_CAR_PULLOUT_LHS || vehAnim == ANIM_ID_CAR_PULLOUT_RHS || vehAnim == ANIM_ID_CAR_PULLOUT_RHS)) {
							cancelJack = false;
							bCancelEnteringCar = true;
						}
					}
				} else
					cancelJack = false;
			}
#endif

			switch (m_nPedState) {
				case PED_IDLE:
					Idle();
					break;
				case PED_LOOK_ENTITY:
				case PED_LOOK_HEADING:
					Look();
					break;
				case PED_ENTER_CAR:
				case PED_CARJACK:
					break;
				case PED_ATTACK:
					Attack();
					break;
				case PED_FIGHT:
					Fight();
					break;
				case PED_AIMGUN:
					PointGunAt();
					break;
				case PED_SEEK_CAR:
					SeekCar();
					break;
				case PED_SEEK_BOAT_POSITION:
					SeekBoatPosition();
					break;
				case PED_FALL:
					Fall();
					break;
				case PED_GETUP:
					SetGetUp();
					break;
				case PED_DRIVING:
				{
					if (!m_pMyVehicle) {
						bInVehicle = false;
						FlagToDestroyWhenNextProcessed();
						return;
					}

#ifdef CAR_AIRBREAK
					if (IsPlayer()) {
						CPad* pad = CPad::GetPad(0);
						if (!pad->ArePlayerControlsDisabled()) {
							if (pad->GetHorn()) {
								float c = Cos(m_fRotationCur);
								float s = Sin(m_fRotationCur);
								m_pMyVehicle->GetRight() = CVector(1.0f, 0.0f, 0.0f);
								m_pMyVehicle->GetForward() = CVector(0.0f, 1.0f, 0.0f);
								m_pMyVehicle->GetUp() = CVector(0.0f, 0.0f, 1.0f);
								if (pad->GetAccelerate()) {
									m_pMyVehicle->ApplyMoveForce(GetForward() * 30.0f);
								}
								else if (pad->GetBrake()) {
									m_pMyVehicle->ApplyMoveForce(-GetForward() * 30.0f);
								}
								else {
									int16 lr = pad->GetSteeringLeftRight();
									if (lr < 0) {
										//m_pMyVehicle->ApplyTurnForce(20.0f * -GetRight(), GetForward());
										m_pMyVehicle->ApplyMoveForce(-GetRight() * 30.0f);
									}
									else if (lr > 0) {
										m_pMyVehicle->ApplyMoveForce(GetRight() * 30.0f);
									}
									else {
										m_pMyVehicle->ApplyMoveForce(0.0f, 0.0f, 50.0f);
									}
								}
							}
						}
					}
#endif

					if (m_pMyVehicle->pDriver == this) {
						DriveVehicle();
						if (!m_pMyVehicle)
							return;
					}
					break;
				}
				case PED_DIE:
					Die();
					break;
				case PED_HANDS_UP:
					if (!RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_HANDSUP)) {
						CAnimManager::AddAnimation(GetClump(), ASSOCGRP_DEFAULT, ANIM_ID_HANDSUP);
						Say(SOUND_PED_HANDS_UP);
					}
					break;
				default:
					break;
			}
			SetMoveAnim();
			if (bPedIsBleeding || m_bleedCounter != 0) {
				if (CGame::nastyGame) {
					if (m_bleedCounter != 0)
						m_bleedCounter--;

					if (!(CTimer::GetFrameCounter() & 3)) {
						CVector cameraDist = GetPosition() - TheCamera.GetPosition();
						if (cameraDist.MagnitudeSqr() < sq(50.0f)) {

							float length = (CGeneral::GetRandomNumber() & 127) * 0.0015f + 0.15f;
							CVector bloodPos(
								((CGeneral::GetRandomNumber() & 127) - 64) * 0.007f,
								((CGeneral::GetRandomNumber() & 127) - 64) * 0.007f,
								1.0f);
							bloodPos += GetPosition();

							CShadows::AddPermanentShadow(SHADOWTYPE_DARK, gpBloodPoolTex, &bloodPos, length, 0.0f,
								0.0f, -length, 255, 255, 0, 0, 4.0f, (CGeneral::GetRandomNumber() & 4095) + 2000, 1.0f);
						}
					}
				}
			}
			ServiceTalking();
			if (bInVehicle && !m_pMyVehicle)
				bInVehicle = false;

			if (m_delayedSoundID >= 0 && CTimer::GetTimeInMilliseconds() > m_delayedSoundTimer) {
				Say(m_delayedSoundID);
				m_delayedSoundID = -1;
				m_delayedSoundTimer = 0;
			}

		} else {
			if (bIsStanding && (!m_pCurrentPhysSurface || IsPlayer())
				|| bIsInWater || !bUsesCollision) {
				SetDead();
			}
			m_pCurrentPhysSurface = nil;
		}
	} else
		ServiceTalking();
}

int32
CPed::ProcessEntityCollision(CEntity *collidingEnt, CColPoint *collidingPoints)
{
	bool collidedWithBoat = false;
	bool belowTorsoCollided = false;
	float gravityEffect = -0.15f * CTimer::GetTimeStep();
	CColPoint intersectionPoint;
	CColLine ourLine;

	CColModel *ourCol = CModelInfo::GetColModel(GetModelIndex());
	CColModel *hisCol = CModelInfo::GetColModel(collidingEnt->GetModelIndex());

	if (!bUsesCollision && !bJustCheckCollision)
		return 0;

	if (collidingEnt->IsVehicle() && ((CVehicle*)collidingEnt)->IsBoat())
		collidedWithBoat = true;

	// ofc we're not vehicle
	if (!m_bIsVehicleBeingShifted && !bSkipLineCol && !collidingEnt->IsPed()) {
		if (!bCollisionProcessed) {
			m_pCurrentPhysSurface = nil;
			if (bIsStanding) {
				bIsStanding = false;
				bWasStanding = true;
			}
			bCollisionProcessed = true;
			m_fCollisionSpeed += m_vecMoveSpeed.Magnitude2D() * CTimer::GetTimeStep();
			bStillOnValidPoly = false;
			if (IsPlayer() || m_fCollisionSpeed >= 1.0f) {
				m_collPoly.valid = false;
				m_fCollisionSpeed = 0.0f;
				bHitSteepSlope = false;
			} else {
				CVector pos = GetPosition();
				float potentialGroundZ = GetPosition().z - FEET_OFFSET;
				if (bWasStanding) {
					pos.z += -0.25f;
					potentialGroundZ += gravityEffect;
				}
				if (CCollision::IsStoredPolyStillValidVerticalLine(pos, potentialGroundZ, intersectionPoint, &m_collPoly)) {
					bStillOnValidPoly = true;
					if(!bHeadStuckInCollision || FEET_OFFSET + intersectionPoint.point.z < GetPosition().z) {
						GetMatrix().GetPosition().z = FEET_OFFSET + intersectionPoint.point.z;
						if (bHeadStuckInCollision)
							bHeadStuckInCollision = false;
					}
					m_vecMoveSpeed.z = 0.0f;
					bIsStanding = true;
				} else {
					m_collPoly.valid = false;
					m_fCollisionSpeed = 0.0f;
					bHitSteepSlope = false;
				}
			}
		}

		if (!bStillOnValidPoly) {
			CVector potentialCenter = GetPosition();
			potentialCenter.z = GetPosition().z - 0.52f;

			// 0.52f should be a ped's approx. radius
			float totalRadiusWhenCollided = collidingEnt->GetBoundRadius() + 0.52f - gravityEffect;
			if (bWasStanding) {
				if (collidedWithBoat) {
					potentialCenter.z += 2.0f * gravityEffect;
					totalRadiusWhenCollided += Abs(gravityEffect);
				} else {
					potentialCenter.z += gravityEffect;
				}
			}
			if (sq(totalRadiusWhenCollided) > (potentialCenter - collidingEnt->GetBoundCentre()).MagnitudeSqr()) {
				ourLine.p0 = GetPosition();
				ourLine.p1 = GetPosition();
				ourLine.p1.z = GetPosition().z - FEET_OFFSET;
				if (bWasStanding) {
					ourLine.p1.z = ourLine.p1.z + gravityEffect;
					ourLine.p0.z = ourLine.p0.z + -0.25f;
				}
				float minDist = 1.0f;
				belowTorsoCollided = CCollision::ProcessVerticalLine(ourLine, collidingEnt->GetMatrix(), *hisCol,
					intersectionPoint, minDist, false, false, &m_collPoly);

				if (collidedWithBoat && bWasStanding && !belowTorsoCollided) {
					ourLine.p0.z = ourLine.p1.z;
					ourLine.p1.z = ourLine.p1.z + gravityEffect;
					belowTorsoCollided = CCollision::ProcessVerticalLine(ourLine, collidingEnt->GetMatrix(), *hisCol,
						intersectionPoint, minDist, false, false, &m_collPoly);
				}
				if (belowTorsoCollided) {
						if (!bIsStanding
							|| FEET_OFFSET + intersectionPoint.point.z > GetPosition().z
							|| collidedWithBoat && 3.12f + intersectionPoint.point.z > GetPosition().z) {

							if (!collidingEnt->IsVehicle() && !collidingEnt->IsObject()) {
								m_pCurSurface = collidingEnt;
								collidingEnt->RegisterReference((CEntity**)&m_pCurSurface);
								bTryingToReachDryLand = false;
								bOnBoat = false;
							} else {
								m_pCurrentPhysSurface = (CPhysical*)collidingEnt;
								collidingEnt->RegisterReference((CEntity**)&m_pCurrentPhysSurface);
								m_vecOffsetFromPhysSurface = intersectionPoint.point - collidingEnt->GetPosition();
								m_pCurSurface = collidingEnt;
								collidingEnt->RegisterReference((CEntity**)&m_pCurSurface);
								m_collPoly.valid = false;
								if (collidingEnt->IsVehicle() && ((CVehicle*)collidingEnt)->IsBoat()) {
									bOnBoat = true;
								} else {
									bOnBoat = false;
								}
							}

							if (!bHeadStuckInCollision || FEET_OFFSET + intersectionPoint.point.z < GetPosition().z) {
								GetMatrix().GetPosition().z = FEET_OFFSET + intersectionPoint.point.z;
								if (bHeadStuckInCollision)
									bHeadStuckInCollision = false;
							}
							m_nSurfaceTouched = intersectionPoint.surfaceB;
							if (m_nSurfaceTouched == SURFACE_STEEP_CLIFF) {
								bHitSteepSlope = true;
								m_vecDamageNormal = intersectionPoint.normal;
							}
						}

						float upperSpeedLimit = 0.33f;
						float lowerSpeedLimit = -0.25f;
						float speed = m_vecMoveSpeed.Magnitude2D();
						if (m_nPedState == PED_IDLE) {
							upperSpeedLimit *= 2.0f;
							lowerSpeedLimit *= 1.5f;
						}
						CAnimBlendAssociation *fallAnim = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_FALL_FALL);
						if (!bWasStanding && ((speed > upperSpeedLimit && !bPushedAlongByCar) || (m_vecMoveSpeed.z < lowerSpeedLimit))
							&& m_pCollidingEntity != collidingEnt) {

							float damage = 100.0f * Max(speed - 0.25f, 0.0f);
							float damage2 = damage;
							if (m_vecMoveSpeed.z < -0.25f)
								damage += (-0.25f - m_vecMoveSpeed.z) * 150.0f;

							uint8 dir = 2; // from backward
							if (m_vecMoveSpeed.x > 0.01f || m_vecMoveSpeed.x < -0.01f || m_vecMoveSpeed.y > 0.01f || m_vecMoveSpeed.y < -0.01f) {
								CVector2D offset = -m_vecMoveSpeed;
								dir = GetLocalDirection(offset);
							}
							if (CSurfaceTable::IsSoftLanding(intersectionPoint.surfaceB))
								damage *= 0.5f;

							InflictDamage(collidingEnt, WEAPONTYPE_FALL, damage, PED_PIECE_TORSO, dir);
							if (IsPlayer() && damage2 > 5.0f)
								Say(SOUND_PED_LAND);

						} else if (!bWasStanding && fallAnim && -0.016f * CTimer::GetTimeStep() > m_vecMoveSpeed.z) {
							InflictDamage(collidingEnt, WEAPONTYPE_FALL, 15.0f, PED_PIECE_TORSO, 2);
						}
						m_vecMoveSpeed.z = 0.0f;
						bIsStanding = true;
				} else {
					bOnBoat = false;
				}
			}
		}
	}

	int ourCollidedSpheres = CCollision::ProcessColModels(GetMatrix(), *ourCol, collidingEnt->GetMatrix(), *hisCol, collidingPoints, nil, nil);
	if (ourCollidedSpheres > 0 || belowTorsoCollided) {
		AddCollisionRecord(collidingEnt);
		if (!collidingEnt->IsBuilding())
			((CPhysical*)collidingEnt)->AddCollisionRecord(this);

		if (ourCollidedSpheres > 0 && (collidingEnt->IsBuilding() || collidingEnt->GetIsStatic())) {
			bHasHitWall = true;
		}
	}
	if (collidingEnt->IsBuilding() || collidingEnt->GetIsStatic()) {
		if (bWasStanding) {
			CVector sphereNormal;
			float normalLength;
			for(int sphere = 0; sphere < ourCollidedSpheres; sphere++) {
				sphereNormal = collidingPoints[sphere].normal;
				if (sphereNormal.z >= -1.0f || !IsPlayer()) {
					normalLength = sphereNormal.Magnitude2D();
					if (normalLength != 0.0f) {
						sphereNormal.x = sphereNormal.x / normalLength;
						sphereNormal.y = sphereNormal.y / normalLength;
					}
				} else {
					float speed = m_vecMoveSpeed.Magnitude2D();
					sphereNormal.x = -m_vecMoveSpeed.x / Max(0.001f, speed);
					sphereNormal.y = -m_vecMoveSpeed.y / Max(0.001f, speed);
					GetMatrix().GetPosition().z -= 0.05f;
					bHeadStuckInCollision = true;
				}
				sphereNormal.Normalise();
				collidingPoints[sphere].normal = sphereNormal;
				if (collidingPoints[sphere].surfaceB == SURFACE_STEEP_CLIFF)
					bHitSteepSlope = true;
			}
		}
	}
	return ourCollidedSpheres;
}

static void
particleProduceFootSplash(CPed *ped, CVector const &pos, float size, int times)
{
	for (int i = 0; i < times; i++) {
		CVector adjustedPos = pos;
		adjustedPos.x += CGeneral::GetRandomNumberInRange(-0.1f, 0.1f);
		adjustedPos.y += CGeneral::GetRandomNumberInRange(-0.1f, 0.1f);

		CVector direction = ped->GetForward() * -0.05f;
		CParticle::AddParticle(PARTICLE_RAIN_SPLASHUP, adjustedPos, direction, nil, size, CRGBA(32, 32, 32, 32), 0, 0, CGeneral::GetRandomNumber() & 1, 200);
	}
}

static void
particleProduceFootDust(CPed *ped, CVector const &pos, float size, int times)
{
	switch (ped->m_nSurfaceTouched)
	{
		case SURFACE_TARMAC:
		case SURFACE_GRAVEL:
		case SURFACE_PAVEMENT:
		case SURFACE_SAND:
		case SURFACE_SAND_BEACH:
		case SURFACE_CONCRETE_BEACH:
			for (int i = 0; i < times; ++i) {
				CVector adjustedPos = pos;
				adjustedPos.x += CGeneral::GetRandomNumberInRange(-0.1f, 0.1f);
				adjustedPos.y += CGeneral::GetRandomNumberInRange(-0.1f, 0.1f);
				// ??
				CGeneral::GetRandomNumber();
				CGeneral::GetRandomNumber();
				CParticle::AddParticle(PARTICLE_PEDFOOT_DUST, adjustedPos, CVector(0.0f, 0.0f, 0.0f), nil, size, CRGBA(0, 0, 0, 0), 0, 0, 0, 0);
			}
			break;
		default:
			break;
	}
}

void
CPed::PlayFootSteps(void)
{
	CAnimBlendAssociation *assoc = RpAnimBlendClumpGetFirstAssociation(GetClump());
	CAnimBlendAssociation *walkRunAssoc = nil;
	float walkRunAssocBlend = 0.0f, idleAssocBlend = 0.0f;

	CVector footPosL(0.0f, 0.0f, 0.0f), footPosR(0.0f, 0.0f, 0.0f);
	bool footPosLok = false, footPosRok = false;

	if (bDoBloodyFootprints) {
		if (m_bloodyFootprintCountOrDeathTime > 0 && m_bloodyFootprintCountOrDeathTime < 300) {
			m_bloodyFootprintCountOrDeathTime--;

			if (m_bloodyFootprintCountOrDeathTime == 0)
				bDoBloodyFootprints = false;
		}
	}

	if (!bIsStanding)
		return;

	for (; assoc; assoc = RpAnimBlendGetNextAssociation(assoc)) {
		if (assoc->flags & ASSOC_WALK) {
			walkRunAssoc = assoc;
			walkRunAssocBlend += assoc->blendAmount;
		} else if ((assoc->flags & ASSOC_NOWALK) == 0) {
			idleAssocBlend += assoc->blendAmount;
		}
	}

	if (walkRunAssoc && walkRunAssocBlend > 0.5f && idleAssocBlend < 1.0f) {

		float stepStart = 1.f / 15.f;
		float stepEnd = walkRunAssoc->hierarchy->totalLength / 2.0f + stepStart;
		float currentTime = walkRunAssoc->currentTime;

		int stepPart = 0;
		
		// This section is shortened/optimized for sanity.

		if (currentTime >= stepStart && currentTime - walkRunAssoc->timeStep < stepStart)
			stepPart = 1;
		else if (currentTime >= stepEnd && currentTime - walkRunAssoc->timeStep < stepEnd)
			stepPart = 2;

		if (stepPart != 0) {
			CVector adjustedFootPos;
			if (stepPart == 1) {
				DMAudio.PlayOneShot(m_audioEntityId, SOUND_STEP_START, 1.0f);
				TransformToNode(footPosL, PED_NODE_LEFT_FOOT);
				footPosLok = true;
				adjustedFootPos = footPosL;
			} else {
				DMAudio.PlayOneShot(m_audioEntityId, SOUND_STEP_END, 1.0f);
				TransformToNode(footPosR, PED_NODE_RIGHT_FOOT);
				footPosRok = true;
				adjustedFootPos = footPosR;
			}

			CVector forward = GetForward();

			adjustedFootPos.z -= 0.1f;
			adjustedFootPos += 0.2f * forward;

			if (bDoBloodyFootprints) {
				CVector2D top(forward * 0.26f);
				CVector2D right(GetRight() * (stepPart == 1 ? 0.14f : 0.1f));

				CShadows::AddPermanentShadow(SHADOWTYPE_DARK, gpBloodPoolTex, &adjustedFootPos,
					top.x, top.y,
					right.x, right.y,
					255, 255, 0, 0, 4.0f, 3000, 1.0f);

				if (m_bloodyFootprintCountOrDeathTime <= 20) {
					m_bloodyFootprintCountOrDeathTime = 0;
					bDoBloodyFootprints = false;
				} else {
					m_bloodyFootprintCountOrDeathTime -= 20;
				}
			}
			if (m_nSurfaceTouched == SURFACE_SAND || m_nSurfaceTouched == SURFACE_SAND_BEACH) {
				CVector2D top(forward * -0.26f);
				CVector2D right(GetRight() * (stepPart == 1 ? 0.1f : 0.14f));

				CShadows::AddPermanentShadow(SHADOWTYPE_DARK, gpShadowPedTex, &adjustedFootPos,
					top.x, top.y,
					right.x, right.y,
					120, 250, 250, 50, 4.0f, 5000, 1.0f);
			}
			if (CWeather::Rain <= 0.1f || CCullZones::CamNoRain() || CCullZones::PlayerNoRain()) {
				if (IsPlayer())
					particleProduceFootDust(this, adjustedFootPos, 0.0f, 4);

			} else if (stepPart == 2) {
				particleProduceFootSplash(this, adjustedFootPos, 0.15f, 4);
			}
		}
	}

	if (IsPlayer() && !walkRunAssoc && bIsLanding) {
		if (!footPosLok)
			TransformToNode(footPosL, PED_NODE_LEFT_FOOT);

		CVector forward = GetForward();

		CVector adjustedFootPosL = footPosL;
		adjustedFootPosL.z -= 0.1f;
		adjustedFootPosL += 0.2f * forward;
		if (bDoBloodyFootprints) {
			CVector2D top(forward * 0.26f);
			CVector2D right(GetRight() * 0.14f);

			CShadows::AddPermanentShadow(SHADOWTYPE_DARK, gpBloodPoolTex, &adjustedFootPosL,
				top.x, top.y,
				right.x, right.y,
				255, 255, 0, 0, 4.0f, 3000, 1.0f);

			if (m_bloodyFootprintCountOrDeathTime <= 20) {
				m_bloodyFootprintCountOrDeathTime = 0;
				bDoBloodyFootprints = false;
			}
			else {
				m_bloodyFootprintCountOrDeathTime -= 20;
			}
		}
		if (m_nSurfaceTouched == SURFACE_SAND || m_nSurfaceTouched == SURFACE_SAND_BEACH) {
			CVector2D top(forward * -0.26f);
			CVector2D right(GetRight() * 0.14f);

			CShadows::AddPermanentShadow(SHADOWTYPE_DARK, gpShadowPedTex, &adjustedFootPosL,
				top.x, top.y,
				right.x, right.y,
				120, 250, 250, 50, 4.0f, 5000, 1.0f);
		}
		if(!footPosRok)
			TransformToNode(footPosR, PED_NODE_RIGHT_FOOT);

		CVector adjustedFootPosR = footPosR;
		adjustedFootPosR.z -= 0.1f;
		adjustedFootPosR += 0.2f * forward;

		if (bDoBloodyFootprints) {
			CVector2D top(forward * 0.26f);
			CVector2D right(GetRight() * 0.1f);

			CShadows::AddPermanentShadow(SHADOWTYPE_DARK, gpBloodPoolTex, &adjustedFootPosR,
				top.x, top.y,
				right.x, right.y,
				255, 255, 0, 0, 4.0f, 3000, 1.0f);

			if (m_bloodyFootprintCountOrDeathTime <= 20) {
				m_bloodyFootprintCountOrDeathTime = 0;
				bDoBloodyFootprints = false;
			} else {
				m_bloodyFootprintCountOrDeathTime -= 20;
			}
		}
		if (m_nSurfaceTouched == SURFACE_SAND || m_nSurfaceTouched == SURFACE_SAND_BEACH) {
			CVector2D top(forward * -0.26f);
			CVector2D right(GetRight() * 0.14f);

			CShadows::AddPermanentShadow(SHADOWTYPE_DARK, gpShadowPedTex, &adjustedFootPosR,
				top.x, top.y,
				right.x, right.y,
				120, 250, 250, 50, 4.0f, 5000, 1.0f);
		}
	}

	if (m_nSurfaceTouched == SURFACE_WATER) {
		CRGBA rubberSmokeColor(255, 255, 255, 196);
		float pedSpeed = CVector2D(m_vecMoveSpeed).Magnitude();
		if (pedSpeed > 0.03f && CTimer::GetFrameCounter() % 2 == 0 && pedSpeed > 0.13f) {
			float particleSize = pedSpeed * 2.0f;

			if (particleSize < 0.25f)
				particleSize = 0.25f;

			if (particleSize > 0.75f)
				particleSize = 0.75f;

			CVector particlePos = GetPosition() + GetForward() * 0.3f;
			particlePos.z -= 1.2f;

			CVector particleDir = m_vecMoveSpeed * -0.75f;

			particleDir.z = CGeneral::GetRandomNumberInRange(0.01f, 0.03f);
			CParticle::AddParticle(PARTICLE_CAR_SPLASH, particlePos, particleDir, nil, 0.5f * particleSize, CRGBA(0,0,0,0), 0, 0, 0, 0);
			CParticle::AddParticle(PARTICLE_RUBBER_SMOKE, particlePos, particleDir, nil, particleSize, rubberSmokeColor, 0, 0, 0, 0);
		}

		if (m_nPedState == PED_JUMP) {
			CVector particlePos = GetPosition();
			particlePos.z -= 0.1f;

			CVector particleDir(0.0f, 0.075f, 0.0f);
			CParticle::AddParticle(PARTICLE_CAR_SPLASH, particlePos, particleDir, nil, 0.005f, CRGBA(0, 0, 0, 0), 0, 0, 0, 0);
			particleDir.x += CGeneral::GetRandomNumberInRange(-0.05f, 0.05f);
			particleDir.y += CGeneral::GetRandomNumberInRange(-0.05f, 0.05f);
			particleDir.z -= CGeneral::GetRandomNumberInRange(0.025f, 0.05f);
			CParticle::AddParticle(PARTICLE_RUBBER_SMOKE, particlePos, particleDir, nil, 0.5f, rubberSmokeColor, 0, 0, 0, 0);
		}
	}
}

// Actually GetLocalDirectionTo(Turn/Look)
int
CPed::GetLocalDirection(const CVector2D &posOffset)
{
	int direction;
	float angle;

	for (angle = posOffset.Heading() - m_fRotationCur + DEGTORAD(45.0f); angle < 0.0f; angle += TWOPI);

	for (direction = RADTODEG(angle)/90.0f; direction > 3; direction -= 4);

	// 0-forward, 1-left, 2-backward, 3-right.
	return direction;
}

void
CPed::SetDirectionToWalkAroundObject(CEntity *obj)
{
	float distLimitForTimer = 8.0f;
	CColModel *objCol = CModelInfo::GetColModel(obj->GetModelIndex());
	CVector objColMin = objCol->boundingBox.min;
	CVector objColMax = objCol->boundingBox.max;
	CVector objColCenter = (objColMin + objColMax) / 2.0f;
	CMatrix objMat(obj->GetMatrix());
	float dirToSet = obj->GetForward().Heading();
	bool goingToEnterCarAndItsVan = false;
	bool goingToEnterCar = false;
	bool objUpsideDown = false;

	float checkIntervalInDist = (objColMax.y - objColMin.y) * 0.1f;
	float checkIntervalInTime;

	if (m_nMoveState == PEDMOVE_NONE || m_nMoveState == PEDMOVE_STILL)
		return;

	CVector2D adjustedColMin(objColMin.x - 0.35f, objColMin.y - 0.35f);
	CVector2D adjustedColMax(objColMax.x + 0.35f, objColMax.y + 0.35f);

	checkIntervalInDist = Max(checkIntervalInDist, 0.5f);
	checkIntervalInDist = Min(checkIntervalInDist, (objColMax.z - objColMin.z) / 2.0f);
	checkIntervalInDist = Min(checkIntervalInDist, (adjustedColMax.x - adjustedColMin.x) / 2.0f);

	if (objMat.GetUp().z < 0.0f)
		objUpsideDown = true;

	if (obj->GetModelIndex() != MI_TRAFFICLIGHTS && obj->GetModelIndex() != MI_SINGLESTREETLIGHTS1 && obj->GetModelIndex() != MI_SINGLESTREETLIGHTS2) {
		objColCenter = obj->GetMatrix() * objColCenter;
	} else {
		checkIntervalInDist = 0.4f;
		if (objMat.GetUp().z <= 0.57f) {

			// Specific calculations for traffic lights, didn't get a bit.
			adjustedColMin.x = 1.2f * (adjustedColMin.x < adjustedColMin.y ? adjustedColMin.x : adjustedColMin.y);
			adjustedColMax.x = 1.2f * (adjustedColMax.x > adjustedColMax.y ? adjustedColMax.x : adjustedColMax.y);
			adjustedColMin.y = 1.2f * objColMin.z;
			adjustedColMax.y = 1.2f * objColMax.z;
			dirToSet = objMat.GetUp().Heading();
			objMat.SetUnity();
			objMat.RotateZ(dirToSet);
			objMat.GetPosition() += obj->GetPosition();
			objColCenter = obj->GetPosition();
		} else {
			objColCenter.x = adjustedColMax.x - 0.25f;
			objColCenter = obj->GetMatrix() * objColCenter;
			distLimitForTimer = 0.75f;
		}
		objUpsideDown = false;
	}
	float oldRotDest = m_fRotationDest;
	float angleToFaceObjCenter = (objColCenter - GetPosition()).Heading();
	float angleDiffBtwObjCenterAndForward = CGeneral::LimitRadianAngle(dirToSet - angleToFaceObjCenter);
	float objTopRightHeading = Atan2(adjustedColMax.x - adjustedColMin.x, adjustedColMax.y - adjustedColMin.y);

	if (IsPlayer()) {
		if (FindPlayerPed()->MoveSpeed() <= 0.0f)
			checkIntervalInTime = 0.0f;
		else
			checkIntervalInTime = 2.0f / FindPlayerPed()->MoveSpeed();
	} else {
		switch (m_nMoveState) {
			case PEDMOVE_WALK:
				checkIntervalInTime = 2.0f;
				break;
			case PEDMOVE_RUN:
				checkIntervalInTime = 0.5f;
				break;
			case PEDMOVE_SPRINT:
				checkIntervalInTime = 0.5f;
				break;
			default:
				checkIntervalInTime = 0.0f;
				break;
		}
	}
	if (m_pSeekTarget == obj && obj->IsVehicle()) {
		if (m_objective == OBJECTIVE_ENTER_CAR_AS_DRIVER || m_objective == OBJECTIVE_ENTER_CAR_AS_PASSENGER) {
			goingToEnterCar = true;
			if (IsPlayer())
				checkIntervalInTime = 0.0f;

			if (((CVehicle*)obj)->bIsVan)
				goingToEnterCarAndItsVan = true;
		}
	}

	int entityOnTopLeftOfObj = 0;
	int entityOnBottomLeftOfObj = 0;
	int entityOnTopRightOfObj = 0;
	int entityOnBottomRightOfObj = 0;

	if (CTimer::GetTimeInMilliseconds() > m_walkAroundObstacleTimer || m_pWalkAroundObstacle != obj) {
		bool collidingThingChanged = true;
		CEntity *obstacle;

		if (!obj->IsVehicle() || objUpsideDown) {
			collidingThingChanged = false;
		} else {
			float adjustedCheckInterval = 0.7f * checkIntervalInDist;
			CVector posToCheck;

			// Top left of obj
			posToCheck.x = adjustedColMin.x + adjustedCheckInterval;
			posToCheck.y = adjustedColMax.y - adjustedCheckInterval;
			posToCheck.z = 0.0f;
			posToCheck = objMat * posToCheck;
			posToCheck.z += 0.6f;
			obstacle = CWorld::TestSphereAgainstWorld(posToCheck, checkIntervalInDist, obj,
				true, true, false, true, false, false);
			if (obstacle) {
				if (obstacle->IsBuilding()) {
					entityOnTopLeftOfObj = 1;
				} else if (obstacle->IsVehicle()) {
					entityOnTopLeftOfObj = 2;
				} else {
					entityOnTopLeftOfObj = 3;
				}
			}

			// Top right of obj
			posToCheck.x = adjustedColMax.x - adjustedCheckInterval;
			posToCheck.y = adjustedColMax.y - adjustedCheckInterval;
			posToCheck.z = 0.0f;
			posToCheck = objMat * posToCheck;
			posToCheck.z += 0.6f;
			obstacle = CWorld::TestSphereAgainstWorld(posToCheck, checkIntervalInDist, obj,
				true, true, false, true, false, false);
			if (obstacle) {
				if (obstacle->IsBuilding()) {
					entityOnTopRightOfObj = 1;
				} else if (obstacle->IsVehicle()) {
					entityOnTopRightOfObj = 2;
				} else {
					entityOnTopRightOfObj = 3;
				}
			}

			// Bottom right of obj
			posToCheck.x = adjustedColMax.x - adjustedCheckInterval;
			posToCheck.y = adjustedColMin.y + adjustedCheckInterval;
			posToCheck.z = 0.0f;
			posToCheck = objMat * posToCheck;
			posToCheck.z += 0.6f;
			obstacle = CWorld::TestSphereAgainstWorld(posToCheck, checkIntervalInDist, obj,
				true, true, false, true, false, false);
			if (obstacle) {
				if (obstacle->IsBuilding()) {
					entityOnBottomRightOfObj = 1;
				} else if (obstacle->IsVehicle()) {
					entityOnBottomRightOfObj = 2;
				} else {
					entityOnBottomRightOfObj = 3;
				}
			}

			// Bottom left of obj
			posToCheck.x = adjustedColMin.x + adjustedCheckInterval;
			posToCheck.y = adjustedColMin.y + adjustedCheckInterval;
			posToCheck.z = 0.0f;
			posToCheck = objMat * posToCheck;
			posToCheck.z += 0.6f;
			obstacle = CWorld::TestSphereAgainstWorld(posToCheck, checkIntervalInDist, obj,
				true, true, false, true, false, false);
			if (obstacle) {
				if (obstacle->IsBuilding()) {
					entityOnBottomLeftOfObj = 1;
				} else if (obstacle->IsVehicle()) {
					entityOnBottomLeftOfObj = 2;
				} else {
					entityOnBottomLeftOfObj = 3;
				}
			}
		}

		if (!entityOnTopLeftOfObj && !entityOnTopRightOfObj) {
			CVector topLeftCorner(adjustedColMin.x - 0.3f, adjustedColMax.y + 0.3f, 0.0f);
			topLeftCorner = objMat * topLeftCorner;
			CVector topRightCorner(adjustedColMax.x + 0.3f, adjustedColMax.y + 0.3f, 0.0f);
			topRightCorner = objMat * topRightCorner;
			CColPoint foundCol;
			CEntity *foundEnt;
			if (CWorld::ProcessLineOfSight(topLeftCorner, topRightCorner, foundCol, foundEnt, true, true, false, true, false, false, false, false)) {
				switch (foundEnt->GetType()) {
				case ENTITY_TYPE_VEHICLE:
					entityOnTopRightOfObj = 2;
					entityOnTopLeftOfObj = 2;
					break;
				case ENTITY_TYPE_BUILDING:
					entityOnTopRightOfObj = 1;
					entityOnTopLeftOfObj = 1;
					break;
				case ENTITY_TYPE_OBJECT:
					entityOnTopRightOfObj = 3;
					entityOnTopLeftOfObj = 3;
					break;
				}
			}
		}
		if (!entityOnBottomRightOfObj && !entityOnBottomLeftOfObj) {
			CVector bottomRightCorner(adjustedColMax.x + 0.3f, adjustedColMin.y - 0.3f, 0.0f);
			bottomRightCorner = objMat * bottomRightCorner;
			CVector bottomLeftCorner(adjustedColMin.x - 0.3f, adjustedColMin.y - 0.3f, 0.0f);
			bottomLeftCorner = objMat * bottomLeftCorner;
			CColPoint foundCol;
			CEntity* foundEnt;
			if (CWorld::ProcessLineOfSight(bottomRightCorner, bottomLeftCorner, foundCol, foundEnt, true, true, false, true, false, false, false, false)) {
				switch (foundEnt->GetType()) {
				case ENTITY_TYPE_VEHICLE:
					entityOnBottomLeftOfObj = 2;
					entityOnBottomRightOfObj = 2;
					break;
				case ENTITY_TYPE_BUILDING:
					entityOnBottomLeftOfObj = 1;
					entityOnBottomRightOfObj = 1;
					break;
				case ENTITY_TYPE_OBJECT:
					entityOnBottomLeftOfObj = 3;
					entityOnBottomRightOfObj = 3;
					break;
				}
			}
		}

		if (entityOnTopLeftOfObj && entityOnTopRightOfObj && entityOnBottomRightOfObj && entityOnBottomLeftOfObj) {
			collidingThingChanged = false;
			entityOnTopLeftOfObj = 0;
			entityOnBottomLeftOfObj = 0;
			entityOnTopRightOfObj = 0;
			entityOnBottomRightOfObj = 0;
		}

		if (!collidingThingChanged) {
			m_walkAroundType = 0;
		} else {
			if (Abs(angleDiffBtwObjCenterAndForward) >= objTopRightHeading) {
				if (PI - objTopRightHeading >= Abs(angleDiffBtwObjCenterAndForward)) {
					if ((angleDiffBtwObjCenterAndForward <= 0.0f || objUpsideDown) && (angleDiffBtwObjCenterAndForward < 0.0f || !objUpsideDown)) {
						if (goingToEnterCar && (m_vehDoor == CAR_DOOR_RF || m_vehDoor == CAR_DOOR_RR)) {
							m_walkAroundType = 0;
						} else {
							if (CGeneral::LimitRadianAngle(m_fRotationDest - angleToFaceObjCenter) >= 0.0f) {
								if (entityOnBottomRightOfObj == 1 || entityOnBottomRightOfObj && !entityOnTopLeftOfObj && !entityOnTopRightOfObj) {
									m_walkAroundType = 1;
								} else if (entityOnBottomLeftOfObj == 1 || entityOnBottomLeftOfObj && !entityOnTopLeftOfObj && !entityOnTopRightOfObj) {
									m_walkAroundType = 1;
								}
							} else {
								if (entityOnTopRightOfObj == 1 || entityOnTopRightOfObj && !entityOnBottomRightOfObj && !entityOnBottomLeftOfObj) {
									m_walkAroundType = 4;
								} else if (entityOnTopLeftOfObj == 1 || entityOnTopLeftOfObj && !entityOnBottomRightOfObj && !entityOnBottomLeftOfObj) {
									m_walkAroundType = 4;
								}
							}
						}
					} else {
						if (goingToEnterCar && (m_vehDoor == CAR_DOOR_LF || m_vehDoor == CAR_DOOR_LR)) {
							m_walkAroundType = 0;
						} else {
							if (CGeneral::LimitRadianAngle(m_fRotationDest - angleToFaceObjCenter) <= 0.0f) {
								if (entityOnBottomLeftOfObj == 1 || entityOnBottomLeftOfObj && !entityOnTopLeftOfObj && !entityOnTopRightOfObj) {
									m_walkAroundType = 2;
								} else if (entityOnBottomRightOfObj == 1 || entityOnBottomRightOfObj && !entityOnTopLeftOfObj && !entityOnTopRightOfObj) {
									m_walkAroundType = 2;
								}
							} else {
								if (entityOnTopLeftOfObj == 1 || entityOnTopLeftOfObj && !entityOnBottomRightOfObj && !entityOnBottomLeftOfObj) {
									m_walkAroundType = 3;
								} else if (entityOnTopRightOfObj == 1 || entityOnTopRightOfObj && !entityOnBottomRightOfObj && !entityOnBottomLeftOfObj) {
									m_walkAroundType = 3;
								}
							}
						}
					}
				} else if (goingToEnterCar && (m_vehDoor == CAR_DOOR_LF || m_vehDoor == CAR_DOOR_LR)
					|| CGeneral::LimitRadianAngle(m_fRotationDest - angleToFaceObjCenter) < 0.0f) {
					if (entityOnTopLeftOfObj == 1 || entityOnTopLeftOfObj && !entityOnTopRightOfObj && !entityOnBottomRightOfObj) {
						m_walkAroundType = 3;
					}
				} else if (entityOnTopRightOfObj == 1 || entityOnTopRightOfObj && !entityOnTopLeftOfObj && !entityOnBottomLeftOfObj) {
					m_walkAroundType = 4;
				}
			} else if (goingToEnterCar && (m_vehDoor == CAR_DOOR_LF || m_vehDoor == CAR_DOOR_LR)
				|| CGeneral::LimitRadianAngle(m_fRotationDest - angleToFaceObjCenter) > 0.0f) {
				if (entityOnBottomLeftOfObj == 1 || entityOnBottomLeftOfObj && !entityOnTopRightOfObj && !entityOnBottomRightOfObj) {
					m_walkAroundType = 2;
				}
			} else if (entityOnBottomRightOfObj == 1 || entityOnBottomRightOfObj && !entityOnTopLeftOfObj && !entityOnBottomLeftOfObj) {
				m_walkAroundType = 1;
			} else {
				m_walkAroundType = 0;
			}
		}
	}
	m_pWalkAroundObstacle = obj;
	m_pWalkAroundObstacle->RegisterReference((CEntity**) &m_pWalkAroundObstacle);

	// TODO: This random may need to be changed.
	m_walkAroundObstacleTimer = CTimer::GetTimeInMilliseconds() + 512 + CGeneral::GetRandomNumber();

	CVector localPosToHead;

	if (Abs(angleDiffBtwObjCenterAndForward) < objTopRightHeading) {
		if (goingToEnterCar) {
			if (goingToEnterCarAndItsVan) {
				if (m_vehDoor == CAR_DOOR_LR || m_vehDoor == CAR_DOOR_RR)
					return;
			}
			if (m_vehDoor != CAR_DOOR_LF && m_vehDoor != CAR_DOOR_LR && (!entityOnBottomRightOfObj || entityOnBottomLeftOfObj)) {
				m_fRotationDest = CGeneral::LimitRadianAngle(dirToSet - HALFPI);
				localPosToHead.x = adjustedColMax.x;
				localPosToHead.z = 0.0f;
				localPosToHead.y = adjustedColMin.y;
			} else {
				m_fRotationDest = CGeneral::LimitRadianAngle(HALFPI + dirToSet);
				localPosToHead.x = adjustedColMin.x;
				localPosToHead.z = 0.0f;
				localPosToHead.y = adjustedColMin.y;
			}
		} else {
			if (m_walkAroundType != 1 && m_walkAroundType != 4
				&& (m_walkAroundType || CGeneral::LimitRadianAngle(m_fRotationDest - angleToFaceObjCenter) <= 0.0f)) {

				m_fRotationDest = CGeneral::LimitRadianAngle(dirToSet - HALFPI);
				localPosToHead.x = adjustedColMax.x;
				localPosToHead.z = 0.0f;
				localPosToHead.y = adjustedColMin.y;
			} else {
				m_fRotationDest = CGeneral::LimitRadianAngle(HALFPI + dirToSet);
				localPosToHead.x = adjustedColMin.x;
				localPosToHead.z = 0.0f;
				localPosToHead.y = adjustedColMin.y;
			}
		}
	} else {
		if (PI - objTopRightHeading >= Abs(angleDiffBtwObjCenterAndForward)) {
			if (angleDiffBtwObjCenterAndForward <= 0.0f) {
				if (!goingToEnterCar || !goingToEnterCarAndItsVan || m_vehDoor != CAR_DOOR_LR && m_vehDoor != CAR_DOOR_RR) {
					if (goingToEnterCar) {
						if (m_vehDoor == CAR_DOOR_RF || (m_vehDoor == CAR_DOOR_RR && !goingToEnterCarAndItsVan))
							return;
					}
					if (m_walkAroundType == 4 || m_walkAroundType == 3
						|| !m_walkAroundType && CGeneral::LimitRadianAngle(m_fRotationDest - angleToFaceObjCenter) > 0.0f) {

						m_fRotationDest = CGeneral::LimitRadianAngle(PI + dirToSet);
						localPosToHead.x = adjustedColMax.x;
						localPosToHead.z = 0.0f;
						localPosToHead.y = adjustedColMin.y;
					} else {
						m_fRotationDest = dirToSet;
						localPosToHead.x = adjustedColMax.x;
						localPosToHead.z = 0.0f;
						localPosToHead.y = adjustedColMax.y;
					}
				} else {
					m_fRotationDest = CGeneral::LimitRadianAngle(PI + dirToSet);
					localPosToHead.x = adjustedColMax.x;
					localPosToHead.z = 0.0f;
					localPosToHead.y = adjustedColMin.y;
				}
			} else if (goingToEnterCar && goingToEnterCarAndItsVan && (m_vehDoor == CAR_DOOR_LR || m_vehDoor == CAR_DOOR_RR)) {
				m_fRotationDest = CGeneral::LimitRadianAngle(PI + dirToSet);
				localPosToHead.x = adjustedColMin.x;
				localPosToHead.z = 0.0f;
				localPosToHead.y = adjustedColMin.y;
			} else {
				if (goingToEnterCar) {
					if (m_vehDoor == CAR_DOOR_LF || m_vehDoor == CAR_DOOR_LR && !goingToEnterCarAndItsVan)
						return;
				}
				if (m_walkAroundType == 1 || m_walkAroundType == 2
					|| !m_walkAroundType && CGeneral::LimitRadianAngle(m_fRotationDest - angleToFaceObjCenter) > 0.0f) {

					m_fRotationDest = dirToSet;
					localPosToHead.x = adjustedColMin.x;
					localPosToHead.z = 0.0f;
					localPosToHead.y = adjustedColMax.y;
				} else {
					m_fRotationDest = CGeneral::LimitRadianAngle(PI + dirToSet);
					localPosToHead.x = adjustedColMin.x;
					localPosToHead.z = 0.0f;
					localPosToHead.y = adjustedColMin.y;
				}
			}
		} else {
			if (goingToEnterCar && (!goingToEnterCarAndItsVan || m_vehDoor != CAR_DOOR_LR && m_vehDoor != CAR_DOOR_RR)) {
				if (m_vehDoor != CAR_DOOR_LF && m_vehDoor != CAR_DOOR_LR && (!entityOnTopRightOfObj || entityOnTopLeftOfObj)) {

					m_fRotationDest = CGeneral::LimitRadianAngle(dirToSet - HALFPI);
					localPosToHead.x = adjustedColMax.x;
					localPosToHead.z = 0.0f;
					localPosToHead.y = adjustedColMax.y;
				} else {
					m_fRotationDest = CGeneral::LimitRadianAngle(HALFPI + dirToSet);
					localPosToHead.x = adjustedColMin.x;
					localPosToHead.z = 0.0f;
					localPosToHead.y = adjustedColMax.y;
				}
			} else {
				if (m_walkAroundType == 2 || m_walkAroundType == 3
					|| !m_walkAroundType && CGeneral::LimitRadianAngle(m_fRotationDest - angleToFaceObjCenter) > 0.0f) {

					m_fRotationDest = CGeneral::LimitRadianAngle(dirToSet - HALFPI);
					localPosToHead.x = adjustedColMax.x;
					localPosToHead.z = 0.0f;
					localPosToHead.y = adjustedColMax.y;
				} else {
					m_fRotationDest = CGeneral::LimitRadianAngle(HALFPI + dirToSet);
					localPosToHead.x = adjustedColMin.x;
					localPosToHead.z = 0.0f;
					localPosToHead.y = adjustedColMax.y;
				}
			}
		}
	}
	if (objUpsideDown)
		localPosToHead.x = localPosToHead.x * -1.0f;

	localPosToHead = objMat * localPosToHead;
	m_actionX = localPosToHead.x;
	m_actionY = localPosToHead.y;
	localPosToHead -= GetPosition();
	m_fRotationDest = CGeneral::LimitRadianAngle(localPosToHead.Heading());

	if (m_fRotationDest != m_fRotationCur && bHitSomethingLastFrame) {
		if (m_fRotationDest == oldRotDest) {
			m_fRotationDest = oldRotDest;
		} else {
			m_fRotationDest = CGeneral::LimitRadianAngle(PI + dirToSet);
		}
	}

	float dist = localPosToHead.Magnitude2D();
	if (dist < 0.5f)
		dist = 0.5f;

	if (dist > distLimitForTimer)
		dist = distLimitForTimer;
	m_nPedStateTimer = CTimer::GetTimeInMilliseconds() + 280.0f * dist * checkIntervalInTime;
}

bool
CPed::IsPedInControl(void)
{
	return m_nPedState <= PED_STATES_NO_AI
		&& !bIsInTheAir && !bIsLanding
		&& m_fHealth > 0.0f;
}

bool
CPed::IsPedShootable(void)
{
	return m_nPedState <= PED_STATES_CAN_SHOOT;
}

bool
CPed::UseGroundColModel(void)
{
	return m_nPedState == PED_FALL ||
		m_nPedState == PED_EVADE_DIVE ||
		m_nPedState == PED_DIE ||
		m_nPedState == PED_DEAD;
}

bool
CPed::CanPedReturnToState(void)
{
	// SA (gta-reversed CPed::CanPedReturnToState) asks for an explicit list of
	// states, not for "anything below PED_STATES_NO_AI minus a few". The
	// difference is not academic: the range test also accepted PED_ANSWER_MOBILE,
	// PED_JOG and PED_HANG_OUT, so a ped that was answering a phone or jogging
	// counted as returnable and RestorePreviousState could cut those states
	// short.
	switch(m_nPedState){
	case PED_NONE:
	case PED_IDLE:
	case PED_LOOK_HEADING:
	case PED_WANDER_RANGE:
	case PED_WANDER_PATH:
	case PED_SEEK_POSITION:
	case PED_SEEK_ENTITY:
	case PED_FLEE_POSITION:
	case PED_FLEE_ENTITY:
	case PED_PURSUE:
	case PED_FOLLOW_PATH:
	case PED_ROCKETLAUNCHER_MODE:
	case PED_DUMMY:
	case PED_PAUSE:
	case PED_CHAT:
	case PED_MUG:
	case PED_AI_CONTROL:
	case PED_SEEK_CAR:
	case PED_SEEK_BOAT_POSITION:
	case PED_FOLLOW_ROUTE:
	case PED_CPR:
	case PED_SOLICIT:
	case PED_BUY_ICE_CREAM:
	case PED_ON_FIRE:
	case PED_SUNBATHE:
	case PED_FLASH:
		return true;
	}
	return false;
}

bool
CPed::CanSetPedState(void)
{
	return !DyingOrDead() && m_nPedState != PED_ARRESTED && !EnteringCar() && m_nPedState != PED_STEAL_CAR;
}

bool
CPed::CanStrafeOrMouseControl(void)
{
#ifdef FREE_CAM
	if (CCamera::bFreeCam)
		return false;
#endif
	return m_nPedState == PED_NONE || m_nPedState == PED_IDLE || m_nPedState == PED_FLEE_POSITION || m_nPedState == PED_FLEE_ENTITY ||
		m_nPedState == PED_ATTACK || m_nPedState == PED_FIGHT || m_nPedState == PED_AIMGUN || m_nPedState == PED_JUMP || m_nPedState == PED_ANSWER_MOBILE;
}

void
CPed::PedSetPreviousStateCB(CAnimBlendAssociation* assoc, void* arg)
{
	CPed* ped = (CPed*)arg;
	ped->RestorePreviousState();
	ped->m_pVehicleAnim = nil;
}

void
CPed::PedGetupCB(CAnimBlendAssociation* animAssoc, void* arg)
{
	CPed* ped = (CPed*)arg;

	if (ped->m_nPedState == PED_GETUP)
		RpAnimBlendClumpSetBlendDeltas(ped->GetClump(), ASSOC_PARTIAL, -1000.0f);

	ped->bFallenDown = false;
	animAssoc->blendDelta = -1000.0f;
	if (ped->m_nPedState == PED_GETUP)
		ped->RestorePreviousState();

	ped->SetMoveState(PEDMOVE_STILL);
	ped->SetMoveAnim();

	ped->bGetUpAnimStarted = false;
}

void
CPed::PedLandCB(CAnimBlendAssociation* animAssoc, void* arg)
{
	CPed* ped = (CPed*)arg;

	animAssoc->blendDelta = -1000.0f;
	ped->bIsLanding = false;

	if (ped->m_nPedState == PED_JUMP)
		ped->RestorePreviousState();
}

void
CPed::PedStaggerCB(CAnimBlendAssociation* animAssoc, void* arg)
{
	/*
	CPed *ped = (CPed*)arg;

	if (ped->m_nPedState == PED_STAGGER)
		// nothing
	*/
}
void
CPed::PedSetOutCarCB(CAnimBlendAssociation *animAssoc, void *arg)
{
	CPed *ped = (CPed*)arg;

	CVehicle *veh = ped->m_pMyVehicle;

	ped->bUsesCollision = true;
	ped->m_actionX = 0.0f;
	ped->m_actionY = 0.0f;
	ped->bVehExitWillBeInstant = false;
	if (veh && veh->IsBoat())
		ped->ApplyMoveSpeed();

	if (ped->m_objective == OBJECTIVE_LEAVE_CAR)
		ped->RestorePreviousObjective();
	else if (ped->m_objective == OBJECTIVE_LEAVE_CAR_AND_DIE) {
		ped->m_fHealth = 0.0f;
		ped->SetDie(ANIM_ID_FLOOR_HIT, 4.0f, 0.5f);
	}

	ped->bInVehicle = false;
	if (veh && (veh->IsAutomobile() || veh->IsBike())) {
		CWorld::pIgnoreEntity = veh;
		if (CWorld::TestSphereAgainstWorld(ped->GetPosition() - CVector(0.f, 0.f, 0.2f),
				0.4f, veh, true, true, false, false, false, false)
			|| CWorld::TestSphereAgainstWorld(ped->GetPosition() + CVector(0.f, 0.f, 0.2f),
				0.4f, veh, true, true, false, false, false, false)
			|| !CWorld::GetIsLineOfSightClear(veh->GetPosition(), ped->GetPosition(), true, false, false, true, false, false, false)) {
			CWorld::pIgnoreEntity = nil;
			ped->PositionPedOutOfCollision();
		}
		CWorld::pIgnoreEntity = nil;
	}

	if (ped->m_nPedState == PED_EXIT_CAR) {
		ped->RestorePreviousState();
		veh = ped->m_pMyVehicle;
	} else if (ped->m_nPedState == PED_DRIVING) {
		ped->m_nPedState = PED_IDLE;
	}

	if (animAssoc)
		animAssoc->blendDelta = -1000.0f;

	ped->RestartNonPartialAnims();
	ped->m_pVehicleAnim = nil;
	ped->m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f);
	veh = ped->m_pMyVehicle;
	if (veh) {
		if (veh && veh->IsBike())
			// BUG? 
			veh->m_nGettingOutFlags &= ~GetBikeDoorFlag(ped->m_vehDoor);
		else
			veh->m_nGettingOutFlags &= ~GetCarDoorFlag(ped->m_vehDoor);

		if (veh->pDriver == ped) {
			veh->RemoveDriver();
#ifndef FIX_BUGS // RemoveDriver does it anyway
			veh->SetStatus(STATUS_ABANDONED);
#endif
			if (veh->m_nDoorLock == CARLOCK_LOCKED_INITIALLY)
				veh->m_nDoorLock = CARLOCK_UNLOCKED;
			if (veh->IsBike()) {
				if (Abs(veh->m_vecMoveSpeed.x) < 0.1 && Abs(veh->m_vecMoveSpeed.y) < 0.1f) {
					((CBike*)veh)->bIsStanding = true;
				}
			}
		} else {
			veh->RemovePassenger(ped);
		}

		if (veh->bIsBus && !veh->IsUpsideDown() && !veh->IsOnItsSide()) {
			float angleAfterExit;
			if (ped->m_vehDoor == CAR_DOOR_LF) {
				angleAfterExit = HALFPI + veh->GetForward().Heading();
			} else {
				angleAfterExit = veh->GetForward().Heading() - HALFPI;
			}
			ped->SetHeading(angleAfterExit);
			ped->m_fRotationDest = angleAfterExit;
			ped->m_fRotationCur = angleAfterExit;
			ped->SetMoveState(PEDMOVE_WALK);
		}
	}

	if (ped->IsPlayer())
		AudioManager.PlayerJustLeftCar();

	ped->ReplaceWeaponWhenExitingVehicle();

	ped->bOnBoat = false;
	ped->m_nStoredMoveState = PEDMOVE_NONE;
}

void
CPed::PedSetDraggedOutCarCB(CAnimBlendAssociation *dragAssoc, void *arg)
{
	CAnimBlendAssociation *quickJackedAssoc;
	CVehicle *vehicle; 
	CPed *ped = (CPed*)arg;

	uint8 exitFlags = 0;
	quickJackedAssoc = RpAnimBlendClumpGetAssociation(ped->GetClump(), ANIM_ID_CAR_CLOSEDOOR_LHS_0);
	if (dragAssoc && dragAssoc->animId == ANIM_ID_BIKE_HIT && ped->m_pMyVehicle) {
		if (ped->m_vehDoor == CAR_DOOR_LF || ped->m_vehDoor == CAR_DOOR_RF) {
			CAnimManager::BlendAnimation(ped->GetClump(), ASSOCGRP_DEFAULT, ANIM_ID_BIKE_FALL_OFF, 100.0f);
			ped->m_pMyVehicle->m_nGettingOutFlags &= ~(CAR_DOOR_FLAG_RF | CAR_DOOR_FLAG_LF);
		} else {
			CAnimManager::BlendAnimation(ped->GetClump(), ASSOCGRP_DEFAULT, ANIM_ID_BIKE_FALLR, 100.0f);
			ped->m_pMyVehicle->m_nGettingOutFlags &= ~(CAR_DOOR_FLAG_RR | CAR_DOOR_FLAG_LR);
		}
		((CBike*)ped->m_pMyVehicle)->KnockOffRider(WEAPONTYPE_UNIDENTIFIED, 0, ped, true);
		return;
	}

	if (ped->m_nPedState != PED_ARRESTED) {
		ped->m_nLastPedState = PED_NONE;
		if (dragAssoc)
			dragAssoc->blendDelta = -1000.0f;
	}
	ped->RestartNonPartialAnims();
	ped->m_pVehicleAnim = nil;
	ped->m_pSeekTarget = nil;
	vehicle = ped->m_pMyVehicle;

	if (vehicle && vehicle->IsBike())
		exitFlags = GetBikeDoorFlagInclJumpInFromFront(ped->m_vehDoor);
	else
		exitFlags = GetCarDoorFlag(ped->m_vehDoor);

	if (vehicle)
		vehicle->m_nGettingOutFlags &= ~exitFlags;

	if (vehicle) {
		if (vehicle->pDriver == ped) {
			vehicle->RemoveDriver();
			if (vehicle->m_nDoorLock == CARLOCK_LOCKED_INITIALLY)
				vehicle->m_nDoorLock = CARLOCK_UNLOCKED;

		} else {
			vehicle->RemovePassenger(ped);
		}
	}
	ped->bInVehicle = false;
	if (ped->IsPlayer())
		AudioManager.PlayerJustLeftCar();

	if (ped->m_objective == OBJECTIVE_LEAVE_CAR_AND_DIE) {
		ped->m_fHealth = 0.0f;
		if(dragAssoc)
			dragAssoc->SetDeleteCallback(PedSetDraggedOutCarPositionCB, ped);
		else
			PedSetDraggedOutCarPositionCB(nil, ped);
		ped->SetDie(ANIM_ID_FLOOR_HIT, 1000.0f, 0.5f);
		return;
	}

	if (!dragAssoc) {
		// A missing animation has no delete event. Complete the positioning
		// immediately, after detaching the occupant from their seat.
		if (quickJackedAssoc)
			PedSetQuickDraggedOutCarPositionCB(nil, ped);
		else
			PedSetDraggedOutCarPositionCB(nil, ped);
	} else if (quickJackedAssoc) {
		dragAssoc->SetDeleteCallback(PedSetQuickDraggedOutCarPositionCB, ped);
	} else {
		dragAssoc->SetDeleteCallback(PedSetDraggedOutCarPositionCB, ped);
		if (ped->CanSetPedState())
			CAnimManager::BlendAnimation(ped->GetClump(), ASSOCGRP_DEFAULT, ANIM_ID_GETUP_0, 1000.0f);
	}

	ped->ReplaceWeaponWhenExitingVehicle();

	ped->m_nStoredMoveState = PEDMOVE_NONE;
	ped->bVehExitWillBeInstant = false;
}

void
CPed::PedSetInCarCB(CAnimBlendAssociation *animAssoc, void *arg)
{
	CPed *ped = (CPed*)arg;

	CVehicle *veh = ped->m_pMyVehicle;

	if (!veh)
		return;

	// Situation of entering car as a driver while there is already a driver exiting atm.
	CPed *driver = veh->pDriver;
	if (driver && driver->m_nPedState == PED_DRIVING && !veh->bIsBus && driver->m_objective == OBJECTIVE_LEAVE_CAR
		&& (ped->m_objective == OBJECTIVE_ENTER_CAR_AS_DRIVER || ped->m_nPedState == PED_CARJACK)) {
		driver->SetDead();
		driver->FlagToDestroyWhenNextProcessed();
		veh->pDriver = nil;
	}

	if (!ped->IsNotInWreckedVehicle() || ped->DyingOrDead())
		return;

	ped->bInVehicle = true;
	if (ped->m_objective == OBJECTIVE_ENTER_CAR_AS_DRIVER || ped->m_nPedState == PED_CARJACK)
		veh->bIsBeingCarJacked = false;

	if (veh->m_nNumGettingIn)
		--veh->m_nNumGettingIn;

	if (ped->IsPlayer() && ((CPlayerPed*)ped)->AdrenalineActive())
		((CPlayerPed*)ped)->ClearAdrenaline();

	if (veh->IsBoat()) {
		if (ped->IsPlayer()) {
			veh->SetStatus(STATUS_PLAYER);
			AudioManager.PlayerJustGotInCar();
		}
		veh->SetDriver(ped);
		if (!veh->bEngineOn)
			veh->bEngineOn = true;

		ped->SetPedState(PED_DRIVING);
		ped->StopNonPartialAnims();
		ped->RemoveWeaponWhenEnteringVehicle();
		return;
	}

	if (ped->m_pVehicleAnim)
		ped->m_pVehicleAnim->blendDelta = -1000.0f;

	ped->bDoBloodyFootprints = false;
	if (veh->m_nAlarmState == -1)
		veh->m_nAlarmState = 15000;

	if (ped->IsPlayer()) {
		if (ped->m_objective == OBJECTIVE_ENTER_CAR_AS_DRIVER || veh->IsBike()) {
			veh->SetStatus(STATUS_PLAYER);
		}
		AudioManager.PlayerJustGotInCar();
	}
	
	if (ped->m_objective == OBJECTIVE_ENTER_CAR_AS_DRIVER || ped->m_nPedState == PED_CARJACK) {
		veh->SetDriver(ped);

		if (!veh->bEngineOn) {
			veh->bEngineOn = true;
			DMAudio.PlayOneShot(veh->m_audioEntityId, SOUND_CAR_ENGINE_START, 1.0f);
		}
		ped->SetPedState(PED_DRIVING);
		if (ped->m_objective == OBJECTIVE_ENTER_CAR_AS_DRIVER)
			ped->RestorePreviousObjective();

	} else {
		if (veh->IsBike()) {
			veh->AddPassenger(ped, 0);
		} else if (veh->bIsBus) {
			veh->AddPassenger(ped);
		} else {
			switch (ped->m_vehDoor) {
				case CAR_DOOR_RF:
					veh->AddPassenger(ped, 0);
					break;
				case CAR_DOOR_RR:
					veh->AddPassenger(ped, 2);
					break;
				case CAR_DOOR_LR:
					veh->AddPassenger(ped, 1);
					break;
				default:
					veh->AddPassenger(ped);
					break;
			}
		}
		ped->SetPedState(PED_DRIVING);
		ped->RestorePreviousObjective();
	}

	int8 doorFlag;
	if (veh->IsBike()) {
		doorFlag = GetBikeDoorFlagInclJumpInFromFront(ped->m_vehDoor);
	} else {
		doorFlag = GetEnterCarDoorFlag(ped->m_vehDoor, veh->m_nNumMaxPassengers);
	}

	veh->m_nGettingInFlags &= ~doorFlag;
	if(ped->IsPlayer())
		REVC_LOG_INFO("Vehicle entry: seated (vehicle model=%d, driver=%d, door=%d)",
			veh->GetModelIndex(), veh->pDriver == ped, ped->m_vehDoor);

	if (veh->bIsBus && !veh->m_nGettingInFlags)
		((CAutomobile*)veh)->SetBusDoorTimer(1000, 1);

	ped->SetObjective(OBJECTIVE_NONE);

	ped->AddInCarAnims(veh, veh->pDriver == ped);
	if (veh->bIsBus)
		ped->bRenderPedInCar = false;

	if (ped->IsPlayer() && veh->VehicleCreatedBy != MISSION_VEHICLE) {
		veh->bHasBeenOwnedByPlayer = true;
	}
	ped->bChangedSeat = true;
}

bool
CPed::CanBeDeleted(void)
{
	if (bInVehicle)
		return false;

	switch (CharCreatedBy) {
		case RANDOM_CHAR:
			return true;
		case MISSION_CHAR:
			return false;
		case UNK_CHAR:
			return false;
		default:
			return true;
	}
}

bool
CPed::CanBeDeletedEvenInVehicle(void)
{
	switch (CharCreatedBy) {
		case RANDOM_CHAR:
			return true;
		case MISSION_CHAR:
			return false;
		case UNK_CHAR:
			return false;
		default:
			return true;
	}
}

// RpHAnimIDGetIndex answers -1 for a bone the skeleton does not carry, and a
// Vice City skeleton has no SA spine or clavicle; the matrix array cannot be
// indexed with it. The wind and body-damage code scales a bone it names, so an
// absent bone simply has nothing to scale.
// SA CPed::ShoulderBoneRotation (0x5DF560), ported verbatim. The player model
// carries separate breast bones for each side. They have to copy the matching
// upper arm's matrix and then keep only half of the X rotation relative to the
// clavicle - otherwise the chest skin tears away from the shoulder whenever the
// arm raises past the horizontal, which is exactly what the player does every
// time he aims. SA runs this for MODEL_PLAYER only, from PreRender.
//
// SA reads the bone matrix with RpHAnimHierarchyGetNodeMatrix; reVC has no such
// helper and indexes the matrix array by RpHAnimIDGetIndex, so an absent bone
// (a Vice City skeleton has no breast bones at all) is skipped instead of
// reading before the array.
void
CPed::ShoulderBoneRotation(RpClump *clump)
{
	RpHAnimHierarchy *hier = GetAnimHierarchyFromClump(clump);
	if(hier == nil)
		return;

	RwMatrix *rwMats = RpHAnimHierarchyGetMatrixArray(hier);
	if(rwMats == nil)
		return;

	// SA lambda GetMatrixOf: bone tag -> its matrix, nil when absent.
	auto GetMatrixOf = [&](int32 bone) -> RwMatrix* {
		int32 idx = RpHAnimIDGetIndex(hier, bone);
		if(idx < 0)
			return nil;
		return &rwMats[idx];
	};

	auto DoUpdate = [&](int32 breastBone, int32 upperArmBone, int32 clavicleBone) {
		RwMatrix *breastRwMat = GetMatrixOf(breastBone);
		RwMatrix *upperArmMat = GetMatrixOf(upperArmBone);
		RwMatrix *clavicleRwMat = GetMatrixOf(clavicleBone);
		if(breastRwMat == nil || upperArmMat == nil || clavicleRwMat == nil)
			return;

		// Make the breast's matrix same as the upper arm's
		RwMatrixCopy(breastRwMat, upperArmMat);

		CMatrix breastMat(breastRwMat);
		CMatrix clavicleMat(clavicleRwMat);

		// Calculate breast to clavicle transformation matrix (and store it in breastMat)
		breastMat = Invert(clavicleMat) * breastMat;

		// Half its X rotation
		float rx, ry, rz;
		breastMat.ConvertToEulerAngles(&rx, &ry, &rz, ORDER_ZYX | SWAP_XZ);
		rx /= 2.0f;
		breastMat.ConvertFromEulerAngles(rx, ry, rz, ORDER_ZYX | SWAP_XZ);

		// Transform it back into its own space
		breastMat = clavicleMat * breastMat;

		// Finally, update its RW associated matrix
		breastMat.UpdateRW();
	};

	DoUpdate(BONE_L_BREAST, BONE_L_UPPER_ARM, BONE_L_CLAVICLE);
	DoUpdate(BONE_R_BREAST, BONE_R_UPPER_ARM, BONE_R_CLAVICLE);
}

static RwMatrix *
GetBoneMatrixSafe(RpHAnimHierarchy *hier, int32 idx)
{
	if(hier == nil || idx < 0)
		return nil;
	RwMatrix *mats = RpHAnimHierarchyGetMatrixArray(hier);
	if(mats == nil)
		return nil;
	return &mats[idx];
}

// A frame the clump does not name stays empty in m_pFrames, and the animated
// frame behind it is what the IK rotations are written into.
RtQuat *
CPed::FrameQuat(int32 node) const
{
	if(node < 0 || node >= TOTAL_PED_NODES || m_pFrames[node] == nil ||
	   m_pFrames[node]->hanimFrame == nil)
		return nil;
	return &m_pFrames[node]->hanimFrame->q;
}

// A ped keeps only the model id it was created with; the model behind that id
// can be absent from the loaded data set, and the skate, stat and animation
// code all read fields from it.
CPedModelInfo *
CPed::PedModelInfo(void) const
{
	return (CPedModelInfo*)CModelInfo::GetModelInfo(m_modelIndex);
}

void
CPed::AddWeaponModel(int id)
{
	if (id != -1) {
		if (m_pWeaponModel)
			RemoveWeaponModel(-1);

		// A weapon id comes from the weapon table and need not name a model the
		// loaded data set carries; an absent one leaves the hand empty rather
		// than dereferencing nothing.
		CBaseModelInfo *weaponModel = CModelInfo::GetModelInfo(id);
		if (weaponModel == nil){
			REVC_LOG_WARNING("ped: weapon model %d is not loaded, no model attached", id);
			m_wepModelID = -1;
			return;
		}

		// CreateInstance answers nil when the model has no atomics loaded yet.
		// A weapon chosen while its DFF is still streaming was dropped here and
		// nothing ever tried again, so the hand stayed empty even though the
		// model finished loading a moment later - only weapons already resident
		// (the ones the game had warmed up) ever appeared. Ask for the model and
		// remember it, so the attachment happens once it is there.
		m_pWeaponModel = (RpAtomic*)weaponModel->CreateInstance();
		if (m_pWeaponModel == nil){
			if (!CStreaming::HasModelLoaded(id)){
				CStreaming::RequestModel(id, STREAMFLAGS_DEPENDENCY);
				m_wepModelID = id; // retried by RequestDelayedWeapon
				return;
			}
			REVC_LOG_WARNING("ped: weapon model %d has no atomics, no model attached", id);
			m_wepModelID = -1;
			return;
		}
		weaponModel->AddRef();
		m_wepModelID = id;

		if (IsPlayer() && id == MI_MINIGUN){
			CBaseModelInfo *minigunTop = CModelInfo::GetModelInfo(MI_MINIGUN2);
			if (minigunTop)
				((CPlayerPed*)this)->m_pMinigunTopAtomic = (RpAtomic*)minigunTop->CreateInstance();
		}
	}
}

static RwObject*
RemoveAllModelCB(RwObject *object, void *data)
{
	RpAtomic *atomic = (RpAtomic*)object;
	if (CVisibilityPlugins::GetAtomicModelInfo(atomic)) {
		RpClumpRemoveAtomic(RpAtomicGetClump(atomic), atomic);
		RpAtomicDestroy(atomic);
	}
	return object;
}

void
CPed::RemoveWeaponModel(int modelId)
{
	// modelId is not used!! This function just removes the current weapon.
	if(m_pWeaponModel){
		if (modelId == -1
			|| CVisibilityPlugins::GetAtomicModelInfo(m_pWeaponModel) == CModelInfo::GetModelInfo(modelId)) {
			CVisibilityPlugins::GetAtomicModelInfo(m_pWeaponModel)->RemoveRef();
			RwFrame* frm = RpAtomicGetFrame(m_pWeaponModel);
			RpAtomicDestroy(m_pWeaponModel);
			RwFrameDestroy(frm);
			m_pWeaponModel = nil;
		}
	}

	if (IsPlayer() && (modelId == -1 || modelId == MI_MINIGUN)) {
		RpAtomic* &atm = ((CPlayerPed*)this)->m_pMinigunTopAtomic;
		if (atm) {
			RwFrame *frm = RpAtomicGetFrame(atm);
			RpAtomicDestroy(atm);
			RwFrameDestroy(frm);
			atm = nil;
		}
	}
	m_wepModelID = -1;
}

void
CPed::RequestDelayedWeapon()
{
	// AddWeaponModel gave up on a weapon whose DFF was still streaming and
	// left its id here. Attach it as soon as the model is resident, or the hand
	// stays empty for a weapon the game did eventually load.
	if (m_pWeaponModel == nil && m_wepModelID != -1 && CStreaming::HasModelLoaded(m_wepModelID)) {
		int pending = m_wepModelID;
		m_wepModelID = -1;
		AddWeaponModel(pending);
	}
	if (m_delayedWeapon != WEAPONTYPE_UNIDENTIFIED) {
		int modelId1 = CWeaponInfo::GetWeaponInfo(m_delayedWeapon)->m_nModelId;
		int modelId2 = CWeaponInfo::GetWeaponInfo(m_delayedWeapon)->m_nModel2Id;
		if (modelId1 != -1)
			CStreaming::RequestModel(modelId1, STREAMFLAGS_DEPENDENCY);
		if (modelId2 != -1)
			CStreaming::RequestModel(modelId2, STREAMFLAGS_DEPENDENCY);

		if ((modelId1 == -1 || CStreaming::HasModelLoaded(modelId1))
			&& (modelId2 == -1 || CStreaming::HasModelLoaded(modelId2))) {
			GiveWeapon(m_delayedWeapon, m_delayedWeaponAmmo, 1);
			m_delayedWeapon = WEAPONTYPE_UNIDENTIFIED;
		}
	}
}

void
CPed::GiveDelayedWeapon(eWeaponType weapon, uint32 ammo)
{
	m_delayedWeapon = weapon;
	m_delayedWeaponAmmo = ammo;
	if (m_delayedWeapon != WEAPONTYPE_UNIDENTIFIED) {
		int modelId1 = CWeaponInfo::GetWeaponInfo(m_delayedWeapon)->m_nModelId;
		int modelId2 = CWeaponInfo::GetWeaponInfo(m_delayedWeapon)->m_nModel2Id;
		if (modelId1 != -1)
			CStreaming::RequestModel(modelId1, STREAMFLAGS_DEPENDENCY);
		if (modelId2 != -1)
			CStreaming::RequestModel(modelId2, STREAMFLAGS_DEPENDENCY);

		if ((modelId1 == -1 || CStreaming::HasModelLoaded(modelId1))
			&& (modelId2 == -1 || CStreaming::HasModelLoaded(modelId2))) {
			GiveWeapon(m_delayedWeapon, m_delayedWeaponAmmo, true);
			m_delayedWeapon = WEAPONTYPE_UNIDENTIFIED;
		}
	}
}

int32
CPed::GiveWeapon(eWeaponType weaponType, uint32 ammo, bool unused)
{
	int slot = GetWeaponSlot(weaponType);

	if (m_weapons[slot].m_eWeaponType == weaponType) {
		GetWeapon(slot).m_nAmmoTotal += ammo;
		if (weaponType < WEAPONTYPE_TOTALWEAPONS && weaponType > WEAPONTYPE_UNARMED && CWeaponInfo::ms_aMaxAmmoForWeapon[weaponType] >= 0) {

			// Looks like abandoned idea. This block never runs, ms_aMaxAmmoForWeapon is always -1.
			GetWeapon(slot).m_nAmmoTotal = Min(CWeaponInfo::ms_aMaxAmmoForWeapon[weaponType], GetWeapon(slot).m_nAmmoTotal);
		} else {
			GetWeapon(slot).m_nAmmoTotal = Min(99999, GetWeapon(slot).m_nAmmoTotal);
		}
		GetWeapon(slot).Reload();
		if (GetWeapon(slot).m_eWeaponState == WEAPONSTATE_OUT_OF_AMMO && GetWeapon(slot).m_nAmmoTotal > 0)
			GetWeapon(slot).m_eWeaponState = WEAPONSTATE_READY;
	} else {
		if (HasWeaponSlot(slot)) {
			if (CWeaponInfo::IsWeaponSlotAmmoMergeable(slot))
				ammo += GetWeapon(slot).m_nAmmoTotal;

			RemoveWeaponModel(CWeaponInfo::GetWeaponInfo(GetWeapon(slot).m_eWeaponType)->m_nModelId);
			GetWeapon(slot).Shutdown();
		}
		GetWeapon(slot).Initialise(weaponType, ammo);
		if (slot == m_currentWeapon && !bInVehicle) {
			AddWeaponModel(CWeaponInfo::GetWeaponInfo(GetWeapon()->m_eWeaponType)->m_nModelId);
		}
	}
	if (GetWeapon(slot).m_eWeaponState != WEAPONSTATE_OUT_OF_AMMO)
		GetWeapon(slot).m_eWeaponState = WEAPONSTATE_READY;

	return slot;
}

int
CPed::GetWeaponSlot(eWeaponType weaponType)
{
	return CWeaponInfo::GetWeaponInfo(weaponType)->m_nWeaponSlot;
}

void
CPed::SetCurrentWeapon(int slot)
{
	if (slot == -1)
		return;

	CWeaponInfo* weaponInfo;
	if (GetWeapon()->m_eWeaponType != WEAPONTYPE_UNARMED) {
		weaponInfo = CWeaponInfo::GetWeaponInfo(GetWeapon()->m_eWeaponType);
		RemoveWeaponModel(weaponInfo->m_nModelId);
	}
	m_currentWeapon = slot;

	if (FindPlayerPed() && IsPlayer())
		((CPlayerPed*)this)->m_nSelectedWepSlot = m_currentWeapon;

	if (HasWeaponSlot(slot)) {
		weaponInfo = CWeaponInfo::GetWeaponInfo(GetWeapon()->m_eWeaponType);
		AddWeaponModel(weaponInfo->m_nModelId);
	}
}

void
CPed::SetCurrentWeapon(eWeaponType weaponType)
{
	SetCurrentWeapon(CWeaponInfo::GetWeaponInfo(weaponType)->m_nWeaponSlot);
}

void
CPed::GrantAmmo(eWeaponType weaponType, uint32 ammo)
{
	int slot = CWeaponInfo::GetWeaponInfo(weaponType)->m_nWeaponSlot;
	if (slot == -1)
		return;

	GetWeapon(slot).m_nAmmoTotal += ammo;
	if (weaponType < WEAPONTYPE_TOTALWEAPONS && weaponType > WEAPONTYPE_UNARMED && CWeaponInfo::ms_aMaxAmmoForWeapon[weaponType] >= 0) {

		// Looks like abandoned idea. This block never runs, ms_aMaxAmmoForWeapon is always -1.
		GetWeapon(slot).m_nAmmoTotal = Min(CWeaponInfo::ms_aMaxAmmoForWeapon[weaponType], GetWeapon(slot).m_nAmmoTotal);
	} else {
		GetWeapon(slot).m_nAmmoTotal = Min(99999, GetWeapon(slot).m_nAmmoTotal);
	}

	if (GetWeapon(slot).m_eWeaponState == WEAPONSTATE_OUT_OF_AMMO && GetWeapon(slot).m_nAmmoTotal > 0)
		GetWeapon(slot).m_eWeaponState = WEAPONSTATE_READY;
}

void
CPed::SetAmmo(eWeaponType weaponType, uint32 ammo)
{
	int slot = CWeaponInfo::GetWeaponInfo(weaponType)->m_nWeaponSlot;
	if (slot == -1)
		return;

	GetWeapon(slot).m_nAmmoTotal = ammo;
	if (weaponType < WEAPONTYPE_TOTALWEAPONS && weaponType > WEAPONTYPE_UNARMED && CWeaponInfo::ms_aMaxAmmoForWeapon[weaponType] >= 0) {

		// Looks like abandoned idea. This block never runs, ms_aMaxAmmoForWeapon is always -1.
		GetWeapon(slot).m_nAmmoTotal = Min(CWeaponInfo::ms_aMaxAmmoForWeapon[weaponType], GetWeapon(slot).m_nAmmoTotal);
	} else {
		GetWeapon(slot).m_nAmmoTotal = Min(99999, GetWeapon(slot).m_nAmmoTotal);
	}
	int32 newClip = GetWeapon(slot).m_nAmmoTotal;
	if (newClip >= GetWeapon(slot).m_nAmmoInClip)
		newClip = GetWeapon(slot).m_nAmmoInClip;
	GetWeapon(slot).m_nAmmoInClip = newClip;

	if (GetWeapon(slot).m_eWeaponState == WEAPONSTATE_OUT_OF_AMMO && GetWeapon(slot).m_nAmmoTotal > 0)
		GetWeapon(slot).m_eWeaponState = WEAPONSTATE_READY;
}

void
CPed::ClearWeapons(void)
{
	RemoveWeaponModel(-1);
	for (int i = 0; i < ARRAY_SIZE(m_weapons); i++) {
		GetWeapon(i).Shutdown();
	}
	SetCurrentWeapon(WEAPONTYPE_UNARMED);
}

void
CPed::RemoveWeaponWhenEnteringVehicle(void)
{
	if (IsPlayer() && HasWeaponSlot(5) && GetWeapon(5).m_nAmmoTotal > 0 && ((CPlayerPed*)this)->GetPlayerInfoForThisPlayerPed()->m_bDriveByAllowed) {
		if (m_storedWeapon == WEAPONTYPE_UNIDENTIFIED)
			m_storedWeapon = GetWeapon()->m_eWeaponType;
		SetCurrentWeapon(GetWeapon(5).m_eWeaponType);
	} else {
		CWeaponInfo *ourWeapon = CWeaponInfo::GetWeaponInfo(GetWeapon()->m_eWeaponType);
		RemoveWeaponModel(ourWeapon->m_nModelId);
	}
}
void
CPed::ReplaceWeaponWhenExitingVehicle(void)
{
	eWeaponType weaponType = GetWeapon()->m_eWeaponType;

	// If it's Uzi, we may have stored weapon. Uzi is the only gun we can use in car.
	if (IsPlayer() && GetWeaponSlot(weaponType) == WEAPONSLOT_SUBMACHINEGUN) {
		if (m_storedWeapon != WEAPONTYPE_UNIDENTIFIED) {
			SetCurrentWeapon(m_storedWeapon);
			m_storedWeapon = WEAPONTYPE_UNIDENTIFIED;
		}
	} else {
		AddWeaponModel(CWeaponInfo::GetWeaponInfo(weaponType)->m_nModelId);
	}
}

// SA CPed::PreRenderAfterTest (0x5E65A0). Runs before the bone matrices are
// rebuilt, so anything written here lands in the pose that is actually drawn.
void
CPed::PreRenderAfterTest(void)
{
	// SA also lets the swimming and jetpack tasks lay the body over and veto
	// the slope lean for the frame. Those tasks do not exist in this fork, so
	// there is nothing to veto yet.
	if(m_pedIK.m_flags & CPedIK::SLOPE_PITCH)
		m_pedIK.PitchForSlope();

	UpdateRpHAnim();
	bCalledPreRender = true;
}

void
CPed::PreRender(void)
{
	// The skin is rebuilt below, so the bone queries of this frame have a
	// hierarchy to read from here on.
	bCalledPreRender = false;
	if(m_nPedState != PED_DRIVING)
		PreRenderAfterTest();

	CShadows::StoreShadowForPed(this,
		CTimeCycle::m_fShadowDisplacementX[CTimeCycle::m_CurrentStoredValue], CTimeCycle::m_fShadowDisplacementY[CTimeCycle::m_CurrentStoredValue],
		CTimeCycle::m_fShadowFrontX[CTimeCycle::m_CurrentStoredValue], CTimeCycle::m_fShadowFrontY[CTimeCycle::m_CurrentStoredValue],
		CTimeCycle::m_fShadowSideX[CTimeCycle::m_CurrentStoredValue], CTimeCycle::m_fShadowSideY[CTimeCycle::m_CurrentStoredValue]);

	// SA CPed::PreRender (0x5E65A0) poses the player's chest bones by hand and
	// then vetoes the hierarchy update for the frame, so the half-rotation it
	// just wrote into the breast bones survives. Without this the player's chest
	// tears away from the shoulder whenever the arm goes past horizontal.
	if (GetModelIndex() == MI_PLAYER) {
		ShoulderBoneRotation(GetClump());
		bDontUpdateHierarchy = true;
	}

	UpdateRpHAnim();

	bool bIsWindModifierTurnedOn = false;
	float fAnyDirectionShift = 1.0f;
	bool bIsPedDrivingBikeOrOpenTopCar = false;
	if (IsPlayer() && CWindModifiers::FindWindModifier(GetPosition(), &fAnyDirectionShift, &fAnyDirectionShift)
		&& !CCullZones::PlayerNoRain() && GetPedState() != PED_DRIVING)
		bIsWindModifierTurnedOn = true;

	if (GetPedState() == PED_DRIVING && m_pMyVehicle) {
		if (m_pMyVehicle->m_nVehicleType == VEHICLE_TYPE_BIKE
			|| (m_pMyVehicle->m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE && m_pMyVehicle->IsOpenTopCar()))
			bIsPedDrivingBikeOrOpenTopCar = true;
	}

	if (bIsWindModifierTurnedOn || bIsPedDrivingBikeOrOpenTopCar) {
		float fWindMult = 0.0f;
		if (bIsPedDrivingBikeOrOpenTopCar) {
			fWindMult = DotProduct(m_pMyVehicle->m_vecMoveSpeed, GetForward());
			if (fWindMult > 0.4f) {
				float volume = (fWindMult - 0.4f) / 0.6f;
				DMAudio.PlayOneShot(m_audioEntityId, SOUND_SHIRT_WIND_FLAP, volume);
			}
		}

		if (bIsWindModifierTurnedOn) 
			fWindMult = Min(fWindMult, Abs(fAnyDirectionShift - 1.0f));

		RpHAnimHierarchy* hier = GetAnimHierarchyFromSkinClump(GetClump());
		int32 idx;
		RwV3d scale;
		float fScaleOffset;

		fScaleOffset = fWindMult * 0.2f;
		scale.x = CGeneral::GetRandomNumberInRange(1.0f - fScaleOffset, 1.0f + fScaleOffset);
		scale.y = CGeneral::GetRandomNumberInRange(1.0f - fScaleOffset, 1.0f + fScaleOffset);
		scale.z = CGeneral::GetRandomNumberInRange(1.0f - fScaleOffset, 1.0f + fScaleOffset);

		idx = RpHAnimIDGetIndex(hier, ConvertPedNode2BoneTag(PED_NODE_NECK));
		RwMatrix* neck = GetBoneMatrixSafe(hier, idx);
		if (neck)
			RwMatrixScale(neck, &scale, rwCOMBINEPRECONCAT);

		fScaleOffset = fWindMult * 0.1f;
		scale.x = CGeneral::GetRandomNumberInRange(1.0f - fScaleOffset, 1.0f + fScaleOffset);
		scale.y = CGeneral::GetRandomNumberInRange(1.0f - fScaleOffset, 1.0f + fScaleOffset);
		scale.z = CGeneral::GetRandomNumberInRange(1.0f - fScaleOffset, 1.0f + fScaleOffset);

		idx = RpHAnimIDGetIndex(hier, ConvertPedNode2BoneTag(PED_NODE_LEFT_CLAVICLE));
		RwMatrix* clavicleL = GetBoneMatrixSafe(hier, idx);
		if (clavicleL)
			RwMatrixScale(clavicleL, &scale, rwCOMBINEPRECONCAT);
		
		idx = RpHAnimIDGetIndex(hier, ConvertPedNode2BoneTag(PED_NODE_RIGHT_CLAVICLE));
		RwMatrix* clavicleR = GetBoneMatrixSafe(hier, idx);
		if (clavicleR)
			RwMatrixScale(clavicleR, &scale, rwCOMBINEPRECONCAT);

		idx = RpHAnimIDGetIndex(hier, ConvertPedNode2BoneTag(PED_NODE_UPPER_TORSO));
		RwMatrix* mid = GetBoneMatrixSafe(hier, idx);
		if (mid)
			RwMatrixScale(mid, &scale, rwCOMBINEPRECONCAT);

		fScaleOffset = fWindMult * 0.2f;
		scale.x = CGeneral::GetRandomNumberInRange(1.0f - fScaleOffset, 1.0f + fScaleOffset);
		scale.y = CGeneral::GetRandomNumberInRange(1.0f - fScaleOffset, 1.0f + fScaleOffset);
		scale.z = CGeneral::GetRandomNumberInRange(1.0f - fScaleOffset, 1.0f + fScaleOffset);

		idx = RpHAnimIDGetIndex(hier, ConvertPedNode2BoneTag(PED_NODE_LEFT_ARM));
		RwMatrix* upperArmL = GetBoneMatrixSafe(hier, idx);
		if (upperArmL)
			RwMatrixScale(upperArmL, &scale, rwCOMBINEPRECONCAT);

		idx = RpHAnimIDGetIndex(hier, ConvertPedNode2BoneTag(PED_NODE_RIGHT_ARM));
		RwMatrix* upperArmR = GetBoneMatrixSafe(hier, idx);
		if (upperArmR)
			RwMatrixScale(upperArmR, &scale, rwCOMBINEPRECONCAT);
	}

	if (bBodyPartJustCameOff && m_bodyPartBleeding == PED_NODE_HEAD) {
		// scale head to 0 if shot off
		RpHAnimHierarchy* hier = GetAnimHierarchyFromSkinClump(GetClump());
		int32 idx = RpHAnimIDGetIndex(hier, ConvertPedNode2BoneTag(PED_NODE_HEAD));
		RwMatrix* head = GetBoneMatrixSafe(hier, idx);
		RwV3d zero = { 0.0f, 0.0f, 0.0f };
		if (head)
			RwMatrixScale(head, &zero, rwCOMBINEPRECONCAT);
	}

	if (IsPlayer() && gfTommyFatness != 1.0f) {
		RpHAnimHierarchy* hier = GetAnimHierarchyFromSkinClump(GetClump());
		int32 idx;
		RwV3d scale;

		scale.x = 1.0f;
		scale.y = 1.0f + gfTommyFatness * 0.7f;
		scale.z = 1.0f + gfTommyFatness * 0.7f;
		idx = RpHAnimIDGetIndex(hier, ConvertPedNode2BoneTag(PED_NODE_HEAD));
		RwMatrix* head = GetBoneMatrixSafe(hier, idx);
		if (head)
			RwMatrixScale(head, &scale, rwCOMBINEPRECONCAT);

		scale.y = 1.0f + gfTommyFatness * 0.2f;
		scale.z = 1.0f + gfTommyFatness * 0.2f;
		idx = RpHAnimIDGetIndex(hier, ConvertPedNode2BoneTag(PED_NODE_NECK));
		RwMatrix* neck = GetBoneMatrixSafe(hier, idx);
		if (neck)
			RwMatrixScale(neck, &scale, rwCOMBINEPRECONCAT);

		scale.y = 1.0f + gfTommyFatness * 0.5f;
		scale.z = 1.0f + gfTommyFatness * 0.5f;
		idx = RpHAnimIDGetIndex(hier, ConvertPedNode2BoneTag(PED_NODE_UPPER_TORSO));
		RwMatrix* mid = GetBoneMatrixSafe(hier, idx);
		if (mid)
			RwMatrixScale(mid, &scale, rwCOMBINEPRECONCAT);

		scale.y = 1.0f + gfTommyFatness;
		scale.z = 1.0f + gfTommyFatness;
		idx = RpHAnimIDGetIndex(hier, ConvertPedNode2BoneTag(PED_NODE_LEFT_LEG));
		RwMatrix* upperLegL = GetBoneMatrixSafe(hier, idx);
		if (upperLegL)
			RwMatrixScale(upperLegL, &scale, rwCOMBINEPRECONCAT);

		idx = RpHAnimIDGetIndex(hier, ConvertPedNode2BoneTag(PED_NODE_RIGHT_LEG));
		RwMatrix* upperLegR = GetBoneMatrixSafe(hier, idx);
		if (upperLegR)
			RwMatrixScale(upperLegR, &scale, rwCOMBINEPRECONCAT);

		scale.y = 1.0f + gfTommyFatness * 0.5f;
		scale.z = 1.0f + gfTommyFatness * 0.5f;
		idx = RpHAnimIDGetIndex(hier, ConvertPedNode2BoneTag(PED_NODE_RIGHT_LOWER_LEG));
		RwMatrix* lowerLegR = GetBoneMatrixSafe(hier, idx);
		if (lowerLegR)
			RwMatrixScale(lowerLegR, &scale, rwCOMBINEPRECONCAT);

		idx = RpHAnimIDGetIndex(hier, ConvertPedNode2BoneTag(PED_NODE_LEFT_LOWER_LEG));
		RwMatrix* lowerLegL = GetBoneMatrixSafe(hier, idx);
		if (lowerLegL)
			RwMatrixScale(lowerLegL, &scale, rwCOMBINEPRECONCAT);

		scale.y = 1.0f + gfTommyFatness * 0.23f;
		scale.z = 1.0f + gfTommyFatness * 0.23f;
		idx = RpHAnimIDGetIndex(hier, ConvertPedNode2BoneTag(PED_NODE_LEFT_FOOT));
		RwMatrix* footL = GetBoneMatrixSafe(hier, idx);
		if (footL)
			RwMatrixScale(footL, &scale, rwCOMBINEPRECONCAT);

		idx = RpHAnimIDGetIndex(hier, ConvertPedNode2BoneTag(PED_NODE_RIGHT_FOOT));
		RwMatrix* footR = GetBoneMatrixSafe(hier, idx);
		if (footR)
			RwMatrixScale(footR, &scale, rwCOMBINEPRECONCAT);

		idx = RpHAnimIDGetIndex(hier, ConvertPedNode2BoneTag(PED_NODE_LEFT_ARM));
		RwMatrix* upperArmL = GetBoneMatrixSafe(hier, idx);
		if (upperArmL)
			RwMatrixScale(upperArmL, &scale, rwCOMBINEPRECONCAT);

		idx = RpHAnimIDGetIndex(hier, ConvertPedNode2BoneTag(PED_NODE_RIGHT_ARM));
		RwMatrix* upperArmR = GetBoneMatrixSafe(hier, idx);
		if (upperArmR)
			RwMatrixScale(upperArmR, &scale, rwCOMBINEPRECONCAT);

		scale.y = 1.0f + gfTommyFatness * 0.2f;
		scale.z = 1.0f + gfTommyFatness * 0.2f;
		idx = RpHAnimIDGetIndex(hier, ConvertPedNode2BoneTag(PED_NODE_LEFT_LOWER_ARM));
		RwMatrix* foreArmL = GetBoneMatrixSafe(hier, idx);
		if (foreArmL)
			RwMatrixScale(foreArmL, &scale, rwCOMBINEPRECONCAT);

		idx = RpHAnimIDGetIndex(hier, ConvertPedNode2BoneTag(PED_NODE_RIGHT_LOWER_ARM));
		RwMatrix* foreArmR = GetBoneMatrixSafe(hier, idx);
		if (foreArmR)
			RwMatrixScale(foreArmR, &scale, rwCOMBINEPRECONCAT);
	}

	if (bBodyPartJustCameOff && bIsPedDieAnimPlaying && m_bodyPartBleeding != -1 && (CTimer::GetFrameCounter() & 7) > 3) {
		CVector bloodDir(0.0f, 0.0f, 0.0f);
		CVector bloodPos(0.0f, 0.0f, 0.0f);

		TransformToNode(bloodPos, m_bodyPartBleeding);

		switch (m_bodyPartBleeding) {
			case PED_NODE_HEAD:
				bloodDir = 0.1f * GetUp();
				break;
			case PED_NODE_LEFT_ARM:
				bloodDir = 0.04f * GetUp() - 0.04f * GetRight();
				break;
			case PED_NODE_RIGHT_ARM:
				bloodDir = 0.04f * GetUp() - 0.04f * GetRight();
				break;
			case PED_NODE_LEFT_LEG:
				bloodDir = 0.04f * GetUp() + 0.05f * GetForward();
				break;
			case PED_NODE_RIGHT_LEG:
				bloodDir = 0.04f * GetUp() + 0.05f * GetForward();
				break;
			default:
				bloodDir = CVector(0.0f, 0.0f, 0.0f);
				break;
		}

		for(int i = 0; i < 4; i++)
			CParticle::AddParticle(PARTICLE_BLOOD_SPURT, bloodPos, bloodDir, nil, 0.0f, 0, 0, 0, 0);
	}
	if (CWeather::Rain > 0.3f && TheCamera.SoundDistUp > 15.0f) {
		if ((TheCamera.GetPosition() - GetPosition()).Magnitude() < 25.0f) {
			bool doSplashUp = true;
			CColModel *ourCol = CModelInfo::GetColModel(GetModelIndex());
			CVector speed = FindPlayerSpeed();

			if (Abs(speed.x) <= 0.05f && Abs(speed.y) <= 0.05f) {
				if (!OnGround() && m_nPedState != PED_ATTACK && m_nPedState != PED_FIGHT) {
					if (!IsPedHeadAbovePos(0.3f) || RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_IDLE_TIRED)) {
						doSplashUp = false;
					}
				} else
					doSplashUp = false;
			} else
				doSplashUp = false;

			if (doSplashUp && ourCol->numSpheres > 0) {
				for(int i = 0; i < ourCol->numSpheres; i++) {
					CColSphere *sphere = &ourCol->spheres[i];
					CVector splashPos;
					switch (sphere->material) {
						case PED_PIECE_LEFT_ARM:
						case PED_PIECE_RIGHT_ARM:
						case PED_PIECE_HEAD:
							splashPos = GetMatrix() * ourCol->spheres[i].center;
							splashPos.z += 0.7f * sphere->radius;
							splashPos.x += CGeneral::GetRandomNumberInRange(-0.15f, 0.15f);
							splashPos.y += CGeneral::GetRandomNumberInRange(-0.15f, 0.15f);
							CParticle::AddParticle(PARTICLE_RAIN_SPLASHUP, splashPos, CVector(0.0f, 0.0f, 0.0f), nil, 0.0f, 0, 0, CGeneral::GetRandomNumber() & 1, 0);
							break;
						default:
							break;
					}
				}
			}
		}
	}
}

CVector vecTestTemp(-1.0f, -1.0f, -1.0f);

void
CPed::Render(void)
{
	if (bInVehicle && m_pMyVehicle && m_nPedState != PED_EXIT_CAR && m_nPedState != PED_DRAGGED_FROM_CAR) {
		if (!bRenderPedInCar)
			return;

		if (!m_pMyVehicle->IsBike() && !IsPlayer()) {
			float camDistSq = (TheCamera.GetPosition() - GetPosition()).MagnitudeSqr();
			if (camDistSq > SQR((m_pMyVehicle->IsBoat() ? 40.0f : 25.0f) * TheCamera.LODDistMultiplier))
				return;
		}
	}

	CEntity::Render();

	if(m_pWeaponModel){
		// The weapon hangs off the right hand bone; a skeleton without it, or a
		// clump that is not skinned, leaves no matrix to hang it from.
		if(m_pFrames[PED_NODE_RIGHT_HAND] == nil)
			return;
		RpHAnimHierarchy *hier = GetAnimHierarchyFromSkinClump(GetClump());
		if(hier == nil)
			return;
		int idx = RpHAnimIDGetIndex(hier, m_pFrames[PED_NODE_RIGHT_HAND]->nodeID);
		RwMatrix *mat = GetBoneMatrixSafe(hier, idx);
		if(mat == nil)
			return;
		RwFrame *frame = RpAtomicGetFrame(m_pWeaponModel);
		if(frame == nil)
			return;
		*RwFrameGetMatrix(frame) = *mat;
		RwFrameUpdateObjects(frame);
		RpAtomicRender(m_pWeaponModel);
		if (IsPlayer()) {
			CPlayerPed *player = (CPlayerPed*)this;
			if (player->m_pMinigunTopAtomic) {
				frame = RpAtomicGetFrame(player->m_pMinigunTopAtomic);
				*RwFrameGetMatrix(frame) = *mat;

				player->GunSpinAngle() = player->GunSpinSpeed() * CTimer::GetTimeStep() + player->GunSpinAngle();
				if (player->GunSpinAngle() > TWOPI)
					player->GunSpinAngle() -= TWOPI;

				CMatrix mgTopMat, localAdjMat;
				mgTopMat.Attach(RwFrameGetMatrix(frame));
				localAdjMat.SetRotateX(player->GunSpinAngle());
				localAdjMat.Rotate(DEGTORAD(-4.477f)* vecTestTemp.x, DEGTORAD(29.731f) * vecTestTemp.y, DEGTORAD(1.064f) * vecTestTemp.z);
				localAdjMat.GetPosition() += CVector(0.829f, -0.001f, 0.226f);
				mgTopMat = mgTopMat * localAdjMat;
				mgTopMat.UpdateRW();

				RwFrameUpdateObjects(frame);
				RpAtomicRender(player->m_pMinigunTopAtomic);
			}
		}
	}
}

void
CPed::SetIdle(void)
{
	if (m_nPedState != PED_IDLE) {
		if (m_nPedState == PED_AIMGUN)
			ClearPointGunAt();

		SetPedState(PED_IDLE);
		SetMoveState(PEDMOVE_STILL);
		m_nLastPedState = PED_NONE;
	}
}

void
CPed::Idle(void)
{
	CVehicle *veh = m_pMyVehicle;
	if (veh && veh->m_nGettingOutFlags && m_vehDoor) {
		if (veh->m_nGettingOutFlags & GetCarDoorFlag(m_vehDoor)) {
			CVector doorPos = GetPositionToOpenCarDoor(veh, m_vehDoor, this);
			CVector doorDist = GetPosition() - doorPos;
			if (doorDist.MagnitudeSqr() < sq(0.5f)) {
				SetMoveState(PEDMOVE_WALK);
				return;
			}
		}
	}

	if (m_nMoveState != PEDMOVE_STILL && !IsPlayer())
		SetMoveState(PEDMOVE_STILL);

	m_moved = CVector2D(0.0f, 0.0f);
}

void
CPed::SetFall(int extraTime, AnimationId animId, uint8 evenIfNotInControl)
{
	if (m_attachedTo)
		return;

	if (!IsPedInControl() && (!evenIfNotInControl || DyingOrDead()))
		return;

	ClearLookFlag();
	ClearAimFlag();
	SetStoredState();
	SetPedState(PED_FALL);
	CAnimBlendAssociation *fallAssoc = nil;
	if (animId == ANIM_ID_UNDEFINED) {
		if (IsPlayer()) {
			fallAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_DEFAULT_CAR_ROLLOUT_LHS);
			if (!fallAssoc)
				fallAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_DEFAULT_CAR_ROLLOUT_LHS);
		}
	} else {
		fallAssoc = RpAnimBlendClumpGetAssociation(GetClump(), animId);

		if (fallAssoc) {
			fallAssoc->SetCurrentTime(0.0f);
			fallAssoc->blendAmount = 0.0f;
			fallAssoc->blendDelta = 8.0f;
			fallAssoc->SetRun();
		}
		else {
			fallAssoc = CAnimManager::BlendAnimation(GetClump(), ASSOCGRP_DEFAULT, animId, 8.0f);
		}
		if(!fallAssoc && (animId == ANIM_ID_BIKE_FALLR || animId == ANIM_ID_BIKE_FALL_OFF))
			fallAssoc = CAnimManager::BlendAnimation(GetClump(), ASSOCGRP_DEFAULT,
				ANIM_ID_FALL_BACK, 8.0f); // same skeleton; no VC clips on SA peds
		if (fallAssoc && animId == ANIM_ID_BIKE_FALLR && fallAssoc->animId == animId)
			fallAssoc->SetCurrentTime(0.4f);
	}

	if (extraTime == -1) {
		m_getUpTimer = UINT32_MAX;
	} else if (fallAssoc) {
		if (IsPlayer()) {
			if (fallAssoc->animId == ANIM_ID_DEFAULT_CAR_ROLLOUT_LHS || fallAssoc->animId == ANIM_ID_DEFAULT_CAR_ROLLOUT_LHS) {
				m_getUpTimer = 1000.0f * fallAssoc->hierarchy->totalLength
					+ CTimer::GetTimeInMilliseconds()
					- 1000.0f * fallAssoc->currentTime
					+ 100.0f;
			} else {
				m_getUpTimer = 1000.0f * fallAssoc->hierarchy->totalLength
					+ CTimer::GetTimeInMilliseconds()
					+ 500.0f;
			}
		} else {
			m_getUpTimer = 1000.0f * fallAssoc->hierarchy->totalLength
				+ CTimer::GetTimeInMilliseconds()
				+ extraTime
				+ ((m_randomSeed + CTimer::GetFrameCounter()) % 1000);
		}
	} else {
		m_getUpTimer = extraTime
			+ CTimer::GetTimeInMilliseconds()
			+ 1000
			+ ((m_randomSeed + CTimer::GetFrameCounter()) % 1000);
	}
	bFallenDown = true;
}

void
CPed::ClearFall(void)
{
	SetGetUp();
}

void
CPed::Fall(void)
{
	if (m_getUpTimer != UINT32_MAX && CTimer::GetTimeInMilliseconds() > m_getUpTimer && bIsStanding)
		ClearFall();

	CAnimBlendAssociation *firstPartialAssoc;
	CAnimBlendAssociation *fallAssoc;

	if (IsPlayer() && (bKnockedUpIntoAir || bKnockedOffBike) && !bIsStanding) {
		firstPartialAssoc = RpAnimBlendClumpGetFirstAssociation(GetClump(), ASSOC_PARTIAL);

		// What???
		if (firstPartialAssoc && (firstPartialAssoc->animId == ANIM_ID_FALL_BACK || firstPartialAssoc->animId == ANIM_ID_FALL_FRONT))
			fallAssoc = firstPartialAssoc;
		else
			fallAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_FALL_BACK);

		if (!fallAssoc)
			fallAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_FALL_FRONT);

		if (!fallAssoc && firstPartialAssoc && 0.8f * firstPartialAssoc->hierarchy->totalLength < firstPartialAssoc->currentTime) {
			if (firstPartialAssoc->flags & ASSOC_FRONTAL) {
				CAnimManager::BlendAnimation(GetClump(), ASSOCGRP_DEFAULT, ANIM_ID_FALL_FRONT, 8.0f);
			} else {
				CAnimManager::BlendAnimation(GetClump(), ASSOCGRP_DEFAULT, ANIM_ID_FALL_BACK, 8.0f);
			}
		} else if (fallAssoc && fallAssoc->blendAmount > 0.3f && fallAssoc->blendDelta >= 0.0f)	{
			float time = fallAssoc->currentTime;

			if (time > 0.667f && time - fallAssoc->timeStep <= 0.667f) {
				fallAssoc->SetCurrentTime(0.0f);
				fallAssoc->SetRun();
			}
		}
	} else if ((bKnockedUpIntoAir || bKnockedOffBike) && bIsStanding && !bWasStanding) {
		fallAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_FALL_BACK);

		if (!fallAssoc)
			fallAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_FALL_FRONT);

		if (fallAssoc) {
			bKnockedUpIntoAir = false;
			bKnockedOffBike = false;
			fallAssoc->speed = 3.0f;
			if (IsPlayer())
				Say(SOUND_PED_LAND);

		} else {
			firstPartialAssoc = RpAnimBlendClumpGetFirstAssociation(GetClump(), ASSOC_PARTIAL);
			if (firstPartialAssoc && !firstPartialAssoc->IsRunning()) {
				bKnockedUpIntoAir = false;
				bKnockedOffBike = false;
			}
		}
	}
}

bool
CPed::CheckIfInTheAir(void)
{
	if (bInVehicle)
		return false;

	CVector pos = GetPosition();
	CColPoint foundColPoint;
	CEntity *foundEntity;

	float startZ = pos.z - 1.54f;
	bool foundGround = CWorld::ProcessVerticalLine(pos, startZ, foundColPoint, foundEntity, true, true, false, true, false, false, nil);
	if (!foundGround && m_nPedState != PED_JUMP)
	{
		pos.z -= FEET_OFFSET;
		if (CWorld::TestSphereAgainstWorld(pos, 0.15f, this, true, false, false, false, false, false))
			foundGround = true;
	}
	return !foundGround;
}

void
CPed::SetInTheAir(void)
{
	if (bIsInTheAir)
		return;

	bIsInTheAir = true;
	CAnimManager::BlendAnimation(GetClump(), ASSOCGRP_DEFAULT, ANIM_ID_FALL_GLIDE, 4.0f);

	if (m_nPedState == PED_ATTACK) {
		ClearAttack();
		ClearPointGunAt();
	} else if (m_nPedState == PED_FIGHT) {
		EndFight(ENDFIGHT_FAST);
	}

}

void
CPed::InTheAir(void)
{
	CColPoint foundCol;
	CEntity *foundEnt;

	CVector ourPos = GetPosition();
	CVector bitBelow = GetPosition();
	bitBelow.z -= 4.04f;

	if (m_vecMoveSpeed.z < 0.0f && !bIsPedDieAnimPlaying) {
		if (!DyingOrDead()) {
			if (CWorld::ProcessLineOfSight(ourPos, bitBelow, foundCol, foundEnt, true, true, false, true, false, false, false)) {
				if (GetPosition().z - foundCol.point.z < 1.3f || bIsStanding)
					SetLanding();
			} else if (!RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_FALL_FALL)) {
				if (m_vecMoveSpeed.z < -0.1f)
					CAnimManager::BlendAnimation(GetClump(), ASSOCGRP_DEFAULT, ANIM_ID_FALL_FALL, 4.0f);
			}
		}
	}
}

void
CPed::SetLanding(void)
{
	if (DyingOrDead())
		return;

	CAnimBlendAssociation *fallAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_FALL_FALL);
	CAnimBlendAssociation *landAssoc;

	if (fallAssoc && bIsDrowning)
		return;

	RpAnimBlendClumpSetBlendDeltas(GetClump(), ASSOC_PARTIAL, -1000.0f);
	if (fallAssoc || m_nPedType == PEDTYPE_COP && bKnockedUpIntoAir) {
		landAssoc = CAnimManager::AddAnimation(GetClump(), ASSOCGRP_DEFAULT, ANIM_ID_FALL_COLLAPSE);
		DMAudio.PlayOneShot(m_audioEntityId, SOUND_FALL_COLLAPSE, 1.0f);

		if (IsPlayer())
			Say(SOUND_PED_LAND);

		if (m_nPedType == PEDTYPE_COP) {
			if (bKnockedUpIntoAir)
				bKnockedUpIntoAir = false;
		}

	} else {
		landAssoc = CAnimManager::AddAnimation(GetClump(), ASSOCGRP_DEFAULT, ANIM_ID_FALL_LAND);
		DMAudio.PlayOneShot(m_audioEntityId, SOUND_FALL_LAND, 1.0f);
	}

	if (!landAssoc)
		landAssoc = CAnimManager::AddAnimation(GetClump(), ASSOCGRP_DEFAULT, ANIM_ID_FALL_LAND);
	bIsInTheAir = false;
	if (!landAssoc) {
		// No landing callback can arrive without a compatible SA clip.
		bIsLanding = false;
		if (m_nPedState == PED_JUMP)
			RestorePreviousState();
		return;
	}
	landAssoc->SetFinishCallback(PedLandCB, this);
	bIsLanding = true;
}

void
CPed::SetGetUp(void)
{
	if (m_nPedState == PED_GETUP && bGetUpAnimStarted)
		return;

	if (!CanSetPedState())
		return;

	if (m_fHealth >= 1.0f || IsPedHeadAbovePos(-0.3f)) {
		if (bUpdateAnimHeading) {
			m_fRotationCur = CGeneral::LimitRadianAngle(m_fRotationCur);
			m_fRotationCur -= HALFPI;
			bUpdateAnimHeading = false;
		}
		if (m_nPedState != PED_GETUP) {
			SetStoredState();
			SetPedState(PED_GETUP);
		}

		// Collision scratch. A file static in the reference; it is filled and
		// read inside this one expression, so a local is both safer and free.
		CColPoint aTempPedColPts[MAX_COLLISION_POINTS];

		CVehicle *collidingVeh = (CVehicle*)m_pCollidingEntity;
		CVehicle *veh = (CVehicle*)CWorld::TestSphereAgainstWorld(GetPosition(), 0.25f, nil, true, true, false, false, false, false);
		if (veh && veh->m_nVehicleType != VEHICLE_TYPE_BIKE && veh != m_attachedTo ||
			collidingVeh && collidingVeh->IsVehicle() && collidingVeh->m_nVehicleType != VEHICLE_TYPE_BIKE
			&& ((uint8)(CTimer::GetFrameCounter() + m_randomSeed + 5) % 8 ||
				 CCollision::ProcessColModels(GetMatrix(), *GetColModel(), collidingVeh->GetMatrix(), *collidingVeh->GetColModel(),
					aTempPedColPts, nil, nil) > 0)) {

			bGetUpAnimStarted = false;
			if (IsPlayer())
				InflictDamage(nil, WEAPONTYPE_RUNOVERBYCAR, CTimer::GetTimeStep(), PED_PIECE_TORSO, 0);
			else if(CPad::GetPad(0)->ArePlayerControlsDisabled())
				InflictDamage(nil, WEAPONTYPE_RUNOVERBYCAR, 1000.0f, PED_PIECE_TORSO, 0);
			return;
		}
		bGetUpAnimStarted = true;
		m_pCollidingEntity = nil;
		bKnockedUpIntoAir = false;
		bKnockedOffBike = false;
		CAnimBlendAssociation *animAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_SPRINT);
		if (animAssoc) {
			if (RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_RUN)) {
				CAnimManager::BlendAnimation(GetClump(), m_animGroup, ANIM_ID_RUN, 8.0f);
			} else {
				CAnimManager::BlendAnimation(GetClump(), m_animGroup, ANIM_ID_IDLE, 8.0f);
			}
			animAssoc->flags |= ASSOC_DELETEFADEDOUT;
		}

		if (RpAnimBlendClumpGetFirstAssociation(GetClump(), ASSOC_FRONTAL))
			animAssoc = CAnimManager::BlendAnimation(GetClump(), ASSOCGRP_DEFAULT, ANIM_ID_GETUP_FRONT, 1000.0f);
		else
			animAssoc = CAnimManager::BlendAnimation(GetClump(), ASSOCGRP_DEFAULT, ANIM_ID_GETUP_0, 1000.0f);

		if(!animAssoc)
			animAssoc = CAnimManager::BlendAnimation(GetClump(), ASSOCGRP_DEFAULT, ANIM_ID_GETUP_0, 1000.0f);
		if(animAssoc)
			animAssoc->SetFinishCallback(PedGetupCB, this);
		else { // Missing custom get-up clip: never strand a living ped in PED_GETUP.
			bFallenDown = false;
			RestorePreviousState(); // requires bGetUpAnimStarted to still be true
			bGetUpAnimStarted = false;
			SetMoveAnim();
		}
	} else {
		m_fHealth = 0.0f;
		SetDie(ANIM_ID_UNDEFINED, 4.0f, 0.0f);
	}
}

// Unused
void
CPed::SetLook(float direction)
{
	if (IsPedInControl()) {
		SetStoredState();
		SetPedState(PED_LOOK_HEADING);
		SetLookFlag(direction, false);
	}
}

void
CPed::SetLook(CEntity* to)
{
	if (IsPedInControl()) {
		SetStoredState();
		SetPedState(PED_LOOK_ENTITY);
		SetLookFlag(to, false);
	}
}

void
CPed::SetLookTimer(int time)
{
	if (CTimer::GetTimeInMilliseconds() > m_lookTimer) {
		m_lookTimer = CTimer::GetTimeInMilliseconds() + time;
	}
}

void
CPed::SetAttackTimer(uint32 time)
{
	if (CTimer::GetTimeInMilliseconds() > m_attackTimer)
		m_attackTimer = Max(m_shootTimer, CTimer::GetTimeInMilliseconds()) + time;
}

void
CPed::SetShootTimer(uint32 time)
{
	if (CTimer::GetTimeInMilliseconds() > m_shootTimer) {
		m_shootTimer = CTimer::GetTimeInMilliseconds() + time;
	}
}

void
CPed::ClearLook(void)
{
	RestorePreviousState();
	ClearLookFlag();
}

void
CPed::Look(void)
{
	TurnBody();
}

bool
CPed::TurnBody(void)
{
	bool turnDone = true;

	if (m_pLookTarget)
		m_fLookDirection = CGeneral::GetRadianAngleBetweenPoints(
					m_pLookTarget->GetPosition().x,
					m_pLookTarget->GetPosition().y,
					GetPosition().x,
					GetPosition().y);

	float limitedLookDir = CGeneral::LimitRadianAngle(m_fLookDirection);
	float currentRot = m_fRotationCur;

	if (currentRot - PI > limitedLookDir)
		limitedLookDir += 2 * PI;
	else if (PI + currentRot < limitedLookDir)
		limitedLookDir -= 2 * PI;

	float neededTurn = currentRot - limitedLookDir;
	m_fRotationDest = limitedLookDir;

	if (Abs(neededTurn) > 0.05f) {
		turnDone = false;
		currentRot -= neededTurn * 0.2f;
	}

	m_fRotationCur = currentRot;
	m_fLookDirection = limitedLookDir;
	return turnDone;
}

void
CPed::SetSeek(CVector pos, float distanceToCountDone)
{
	if (!IsPedInControl()
		|| (m_nPedState == PED_SEEK_POSITION && m_vecSeekPos.x == pos.x && m_vecSeekPos.y == pos.y))
		return;

	if (!CWeaponInfo::GetWeaponInfo(GetWeapon()->m_eWeaponType)->IsFlagSet(WEAPONFLAG_CANAIM_WITHARM)) {
		ClearPointGunAt();
	}

	if (m_nPedState != PED_SEEK_POSITION)
		SetStoredState();

	SetPedState(PED_SEEK_POSITION);
	m_distanceToCountSeekDone = distanceToCountDone;
	m_vecSeekPos = pos;
}

void
CPed::SetSeek(CEntity *seeking, float distanceToCountDone)
{
	if (!IsPedInControl())
		return;

	if (m_nPedState == PED_SEEK_ENTITY && m_pSeekTarget == seeking)
		return;

	if (!seeking)
		return;

	if (m_nPedState != PED_SEEK_ENTITY)
		SetStoredState();

	SetPedState(PED_SEEK_ENTITY);
	m_distanceToCountSeekDone = distanceToCountDone;
	m_pSeekTarget = seeking;
	m_pSeekTarget->RegisterReference((CEntity **) &m_pSeekTarget);
	SetMoveState(PEDMOVE_STILL);
}

void
CPed::ClearSeek(void)
{
	SetIdle();
}

bool
CPed::Seek(void)
{
	float distanceToCountItDone = m_distanceToCountSeekDone;

	if (!m_pSeekTarget && m_nPedState == PED_SEEK_ENTITY)
		ClearSeek();

	float seekPosDist = (m_vecSeekPos - GetPosition()).Magnitude2D();
	eMoveState nextMove = PEDMOVE_WALK;

	if (m_nPedState == PED_SEEK_ENTITY) {
		if (m_pSeekTarget->IsPed()) {
			if (((CPed*)m_pSeekTarget)->bInVehicle)
				distanceToCountItDone += 2.0f;
		}
	}

	if (seekPosDist >= distanceToCountItDone) {
		if (CTimer::GetTimeInMilliseconds() <= m_nPedStateTimer) {
			if (m_actionX != 0.0f && m_actionY != 0.0f) {
				m_fRotationDest = CGeneral::GetRadianAngleBetweenPoints(
					m_actionX, m_actionY,
					GetPosition().x, GetPosition().y);

				float neededTurn = Abs(m_fRotationDest - m_fRotationCur);
				if (neededTurn > PI)
					neededTurn = TWOPI - neededTurn;

				if (neededTurn > HALFPI) {
					if (seekPosDist >= 1.0f) {
						if (seekPosDist < 2.0f)
							nextMove = PEDMOVE_WALK;
					} else {
						nextMove = PEDMOVE_STILL;
					}
				}

				CVector2D moveDist(GetPosition().x - m_actionX, GetPosition().y - m_actionY);
				if (moveDist.Magnitude() < 0.5f) {
					m_nPedStateTimer = 0;
					m_actionX = 0.f;
					m_actionY = 0.f;
				}
			}
		} else {
			m_fRotationDest = CGeneral::GetRadianAngleBetweenPoints(
				m_vecSeekPos.x, m_vecSeekPos.y,
				GetPosition().x, GetPosition().y);

			float neededTurn = Abs(m_fRotationDest - m_fRotationCur);
			if (neededTurn > PI)
				neededTurn = TWOPI - neededTurn;

			if (neededTurn > HALFPI) {
				if (seekPosDist >= 1.0f && neededTurn <= DEGTORAD(135.0f)) {
					if (seekPosDist < 2.0f)
						nextMove = PEDMOVE_WALK;
				} else {
					nextMove = PEDMOVE_STILL;
				}
			}
		}

		SetMoveState(nextMove);
		SetMoveAnim();
		return false;
	}

	if (m_nMoveState != PEDMOVE_STILL) {
		m_nPedStateTimer = 0;
		m_actionX = 0.f;
		m_actionY = 0.f;
	}

	return true;
}

void
CPed::SetEvasiveStep(CPhysical *reason, uint8 animType)
{
	AnimationId stepAnim;

	if (m_nPedState == PED_EVADE_STEP || !IsPedInControl() || ((IsPlayer() || !bRespondsToThreats) && animType == 0))
		return;

	float angleToFace = CGeneral::GetRadianAngleBetweenPoints(
		reason->GetPosition().x, reason->GetPosition().y,
		GetPosition().x, GetPosition().y);
	angleToFace = CGeneral::LimitRadianAngle(angleToFace);
	m_fRotationCur = CGeneral::LimitRadianAngle(m_fRotationCur);
	float neededTurn = Abs(angleToFace - m_fRotationCur);
	bool vehPressedHorn = false;

	if (neededTurn > PI)
		neededTurn = TWOPI - neededTurn;

	if (reason->IsVehicle() && ((CVehicle*)reason)->IsAutomobile()) {
		CVehicle *veh = (CVehicle*)reason;
		if (veh->m_nCarHornTimer != 0) {
			vehPressedHorn = true;
			if (!IsPlayer())
				animType = 1;
		}
	}

	if (neededTurn <= DEGTORAD(90.0f) || reason->GetModelIndex() == MI_RCBANDIT || vehPressedHorn || animType != 0) {
		SetLookFlag(reason, true);
		if ((CGeneral::GetRandomNumber() & 1) && reason->GetModelIndex() != MI_RCBANDIT && animType == 0) {
			stepAnim = ANIM_ID_IDLE_TAXI;

		} else {
			float dangerDirection = CGeneral::GetRadianAngleBetweenPoints(
				reason->m_vecMoveSpeed.x, reason->m_vecMoveSpeed.y,
				0.0f, 0.0f);

			// Let's turn our back to the "reason"
			angleToFace += PI;

			if (angleToFace > PI)
				angleToFace -=  TWOPI;

			// We don't want to run towards car's direction
			float dangerZone = angleToFace - dangerDirection;
			dangerZone = CGeneral::LimitRadianAngle(dangerZone);

			// So, add or subtract 90deg (jump to left/right) according to that
			if (dangerZone > 0.0f)
				angleToFace = dangerDirection - HALFPI;
			else
				angleToFace = dangerDirection + HALFPI;

			stepAnim = ANIM_ID_UNDEFINED;
			if (animType == 0 || animType == 1)
				stepAnim = ANIM_ID_EV_STEP;
			else if (animType == 2)
				stepAnim = ANIM_ID_HANDSCOWER;
		}
		if (!RpAnimBlendClumpGetAssociation(GetClump(), stepAnim)) {
			CAnimBlendAssociation *stepAssoc = CAnimManager::BlendAnimation(GetClump(), ASSOCGRP_DEFAULT, stepAnim, 8.0f);
			if (!stepAssoc) return;
			stepAssoc->flags &= ~ASSOC_DELETEFADEDOUT;
			stepAssoc->SetFinishCallback(PedEvadeCB, this);

			if (animType == 0)
				Say(SOUND_PED_EVADE);

			m_fRotationCur = CGeneral::LimitRadianAngle(angleToFace);
			ClearAimFlag();
			SetStoredState();
			SetPedState(PED_EVADE_STEP);
		}
	}
}

void
CPed::SetEvasiveDive(CPhysical *reason, uint8 onlyRandomJump)
{
	if (!IsPedInControl() || !bRespondsToThreats)
		return;

	CAnimBlendAssociation *animAssoc;
	float angleToFace, neededTurn;
	bool handsUp = false;

	angleToFace = m_fRotationCur;
	CVehicle *veh = (CVehicle*) reason;
	if (reason->IsVehicle() && veh->m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE && veh->m_nCarHornTimer != 0 && !IsPlayer()) {
		onlyRandomJump = true;
	}

	if (onlyRandomJump) {
		if (reason) {
			// Simple version of my bug fix below. Doesn't calculate "danger zone", selects jump direction randomly.
			// Also doesn't include random hands up, sound etc. Only used on player ped and peds running from gun shots.

			float vehDirection = CGeneral::GetRadianAngleBetweenPoints(
				veh->m_vecMoveSpeed.x, veh->m_vecMoveSpeed.y,
				0.0f, 0.0f);
			angleToFace = (CGeneral::GetRandomNumber() & 1) * PI + (-0.5f*PI) + vehDirection;
			angleToFace = CGeneral::LimitRadianAngle(angleToFace);
		}
	} else {
		if (IsPlayer()) {
			((CPlayerPed*)this)->m_nEvadeAmount = 5;
			((CPlayerPed*)this)->m_pEvadingFrom = reason;
			reason->RegisterReference((CEntity**) &((CPlayerPed*)this)->m_pEvadingFrom);
			return;
		}

		angleToFace = CGeneral::GetRadianAngleBetweenPoints(
			reason->GetPosition().x, reason->GetPosition().y,
			GetPosition().x, GetPosition().y);
		angleToFace = CGeneral::LimitRadianAngle(angleToFace);
		m_fRotationCur = CGeneral::LimitRadianAngle(m_fRotationCur);

		// FIX: Peds no more select dive direction randomly. Taken from SetEvasiveStep, last if statement inverted
#ifdef FIX_BUGS
		float vehDirection = CGeneral::GetRadianAngleBetweenPoints(
			veh->m_vecMoveSpeed.x, veh->m_vecMoveSpeed.y,
			0.0f, 0.0f);

		// Let's turn our back to the "reason"
		angleToFace += PI;

		if (angleToFace > PI)
			angleToFace -= 2 * PI;

		// We don't want to dive towards car's direction
		float dangerZone = angleToFace - vehDirection;
		dangerZone = CGeneral::LimitRadianAngle(dangerZone);

		// So, add or subtract 90deg (jump to left/right) according to that
		if (dangerZone > 0.0f)
			angleToFace = 0.5f * PI + vehDirection;
		else
			angleToFace = vehDirection - 0.5f * PI;
#endif

		neededTurn = Abs(angleToFace - m_fRotationCur);

		if (neededTurn > PI)
			neededTurn = 2 * PI - neededTurn;

		if (neededTurn <= 0.5f*PI) {
			if (CGeneral::GetRandomNumber() & 1)
				handsUp = true;
		} else {
			if (CGeneral::GetRandomNumber() & 7)
				return;
		}

		// VC's primitve solution to dive direction problem, see above for better one. This part doesn't exist on III at all
#ifndef FIX_BUGS
		angleToFace += HALFPI;
		if (CGeneral::GetRandomNumber() & 1)
			angleToFace -= PI;
#endif
		Say(SOUND_PED_EVADE);
	}

	if (handsUp) {
		m_fRotationCur = angleToFace;
		ClearLookFlag();
		ClearAimFlag();
		SetLookFlag(reason, true);
		animAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_HANDSUP);
		if (animAssoc)
			return;

		animAssoc = CAnimManager::BlendAnimation(GetClump(), ASSOCGRP_DEFAULT, ANIM_ID_HANDSUP, 8.0f);
		if (!animAssoc) {
			ClearLookFlag();
			return;
		}
		animAssoc->flags &= ~ASSOC_DELETEFADEDOUT;
		animAssoc->SetFinishCallback(PedEvadeCB, this);
		SetStoredState();
		SetPedState(PED_EVADE_STEP);
	} else {
		m_fRotationCur = angleToFace;
		ClearLookFlag();
		ClearAimFlag();
		SetStoredState();
		SetPedState(PED_EVADE_DIVE);
		animAssoc = CAnimManager::BlendAnimation(GetClump(), ASSOCGRP_DEFAULT, ANIM_ID_EV_DIVE, 8.0f);
		if (animAssoc)
			animAssoc->SetFinishCallback(PedEvadeCB, this);
		else
			PedEvadeCB(nil, this);
	}

	if (reason->IsVehicle() && m_nPedType == PEDTYPE_COP) {
		if (veh->pDriver && veh->pDriver->IsPlayer()) {
			CWanted *wanted = FindPlayerPed()->Wanted();
			wanted->RegisterCrime_Immediately(CRIME_RECKLESS_DRIVING, GetPosition(), (uintptr)this, false);
			wanted->RegisterCrime_Immediately(CRIME_SPEEDING, GetPosition(), (uintptr)this, false);
		}
	}
}

void
CPed::PedEvadeCB(CAnimBlendAssociation* animAssoc, void* arg)
{
	CPed* ped = (CPed*)arg;

	if (!animAssoc) {
		ped->ClearLookFlag();
		if (ped->m_nPedState == PED_EVADE_DIVE || ped->m_nPedState == PED_EVADE_STEP)
			ped->RestorePreviousState();

	} else if (animAssoc->animId == ANIM_ID_EV_DIVE) {
		ped->bUpdateAnimHeading = true;
		ped->ClearLookFlag();
		if (ped->m_nPedState == PED_EVADE_DIVE) {
			ped->m_getUpTimer = CTimer::GetTimeInMilliseconds() + 1;
			ped->SetPedState(PED_FALL);
		}
		animAssoc->flags &= ~ASSOC_FADEOUTWHENDONE;
		animAssoc->flags |= ASSOC_DELETEFADEDOUT;

	} else if (animAssoc->flags & ASSOC_FADEOUTWHENDONE) {
		ped->ClearLookFlag();
		if (ped->m_nPedState == PED_EVADE_DIVE || ped->m_nPedState == PED_EVADE_STEP)
			ped->RestorePreviousState();

	} else if (ped->m_nPedState != PED_ARRESTED) {
		animAssoc->flags |= ASSOC_DELETEFADEDOUT;
		if (animAssoc->blendDelta >= 0.0f)
			animAssoc->blendDelta = -4.0f;

		ped->ClearLookFlag();
		if (ped->m_nPedState == PED_EVADE_DIVE || ped->m_nPedState == PED_EVADE_STEP) {
			ped->RestorePreviousState();
		}
	}
}

void
CPed::SetDie(AnimationId animId, float delta, float speed)
{
	CPlayerPed *player = FindPlayerPed();
	if (player == this) {
		if (!player->CanBeDamaged())
			return;
	}

	if (DyingOrDead())
		return;

	CAnimBlendAssociation *dieAssoc = nil;
	if (m_nPedState == PED_FALL || m_nPedState == PED_GETUP)
		delta *=  0.5f;

	SetStoredState();
	ClearAll();
	m_fHealth = 0.0f;
	if (m_nPedState == PED_DRIVING) {
		if (!IsPlayer() && (!m_pMyVehicle || !m_pMyVehicle->IsBike()))
			FlagToDestroyWhenNextProcessed();
	} else if (bInVehicle) {
		if (m_pVehicleAnim)
			m_pVehicleAnim->blendDelta = -1000.0f;
	} else if (EnteringCar()) {
		QuitEnteringCar();
	}

	SetPedState(PED_DIE);
	if (animId == ANIM_ID_UNDEFINED) {
		bIsPedDieAnimPlaying = false;
	} else {
		dieAssoc = CAnimManager::BlendAnimation(GetClump(), ASSOCGRP_DEFAULT, animId, delta);
		if(!dieAssoc && animId != ANIM_ID_KO_SHOT_FRONT_0)
			dieAssoc = CAnimManager::BlendAnimation(GetClump(), ASSOCGRP_DEFAULT, ANIM_ID_KO_SHOT_FRONT_0, delta);
		if(dieAssoc){
			if (speed > 0.0f)
				dieAssoc->speed = speed;
			dieAssoc->flags &= ~ASSOC_FADEOUTWHENDONE;
			if (dieAssoc->IsRunning()) {
				dieAssoc->SetFinishCallback(FinishDieAnimCB, this);
				bIsPedDieAnimPlaying = true;
			}
		} else {
			bIsPedDieAnimPlaying = false;
			static bool warned = false;
			if(!warned){
				REVC_LOG_WARNING("Death animation missing in the ped bank (model=%d, anim=%d)",
					GetModelIndex(), animId);
				warned = true;
			}
		}
	}

	Say(SOUND_PED_DEATH);
	if (m_nLastPedState == PED_ENTER_CAR || m_nLastPedState == PED_CARJACK)
		QuitEnteringCar();

	if (!bInVehicle)
		StopNonPartialAnims();

	m_bloodyFootprintCountOrDeathTime = CTimer::GetTimeInMilliseconds();
	if (!CGame::nastyGame && animId == ANIM_ID_FLOOR_HIT) {
		if (dieAssoc) {
			dieAssoc->SetCurrentTime(dieAssoc->hierarchy->totalLength - 0.01f);
			dieAssoc->SetRun();
		}
	}
}

void
CPed::FinishDieAnimCB(CAnimBlendAssociation *animAssoc, void *arg)
{
	CPed *ped = (CPed*)arg;

	if (ped->bIsPedDieAnimPlaying)
		ped->bIsPedDieAnimPlaying = false;
}

void
CPed::SetDead(void)
{
	if (!RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_DROWN))
		bUsesCollision = false;

	m_fHealth = 0.0f;
	if (m_nPedState == PED_DRIVING)
		bIsVisible = false;

	SetPedState(PED_DEAD);
	m_pVehicleAnim = nil;
	m_pCollidingEntity = nil;

	CWeaponInfo *weapon = CWeaponInfo::GetWeaponInfo(GetWeapon()->m_eWeaponType);
	RemoveWeaponModel(weapon->m_nModelId);

	m_currentWeapon = WEAPONTYPE_UNARMED;
	if (this != FindPlayerPed()) {
		RemoveWeaponAnims(0, -1000.0f);
		CreateDeadPedWeaponPickups();
		CreateDeadPedMoney();
	}

	m_bloodyFootprintCountOrDeathTime = CTimer::GetTimeInMilliseconds();
	m_deadBleeding = false;
	bDoBloodyFootprints = false;
	bVehExitWillBeInstant = false;
}

void
CPed::Die(void)
{
	// UNUSED: This is a perfectly empty function.
}

void
StartTalkingOnMobileCB(CAnimBlendAssociation* assoc, void* arg)
{
	CPed* ped = (CPed*)arg;
	if (ped->m_nPedState == PED_ANSWER_MOBILE)
		CAnimManager::BlendAnimation(ped->GetClump(), ASSOCGRP_DEFAULT, ANIM_ID_PHONE_TALK, 4.0f);
}

void
FinishTalkingOnMobileCB(CAnimBlendAssociation *assoc, void *arg)
{
	CPed *ped = (CPed*)arg;
	if (ped->m_storedWeapon != WEAPONTYPE_UNIDENTIFIED) {
		ped->RemoveWeaponModel(MI_MOBILE);
		ped->SetCurrentWeapon(ped->m_storedWeapon);
		ped->m_storedWeapon = WEAPONTYPE_UNIDENTIFIED;
	}
	ped->m_lookTimer = 0;
}

void
CPed::Teleport(CVector pos)
{
	CWorld::Remove(this);
	SetPosition(pos);
	bIsStanding = false;
	m_nPedStateTimer = 0;
	m_actionX = 0.0f;
	m_actionY = 0.0f;
	m_pDamageEntity = nil;
	CWorld::Add(this);
}

void
CPed::SetSeekCar(CVehicle *car, uint32 doorNode)
{
	if (m_nPedState == PED_SEEK_CAR)
		return;

	if (!CanSetPedState() || m_nPedState == PED_DRIVING)
		return;

	SetStoredState();
	m_pSeekTarget = car;
	m_pSeekTarget->RegisterReference((CEntity**) &m_pSeekTarget);
	m_carInObjective = car;
	m_carInObjective->RegisterReference((CEntity**) &m_carInObjective);
	m_pMyVehicle = car;
	m_pMyVehicle->RegisterReference((CEntity**) &m_pMyVehicle);
	// m_pSeekTarget->RegisterReference((CEntity**) &m_pSeekTarget);
	m_vehDoor = doorNode;
	m_distanceToCountSeekDone = 0.5f;
	SetPedState(PED_SEEK_CAR);

}

void
CPed::SeekCar(void)
{
	CVehicle *vehToSeek = m_carInObjective;
	CVector dest(0.0f, 0.0f, 0.0f);
	if (!vehToSeek) {
		RestorePreviousState();
		return;
	}

	if (m_objective != OBJECTIVE_ENTER_CAR_AS_PASSENGER) {
		if (!vehToSeek->IsBike() && m_vehDoor && m_objective != OBJECTIVE_ENTER_CAR_AS_DRIVER) {
			if (IsRoomToBeCarJacked()) {
				dest = GetPositionToOpenCarDoor(vehToSeek, m_vehDoor, this);
			} else if (m_nPedType == PEDTYPE_COP) {
				dest = GetPositionToOpenCarDoor(vehToSeek, CAR_DOOR_RF, this);
			} else {
				SetMoveState(PEDMOVE_STILL);
			}
		} else
			GetNearestDoor(vehToSeek, dest);
	} else {
		if (m_carJackTimer > CTimer::GetTimeInMilliseconds()) {
			SetMoveState(PEDMOVE_STILL);
			return;
		}
		if (vehToSeek->GetModelIndex() == MI_COACH) {
			GetNearestDoor(vehToSeek, dest);
		} else {
			if (!GetNearestPassengerDoor(vehToSeek, dest)) {
				if (vehToSeek->m_nNumPassengers == vehToSeek->m_nNumMaxPassengers) {
					RestorePreviousObjective();
					RestorePreviousState();
				} else {
					SetMoveState(PEDMOVE_STILL);
				}
				bVehEnterDoorIsBlocked = true;
				return;
			}
			bVehEnterDoorIsBlocked = false;
		}
	}

	if (dest.x == 0.0f && dest.y == 0.0f) {
#ifdef FIX_BUGS
		if ((!IsPlayer() && CharCreatedBy != MISSION_CHAR) || vehToSeek->VehicleCreatedBy != MISSION_VEHICLE || vehToSeek->pDriver || !vehToSeek->CanPedOpenLocks(this)) {
			if(IsPlayer())
				REVC_LOG_WARNING("Vehicle entry: no usable door (vehicle model=%d, objective=%d, driver=%d, locks=%d)",
					vehToSeek->GetModelIndex(), m_objective, vehToSeek->pDriver != nil,
					vehToSeek->CanPedOpenLocks(this));
#else
		if ((!IsPlayer() && CharCreatedBy != MISSION_CHAR) || vehToSeek->VehicleCreatedBy != MISSION_VEHICLE || vehToSeek->pDriver) {
#endif
			RestorePreviousState();
			if (IsPlayer()) {
				ClearObjective();
			} else if (CharCreatedBy == RANDOM_CHAR) {
				m_carJackTimer = CTimer::GetTimeInMilliseconds() + 30000;
			}
			SetMoveState(PEDMOVE_STILL);
			TheCamera.ClearPlayerWeaponMode();
				return;
		}
		dest = vehToSeek->GetPosition();
		if (bCollidedWithMyVehicle) {
			WarpPedIntoCar(m_pMyVehicle);
			return;
		}
	}
	CVector doorDest = dest;
	bool foundBetterPosToSeek = PossiblyFindBetterPosToSeekCar(&dest, vehToSeek);
	m_vecSeekPos = dest;
	float distToDestSqr = (m_vecSeekPos - GetPosition()).MagnitudeSqr();

	if (bIsRunning ||
		vehToSeek->pDriver && distToDestSqr > sq(2.0f) && (Abs(vehToSeek->m_vecMoveSpeed.x) > 0.01f || Abs(vehToSeek->m_vecMoveSpeed.y) > 0.01f))
		SetMoveState(PEDMOVE_RUN);
	else if (distToDestSqr < sq(2.0f))
		SetMoveState(PEDMOVE_WALK);

	if (distToDestSqr >= 1.0f)
		bCanPedEnterSeekedCar = false;
	else if (2.0f * vehToSeek->GetColModel()->boundingBox.max.x > distToDestSqr)
		bCanPedEnterSeekedCar = true;

	if (vehToSeek->m_nGettingInFlags & GetCarDoorFlag(m_vehDoor))
		bVehEnterDoorIsBlocked = true;
	else
		bVehEnterDoorIsBlocked = false;

	// Sample player approaches without flooding the log on every frame.
	bool arrived = Seek();
	if (IsPlayer() && (CTimer::GetFrameCounter() & 31) == 0)
		REVC_LOG_INFO("Vehicle entry seek: model=%d door=%d dist2d=%.2f dz=%.2f radius=%.2f arrived=%d detour=%d blocked=%d canEnter=%d doorPos=(%.2f,%.2f,%.2f) seekPos=(%.2f,%.2f,%.2f) pedPos=(%.2f,%.2f,%.2f)",
			vehToSeek->GetModelIndex(), m_vehDoor, (dest - GetPosition()).Magnitude2D(), dest.z - GetPosition().z,
			m_distanceToCountSeekDone, arrived, foundBetterPosToSeek, bVehEnterDoorIsBlocked, vehToSeek->CanPedEnterCar(),
			doorDest.x, doorDest.y, doorDest.z, dest.x, dest.y, dest.z, GetPosition().x, GetPosition().y, GetPosition().z);

	// Arrived to the car
	if (arrived) {
		if (!foundBetterPosToSeek) {
			if (1.6f + GetPosition().z > dest.z && GetPosition().z - 0.5f < dest.z) {
			m_fRotationCur = m_fRotationDest;
			if (!bVehEnterDoorIsBlocked) {
				vehToSeek->SetIsStatic(false);
				if (vehToSeek->m_nNumGettingIn < vehToSeek->m_nNumMaxPassengers + 1
					&& vehToSeek->CanPedEnterCar()) {

						switch (vehToSeek->GetStatus()) {
							case STATUS_PLAYER:
							case STATUS_PHYSICS:
							case STATUS_PLAYER_DISABLED:
								if (vehToSeek->IsBike()) {
									if (((m_vehDoor == CAR_DOOR_LF || m_vehDoor == CAR_DOOR_RF || m_vehDoor == CAR_WINDSCREEN) && vehToSeek->pDriver ||
										(m_vehDoor == CAR_DOOR_LR || m_vehDoor == CAR_DOOR_RR) && vehToSeek->pPassengers[0])) {
										SetCarJack(vehToSeek);
									} else {
										SetEnterCar(vehToSeek, m_vehDoor);
									}
								} else if (!vehToSeek->bIsBus &&
									(m_vehDoor == CAR_DOOR_LF && vehToSeek->pDriver || m_vehDoor == CAR_DOOR_RF && vehToSeek->pPassengers[0] || m_vehDoor == CAR_DOOR_LR && vehToSeek->pPassengers[1] || m_vehDoor == CAR_DOOR_RR && vehToSeek->pPassengers[2])) {
									SetCarJack(vehToSeek);
								} else {
									SetEnterCar(vehToSeek, m_vehDoor);
								}
								break;
							case STATUS_ABANDONED:
								if (vehToSeek->IsBike()) {
									if ((m_vehDoor == CAR_DOOR_LR || m_vehDoor == CAR_DOOR_RR) && vehToSeek->pPassengers[0]) {
										if (vehToSeek->pPassengers[0]->bDontDragMeOutCar) {
											if (IsPlayer())
												SetEnterCar(vehToSeek, m_vehDoor);
										} else {
											SetCarJack(vehToSeek);
										}
									} else {
										SetEnterCar(vehToSeek, m_vehDoor);
									}
								} else if (m_vehDoor == CAR_DOOR_RF && vehToSeek->pPassengers[0]) {
									if (vehToSeek->pPassengers[0]->bDontDragMeOutCar) {
										if (IsPlayer())
											SetEnterCar(vehToSeek, m_vehDoor);
									} else {
										SetCarJack(vehToSeek);
									}
								} else {
									SetEnterCar(vehToSeek, m_vehDoor);
								}
								break;
							case STATUS_WRECKED:
								SetIdle();
								break;
							default:
								return;
						}
					} else {
						RestorePreviousState();
					}
				} else {
					SetMoveState(PEDMOVE_STILL);
				}
			}
		}
	}
}

bool
CPed::IsPlayer(void) const
{
	// We still have those in enum, so let's also check for them.
	return m_nPedType == PEDTYPE_PLAYER1 || m_nPedType == PEDTYPE_PLAYER2 ||
		m_nPedType == PEDTYPE_PLAYER_NETWORK || m_nPedType == PEDTYPE_PLAYER_UNUSED;
}

bool
IsPedPointerValid(CPed* pPed)
{
	if (!IsPedPointerValid_NotInWorld(pPed))
		return false;
	if (pPed->bInVehicle && pPed->m_pMyVehicle)
		return IsEntityPointerValid(pPed->m_pMyVehicle);
	return pPed->m_entryInfoList.first || pPed == FindPlayerPed();
}

bool
IsPedPointerValid_NotInWorld(CPed* pPed)
{
	// The slot can still contain old ped fields after delete. Range alone is
	// not proof that a pointer from a nearby-ped/event list is still alive.
	return pPed && CPools::GetPedPool()->IsAllocated(pPed);
}

bool
CPed::IsPointerValid(void)
{
	// This method is called to DISCARD stale pointers during
	// ped destruction. GetIndex asserts for a freed/foreign slot before the
	// old range check had a chance to reject it. Check allocation first without
	// reading any ped fields or creating a handle.
	if (!CPools::GetPedPool()->IsAllocated(this))
		return false;
	return m_entryInfoList.first || FindPlayerPed() == this;
}

void
CPed::SetPedPositionInCar(void)
{
	bool notYet = false;

	if (bChangedSeat) {
		if (m_pMyVehicle->IsBike()) {
			if (RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_CAR_GETIN_LHS_0)
				|| RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_CAR_GETIN_RHS_0)
				|| RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_CAR_GETIN_BIKE_FRONT)) {
				LineUpPedWithCar(LINE_UP_TO_CAR_START);
				return;
			}
			bChangedSeat = false;
		} else {
			if (RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_CAR_GETIN_LHS_0)
				|| RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_CAR_GETIN_LHS_0)
				|| RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_CAR_CLOSEDOOR_LHS_0)
				|| RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_CAR_CLOSEDOOR_LHS_0)
				|| RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_CAR_SHUFFLE_RHS_0)
				|| RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_CAR_SHUFFLE_RHS_0)
				|| RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_CAR_CLOSEDOOR_LHS_1)
				|| RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_CAR_CLOSEDOOR_RHS_1)
				|| RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_CAR_GETIN_LHS_1)
				|| RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_CAR_GETIN_RHS_1)
				|| RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_CAR_GETIN_LHS_0)
				|| RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_CAR_GETIN_LHS_0)
				|| RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_CAR_GETIN_LHS_0)) {
				notYet = true;
			}
		}
		if (notYet) {
			LineUpPedWithCar(LINE_UP_TO_CAR_START);
			bChangedSeat = false;
			return;
		}
	}
	CVehicleModelInfo *vehModel = (CVehicleModelInfo *)CModelInfo::GetModelInfo(m_pMyVehicle->GetModelIndex());
	CMatrix newMat(m_pMyVehicle->GetMatrix());
	CVector seatPos;
	if (m_pMyVehicle->pDriver == this) {
		seatPos = vehModel->GetFrontSeatPosn();
		if (!m_pMyVehicle->IsBoat() && !m_pMyVehicle->IsBike())
			seatPos.x = -seatPos.x;

	} else if (m_pMyVehicle->pPassengers[0] == this) {
		seatPos = m_pMyVehicle->IsBike() ? vehModel->m_positions[CAR_POS_BACKSEAT]: vehModel->GetFrontSeatPosn();

	} else if (m_pMyVehicle->pPassengers[1] == this) {
		seatPos = vehModel->m_positions[CAR_POS_BACKSEAT];
		seatPos.x = -seatPos.x;

	} else {
		if (m_pMyVehicle->pPassengers[2] == this) {
			seatPos = vehModel->m_positions[CAR_POS_BACKSEAT];
		} else {
			seatPos = vehModel->GetFrontSeatPosn();
		}
	}
	if (m_pMyVehicle->IsBike()) {
		((CBike*)m_pMyVehicle)->CalculateLeanMatrix();
		newMat = ((CBike*)m_pMyVehicle)->m_leanMatrix;
	}
	newMat.GetPosition() += Multiply3x3(newMat, seatPos);
	// Already done below (SetTranslate(0.0f, 0.0f, 0.0f))
	// tempMat.SetUnity();

	// Rear seats on vans don't face to front, so rotate them HALFPI.
	if (m_pMyVehicle->bIsVan) {
		CMatrix tempMat;
		if (m_pMyVehicle->pPassengers[1] == this) {
			m_fRotationCur = m_pMyVehicle->GetForward().Heading() - HALFPI;
			tempMat.SetTranslate(0.0f, 0.0f, 0.0f);
			tempMat.RotateZ(-HALFPI);
			tempMat.Translate(0.0f, 0.6f, 0.0f);
			newMat = newMat * tempMat;
		} else if (m_pMyVehicle->pPassengers[2] == this) {
			m_fRotationCur = m_pMyVehicle->GetForward().Heading() + HALFPI;
			tempMat.SetTranslate(0.0f, 0.0f, 0.0f);
			tempMat.RotateZ(HALFPI);
			newMat = newMat * tempMat;
		} else {
			m_fRotationCur = m_pMyVehicle->GetForward().Heading();
		}
	} else {
		m_fRotationCur = m_pMyVehicle->GetForward().Heading();
	}
	SetMatrix(newMat);
}

void
CPed::RestoreHeadingRate(void)
{
	m_headingRate = 15.0f;
}

void
CPed::RestoreHeadingRateCB(CAnimBlendAssociation *assoc, void *arg)
{
	((CPed*)arg)->RestoreHeadingRate();
}

void
CPed::FlagToDestroyWhenNextProcessed(void)
{
	bRemoveFromWorld = true;
	if (!InVehicle())
		return;
	if (m_pMyVehicle->pDriver == this){
		m_pMyVehicle->pDriver = nil;
		if (IsPlayer() && m_pMyVehicle->GetStatus() != STATUS_WRECKED)
			m_pMyVehicle->SetStatus(STATUS_ABANDONED);
	}else{
		m_pMyVehicle->RemovePassenger(this);
	}
	bInVehicle = false;
	m_pMyVehicle = nil;

	if (CharCreatedBy == MISSION_CHAR)
		SetPedState(PED_DEAD);
	else
		SetPedState(PED_NONE);
	m_pVehicleAnim = nil;
}

bool
CPed::PossiblyFindBetterPosToSeekCar(CVector *pos, CVehicle *veh)
{
	bool foundIt = false;

	CVector helperPos = GetPosition();
	helperPos.z = pos->z - 0.5f;

	CVector foundPos = *pos;
	foundPos.z -= 0.5f;

	// If there is another car between target car and us.
	if (CWorld::TestSphereAgainstWorld((foundPos + helperPos) / 2.0f, 0.25f, veh, false, true, false, false, false, false)) {

		CColModel *vehCol = veh->GetModelInfo()->GetColModel();
		CVector *colMin = &vehCol->boundingBox.min;
		CVector *colMax = &vehCol->boundingBox.max;

		CVector leftRearPos = CVector(colMin->x - 0.5f, colMin->y - 0.5f, 0.0f);
		CVector rightRearPos = CVector(0.5f + colMax->x, colMin->y - 0.5f, 0.0f);
		CVector leftFrontPos = CVector(colMin->x - 0.5f, 0.5f + colMax->y, 0.0f);
		CVector rightFrontPos = CVector(0.5f + colMax->x, 0.5f + colMax->y, 0.0f);

		leftRearPos = veh->GetMatrix() * leftRearPos;
		rightRearPos = veh->GetMatrix() * rightRearPos;
		leftFrontPos = veh->GetMatrix() * leftFrontPos;
		rightFrontPos = veh->GetMatrix() * rightFrontPos;

		// Makes helperPos veh-ped distance vector.
		helperPos -= veh->GetPosition();

		// ?!? I think it's absurd to use this unless another function like SeekCar finds next pos. with it and we're trying to simulate it's behaviour.
		// On every run it returns another pos. for ped, with same distance to the veh.
		// Sequence of positions are not guaranteed, it depends on global pos. (So sometimes it returns positions to make ped draw circle, sometimes don't)
		helperPos = veh->GetMatrix() * helperPos;

		float vehForwardHeading = veh->GetForward().Heading();

		// I'm absolutely not sure about these namings.
		// NTVF = needed turn if we're looking to vehicle front and wanna look to...

		float potentialLrHeading = Atan2(leftRearPos.x - helperPos.x, leftRearPos.y - helperPos.y);
		float NTVF_LR = CGeneral::LimitRadianAngle(potentialLrHeading - vehForwardHeading);

		float potentialRrHeading = Atan2(rightRearPos.x - helperPos.x, rightRearPos.y - helperPos.y);
		float NTVF_RR = CGeneral::LimitRadianAngle(potentialRrHeading - vehForwardHeading);

		float potentialLfHeading = Atan2(leftFrontPos.x - helperPos.x, leftFrontPos.y - helperPos.y);
		float NTVF_LF = CGeneral::LimitRadianAngle(potentialLfHeading - vehForwardHeading);

		float potentialRfHeading = Atan2(rightFrontPos.x - helperPos.x, rightFrontPos.y - helperPos.y);
		float NTVF_RF = CGeneral::LimitRadianAngle(potentialRfHeading - vehForwardHeading);

		bool canHeadToLr = NTVF_LR <= -PI || NTVF_LR >= -HALFPI;

		bool canHeadToRr = NTVF_RR <= HALFPI || NTVF_RR >= PI;

		bool canHeadToLf = NTVF_LF >= 0.0f || NTVF_LF <= -HALFPI;

		bool canHeadToRf = NTVF_RF <= 0.0f || NTVF_RF >= HALFPI;

		// Only order of conditions are different among enterTypes.
		if (m_vehDoor == CAR_DOOR_RR) {
			if (canHeadToRr) {
				foundPos = rightRearPos;
				foundIt = true;
			} else if (canHeadToRf) {
				foundPos = rightFrontPos;
				foundIt = true;
			} else if (canHeadToLr) {
				foundPos = leftRearPos;
				foundIt = true;
			} else if (canHeadToLf) {
				foundPos = leftFrontPos;
				foundIt = true;
			}
		} else if(m_vehDoor == CAR_DOOR_RF) {
			if (canHeadToRf) {
				foundPos = rightFrontPos;
				foundIt = true;
			} else if (canHeadToRr) {
				foundPos = rightRearPos;
				foundIt = true;
			} else if (canHeadToLf) {
				foundPos = leftFrontPos;
				foundIt = true;
			} else if (canHeadToLr) {
				foundPos = leftRearPos;
				foundIt = true;
			}
		} else if (m_vehDoor == CAR_DOOR_LF) {
			if (canHeadToLf) {
				foundPos = leftFrontPos;
				foundIt = true;
			} else if (canHeadToLr) {
				foundPos = leftRearPos;
				foundIt = true;
			} else if (canHeadToRf) {
				foundPos = rightFrontPos;
				foundIt = true;
			} else if (canHeadToRr) {
				foundPos = rightRearPos;
				foundIt = true;
			}
		} else if (m_vehDoor == CAR_DOOR_LR) {
			if (canHeadToLr) {
				foundPos = leftRearPos;
				foundIt = true;
			} else if (canHeadToLf) {
				foundPos = leftFrontPos;
				foundIt = true;
			} else if (canHeadToRr) {
				foundPos = rightRearPos;
				foundIt = true;
			} else if (canHeadToRf) {
				foundPos = rightFrontPos;
				foundIt = true;
			}
		}
	}
	if (!foundIt)
		return false;

	helperPos = GetPosition() - foundPos;
	helperPos.z = 0.0f;
	if (helperPos.MagnitudeSqr() <= sq(0.5f))
		return false;

	pos->x = foundPos.x;
	pos->y = foundPos.y;
	return true;
}

bool
CPed::CanPedJumpThis(CEntity *unused, CVector *damageNormal)
{
	if (m_nSurfaceTouched == SURFACE_WATER)
		return true;

	CVector pos = GetPosition();
	CVector forwardOffset = GetForward();
	if (damageNormal && damageNormal->z > 0.17f) {
		if (damageNormal->z > 0.9f)
			return false;

		CColModel *ourCol = GetColModel();
		pos.z = ourCol->spheres->center.z - ourCol->spheres->radius * damageNormal->z + pos.z;
		pos.z = pos.z + 0.05f;
		float collPower = damageNormal->Magnitude2D();
		if (damageNormal->z > 0.5f) {
			CVector invDamageNormal(-damageNormal->x, -damageNormal->y, 0.0f);
			invDamageNormal *= 1.0f / collPower;
			CVector estimatedJumpDist = invDamageNormal + collPower * invDamageNormal * ourCol->spheres->radius;
			forwardOffset = estimatedJumpDist * Min(2.0f / collPower, 4.0f);
		} else {
			forwardOffset += collPower * ourCol->spheres->radius * forwardOffset;
		}
	} else {
		pos.z -= 0.15f;
	}

	CVector forwardPos = pos + forwardOffset;
	return CWorld::GetIsLineOfSightClear(pos, forwardPos, true, false, false, true, false, false, false);
}

// SA CTaskComplexJump::CreateSubTask (0x67D980) passes m_bHighJump = true for
// the player: that flag is the only thing that separates an 8.5 from a 4.5
// launch, so the player clears obstacles a pedestrian only hops over.
void
CPed::SetJump(void)
{
	SetJump(IsPlayer());
}

// SA CTaskSimpleJump carries m_bClimbJump alongside m_bHighJump: a jump taken
// while hanging on a wall does not get any horizontal launch, it kicks the ped
// off the surface straight up with the CLIMB_JUMP clip.
void
CPed::SetJump(bool highJump, bool climbJump)
{
	if (!bInVehicle && m_nPedState != PED_JUMP && !RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_JUMP_LAUNCH) &&
		(m_nSurfaceTouched != SURFACE_STEEP_CLIFF || DotProduct(GetForward(), m_vecDamageNormal) >= 0.0f)) {

		SetStoredState();
		SetPedState(PED_JUMP);
		// The launch force belongs to the moment the button is pressed, not to
		// the end of the launch clip. Applying it from the clip's finish
		// callback meant the ped spent the whole 0.2s of JUMP_launch still
		// grounded: it kept the run clip and the run speed, so it slid forward
		// and only then hopped. gta-reversed does the same thing in
		// CTaskSimpleJump::ProcessPed -> Launch: the clip starts, and the force
		// that leaves the ground is applied at once.
		// SA picks the launch clip by where the run cycle happens to be:
		// CTaskSimpleJump::StartLaunchAnim adds 0.367 to the move clip's progress
		// and takes JUMP_launch_R once that passes half a cycle, so the feet
		// leave the ground in the right phase. Always taking JUMP_launch made a
		// running jump restart the walk cycle from a planted foot, which reads
		// as a jump with no animation at all.
		float moveBlendAmount = 0.0f;
		CAnimBlendAssociation *moveAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_SPRINT);
		if (!moveAssoc || moveAssoc->blendAmount < 0.3f)
			moveAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_RUN);
		if (!moveAssoc || moveAssoc->blendAmount < 0.3f)
			moveAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_WALK);
		if (moveAssoc && moveAssoc->blendAmount > 0.3f && moveAssoc->hierarchy &&
		   moveAssoc->hierarchy->totalLength > 0.0f) {
			moveBlendAmount = moveAssoc->currentTime / moveAssoc->hierarchy->totalLength + 0.367f;
			if (moveBlendAmount > 1.0f)
				moveBlendAmount -= 1.0f;
		}

		CAnimBlendAssociation *jumpAssoc = CAnimManager::BlendAnimation(GetClump(), ASSOCGRP_DEFAULT,
			moveBlendAmount >= 0.5f ? ANIM_ID_JUMP_LAUNCH_R : ANIM_ID_JUMP_LAUNCH, 8.0f);
		if (!jumpAssoc) {
			RestorePreviousState(); // no launch clip => no finish callback
			return;
		}
		ApplyLaunchForce(highJump, climbJump);
		// SA CTaskSimpleJump::Launch blends JUMP_GLIDE right after the force is
		// applied, so the ped is already in the airborne pose while the launch
		// clip plays. Without it the body keeps the run pose through the
		// anticipation and only changes once the launch clip ends, which is the
		// "hop with no jump animation" users see.
		CAnimManager::BlendAnimation(GetClump(), ASSOCGRP_DEFAULT,
			climbJump ? ANIM_ID_CLIMB_JUMP : ANIM_ID_JUMP_GLIDE, 8.0f);
		jumpAssoc->SetFinishCallback(FinishLaunchCB, this);
		m_fRotationDest = m_fRotationCur;
	}
}

// The velocity that leaves the ground. Called the instant the jump starts:
// the launch clip is only 0.2s of anticipation, so waiting for its finish
// callback meant the ped kept run speed and its run clip all the way through
// it - it slid, then hopped. The numbers are the SA ones from
// CTaskSimpleJump::Launch, which this port already used for the force itself.
void
CPed::ApplyLaunchForce(void)
{
	ApplyLaunchForce(IsPlayer());
}

void
CPed::ApplyLaunchForce(bool highJump, bool climbJump)
{
	float horizontalSpeed = 0.1f;
	CAnimBlendAssociation *sprintAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_SPRINT);

	// SA: CTaskSimpleJump::Launch interpolates 0.17 -> 0.22 for a sprint and
	// 0.1 -> 0.17 for a run, which is what these two lines already are.
	if (sprintAssoc) {
		horizontalSpeed = 0.05f * sprintAssoc->blendAmount + 0.17f;
	} else {
		CAnimBlendAssociation *runAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_RUN);
		if (runAssoc)
			horizontalSpeed = 0.07f * runAssoc->blendAmount + 0.10f;
	}

	// SA CTaskSimpleJump::Launch: fJumpForce = (ped->IsPlayer() || m_bHighJump)
	// ? 8.5F : 4.5F. m_bHighJump is passed in by the caller, not derived here.
	float jumpForce = (IsPlayer() || highJump) ? 8.5f : 4.5f;

	// SA scales horizontal speed and jump force by 1 + (fat - 50) * -0.1 / 950.
	// With the default fat value of 0, the factor is 1.005263f.
	// reVC keeps no CPed->player-data pointer; IsPlayer() is the same test SA's
	// ped->GetPlayerData() performs, since only the player ped has one.
	if (IsPlayer()) {
		const float modifier = 1.005263f;
		horizontalSpeed *= modifier;
		jumpForce *= modifier;
	}

	ApplyMoveForce(0.0f, 0.0f, jumpForce);

	// SA CTaskSimpleJump::Launch: a climb jump kills the horizontal speed
	// outright - the ped kicks off the wall, it does not run off it.
	if (climbJump) {
		m_vecMoveSpeed.x = 0.0f;
		m_vecMoveSpeed.y = 0.0f;
	} else if (sq(horizontalSpeed) > m_vecMoveSpeed.MagnitudeSqr2D() || m_pCurrentPhysSurface) {

#ifdef FREE_CAM
		if (TheCamera.Cams[0].Using3rdPersonMouseCam() && !CCamera::bFreeCam) {
#else
		if (TheCamera.Cams[0].Using3rdPersonMouseCam()) {
#endif
			float fpsAngle = WorkOutHeadingForMovingFirstPerson(m_fRotationCur);
			m_vecMoveSpeed.x = -horizontalSpeed * Sin(fpsAngle);
			m_vecMoveSpeed.y = horizontalSpeed * Cos(fpsAngle);
		} else {
			m_vecMoveSpeed.x = -horizontalSpeed * Sin(m_fRotationCur);
			m_vecMoveSpeed.y = horizontalSpeed * Cos(m_fRotationCur);
		}

		if (m_pCurrentPhysSurface) {
			m_vecMoveSpeed.x += m_pCurrentPhysSurface->m_vecMoveSpeed.x;
			m_vecMoveSpeed.y += m_pCurrentPhysSurface->m_vecMoveSpeed.y;
		}
	}

	bIsStanding = false;
	bIsInTheAir = true;
}

void
CPed::FinishLaunchCB(CAnimBlendAssociation *animAssoc, void *arg)
{
	CPed *ped = (CPed*)arg;

	if (ped->m_nPedState != PED_JUMP)
		return;

	CVector forward(0.09f * ped->GetForward() + ped->GetPosition());
	forward.z += CModelInfo::GetColModel(ped->GetModelIndex())->spheres[2].center.z + 0.35f;

	CEntity *obstacle = CWorld::TestSphereAgainstWorld(forward, 0.25f, nil, true, true, false, true, false, false);
	if (!obstacle) {
		// Forward of forward
		forward += 0.15f * ped->GetForward();
		forward.z += 0.15f;
		obstacle = CWorld::TestSphereAgainstWorld(forward, 0.25f, nil, true, true, false, true, false, false);
	}

	if (!obstacle && CCullZones::CamStairsForPlayer() && CCullZones::FindZoneWithStairsAttributeForPlayer())
		obstacle = ped;

	if (obstacle) {
		animAssoc->flags |= ASSOC_DELETEFADEDOUT;
		CAnimBlendAssociation *handsCoverAssoc = CAnimManager::BlendAnimation(ped->GetClump(), ASSOCGRP_DEFAULT, ANIM_ID_HIT_WALL, 8.0f);
		if (!handsCoverAssoc) {
			FinishHitHeadCB(nil, ped);
			return;
		}
		handsCoverAssoc->flags &= ~ASSOC_FADEOUTWHENDONE;
		handsCoverAssoc->SetFinishCallback(FinishHitHeadCB, ped);
		ped->bIsLanding = true;
		return;
	}

	ped->bIsStanding = false;
	ped->bIsInTheAir = true;
	animAssoc->blendDelta = -1000.0f;
	CAnimManager::AddAnimation(ped->GetClump(), ASSOCGRP_DEFAULT, ANIM_ID_JUMP_GLIDE);

	if (ped->bDoBloodyFootprints) {
		CVector bloodPos(0.0f, 0.0f, 0.0f);
		ped->TransformToNode(bloodPos, PED_NODE_LEFT_FOOT);

		bloodPos.z -= 0.1f;
		bloodPos += 0.2f * ped->GetForward();

		CShadows::AddPermanentShadow(SHADOWTYPE_DARK, gpBloodPoolTex, &bloodPos,
			0.26f * ped->GetForward().x,
			0.26f * ped->GetForward().y,
			0.14f * ped->GetRight().x,
			0.14f * ped->GetRight().y,
			255, 255, 0, 0, 4.0f, 3000, 1.0f);

		bloodPos = CVector(0.0f, 0.0f, 0.0f);
		ped->TransformToNode(bloodPos, PED_NODE_RIGHT_FOOT);

		bloodPos.z -= 0.1f;
		bloodPos += 0.2f * ped->GetForward();
		CShadows::AddPermanentShadow(SHADOWTYPE_DARK, gpBloodPoolTex, &bloodPos,
			0.26f * ped->GetForward().x,
			0.26f * ped->GetForward().y,
			0.14f * ped->GetRight().x,
			0.14f * ped->GetRight().y,
			255, 255, 0, 0, 4.0f, 3000, 1.0f);

		if (ped->m_bloodyFootprintCountOrDeathTime <= 40) {
			ped->m_bloodyFootprintCountOrDeathTime = 0;
			ped->bDoBloodyFootprints = false;
		} else {
			ped->m_bloodyFootprintCountOrDeathTime -= 40;
		}
	}
}

void
CPed::FinishJumpCB(CAnimBlendAssociation *animAssoc, void *arg)
{
	CPed *ped = (CPed*)arg;

	ped->bResetWalkAnims = true;
	ped->bIsLanding = false;

	animAssoc->blendDelta = -1000.0f;
}

void
CPed::FinishHitHeadCB(CAnimBlendAssociation *animAssoc, void *arg)
{
	CPed *ped = (CPed*)arg;

	if (animAssoc) {
		animAssoc->blendDelta = -4.0f;
		animAssoc->flags |= ASSOC_DELETEFADEDOUT;
	}

	if (ped->m_nPedState == PED_JUMP)
		ped->RestorePreviousState();

	ped->bIsLanding = false;
}

void
CPed::WarpPedIntoCar(CVehicle *car)
{
	bInVehicle = true;
	m_pMyVehicle = car;
	m_pMyVehicle->RegisterReference((CEntity **) &m_pMyVehicle);
	m_carInObjective = car;
	m_carInObjective->RegisterReference((CEntity **) &m_carInObjective);
	SetPedState(PED_DRIVING);
	bUsesCollision = false;
	bIsInTheAir = false;
	bVehExitWillBeInstant = true;
	if (m_objective == OBJECTIVE_ENTER_CAR_AS_DRIVER) {
		car->SetDriver(this);
		car->pDriver->RegisterReference((CEntity **) &car->pDriver);

	} else if (m_objective == OBJECTIVE_ENTER_CAR_AS_PASSENGER) {
		if (car->IsBike() && !car->pPassengers[0]) {
			car->pPassengers[0] = this;
			car->pPassengers[0]->RegisterReference((CEntity**) &car->pPassengers[0]);
		}
		for (int i = 0; i < 4; i++) {
			if (!car->pPassengers[i]) {
				car->pPassengers[i] = this;
				car->pPassengers[i]->RegisterReference((CEntity **) &car->pPassengers[i]);
				break;
			}
		}
	} else
		return;

	if (IsPlayer()) {
		car->SetStatus(STATUS_PLAYER);
		AudioManager.PlayerJustGotInCar();
	} else {
		car->SetStatus(STATUS_PHYSICS);
	}

	CWorld::Remove(this);
	SetPosition(car->GetPosition());
	CWorld::Add(this);

	if (!car->bEngineOn) {
		car->bEngineOn = true;
		DMAudio.PlayOneShot(car->m_audioEntityId, SOUND_CAR_ENGINE_START, 1.0f);
	}

	RpAnimBlendClumpSetBlendDeltas(GetClump(), ASSOC_PARTIAL, -1000.0f);

	AddInCarAnims(car, car->pDriver == this);
	RemoveWeaponWhenEnteringVehicle();

	if (car->bIsBus)
		bRenderPedInCar = false;

	bChangedSeat = true;
}

void
CPed::AttachPedToEntity(CEntity *ent, CVector offset, uint16 type, float rot, eWeaponType weapon)
{
	if (!ent || bInVehicle)
		return;

	m_attachedTo = ent;
	m_attachedTo->RegisterReference(&m_attachedTo);
	m_vecAttachOffset = offset;
	m_attachType = type;
	m_attachRotStep = rot;
	if (IsPlayer()) {
		bUsesCollision = false;
	} else if (ent->IsVehicle()) {
		m_pCollidingEntity = ent;
	}

	if (IsPlayer()) {
		m_objective = OBJECTIVE_NONE;
		m_prevObjective = OBJECTIVE_NONE;
	}
	SetStoredState();
	SetPedState(PED_IDLE);
	CAnimManager::BlendAnimation(GetClump(), m_animGroup, ANIM_ID_IDLE, 1000.0f);

	if (m_storedWeapon == WEAPONTYPE_UNIDENTIFIED) {
		m_storedWeapon = GetWeapon()->m_eWeaponType;
		m_attachWepAmmo = GetWeapon()->m_nAmmoTotal;
	}
	if (IsPlayer()) {
		GiveWeapon(weapon, 30000, 1);
#ifndef FIX_BUGS
		((CPlayerPed*)this)->m_nSelectedWepSlot = weapon;
#else
		((CPlayerPed*)this)->m_nSelectedWepSlot = GetWeaponSlot(weapon);
#endif
		((CPlayerPed*)this)->MakeChangesForNewWeapon(weapon);
		TheCamera.SetNewPlayerWeaponMode(CCam::MODE_HELICANNON_1STPERSON, 0, 0);
		SetPedState(PED_SNIPER_MODE);
	} else {
		GiveWeapon(weapon, 30000, true);
		SetCurrentWeapon(weapon);
	}

	PositionAttachedPed();
}

void
CPed::DettachPedFromEntity(void)
{
	CEntity* pVehicleAttachedTo = m_attachedTo;
	m_attachedTo = nil;
	if (m_nPedState == PED_DIE) {
		m_pCollidingEntity = pVehicleAttachedTo;
		ApplyMoveForce(pVehicleAttachedTo->GetForward() * -4.0f + CVector(0.0f, 0.0f, 4.0f));
		bIsStanding = false;
	} else if (m_nPedState != PED_DEAD) {
		RestorePreviousState();
		CAnimManager::BlendAnimation(GetClump(), m_animGroup, ANIM_ID_IDLE, 1000.0f);
		bUsesCollision = true;
		if (m_storedWeapon != WEAPONTYPE_UNIDENTIFIED) {
			GetWeapon()->m_nAmmoInClip = 0;
			GetWeapon()->m_nAmmoTotal = 0;
			SetCurrentWeapon(m_storedWeapon);
			GetWeapon()->m_nAmmoTotal = m_attachWepAmmo;
			m_storedWeapon = WEAPONTYPE_UNIDENTIFIED;
		}
	}
}

void
CPed::PositionAttachedPed()
{
	if(!m_attachedTo)
		return;

	CMatrix rotMatrix, targetMat;
	targetMat = m_attachedTo->GetMatrix();
	targetMat.GetPosition() += Multiply3x3(m_attachedTo->GetMatrix(), m_vecAttachOffset);
	float objAngle = m_attachedTo->GetForward().Heading();

	if (!IsPlayer()) {
		float targetAngle = objAngle;
		switch (m_attachType) {
			case 1:
				targetAngle += HALFPI;
				break;
			case 2:
				targetAngle += PI;
				break;
			case 3:
				targetAngle -= HALFPI;
				break;
			default:
				break;
		}
		targetAngle = CGeneral::LimitRadianAngle(targetAngle);
		m_fRotationCur = CGeneral::LimitRadianAngle(m_fRotationCur);
		float neededTurn = m_fRotationCur - targetAngle;

		if (neededTurn > PI)
			neededTurn -= TWOPI;
		else if (neededTurn < -PI)
			neededTurn += TWOPI;

		if (neededTurn > m_attachRotStep)
			m_fRotationCur = CGeneral::LimitRadianAngle(targetAngle + m_attachRotStep);
		else if (-m_attachRotStep > neededTurn)
			m_fRotationCur = CGeneral::LimitRadianAngle(targetAngle - m_attachRotStep);
		else
			m_fRotationCur = CGeneral::LimitRadianAngle(m_fRotationCur);
	}
	rotMatrix.SetRotateZ(m_fRotationCur - objAngle);
	targetMat = targetMat * rotMatrix;
	GetMatrix() = targetMat;
	if (m_attachedTo->IsVehicle() || m_attachedTo->IsObject()) {
		m_vecMoveSpeed = ((CPhysical*)m_attachedTo)->m_vecMoveSpeed;
		m_vecTurnSpeed = ((CPhysical*)m_attachedTo)->m_vecTurnSpeed;
	}
}

void
CPed::Undress(const char* name)
{
	int mi = GetModelIndex();
	CAnimBlendAssociation* pAnim = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_PHONE_OUT);
	if (pAnim)
		FinishTalkingOnMobileCB(pAnim, this);

	DeleteRwObject();
	if (IsPlayer())
		mi = MI_PLAYER;
	CStreaming::RequestSpecialModel(mi, name, STREAMFLAGS_DEPENDENCY | STREAMFLAGS_SCRIPTOWNED);
	CWorld::Remove(this);
}

void
CPed::Dress(void)
{
	int mi = GetModelIndex();
	m_modelIndex = -1;
	SetModelIndex(mi);
	m_nPedState = PED_IDLE;
	m_nLastPedState = PED_NONE;
	m_objective = OBJECTIVE_NONE;
	m_prevObjective = OBJECTIVE_NONE;
	CWorld::Add(this);
	RestoreHeadingRate();
}

void
CPed::Say(uint16 audio, int32 time)
{
	if (m_delayedSoundID == -1) {
		m_delayedSoundID = audio;
		m_delayedSoundTimer = CTimer::GetTimeInMilliseconds() + time;
	}
}

// Corresponds to ped sounds (from SOUND_PED_DEATH to SOUND_PED_TAXI_CALL)
PedAudioData CommentWaitTime[56] = {
  { 500, 800, 500, 2 },
  { 500, 800, 500, 2 },
  { 500, 800, 500, 2 },
  { 500, 800, 500, 2 },
  { 100, 2, 100, 2 },
  { 500, 500, 2000, 1000 },
  { 2000, 50, 2050, 1000 },
  { 5000, 2000, 7000, 3000 },
  { 5000, 2000, 7000, 3000 },
  { 300, 200, 500, 200 },
  { 3000, 1000, 4000, 1000 },
  { 6000, 6000, 6000, 6000 },
  { 4000, 1000, 5000, 1000 },
  { 3000, 1000, 4000, 1000 },
  { 1000, 1000, 2000, 2000 },
  { 1000, 500, 2000, 1500 },
  { 1700, 1000, 3000, 1000 },
  { 800, 200, 1000, 500 },
  { 800, 200, 1000, 500 },
  { 800, 400, 2000, 1000 },
  { 800, 400, 2000, 1000 },
  { 2000, 2000, 4000, 4000 },
  { 2000, 2000, 4000, 1000 },
  { 4000, 1000, 5000, 1000 },
  { 800, 400, 1200, 500 },
  { 5000, 1000, 6000, 2000 },
  { 5000, 1000, 6000, 2000 },
  { 5000, 1000, 6000, 2000 },
  { 5000, 1000, 6000, 2000 },
  { 5000, 1000, 6000, 2000 },
  { 5000, 1000, 6000, 2000 },
  { 5000, 1000, 6000, 2000 },
  { 4000, 2000, 7000, 2000 },
  { 1000, 300, 2000, 1000 },
  { 1500, 1000, 2500, 1000 },
  { 200, 200, 200, 200 },
  { 2000, 1000, 4000, 1000 },
  { 2000, 1000, 4000, 1000 },
  { 1000, 500, 3000, 1000 },
  { 1000, 500, 1000, 1000 },
  { 3000, 2000, 5000, 1000 },
  { 3000, 2000, 5000, 1000 },
  { 3000, 2000, 3000, 2000 },
  { 2000, 1000, 3000, 1000 },
  { 2500, 1000, 5000, 5000 },
  { 2000, 1000, 3000, 2000 },
  { 4000, 1000, 5000, 1000 },
  { 1000, 500, 2000, 4000 },
  { 1000, 500, 2000, 5000 },
  { 2000, 500, 2500, 500 },
  { 1000, 500, 3000, 2000 },
  { 1600, 1000, 2000, 2000 },
  { 2000, 1000, 4000, 2000 },
  { 1500, 1000, 2500, 1000 },
  { 1000, 1000, 5000, 5000 },
  { 0, 0, 0, 0 }
};

bool
CPed::ServiceTalkingWhenDead(void)
{
	return m_queuedSound == SOUND_PED_DEATH;
}

void
CPed::ServiceTalking(void)
{
	if (bBodyPartJustCameOff && m_bodyPartBleeding == PED_NODE_HEAD)
		return;

	if (!CGame::germanGame && m_pFire)
		m_queuedSound = SOUND_PED_BURNING;

	if (m_queuedSound != SOUND_NO_SOUND) {
		if (m_queuedSound == SOUND_PED_DEATH)
			m_soundStart = CTimer::GetTimeInMilliseconds() - 1;

		if (CTimer::GetTimeInMilliseconds() > m_soundStart) {
			DMAudio.PlayOneShot(m_audioEntityId, m_queuedSound, 1.0f);
			m_lastSoundStart = CTimer::GetTimeInMilliseconds();
			m_soundStart =
				CommentWaitTime[m_queuedSound - SOUND_PED_DEATH].m_nFixedDelayTime
				+ CTimer::GetTimeInMilliseconds()
				+ CGeneral::GetRandomNumberInRange(0, CommentWaitTime[m_queuedSound - SOUND_PED_DEATH].m_nOverrideFixedDelayTime);

			if (m_queuedSound == SOUND_PED_PLAYER_BEFORESEX && IsPlayer())
				m_soundStart += 2000;

			m_lastQueuedSound = m_queuedSound;
			m_queuedSound = SOUND_NO_SOUND;
		}
	}
}

void
CPed::Say(uint16 audio)
{
	if (3.0f + TheCamera.GetPosition().z < GetPosition().z)
		return;

	if (TheCamera.m_CameraAverageSpeed > 1.65f) {
		if (audio != SOUND_PED_DAMAGE && audio != SOUND_PED_HIT && audio != SOUND_PED_LAND)
			return;

	} else if (TheCamera.m_CameraAverageSpeed > 1.25f) {
		if (audio != SOUND_PED_DEATH &&
			audio != SOUND_PED_DAMAGE && audio != SOUND_PED_HIT && audio != SOUND_PED_LAND &&
			audio != SOUND_PED_TAXI_WAIT && audio != SOUND_PED_EVADE)
			return;

	} else if (TheCamera.m_CameraAverageSpeed > 0.9f) {
		switch (audio) {
			case SOUND_PED_DEATH:
			case SOUND_PED_DAMAGE:
			case SOUND_PED_HIT:
			case SOUND_PED_LAND:
			case SOUND_PED_BURNING:
			case SOUND_PED_FLEE_SPRINT:
			case SOUND_PED_TAXI_WAIT:
			case SOUND_PED_EVADE:
			case SOUND_PED_CRASH_VEHICLE:
			case SOUND_PED_CRASH_CAR:
			case SOUND_PED_ANNOYED_DRIVER:
				break;
			default:
				return;
		}
	}

	if (audio < m_queuedSound) {
		if (audio != m_lastQueuedSound || audio == SOUND_PED_DEATH

		// See VC Ped Speech patch
#ifdef FIX_BUGS
			|| CommentWaitTime[audio - SOUND_PED_DEATH].m_nOverrideMaxRandomDelayTime
				+ (uint32)CGeneral::GetRandomNumberInRange(0, CommentWaitTime[audio - SOUND_PED_DEATH].m_nMaxRandomDelayTime)
#else
			|| CommentWaitTime[m_queuedSound - SOUND_PED_DEATH].m_nOverrideMaxRandomDelayTime
			+ (uint32)CGeneral::GetRandomNumberInRange(0, CommentWaitTime[m_queuedSound - SOUND_PED_DEATH].m_nMaxRandomDelayTime)
#endif
				+ m_lastSoundStart <= CTimer::GetTimeInMilliseconds()) {
			m_queuedSound = audio;
		}
	}
}

// ---------------------------------------------------------------------------
// Vehicle entry/exit, seek and the transport objective core.
// ---------------------------------------------------------------------------
// The player's transport objectives. LEAVE_CAR, LEAVE_CAR_AND_DIE and the two
// ENTER_CAR values are temporary: they run on top of whatever the ped was doing
// and hand control back when they complete or become impossible.

bool
CPed::IsTemporaryObjective(eObjective objective)
{
	return objective == OBJECTIVE_LEAVE_CAR || objective == OBJECTIVE_LEAVE_CAR_AND_DIE
		|| objective == OBJECTIVE_ENTER_CAR_AS_DRIVER || objective == OBJECTIVE_ENTER_CAR_AS_PASSENGER;
}

void
CPed::SetObjective(eObjective newObj)
{
	if (DyingOrDead() || m_attachedTo)
		return;

	if (newObj == OBJECTIVE_NONE) {
		m_objective = OBJECTIVE_NONE;
		m_prevObjective = OBJECTIVE_NONE;
	} else if (m_prevObjective != newObj || m_prevObjective == OBJECTIVE_NONE) {
		if (m_objective == newObj)
			return;

		if (IsTemporaryObjective(m_objective)) {
			m_prevObjective = newObj;
		} else {
			m_objective = newObj;
		}
		bObjectiveCompleted = false;
	}
}

void
CPed::SetObjective(eObjective newObj, void *entity)
{
	if (DyingOrDead())
		return;

	if (m_prevObjective == newObj && m_prevObjective != OBJECTIVE_NONE)
		return;

	if (entity == this)
		return;

	if (m_objective == newObj) {
		switch (newObj) {
			case OBJECTIVE_LEAVE_CAR:
			case OBJECTIVE_LEAVE_CAR_AND_DIE:
				return;
			case OBJECTIVE_ENTER_CAR_AS_PASSENGER:
			case OBJECTIVE_ENTER_CAR_AS_DRIVER:
				if (m_carInObjective == entity)
					return;
				break;
			default:
				break;
		}
	} else if ((newObj == OBJECTIVE_LEAVE_CAR || newObj == OBJECTIVE_LEAVE_CAR_AND_DIE) && !bInVehicle)
		return;

	bObjectiveCompleted = false;
	ClearPointGunAt();
	if (m_objective != newObj) {
		// A temporary objective never becomes the one to return to; a
		// non-temporary one is kept unless an enter-car objective replaces it.
		if (!IsTemporaryObjective(m_objective)
			&& newObj != OBJECTIVE_ENTER_CAR_AS_DRIVER && newObj != OBJECTIVE_ENTER_CAR_AS_PASSENGER)
			m_prevObjective = m_objective;
		m_objective = newObj;
	}

	switch (newObj) {
		case OBJECTIVE_LEAVE_CAR:
		case OBJECTIVE_LEAVE_CAR_AND_DIE:
			m_carInObjective = (CVehicle*)entity;
			m_carInObjective->RegisterReference((CEntity **)&m_carInObjective);
			if (m_carInObjective->bIsBus && m_leaveCarTimer == 0) {
				// Bus passengers file out one after another.
				for (int i = 0; i < m_carInObjective->m_nNumMaxPassengers; i++) {
					if (m_carInObjective->pPassengers[i] == this) {
						m_leaveCarTimer = CTimer::GetTimeInMilliseconds() + 1200 * i;
						break;
					}
				}
			}
			break;
		case OBJECTIVE_ENTER_CAR_AS_PASSENGER:
		case OBJECTIVE_ENTER_CAR_AS_DRIVER:
			if (m_nMoveState == PEDMOVE_STILL)
				SetMoveState(PEDMOVE_RUN);
			m_carInObjective = (CVehicle*)entity;
			m_carInObjective->RegisterReference((CEntity**)&m_carInObjective);
			m_pSeekTarget = m_carInObjective;
			m_pSeekTarget->RegisterReference((CEntity**)&m_pSeekTarget);
			m_vecSeekPos = CVector(0.0f, 0.0f, 0.0f);
			break;
		default:
			break;
	}
}

void
CPed::ClearObjective(void)
{
	if (IsPedInControl() || m_nPedState == PED_DRIVING) {
		m_objective = OBJECTIVE_NONE;
		m_carInObjective = nil;

		if (m_nPedState == PED_DRIVING && m_pMyVehicle) {
			if (m_pMyVehicle->pDriver != this)
				SetObjective(OBJECTIVE_LEAVE_CAR, m_pMyVehicle);
			m_nLastPedState = PED_NONE;
		} else {
			SetIdle();
			SetMoveState(PEDMOVE_STILL);
		}
	} else {
		bClearObjective = true;
	}
}

void
CPed::RestorePreviousObjective(void)
{
	if (m_objective == OBJECTIVE_NONE)
		return;

	m_objective = m_prevObjective;
	m_prevObjective = OBJECTIVE_NONE;
	bObjectiveCompleted = false;
}

void
CPed::ProcessObjective(void)
{
	if (bClearObjective && (IsPedInControl() || m_nPedState == PED_DRIVING)) {
		ClearObjective();
		bClearObjective = false;
	}

	if (m_objective == OBJECTIVE_NONE || !(IsPedInControl() || m_nPedState == PED_DRIVING))
		return;

	CVector distWithTarget;
	if (m_carInObjective) {
		if (bInVehicle && m_pMyVehicle)
			distWithTarget = m_carInObjective->GetPosition() - m_pMyVehicle->GetPosition();
		else
			distWithTarget = m_carInObjective->GetPosition() - GetPosition();
	}

	switch (m_objective) {
		case OBJECTIVE_LEAVE_CAR_AND_DIE:
			if (CTimer::GetTimeInMilliseconds() > m_leaveCarTimer) {
				if (InVehicle() && m_nPedState != PED_EXIT_CAR && m_nPedState != PED_DRAGGED_FROM_CAR) {
					if (m_pMyVehicle->IsBoat())
						SetExitBoat(m_pMyVehicle);
					else if (m_pMyVehicle->bIsBus)
						SetExitCar(m_pMyVehicle, 0);
					else {
						// Taken out through the door of the seat, like a jacked driver.
						eCarNodes doorNode = CAR_DOOR_LF;
						if (m_pMyVehicle->pDriver != this) {
							if (m_pMyVehicle->pPassengers[0] == this)
								doorNode = CAR_DOOR_RF;
							else if (m_pMyVehicle->pPassengers[1] == this)
								doorNode = CAR_DOOR_LR;
							else if (m_pMyVehicle->pPassengers[2] == this)
								doorNode = CAR_DOOR_RR;
						}
						SetBeingDraggedFromCar(m_pMyVehicle, doorNode, false);
					}
				}
			}
			break;
		case OBJECTIVE_LEAVE_CAR:
			if (CTimer::GetTimeInMilliseconds() > m_leaveCarTimer) {
				// The player holding the accelerator cancels the exit.
				if (InVehicle() && (FindPlayerPed() != this || !CPad::GetPad(0)->GetAccelerate())) {
					if (m_nPedState != PED_EXIT_CAR && m_nPedState != PED_DRAGGED_FROM_CAR) {
						if (m_pMyVehicle->IsBoat())
							SetExitBoat(m_pMyVehicle);
						else
							SetExitCar(m_pMyVehicle, 0);
					}
				} else
					RestorePreviousObjective();
			}
			break;
		case OBJECTIVE_ENTER_CAR_AS_PASSENGER:
			if (m_carInObjective && !bInVehicle && m_carInObjective->m_nNumPassengers >= m_carInObjective->m_nNumMaxPassengers) {
				RestorePreviousObjective();
				RestorePreviousState();
				if (IsPedInControl())
					m_pMyVehicle = nil;
				break;
			}
			// fall through
		case OBJECTIVE_ENTER_CAR_AS_DRIVER:
			if (!m_carInObjective || bInVehicle) {
				if (bInVehicle && m_pMyVehicle != m_carInObjective) {
					// Seated somewhere else: leave that car first.
					SetExitCar(m_pMyVehicle, 0);
				} else {
					bObjectiveCompleted = true;
					RestorePreviousState();
				}
			} else if (m_leaveCarTimer > CTimer::GetTimeInMilliseconds()) {
				SetMoveState(PEDMOVE_STILL);
			} else if (IsPedInControl()) {
				if (m_carInObjective->IsUpsideDown() && m_carInObjective->m_nVehicleType != VEHICLE_TYPE_BIKE) {
					RestorePreviousObjective();
					RestorePreviousState();
					return;
				}
				if (!m_carInObjective->IsBoat() || m_nPedState == PED_SEEK_BOAT_POSITION) {
					if (m_nPedState != PED_SEEK_CAR)
						SetSeekCar(m_carInObjective, 0);
				} else
					SetSeekBoatPosition(m_carInObjective);
				if (m_nMoveState == PEDMOVE_STILL && !bVehEnterDoorIsBlocked)
					SetMoveState(PEDMOVE_RUN);

				if (!bInVehicle && (m_carInObjective->m_fHealth <= 0.0f || ENTER_CAR_MAX_DIST < distWithTarget.Magnitude())) {
					RestorePreviousObjective();
					RestorePreviousState();
					if (IsPedInControl())
						m_pMyVehicle = nil;
				}
			}
			break;
		default:
			break;
	}

	if (bObjectiveCompleted) {
		RestorePreviousObjective();
		if (IsPedInControl())
			RestorePreviousState();
		ClearAimFlag();
		ClearLookFlag();
	}
}

// The animation a vehicle's occupant plays, resolved the way SA resolves it.
//
// The model names one of the 30 vehicle animation groups; that group holds two
// candidate AssocGroupIds and eighteen flags that decide, per animation, which
// of the two is used. So the answer is not "the model's group" - it is
// CVehicleAnimGroupData::GetGroupForAnim(model's group, this specific clip).
// A model whose group is not in the table falls back to the standard car set.
static AssocGroupId
GetVehicleAnimGroup(CVehicle *veh, AnimationId animId)
{
	if(veh == nil)
		return ASSOCGRP_STDCARAMIMS;
	// The index into the 30-group table lives in the handling record, not in
	// the model: handling.cfg names the group on the vehicle's own line, so a
	// car and a van built on the same model animate from different banks.
	const int32 groupId = veh->GetAnimGroupId();
	if(groupId >= CVehicleAnimGroupData::NUM_VEH_ANIM_GROUPS)
		return ASSOCGRP_STDCARAMIMS;
	return CVehicleAnimGroupData::GetGroupForAnim((AssocGroupId)groupId, animId);
}

CVector vecPedCarDoorAnimOffset;
CVector vecPedCarDoorLoAnimOffset;
CVector vecPedVanRearDoorAnimOffset;
CVector vecPedQuickDraggedOutCarAnimOffset;
CVector vecPedDraggedOutCarAnimOffset;
CVector vecPedStdBikeJumpRhsAnimOffset;
CVector vecPedVespaBikeJumpRhsAnimOffset;
CVector vecPedHarleyBikeJumpRhsAnimOffset;
CVector vecPedDirtBikeJumpRhsAnimOffset;
CVector vecPedBikeKickAnimOffset;

static CVector GetAnimationLastTranslation(AssocGroupId groupId, AnimationId animId, RpClump *clump = nil);
static CVector GetPedAnimationOffset(CPed *ped, AssocGroupId groupId, AnimationId animId, const CVector &nativeOffset);
static bool FindZCoorForPed(CVector *pos);

// Ground height under a ped being placed: two vertical lines a bit apart,
// the higher hit wins. Returns false when nothing was hit within 100 units.
static bool
FindZCoorForPed(CVector *pos)
{
	float startZ = pos->z - 100.0f;
	float foundColZ = -100.0f;
	float foundColZ2 = -100.0f;
	CColPoint foundCol;
	CEntity* foundEnt;

	CVector vec(pos->x, pos->y, pos->z + 1.0f);

	if (CWorld::ProcessVerticalLine(vec, startZ, foundCol, foundEnt, true, false, false, false, true, false, nil))
		foundColZ = foundCol.point.z;

	vec.x += 0.1f;
	vec.y += 0.1f;

	if (CWorld::ProcessVerticalLine(vec, startZ, foundCol, foundEnt, true, false, false, false, true, false, nil))
		foundColZ2 = foundCol.point.z;

	float zForPed = Max(foundColZ, foundColZ2);
	if (zForPed > -99.0f) {
		pos->z = FEET_OFFSET + zForPed;
		return true;
	}
	return false;
}

bool
CPed::CanSeeEntity(CEntity *entity, float threshold)
{
	float neededAngle = CGeneral::GetRadianAngleBetweenPoints(
		entity->GetPosition().x,
		entity->GetPosition().y,
		GetPosition().x,
		GetPosition().y);

	if (neededAngle < 0.0f)
		neededAngle += TWOPI;
	else if (neededAngle > TWOPI)
		neededAngle -= TWOPI;

	float ourAngle = m_fRotationCur;
	if (ourAngle < 0.0f)
		ourAngle += TWOPI;
	else if (ourAngle > TWOPI)
		ourAngle -= TWOPI;

	float neededTurn = Abs(neededAngle - ourAngle);

	return neededTurn < threshold || TWOPI - threshold < neededTurn;
}

// A parallel SA dictionary can omit individual car clips. Never dereference a
// missing association, and never strand the pedestrian in PED_ENTER_CAR just
// because the clip is absent: when the whole entry sequence cannot be animated,
// put him straight into the seat instead. That keeps cars usable with a partial
// SA animation set, which previously only let bikes work.
static bool
ContinueVehicleEntryWithAnimation(CPed *ped, void (*finish)(CAnimBlendAssociation*, void*), const char *stage)
{
	CAnimBlendAssociation *anim = ped->m_pVehicleAnim;
	if(anim == nil){
		CVehicle *veh = ped->m_pMyVehicle;
		if(ped->IsPlayer())
			REVC_LOG_WARNING("Vehicle entry: missing %s animation (ped model=%d, vehicle model=%d, door=%d), seating directly",
				stage, ped->GetModelIndex(), veh ? veh->GetModelIndex() : -1, ped->m_vehDoor);
		if(veh && !veh->IsBike() && veh->IsAutomobile() && ped->m_fHealth > 0.0f){
			// PedSetInCarCB ignores its association argument, so it can drive the
			// seat transition without any clip playing.
			ped->m_pVehicleAnim = nil;
			ped->PedSetInCarCB(nil, ped);
			return false;
		}
		ped->QuitEnteringCar();
		return false;
	}
	if(ped->IsPlayer())
		REVC_LOG_INFO("Vehicle entry: %s animation %d (door=%d)", stage, anim->animId, ped->m_vehDoor);
	anim->SetFinishCallback(finish, ped);
	return true;
}

void
CPed::PedAnimAlignCB(CAnimBlendAssociation *animAssoc, void *arg)
{
	CPed *ped = (CPed*)arg;

	CVehicle *veh = ped->m_pMyVehicle;
	if (animAssoc)
		animAssoc->blendDelta = -1000.0f;

	if (!ped->IsNotInWreckedVehicle())
		return;

	if (!ped->EnteringCar()) {
		if (ped->m_nPedState != PED_DRIVING)
			ped->QuitEnteringCar();

		return;
	}

	if (!ped->m_vehDoor) {
		ped->QuitEnteringCar();
		return;
	}

	if (ped->m_fHealth == 0.0f) {
		ped->QuitEnteringCar();
		return;
	}
	bool itsVan = !!veh->bIsVan;
	bool itsBus = !!veh->bIsBus;
	bool itsLow = !!veh->bLowVehicle;
	eDoors enterDoor;

	switch (ped->m_vehDoor) {
		case CAR_DOOR_RF:
			itsVan = false;
			enterDoor = DOOR_FRONT_RIGHT;
			break;
		case CAR_DOOR_RR:
			enterDoor = DOOR_REAR_RIGHT;
			break;
		case CAR_DOOR_LF:
			itsVan = false;
			enterDoor = DOOR_FRONT_LEFT;
			break;
		case CAR_DOOR_LR:
			enterDoor = DOOR_REAR_LEFT;
			break;
		default:
			break;
	}

	if (veh->IsBike()) {
		CPed *pedToDragOut = nil;
		if (veh->GetStatus() == STATUS_ABANDONED) {
			if (ped->m_vehDoor == CAR_WINDSCREEN) {
				ped->m_pVehicleAnim = CAnimManager::BlendAnimation(ped->GetClump(), ((CBike*)veh)->m_bikeAnimType, ANIM_ID_CAR_GETIN_BIKE_FRONT, 6.0f);
				if(!ContinueVehicleEntryWithAnimation(ped, PedAnimGetInCB, "get in"))
					return;
				((CBike*)veh)->bIsBeingPickedUp = true;

			} else if (veh->GetRight().z >= 0.5f || veh->GetRight().z <= -0.5f || veh->GetUp().z <= 0.0f) {
				if (enterDoor == DOOR_FRONT_LEFT || enterDoor == DOOR_REAR_LEFT) {
					if (veh->GetRight().z > 0.0f)
						ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), ASSOCGRP_DEFAULT, ANIM_ID_BIKE_PICKUPR);
					else
						ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), ASSOCGRP_DEFAULT, ANIM_ID_BIKE_PULLUPR);

				} else {
					if (veh->GetRight().z < 0.0f)
						ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), ASSOCGRP_DEFAULT, ANIM_ID_BIKE_PICKUPL);
					else
						ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), ASSOCGRP_DEFAULT, ANIM_ID_BIKE_PULLUPL);
				}
				if(!ContinueVehicleEntryWithAnimation(ped, PedAnimDoorOpenCB, "open door"))
					return;

			} else {
				ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), ((CBike*)veh)->m_bikeAnimType,
					enterDoor == DOOR_FRONT_LEFT || enterDoor == DOOR_REAR_LEFT ? ANIM_ID_CAR_GETIN_LHS_0 : ANIM_ID_CAR_GETIN_RHS_0);
				if(!ContinueVehicleEntryWithAnimation(ped, PedAnimGetInCB, "get in"))
					return;
				((CBike*)veh)->bIsBeingPickedUp = true;
			}
		} else if (ped->m_vehDoor == CAR_WINDSCREEN) {
			if (veh->pDriver->m_nPedState != PED_DRIVING || veh->pDriver->bDontDragMeOutCar) {
				ped->QuitEnteringCar();
			} else {
				ped->m_pVehicleAnim = CAnimManager::BlendAnimation(ped->GetClump(), ((CBike*)veh)->m_bikeAnimType, ANIM_ID_CAR_GETIN_BIKE_FRONT, 6.0f);
				if(!ContinueVehicleEntryWithAnimation(ped, PedAnimGetInCB, "get in"))
					return;
				pedToDragOut = veh->pDriver;
			}
			((CBike*)veh)->bIsBeingPickedUp = true;
		} else {
			if (enterDoor == DOOR_FRONT_LEFT || enterDoor == DOOR_FRONT_RIGHT) {
				if (veh->pDriver) {
					if (veh->m_vecMoveSpeed.Magnitude() > 0.2f) {
						ped->QuitEnteringCar();
						ped->SetFall(1000, ped->m_vehDoor == CAR_DOOR_LF || ped->m_vehDoor == CAR_DOOR_LR ? ANIM_ID_KO_SPIN_L : ANIM_ID_KO_SPIN_R, false);
						return;
					}
					if (veh->pDriver->m_nPedState != PED_DRIVING || veh->pDriver->bDontDragMeOutCar) {
						ped->QuitEnteringCar();
					} else {
						ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), ASSOCGRP_DEFAULT, enterDoor == DOOR_FRONT_LEFT ? ANIM_ID_BIKE_ELBOWR : ANIM_ID_BIKE_ELBOWL);
						if(!ContinueVehicleEntryWithAnimation(ped, PedAnimPullPedOutCB, "pull occupant out"))
							return;
						pedToDragOut = veh->pDriver;
					}
					((CBike*)veh)->bIsBeingPickedUp = true;

				} else {
					ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), ((CBike*)veh)->m_bikeAnimType, enterDoor == DOOR_FRONT_LEFT ? ANIM_ID_CAR_GETIN_LHS_0 : ANIM_ID_CAR_GETIN_RHS_0);
					if(!ContinueVehicleEntryWithAnimation(ped, PedAnimGetInCB, "get in"))
						return;
					((CBike*)veh)->bIsBeingPickedUp = true;
				}
			} else {
				if (veh->pPassengers[0]) {
					if (veh->m_vecMoveSpeed.Magnitude() > 0.2f) {
						ped->QuitEnteringCar();
						ped->SetFall(1000, ped->m_vehDoor == CAR_DOOR_LF || ped->m_vehDoor == CAR_DOOR_LR ? ANIM_ID_KO_SPIN_L : ANIM_ID_KO_SPIN_R, false);
						return;
					}
					if (veh->pPassengers[0]->m_nPedState != PED_DRIVING || veh->pPassengers[0]->bDontDragMeOutCar) {
						ped->QuitEnteringCar();
					} else {
						ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), ASSOCGRP_DEFAULT,
							enterDoor == DOOR_REAR_LEFT ? ANIM_ID_BIKE_ELBOWR : ANIM_ID_BIKE_ELBOWL);
						if(!ContinueVehicleEntryWithAnimation(ped, PedAnimPullPedOutCB, "pull occupant out"))
							return;
						pedToDragOut = veh->pPassengers[0];
					}
					((CBike*)veh)->bIsBeingPickedUp = true;

				} else {
					ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(),
						((CBike*)veh)->m_bikeAnimType, enterDoor == DOOR_REAR_LEFT ? ANIM_ID_CAR_GETIN_LHS_0 : ANIM_ID_CAR_GETIN_RHS_0);
					if(!ContinueVehicleEntryWithAnimation(ped, PedAnimGetInCB, "get in"))
						return;
					((CBike*)veh)->bIsBeingPickedUp = true;
				}
			}
		}

		if (pedToDragOut) {
			pedToDragOut->SetBeingDraggedFromCar(veh, ped->m_vehDoor, false);
			if (pedToDragOut == veh->pDriver) {
				if (veh->pPassengers[0])
					veh->pPassengers[0]->SetBeingDraggedFromCar(veh, CAR_DOOR_LR, false);
			}
		}
		return;
	}

	if (veh->IsDoorMissing(enterDoor) || veh->IsDoorFullyOpen(enterDoor)) {

		if (ped->m_nPedState == PED_CARJACK || veh->m_nNumMaxPassengers == 0 && veh->pDriver && enterDoor == DOOR_FRONT_RIGHT) {
			ped->PedAnimDoorOpenCB(nil, ped);
			return;
		}
		// The body of the car decides which set the occupant climbs through: a
		// bus and a van have their own step, a lowrider sits lower than a
		// saloon. Everything comes from the model so the entry and the exit of
		// the same car agree.
		// The group is resolved per clip, not per model: the same car can take
		// one animation from its first candidate group and the next from its
		// second, depending on the eighteen flags of the `^` line.
		if (enterDoor != DOOR_FRONT_LEFT && enterDoor != DOOR_REAR_LEFT) {
			if (itsVan)
				ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), ASSOCGRP_VANCARANIMS, ANIM_ID_CAR_GETIN_RHS_1);
			else
				ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(),
					GetVehicleAnimGroup(veh, ANIM_ID_CAR_GETIN_RHS_0), ANIM_ID_CAR_GETIN_RHS_0);
		} else if (itsVan) {
			ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), ASSOCGRP_VANCARANIMS, ANIM_ID_CAR_GETIN_LHS_1);
		} else {
			ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(),
				GetVehicleAnimGroup(veh, ANIM_ID_CAR_GETIN_LHS_0), ANIM_ID_CAR_GETIN_LHS_0);
		}
		if(!ContinueVehicleEntryWithAnimation(ped, PedAnimGetInCB, "get in"))
			return;

	} else if (veh->CanPedOpenLocks(ped)) {

		if (enterDoor != DOOR_FRONT_LEFT && enterDoor != DOOR_REAR_LEFT) {
			if (itsVan) {
				ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), ASSOCGRP_VANCARANIMS, ANIM_ID_CAR_OPEN_RHS_1);
			} else if (itsBus) {
				ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), ASSOCGRP_COACHCARANIMS, ANIM_ID_CAR_OPEN_LHS);
			} else {
				ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), GetVehicleAnimGroup(veh, ANIM_ID_CAR_OPEN_RHS), ANIM_ID_CAR_OPEN_RHS);
			}
		} else if (itsVan) {
			ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), ASSOCGRP_VANCARANIMS, ANIM_ID_CAR_OPEN_LHS_1);
		} else if (itsBus) {
			ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), ASSOCGRP_COACHCARANIMS, ANIM_ID_CAR_OPEN_LHS);
		} else {

			if (veh->IsOpenTopCar() && !veh->pDriver && ped->m_objective == OBJECTIVE_ENTER_CAR_AS_DRIVER) {
				ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), GetVehicleAnimGroup(veh, ANIM_ID_CAR_GETIN_LHS_0), ANIM_ID_CAR_GETIN_LHS_0);
				if(!ContinueVehicleEntryWithAnimation(ped, PedAnimGetInCB, "get in"))
					return;
				return;
			}
			ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), GetVehicleAnimGroup(veh, ANIM_ID_CAR_OPEN_LHS), ANIM_ID_CAR_OPEN_LHS);
		}
		if(!ContinueVehicleEntryWithAnimation(ped, PedAnimDoorOpenCB, "open door"))
			return;

	} else {
		if (enterDoor != DOOR_FRONT_LEFT && enterDoor != DOOR_REAR_LEFT)
			ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), GetVehicleAnimGroup(veh, ANIM_ID_CAR_DOORLOCKED_RHS), ANIM_ID_CAR_DOORLOCKED_RHS);
		else
			ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), GetVehicleAnimGroup(veh, ANIM_ID_CAR_DOORLOCKED_LHS), ANIM_ID_CAR_DOORLOCKED_LHS);

		ped->bCancelEnteringCar = true;
		if(!ContinueVehicleEntryWithAnimation(ped, PedAnimDoorOpenCB, "open door"))
			return;
	}
}

void
CPed::PedAnimDoorOpenCB(CAnimBlendAssociation* animAssoc, void* arg)
{
	CPed* ped = (CPed*)arg;

	CVehicle* veh = ped->m_pMyVehicle;

	if (animAssoc)
		animAssoc->blendDelta = -1000.0f;

	if (!ped->IsNotInWreckedVehicle())
		return;

	if (!ped->EnteringCar()) {
		if(ped->m_nPedState != PED_DRIVING)
			ped->QuitEnteringCar();

		return;
	}

	eDoors door;
	CPed *pedInSeat = nil;
	switch (ped->m_vehDoor) {
		case CAR_DOOR_RF:
			door = DOOR_FRONT_RIGHT;
			pedInSeat = veh->pPassengers[0];
			if (!veh->pPassengers[0] && ped->m_objective == OBJECTIVE_ENTER_CAR_AS_DRIVER)
				pedInSeat = veh->pDriver;
			break;
		case CAR_DOOR_RR:
			door = DOOR_REAR_RIGHT;
			pedInSeat = veh->pPassengers[2];
			break;
		case CAR_DOOR_LF:
			door = DOOR_FRONT_LEFT;
			pedInSeat = veh->pDriver;
			if (veh->bIsBus && ped->m_objective == OBJECTIVE_ENTER_CAR_AS_PASSENGER)
				pedInSeat = nil;
			break;
		case CAR_DOOR_LR:
			door = DOOR_REAR_LEFT;
			pedInSeat = veh->pPassengers[1];
			break;
		default: assert(0);
	}

	if (ped->m_fHealth == 0.0f || CPad::GetPad(0)->ArePlayerControlsDisabled() && pedInSeat && pedInSeat->IsPlayer()) {
		ped->QuitEnteringCar();
		return;
	}

	bool isVan = veh->bIsVan;
	bool isBus = veh->bIsBus;
	bool isLow = veh->bLowVehicle;
	bool vehUpsideDown = veh->IsUpsideDown();
	if (ped->bCancelEnteringCar) {
		if (ped->IsPlayer()) {
			if (veh->pDriver) {
				if (veh->pDriver->m_nPedType == PEDTYPE_COP) {
					FindPlayerPed()->SetWantedLevelNoDrop(1);
				}
			}
		}
#ifdef CANCELLABLE_CAR_ENTER
		if (!veh->IsDoorMissing(door) && veh->CanPedOpenLocks(ped) && veh->IsAutomobile()) {
			((CAutomobile*)veh)->Damage.SetDoorStatus(door, DOOR_STATUS_SWINGING);
		}
#endif
		ped->QuitEnteringCar();
		ped->RestorePreviousObjective();
		ped->bCancelEnteringCar = false;
		return;
	}
	if (!veh->IsDoorMissing(door) && veh->IsAutomobile()) {
		((CAutomobile*)veh)->Damage.SetDoorStatus(door, DOOR_STATUS_SWINGING);
	}

	if (veh->m_vecMoveSpeed.Magnitude() > 0.2f ||
		veh->IsAutomobile() && veh->GetVehicleAppearance() == VEHICLE_APPEARANCE_HELI && ((CAutomobile*)veh)->m_nWheelsOnGround == 0) {
		ped->QuitEnteringCar();
		if (ped->m_vehDoor != CAR_DOOR_LF && ped->m_vehDoor != CAR_DOOR_LR)
			ped->SetFall(1000, ANIM_ID_KO_SPIN_R, false);
		else
			ped->SetFall(1000, ANIM_ID_KO_SPIN_L, false);
		
		return;
	}
	veh->ProcessOpenDoor(ped->m_vehDoor, ANIM_ID_CAR_OPEN_LHS, 1.0f);

	if (ped->m_vehDoor == CAR_DOOR_LF || ped->m_vehDoor == CAR_DOOR_RF)
		isVan = false;

	if (ped->m_nPedState != PED_CARJACK || isBus) {

		if (ped->m_vehDoor == CAR_DOOR_LF || ped->m_vehDoor == CAR_DOOR_LR) {
			if (veh->IsBike()) {
				ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), ((CBike*)veh)->m_bikeAnimType, ANIM_ID_CAR_GETIN_LHS_0);
			} else if (isVan) {
				ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), ASSOCGRP_VANCARANIMS, ANIM_ID_CAR_GETIN_LHS_1);
			} else if (isBus) {
				ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), ASSOCGRP_COACHCARANIMS, ANIM_ID_CAR_GETIN_LHS_0);
			} else if (isLow) {
				ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), GetVehicleAnimGroup(veh, ANIM_ID_CAR_GETIN_LHS_0), ANIM_ID_CAR_GETIN_LHS_0);
			} else {
				ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), GetVehicleAnimGroup(veh, ANIM_ID_CAR_GETIN_LHS_0), ANIM_ID_CAR_GETIN_LHS_0);
			}
		} else {
			if (veh->IsBike()) {
				ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), ((CBike*)veh)->m_bikeAnimType, ANIM_ID_CAR_GETIN_RHS_0);
			} else if (isVan) {
				ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), ASSOCGRP_VANCARANIMS, ANIM_ID_CAR_GETIN_RHS_1);
			} else if (isBus) {
				ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), ASSOCGRP_COACHCARANIMS, ANIM_ID_CAR_GETIN_LHS_0);
			} else if (isLow) {
				ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), GetVehicleAnimGroup(veh, ANIM_ID_CAR_GETIN_RHS_0), ANIM_ID_CAR_GETIN_RHS_0);
			} else {
				ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), GetVehicleAnimGroup(veh, ANIM_ID_CAR_GETIN_RHS_0), ANIM_ID_CAR_GETIN_RHS_0);
			}

			if (ped->m_vehDoor == CAR_DOOR_RF && pedInSeat && veh->IsAutomobile())
				pedInSeat->SetObjective(OBJECTIVE_LEAVE_CAR, veh);
		}
		
		if(!ContinueVehicleEntryWithAnimation(ped, PedAnimGetInCB, "get in"))
			return;
	} else {
		CPed *pedToDragOut = nil;
		switch (ped->m_vehDoor) {
			case CAR_DOOR_RF: pedToDragOut = veh->pPassengers[0]; break;
			case CAR_DOOR_RR: pedToDragOut = veh->pPassengers[2]; break;
			case CAR_DOOR_LF: pedToDragOut = veh->pDriver; break;
			case CAR_DOOR_LR: pedToDragOut = veh->pPassengers[1]; break;
			default: assert(0);
		}

		if (vehUpsideDown) {
			ped->QuitEnteringCar();
		}

		if (ped->m_vehDoor != CAR_DOOR_LF && ped->m_vehDoor != CAR_DOOR_LR) {

			if (pedToDragOut && !pedToDragOut->bDontDragMeOutCar) {
				if (pedToDragOut->m_nPedState != PED_DRIVING) {
					ped->QuitEnteringCar();
					pedToDragOut = nil;
				} else {
					if (isLow)
						ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), GetVehicleAnimGroup(veh, ANIM_ID_CAR_PULLOUT_RHS), ANIM_ID_CAR_PULLOUT_RHS);
					else
						ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), GetVehicleAnimGroup(veh, ANIM_ID_CAR_PULLOUT_RHS), ANIM_ID_CAR_PULLOUT_RHS);

					if(!ContinueVehicleEntryWithAnimation(ped, PedAnimPullPedOutCB, "pull occupant out"))
						return;
				}
			} else {
				if (isLow)
					ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), GetVehicleAnimGroup(veh, ANIM_ID_CAR_GETIN_RHS_0), ANIM_ID_CAR_GETIN_RHS_0);
				else
					ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), GetVehicleAnimGroup(veh, ANIM_ID_CAR_GETIN_RHS_0), ANIM_ID_CAR_GETIN_RHS_0);

				if(!ContinueVehicleEntryWithAnimation(ped, PedAnimGetInCB, "get in"))
					return;
			}
		} else if (pedToDragOut) {

			if (pedToDragOut->m_nPedState != PED_DRIVING || pedToDragOut->bDontDragMeOutCar) {
				ped->QuitEnteringCar();
				pedToDragOut = nil;
			} else {
				if (isLow)
					ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), GetVehicleAnimGroup(veh, ANIM_ID_CAR_PULLOUT_LHS), ANIM_ID_CAR_PULLOUT_LHS);
				else
					ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), GetVehicleAnimGroup(veh, ANIM_ID_CAR_PULLOUT_LHS), ANIM_ID_CAR_PULLOUT_LHS);
					
				if(!ContinueVehicleEntryWithAnimation(ped, PedAnimPullPedOutCB, "pull occupant out"))
					return;
			}
		} else {
			if (isLow)
				ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), GetVehicleAnimGroup(veh, ANIM_ID_CAR_GETIN_LHS_0), ANIM_ID_CAR_GETIN_LHS_0);
			else
				ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), GetVehicleAnimGroup(veh, ANIM_ID_CAR_GETIN_LHS_0), ANIM_ID_CAR_GETIN_LHS_0);

			if(!ContinueVehicleEntryWithAnimation(ped, PedAnimGetInCB, "get in"))
				return;
		}

		if (pedToDragOut)
			pedToDragOut->SetBeingDraggedFromCar(veh, ped->m_vehDoor, false);
	}
	return;
}

void
CPed::PedAnimPullPedOutCB(CAnimBlendAssociation* animAssoc, void* arg)
{
	CPed* ped = (CPed*)arg;

	CVehicle* veh = ped->m_pMyVehicle;
	if (animAssoc)
		animAssoc->blendDelta = -1000.0f;

	if (ped->EnteringCar()) {
		if (!ped->IsNotInWreckedVehicle())
			return;

#ifdef CANCELLABLE_CAR_ENTER
		if (ped->bCancelEnteringCar) {
			ped->QuitEnteringCar();
			ped->RestorePreviousObjective();
			ped->bCancelEnteringCar = false;
			return;
		}
#endif

		bool isLow = !!veh->bLowVehicle;

		int padNo;
		if (ped->IsPlayer()) {

			// BUG? This will cause crash if m_nPedType is bigger then 1, there are only 2 pads
			switch (ped->m_nPedType) {
				case PEDTYPE_PLAYER1:
					padNo = 0;
					break;
				case PEDTYPE_PLAYER2:
					padNo = 1;
					break;
				case PEDTYPE_PLAYER_NETWORK:
					padNo = 2;
					break;
				case PEDTYPE_PLAYER_UNUSED:
					padNo = 3;
					break;
			}
			CPad *pad = CPad::GetPad(padNo);

			if (!pad->ArePlayerControlsDisabled()) {

				if (pad->GetTarget()
					|| pad->NewState.LeftStickX
					|| pad->NewState.LeftStickY
					|| pad->NewState.DPadUp
					|| pad->NewState.DPadDown
					|| pad->NewState.DPadLeft
					|| pad->NewState.DPadRight) {
					ped->QuitEnteringCar();
					ped->RestorePreviousObjective();
					return;
				}
			}
		}

		if (ped->m_objective == OBJECTIVE_ENTER_CAR_AS_DRIVER) {
			if (ped->m_vehDoor == CAR_DOOR_LF || ped->m_vehDoor == CAR_DOOR_LR) {
				if (veh->IsBike())
					ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), ((CBike*)veh)->m_bikeAnimType, ANIM_ID_CAR_GETIN_LHS_0);
				else if (isLow)
					ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), GetVehicleAnimGroup(veh, ANIM_ID_CAR_GETIN_LHS_0), ANIM_ID_CAR_GETIN_LHS_0);
				else
					ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), GetVehicleAnimGroup(veh, ANIM_ID_CAR_GETIN_LHS_0), ANIM_ID_CAR_GETIN_LHS_0);
			} else {
				if (veh->IsBike())
					ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), ((CBike*)veh)->m_bikeAnimType, ANIM_ID_CAR_GETIN_RHS_0);
				else if (isLow)
					ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), GetVehicleAnimGroup(veh, ANIM_ID_CAR_GETIN_RHS_0), ANIM_ID_CAR_GETIN_RHS_0);
				else
					ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), GetVehicleAnimGroup(veh, ANIM_ID_CAR_GETIN_RHS_0), ANIM_ID_CAR_GETIN_RHS_0);
			}
			if(!ContinueVehicleEntryWithAnimation(ped, PedAnimGetInCB, "get in"))
				return;
		} else {
			ped->QuitEnteringCar();
		}
	} else if(ped->m_nPedState != PED_DRIVING) {
		ped->QuitEnteringCar();
	}
}

void
CPed::PedAnimGetInCB(CAnimBlendAssociation *animAssoc, void *arg)
{
	CPed *ped = (CPed*) arg;

	CVehicle *veh = ped->m_pMyVehicle;
	if (animAssoc)
		animAssoc->blendDelta = -1000.0f;

	if (!ped->IsNotInWreckedVehicle() || ped->DyingOrDead())
		return;

	if (!ped->EnteringCar()) {
		if(ped->m_nPedState != PED_DRIVING)
			ped->QuitEnteringCar();
		return;
	}

	ped->RemoveWeaponWhenEnteringVehicle();
	if (ped->IsPlayer() && ped->m_vehDoor == CAR_DOOR_LF
		&& (Pads[0].GetAccelerate() >= 255.0f || Pads[0].GetBrake() >= 255.0f)
		&& veh->IsAutomobile() && !veh->pDriver) {

		if (!animAssoc || animAssoc->animId != ANIM_ID_CAR_GETIN_LHS_0)
		if (((CAutomobile*)veh)->Damage.GetDoorStatus(DOOR_FRONT_LEFT) != DOOR_STATUS_MISSING)
			((CAutomobile*)veh)->Damage.SetDoorStatus(DOOR_FRONT_LEFT, DOOR_STATUS_SWINGING);

		PedSetInCarCB(nil, ped);
		return;
	}
	if (veh->IsBike()) {
		ped->PedSetInCarCB(nil, ped);
		return;
	}
	bool isVan = !!veh->bIsVan;
	bool isBus = !!veh->bIsBus;
	bool isLow = !!veh->bLowVehicle;
	eDoors enterDoor;
	switch (ped->m_vehDoor) {
		case CAR_DOOR_RF:
			isVan = false;
			enterDoor = DOOR_FRONT_RIGHT;
			break;
		case CAR_DOOR_RR:
			enterDoor = DOOR_REAR_RIGHT;
			break;
		case CAR_DOOR_LF:
			isVan = false;
			enterDoor = DOOR_FRONT_LEFT;
			break;
		case CAR_DOOR_LR:
			enterDoor = DOOR_REAR_LEFT;
			break;
		default:
			break;
	}
	bool doorClosed = true;
	if (veh->IsOpenTopCar() && enterDoor == DOOR_FRONT_LEFT && veh->IsDoorClosed(DOOR_FRONT_LEFT)) {
		doorClosed = false;

	} else if (!veh->IsDoorMissing(enterDoor)) {
		if (veh->IsAutomobile())
			((CAutomobile*)veh)->Damage.SetDoorStatus(enterDoor, DOOR_STATUS_SWINGING);
	}

	CPed *driver = veh->pDriver;
	if (driver && (ped->m_objective == OBJECTIVE_ENTER_CAR_AS_DRIVER || ped->m_nPedState == PED_CARJACK)) {
		if (veh->bIsBus) {
			driver->SetObjective(OBJECTIVE_LEAVE_CAR, veh);
			if (driver->IsPlayer()) {
				veh->bIsHandbrakeOn = true;
				veh->SetStatus(STATUS_PLAYER_DISABLED);
			}
			veh->bIsBeingCarJacked = false;
			PedSetInCarCB(nil, ped);
			ped->m_leaveCarTimer = CTimer::GetTimeInMilliseconds() + 400;
			return;
		}
		if (driver != ped && ped->m_vehDoor != CAR_DOOR_LF) {
			driver->SetObjective(OBJECTIVE_LEAVE_CAR, veh);
			driver->Say(SOUND_PED_CAR_JACKED);
			if (veh->m_nDoorLock == CARLOCK_UNLOCKED)
				ped->Say(SOUND_PED_CAR_JACKING);
		}
	}
	if (veh->IsDoorMissing(enterDoor) || !doorClosed || isBus) {
		PedAnimDoorCloseCB(nil, ped);
	} else {
		if (enterDoor != DOOR_FRONT_LEFT && enterDoor != DOOR_REAR_LEFT) {
			if (isVan) {
				ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), ASSOCGRP_VANCARANIMS, ANIM_ID_CAR_CLOSEDOOR_RHS_1);
			} else if (isLow) {
				ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), GetVehicleAnimGroup(veh, ANIM_ID_CAR_CLOSEDOOR_RHS_0), ANIM_ID_CAR_CLOSEDOOR_RHS_0);
			} else {
				ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), GetVehicleAnimGroup(veh, ANIM_ID_CAR_CLOSEDOOR_RHS_0), ANIM_ID_CAR_CLOSEDOOR_RHS_0);
			}
		} else if (isVan) {
			ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), ASSOCGRP_VANCARANIMS, ANIM_ID_CAR_CLOSEDOOR_LHS_1);
		} else if (isLow) {
			ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), GetVehicleAnimGroup(veh, ANIM_ID_CAR_CLOSEDOOR_LHS_0), ANIM_ID_CAR_CLOSEDOOR_LHS_0);
		} else {
			ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), GetVehicleAnimGroup(veh, ANIM_ID_CAR_CLOSEDOOR_LHS_0), ANIM_ID_CAR_CLOSEDOOR_LHS_0);
		}
		if(!ContinueVehicleEntryWithAnimation(ped, PedAnimDoorCloseCB, "close door"))
			return;
	}
}

void
CPed::PedShuffle(void)
{
	if (m_pMyVehicle->pPassengers[0] == this) {
		CPed *driver = m_pMyVehicle->pDriver;
		if (!driver || driver->m_objective == OBJECTIVE_LEAVE_CAR) {
			m_pVehicleAnim = CAnimManager::AddAnimation(GetClump(), GetVehicleAnimGroup(m_pMyVehicle, ANIM_ID_CAR_SHUFFLE_RHS_0), ANIM_ID_CAR_SHUFFLE_RHS_0);
			if(!m_pVehicleAnim){
				if(IsPlayer())
					REVC_LOG_WARNING("Vehicle entry: missing passenger-to-driver shuffle animation (ped model=%d, vehicle model=%d)",
						GetModelIndex(), m_pMyVehicle->GetModelIndex());
				return; // keep the passenger seated rather than stranding them outside
			}
			m_objective = OBJECTIVE_ENTER_CAR_AS_DRIVER;
			m_pMyVehicle->RemovePassenger(this);
			bInVehicle = false;
			m_pVehicleAnim->SetFinishCallback(PedSetInCarCB, this);
		}
	}
}

void
CPed::PedAnimDoorCloseCB(CAnimBlendAssociation *animAssoc, void *arg)
{
	CPed *ped = (CPed*)arg;

	CAutomobile *veh = (CAutomobile*)(ped->m_pMyVehicle);
	
	if (!ped->IsNotInWreckedVehicle() || ped->DyingOrDead())
		return;

	if (ped->EnteringCar()) {
		bool isLow = !!veh->bLowVehicle;

		if (!veh->bIsBus)
			veh->ProcessOpenDoor(ped->m_vehDoor, ANIM_ID_CAR_CLOSEDOOR_LHS_0, 1.0f);

		eDoors door;
		switch (ped->m_vehDoor) {
			case CAR_DOOR_RF: door = DOOR_FRONT_RIGHT; break;
			case CAR_DOOR_RR: door = DOOR_REAR_RIGHT; break;
			case CAR_DOOR_LF: door = DOOR_FRONT_LEFT; break;
			case CAR_DOOR_LR: door = DOOR_REAR_LEFT; break;
			default: assert(0);
		}

		if (veh->Damage.GetDoorStatus(door) == DOOR_STATUS_SWINGING)
			veh->Damage.SetDoorStatus(door, DOOR_STATUS_OK);

		if (door == DOOR_FRONT_LEFT || ped->m_objective == OBJECTIVE_ENTER_CAR_AS_PASSENGER || veh->bIsBus || veh->m_nNumMaxPassengers == 0) {
			PedSetInCarCB(nil, ped);
		} else if (ped->m_vehDoor == CAR_DOOR_RF
				&& (veh->m_nGettingInFlags & CAR_DOOR_FLAG_LF ||
					(veh->pDriver != nil && 
						(veh->pDriver->m_objective != OBJECTIVE_LEAVE_CAR
							&& veh->pDriver->m_objective != OBJECTIVE_LEAVE_CAR_AND_DIE
							|| !veh->IsRoomForPedToLeaveCar(CAR_DOOR_LF, nil))))) {

			if (ped->m_objective == OBJECTIVE_ENTER_CAR_AS_DRIVER || ped->m_nPedState == PED_CARJACK)
				veh->bIsBeingCarJacked = false;

			ped->m_objective = OBJECTIVE_ENTER_CAR_AS_PASSENGER;
			PedSetInCarCB(nil, ped);

			ped->SetObjective(OBJECTIVE_LEAVE_CAR, veh);
		} else {
			if (animAssoc)
				animAssoc->blendDelta = -1000.0f;

			if (isLow)
				ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), GetVehicleAnimGroup(veh, ANIM_ID_CAR_SHUFFLE_RHS_0), ANIM_ID_CAR_SHUFFLE_RHS_0);
			else
				ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), GetVehicleAnimGroup(veh, ANIM_ID_CAR_SHUFFLE_RHS_0), ANIM_ID_CAR_SHUFFLE_RHS_0);

			if(!ContinueVehicleEntryWithAnimation(ped, PedSetInCarCB, "seat"))
				return;
		}
	} else if (ped->m_nPedState != PED_DRIVING) {
		ped->QuitEnteringCar();
	}
}

void
CPed::PedAnimShuffleCB(CAnimBlendAssociation* assoc, void* arg)
{
	CPed *ped = (CPed*)arg;
	if (ped->EnteringCar()) {
		PedSetInCarCB(nil, ped);
	} else if (ped->m_nPedState != PED_DRIVING) {
		ped->QuitEnteringCar();
	}
}

void
CPed::PedAnimStepOutCarCB(CAnimBlendAssociation* animAssoc, void* arg)
{
	CPed* ped = (CPed*)arg;

	CVehicle* veh = ped->m_pMyVehicle;
	if (animAssoc) {
		if ((animAssoc->animId == ANIM_ID_DEFAULT_CAR_ROLLOUT_LHS || animAssoc->animId == ANIM_ID_DEFAULT_CAR_ROLLOUT_LHS) && ped && ped->m_nPedState == PED_FALL) {
			ped->RestoreHeadingRate();
			return;
		}
		animAssoc->blendDelta = -1000.0f;
		if (animAssoc->animId == ANIM_ID_UNKNOWN_26)
			ped->RestoreHeadingRate();
	}

	if (!veh) {
		PedSetOutCarCB(nil, ped);
		return;
	}
	CVector posForZ = ped->GetPosition();
	FindZCoorForPed(&posForZ);
	if (ped->GetPosition().z - 0.5f > posForZ.z) {
		PedSetOutCarCB(nil, ped);
		return;
	}

	veh->m_nStaticFrames = 0;
	veh->m_vecMoveSpeed += CVector(0.001f, 0.001f, 0.001f);
	veh->m_vecTurnSpeed += CVector(0.001f, 0.001f, 0.001f);
	if (!veh->bIsBus)
		veh->ProcessOpenDoor(ped->m_vehDoor, ANIM_ID_CAR_GETOUT_LHS_0, 1.0f);

	/* 
	// Duplicate and only in PC for some reason
	if (!veh) {
		PedSetOutCarCB(nil, ped);
		return;
	}
	*/
	eDoors door;
	switch (ped->m_vehDoor) {
		case CAR_DOOR_RF:
			door = DOOR_FRONT_RIGHT;
			break;
		case CAR_DOOR_RR:
			door = DOOR_REAR_RIGHT;
			break;
		case CAR_DOOR_LF:
			door = DOOR_FRONT_LEFT;
			break;
		case CAR_DOOR_LR:
			door = DOOR_REAR_LEFT;
			break;
		default:
			break;
	}
	bool closeDoor = !veh->IsDoorMissing(door);

	int padNo;
	if (ped->IsPlayer()) {

		// BUG? This will cause crash if m_nPedType is bigger then 1, there are only 2 pads
		switch (ped->m_nPedType) {
			case PEDTYPE_PLAYER1:
				padNo = 0;
				break;
			case PEDTYPE_PLAYER2:
				padNo = 1;
				break;
			case PEDTYPE_PLAYER_NETWORK:
				padNo = 2;
				break;
			case PEDTYPE_PLAYER_UNUSED:
				padNo = 3;
				break;
		}
		CPad* pad = CPad::GetPad(padNo);
		bool engineIsIntact = veh->IsAutomobile() && ((CAutomobile*)veh)->Damage.GetEngineStatus() >= 225;
		if (!pad->ArePlayerControlsDisabled() && veh->m_nDoorLock != CARLOCK_FORCE_SHUT_DOORS
			&& (pad->GetTarget()
				|| pad->NewState.LeftStickX
				|| pad->NewState.LeftStickY
				|| pad->NewState.DPadUp
				|| pad->NewState.DPadDown
				|| pad->NewState.DPadLeft
				|| pad->NewState.DPadRight)
			|| veh->bIsBus
			|| veh->m_pCarFire
			|| engineIsIntact) {
			closeDoor = false;
		}
	}

	if (ped->m_objective == OBJECTIVE_LEAVE_CAR_AND_DIE)
		closeDoor = false;

	if (!closeDoor) {
		if (!veh->IsDoorMissing(door) && !veh->bIsBus) {
			((CAutomobile*)veh)->Damage.SetDoorStatus(door, DOOR_STATUS_SWINGING);
		}
		PedSetOutCarCB(nil, ped);
		return;
	}

	switch (door) {
		case DOOR_FRONT_LEFT:
			ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), GetVehicleAnimGroup(veh, ANIM_ID_CAR_CLOSE_LHS_0), ANIM_ID_CAR_CLOSE_LHS_0);
			break;
		case DOOR_FRONT_RIGHT:
			ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), GetVehicleAnimGroup(veh, ANIM_ID_CAR_CLOSE_RHS_0), ANIM_ID_CAR_CLOSE_RHS_0);
			break;
		case DOOR_REAR_LEFT:
			ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), GetVehicleAnimGroup(veh, ANIM_ID_CAR_CLOSE_LHS_0), ANIM_ID_CAR_CLOSE_LHS_0);
			break;
		case DOOR_REAR_RIGHT:
			ped->m_pVehicleAnim = CAnimManager::AddAnimation(ped->GetClump(), GetVehicleAnimGroup(veh, ANIM_ID_CAR_CLOSE_RHS_0), ANIM_ID_CAR_CLOSE_RHS_0);
			break;
		default:
			break;
	}

	if (ped->m_pVehicleAnim)
		ped->m_pVehicleAnim->SetFinishCallback(PedSetOutCarCB, ped);
	return;
}

void
CPed::LineUpPedWithCar(PedLineUpPhase phase)
{
	bool vehIsUpsideDown = false;
	bool stillGettingInOut = false;
	int vehAnim = ANIM_ID_UNDEFINED;
	float seatPosMult = 0.0f;
	float currentZ;
	float adjustedTimeStep;
	CVector autoZPos;

	if (!bChangedSeat && phase != LINE_UP_TO_CAR_2) {
		if (m_pMyVehicle->IsBike()) {
			if (RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_BIKE_RIDE) ||
				RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_BIKE_PASSENGER)) {
				SetPedPositionInCar();
				return;
			}
		} else {
			if (RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_CAR_SIT)) {
				SetPedPositionInCar();
				return;
			}
			if (RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_CAR_LSIT)) {
				SetPedPositionInCar();
				return;
			}
			if (RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_CAR_SIT_WEAK)) {
				SetPedPositionInCar();
				return;
			}
			if (RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_CAR_SIT_PRO)) {
				SetPedPositionInCar();
				return;
			}
		}
		bChangedSeat = true;
	}
	if (phase == LINE_UP_TO_CAR_FALL) {
		SetPedPositionInCar();
		autoZPos = GetPosition();
		FindZCoorForPed(&autoZPos);
		if (m_pVehicleAnim && (m_pVehicleAnim->animId == ANIM_ID_DEFAULT_CAR_ROLLOUT_LHS || m_pVehicleAnim->animId == ANIM_ID_DEFAULT_CAR_ROLLOUT_LHS)
			&& autoZPos.z > GetPosition().z) {
			m_matrix.GetPosition().z = autoZPos.z;
		}
		if (m_pVehicleAnim && m_pVehicleAnim->animId == ANIM_ID_BIKE_HIT) {
			if (autoZPos.z > GetPosition().z)
				m_matrix.GetPosition().z += m_pVehicleAnim->GetProgress() * (autoZPos.z - GetPosition().z);

		} else if (m_pVehicleAnim) {
			if (m_pVehicleAnim->animId == ANIM_ID_UNKNOWN_26) {
				if (autoZPos.z > GetPosition().z) {
					m_matrix.GetPosition().z += (m_pVehicleAnim->currentTime * (20.f / 7.f)) * (autoZPos.z - GetPosition().z);
				}
			}
		}
		return;
	}
	if (phase == LINE_UP_TO_CAR_START) {
		m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f);
	}
	CVehicle *veh = m_pMyVehicle;

	// Not quite right, IsUpsideDown func. checks for <= -0.9f.
	if (veh->GetUp().z <= -0.8f)
		vehIsUpsideDown = true;

	if (m_vehDoor == CAR_DOOR_RF || m_vehDoor == CAR_DOOR_RR) {
		if (vehIsUpsideDown) {
			m_fRotationDest = -PI + veh->GetForward().Heading();
		} else if (veh->bIsBus) {
			m_fRotationDest = 0.5f * PI + veh->GetForward().Heading();
		} else {
			m_fRotationDest = veh->GetForward().Heading();
		}
	} else if (m_vehDoor == CAR_DOOR_LF || m_vehDoor == CAR_DOOR_LR) {
		if (vehIsUpsideDown) {
			m_fRotationDest = veh->GetForward().Heading();
		} else if (veh->bIsBus) {
			m_fRotationDest = -0.5f * PI + veh->GetForward().Heading();
		} else {
			m_fRotationDest = veh->GetForward().Heading();
		}
	} else {
		// I don't know will this part ever run(maybe boats?), but the game also handles that. I don't know is it intentional.

		if (vehIsUpsideDown) {
			m_fRotationDest = veh->GetForward().Heading();
		} else if (veh->bIsBus) {
			m_fRotationDest = 0.5f * PI + veh->GetForward().Heading();
		} else if (m_vehDoor == CAR_WINDSCREEN) {
			m_fRotationDest = veh->GetForward().Heading() + PI;
		} else {
			m_fRotationDest = veh->GetForward().Heading();
		}
	}

	bool multExtractedFromAnim = false;
	bool multExtractedFromAnimBus = false;
	float zBlend;
	if (m_pVehicleAnim) {
		vehAnim = m_pVehicleAnim->animId;

		switch (vehAnim) {
			case ANIM_ID_CAR_JACKEDRHS:
			case ANIM_ID_CAR_JACKEDLHS:
			case ANIM_ID_CAR_GETIN_LHS_1:
			case ANIM_ID_CAR_GETIN_RHS_1:
				multExtractedFromAnim = true;
				zBlend = Max(m_pVehicleAnim->GetProgress() - 0.3f, 0.0f) / (1.0f - 0.3f);
				// fall through

			case ANIM_ID_CAR_CLOSEDOOR_LHS_0:
			case ANIM_ID_CAR_GETOUT_LHS_0:
			case ANIM_ID_CAR_GETOUT_RHS_0:

				if (!multExtractedFromAnim) {
					multExtractedFromAnim = true;
					zBlend = Max(m_pVehicleAnim->GetProgress() - 0.5f, 0.0f) / (1.0f - 0.5f);
				}
				// fall through

			case ANIM_ID_DEFAULT_CAR_CRAWLOUTRHS_0:
			case ANIM_ID_CAR_GETOUT_LHS_1:
			case ANIM_ID_CAR_GETOUT_RHS_1:
				seatPosMult = m_pVehicleAnim->GetProgress();
				break;
			case ANIM_ID_CAR_GETIN_RHS_0:
			case ANIM_ID_CAR_GETIN_LHS_0:
				if (veh && veh->IsAutomobile() && veh->bIsBus) {
					multExtractedFromAnimBus = true;
					zBlend = Min(m_pVehicleAnim->GetProgress(), 0.5f) / 0.5f;
				}
				// fall through

			case ANIM_ID_CAR_OPEN_LHS:
			case ANIM_ID_DRIVE_BOAT:
				seatPosMult = m_pVehicleAnim->GetTimeLeft() / m_pVehicleAnim->hierarchy->totalLength;
				break;
			case ANIM_ID_CAR_CLOSEDOOR_RHS_0:
			case ANIM_ID_CAR_SHUFFLE_RHS_0:
				seatPosMult = 0.0f;
				break;
			{
				float animLength = m_pVehicleAnim->hierarchy->totalLength;
				seatPosMult = Max(0.0f, 0.5f * animLength - m_pVehicleAnim->currentTime) / animLength;
				break;
			}
			case ANIM_ID_CAR_CLOSE_LHS_0:
			case ANIM_ID_CAR_CLOSE_RHS_0:
				seatPosMult = 1.0f;
				break;
			default:
				if (veh->IsBike()) {
					seatPosMult = 0.0f;
				} else {
					if (bInVehicle)
						seatPosMult = 0.0f;
					else
						seatPosMult = 1.0f;
				}
				break;
		}
	} else {
		if (veh->IsBike()) {
			seatPosMult = 0.0f;
		} else {
			if (bInVehicle)
				seatPosMult = 0.0f;
			else
				seatPosMult = 1.0f;
		}
	}

	CVector neededPos;

	if (phase == LINE_UP_TO_CAR_2) {
		neededPos = GetPosition();
	} else {
		neededPos = GetPositionToOpenCarDoor(veh, m_vehDoor, seatPosMult, this);
	}

	autoZPos = neededPos;

	if (veh->bIsInWater) {
		if (veh->m_nVehicleType == VEHICLE_TYPE_BOAT && veh->IsUpsideDown())
			autoZPos.z += 1.0f;
	} else {
		FindZCoorForPed(&autoZPos);
	}

	if (phase == LINE_UP_TO_CAR_END || phase == LINE_UP_TO_CAR_2) {
		neededPos.z = GetPosition().z;

		// Getting out
		if (!veh->bIsBus || (veh->bIsBus && vehIsUpsideDown)) {
			float nextZSpeed = m_vecMoveSpeed.z - GRAVITY * CTimer::GetTimeStep();

			// If we're not in ground at next step, apply animation
			if (neededPos.z + nextZSpeed >= autoZPos.z) {
				m_vecMoveSpeed.z = nextZSpeed;
				ApplyMoveSpeed();
				// Removing below line breaks the animation
				neededPos.z = GetPosition().z;
			} else {
				neededPos.z = autoZPos.z;
				m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f);
			}
		}
	}

	if (autoZPos.z > neededPos.z) {
		if(m_pVehicleAnim) vehAnim = m_pVehicleAnim->animId;
		if (veh->IsBike() && m_pVehicleAnim && vehAnim != ANIM_ID_CAR_GETIN_BIKE_FRONT) {
			float zBlend;
			if (vehAnim != ANIM_ID_CAR_GETOUT_LHS_0 && vehAnim != ANIM_ID_CAR_GETOUT_RHS_0) {
				if (vehAnim != ANIM_ID_CAR_GETIN_LHS_0 && vehAnim != ANIM_ID_CAR_GETIN_RHS_0) {
					zBlend = 0.0f;
				} else {
					float animLength = m_pVehicleAnim->hierarchy->totalLength;
					zBlend = Min(1.0f, 2.0f * m_pVehicleAnim->currentTime / animLength);
				}
			} else {
				zBlend = 1.0f - seatPosMult;
			}
			float curZ = veh->GetPosition().z + FEET_OFFSET;
			neededPos.z = ((curZ - autoZPos.z) - veh->GetHeightAboveRoad()) * zBlend + autoZPos.z;
		} else if (multExtractedFromAnim) {
			neededPos.z += (autoZPos.z - neededPos.z) * zBlend;
		} else {
			currentZ = GetPosition().z;
			if (m_pVehicleAnim && vehAnim != ANIM_ID_CAR_GETIN_LHS_1 && vehAnim != ANIM_ID_CAR_CLOSEDOOR_LHS_1 && vehAnim != ANIM_ID_CAR_CLOSEDOOR_RHS_1 && vehAnim != ANIM_ID_CAR_GETIN_RHS_1) {
				neededPos.z = autoZPos.z;
				m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f);
			} else if (neededPos.z < currentZ && m_pVehicleAnim && vehAnim != ANIM_ID_CAR_CLOSEDOOR_LHS_1 && vehAnim != ANIM_ID_CAR_CLOSEDOOR_RHS_1) {
				adjustedTimeStep = Max(m_pVehicleAnim->timeStep, 0.1f);

				// Smoothly change ped position
				neededPos.z = currentZ - (currentZ - neededPos.z) / (m_pVehicleAnim->GetTimeLeft() / adjustedTimeStep);
			}
		}
	} else {
		// We may need to raise up the ped
		if (phase == LINE_UP_TO_CAR_START) {
			currentZ = GetPosition().z;

			if (neededPos.z > currentZ) {
				if (multExtractedFromAnimBus) {
					neededPos.z = (neededPos.z - currentZ) * zBlend + currentZ;
				} else {
					if (m_pVehicleAnim &&
						(vehAnim == ANIM_ID_CAR_GETIN_RHS_0 || vehAnim == ANIM_ID_CAR_GETIN_RHS_0 || vehAnim == ANIM_ID_CAR_GETIN_LHS_0 || vehAnim == ANIM_ID_CAR_GETIN_LHS_0
							|| vehAnim == ANIM_ID_CAR_OPEN_LHS || vehAnim == ANIM_ID_CAR_GETIN_LHS_1 || vehAnim == ANIM_ID_CAR_GETIN_RHS_1)) {
						adjustedTimeStep = Max(m_pVehicleAnim->timeStep, 0.1f);

						// Smoothly change ped position
						neededPos.z = (neededPos.z - currentZ) / (m_pVehicleAnim->GetTimeLeft() / adjustedTimeStep) + currentZ;
					} else if (EnteringCar() || m_nPedState == PED_DRIVING && veh->IsBike()) {
						neededPos.z = Max(currentZ, autoZPos.z);
					}
				}
			}
		}
	}

	if (CTimer::GetTimeInMilliseconds() < m_nPedStateTimer)
		stillGettingInOut = veh->m_nVehicleType != VEHICLE_TYPE_BOAT || bOnBoat;

	if (!stillGettingInOut) {
		m_fRotationCur = m_fRotationDest;
	} else {
		float limitedDest = CGeneral::LimitRadianAngle(m_fRotationDest);
		float timeUntilStateChange = (m_nPedStateTimer - CTimer::GetTimeInMilliseconds())/600.0f;

		if (timeUntilStateChange <= 0.0f) {
			m_vecOffsetSeek.x = 0.0f;
			m_vecOffsetSeek.y = 0.0f;
		}
		m_vecOffsetSeek.z = 0.0f;

		neededPos -= timeUntilStateChange * m_vecOffsetSeek;

		if (PI + m_fRotationCur < limitedDest) {
			limitedDest -= 2 * PI;
		} else if (m_fRotationCur - PI > limitedDest) {
			limitedDest += 2 * PI;
		}
		m_fRotationCur -= (m_fRotationCur - limitedDest) * (1.0f - timeUntilStateChange);
	}

	if (seatPosMult > 0.2f || vehIsUpsideDown || veh->IsBike()) {
		SetPosition(neededPos);
		SetHeading(m_fRotationCur);
	} else {
		CMatrix vehDoorMat(veh->GetMatrix());
		vehDoorMat.GetPosition() += Multiply3x3(vehDoorMat, GetLocalPositionToOpenCarDoor(veh, m_vehDoor, 0.0f, this));

		if (m_vehDoor == CAR_WINDSCREEN || veh->bIsBus) {
			CMatrix correctionMat;
			if (veh->bIsBus && (m_vehDoor == CAR_DOOR_LF || m_vehDoor == CAR_DOOR_LR))
				correctionMat.SetRotateZ(-HALFPI);
			else if (veh->bIsBus && (m_vehDoor == CAR_DOOR_RF || m_vehDoor == CAR_DOOR_RR))
				correctionMat.SetRotateZ(HALFPI);
			else
				correctionMat.SetRotateZ(PI);

			vehDoorMat = vehDoorMat * correctionMat;
		}
		GetMatrix() = vehDoorMat;
	}

}

void
CPed::SetCarJack(CVehicle* car)
{
	uint8 doorFlag;
	eDoors door;
	CPed *pedInSeat = nil;

	if (car->IsBoat())
		return;

	if (car->IsBike()) {
		switch (m_vehDoor) {
			case CAR_DOOR_RF:
				doorFlag = CAR_DOOR_FLAG_LF | CAR_DOOR_FLAG_RF;
				door = DOOR_FRONT_RIGHT;
				pedInSeat = car->pDriver;
				break;
			case CAR_DOOR_RR:
				doorFlag = CAR_DOOR_FLAG_LR | CAR_DOOR_FLAG_RR;
				door = DOOR_REAR_RIGHT;
				pedInSeat = car->pPassengers[0];
				break;
			case CAR_DOOR_LF:
			case CAR_WINDSCREEN:
				doorFlag = CAR_DOOR_FLAG_LF | CAR_DOOR_FLAG_RF;
				door = DOOR_FRONT_LEFT;
				pedInSeat = car->pDriver;
				break;
			case CAR_DOOR_LR:
				doorFlag = CAR_DOOR_FLAG_LR | CAR_DOOR_FLAG_RR;
				door = DOOR_REAR_LEFT;
				pedInSeat = car->pPassengers[0];
				break;
			default:
				doorFlag = CAR_DOOR_FLAG_UNKNOWN;
				break;
		}
	} else {
		switch (m_vehDoor) {
			case CAR_DOOR_RF:
				doorFlag = CAR_DOOR_FLAG_RF;
				door = DOOR_FRONT_RIGHT;
				pedInSeat = car->pPassengers[0];
				break;
			case CAR_DOOR_RR:
				doorFlag = CAR_DOOR_FLAG_RR;
				door = DOOR_REAR_RIGHT;
				pedInSeat = car->pPassengers[2];
				break;
			case CAR_DOOR_LF:
				doorFlag = CAR_DOOR_FLAG_LF;
				door = DOOR_FRONT_LEFT;
				pedInSeat = car->pDriver;
				break;
			case CAR_DOOR_LR:
				doorFlag = CAR_DOOR_FLAG_LR;
				door = DOOR_REAR_LEFT;
				pedInSeat = car->pPassengers[1];
				break;
			default:
				doorFlag = CAR_DOOR_FLAG_UNKNOWN;
				break;
		}
	}

	if(car->bIsBus)
		pedInSeat = car->pDriver;

	if (m_fHealth > 0.0f && (IsPlayer() || CharCreatedBy == MISSION_CHAR
			|| (car->VehicleCreatedBy != MISSION_VEHICLE && car->GetModelIndex() != MI_DODO)))
		if (pedInSeat && !pedInSeat->IsPedDoingDriveByShooting() && pedInSeat->m_nPedState == PED_DRIVING)
			if (m_nPedState != PED_CARJACK && !m_pVehicleAnim)
				if ((car->IsDoorReady(door) || car->IsDoorFullyOpen(door)))
					if (!car->bIsBeingCarJacked && !(doorFlag & car->m_nGettingInFlags) && !(doorFlag & car->m_nGettingOutFlags))
						SetCarJack_AllClear(car, m_vehDoor, doorFlag);
}

void
CPed::SetCarJack_AllClear(CVehicle* car, uint32 doorNode, uint32 doorFlag)
{
	if (m_nPedState != PED_SEEK_CAR)
		SetStoredState();

	m_pSeekTarget = car;
	m_pSeekTarget->RegisterReference((CEntity**)&m_pSeekTarget);
	SetPedState(PED_CARJACK);
	car->bIsBeingCarJacked = true;
	m_pMyVehicle = (CVehicle*)m_pSeekTarget;
	m_pMyVehicle->RegisterReference((CEntity**)&m_pMyVehicle);
	((CVehicle*)m_pSeekTarget)->m_nNumGettingIn++;

	if (m_nPedType == PEDTYPE_COP)
		Say(SOUND_PED_ARREST_COP);
	else if (car->m_nDoorLock == CARLOCK_UNLOCKED)
		Say(SOUND_PED_CAR_JACKING, 1000);

	CVector carEnterPos;
	carEnterPos = GetPositionToOpenCarDoor(car, m_vehDoor, this);

	car->m_nGettingInFlags |= doorFlag;
	m_vecOffsetSeek = carEnterPos - GetPosition();
	m_nPedStateTimer = CTimer::GetTimeInMilliseconds() + 600;

	if(car->IsBike()){
		bUsesCollision = false;
		PedAnimAlignCB(nil, this);
	} else {
		float zDiff = Max(0.0f, carEnterPos.z - GetPosition().z);
		bUsesCollision = false;

		if (m_vehDoor == CAR_DOOR_LF || m_vehDoor == CAR_DOOR_LR)
			m_pVehicleAnim = CAnimManager::BlendAnimation(GetClump(), GetVehicleAnimGroup(m_pMyVehicle, zDiff > 4.4f ? ANIM_ID_CAR_ALIGNHI_LHS : ANIM_ID_CAR_ALIGN_LHS), zDiff > 4.4f ? ANIM_ID_CAR_ALIGNHI_LHS : ANIM_ID_CAR_ALIGN_LHS, 4.0f);
		else
			m_pVehicleAnim = CAnimManager::BlendAnimation(GetClump(), GetVehicleAnimGroup(m_pMyVehicle, zDiff > 4.4f ? ANIM_ID_CAR_ALIGNHI_RHS : ANIM_ID_CAR_ALIGN_RHS), zDiff > 4.4f ? ANIM_ID_CAR_ALIGNHI_RHS : ANIM_ID_CAR_ALIGN_RHS, 4.0f);

		if(m_pVehicleAnim)
			m_pVehicleAnim->SetFinishCallback(PedAnimAlignCB, this);
		else{
			REVC_LOG_WARNING("Vehicle jack: missing align animation (ped=%d, vehicle=%d, door=%d)",
				GetModelIndex(), car->GetModelIndex(), m_vehDoor);
			QuitEnteringCar();
		}
	}
}

void
CPed::SetBeingDraggedFromCar(CVehicle *veh, uint32 vehEnterType, bool quickJack)
{
	if (m_nPedState == PED_DRAGGED_FROM_CAR)
		return;

	bUsesCollision = false;
	m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f);
	m_nLastPedState = PED_IDLE;
	SetMoveState(PEDMOVE_STILL);
	m_pSeekTarget = veh;
	m_pSeekTarget->RegisterReference((CEntity **) &m_pSeekTarget);

	if (veh->IsBike()) {
		((CBike*)veh)->bIsBeingPickedUp = true;
		if (veh->pPassengers[0] != this && (vehEnterType != CAR_WINDSCREEN || veh->pPassengers[0]))
			m_vehDoor = CAR_DOOR_LF;
		else
			m_vehDoor = CAR_DOOR_LR;
	} else {
		m_vehDoor = vehEnterType;
	}

	if (m_vehDoor == CAR_DOOR_LF) {
		if (veh->pDriver && veh->pDriver->IsPlayer())
			veh->SetStatus(STATUS_PLAYER_DISABLED);
		else
			veh->SetStatus(STATUS_ABANDONED);
	}
	RemoveInCarAnims();
	SetMoveState(PEDMOVE_NONE);
	LineUpPedWithCar(veh->IsBike() ? LINE_UP_TO_CAR_FALL : LINE_UP_TO_CAR_START);
	m_pVehicleAnim = nil;
	SetPedState(PED_DRAGGED_FROM_CAR);
	bChangedSeat = false;
	bWillBeQuickJacked = quickJack;

	SetHeading(m_fRotationCur);

	Say(SOUND_PED_CAR_JACKED);

	if(veh->IsBike())
		veh->m_nGettingOutFlags |= GetBikeDoorFlagInclJumpInFromFront(m_vehDoor);
	else
		veh->m_nGettingOutFlags |= GetCarDoorFlag(m_vehDoor);
}

void
CPed::BeingDraggedFromCar(void)
{
	AnimationId enterAnim;
	bool dontRunAnim = false;

	if (!m_pVehicleAnim) {
		CAnimManager::BlendAnimation(GetClump(), m_animGroup, ANIM_ID_IDLE, 1000.0f);

		AssocGroupId assocGroup;
		if (m_pMyVehicle && m_pMyVehicle->IsBike()) {
			enterAnim = ANIM_ID_BIKE_HIT;
			assocGroup = ((CBike*)m_pMyVehicle)->m_bikeAnimType;

		} else {
			if (m_vehDoor == CAR_DOOR_LF || m_vehDoor == CAR_DOOR_LR) {
				if (bWillBeQuickJacked && m_vehDoor == CAR_DOOR_LF) {
					enterAnim = ANIM_ID_CAR_CLOSEDOOR_LHS_0;
				} else if (m_pMyVehicle->bLowVehicle) {
					enterAnim = ANIM_ID_CAR_JACKEDLHS;
				} else {
					enterAnim = ANIM_ID_CAR_JACKEDLHS;
				}
			} else if (m_vehDoor == CAR_DOOR_RF || m_vehDoor == CAR_DOOR_RR) {
				if (m_pMyVehicle->bLowVehicle)
					enterAnim = ANIM_ID_CAR_JACKEDRHS;
				else
					enterAnim = ANIM_ID_CAR_JACKEDRHS;
			} else
				dontRunAnim = true;

			assocGroup = ASSOCGRP_DEFAULT;
		}

		if (!dontRunAnim)
			m_pVehicleAnim = CAnimManager::AddAnimation(GetClump(), assocGroup, enterAnim);

		if(!m_pVehicleAnim){
			REVC_LOG_WARNING("Vehicle drag: missing animation (ped=%d, vehicle=%d, door=%d)",
				GetModelIndex(), m_pMyVehicle ? m_pMyVehicle->GetModelIndex() : -1, m_vehDoor);
			// The callback below has an explicit no-animation path, including
			// seat/door cleanup. A plain nil dereference stranded the occupant.
			PedSetDraggedOutCarCB(nil, this);
			return;
		}
		m_pVehicleAnim->SetFinishCallback(PedSetDraggedOutCarCB, this);

		if (m_pMyVehicle && m_pMyVehicle->IsBike()) {
			LineUpPedWithCar(LINE_UP_TO_CAR_FALL);
		} else {
			LineUpPedWithCar(LINE_UP_TO_CAR_START);
		}
		return;

	} else if (m_pVehicleAnim->animId == ANIM_ID_BIKE_HIT) {
		LineUpPedWithCar(LINE_UP_TO_CAR_FALL);

	} else if (m_pVehicleAnim->currentTime <= 1.4f) {
		m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f);
		LineUpPedWithCar(LINE_UP_TO_CAR_START);

	} else {
		LineUpPedWithCar(LINE_UP_TO_CAR_2);
	}

	static float mult = 5.f;
	if (m_objective == OBJECTIVE_LEAVE_CAR_AND_DIE) {
		if (m_pMyVehicle) {
			m_pMyVehicle->ProcessOpenDoor(m_vehDoor, ANIM_ID_UNDEFINED, m_pVehicleAnim->currentTime * mult);
		}
	}
}

void
CPed::SetEnterCar(CVehicle *car, uint32 unused)
{
	// The door flag and the "is this door usable" test both come from
	// CCarEnterExit now, so the bike/car difference is stated once instead
	// of being spelled out inline. The old switch also left `door`
	// uninitialised on the CAR_DOOR_FLAG_UNKNOWN paths, which made the
	// IsDoorReady test below read an indeterminate value.
	const uint8 doorFlag = (uint8)CCarEnterExit::ComputeDoorFlag(car, m_vehDoor, true);
	if(IsPlayer()){
		static uint32 lastEnterLog;
		uint32 now = CTimer::GetTimeInMilliseconds();
		if(now - lastEnterLog >= 500){
			bool doorReady = doorFlag ? CCarEnterExit::IsCarDoorReady(car, m_vehDoor) : true;
			REVC_LOG_INFO("Vehicle entry: SetEnterCar(door=%d flag=%d) control=%d health=%.0f in=%d out=%d jacked=%d anim=%d ready=%d model=%d",
				m_vehDoor, doorFlag, IsPedInControl(), m_fHealth,
				car->m_nGettingInFlags, car->m_nGettingOutFlags,
				car->bIsBeingCarJacked, m_pVehicleAnim != nil, doorReady, car->GetModelIndex());
			lastEnterLog = now;
		}
	}
	if (!IsPedInControl() || m_fHealth <= 0.0f
		|| doorFlag & car->m_nGettingInFlags || doorFlag & car->m_nGettingOutFlags
		|| car->bIsBeingCarJacked || m_pVehicleAnim
		|| (doorFlag && !CCarEnterExit::IsCarDoorReady(car, m_vehDoor))){
		if(IsPlayer()){
			static uint32 lastRefusalLog;
			uint32 now = CTimer::GetTimeInMilliseconds();
			if(now - lastRefusalLog >= 1000){
				REVC_LOG_INFO("Vehicle entry: door %d refused (state=%d, flags=%d/%d, jacked=%d, anim=%d)",
					m_vehDoor, m_nPedState, car->m_nGettingInFlags, car->m_nGettingOutFlags,
					car->bIsBeingCarJacked, m_pVehicleAnim != nil);
				lastRefusalLog = now;
			}
		}
		SetMoveState(PEDMOVE_STILL);
	}else
		SetEnterCar_AllClear(car, m_vehDoor, doorFlag);
}

void
CPed::SetEnterCar_AllClear(CVehicle *car, uint32 doorNode, uint32 doorFlag)
{
	float zDiff = 0.0f;
	car->m_nGettingInFlags |= doorFlag;
	bVehEnterDoorIsBlocked = false;
	if (m_nPedState != PED_SEEK_CAR && m_nPedState != PED_SEEK_BOAT_POSITION)
		SetStoredState();

	m_pSeekTarget = car;
	m_pSeekTarget->RegisterReference((CEntity **) &m_pSeekTarget);
	m_vehDoor = doorNode;
	SetPedState(PED_ENTER_CAR);
	if (m_vehDoor == CAR_DOOR_RF && m_objective == OBJECTIVE_ENTER_CAR_AS_DRIVER && !car->IsBike()) {
		car->bIsBeingCarJacked = true;
	}

	m_pMyVehicle = (CVehicle*)m_pSeekTarget;
	m_pMyVehicle->RegisterReference((CEntity**) &m_pMyVehicle);
	((CVehicle*)m_pSeekTarget)->m_nNumGettingIn++;
	bUsesCollision = false;
	CVector doorOpenPos = GetPositionToOpenCarDoor(car, m_vehDoor, this);

	// Because buses have stairs
	if (!m_pMyVehicle->bIsBus)
		zDiff = Max(0.0f, doorOpenPos.z - GetPosition().z);

	m_vecOffsetSeek = doorOpenPos - GetPosition();
	m_nPedStateTimer = CTimer::GetTimeInMilliseconds() + 600;

	if (car->IsBoat()) {
		if (car->m_pHandlingData->m_nModelFlags & VEHICLE_HANDLING_MODEL_SIT_IN_BOAT)
			m_pVehicleAnim = CAnimManager::BlendAnimation(GetClump(), GetVehicleAnimGroup(m_pMyVehicle, ANIM_ID_CAR_SIT), ANIM_ID_CAR_SIT, 100.0f);
		else
			m_pVehicleAnim = CAnimManager::BlendAnimation(GetClump(), ASSOCGRP_DEFAULT, ANIM_ID_DRIVE_BOAT, 100.0f);

		PedSetInCarCB(nil, this);
		bVehExitWillBeInstant = true;

	} else if (car->IsBike()) {
		PedAnimAlignCB(0, this);

	} else {
		if (m_vehDoor == CAR_DOOR_LF || m_vehDoor == CAR_DOOR_LR)
			m_pVehicleAnim = CAnimManager::BlendAnimation(GetClump(), GetVehicleAnimGroup(m_pMyVehicle, zDiff > 4.4f ? ANIM_ID_CAR_ALIGNHI_RHS : ANIM_ID_CAR_ALIGN_RHS), zDiff > 4.4f ? ANIM_ID_CAR_ALIGNHI_RHS : ANIM_ID_CAR_ALIGN_RHS, 4.0f);
		else
			m_pVehicleAnim = CAnimManager::BlendAnimation(GetClump(), GetVehicleAnimGroup(m_pMyVehicle, zDiff > 4.4f ? ANIM_ID_CAR_ALIGNHI_LHS : ANIM_ID_CAR_ALIGN_LHS), zDiff > 4.4f ? ANIM_ID_CAR_ALIGNHI_LHS : ANIM_ID_CAR_ALIGN_LHS, 4.0f);
		if(!ContinueVehicleEntryWithAnimation(this, PedAnimAlignCB, "align"))
			return;
	}
}

void
CPed::EnterCar(void)
{
	if (IsNotInWreckedVehicle() && m_fHealth > 0.0f) {
		CVehicle *veh = m_pMyVehicle;

		// Not used.
		// CVector posForDoor = GetPositionToOpenCarDoor(veh, m_vehDoor);

		if (veh->CanPedOpenLocks(this)) {
			if (m_vehDoor && m_pVehicleAnim) {
				veh->ProcessOpenDoor(m_vehDoor, m_pVehicleAnim->animId, m_pVehicleAnim->currentTime);
			}
		}
		bIsInTheAir = false;
		LineUpPedWithCar(LINE_UP_TO_CAR_START);
		if (veh->IsBike()) {
			CBike *bike = (CBike*)veh;
			if (bike->GetStatus() == STATUS_ABANDONED && !bike->bIsBeingPickedUp && m_pVehicleAnim) {
				int anim = m_pVehicleAnim->animId;

				// One is pickup and other one is pullup, not same :p
				if ((anim == ANIM_ID_BIKE_PICKUPR || anim == ANIM_ID_BIKE_PICKUPL) && m_pVehicleAnim->currentTime > 0.4667f)
					bike->bIsBeingPickedUp = true;
				else if ((anim == ANIM_ID_BIKE_PULLUPR || anim == ANIM_ID_BIKE_PULLUPL) && m_pVehicleAnim->currentTime > 0.4667f)
					bike->bIsBeingPickedUp = true;
			} else if (m_nPedState == PED_CARJACK && m_pVehicleAnim) {
				if (m_pVehicleAnim->currentTime > 0.4f && m_pVehicleAnim->currentTime - m_pVehicleAnim->timeStep <= 0.4f) {
					int anim = m_pVehicleAnim->animId;
					if (anim == ANIM_ID_CAR_GETIN_BIKE_FRONT) {
						DMAudio.PlayOneShot(m_audioEntityId, SOUND_187, 3.0f);
					} else if (anim == ANIM_ID_BIKE_ELBOWR || anim == ANIM_ID_BIKE_ELBOWL) {
						DMAudio.PlayOneShot(m_audioEntityId, SOUND_186, 3.0f);
					}
				}
			}
		}
	} else {
		QuitEnteringCar();
		SetDie();
	}
}

void
CPed::QuitEnteringCar(void)
{
	CVehicle *veh = m_pMyVehicle;
	if (m_pVehicleAnim)
		m_pVehicleAnim->blendDelta = -1000.0f;
	
	RestartNonPartialAnims();

	if (!RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_IDLE))
		CAnimManager::BlendAnimation(GetClump(), m_animGroup, ANIM_ID_IDLE, 100.0f);
	
	if (veh) {
		const bool wasJacking = m_objective == OBJECTIVE_ENTER_CAR_AS_DRIVER || m_nPedState == PED_CARJACK;
		if (wasJacking)
			veh->bIsBeingCarJacked = false;

		if (veh->m_nNumGettingIn != 0)
			veh->m_nNumGettingIn--;

		if (m_objective == OBJECTIVE_ENTER_CAR_AS_DRIVER || m_objective == OBJECTIVE_ENTER_CAR_AS_PASSENGER)
			RestorePreviousObjective();

		// Clearing the getting-in flag for this door is CCarEnterExit's job in
		// SA: it knows the bike/tandem flag widening that a four-door car does
		// not need.
		if (veh->IsBike()) {
			veh->m_nGettingInFlags &= ~GetBikeDoorFlag(m_vehDoor);
			((CBike*)veh)->bIsBeingPickedUp = false;
		} else
			veh->m_nGettingInFlags &= ~(uint8)CCarEnterExit::ComputeDoorFlag(veh, m_vehDoor, true);
	}

	bUsesCollision = true;

	if (DyingOrDead()) {
		if (m_pVehicleAnim) {
			m_pVehicleAnim->blendDelta = -4.0f;
			m_pVehicleAnim->flags |= ASSOC_DELETEFADEDOUT;
			m_pVehicleAnim->flags &= ~ASSOC_RUNNING;
		}
	} else
		SetIdle();

	m_pVehicleAnim = nil;
	
}

void
CPed::SetExitBoat(CVehicle *boat)
{
	SetPedState(PED_IDLE);
	CVector newPos = GetPosition();
	RemoveInCarAnims();
	CColModel* boatCol = boat->GetColModel();
	if (boat->IsUpsideDown()) {
		newPos = CVector(0.0f, 0.0f, boatCol->boundingBox.min.z);
		newPos = boat->GetMatrix() * newPos;
		newPos.z += 1.0f;
		m_vehDoor = CAR_DOOR_RF;
		PedSetOutCarCB(nil, this);
		bIsStanding = true;
		m_pCurSurface = boat;
		m_pCurSurface->RegisterReference((CEntity**)&m_pCurSurface);
		m_pCurrentPhysSurface = boat;
	} else {
		if (boat->m_modelIndex == MI_SKIMMER) {
			if (!boat->bIsInWater) {
				m_vehDoor = CAR_DOOR_RF;
				PedSetOutCarCB(nil, this);
				bIsStanding = true;
				SetMoveState(PEDMOVE_STILL);
				bTryingToReachDryLand = true;
				float upMult = 1.04f + boatCol->boundingBox.min.z;
				float rightMult = 0.6f * boatCol->boundingBox.max.x;
				newPos = upMult * boat->GetUp() + rightMult * boat->GetRight() + boat->GetPosition();
				SetPosition(newPos);
				if (m_pMyVehicle) {
					PositionPedOutOfCollision();
				} else {
					m_pMyVehicle = boat;
					PositionPedOutOfCollision();
					m_pMyVehicle = nil;
				}
				return;
			}

			newPos.z += 2.0f;
		}
		m_vehDoor = CAR_DOOR_RF;
		PedSetOutCarCB(nil, this);
		bIsStanding = true;
		m_pCurSurface = boat;
		m_pCurSurface->RegisterReference((CEntity**)&m_pCurSurface);
		m_pCurrentPhysSurface = boat;
		CColPoint foundCol;
		CEntity *foundEnt = nil;
		if (CWorld::ProcessVerticalLine(newPos, newPos.z - 1.4f, foundCol, foundEnt, false, true, false, false, false, false, nil))
			newPos.z = FEET_OFFSET + foundCol.point.z;
	}
	SetPosition(newPos);
	SetMoveState(PEDMOVE_STILL);
	m_vecMoveSpeed = boat->m_vecMoveSpeed;
}

// wantedDoorNode = 0 means that func. will determine it
void
CPed::SetExitCar(CVehicle *veh, uint32 wantedDoorNode)
{
	uint32 optedDoorNode = wantedDoorNode;
	bool teleportNeeded = false;
	bool isLow = !!veh->bLowVehicle;

	bool canJumpOut = false;
	if (!veh->CanPedExitCar(false)) {
		if (IsPlayer()) {
			canJumpOut = veh->IsBike() ? veh->CanPedJumpOffBike() : veh->CanPedJumpOutCar(this);
		}
		if (!canJumpOut) {
			if (veh->pDriver) {
				if (veh->pDriver->IsPlayer()) {
					if (veh->pDriver != this) {
						m_leaveCarTimer = CTimer::GetTimeInMilliseconds() + 5000;
					}
				}
			}
			return;
		}
	}

	if (m_nPedState == PED_EXIT_CAR || m_nPedState == PED_DRAGGED_FROM_CAR)
		return;

	m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f);
	m_vecTurnSpeed = CVector(0.0f, 0.0f, 0.0f);
	if (wantedDoorNode == 0) {
		optedDoorNode = CAR_DOOR_LF;
		if (veh->IsBike()) {
			if (canJumpOut) {
				optedDoorNode = veh->pPassengers[0] == this ? CAR_BUMP_REAR : CAR_BOOT;
			} else if (veh->pPassengers[0] == this) {
				optedDoorNode = CAR_DOOR_LR;
			} else {
				optedDoorNode = CAR_DOOR_LF;
			}
		} else if (veh->bIsBus) {
			optedDoorNode = CAR_DOOR_LF;
		} else if (veh->pDriver == this) {
			optedDoorNode = CAR_DOOR_LF;
		} else if (veh->pPassengers[0] == this) {
			optedDoorNode = CAR_DOOR_RF;
		} else if (veh->pPassengers[1] == this) {
			optedDoorNode = CAR_DOOR_LR;
		} else if (veh->pPassengers[2] == this) {
			optedDoorNode = CAR_DOOR_RR;
		} else {
			for (int i = 3; i < veh->m_nNumMaxPassengers; ++i) {
				if (veh->pPassengers[i] == this) {
					if (i & 1)
						optedDoorNode = CAR_DOOR_RR;
					else
						optedDoorNode = CAR_DOOR_LR;

					break;
				}
			}
		}
	}
	bool someoneExitsFromOurExitDoor = false;
	bool someoneEntersFromOurExitDoor = false;
	if (veh->IsBike()) {
		switch (optedDoorNode) {
			case CAR_BUMP_REAR:
			case CAR_DOOR_RR:
			case CAR_DOOR_LR:
				if (veh->m_nGettingInFlags & (CAR_DOOR_FLAG_LR | CAR_DOOR_FLAG_RR))
					someoneEntersFromOurExitDoor = true;
				break;
			case CAR_DOOR_RF:
			case CAR_DOOR_LF:
			case CAR_BOOT:
				if (veh->m_nGettingInFlags & (CAR_DOOR_FLAG_LF | CAR_DOOR_FLAG_RF))
					someoneEntersFromOurExitDoor = true;
				break;
			default:
				break;
		}
	} else {
		switch (optedDoorNode) {
			case CAR_DOOR_RF:
				if (veh->m_nGettingInFlags & CAR_DOOR_FLAG_RF)
					someoneEntersFromOurExitDoor = true;
				if (veh->m_nGettingOutFlags & CAR_DOOR_FLAG_RF)
					someoneExitsFromOurExitDoor = true;
				break;
			case CAR_DOOR_RR:
				if (veh->m_nGettingInFlags & CAR_DOOR_FLAG_RR)
					someoneEntersFromOurExitDoor = true;
				if (veh->m_nGettingOutFlags & CAR_DOOR_FLAG_RR)
					someoneExitsFromOurExitDoor = true;
				break;
			case CAR_DOOR_LF:
				if (veh->m_nGettingInFlags & CAR_DOOR_FLAG_LF)
					someoneEntersFromOurExitDoor = true;
				if (veh->m_nGettingOutFlags & CAR_DOOR_FLAG_LF)
					someoneExitsFromOurExitDoor = true;
				break;
			case CAR_DOOR_LR:
				if (veh->m_nGettingInFlags & CAR_DOOR_FLAG_LR)
					someoneEntersFromOurExitDoor = true;
				if (veh->m_nGettingOutFlags & CAR_DOOR_FLAG_LR)
					someoneExitsFromOurExitDoor = true;
				break;
			default:
				break;
		}
	}
	if (someoneEntersFromOurExitDoor && m_objective == OBJECTIVE_LEAVE_CAR) {
		RestorePreviousObjective();
		return;
	}
	if (!someoneExitsFromOurExitDoor) {
		bool thereIsRoom;
		if (canJumpOut) {
			thereIsRoom = 1;
		} else {
			// Again, unused...
			// CVector exitPos = GetPositionToOpenCarDoor(veh, optedDoorNode);
			thereIsRoom = veh->IsRoomForPedToLeaveCar(optedDoorNode, nil);
		}

		if (!thereIsRoom) {
			bool trySideSeat = false;
			CPed *pedOnSideSeat;
			int firstOptedDoor = optedDoorNode;
			if (veh->IsBike()) {
				switch (optedDoorNode) {
					case CAR_DOOR_RF:
						optedDoorNode = CAR_DOOR_LF;
						break;
					case CAR_DOOR_RR:
						optedDoorNode = CAR_DOOR_LR;
						break;
					case CAR_DOOR_LF:
						optedDoorNode = CAR_DOOR_RF;
						break;
					case CAR_DOOR_LR:
						optedDoorNode = CAR_DOOR_RR;
						break;
					default:
						break;
				}
			} else {
				switch (optedDoorNode) {
					case CAR_DOOR_RF:
						if (veh->pDriver || veh->m_nGettingInFlags & CAR_DOOR_FLAG_LF) {
							pedOnSideSeat = veh->pDriver;
							trySideSeat = true;
						} else
							optedDoorNode = CAR_DOOR_LF;

						break;
					case CAR_DOOR_RR:
						if (veh->pPassengers[1] || veh->m_nGettingInFlags & CAR_DOOR_FLAG_LR) {
							pedOnSideSeat = veh->pPassengers[1];
							trySideSeat = true;
						} else
							optedDoorNode = CAR_DOOR_LR;

						break;
					case CAR_DOOR_LF:
						if (veh->pPassengers[0] || veh->m_nGettingInFlags & CAR_DOOR_FLAG_RF) {
							pedOnSideSeat = veh->pPassengers[0];
							trySideSeat = true;
						} else
							optedDoorNode = CAR_DOOR_RF;

						break;
					case CAR_DOOR_LR:
						if (veh->pPassengers[2] || veh->m_nGettingInFlags & CAR_DOOR_FLAG_RR) {
							pedOnSideSeat = (CPed*)veh->pPassengers[2];
							trySideSeat = true;
						} else
							optedDoorNode = CAR_DOOR_RR;

						break;
					default:
						break;
				}
			}
			if (trySideSeat) {
				if (!pedOnSideSeat || !IsPlayer() && CharCreatedBy != MISSION_CHAR)
					return;

				switch (optedDoorNode) {
					case CAR_DOOR_RF:
						optedDoorNode = CAR_DOOR_LF;
						break;
					case CAR_DOOR_RR:
						optedDoorNode = CAR_DOOR_LR;
						break;
					case CAR_DOOR_LF:
						optedDoorNode = CAR_DOOR_RF;
						break;
					case CAR_DOOR_LR:
						optedDoorNode = CAR_DOOR_RR;
						break;
					default:
						break;
				}
			}
			// ...
			// CVector exitPos = GetPositionToOpenCarDoor(veh, optedDoorNode);
			if (!veh->IsRoomForPedToLeaveCar(optedDoorNode, nil)) {
				if (!IsPlayer() && CharCreatedBy != MISSION_CHAR)
					return;

				// needed for PositionPedOutOfCollision()
				optedDoorNode = firstOptedDoor;
				m_vehDoor = firstOptedDoor;
				PositionPedOutOfCollision();
				teleportNeeded = true;
			}
		}

		if (!teleportNeeded && veh->IsOnItsSide()) {
			m_vehDoor = optedDoorNode;
			PositionPedOutOfCollision();
			teleportNeeded = true;
		}

		m_nLastPedState = PED_IDLE;
		m_nPrevMoveState = PEDMOVE_STILL;
		SetMoveState(PEDMOVE_STILL);

		bUsesCollision = false;
		m_pSeekTarget = veh;
		m_pSeekTarget->RegisterReference((CEntity**) &m_pSeekTarget);
		m_vehDoor = optedDoorNode;
		SetPedState(PED_EXIT_CAR);
		if (m_pVehicleAnim && m_pVehicleAnim->flags & ASSOC_PARTIAL)
			m_pVehicleAnim->blendDelta = -1000.0f;
		RemoveInCarAnims();
		m_pVehicleAnim = nil; // never reuse a fading in-car association as an exit clip
		SetMoveState(PEDMOVE_NONE);
		CAnimManager::BlendAnimation(GetClump(), m_animGroup, ANIM_ID_IDLE, 100.0f);

		if (teleportNeeded) {
			PedSetOutCarCB(nil, this);
		} else {
			if (veh->GetUp().z <= -0.8f && !veh->IsBike()) {
				if (m_vehDoor == CAR_DOOR_RF || m_vehDoor == CAR_DOOR_RR) {
					m_pVehicleAnim = CAnimManager::AddAnimation(GetClump(), ASSOCGRP_DEFAULT, ANIM_ID_DEFAULT_CAR_CRAWLOUTRHS_0);
				} else if (m_vehDoor == CAR_DOOR_LF || m_vehDoor == CAR_DOOR_LR) {
					m_pVehicleAnim = CAnimManager::AddAnimation(GetClump(), ASSOCGRP_DEFAULT, ANIM_ID_DEFAULT_CAR_CRAWLOUTRHS_0);
				}
				if(m_pVehicleAnim)
					m_pVehicleAnim->SetFinishCallback(PedSetOutCarCB, this);

			} else if (veh->IsBike()) {
				CBike* bike = (CBike*)veh;
				switch (m_vehDoor) {
					case CAR_BUMP_REAR:
					case CAR_BOOT:
						m_pVehicleAnim = CAnimManager::AddAnimation(GetClump(), bike->m_bikeAnimType, ANIM_ID_UNKNOWN_26);
						break;
					case CAR_DOOR_RF:
					case CAR_DOOR_RR:
						m_pVehicleAnim = CAnimManager::AddAnimation(GetClump(), bike->m_bikeAnimType, ANIM_ID_CAR_GETOUT_RHS_0);
						break;
					case CAR_DOOR_LF:
					case CAR_DOOR_LR:
						m_pVehicleAnim = CAnimManager::AddAnimation(GetClump(), bike->m_bikeAnimType, ANIM_ID_CAR_GETOUT_LHS_0);
						break;
					default:
						break;
				}
				int8 exitFlags = 0;

				// Bike door flags incl. passenger jump off
				switch (m_vehDoor) {
					case CAR_BUMP_REAR:
					case CAR_DOOR_RR:
					case CAR_DOOR_LR:
						exitFlags = CAR_DOOR_FLAG_RR | CAR_DOOR_FLAG_LR;
						break;
					case CAR_DOOR_RF:
					case CAR_DOOR_LF:
					case CAR_BOOT:
						exitFlags = CAR_DOOR_FLAG_RF | CAR_DOOR_FLAG_LF;
						break;
				}

				// Passenger get off
				if (m_vehDoor == CAR_BUMP_REAR || m_vehDoor == CAR_BOOT) {
					if(m_pVehicleAnim)
						m_pVehicleAnim->SetFinishCallback(RestoreHeadingRateCB, this);
					m_headingRate = 0.0f;

				} else {
					veh->m_nGettingOutFlags |= exitFlags;
					if(m_pVehicleAnim)
						m_pVehicleAnim->SetFinishCallback(PedAnimStepOutCarCB, this);
				}

			} else {
				switch (m_vehDoor) {
					case CAR_DOOR_RF:
						if (canJumpOut) {
							m_pVehicleAnim = CAnimManager::AddAnimation(GetClump(), ASSOCGRP_DEFAULT, ANIM_ID_DEFAULT_CAR_ROLLOUT_LHS);
						} else {
							// The model carries the set its occupants use, so a
							// light car steps out the way a light car should and
							// only a real coach uses the coach set.
							m_pVehicleAnim = CAnimManager::AddAnimation(GetClump(),
								GetVehicleAnimGroup(veh, ANIM_ID_CAR_GETOUT_RHS_0), ANIM_ID_CAR_GETOUT_RHS_0);
						}
						break;
					case CAR_DOOR_RR:
						if (canJumpOut) {
							m_pVehicleAnim = CAnimManager::AddAnimation(GetClump(), ASSOCGRP_DEFAULT, ANIM_ID_DEFAULT_CAR_ROLLOUT_LHS);
						} else if (veh->bIsVan) {
							m_pVehicleAnim = CAnimManager::AddAnimation(GetClump(), ASSOCGRP_VANCARANIMS, ANIM_ID_CAR_GETOUT_RHS_1);
						} else if (isLow) {
							m_pVehicleAnim = CAnimManager::AddAnimation(GetClump(), GetVehicleAnimGroup(veh, ANIM_ID_CAR_GETOUT_RHS_0), ANIM_ID_CAR_GETOUT_RHS_0);
						} else {
							m_pVehicleAnim = CAnimManager::AddAnimation(GetClump(), GetVehicleAnimGroup(veh, ANIM_ID_CAR_GETOUT_RHS_0), ANIM_ID_CAR_GETOUT_RHS_0);
						}
						break;
					case CAR_DOOR_LF:
						if (canJumpOut) {
							m_pVehicleAnim = CAnimManager::AddAnimation(GetClump(), ASSOCGRP_DEFAULT, ANIM_ID_DEFAULT_CAR_ROLLOUT_LHS);
						} else if (veh->bIsBus) {
							m_pVehicleAnim = CAnimManager::AddAnimation(GetClump(), ASSOCGRP_COACHCARANIMS, ANIM_ID_CAR_GETOUT_LHS_0);
						} else {
							if (isLow)
								m_pVehicleAnim = CAnimManager::AddAnimation(GetClump(), GetVehicleAnimGroup(veh, ANIM_ID_CAR_GETOUT_LHS_0), ANIM_ID_CAR_GETOUT_LHS_0);
							else
								m_pVehicleAnim = CAnimManager::AddAnimation(GetClump(), GetVehicleAnimGroup(veh, ANIM_ID_CAR_GETOUT_LHS_0), ANIM_ID_CAR_GETOUT_LHS_0);
						}
						break;
					case CAR_DOOR_LR:
						if (canJumpOut) {
							m_pVehicleAnim = CAnimManager::AddAnimation(GetClump(), ASSOCGRP_DEFAULT, ANIM_ID_DEFAULT_CAR_ROLLOUT_LHS);
						} else if (veh->bIsVan) {
							m_pVehicleAnim = CAnimManager::AddAnimation(GetClump(), ASSOCGRP_VANCARANIMS, ANIM_ID_CAR_GETOUT_LHS_1);
						} else if (isLow) {
							m_pVehicleAnim = CAnimManager::AddAnimation(GetClump(), GetVehicleAnimGroup(veh, ANIM_ID_CAR_GETOUT_LHS_0), ANIM_ID_CAR_GETOUT_LHS_0);
						} else {
							m_pVehicleAnim = CAnimManager::AddAnimation(GetClump(), GetVehicleAnimGroup(veh, ANIM_ID_CAR_GETOUT_LHS_0), ANIM_ID_CAR_GETOUT_LHS_0);
						}
						break;
					default:
						break;
				}
				if (!canJumpOut) {
					veh->m_nGettingOutFlags |= GetCarDoorFlag(m_vehDoor);
				}
				if(m_pVehicleAnim)
					m_pVehicleAnim->SetFinishCallback(canJumpOut ? RestoreHeadingRateCB : PedAnimStepOutCarCB, this);
			}
			if(!m_pVehicleAnim){
				REVC_LOG_WARNING("Vehicle exit: missing animation (ped=%d, vehicle=%d, door=%d)",
					GetModelIndex(), veh->GetModelIndex(), m_vehDoor);
				if(veh->IsBike())
					RestoreHeadingRate();
				PedSetOutCarCB(nil, this); // already supports an absent association
			}
		}
		bChangedSeat = false;
		if (veh->bIsBus)
			bRenderPedInCar = true;

		if (veh->pDriver == this) {
			if (IsPlayer())
				veh->SetStatus(STATUS_PLAYER_DISABLED);
			else
				veh->SetStatus(STATUS_ABANDONED);
		}
	}
}

void
CPed::ExitCar(void)
{
	if (!m_pVehicleAnim) {
		if (InVehicle()) {
			if (m_pMyVehicle->IsBike()) {
				if (m_vehDoor == CAR_BOOT || m_vehDoor == CAR_BUMP_REAR) {
					((CBike*)m_pMyVehicle)->KnockOffRider(WEAPONTYPE_UNARMED, 0, this, false);
				}
			} else if (m_pMyVehicle->IsAutomobile()) {
				if (RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_DEFAULT_CAR_ROLLOUT_LHS)) {
					((CAutomobile*)m_pMyVehicle)->KnockPedOutCar(WEAPONTYPE_UNIDENTIFIED, CAR_DOOR_LF, this);
				} else if (RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_DEFAULT_CAR_ROLLOUT_LHS)) {
					((CAutomobile*)m_pMyVehicle)->KnockPedOutCar(WEAPONTYPE_UNIDENTIFIED, CAR_DOOR_RF, this);
				}
			}
		}
		return;
	}

	AnimationId exitAnim = (AnimationId) m_pVehicleAnim->animId;
	float animTime = m_pVehicleAnim->currentTime;

	if (exitAnim == ANIM_ID_UNKNOWN_26) {
		if (animTime > 0.35f && m_pMyVehicle && m_pMyVehicle->IsBike())
			((CBike*)m_pMyVehicle)->KnockOffRider(WEAPONTYPE_UNARMED, 0, this, false);
		else
			LineUpPedWithCar(LINE_UP_TO_CAR_FALL);

	} else if (exitAnim == ANIM_ID_DEFAULT_CAR_ROLLOUT_LHS || exitAnim == ANIM_ID_DEFAULT_CAR_ROLLOUT_LHS) {
		if (animTime > 0.07f && m_pMyVehicle && m_pMyVehicle->IsAutomobile()) {
			if (exitAnim == ANIM_ID_DEFAULT_CAR_ROLLOUT_LHS) {
				((CAutomobile*)m_pMyVehicle)->KnockPedOutCar(WEAPONTYPE_UNIDENTIFIED, CAR_DOOR_LF, this);
			} else {
				((CAutomobile*)m_pMyVehicle)->KnockPedOutCar(WEAPONTYPE_UNIDENTIFIED, CAR_DOOR_RF, this);
			}
		} else {
			LineUpPedWithCar(LINE_UP_TO_CAR_FALL);
		}
	} else {
		m_pMyVehicle->ProcessOpenDoor(m_vehDoor, exitAnim, animTime);

		if (m_pSeekTarget) {
			// Car is upside down
			if (m_pMyVehicle->GetUp().z > -0.8f) {
				if (exitAnim == ANIM_ID_CAR_CLOSE_RHS_0 || exitAnim == ANIM_ID_CAR_CLOSE_LHS_0 || animTime > 0.3f)
					LineUpPedWithCar(LINE_UP_TO_CAR_END);
				else
					LineUpPedWithCar((m_pMyVehicle->GetModelIndex() == MI_DODO ? LINE_UP_TO_CAR_END : LINE_UP_TO_CAR_START));
			}
			else {
				LineUpPedWithCar(LINE_UP_TO_CAR_END);
			}
		}

		// If there is someone in front of the door, make him fall while we exit.
		if (m_nPedState == PED_EXIT_CAR) {
			CPed *nearPeds[10];
			int16 numNearPeds = FindNearPeds(GetPosition(), 0.2f, nearPeds, ARRAY_SIZE(nearPeds));
			CPed* foundPed = numNearPeds > 0 ? nearPeds[0] : nil;
			if (foundPed && !foundPed->IsPlayer())
				if (animTime > 0.4f && foundPed->IsPedInControl())
					foundPed->SetFall(1000, ANIM_ID_KO_SKID_FRONT, 1);
		}
	}
}

CVector
CPed::GetPositionToOpenCarDoor(CVehicle *veh, uint32 component, CPed *ped)
{
	CVector doorPos;
	CVector vehDoorOffset;
	CVehicleModelInfo* vehModel = veh->GetModelInfo();

	if (veh->IsBike()) {
		CBike* bike = (CBike*)veh;
		CVector kickOffset = GetPedAnimationOffset(ped, ASSOCGRP_HARLEYBIKEANIMS, ANIM_ID_CAR_GETIN_BIKE_FRONT, vecPedBikeKickAnimOffset);

		if (component == CAR_WINDSCREEN) {
			doorPos = vehModel->GetFrontSeatPosn();
			return bike->GetMatrix() * (doorPos +
				CVector(-kickOffset.x, kickOffset.y, -kickOffset.z));
		} else {
			switch (bike->m_bikeAnimType) {
			case ASSOCGRP_VESPABIKEANIMS:
				vehDoorOffset = GetPedAnimationOffset(ped, ASSOCGRP_VESPABIKEANIMS, ANIM_ID_CAR_GETIN_LHS_0, vecPedVespaBikeJumpRhsAnimOffset);
				break;
			case ASSOCGRP_HARLEYBIKEANIMS:
				vehDoorOffset = GetPedAnimationOffset(ped, ASSOCGRP_HARLEYBIKEANIMS, ANIM_ID_CAR_GETIN_LHS_0, vecPedHarleyBikeJumpRhsAnimOffset);
				break;
			case ASSOCGRP_DIRTBIKEANIMS:
				vehDoorOffset = GetPedAnimationOffset(ped, ASSOCGRP_DIRTBIKEANIMS, ANIM_ID_CAR_GETIN_LHS_0, vecPedDirtBikeJumpRhsAnimOffset);
				break;
			default:
				vehDoorOffset = GetPedAnimationOffset(ped, ASSOCGRP_STDBIKEANIMS, ANIM_ID_CAR_GETIN_LHS_0, vecPedStdBikeJumpRhsAnimOffset);
				break;
			}
		}

		doorPos = vehModel->GetFrontSeatPosn();
		if (component == CAR_DOOR_LR || component == CAR_DOOR_RR) {
			doorPos = vehModel->m_positions[CAR_POS_BACKSEAT];
		}

		if (component == CAR_DOOR_LR || component == CAR_DOOR_LF) {
			vehDoorOffset.x *= -1.f;
		}

		CVector correctedPos;
		bike->GetCorrectedWorldDoorPosition(correctedPos, vehDoorOffset, doorPos);
		return correctedPos;
	} else {
		float seatOffset;
		if (veh->bIsVan && (component == CAR_DOOR_LR || component == CAR_DOOR_RR)) {
			seatOffset = 0.0f;
			vehDoorOffset = GetPedAnimationOffset(ped, ASSOCGRP_VANCARANIMS, ANIM_ID_CAR_GETIN_LHS_1, vecPedVanRearDoorAnimOffset);
		} else {
			seatOffset = veh->m_pHandlingData->m_fSeatOffsetDistance;
			if (veh->bLowVehicle) {
				vehDoorOffset = GetPedAnimationOffset(ped, ASSOCGRP_LOWCARAMIMS, ANIM_ID_CAR_GETIN_LHS_0, vecPedCarDoorLoAnimOffset);
			} else {
				vehDoorOffset = GetPedAnimationOffset(ped, ASSOCGRP_STDCARAMIMS, ANIM_ID_CAR_GETIN_LHS_0, vecPedCarDoorAnimOffset);
			}
		}

		switch (component) {
		case CAR_DOOR_RF:
			doorPos = vehModel->GetFrontSeatPosn();
			doorPos.x += seatOffset;
			vehDoorOffset.x = -vehDoorOffset.x;
			break;

		case CAR_DOOR_RR:
			doorPos = vehModel->m_positions[CAR_POS_BACKSEAT];
			doorPos.x += seatOffset;
			vehDoorOffset.x = -vehDoorOffset.x;
			break;

		case CAR_DOOR_LF:
			doorPos = vehModel->GetFrontSeatPosn();
			doorPos.x += seatOffset;
			doorPos.x = -doorPos.x;
			break;

		case CAR_DOOR_LR:
			doorPos = vehModel->m_positions[CAR_POS_BACKSEAT];
			doorPos.x += seatOffset;
			doorPos.x = -doorPos.x;
			break;

		default:
			doorPos = vehModel->GetFrontSeatPosn();
			vehDoorOffset = CVector(0.0f, 0.0f, 0.0f);
			break;
		}

		CVector diffVec = doorPos - vehDoorOffset;
		return Multiply3x3(veh->GetMatrix(), diffVec) + veh->GetPosition();

		//unused
		//doorPos = Multiply3x3(veh->GetMatrix(), doorPos) + veh->GetMatrix();
	}
}

void
CPed::GetNearestDoor(CVehicle *veh, CVector &posToOpen)
{
	// SA has no CPed::GetNearestDoor: the task asks CCarEnterExit for the door
	// it should walk to, and that one function covers the bike jump-on side,
	// the towed-vehicle blocked side and the driver-dragged-out offsets that
	// reVC used to spell out here. Delegate, and always leave a usable
	// position behind - the callers seek to it unconditionally.
	posToOpen = GetPositionToOpenCarDoor(veh, CAR_DOOR_RF, this);

	int32 doorId = CAR_NODE_NONE;
	if(CCarEnterExit::GetNearestCarDoor(this, veh, posToOpen, doorId) && doorId > 0)
		m_vehDoor = (uint16)doorId;
}

bool
CPed::GetNearestPassengerDoor(CVehicle *veh, CVector &posToOpen)
{
	// SA asks CCarEnterExit for this; the door choice, the tandem-seat test and
	// the "is there room" test all live there now. The caller here wants the
	// door to be enterable and the space next to it free.
	int32 doorId = CAR_NODE_NONE;
	const bool ok = CCarEnterExit::GetNearestCarPassengerDoor(this, veh, &posToOpen, &doorId, true, true, true);
	if(ok && doorId > 0)
		m_vehDoor = (uint16)doorId;
	return ok;
}

void
CPed::GoToNearestDoor(CVehicle *veh)
{
	CVector posToOpen;
	GetNearestDoor(veh, posToOpen);
	SetSeek(posToOpen, 0.5f);
	SetMoveState(PEDMOVE_RUN);
}

static CVector
GetAnimationLastTranslation(AssocGroupId groupId, AnimationId animId, RpClump *clump)
{
	CAnimBlendHierarchy *hier = clump ?
		CAnimManager::GetAnimHierarchyForOffset(clump, groupId, animId) :
		CAnimManager::GetAnimHierarchyForOffset(groupId, animId);
	if(hier == nil || hier->sequences == nil || hier->numSequences <= 0)
		return CVector(0.0f, 0.0f, 0.0f);
	CAnimManager::UncompressAnimation(hier);
	// SA IFPs can put non-root sequences before the root. Never use an arm's
	// translation to align a ped with a vehicle door.
	CAnimBlendSequence *root = nil;
	for(int i = 0; i < hier->numSequences; i++){
		CAnimBlendSequence *seq = &hier->sequences[i];
		const char *name = seq->name;
		while(*name == ' ') name++;
		if(seq->boneTag == BONE_ROOT || strncasecmp(name, "Root", 4) == 0){
			root = seq;
			break;
		}
	}
	if(root == nil && clump == nil)
		root = hier->sequences; // native VC's original first-sequence convention
	if(root == nil || root->numFrames <= 0 || !root->HasTranslation())
		return CVector(0.0f, 0.0f, 0.0f);
	KeyFrameTrans *lastFrame = (KeyFrameTrans*)root->GetKeyFrame(root->numFrames - 1);
	return lastFrame->translation;
}

static CVector
GetPedAnimationOffset(CPed *ped, AssocGroupId groupId, AnimationId animId, const CVector &nativeOffset)
{
	(void)nativeOffset;
	return GetAnimationLastTranslation(groupId, animId, ped ? ped->GetClump() : nil);
}

void
CPed::SetAnimOffsetForEnterOrExitVehicle(void)
{
	// FIX_BUGS: If there were no translations on enter anims, there were overflows all over this function.

	int animBlocks[] = {
		CAnimManager::GetAnimationBlockIndex("van"),
		CAnimManager::GetAnimationBlockIndex("bikes"),
		CAnimManager::GetAnimationBlockIndex("bikev"),
		CAnimManager::GetAnimationBlockIndex("bikeh"),
		CAnimManager::GetAnimationBlockIndex("biked")
	};
	for(int i = 0; i < ARRAY_SIZE(animBlocks); i++)
		if(animBlocks[i] >= 0)
			CStreaming::RequestAnim(animBlocks[i], STREAMFLAGS_DEPENDENCY);
	CStreaming::LoadAllRequestedModels(false);
	for(int i = 0; i < ARRAY_SIZE(animBlocks); i++)
		if(animBlocks[i] >= 0)
			CAnimManager::AddAnimBlockRef(animBlocks[i]);

	// These offsets come from the root translation of the car clips, so they
	// have to be read out of a car set. The standard group does not declare
	// CAR_GETIN_LHS_0 at all, which used to leave every offset at zero and put
	// the ped at the wrong place when getting in or out.
	vecPedDraggedOutCarAnimOffset = GetAnimationLastTranslation(ASSOCGRP_STDCARAMIMS, ANIM_ID_CAR_JACKEDLHS);
	vecPedCarDoorAnimOffset = GetAnimationLastTranslation(ASSOCGRP_STDCARAMIMS, ANIM_ID_CAR_GETIN_LHS_0);
	vecPedCarDoorLoAnimOffset = GetAnimationLastTranslation(ASSOCGRP_LOWCARAMIMS, ANIM_ID_CAR_GETIN_LHS_0);
	vecPedQuickDraggedOutCarAnimOffset = GetAnimationLastTranslation(ASSOCGRP_STDCARAMIMS, ANIM_ID_CAR_CLOSEDOOR_LHS_0);
	vecPedVanRearDoorAnimOffset = GetAnimationLastTranslation(ASSOCGRP_VANCARANIMS, ANIM_ID_CAR_GETIN_LHS_1);
	vecPedStdBikeJumpRhsAnimOffset = GetAnimationLastTranslation(ASSOCGRP_STDBIKEANIMS, ANIM_ID_CAR_GETIN_BIKE_FRONT);
	vecPedVespaBikeJumpRhsAnimOffset = GetAnimationLastTranslation(ASSOCGRP_VESPABIKEANIMS, ANIM_ID_CAR_GETIN_BIKE_FRONT);
	vecPedHarleyBikeJumpRhsAnimOffset = GetAnimationLastTranslation(ASSOCGRP_HARLEYBIKEANIMS, ANIM_ID_CAR_GETIN_BIKE_FRONT);
	vecPedDirtBikeJumpRhsAnimOffset = GetAnimationLastTranslation(ASSOCGRP_DIRTBIKEANIMS, ANIM_ID_CAR_GETIN_BIKE_FRONT);
	vecPedBikeKickAnimOffset = GetAnimationLastTranslation(ASSOCGRP_HARLEYBIKEANIMS, ANIM_ID_CAR_GETIN_BIKE_FRONT);

	REVC_LOG_INFO("Car approach offsets: normal=(%.3f, %.3f, %.3f) low=(%.3f, %.3f, %.3f); VC group=%d",
		vecPedCarDoorAnimOffset.x, vecPedCarDoorAnimOffset.y, vecPedCarDoorAnimOffset.z,
		vecPedCarDoorLoAnimOffset.x, vecPedCarDoorLoAnimOffset.y, vecPedCarDoorLoAnimOffset.z,
		CAnimManager::GetAnimAssociation(ASSOCGRP_STDCARAMIMS, ANIM_ID_CAR_GETIN_LHS_0) != nil);
	if(vecPedCarDoorAnimOffset.MagnitudeSqr() < 0.01f)
		REVC_LOG_WARNING("CAR_getin_LHS has no root translation: door reachability may fail");

	for(int i = 0; i < ARRAY_SIZE(animBlocks); i++)
		if(animBlocks[i] >= 0)
			CAnimManager::RemoveAnimBlockRef(animBlocks[i]);
}

void
CPed::PedSetQuickDraggedOutCarPositionCB(CAnimBlendAssociation *animAssoc, void *arg)
{
	CPed *ped = (CPed*)arg;

	CVehicle *veh = ped->m_pMyVehicle;

	CVector finalPos;
	CVector draggedOutOffset;

	CMatrix pedMat(ped->GetMatrix());
	ped->bUsesCollision = true;
	ped->RestartNonPartialAnims();
	draggedOutOffset = vecPedQuickDraggedOutCarAnimOffset;
	if (ped->m_vehDoor == CAR_DOOR_RF || ped->m_vehDoor == CAR_DOOR_RR)
		draggedOutOffset.x = -draggedOutOffset.x;

	finalPos = Multiply3x3(pedMat, draggedOutOffset) + ped->GetPosition();
	FindZCoorForPed(&finalPos);
	ped->m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f);
	ped->SetPosition(finalPos);

	if (veh) {
		ped->m_fRotationDest = veh->GetForward().Heading() - HALFPI;
		ped->m_fRotationCur = ped->m_fRotationDest;
		ped->CalculateNewOrientation();

		if (!veh->IsRoomForPedToLeaveCar(ped->m_vehDoor, &vecPedQuickDraggedOutCarAnimOffset))
			ped->PositionPedOutOfCollision();
	}

	if (!ped->CanSetPedState())
		return;

	ped->SetIdle();
}

void
CPed::PedSetDraggedOutCarPositionCB(CAnimBlendAssociation* animAssoc, void* arg)
{
	CPed *ped = (CPed*)arg;

	ped->bUsesCollision = true;
	ped->RestartNonPartialAnims();

	CMatrix pedMat(ped->GetMatrix());
	CVector draggedOutOffset;
	if (ped->m_pMyVehicle && ped->m_pMyVehicle->IsBike()) {
		AssocGroupId animGroup = ((CBike*)ped->m_pMyVehicle)->m_bikeAnimType;
		switch (animGroup) {
			case ASSOCGRP_VESPABIKEANIMS:
				draggedOutOffset = vecPedVespaBikeJumpRhsAnimOffset;
				break;
			case ASSOCGRP_HARLEYBIKEANIMS:
				draggedOutOffset = vecPedHarleyBikeJumpRhsAnimOffset;
				break;
			case ASSOCGRP_DIRTBIKEANIMS:
				draggedOutOffset = vecPedDirtBikeJumpRhsAnimOffset;
				break;
			default:
				draggedOutOffset = vecPedStdBikeJumpRhsAnimOffset;
				break;
		}
	} else {
		// SA CCarEnterExit asks the vehicle's own animation group how far the
		// ped ends up from the seat after being dragged out (0x6E3D10), rather
		// than using one offset for every car: a truck's sill is taller than a
		// saloon's, so the ped lands further out. The mode depends on the door
		// side, which is why the sign flip below applies to the bike offsets
		// only - the group already returns the sided offset.
		const bool rightDoor = ped->m_vehDoor == CAR_DOOR_RF || ped->m_vehDoor == CAR_DOOR_RR;
		if (ped->m_pMyVehicle) {
			draggedOutOffset = CVehicleAnimGroupData::GetAnimDoorOffset((AssocGroupId)ped->m_pMyVehicle->GetAnimGroupId(),
				rightDoor ? JACK_PED_RIGHT : JACK_PED_LEFT);
			draggedOutOffset = CVector(draggedOutOffset.x, draggedOutOffset.y, 0.0f);
		} else {
			draggedOutOffset = CVector(vecPedDraggedOutCarAnimOffset.x, vecPedDraggedOutCarAnimOffset.y, 0.0f);
			if (rightDoor)
				draggedOutOffset.x = -draggedOutOffset.x;
		}
	}

	CVector posAfterBeingDragged = Multiply3x3(pedMat, draggedOutOffset);
	posAfterBeingDragged += ped->GetPosition();
	FindZCoorForPed(&posAfterBeingDragged);
	ped->m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f);
	ped->SetPosition(posAfterBeingDragged);

	if (ped->m_pMyVehicle && !ped->m_pMyVehicle->IsBike() && !ped->m_pMyVehicle->IsRoomForPedToLeaveCar(ped->m_vehDoor, &draggedOutOffset)) {
		ped->PositionPedOutOfCollision();
	}

	if (!ped->CanSetPedState())
		return;
	
	if (!ped->m_pMyVehicle) {
		ped->SetIdle();
		ped->SetGetUp();
		return;
	}

	ped->SetIdle();
	ped->SetGetUp();
}

CVector
CPed::GetPositionToOpenCarDoor(CVehicle *veh, uint32 component, float offset, CPed *ped)
{	
	CVector doorPos;
	CMatrix vehMat(veh->GetMatrix());

	if (veh->IsBike()) {
		CVehicleModelInfo* vehModel = (CVehicleModelInfo*)CModelInfo::GetModelInfo(veh->GetModelIndex());
		CVector vehDoorOffset;
		CBike* bike = (CBike*)veh;
		CVector kickOffset = GetPedAnimationOffset(ped, ASSOCGRP_HARLEYBIKEANIMS, ANIM_ID_CAR_GETIN_BIKE_FRONT, vecPedBikeKickAnimOffset);
		doorPos = vehModel->GetFrontSeatPosn();

		if (component == CAR_WINDSCREEN) {
#ifdef FIX_BUGS
			return bike->GetMatrix() * (doorPos +
				CVector(-kickOffset.x, kickOffset.y, -kickOffset.z));
#else
			return bike->GetMatrix() * (doorPos + kickOffset);
#endif
		} else {
			switch (bike->m_bikeAnimType) {
			case ASSOCGRP_VESPABIKEANIMS:
				vehDoorOffset = GetPedAnimationOffset(ped, ASSOCGRP_VESPABIKEANIMS, ANIM_ID_CAR_GETIN_LHS_0, vecPedVespaBikeJumpRhsAnimOffset);
				break;
			case ASSOCGRP_HARLEYBIKEANIMS:
				vehDoorOffset = GetPedAnimationOffset(ped, ASSOCGRP_HARLEYBIKEANIMS, ANIM_ID_CAR_GETIN_LHS_0, vecPedHarleyBikeJumpRhsAnimOffset);
				break;
			case ASSOCGRP_DIRTBIKEANIMS:
				vehDoorOffset = GetPedAnimationOffset(ped, ASSOCGRP_DIRTBIKEANIMS, ANIM_ID_CAR_GETIN_LHS_0, vecPedDirtBikeJumpRhsAnimOffset);
				break;
			default:
				vehDoorOffset = GetPedAnimationOffset(ped, ASSOCGRP_STDBIKEANIMS, ANIM_ID_CAR_GETIN_LHS_0, vecPedStdBikeJumpRhsAnimOffset);
				break;
			}
			vehDoorOffset.x += offset * bike->m_pHandlingData->m_fSeatOffsetDistance;
			if (component == CAR_DOOR_LR || component == CAR_DOOR_RR) {
				doorPos = vehModel->m_positions[CAR_POS_BACKSEAT];
			}

			if (component == CAR_DOOR_LR || component == CAR_DOOR_LF)
				vehDoorOffset.x *= -1.f;

			CVector correctedPos;
			bike->GetCorrectedWorldDoorPosition(correctedPos, vehDoorOffset, doorPos);
			return correctedPos;
		}
	}
	doorPos = Multiply3x3(vehMat, GetLocalPositionToOpenCarDoor(veh, component, offset, ped));

	return veh->GetPosition() + doorPos;
}

CVector
CPed::GetLocalPositionToOpenCarDoor(CVehicle *veh, uint32 component, float seatPosMult, CPed *ped)
{
	CVehicleModelInfo *vehModel; 
	CVector vehDoorPos;
	CVector vehDoorOffset;
	float seatOffset;

	vehModel = (CVehicleModelInfo *)CModelInfo::GetModelInfo(veh->GetModelIndex());

	if (veh->IsBike()) {
		CBike *bike = (CBike*)veh;
		CVector kickOffset = GetPedAnimationOffset(ped, ASSOCGRP_HARLEYBIKEANIMS, ANIM_ID_CAR_GETIN_BIKE_FRONT, vecPedBikeKickAnimOffset);
		vehDoorPos = vehModel->GetFrontSeatPosn();

		if (component == CAR_WINDSCREEN) {
			return bike->GetMatrix() * (vehDoorPos + kickOffset);
		} else {
			switch (bike->m_bikeAnimType) {
				case ASSOCGRP_VESPABIKEANIMS:
					vehDoorOffset = GetPedAnimationOffset(ped, ASSOCGRP_VESPABIKEANIMS, ANIM_ID_CAR_GETIN_LHS_0, vecPedVespaBikeJumpRhsAnimOffset);
					break;
				case ASSOCGRP_HARLEYBIKEANIMS:
					vehDoorOffset = GetPedAnimationOffset(ped, ASSOCGRP_HARLEYBIKEANIMS, ANIM_ID_CAR_GETIN_LHS_0, vecPedHarleyBikeJumpRhsAnimOffset);
					break;
				case ASSOCGRP_DIRTBIKEANIMS:
					vehDoorOffset = GetPedAnimationOffset(ped, ASSOCGRP_DIRTBIKEANIMS, ANIM_ID_CAR_GETIN_LHS_0, vecPedDirtBikeJumpRhsAnimOffset);
					break;
				default:
					vehDoorOffset = GetPedAnimationOffset(ped, ASSOCGRP_STDBIKEANIMS, ANIM_ID_CAR_GETIN_LHS_0, vecPedStdBikeJumpRhsAnimOffset);
					break;
			}
			float xOffsetFromAnim = vehDoorOffset.x + seatPosMult * bike->m_pHandlingData->m_fSeatOffsetDistance;
			if (component == CAR_DOOR_LR || component == CAR_DOOR_RR) {
				vehDoorPos = vehModel->m_positions[CAR_POS_BACKSEAT];
			}

			if (component == CAR_DOOR_LR || component == CAR_DOOR_LF)
				xOffsetFromAnim *= -1.f;

			return bike->GetMatrix() * (vehDoorPos + CVector(xOffsetFromAnim, vehDoorOffset.y, vehDoorOffset.z));
		}
	} else {
		if (veh->bIsVan && (component == CAR_DOOR_LR || component == CAR_DOOR_RR)) {
			seatOffset = 0.0f;
			vehDoorOffset = GetPedAnimationOffset(ped, ASSOCGRP_VANCARANIMS, ANIM_ID_CAR_GETIN_LHS_1, vecPedVanRearDoorAnimOffset);
		} else {
			seatOffset = veh->m_pHandlingData->m_fSeatOffsetDistance * seatPosMult;
			if (veh->bLowVehicle) {
				vehDoorOffset = GetPedAnimationOffset(ped, ASSOCGRP_LOWCARAMIMS, ANIM_ID_CAR_GETIN_LHS_0, vecPedCarDoorLoAnimOffset);
			} else {
				vehDoorOffset = GetPedAnimationOffset(ped, ASSOCGRP_STDCARAMIMS, ANIM_ID_CAR_GETIN_LHS_0, vecPedCarDoorAnimOffset);
			}
		}

		switch (component) {
			case CAR_DOOR_RF:
				vehDoorPos = vehModel->GetFrontSeatPosn();
				vehDoorPos.x += seatOffset;
				vehDoorOffset.x = -vehDoorOffset.x;
				break;

			case CAR_DOOR_RR:
				vehDoorPos = vehModel->m_positions[CAR_POS_BACKSEAT];
				vehDoorPos.x += seatOffset;
				vehDoorOffset.x = -vehDoorOffset.x;
				break;

			case CAR_DOOR_LF:
				vehDoorPos = vehModel->GetFrontSeatPosn();
				vehDoorPos.x = -(vehDoorPos.x + seatOffset);
				break;

			case CAR_DOOR_LR:
				vehDoorPos = vehModel->m_positions[CAR_POS_BACKSEAT];
				vehDoorPos.x = -(vehDoorPos.x + seatOffset);
				break;

			default:
				vehDoorPos = vehModel->GetFrontSeatPosn();
				vehDoorOffset = CVector(0.0f, 0.0f, 0.0f);
		}
		return vehDoorPos - vehDoorOffset;
	}
}

// TODO: what is this parameter for?
void
CPed::SetDuck(uint32 time, bool sth)
{
	if (bIsDucking || CTimer::GetTimeInMilliseconds() <= m_duckTimer && !sth) {
		if (sth && CTimer::GetTimeInMilliseconds() + time > m_duckTimer)
			m_duckTimer = CTimer::GetTimeInMilliseconds() + time;
		return;
	}

	CAnimBlendAssociation *duckAssoc;
	if (bCrouchWhenShooting) {
		duckAssoc = CAnimManager::BlendAnimation(GetClump(), ASSOCGRP_DEFAULT, ANIM_ID_WEAPON_CROUCH, 4.0f);
		if (!duckAssoc) return; // stay upright if this SA bank has no crouch clip
		duckAssoc->flags &= ~ASSOC_FADEOUTWHENDONE;
		bIsDucking = true;
		if (IsPlayer() && ((CPlayerPed*)this)->GetPlayerData())
			((CPlayerPed*)this)->GetPlayerData()->m_vecFightMovement = CVector2D(0.0f, 0.0f);
		m_duckTimer = CTimer::GetTimeInMilliseconds() + time;
	} else {
		duckAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_DUCK_COWER);
		if (!duckAssoc || duckAssoc->blendDelta < 0.0f) {
			duckAssoc = CAnimManager::BlendAnimation(GetClump(), ASSOCGRP_DEFAULT, ANIM_ID_DUCK_COWER, 4.0f);
			if (!duckAssoc) return;
			bIsDucking = true;
			m_duckTimer = CTimer::GetTimeInMilliseconds() + time;
		}
	}
}

void
CPed::Duck(void)
{
	if (CTimer::GetTimeInMilliseconds() > m_duckTimer)
		ClearDuck();
	else if (bIsDucking && bCrouchWhenShooting) {
		CWeaponInfo *weapon = CWeaponInfo::GetWeaponInfo(GetWeapon()->m_eWeaponType);
		CAnimBlendAssociation *crouchAnim = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_WEAPON_CROUCH);
		if (!crouchAnim) {
			if(GetCrouchFireAnim(weapon))
				crouchAnim = RpAnimBlendClumpGetAssociation(GetClump(), GetCrouchFireAnim(weapon));
		}
		if (!crouchAnim) {
			if(GetCrouchReloadAnim(weapon))
				crouchAnim = RpAnimBlendClumpGetAssociation(GetClump(), GetCrouchReloadAnim(weapon));
		}
		if (!crouchAnim && IsPlayer() && IsPedInControl())
			crouchAnim = CAnimManager::BlendAnimation(GetClump(), ASSOCGRP_DEFAULT, ANIM_ID_WEAPON_CROUCH, 4.0f);
		if (!crouchAnim) {
			ClearDuck(false);
#if defined FIX_BUGS || defined FREE_CAM
			if (IsPlayer())
				bCrouchWhenShooting = false;
#endif
		}
	}
}

void
CPed::ClearDuck(bool clearTimer)
{
	// Both poses can be present during a transition. Fade each one rather than
	// leaving the weapon crouch active when the cower association also exists.
	const AnimationId duckAnims[] = {
		ANIM_ID_DUCK_COWER, ANIM_ID_WEAPON_CROUCH, ANIM_ID_GUNCROUCHFWD,
		ANIM_ID_GUNCROUCHBWD, ANIM_ID_CROUCH_ROLL_L, ANIM_ID_CROUCH_ROLL_R
	};
	for (int i = 0; i < ARRAY_SIZE(duckAnims); ++i) {
		CAnimBlendAssociation *animAssoc = RpAnimBlendClumpGetAssociation(GetClump(), duckAnims[i]);
		if (animAssoc) {
			if (duckAnims[i] == ANIM_ID_CROUCH_ROLL_L || duckAnims[i] == ANIM_ID_CROUCH_ROLL_R)
				animAssoc->callbackType = CAnimBlendAssociation::CB_NONE;
			animAssoc->flags |= ASSOC_DELETEFADEDOUT;
			animAssoc->blendDelta = -4.0f;
		}
	}
	if (IsPlayer() && ((CPlayerPed*)this)->GetPlayerData())
		((CPlayerPed*)this)->GetPlayerData()->m_vecFightMovement = CVector2D(0.0f, 0.0f);
	bIsDucking = false;
	if (IsPlayer() && m_nPedState == PED_AIMGUN)
		CAnimManager::BlendAnimation(GetClump(), ASSOCGRP_DEFAULT, ANIM_ID_GUN_STAND, 4.0f);

	if (clearTimer) {
		m_duckTimer = 0;
	}
}

void
CPed::SetSeekBoatPosition(CVehicle *boat)
{
	if (m_nPedState == PED_SEEK_BOAT_POSITION || boat->pDriver || !IsPedInControl())
		return;

	SetStoredState();
	m_carInObjective = boat;
	m_carInObjective->RegisterReference((CEntity **) &m_carInObjective);
	m_pMyVehicle = boat;
	m_pMyVehicle->RegisterReference((CEntity **) &m_pMyVehicle);
	m_distanceToCountSeekDone = 0.5f;
	SetPedState(PED_SEEK_BOAT_POSITION);
}

void
CPed::SeekBoatPosition(void)
{
	if (m_carInObjective && !m_carInObjective->pDriver) {
		CVehicleModelInfo *boatModel = m_carInObjective->GetModelInfo();

		CVector enterOffset;
		enterOffset = boatModel->GetFrontSeatPosn();
		enterOffset.x = 0.0f;
		CMatrix boatMat(m_carInObjective->GetMatrix());
		SetMoveState(PEDMOVE_WALK);
		m_vecSeekPos = boatMat * enterOffset;
		if (Seek()) {
			// We arrived to the boat
			m_vehDoor = 0;
			SetEnterCar(m_carInObjective, 0);
		}
	} else
		RestorePreviousState();
}

bool
CPed::IsRoomToBeCarJacked(void)
{
	if (!m_pMyVehicle)
		return false;

	CVector offset;
	if (m_pMyVehicle->IsBike()) {
		offset = vecPedStdBikeJumpRhsAnimOffset;
	} else if (m_pMyVehicle->bLowVehicle || m_nPedType == PEDTYPE_COP) {
		offset = vecPedDraggedOutCarAnimOffset;
	} else {
		offset = vecPedQuickDraggedOutCarAnimOffset;
	}

	offset.z = 0.0f;
	if (m_pMyVehicle->IsRoomForPedToLeaveCar(CAR_DOOR_LF, &offset)) {
		return true;
	}

	return false;
}

void
CPed::AddInCarAnims(CVehicle* car, bool isDriver)
{
	if (car->IsBoat()) {
		if (car->m_pHandlingData->m_nModelFlags & VEHICLE_HANDLING_MODEL_SIT_IN_BOAT) {
			m_pVehicleAnim = CAnimManager::BlendAnimation(GetClump(), GetVehicleAnimGroup(m_pMyVehicle, ANIM_ID_CAR_SIT), ANIM_ID_CAR_SIT, 100.0f);
		} else {
			m_pVehicleAnim = CAnimManager::BlendAnimation(GetClump(), ASSOCGRP_DEFAULT, ANIM_ID_DRIVE_BOAT, 100.0f);
		}
	} else if (car->IsBike()) {
		if (isDriver) {
			m_pVehicleAnim = CAnimManager::BlendAnimation(GetClump(), ((CBike*)car)->m_bikeAnimType, ANIM_ID_BIKE_RIDE, 100.0f);
		} else {
			m_pVehicleAnim = CAnimManager::BlendAnimation(GetClump(), ((CBike*)car)->m_bikeAnimType, ANIM_ID_BIKE_PASSENGER, 100.0f);
		}
	} else {
		// The seat pose comes from the car's own set: a lowrider has a lower
		// one than a saloon, and the weak/pro poses are variants inside it.
		// Each seat pose is its own clip and so resolves its group separately:
		// the two candidates in a group can differ only for some animations.
		//
		// A kart and a truck sit their driver quite differently from a saloon -
		// the kart set has the driver's knees up, the truck set has him leaning
		// on the wheel - and SA asks the handling line's anim group which of the
		// special driving sets applies, which is why this is not a model test.
		const AssocGroupId carGroup = (AssocGroupId)car->GetAnimGroupId();
		const bool isKart = CVehicleAnimGroupData::UsesKartDrivingAnims(carGroup);
		const bool isTruck = CVehicleAnimGroupData::UsesTruckDrivingAnims(carGroup);

		if (isDriver) {
			AnimationId seatAnim;
			if (car->bLowVehicle)
				seatAnim = ANIM_ID_CAR_LSIT;
			else if (isKart)
				seatAnim = ANIM_ID_KART_DRIVE;
			else if (isTruck)
				seatAnim = ANIM_ID_DRIVE_TRUCK;
			else
				seatAnim = ANIM_ID_CAR_SIT;
			m_pVehicleAnim = CAnimManager::BlendAnimation(GetClump(),
				GetVehicleAnimGroup(car, seatAnim), seatAnim, 100.0f);
		} else {
			// Only the driver has a kart or truck pose; a passenger sits the
			// way a passenger always sits.
			const AnimationId seatAnim = car->bLowVehicle ? ANIM_ID_CAR_SIT_PRO : ANIM_ID_CAR_SIT_WEAK;
			m_pVehicleAnim = CAnimManager::BlendAnimation(GetClump(),
				GetVehicleAnimGroup(car, seatAnim), seatAnim, 100.0f);
		}
	}

	StopNonPartialAnims();
}

void
CPed::RemoveDrivebyAnims()
{
	CAnimBlendAssociation *animAssoc;

	AnimationId LeftAnim = ANIM_ID_DRIVEBY_L;
	AnimationId RightAnim = ANIM_ID_DRIVEBY_R;

	if (m_pMyVehicle->m_pHandlingData->m_nModelFlags & VEHICLE_HANDLING_MODEL_IS_BIKE) {
		LeftAnim = ANIM_ID_BIKE_DRIVEBYRHS;
		RightAnim = ANIM_ID_BIKE_DRIVEBYLHS;
	} else if (m_pMyVehicle->bLowVehicle) {
		LeftAnim = ANIM_ID_DRIVEBYL_L;
		RightAnim = ANIM_ID_DRIVEBYL_R;
	}

	animAssoc = RpAnimBlendClumpGetAssociation((RpClump*)m_rwObject, ANIM_ID_BIKE_DRIVEBYRHS);
	if (animAssoc)
		animAssoc->blendDelta = -1000.0f;
	animAssoc = RpAnimBlendClumpGetAssociation((RpClump*)m_rwObject, ANIM_ID_BIKE_DRIVEBYLHS);
	if (animAssoc)
		animAssoc->blendDelta = -1000.0f;
	animAssoc = RpAnimBlendClumpGetAssociation((RpClump*)m_rwObject, ANIM_ID_BIKE_DRIVEBYFT);
	if (animAssoc)
		animAssoc->blendDelta = -1000.0f;
}

void
CPed::RemoveInCarAnims(void)
{
	CAnimBlendAssociation* assoc;

	for (assoc = RpAnimBlendClumpGetFirstAssociation(GetClump(), ASSOC_DRIVING); assoc;
		assoc = RpAnimBlendGetNextAssociation(assoc, ASSOC_DRIVING)) {
		assoc->flags |= ASSOC_DELETEFADEDOUT;
		assoc->blendDelta = -1000.0f;
	}
}

bool
CPed::PositionPedOutOfCollision(void)
{
	CVehicle *veh = m_pMyVehicle;
	if (!veh)
		return false;

	if (bDonePositionOutOfCollision)
		return true;

	bool foundAPos = false;
	CColModel *vehCol = veh->GetColModel();
	CVector vehPos = veh->GetPosition();
	CVector ourPos = GetPosition();
	CVector newPos = ourPos;
	CWorld::pIgnoreEntity = veh;
	bUsesCollision = false;
	bJustCheckCollision = true;
	m_vecMoveSpeed = CVector(0.f, 0.f, 0.f);
	if (veh->IsOnItsSide()) {
		// Top of the veh.
		newPos = vehPos;
		newPos.z = FEET_OFFSET + vehCol->boundingBox.max.x + vehPos.z;
		GetMatrix().SetTranslate(newPos);
		if (!CheckCollision()) {
			if (CWorld::GetIsLineOfSightClear(vehPos, newPos, true, false, false, true, false, false, false))
				foundAPos = true;
		}

	} else if (m_vehDoor != 0) {
		// Try the normal way
		CVector pos = GetPositionToOpenCarDoor(m_pMyVehicle, m_vehDoor, this);
		newPos = pos;
		GetMatrix().SetTranslate(newPos);
		if (!CheckCollision()) {
			if (CWorld::GetIsLineOfSightClear(vehPos, newPos, true, false, false, true, false, false, false))
				foundAPos = true;
		}
	}

	float vehRelativeExitX = vehCol->boundingBox.min.x - 0.355f;
	if (m_vehDoor == CAR_DOOR_RF || m_vehDoor == CAR_DOOR_RR)
		vehRelativeExitX = 0.355f + vehCol->boundingBox.max.x;

	if (!foundAPos) {
		// Check sides of veh., respective to seat column-veh. center difference(why?)
		float exitOffset = vehRelativeExitX - DotProduct(ourPos - vehPos, veh->GetRight());
		newPos = exitOffset * veh->GetRight() + ourPos;
		GetMatrix().SetTranslate(newPos);
		if (!CheckCollision()) {
			if (CWorld::GetIsLineOfSightClear(vehPos, newPos, true, false, false, true, false, false, false))
				foundAPos = true;
		}
	}
	if (!foundAPos) {
		// Iterate through sections of veh. length + static offset on X
		float minY = vehCol->boundingBox.min.y;
		float ySection = (vehCol->boundingBox.max.y - minY) / 3.f;
		for (int i = 0; i < 4; i++) {
			float fwdMult = i * ySection + minY;
			newPos = vehRelativeExitX * veh->GetRight() + fwdMult * veh->GetForward() + vehPos;
			GetMatrix().SetTranslate(newPos);
			if (!CheckCollision()) {
				if (CWorld::GetIsLineOfSightClear(vehPos, newPos, true, false, false, true, false, false, false)) {
					foundAPos = true;
					break;
				}
			}
		}
	}
	if (!foundAPos) {
		// Back of veh.
		newPos = (vehCol->boundingBox.min.y - 0.355f) * veh->GetForward() + vehPos;
		GetMatrix().SetTranslate(newPos);
		if (!CheckCollision()) {
			if (CWorld::GetIsLineOfSightClear(vehPos, newPos, true, false, false, true, false, false, false))
				foundAPos = true;
		}
	}
	if (!foundAPos) {
		// Front of veh.
		newPos = (0.355f + vehCol->boundingBox.max.y) * veh->GetForward() + vehPos;
		GetMatrix().SetTranslate(newPos);
		if (!CheckCollision()) {
			if (CWorld::GetIsLineOfSightClear(vehPos, newPos, true, false, false, true, false, false, false))
				foundAPos = true;
		}
	}
	if (!foundAPos) {
		// Opposite X side + back
		newPos = vehCol->boundingBox.min.y * veh->GetForward() + vehPos - vehRelativeExitX * veh->GetRight();
		GetMatrix().SetTranslate(newPos);
		if (!CheckCollision()) {
			if (CWorld::GetIsLineOfSightClear(vehPos, newPos, true, false, false, true, false, false, false))
				foundAPos = true;
		}
	}
	if (!foundAPos) {
		// Opposite X side + front
		newPos = vehCol->boundingBox.max.y * veh->GetForward() + vehPos - vehRelativeExitX * veh->GetRight();
		GetMatrix().SetTranslate(newPos);
		if (!CheckCollision()) {
			if (CWorld::GetIsLineOfSightClear(vehPos, newPos, true, false, false, true, false, false, false))
				foundAPos = true;
		}
	}
	if (!foundAPos) {
		// Top of veh.
		if (veh->IsAutomobile()) {
			newPos = vehCol->boundingBox.max.z * veh->GetUp() + vehPos;
			newPos.z += FEET_OFFSET;
			GetMatrix().SetTranslate(newPos);
			if (!CheckCollision()) {
				if (CWorld::GetIsLineOfSightClear(vehPos, newPos, true, false, false, true, false, false, false))
					foundAPos = true;
			}
		}
	}
	m_vecMoveSpeed = CVector(0.f, 0.f, 0.f);
	m_vecTurnSpeed = CVector(0.f, 0.f, 0.f);
	veh->m_vecMoveSpeed = CVector(0.f, 0.f, 0.f);
	veh->m_vecTurnSpeed = CVector(0.f, 0.f, 0.f);
	CWorld::pIgnoreEntity = nil;
	bUsesCollision = true;
	bJustCheckCollision = false;
	bDonePositionOutOfCollision = true;
	return foundAPos;
}

// "Any" means he shouldn't have to be in vehicle.
bool
CPed::PositionAnyPedOutOfCollision(void)
{
	CVehicle *veh;
	CVector posNearVeh;
	CVector posSomewhereClose;
	bool putNearVeh = false;
	bool putSomewhereClose = false;
	int smallestDistNearVeh = 999;
	int smallestDistSomewhereClose = 999;

	CVector potentialPos;
	potentialPos.y = GetPosition().y - 3.5f;
	potentialPos.z = GetPosition().z;

	for (int yTry = 0; yTry < 15; yTry++) {
		potentialPos.x = GetPosition().x - 3.5f;

		for (int xTry = 0; xTry < 15; xTry++) {
			FindZCoorForPed(&potentialPos);
			if (!CWorld::TestSphereAgainstWorld(potentialPos, 0.6f, this, true, false, false, true, false, false)) {
				float potentialChangeSqr = (potentialPos - GetPosition()).MagnitudeSqr();
				veh = (CVehicle*)CWorld::TestSphereAgainstWorld(potentialPos, 0.6f, this, false, true, false, false, false, false);
				if (veh) {
					if (potentialChangeSqr < smallestDistNearVeh) {
						posNearVeh = potentialPos;
						putNearVeh = true;
						smallestDistNearVeh = potentialChangeSqr;
					}
				} else if (potentialChangeSqr < smallestDistSomewhereClose) {
					smallestDistSomewhereClose = potentialChangeSqr;
					posSomewhereClose = potentialPos;
					putSomewhereClose = true;
				}
			}
			potentialPos.x += 0.5f;
		}
		potentialPos.y += 0.5f;
	}

	if (!putSomewhereClose && !putNearVeh)
		return false;

	// We refrain from using posNearVeh, probably because of it may be top of the vehicle.
	if (putSomewhereClose) {
		SetPosition(posSomewhereClose);
	} else {
		CVector vehSize = veh->GetModelInfo()->GetColModel()->boundingBox.max;
		posNearVeh.z += vehSize.z;
		SetPosition(posNearVeh);
	}
	return true;
}
