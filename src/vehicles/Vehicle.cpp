#include "common.h"
#include "main.h"

#include "General.h"
#include "Timer.h"
#include "Pad.h"
#include "Vehicle.h"
#include "VehicleAnimGroupData.h"
#include "CustomCarPlateMgr.h"
#include "ControllerConfig.h"
#include "Bike.h"
#include "Automobile.h"
#include "Pools.h"
#include "cHandlingDataMgr.h"
#include "CarCtrl.h"
#include "Population.h"
#include "ModelIndices.h"
#include "World.h"
#include "Lights.h"
#include "PointLights.h"
#include "Renderer.h"
#include "VisibilityPlugins.h"
#include "DMAudio.h"
#include "Radar.h"
#include "Fire.h"
#include "Darkel.h"
#include "Streaming.h"
#include "Camera.h"
#include "Stats.h"
#include "Garages.h"
#include "Wanted.h"
#include "SurfaceTable.h"
#include "Particle.h"
#include "WaterLevel.h"
#include "Timecycle.h"
#include "Weather.h"
#include "Coronas.h"
#include "SaveBuf.h"
#include "TxdStore.h"
#include "RwHelper.h"

bool CVehicle::bWheelsOnlyCheat;
bool CVehicle::bCheat3;
bool CVehicle::bCheat4;
bool CVehicle::bCheat5;
bool CVehicle::bCheat8;
bool CVehicle::bCheat9;
bool CVehicle::bCheat10;
bool CVehicle::bHoverCheat;
bool CVehicle::bAllTaxisHaveNitro;
bool CVehicle::m_bDisableMouseSteering = true;
int32 CVehicle::m_nLastControlInput = KEYBOARD;
bool CVehicle::bDisableRemoteDetonation;
bool CVehicle::bDisableRemoteDetonationOnContact;
#ifndef MASTER
bool CVehicle::m_bDisplayHandlingInfo;
#endif

void *CVehicle::operator new(size_t sz) throw() { return CPools::GetVehiclePool()->New();  }
void *CVehicle::operator new(size_t sz, int handle) throw() { return CPools::GetVehiclePool()->New(handle); }
void CVehicle::operator delete(void *p, size_t sz) throw() { CPools::GetVehiclePool()->Delete((CVehicle*)p); }
void CVehicle::operator delete(void *p, int handle) throw() { CPools::GetVehiclePool()->Delete((CVehicle*)p); }

#ifdef FIX_BUGS
// I think they meant that
#define DAMAGE_FLEE_IN_CAR_PROBABILITY_VALUE (MYRAND_MAX * 35 / 100)
#define DAMAGE_FLEE_ON_FOOT_PROBABILITY_VALUE (MYRAND_MAX * 70 / 100)
#else
#define DAMAGE_FLEE_IN_CAR_PROBABILITY_VALUE (35000)
#define DAMAGE_FLEE_ON_FOOT_PROBABILITY_VALUE (70000)
#endif
#define DAMAGE_HEALTH_TO_FLEE_ALWAYS (200.0f)
#define DAMAGE_HEALTH_TO_CATCH_FIRE (250.0f)


CVehicle::CVehicle(uint8 CreatedBy)
{
	int i;

	m_nCurrentGear = 1;
	m_fChangeGearTime = 0.0f;
	m_fTransmissionInertiaRatio = 0.0f;
	m_fTransmissionInertiaSmoothing = 0.0f;
	m_fSteerInput = 0.0f;
	m_type = ENTITY_TYPE_VEHICLE;
	// The derived constructor overwrites this with its own class; IGNORE marks
	// an instance whose class has not been chosen yet.
	m_nVehicleType = VEHICLE_TYPE_IGNORE;
	m_nVehicleSubType = VEHICLE_TYPE_IGNORE;
	VehicleCreatedBy = CreatedBy;
	m_nRouteSeed = 0;
	bIsLocked = false;
	bIsLawEnforcer = false;
	bIsAmbulanceOnDuty = false;
	bIsFireTruckOnDuty = false;
#ifdef FIX_BUGS
	bIsHandbrakeOn = false;
#endif
	CCarCtrl::UpdateCarCount(this, false);
	m_fHealth = 1000.0f;
	bEngineOn = true;
	bFreebies = true;
	pDriver = nil;
	m_nNumPassengers = 0;
	m_nNumGettingIn = 0;
	m_nGettingInFlags = 0;
	m_nGettingOutFlags = 0;
	m_nNumMaxPassengers = ARRAY_SIZE(pPassengers);
	for(i = 0; i < m_nNumMaxPassengers; i++)
		pPassengers[i] = nil;
	m_nBombTimer = 0;
	m_pBlowUpEntity = nil;
	m_nPacManPickupsCarried = 0;
	bComedyControls = false;
	bCraneMessageDone = false;
	bExtendedRange = false;
	bTakeLessDamage = false;
	bIsDamaged = false;
	bFadeOut = false;
	bIsBeingCarJacked = false;
	m_nTimeOfDeath = 0;
	m_pCarFire = nil;
	bHasBeenOwnedByPlayer = false;
	bCreateRoadBlockPeds = false;
	bCanBeDamaged = true;
	bUsingSpecialColModel = false;
	bOccupantsHaveBeenGenerated = false;
	bGunSwitchedOff = false;
	m_nGunFiringTime = 0;
	m_nTimeBlocked = 0;
	bLightsOn = false;
	bVehicleColProcessed = false;
	m_numPedsUseItAsCover = 0;
	bIsCarParkVehicle = false;
	bHasAlreadyBeenRecorded = false;
	m_bSirenOrAlarm = false;
	m_nCarHornTimer = 0;
	m_nCarHornPattern = 0;
	m_nCarHornDelay = 0;
	bPartOfConvoy = false;
	bHeliMinimumTilt = false;
	bAudioChangingGear = false;
	bIsDrowning = false;
	bTyresDontBurst = false;
	bCreatedAsPoliceVehicle = false;
	bRestingOnPhysical = false;
	bParking = false;
	bCanPark = CGeneral::GetRandomNumberInRange(0.0f, 1.0f) < 0.0f;	// never true. probably doesn't work very well
	bIsVan = false;
	bIsBus = false;
	bIsBig = false;
	bLowVehicle = false;

	m_bombType = CARBOMB_NONE;
	bDriverLastFrame = false;
	m_pBombRigger = nil;

	m_nSetPieceExtendedRangeTime = 0;
	m_nAlarmState = 0;
	m_nDoorLock = CARLOCK_UNLOCKED;
	m_nLastWeaponDamage = -1;
	m_pLastDamageEntity = nil;
	m_pTowingVehicle = nil;
	m_pVehicleBeingTowed = nil;
	// SetModelIndex creates the plate once the model is known; until then the
	// vehicle owns nothing.
	m_pCustomCarPlate = nil;
	// A new car has no parts fitted and wears no paintjob. -1 in every slot is
	// SA's "empty"; the functional flags stay clear until a part is fitted.
	for(int32 s = 0; s < NUM_VEHICLE_UPGRADES; s++)
		m_anUpgrades[s] = -1;
	bHydraulicInst = false;
	bNosInst = false;
	bUpgradedStereo = false;
	m_nNitroBoosts = 0;
	m_nRemapTxd = -1;
	m_nPreviousRemapTxd = -1;
	m_pRemapTexture = nil;
	// Until carcols gives the car four channels, the third and fourth hold the
	// same pair as the first two, which is what SA does for a two-colour line.
	m_currentColour3 = 0;
	m_currentColour4 = 0;
	for(int32 w = 0; w < NUM_WHEEL_SLOTS; w++)
		m_apHiddenWheelAtomics[w] = nil;
	m_fMapObjectHeightAhead = m_fMapObjectHeightBehind = 0.0f;
	m_audioEntityId = DMAudio.CreateEntity(AUDIOTYPE_PHYSICAL, this);
	if(m_audioEntityId >= 0)
		DMAudio.SetEntityStatus(m_audioEntityId, TRUE);
	//m_nRadioStation = CGeneral::GetRandomNumber() % NUM_RADIOS;
	switch(GetModelIndex()){
	case MI_HUNTER:
	case MI_ANGEL:
	case MI_FREEWAY:
		m_nRadioStation = V_ROCK;
		break;
	case MI_RCBARON:
	case MI_RCBANDIT:
	case MI_RCRAIDER:
	case MI_RCGOBLIN:
	case MI_TOPFUN:
	case MI_CADDY:
	case MI_BAGGAGE:
		m_nRadioStation = RADIO_OFF;
		break;
	default:
		m_nRadioStation = CGeneral::GetRandomNumber() % NUM_RADIOS;
		break;
	}
	m_pCurGroundEntity = nil;
	m_bRainAudioCounter = 0;
	m_bRainSamplesCounter = 0;
	m_comedyControlState = 0;
	m_aCollPolys[0].valid = false;
	m_aCollPolys[1].valid = false;
	AutoPilot.m_nCarMission = MISSION_NONE;
	AutoPilot.m_nTempAction = TEMPACT_NONE;
	AutoPilot.m_nTimeToStartMission = CTimer::GetTimeInMilliseconds();
	AutoPilot.m_bStayInCurrentLevel = false;
	AutoPilot.m_bIgnorePathfinding = false;
	AutoPilot.m_nSwitchDistance = 20;
}

static RpAtomic *ReleaseUpgradeRefsCB(RpAtomic *atomic, void *data);

CVehicle::~CVehicle()
{
	// The plate texture belongs to this vehicle, not to the model, so it goes
	// with the vehicle - otherwise a car with a scripted plate that is deleted
	// leaks its texture for the rest of the session.
	CustomCarPlate_TextureDestroy();

	// A fitted paintjob holds a reference on its texture dictionary. Dropping it
	// here is what lets the streaming code free that dictionary once the last
	// car wearing it is gone; leaving it would keep every paintjob ever applied
	// in memory for the rest of the session.
	if(m_nPreviousRemapTxd != -1){
		CTxdStore::RemoveRef(m_nPreviousRemapTxd);
		m_nPreviousRemapTxd = -1;
		m_nRemapTxd = -1;
		m_pRemapTexture = nil;
	}

	// The fitted parts are atomics of this vehicle's clump, which the entity
	// destructor destroys. What has to be undone here is the reference the
	// fitting code took on each part's model - one per atomic, and a wheel
	// upgrade is four atomics - otherwise the models of every part ever fitted
	// stay pinned in memory after the car is gone.
	if(GetClump()){
		// Wheel meshes that were hidden by a rim upgrade are put back first, so
		// the clump destructor disposes of them together with the rest of the
		// car instead of leaving them orphaned.
		for(int32 w = 0; w < NUM_WHEEL_SLOTS; w++){
			if(m_apHiddenWheelAtomics[w] == nil)
				continue;
			RpClumpAddAtomic(GetClump(), m_apHiddenWheelAtomics[w]);
			m_apHiddenWheelAtomics[w] = nil;
		}
		RpClumpForAllAtomics(GetClump(), ReleaseUpgradeRefsCB, nil);
	}
	for(int32 i = 0; i < NUM_VEHICLE_UPGRADES; i++)
		m_anUpgrades[i] = -1;

	m_nAlarmState = 0;
	if (m_audioEntityId >= 0){
		DMAudio.DestroyEntity(m_audioEntityId);
		m_audioEntityId = -5;
	}
	CRadar::ClearBlipForEntity(BLIP_CAR, CPools::GetVehiclePool()->GetIndex(this));
	if (pDriver)
		pDriver->FlagToDestroyWhenNextProcessed();
	for (int i = 0; i < m_nNumMaxPassengers; i++){
		if (pPassengers[i])
			pPassengers[i]->FlagToDestroyWhenNextProcessed();
	}
	if (m_pCarFire)
		m_pCarFire->Extinguish();
	CCarCtrl::UpdateCarCount(this, true);
	if (bIsAmbulanceOnDuty){
		CCarCtrl::NumAmbulancesOnDuty--;
		bIsAmbulanceOnDuty = false;
	}
	if (bIsFireTruckOnDuty){
		CCarCtrl::NumFiretrucksOnDuty--;
		bIsFireTruckOnDuty = false;
	}
}

void
CVehicle::SetModelIndex(uint32 id)
{
	CEntity::SetModelIndex(id);
	m_aExtras[0] = CVehicleModelInfo::ms_compsUsed[0];
	m_aExtras[1] = CVehicleModelInfo::ms_compsUsed[1];
	m_nNumMaxPassengers = CVehicleModelInfo::GetMaximumNumberOfPassengersFromNumberOfDoors(id);

	// The plate is per vehicle, the texture it is generated into belongs to the
	// model, so a vehicle that is given a new model has to drop its own plate
	// first or it will keep showing the previous model's text.
	CustomCarPlate_TextureDestroy();
	CustomCarPlate_TextureCreate(GetModelInfo());
}

// SA CVehicle::CustomCarPlate_TextureCreate (0x6D10E0).
//
// Two cases. If the model is carrying a scripted plate text, this vehicle gets
// a texture of its own - the text is baked in and the model's pending text and
// plate type are consumed, so the next car built on the model gets a normal
// plate. Otherwise the vehicle just references the model's shared plate
// texture, which is what keeps a street full of cars down to one texture.
bool
CVehicle::CustomCarPlate_TextureCreate(CVehicleModelInfo *model)
{
	m_pCustomCarPlate = nil;

	if(model == nil || model->m_pPlateMaterial == nil)
		return false;

	char *text = model->GetCustomCarPlateText();
	if(text){
		m_pCustomCarPlate = CCustomCarPlateMgr::CreatePlateTexture(text, model->m_nPlateType);
		// The text is one-shot: SA clears it and marks the plate type as
		// already used, so a second car does not inherit the script's plate.
		model->SetCustomCarPlateText(nil);
		model->m_nPlateType = (uint8)-1;
	}else{
		m_pCustomCarPlate = RpMaterialGetTexture(model->m_pPlateMaterial);
		if(m_pCustomCarPlate)
			RwTextureAddRef(m_pCustomCarPlate);
	}

	return true;
}

// SA CVehicle::CustomCarPlate_TextureDestroy (0x6D1150).
void
CVehicle::CustomCarPlate_TextureDestroy(void)
{
	if(m_pCustomCarPlate){
		RwTextureDestroy(m_pCustomCarPlate);
		m_pCustomCarPlate = nil;
	}
}

bool
CVehicle::SetupLighting(void)
{
	ActivateDirectional();
	SetAmbientColoursForPedsCarsAndObjects();

	if(bRenderScorched){
		WorldReplaceNormalLightsWithScorched(Scene.world, 0.1f);
	}else{
		CVector coors = GetPosition();
		float lighting = CPointLights::GenerateLightsAffectingObject(&coors);
		if(lighting != 1.0f){
			SetAmbientAndDirectionalColours(lighting);
			return true;
		}
	}

	return false;
}

void
CVehicle::RemoveLighting(bool reset)
{
	CRenderer::RemoveVehiclePedLights(this, reset);
}

bool
CVehicle::IsClearToDriveAway(void)
{
	CColPoint point;
	float length = GetColModel()->boundingBox.GetSize().y;
	CEntity *ent = nil;
	CVector front = GetForward() * (length*0.5f + 3.0f);
	return !CWorld::ProcessLineOfSight(GetPosition() + front, GetPosition(),
			point, ent, true, true, false, false, false, true, true) ||
		ent == this;
}

float
CVehicle::GetHeightAboveRoad(void)
{
	return -1.0f * GetColModel()->boundingBox.min.z;
}

static CColModel rotorColModel;
static CColSphere rotorColSphere;
float ROTOR_SEMI_THICKNESS = 0.05f;
float ROTOR_TURN_SPEED = 0.2f;
float ROTOR_DISGUARD_MULT = 0.3f;
float ROTOR_COL_ELASTICITY = 1.0f;
float ROTOR_COL_TURNMULT = -0.001f;
float ROTOR_DEFAULT_DAMAGE = 100.0f;

bool
CVehicle::DoBladeCollision(CVector pos, CMatrix &matrix, int16 rotorType, float radius, float damageMult)
{
	// "min"/"max" would collide with the min/max macros windows.h leaves
	// behind on some platform configurations.
	CVector boxMax(radius, radius, radius);
	CVector boxMin(-radius, -radius, -radius);

	switch(rotorType){
	case ROTOR_TOP:
	case ROTOR_BOTTOM:
		boxMin.z = -ROTOR_SEMI_THICKNESS;
		boxMax.z = ROTOR_SEMI_THICKNESS;
		break;
	case ROTOR_FRONT:
	case ROTOR_BACK:
		boxMin.y = -ROTOR_SEMI_THICKNESS;
		boxMax.y = ROTOR_SEMI_THICKNESS;
		break;
	case ROTOR_RIGHT:
	case ROTOR_LEFT:
		boxMin.x = -ROTOR_SEMI_THICKNESS;
		boxMax.x = ROTOR_SEMI_THICKNESS;
		break;
	}

	boxMin += pos;
	boxMax += pos;
	rotorColModel.boundingBox.Set(boxMin, boxMax);
	rotorColModel.boundingSphere.Set(radius, pos);
	rotorColSphere.Set(radius, pos, 0, 0);
	rotorColModel.spheres = &rotorColSphere;
	rotorColModel.numSpheres = 1;

	pos = matrix * pos;
	bool hadCollision = false;
	int minX = CWorld::GetSectorIndexX(pos.x - radius);
	int minY = CWorld::GetSectorIndexY(pos.y - radius);
	int maxX = CWorld::GetSectorIndexX(pos.x + radius);

	int maxY = CWorld::GetSectorIndexY(pos.y + radius);

	CWorld::AdvanceCurrentScanCode();
	for(int curY = minY; curY <= maxY; curY++) {
		for(int curX = minX; curX <= maxX; curX++) {
			CSector *sector = CWorld::GetSector(curX, curY);
			if(BladeColSectorList(sector->m_lists[ENTITYLIST_BUILDINGS], rotorColModel, matrix, rotorType, damageMult))
				hadCollision = true;
			if(BladeColSectorList(sector->m_lists[ENTITYLIST_BUILDINGS_OVERLAP], rotorColModel, matrix, rotorType, damageMult))
				hadCollision = true;
			if(BladeColSectorList(sector->m_lists[ENTITYLIST_VEHICLES], rotorColModel, matrix, rotorType, damageMult))
				hadCollision = true;
			if(BladeColSectorList(sector->m_lists[ENTITYLIST_VEHICLES_OVERLAP], rotorColModel, matrix, rotorType, damageMult))
				hadCollision = true;
			if(BladeColSectorList(sector->m_lists[ENTITYLIST_PEDS], rotorColModel, matrix, rotorType, 0.0f))
				hadCollision = true;
			if(BladeColSectorList(sector->m_lists[ENTITYLIST_PEDS_OVERLAP], rotorColModel, matrix, rotorType, 0.0f))
				hadCollision = true;
			if(BladeColSectorList(sector->m_lists[ENTITYLIST_OBJECTS], rotorColModel, matrix, rotorType, damageMult))
				hadCollision = true;
			if(BladeColSectorList(sector->m_lists[ENTITYLIST_OBJECTS_OVERLAP], rotorColModel, matrix, rotorType, damageMult))
				hadCollision = true;
		}
	}
	rotorColModel.spheres = nil;
	rotorColModel.numSpheres = 0;

	return hadCollision;
}

bool
CVehicle::BladeColSectorList(CPtrList &list, CColModel &rotorColModel, CMatrix &matrix, int16 rotorType, float damageMult)
{
	int i;
	CVector axis;
	CVector turnSpeed(0.0f, 0.0f, 0.0f);
	switch(rotorType){
	case ROTOR_TOP:
		turnSpeed.z = -ROTOR_TURN_SPEED;
		axis = -matrix.GetUp();
		break;
	case ROTOR_BOTTOM:
		turnSpeed.z = ROTOR_TURN_SPEED;
		axis = matrix.GetUp();
		break;

	case ROTOR_FRONT:
		turnSpeed.y = -ROTOR_TURN_SPEED;
		axis = -matrix.GetForward();
		break;
	case ROTOR_BACK:
		turnSpeed.y = ROTOR_TURN_SPEED;
		axis = matrix.GetForward();
		break;

	case ROTOR_RIGHT:
		turnSpeed.x = -ROTOR_TURN_SPEED;
		axis = -matrix.GetRight();
		break;
	case ROTOR_LEFT:
		turnSpeed.x = ROTOR_TURN_SPEED;
		axis = matrix.GetRight();
		break;
	}
	turnSpeed = Multiply3x3(matrix, turnSpeed);
	CVector center = rotorColModel.boundingSphere.center;
	center = matrix*center;

	for(CPtrNode *node = list.first; node; node = node->next) {
		CEntity *entity = (CEntity *)node->item;
		if(entity == (CEntity*)this ||
		   !entity->bUsesCollision ||
		   entity->m_scanCode == CWorld::GetCurrentScanCode())
			continue;

		entity->m_scanCode = CWorld::GetCurrentScanCode();

		int numCollisions;
		CColModel *entityCol;
		if(entity->IsPed())
			entityCol = ((CPedModelInfo*)CModelInfo::GetModelInfo(entity->GetModelIndex()))->AnimatePedColModelSkinned(entity->GetClump());
		else
			entityCol = entity->GetColModel();
		if(entityCol)
			numCollisions = CCollision::ProcessColModels(matrix, rotorColModel, entity->GetMatrix(), *entityCol,
				CWorld::m_aTempColPts, nil, nil);
		else
			numCollisions = 0;

		if(numCollisions > 0 && entity->IsPed()){
			CPed *ped = (CPed*)entity;
			CVector2D dirToRotor = GetPosition() - entity->GetPosition();
			dirToRotor.Normalise();
			int localDir = ped->GetLocalDirection(dirToRotor);
			if(ped->m_attachedTo == nil){
				ped->bIsStanding = false;
				ped->ApplyMoveForce(-5.0f*dirToRotor.x, -5.0f*dirToRotor.y, 5.0f);
			}
			ped->InflictDamage(this, WEAPONTYPE_RUNOVERBYCAR, 1000.0f, PEDPIECE_TORSO, localDir);

			if(CGame::nastyGame && ped->GetIsOnScreen()){
				for(i = 0; i < 16; i++)
					CParticle::AddParticle(PARTICLE_BLOOD_SMALL, ped->GetPosition(), CVector(dirToRotor.x, dirToRotor.y, 1.0f) * 0.01f);
				CParticle::AddParticle(PARTICLE_TEST, ped->GetPosition(), CVector(0.0f, 0.0f, 0.02f), nil, 0.1f);
				CParticle::AddParticle(PARTICLE_TEST, ped->GetPosition()+CVector(0.0f, 0.0f, 0.2f), CVector(0.0f, 0.0f, -0.01f), nil, 0.1f);
			}
		}else if(numCollisions > 0 && entity->GetModelIndex() != MI_MISSILE){
			float impulse = 0.0f;
			bool hadCollision = false;
			float savedElasticity = m_fElasticity;
			m_fElasticity = ROTOR_COL_ELASTICITY;

			for(i = 0; i < numCollisions; i++){
				CVector colpos = CWorld::m_aTempColPts[i].point;
				CVector localColpos = colpos - center;
				float axisDir = DotProduct(axis, localColpos);
				float colDir = DotProduct(CWorld::m_aTempColPts[i].normal, localColpos);

				if(2.0f*ROTOR_SEMI_THICKNESS < Abs(axisDir) &&
				   ROTOR_DISGUARD_MULT*Abs(colDir) < Abs(axisDir))
					continue;

				hadCollision = true;
				colpos -= axisDir*axis;	// get rid of axis component

				CVector tangentSpeed = CrossProduct(turnSpeed, colpos - center);

				// Particles
				for(int j = 0; j < 4; j++){
					CParticle::AddParticle(PARTICLE_SPARK_SMALL, colpos, (tangentSpeed+m_vecMoveSpeed)/2.0f);
					CParticle::AddParticle(PARTICLE_SPARK, colpos, 0.1f*CWorld::m_aTempColPts[i].normal);
				}

				// Apply Collision
				if(IsAutomobile()){
					CAutomobile *heli = (CAutomobile*)this;
					if(heli->m_aWheelSpeed[1] > 0.15f){
						ApplyCollision(CWorld::m_aTempColPts[i], impulse);
						ApplyTurnForce(m_fTurnMass*ROTOR_COL_TURNMULT*tangentSpeed, colpos - center);
						heli->m_aWheelSpeed[1] = 0.15f;
					}else if(heli->m_aWheelSpeed[1] < 0.075f && heli->m_aWheelSpeed[1] > 0.0f)
						heli->m_aWheelSpeed[1] *= -1.0f;
				}

				float damageImpulse = damageMult * Max(impulse, ROTOR_DEFAULT_DAMAGE*m_fMass/3000.0f);
				if(damageImpulse > m_fDamageImpulse)
					SetDamagedPieceRecord(0, damageImpulse, entity, CWorld::m_aTempColPts[i].normal);

			}

			if(hadCollision && !entity->IsPed())
				DMAudio.ReportCollision(this, entity, SURFACE_CAR_PANEL, SURFACE_TARMAC, 50.0f, 0.09f);
			m_fElasticity = savedElasticity;
		}
	}
	return false;
}


float fBurstSpeedMax = 0.3f;
float fBurstTyreMod = 0.13f;

// Wheel traction and braking, shared by every wheeled vehicle. The state is a
// per-wheel out-parameter because the caller keeps it between frames: a wheel
// that was already skidding loses grip for as long as it keeps sliding.
void
CVehicle::ProcessWheel(CVector &wheelFwd, CVector &wheelRight, CVector &wheelContactSpeed, CVector &wheelContactPoint,
	int32 wheelsOnGround, float thrust, float brake, float adhesion, int8 wheelId, float &wheelSpeedUnused,
	tWheelState &wheelState, uint16 wheelStatus)
{
	bool braking = brake != 0.0f;
	bool driving = !braking && thrust != 0.0f;
	bool alreadySkidding = wheelState != WHEEL_STATE_NORMAL;

	float contactSpeedFwd = DotProduct(wheelContactSpeed, wheelFwd);
	float contactSpeedRight = DotProduct(wheelContactSpeed, wheelRight);

	adhesion *= CTimer::GetTimeStep();
	if(alreadySkidding){
		adhesion *= m_pHandlingData->m_fTractionLoss;
		if(wheelState == WHEEL_STATE_SPINNING &&
		   (GetStatus() == STATUS_PLAYER || GetStatus() == STATUS_PLAYER_REMOTE))
			adhesion *= 1.0f - Abs(m_fGasPedal)*WS_ALREADY_SPINNING_LOSS;
	}

	wheelState = WHEEL_STATE_NORMAL;

	// how much force we want to apply in these axes
	float fwd = 0.0f;
	float right = 0.0f;

	// moving sideways: oppose the contact point's sideways speed
	if(contactSpeedRight != 0.0f){
		right = -contactSpeedRight/wheelsOnGround;
		if(wheelStatus == WHEEL_STATUS_BURST)
			right += Min(contactSpeedFwd, fBurstSpeedMax)*CGeneral::GetRandomNumberInRange(-fBurstTyreMod, fBurstTyreMod);
	}

	if(driving){
		fwd = thrust;
		right = Clamp(right, -adhesion, adhesion);
	}else if(contactSpeedFwd != 0.0f){
		fwd = -contactSpeedFwd/wheelsOnGround;

		// no pedal pressed: engine braking, weaker for light vehicles
		if(!braking && Abs(m_fGasPedal) < 0.01f){
			if(IsBike())
				brake = mod_HandlingManager.fWheelFriction*0.6f/(m_pHandlingData->m_fMass + 200.0f);
			else if(IsSubPlane())
				brake = 0.0f;
			else{
				brake = mod_HandlingManager.fWheelFriction/m_pHandlingData->m_fMass;
				if(m_pHandlingData->m_fMass < 500.0f)
					brake *= 0.1f;
				else if(GetModelIndex() == MI_RCBANDIT)
					brake *= 0.2f;
			}
		}

		if(brake > adhesion){
			if(Abs(contactSpeedFwd) > 0.005f)
				wheelState = WHEEL_STATE_FIXED;
		}else
			fwd = Clamp(fwd, -brake, brake);
	}

	float slipSq = sq(right) + sq(fwd);
	if(slipSq > sq(adhesion)){
		if(wheelState != WHEEL_STATE_FIXED){
			// the outer front wheel is allowed twice the longitudinal slip
			// before it is considered spinning, which is what makes a
			// rear-wheel-drive car step out under power
			float tractionLimit = contactSpeedFwd > 0.15f && (!wheelId || wheelId == CARWHEEL_FRONT_RIGHT)
				? 2.0f*WS_TRAC_FRAC_LIMIT
				: WS_TRAC_FRAC_LIMIT;
			wheelState = driving && tractionLimit*adhesion < Abs(fwd)
				? WHEEL_STATE_SPINNING
				: WHEEL_STATE_SKIDDING;
		}

		float tractionLoss = 1.0f;
		if(!alreadySkidding && wheelState == WHEEL_STATE_SPINNING &&
		   (GetStatus() == STATUS_PLAYER || GetStatus() == STATUS_PLAYER_REMOTE))
			tractionLoss = m_pHandlingData->m_fTractionLoss*(1.0f - Abs(m_fGasPedal)*WS_ALREADY_SPINNING_LOSS);
		tractionLoss *= adhesion/Sqrt(slipSq);
		fwd *= tractionLoss;
		right *= tractionLoss;
	}

	if(fwd != 0.0f || right != 0.0f){
		CVector moveVec = fwd*wheelFwd + right*wheelRight;
		CVector turnVec = moveVec;

		// anti-dive: brake and drive forces are split off the move force so the
		// suspension can counteract the pitch they would otherwise cause
		bool separateTurnForce = m_pHandlingData->m_fSuspensionAntiDiveMultiplier > 0.0f && (braking || driving);
		if(separateTurnForce)
			turnVec -= (braking ? 1.0f : 0.5f)*m_pHandlingData->m_fSuspensionAntiDiveMultiplier*wheelFwd*fwd;

		float speed = moveVec.Magnitude();
		float turnSpeed = separateTurnForce ? turnVec.Magnitude() : speed;
		moveVec.Normalise();
		if(separateTurnForce)
			turnVec.Normalise();
		else
			turnVec = moveVec;

		ApplyMoveForce(moveVec*m_fMass*speed);
		ApplyTurnForce(turnVec*GetMass(wheelContactPoint, turnVec)*turnSpeed, wheelContactPoint);
	}
}

float fBurstBikeSpeedMax = 0.12f;
float fBurstBikeTyreMod = 0.05f;
float fTweakBikeWheelTurnForce = 2.0f;

void
CVehicle::ProcessBikeWheel(CVector &wheelFwd, CVector &wheelRight, CVector &wheelContactSpeed, CVector &wheelContactPoint,
	int32 wheelsOnGround, float thrust, float brake, float adhesion, float destabTraction, int8 wheelId, float *wheelSpeed, tWheelState *wheelState, eBikeWheelSpecial special, uint16 wheelStatus)
{
	// These were statics in the original: bAlreadySkidding was never reset, so
	// once any bike had skidded every wheel on every vehicle lost traction for
	// the rest of the session. The reference made them locals (see
	// CVehicle::ProcessBikeWheel in gta-reversed); bDriving and bReversing are
	// also correctly gated on "not braking", which the statics version was not.
	bool bAlreadySkidding = false;
	bool bBraking;
	bool bDriving;
	bool bReversing;

	// how much force we want to apply in these axes
	float fwd = 0.0f;
	float right = 0.0f;

	bBraking = brake != 0.0f;
	if(bBraking)
		thrust = 0.0f;
	bDriving = !bBraking && thrust != 0.0f;
	bReversing = !bBraking && thrust < 0.0f;

	float contactSpeedFwd = DotProduct(wheelContactSpeed, wheelFwd);
	float contactSpeedRight;

	if(*wheelState != WHEEL_STATE_NORMAL)
		bAlreadySkidding = true;
	*wheelState = WHEEL_STATE_NORMAL;

	adhesion *= CTimer::GetTimeStep();
	if(bAlreadySkidding)
		adhesion *= m_pHandlingData->m_fTractionLoss;

	if(special == BIKE_WHEELSPEC_2 || special == BIKE_WHEELSPEC_3)
		contactSpeedRight = 0.0f;
	else
		contactSpeedRight = DotProduct(wheelContactSpeed, wheelRight);

	// moving sideways
	if(contactSpeedRight != 0.0f){
		// exert opposing force
		right = -contactSpeedRight/wheelsOnGround;
#ifdef FIX_BUGS
		// contactSpeedRight is independent of framerate but right has timestep as a factor
		// so we probably have to fix this
		// see above
		//right *= CTimer::GetTimeStepFix();
#endif

		if(wheelStatus == WHEEL_STATUS_BURST){
			float fwdspeed = Min(contactSpeedFwd, fBurstBikeSpeedMax);
			right += fwdspeed * CGeneral::GetRandomNumberInRange(-fBurstBikeTyreMod, fBurstBikeTyreMod);
		}
	}

	if(bDriving){
		fwd = thrust;

		// limit sideways force (why?)
		if(right > 0.0f){
			if(right > adhesion)
				right = adhesion;
		}else{
			if(right < -adhesion)
				right = -adhesion;
		}
	}else if(contactSpeedFwd != 0.0f){
		fwd = -contactSpeedFwd/wheelsOnGround;
#ifdef FIX_BUGS
		// contactSpeedFwd is independent of framerate but fwd has timestep as a factor
		// so we probably have to fix this
		// see above
		//fwd *= CTimer::GetTimeStepFix();
#endif

		if(!bBraking){
			if(m_fGasPedal < 0.01f){
				if(IsBike())
					brake = 0.6f * mod_HandlingManager.fWheelFriction / (m_pHandlingData->m_fMass + 200.0f);
				else if(m_pHandlingData->m_fMass < 500.0f)
					brake = mod_HandlingManager.fWheelFriction / m_fMass;
				else if(GetModelIndex() == MI_RCBANDIT)
					brake = 0.2f * mod_HandlingManager.fWheelFriction / m_fMass;
				else
					brake = mod_HandlingManager.fWheelFriction / m_fMass;
#ifdef FIX_BUGS
				brake *= CTimer::GetTimeStepFix();
#endif
			}
		}

		if(brake > adhesion){
			if(Abs(contactSpeedFwd) > 0.005f)
				*wheelState = WHEEL_STATE_FIXED;
		}else {
			if(fwd > 0.0f){
				if(fwd > brake)
					fwd = brake;
			}else{
				if(fwd < -brake)
					fwd = -brake;
			}
		}
	}

	float speedSq = sq(right) + sq(fwd);
	if(sq(adhesion) < speedSq){
		if(*wheelState != WHEEL_STATE_FIXED){
			if(bDriving && contactSpeedFwd < 0.2f)
				*wheelState = WHEEL_STATE_SPINNING;
			else
				*wheelState = WHEEL_STATE_SKIDDING;
		}

		float l = Sqrt(speedSq);
		float tractionLoss = bAlreadySkidding ? 1.0f : m_pHandlingData->m_fTractionLoss;
		right *= adhesion * tractionLoss / l;
		fwd *= adhesion * tractionLoss / l;

		if(destabTraction < 1.0f)
			right *= destabTraction;
	}else if(destabTraction < 1.0f){
		if(!bAlreadySkidding)
			destabTraction *= m_pHandlingData->m_fTractionLoss;
		if(sq(adhesion*destabTraction) < speedSq){
			float l = Sqrt(speedSq);
			right *= adhesion * destabTraction / l;
		}
	}

	if(fwd != 0.0f || right != 0.0f){
		CVector direction = fwd*wheelFwd + right*wheelRight;

		float speed = direction.Magnitude();
		direction.Normalise();

		float impulse = speed*m_fMass;
		float turnImpulse = speed*GetMass(wheelContactPoint, direction);
		CVector vTurnImpulse = turnImpulse * direction;
		ApplyMoveForce(impulse * direction);

		float turnRight = DotProduct(vTurnImpulse, GetRight());
		float contactRight = DotProduct(wheelContactPoint, GetRight());
		float contactFwd = DotProduct(wheelContactPoint, GetForward());

		if(wheelId != BIKEWHEEL_REAR || !bBraking && !bReversing)
			ApplyTurnForce((vTurnImpulse - turnRight*GetRight()) * fTweakBikeWheelTurnForce,
				wheelContactPoint - contactRight*GetRight());

		ApplyTurnForce(turnRight*GetRight(), contactFwd*GetForward());
	}
}

float
CVehicle::ProcessWheelRotation(tWheelState state, const CVector &fwd, const CVector &speed, float radius)
{
	float angularVelocity;
	switch(state){
	case WHEEL_STATE_SPINNING:
		angularVelocity = -1.1f;	// constant speed forward
		break;
	case WHEEL_STATE_FIXED:
		angularVelocity = 0.0f;		// not moving
		break;
	default:
		angularVelocity = -DotProduct(fwd, speed) / radius;	// forward speed
		break;
	}
	return angularVelocity * CTimer::GetTimeStep();
}

int
CVehicle::FindTyreNearestPoint(float x, float y)
{
	CVector pos = CVector(x - GetPosition().x, y - GetPosition().y, 0.0f);
	float fwd = DotProduct(GetForward(), pos);
	float right = DotProduct(GetRight(), pos);

	int piece;
	if(IsBike()){
		piece = fwd > 0.0f ? CAR_PIECE_WHEEL_LF : CAR_PIECE_WHEEL_LR;
	}else{
		piece = fwd > 0.0f ?
			right > 0.0f ? CAR_PIECE_WHEEL_RF : CAR_PIECE_WHEEL_LF :
			right > 0.0f ? CAR_PIECE_WHEEL_RR : CAR_PIECE_WHEEL_LR;
	}
	return piece - CAR_PIECE_WHEEL_LF;
}

void
CVehicle::InflictDamage(CEntity *damagedBy, eWeaponType weaponType, float damage, CVector pos)
{
	if (!bCanBeDamaged)
		return;
	if(GetStatus() == STATUS_PLAYER && CStats::GetPercentageProgress() >= 100.0f)
		damage *= 0.5f;
	if (GetStatus() != STATUS_PLAYER && bOnlyDamagedByPlayer && (damagedBy != FindPlayerPed() && damagedBy != FindPlayerVehicle()))
		return;

	if(damage > 10.0f && (damagedBy == FindPlayerPed() || damagedBy == FindPlayerVehicle()) && GetStatus() != STATUS_WRECKED){
		CWorld::Players[CWorld::PlayerInFocus].m_nHavocLevel += 2;
		CWorld::Players[CWorld::PlayerInFocus].m_fMediaAttention += 1.0f;
		CStats::PropertyDestroyed += CGeneral::GetRandomNumberInRange(5, 25);
	}

	bool bFrightensDriver = false;
	switch (weaponType) {
	case WEAPONTYPE_UNARMED:
	case WEAPONTYPE_BRASSKNUCKLE:
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
			return;
		break;
	case WEAPONTYPE_COLT45:
	case WEAPONTYPE_PYTHON:
	case WEAPONTYPE_SHOTGUN:
	case WEAPONTYPE_SPAS12_SHOTGUN:
	case WEAPONTYPE_STUBBY_SHOTGUN:
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
	case WEAPONTYPE_HELICANNON:
	case WEAPONTYPE_UZI_DRIVEBY:
		if (bBulletProof)
			return;
		bFrightensDriver = true;
		break;
	case WEAPONTYPE_GRENADE:
	case WEAPONTYPE_MOLOTOV:
	case WEAPONTYPE_ROCKET:
	case WEAPONTYPE_EXPLOSION:
		if (bExplosionProof)
			return;
		bFrightensDriver = true;
		break;
	case WEAPONTYPE_FLAMETHROWER:
		if (bFireProof)
			return;
		break;
	case WEAPONTYPE_RAMMEDBYCAR:
		if (bCollisionProof)
			return;
		break;
	default:
		break;
	}

	if(bFrightensDriver && GetStatus() == STATUS_PLAYER && m_fHealth < 250.0f)
		return;

	// Pop tires
	if(damagedBy && damagedBy->IsPed() && (IsAutomobile() || IsBike())){
		int accuracy = 0;
		switch(weaponType){
		case WEAPONTYPE_COLT45:
			accuracy = 10;
			break;
		case WEAPONTYPE_PYTHON:
			if(!((CPed*)damagedBy)->IsPlayer())
				accuracy = 64;
			break;
		case WEAPONTYPE_SHOTGUN:
		case WEAPONTYPE_STUBBY_SHOTGUN:
		case WEAPONTYPE_M60:
		case WEAPONTYPE_HELICANNON:
			accuracy = 25;
			break;
		case WEAPONTYPE_TEC9:
		case WEAPONTYPE_UZI:
		case WEAPONTYPE_SILENCED_INGRAM:
		case WEAPONTYPE_MP5:
		case WEAPONTYPE_UZI_DRIVEBY:
			accuracy = 15;
			break;
		case WEAPONTYPE_M4:
		case WEAPONTYPE_RUGER:
			if(!((CPed*)damagedBy)->IsPlayer())
				accuracy = 15;
			break;
		}

		if(((CPed*)damagedBy)->IsPlayer() && (CCamera::m_bUseMouse3rdPerson || TheCamera.Using1stPersonWeaponMode()))
			accuracy = 0;

		if(accuracy != 0 && !bTyresDontBurst && (CGeneral::GetRandomNumber()&0x7F) < accuracy){
			if(IsBike())
				BurstTyre(FindTyreNearestPoint(pos.x, pos.y) + CAR_PIECE_WHEEL_LF, false);
			else if(GetVehicleAppearance() == VEHICLE_APPEARANCE_AUTOMOBILE)
				BurstTyre(FindTyreNearestPoint(pos.x, pos.y) + CAR_PIECE_WHEEL_LF, true);
		}
	}

	if (m_fHealth > 0.0f) {
		if (VehicleCreatedBy == RANDOM_VEHICLE && pDriver &&
			(GetStatus() == STATUS_SIMPLE || GetStatus() == STATUS_PHYSICS) &&
			AutoPilot.m_nCarMission == MISSION_CRUISE) {
			if (m_randomSeed < DAMAGE_FLEE_IN_CAR_PROBABILITY_VALUE) {
				CCarCtrl::SwitchVehicleToRealPhysics(this);
				AutoPilot.m_nDrivingStyle = DRIVINGSTYLE_AVOID_CARS;
				AutoPilot.m_nCruiseSpeed = GAME_SPEED_TO_CARAI_SPEED * m_pHandlingData->m_Transmission.m_fMaxCruiseVelocity;
				SetStatus(STATUS_PHYSICS);
			}
		}
		m_nLastWeaponDamage = weaponType;
		m_pLastDamageEntity = damagedBy;
		float oldHealth = m_fHealth;
		if (m_fHealth > damage) {
			m_fHealth -= damage;
			if (VehicleCreatedBy == RANDOM_VEHICLE && !IsBoat()){
				switch (GetStatus()) {
				case STATUS_SIMPLE:
				case STATUS_PHYSICS:
					if(AutoPilot.m_nDrivingStyle == DRIVINGSTYLE_PLOUGH_THROUGH ||
					   CGeneral::GetRandomNumberInRange(0.0f, 1.0f) > 0.5f && AutoPilot.m_nCarMission == MISSION_CRUISE){
						// Drive away like a maniac
						if(pDriver && pDriver->m_objective != OBJECTIVE_LEAVE_CAR){
							if(AutoPilot.m_nDrivingStyle != DRIVINGSTYLE_PLOUGH_THROUGH)
								AutoPilot.m_nCruiseSpeed *= 1.5f;
							AutoPilot.m_nDrivingStyle = DRIVINGSTYLE_PLOUGH_THROUGH;
						}
					}else{
						// Leave vehicle
						if (pDriver && pDriver->CharCreatedBy != MISSION_CHAR) {
							SetStatus(STATUS_ABANDONED);
							pDriver->bFleeAfterExitingCar = true;
							pDriver->SetObjective(OBJECTIVE_LEAVE_CAR, this);
							pDriver->Say(SOUND_PED_FLEE_SPRINT);
						}
						int time = 200;
						for (int i = 0; i < m_nNumMaxPassengers; i++) {
							if (pPassengers[i] &&
							    pPassengers[i]->m_objective != OBJECTIVE_LEAVE_CAR &&
							    pPassengers[i]->CharCreatedBy != MISSION_CHAR) {
								pPassengers[i]->bFleeAfterExitingCar = true;
								pPassengers[i]->SetObjective(OBJECTIVE_LEAVE_CAR, this);
								pPassengers[i]->m_objectiveTimer = CTimer::GetTimeInMilliseconds() + time;
								pPassengers[i]->Say(SOUND_PED_FLEE_SPRINT);
								time += 200;
							}
						}
					}
					break;
				default:
					break;
				}
			}
			if (oldHealth >= DAMAGE_HEALTH_TO_CATCH_FIRE && m_fHealth < DAMAGE_HEALTH_TO_CATCH_FIRE) {
				if (IsAutomobile()) {
					CAutomobile* pThisCar = (CAutomobile*)this;
					pThisCar->Damage.SetEngineStatus(ENGINE_STATUS_ON_FIRE);
					pThisCar->m_pSetOnFireEntity = damagedBy;
					if (damagedBy)
						damagedBy->RegisterReference((CEntity**)&pThisCar->m_pSetOnFireEntity);
				}
			}
		}
		else {
			m_fHealth = 0.0f;
			if (weaponType == WEAPONTYPE_EXPLOSION) {
				// between 1000 and 3047. Also not very nice: can't be saved by respray or cheat
				m_nBombTimer = 1000 + CGeneral::GetRandomNumber() & 0x7FF;
				m_pBlowUpEntity = damagedBy;
				if (damagedBy)
					damagedBy->RegisterReference((CEntity**)&m_pBlowUpEntity);
			}
			else
				BlowUpCar(damagedBy);
		}
	}
#ifdef FIX_BUGS // removing dumb case when shooting police car in player's own garage gives wanted level
	if (GetModelIndex() == MI_POLICE && damagedBy == FindPlayerPed() && damagedBy != nil && !bHasBeenOwnedByPlayer)
#else
	if (GetModelIndex() == MI_POLICE && damagedBy == FindPlayerPed())
#endif
		FindPlayerPed()->SetWantedLevelNoDrop(1);
}

void
CVehicle::DoFixedMachineGuns(void)
{
	if(TheCamera.Cams[TheCamera.ActiveCam].DirectionWasLooking == LOOKING_FORWARD){
		if(CPad::GetPad(0)->GetCarGunFired() && !bGunSwitchedOff){
			FireFixedMachineGuns();
		}else{
			if(CTimer::GetTimeInMilliseconds() > m_nGunFiringTime + 1400)
				m_nAmmoInClip = 20;
		}
	}
}

void
CVehicle::FireFixedMachineGuns(void)
{
	if (CTimer::GetTimeInMilliseconds() <= m_nGunFiringTime + 150)
		return;
	CVector source, target;
	float dx, dy, len;

	dx = GetForward().x;
	dy = GetForward().y;
	len = Sqrt(SQR(dx) + SQR(dy));
	if (len < 0.1f) len = 0.1f;
	dx /= len;
	dy /= len;

	m_nGunFiringTime = CTimer::GetTimeInMilliseconds();

	source = GetMatrix() * CVector(2.0f, 2.5f, 1.0f);
	target = source + CVector(dx, dy, 0.0f) * 60.0f;
	target += CVector(
		((CGeneral::GetRandomNumber() & 0xFF) - 128) * 0.015f,
		((CGeneral::GetRandomNumber() & 0xFF) - 128) * 0.015f,
		((CGeneral::GetRandomNumber() & 0xFF) - 128) * 0.02f);
	CWeapon::DoTankDoomAiming(this, pDriver, &source, &target);
	FireOneInstantHitRound(&source, &target, 15);

	source = GetMatrix() * CVector(-2.0f, 2.5f, 1.0f);
	target = source + CVector(dx, dy, 0.0f) * 60.0f;
	target += CVector(
		((CGeneral::GetRandomNumber() & 0xFF) - 128) * 0.015f,
		((CGeneral::GetRandomNumber() & 0xFF) - 128) * 0.015f,
		((CGeneral::GetRandomNumber() & 0xFF) - 128) * 0.02f);
	CWeapon::DoTankDoomAiming(this, pDriver, &source, &target);
	FireOneInstantHitRound(&source, &target, 15);

	DMAudio.PlayOneShot(m_audioEntityId, SOUND_WEAPON_SHOT_FIRED, 0.0f);

	m_nAmmoInClip--;
	if (m_nAmmoInClip == 0) {
		m_nAmmoInClip = 20;
		m_nGunFiringTime = CTimer::GetTimeInMilliseconds() + 1400;
	}
}

void
CVehicle::ActivateBomb(void)
{
	if(m_bombType == CARBOMB_TIMED){
		m_bombType = CARBOMB_TIMEDACTIVE;
		m_nBombTimer = 7000;
		m_pBlowUpEntity = FindPlayerPed();
		CGarages::TriggerMessage("GA_12", -1, 3000, -1);
		DMAudio.PlayOneShot(m_audioEntityId, SOUND_BOMB_TIMED_ACTIVATED, 1.0f);
	}else if(m_bombType == CARBOMB_ONIGNITION){
		m_bombType = CARBOMB_ONIGNITIONACTIVE;
		CGarages::TriggerMessage("GA_12", -1, 3000, -1);
		DMAudio.PlayOneShot(m_audioEntityId, SOUND_BOMB_ONIGNITION_ACTIVATED, 1.0f);
	}
}

void
CVehicle::ActivateBombWhenEntered(void)
{
	if(pDriver){
		if(!bDriverLastFrame && m_bombType == CARBOMB_ONIGNITIONACTIVE){
			// If someone enters the car and there is a bomb, detonate
			m_nBombTimer = 1000;
			m_pBlowUpEntity = m_pBombRigger;
			if(m_pBlowUpEntity)
				m_pBlowUpEntity->RegisterReference((CEntity**)&m_pBlowUpEntity);
			DMAudio.PlayOneShot(m_audioEntityId, SOUND_BOMB_TICK, 1.0f);
		}
		bDriverLastFrame = true;
	}else
		bDriverLastFrame = false;
}

void
CVehicle::ExtinguishCarFire(void)
{
	if(GetStatus() != STATUS_WRECKED)
		m_fHealth = Max(m_fHealth, 300.0f);
	if(m_pCarFire)
		m_pCarFire->Extinguish();
	if(IsAutomobile()){
		CAutomobile *car = (CAutomobile*)this;
		if(car->Damage.GetEngineStatus() >= ENGINE_STATUS_ON_FIRE)
			car->Damage.SetEngineStatus(ENGINE_STATUS_ON_FIRE-10);
		car->m_fFireBlowUpTimer = 0.0f;
	}
}

bool
CVehicle::ShufflePassengersToMakeSpace(void)
{
	if (m_nNumPassengers >= m_nNumMaxPassengers)
		return false;
	if (pPassengers[1] &&
		!(m_nGettingInFlags & CAR_DOOR_FLAG_LR) &&
		IsRoomForPedToLeaveCar(CAR_DOOR_LR, nil)) {
		if (!pPassengers[2] && !(m_nGettingInFlags & CAR_DOOR_FLAG_RR)) {
			pPassengers[2] = pPassengers[1];
			pPassengers[1] = nil;
			pPassengers[2]->m_vehDoor = CAR_DOOR_RR;
			return true;
		}
		if (!pPassengers[0] && !(m_nGettingInFlags & CAR_DOOR_FLAG_RF)) {
			pPassengers[0] = pPassengers[1];
			pPassengers[1] = nil;
			pPassengers[0]->m_vehDoor = CAR_DOOR_RF;
			return true;
		}
		return false;
	}
	if (pPassengers[2] &&
		!(m_nGettingInFlags & CAR_DOOR_FLAG_RR) &&
		IsRoomForPedToLeaveCar(CAR_DOOR_RR, nil)) {
		if (!pPassengers[1] && !(m_nGettingInFlags & CAR_DOOR_FLAG_LR)) {
			pPassengers[1] = pPassengers[2];
			pPassengers[2] = nil;
			pPassengers[1]->m_vehDoor = CAR_DOOR_LR;
			return true;
		}
		if (!pPassengers[0] && !(m_nGettingInFlags & CAR_DOOR_FLAG_RF)) {
			pPassengers[0] = pPassengers[2];
			pPassengers[2] = nil;
			pPassengers[0]->m_vehDoor = CAR_DOOR_RF;
			return true;
		}
		return false;
	}
	if (pPassengers[0] &&
		!(m_nGettingInFlags & CAR_DOOR_FLAG_RF) &&
		IsRoomForPedToLeaveCar(CAR_DOOR_RF, nil)) {
		if (!pPassengers[1] && !(m_nGettingInFlags & CAR_DOOR_FLAG_LR)) {
			pPassengers[1] = pPassengers[0];
			pPassengers[0] = nil;
			pPassengers[1]->m_vehDoor = CAR_DOOR_LR;
			return true;
		}
		if (!pPassengers[2] && !(m_nGettingInFlags & CAR_DOOR_FLAG_RR)) {
			pPassengers[2] = pPassengers[0];
			pPassengers[0] = nil;
			pPassengers[2]->m_vehDoor = CAR_DOOR_RR;
			return true;
		}
		return false;
	}
	return false;
}

void
CVehicle::MakeNonDraggedPedsLeaveVehicle(CPed *ped1, CPed *ped2, CPlayerPed *&player, CCopPed *&cop)
{
	int i;
	player = nil;
	cop = nil;

	if(ped1->IsPlayer() && ped2->m_nPedType == PEDTYPE_COP &&
	   ((CPlayerPed*)ped1)->m_pWanted->GetWantedLevel() > 0 &&
	   ped2->m_pedInObjective == ped1){
		player = (CPlayerPed*)ped1;
		cop = (CCopPed*)ped2;
		return;
	}

	bool ped1IsDriver = ped1 == pDriver;

	// Just what the hell is this weird code?
	CPed *peds[9];
	CPed *peds2[9];
	int numPeds = 0;
	int numPeds2 = 0;
	for(i = 0; i < m_nNumMaxPassengers; i++){
		CPed *p = pPassengers[i];
		if(p && p != ped1 && !p->bStayInCarOnJack){
			peds[numPeds++] = p;
			// uhh what?
			if(i > 0 || ped1IsDriver)
				peds2[numPeds2++] = p;
		}
	}

	// So we're copying this array for no reason...
	CPed *peds3[9];
	int numPeds3 = 0;
	for(i = 0; i < numPeds; i++){
		if(peds[i]->IsPlayer() && ped2->m_nPedType == PEDTYPE_COP &&
		   ((CPlayerPed*)peds[i])->m_pWanted->GetWantedLevel() > 0 &&
		   ped2->m_pedInObjective == peds[i]){
			player = (CPlayerPed*)peds[i];
			cop = (CCopPed*)ped2;
			return;
		}
		peds3[numPeds3++] = peds[i];
	}

	int time = 1800;
	for(i = 0; i < numPeds3; i++){
		peds3[i]->m_leaveCarTimer = CTimer::GetTimeInMilliseconds() + time;
		peds3[i]->SetObjective(OBJECTIVE_LEAVE_CAR, this);
		time += CGeneral::GetRandomNumberInRange(300.0f, 600.0f);
	}

	if(IsAutomobile() && numPeds2 > 0 && CGeneral::GetRandomTrueFalse())
		for(i = 0; i < numPeds2; i++)
			if(peds2[i]->IsFemale() || CGeneral::GetRandomTrueFalse()){
				peds2[i]->m_leaveCarTimer = CTimer::GetTimeInMilliseconds() + 10000;
				peds2[i]->bHeldHostageInCar = true;
				peds2[i]->bFleeAfterExitingCar = true;
			}
}

void
CVehicle::ProcessDelayedExplosion(void)
{
	if(m_nBombTimer == 0)
		return;

	int tick = CTimer::GetTimeStep()/60.0f*1000.0f;
	int16 prev = m_nBombTimer;
	if(tick > m_nBombTimer)
		m_nBombTimer = 0;
	else
		m_nBombTimer -= tick;

	if(IsAutomobile() && ((CAutomobile*)this)->m_bombType == CARBOMB_TIMEDACTIVE && (m_nBombTimer & 0xFE00) != (prev & 0xFE00))
		DMAudio.PlayOneShot(m_audioEntityId, SOUND_CAR_BOMB_TICK, 0.0f);

	if (m_nBombTimer == 0)
		BlowUpCar(m_pBlowUpEntity);
}

bool
CVehicle::IsLawEnforcementVehicle(void)
{
	switch(GetModelIndex()){
	case MI_POLICE:
	case MI_ENFORCER:
	case MI_PREDATOR:
	case MI_RHINO:
	case MI_BARRACKS:
	case MI_FBIRANCH:
	case MI_VICECHEE:
		return true;
	default:
		return false;
	}
}

bool
CVehicle::UsesSiren(void)
{
	switch(GetModelIndex()){
	case MI_FIRETRUCK:
	case MI_AMBULAN:
	case MI_FBICAR:
	case MI_MRWHOOP:
	case MI_POLICE:
	case MI_ENFORCER:
	case MI_PREDATOR:
	case MI_FBIRANCH:
	case MI_VICECHEE:
		return true;
	default:
		return false;
	}
}

bool
CVehicle::IsVehicleNormal(void)
{
	if (!pDriver || m_nNumPassengers != 0 || GetStatus() == STATUS_WRECKED)
		return false;
	return GetModelInfo()->m_vehicleClass != -1;
}

bool
CVehicle::CarHasRoof(void)
{
	if((m_pHandlingData->m_nModelFlags & VEHICLE_HANDLING_MODEL_CONVERTIBLE) == 0)
		return true;
	// component 0 is assumed to be a roof
	return m_aExtras[0] == 0 || m_aExtras[1] == 0;
}

bool
CVehicle::IsUpsideDown(void)
{
	if(GetUp().z > -0.9f)
		return false;
	return true;
}

bool
CVehicle::IsOnItsSide(void)
{
	if(GetRight().z < 0.8f && GetRight().z > -0.8f)
		return false;
	return true;
}

bool
CVehicle::CanBeDeleted(void)
{
	int i;

	if(m_nNumGettingIn || m_nGettingOutFlags)
		return false;

	if(pDriver){
		// This looks like it was inlined
		if(pDriver->CharCreatedBy == MISSION_CHAR)
			return false;
		if(pDriver->GetPedState() != PED_DRIVING &&
		   pDriver->GetPedState() != PED_DEAD)
			return false;
	}

	for(i = 0; i < ARRAY_SIZE(pPassengers); i++){
		// Same check as above
		if(pPassengers[i]){
			if(pPassengers[i]->CharCreatedBy == MISSION_CHAR)
				return false;
			if(pPassengers[i]->GetPedState() != PED_DRIVING &&
			   pPassengers[i]->GetPedState() != PED_DEAD)
				return false;
		}
		// and then again... probably because something was inlined
		if(pPassengers[i]){
			if(pPassengers[i]->GetPedState() != PED_DRIVING &&
			   pPassengers[i]->GetPedState() != PED_DEAD)
				return false;
		}
	}

	switch(VehicleCreatedBy){
	case RANDOM_VEHICLE: return true;
	case MISSION_VEHICLE: return false;
	case PARKED_VEHICLE: return true;
	case PERMANENT_VEHICLE: return false;
	}
	return true;
}

bool
CVehicle::CanPedOpenLocks(CPed *ped)
{
	if(m_nDoorLock == CARLOCK_LOCKED ||
	   m_nDoorLock == CARLOCK_LOCKED_INITIALLY ||
	   m_nDoorLock == CARLOCK_LOCKED_PLAYER_INSIDE ||
	   m_nDoorLock == CARLOCK_LOCKED_BUT_CAN_BE_DAMAGED)
		return false;
	if(ped->IsPlayer() && m_nDoorLock == CARLOCK_LOCKOUT_PLAYER_ONLY)
		return false;
	return true;
}

bool
CVehicle::CanDoorsBeDamaged(void)
{
	return m_nDoorLock == CARLOCK_NOT_USED ||
		m_nDoorLock == CARLOCK_UNLOCKED ||
		m_nDoorLock == CARLOCK_LOCKED_BUT_CAN_BE_DAMAGED;
}

bool
CVehicle::CanPedEnterCar(void)
{
	// can't enter when car is on side
	if(IsBike() || GetUp().z > 0.1f ||  GetUp().z < -0.1f){
		// also when car is moving too fast
		if(m_vecMoveSpeed.MagnitudeSqr() > sq(0.2f))
			return false;
		if(m_vecTurnSpeed.MagnitudeSqr() > sq(0.2f))
			return false;
		return true;
	}
	return false;
}

bool
CVehicle::CanPedExitCar(bool jumpExit)
{
	CVector up = GetUp();
	if(up.z > 0.1f || up.z < -0.1f){
		if (IsBoat())
			return true;
		// can't exit when car is moving too fast
		if(m_vecMoveSpeed.MagnitudeSqr() > 0.005f && !jumpExit)
			return false;
		// if car is slow enough, check turn speed
		if(Abs(m_vecTurnSpeed.x) > 0.01f ||
		   Abs(m_vecTurnSpeed.y) > 0.01f ||
		   Abs(m_vecTurnSpeed.z) > 0.01f)
			return false;
		return true;
	}else{
		// What is this? just > replaced by >= ??

		// can't exit when car is moving too fast
		if(m_vecMoveSpeed.MagnitudeSqr() >= 0.005f)
			return false;
		// if car is slow enough, check turn speed
		if(Abs(m_vecTurnSpeed.x) >= 0.01f ||
		   Abs(m_vecTurnSpeed.y) >= 0.01f ||
		   Abs(m_vecTurnSpeed.z) >= 0.01f)
			return false;
		return true;
	}
}

// SA CVehicle::CanPedJumpOutCar (0x6BFC30), ported as far as the VC class
// allows. Three differences from what was here, all of them visible:
//
// 1. reVC measured m_vecMoveSpeed.MagnitudeSqr(), i.e. INCLUDING the vertical
//    component. A car on a slope, or one that had just been dropped, has a
//    large z speed while standing still horizontally, so the ped either could
//    not bail out of a parked car on a hill or bailed out of a rocket. SA
//    measures SquaredMagnitude2D - horizontal speed only.
//
// 2. The bike branch was missing entirely, which is why bikes were being run
//    through the car test and the driver could not step off a moving bike at
//    the speed SA allows.
//
// 3. SA's "pause the vehicle while the ped steps out" tail (the 0.9 turn speed
//    damping and the sqrt-based move-speed decay) needs CVehicle::CanPedStepOutCar,
//    which VC does not have; that part is left out rather than approximated.
bool
CVehicle::CanPedJumpOutCar(CPed *ped)
{
	if (IsBike()) {
		// SA lets the rider off only when there is no other passenger in the
		// way, and only above 0.07 squared horizontal speed.
		if (!pPassengers[0] || ped == pPassengers[0])
			return m_vecMoveSpeed.MagnitudeSqr2D() >= 0.07f;
		return false;
	}

	float horSpeedSqr = m_vecMoveSpeed.MagnitudeSqr2D();

	// SA: only a flat-land car that is upright, or one that has not hit
	// anything in the last second, is held to the narrow 0.1..0.5 band. Planes,
	// helis and a tipped-over car get out at any speed above 0.1.
	if (!IsSubPlane() && !IsSubHeli() &&
	    (!IsAutomobile() || GetUp().z >= 0.3f))
		return horSpeedSqr >= 0.1f && horSpeedSqr <= 0.5f;

	return horSpeedSqr >= 0.1f;
}

bool
CVehicle::CanPedJumpOutCar(void)
{
	return CanPedJumpOutCar(nil);
}

bool
CVehicle::CanPedJumpOffBike(void)
{
	if(pPassengers[0])
		return false;
	return m_vecMoveSpeed.MagnitudeSqr() < 0.07f ? false : true;
}

void
CVehicle::ChangeLawEnforcerState(uint8 enable)
{
	if (enable) {
		if (!bIsLawEnforcer) {
			bIsLawEnforcer = true;
			CCarCtrl::NumLawEnforcerCars++;
		}
	} else {
		if (bIsLawEnforcer) {
			bIsLawEnforcer = false;
			CCarCtrl::NumLawEnforcerCars--;
		}
	}
}

CPed*
CVehicle::SetUpDriver(void)
{
	if(pDriver)
		return pDriver;
	if(VehicleCreatedBy != RANDOM_VEHICLE)
		return nil;

	pDriver = CPopulation::AddPedInCar(this, true);
	pDriver->m_pMyVehicle = this;
	pDriver->m_pMyVehicle->RegisterReference((CEntity**)&pDriver->m_pMyVehicle);
	pDriver->bInVehicle = true;
	pDriver->SetPedState(PED_DRIVING);
	if(bIsBus)
		pDriver->bRenderPedInCar = false;
	return pDriver;
}

CPed*
CVehicle::SetupPassenger(int n)
{
	int i;

	if(pPassengers[n])
		return pPassengers[n];

	if((IsTaxi() || IsLimo()) && n == 0)
		pPassengers[0] = nil;
	else{
		CPed *passenger = CPopulation::AddPedInCar(this, false);
		pPassengers[n] = passenger;
		passenger->m_pMyVehicle = this;
		passenger->m_pMyVehicle->RegisterReference((CEntity**)&pPassengers[n]->m_pMyVehicle);
		passenger->bInVehicle = true;
		passenger->SetPedState(PED_DRIVING);

		if(passenger->m_nPedType == PEDTYPE_CIVMALE || passenger->m_nPedType == PEDTYPE_CIVFEMALE)
			for(i = 0; i < n; i++)
				if(pPassengers[i] && pPassengers[n] &&
				   (pPassengers[i]->m_nPedType == PEDTYPE_CIVMALE || pPassengers[i]->m_nPedType == PEDTYPE_CIVFEMALE) &&
				   passenger->GetModelIndex() == pPassengers[i]->GetModelIndex()){
					pPassengers[n] = nil;
					CPopulation::RemovePed(passenger);
				}
	}
	if(bIsBus && pPassengers[n])
		pPassengers[n]->bRenderPedInCar = false;
	++m_nNumPassengers;
	return pPassengers[n];
}

void
CVehicle::SetDriver(CPed *driver)
{
	pDriver = driver;
	pDriver->RegisterReference((CEntity**)&pDriver);

	if(bFreebies && driver == FindPlayerPed()){
		bFreebies = false;
		switch(GetModelIndex()){
		case MI_AMBULAN:
			FindPlayerPed()->m_fHealth = Max(FindPlayerPed()->m_fHealth, Min(FindPlayerPed()->m_fHealth + 20.0f, CWorld::Players[0].m_nMaxHealth));
			break;

		case MI_TAXI:
		case MI_CABBIE:
		case MI_ZEBRA:
		case MI_KAUFMAN:
			CWorld::Players[CWorld::PlayerInFocus].m_nMoney += 12;
			break;

		case MI_POLICE:
			CStreaming::RequestModel(MI_SHOTGUN, STREAMFLAGS_DONT_REMOVE);
			bFreebies = true;
			break;

		case MI_ENFORCER:
			driver->m_fArmour = Max(driver->m_fArmour, CWorld::Players[0].m_nMaxArmour);
			break;

		case MI_CADDY:
			if(!(driver->IsPlayer() && ((CPlayerPed*)driver)->DoesPlayerWantNewWeapon(WEAPONTYPE_GOLFCLUB, true)))
				CStreaming::RequestModel(MI_GOLFCLUB, STREAMFLAGS_DONT_REMOVE);
			break;
		}
	}

	if(IsBike())
		ApplyMoveForce(-0.02f*driver->m_fMass * GetUp());
	else
		ApplyTurnForce(0.0f, 0.0f, -0.02f*driver->m_fMass,
			driver->GetPosition().x - GetPosition().x,
			driver->GetPosition().y - GetPosition().y,
			0.0f);
}

bool
CVehicle::AddPassenger(CPed *passenger)
{
	int i;

	if(IsBike())
		ApplyTurnForce(-0.02f*passenger->m_fMass * GetUp(), -0.1f*GetForward());
	else
		ApplyTurnForce(0.0f, 0.0f, -0.2f*passenger->m_fMass,
			passenger->GetPosition().x - GetPosition().x,
			passenger->GetPosition().y - GetPosition().y,
			0.0f);

	for(i = 0; i < m_nNumMaxPassengers; i++)
		if(pPassengers[i] == nil){
			pPassengers[i] = passenger;
			m_nNumPassengers++;
			return true;
		}
	return false;
}

bool
CVehicle::AddPassenger(CPed *passenger, uint8 n)
{
	if(bIsBus)
		return AddPassenger(passenger);

	if(IsBike())
		ApplyTurnForce(-0.02f*passenger->m_fMass * GetUp(), -0.1f*GetForward());
	else
		ApplyTurnForce(0.0f, 0.0f, -0.2f*passenger->m_fMass,
			passenger->GetPosition().x - GetPosition().x,
			passenger->GetPosition().y - GetPosition().y,
			0.0f);

	if(n < m_nNumMaxPassengers && pPassengers[n] == nil){
		pPassengers[n] = passenger;
		m_nNumPassengers++;
		return true;
	}
	return false;
}

void
CVehicle::RemoveDriver(void)
{
#ifdef FIX_BUGS
	if (GetStatus() != STATUS_WRECKED)
#endif
		SetStatus(STATUS_ABANDONED);
	if(pDriver == FindPlayerPed()){
		if(GetModelIndex() == MI_POLICE && CStreaming::HasModelLoaded(MI_SHOTGUN)){
			if(bFreebies){
				if(((CPlayerPed*)pDriver)->DoesPlayerWantNewWeapon(WEAPONTYPE_SHOTGUN, true))
					pDriver->GiveWeapon(WEAPONTYPE_SHOTGUN, 5, true);
				else
					pDriver->GrantAmmo(WEAPONTYPE_SHOTGUN, 5);
				bFreebies = false;
			}
			CStreaming::SetModelIsDeletable(MI_SHOTGUN);
		}else if(GetModelIndex() == MI_CADDY && CStreaming::HasModelLoaded(MI_GOLFCLUB)){
			if(((CPlayerPed*)pDriver)->DoesPlayerWantNewWeapon(WEAPONTYPE_GOLFCLUB, true))
				pDriver->GiveWeapon(WEAPONTYPE_GOLFCLUB, 1, true);
			CStreaming::SetModelIsDeletable(MI_GOLFCLUB);
		}
	}
	pDriver = nil;
}

void
CVehicle::RemovePassenger(CPed *p)
{
	if (IsTrain()){
		for (int i = 0; i < ARRAY_SIZE(pPassengers); i++){
			if (pPassengers[i] == p) {
				pPassengers[i] = nil;
				m_nNumPassengers--;
				return;
			}
		}
		return;
	}
	for (int i = 0; i < m_nNumMaxPassengers; i++){
		if (pPassengers[i] == p){
			pPassengers[i] = nil;
			m_nNumPassengers--;
			return;
		}
	}
}

bool
CVehicle::IsDriver(CPed *ped)
{
	return ped && ped == pDriver;
}

bool
CVehicle::IsDriver(int32 model)
{
	return pDriver && pDriver->GetModelIndex() == model;
}

bool
CVehicle::IsPassenger(CPed *ped)
{
	int i;
	if(ped == nil)
		return false;
	for(i = 0; i < 8; i++)
		if(pPassengers[i] == ped)
			return true;
	return false;
}

bool
CVehicle::IsPassenger(int32 model)
{
	int i;
	for(i = 0; i < 8; i++)
		if(pPassengers[i] && pPassengers[i]->GetModelIndex() == model)
			return true;
	return false;
}

void
CVehicle::UpdatePassengerList(void)
{
	int i;
	bool hasPassenger = false;
	if(m_nNumPassengers)
		for(i = 0; i < 8; i++)
			if(pPassengers[i]){
				hasPassenger = true;
				break;
			}
	if(!hasPassenger)
		m_nNumPassengers = 0;
}

void
CVehicle::ProcessCarAlarm(void)
{
	uint32 step;

	if(m_nAlarmState == 0 || m_nAlarmState == -1)
		return;

	step = CTimer::GetTimeStepInMilliseconds();
	if((uint16)m_nAlarmState < step){
		m_nAlarmState = 0;
		m_nCarHornTimer = 0;
	}else
		m_nAlarmState -= step;
}

bool
CVehicle::IsSphereTouchingVehicle(float sx, float sy, float sz, float radius)
{
	float x, y, z;
	// sphere relative to vehicle
	CVector sph = CVector(sx, sy, sz) - GetPosition();
	CColModel *colmodel = GetColModel();

	x = DotProduct(sph, GetRight());
	if(colmodel->boundingBox.min.x - radius > x ||
	   colmodel->boundingBox.max.x + radius < x)
		return false;
	y = DotProduct(sph, GetForward());
	if(colmodel->boundingBox.min.y - radius > y ||
	   colmodel->boundingBox.max.y + radius < y)
		return false;
	z = DotProduct(sph, GetUp());
	if(colmodel->boundingBox.min.z - radius > z ||
	   colmodel->boundingBox.max.z + radius < z)
		return false;

	return true;
}

RpMaterial*
SetCompAlphaCB(RpMaterial *material, void *data)
{
	uint32 alpha = (uint32)(uintptr)data;
	RwRGBA *col = (RwRGBA*)RpMaterialGetColor(material);	// get rid of const
	col->alpha = alpha;
	return material;
}

void
CVehicle::SetComponentAtomicAlpha(RpAtomic *atomic, int32 alpha)
{
	RpGeometry *geo = RpAtomicGetGeometry(atomic);
	RpGeometrySetFlags(geo, RpGeometryGetFlags(geo) | rpGEOMETRYMODULATEMATERIALCOLOR);
	RpGeometryForAllMaterials(geo, SetCompAlphaCB, (void*)alpha);
}

void
CVehicle::UpdateClumpAlpha(void)
{
	int clumpAlpha = CVisibilityPlugins::GetClumpAlpha((RpClump*)m_rwObject);
	if(bFadeOut){
		clumpAlpha -= 8;
		if(clumpAlpha < 0)
			clumpAlpha = 0;
	}else if(clumpAlpha < 255){
		clumpAlpha += 16;
		if(clumpAlpha > 255)
			clumpAlpha = 255;
	}
	CVisibilityPlugins::SetClumpAlpha((RpClump*)m_rwObject, clumpAlpha);
}

void
CVehicle::HeliDustGenerate(CEntity *heli, float radius, float ground, int rnd)
{
	int i;
	float angle;
	CColPoint point;
	CEntity *entity;
	uint8 r, g, b;

	if(heli == nil)
		return;

	uint8 surface = SURFACE_TARMAC;
	int frm = CTimer::GetFrameCounter() & 7;
	float testLowZ = ground - 10.0f;
	float dustSize = 0.0f;
	float baseSize = 1.0f;
	float offset = 1.0f;	// when heli is tilted
	float particleZ = -101.0f;
	int n = 0;

	if(heli->GetModelIndex() == MI_RCGOBLIN || heli->GetModelIndex() == MI_RCRAIDER){
		radius = 3.0f;
		dustSize = 0.04f;
		baseSize = 0.07f;
		offset = 0.3f;
	}

	CVector heliPos = heli->GetPosition();

	if(heli->IsVehicle() && ((CVehicle*)heli)->IsAutomobile()){
		heliPos.x -= (heliPos.z - ground)*heli->GetUp().x*offset*0.5f;
		heliPos.y -= (heliPos.z - ground)*heli->GetUp().y*offset*0.5f;
	}

	float steamSize = 0.25f * radius * baseSize;
	float splashSize = 0.3f * radius * baseSize;

	i = 0;
	for(i = 0; i < 32+rnd; i++){
		angle = i * TWOPI/32.0f;
		CVector pos(radius*Cos(angle), radius*Sin(angle), 0.0f);
		CVector dir = CVector(pos.x, pos.y, 1.0f)*0.01f;
		pos += heliPos;

		if(i < 32 && i == 4*frm){
			if(CWorld::ProcessVerticalLine(pos, testLowZ, point, entity, true, false, false, false, true, false, nil)){
				n = rnd;
				particleZ = point.point.z;
				surface = point.surfaceB;
			}else
				n = 0;

			float waterLevel = 0.0f;
			if(CWaterLevel::GetWaterLevel(pos, &waterLevel, false) && waterLevel > particleZ){
				surface = SURFACE_WATER;
				n = rnd;
				particleZ = waterLevel;
			}
		}

		if(n){
			pos.z = particleZ;
			if(surface == SURFACE_WATER){
				float red = (0.3*CTimeCycle::GetDirectionalRed() + CTimeCycle::GetAmbientRed_Obj())*255.0f/4.0f;
				float green = (0.3*CTimeCycle::GetDirectionalGreen() + CTimeCycle::GetAmbientGreen_Obj())*255.0f/4.0f;
				float blue = (0.3*CTimeCycle::GetDirectionalBlue() + CTimeCycle::GetAmbientBlue_Obj())*255.0f/4.0f;
				r = Clamp(red, 0.0f, 255.0f);
				g = Clamp(green, 0.0f, 255.0f);
				b = Clamp(blue, 0.0f, 255.0f);
				RwRGBA col1 = { r, g, b, (RwUInt8)CGeneral::GetRandomNumberInRange(8, 32) };
				RwRGBA col2 = { 255, 255, 255, 32 };

				if(n&1)
					CParticle::AddParticle(PARTICLE_STEAM_NY_SLOWMOTION, pos, dir, nil, steamSize, col2);
				else
					CParticle::AddParticle(PARTICLE_CAR_SPLASH, pos, dir, nil, splashSize, col1,
						CGeneral::GetRandomNumberInRange(0.0f, 10.0f),
						CGeneral::GetRandomNumberInRange(0.0f, 90.0f), 1);
			}else{
				switch(surface){
				default:
				case SURFACE_TARMAC:
					r = 10;
					g = 10;
					b = 10;
					break;
				case SURFACE_GRASS:
					r = 10;
					g = 10;
					b = 3;
					break;
				case SURFACE_GRAVEL:
					r = 10;
					g = 8;
					b = 7;
					break;
				case SURFACE_MUD_DRY:
					r = 10;
					g = 6;
					b = 3;
					break;
				case SURFACE_SAND:
				case SURFACE_SAND_BEACH:
					r = 10;
					g = 10;
					b = 7;
					break;
				}
				RwRGBA col = { r, g, b, 32 };
				if(heliPos.z - pos.z < 20.0f)
					CParticle::AddParticle(PARTICLE_HELI_DUST, pos, dir, nil, dustSize, col);
			}

			n--;
		}
	}
}

#define GLARE_MIN_DIST (13.0f)
#define GLARE_FULL_DIST (30.0f)
#define GLARE_MIN_ANGLE (0.99f)
#define GLARE_FULL_ANGLE (0.995f)

void
CVehicle::DoSunGlare(void)
{
	if(bRenderScorched || GetPosition().z < 0.0f ||
	   GetVehicleAppearance() != VEHICLE_APPEARANCE_AUTOMOBILE || CWeather::SunGlare <= 0.0f)
		return;

	CVector camDir = TheCamera.GetPosition() - GetPosition();
	float dist = camDir.Magnitude();
	camDir *= 2.0f/dist;
	CVector glareVec = camDir + CTimeCycle::GetSunDirection();
	CVector localGlareVec;
	localGlareVec.x = DotProduct(glareVec, GetRight());
	localGlareVec.y = DotProduct(glareVec, GetForward());
	localGlareVec.z = 0.0;
	localGlareVec.Normalise();

	CVector2D fwd2D = GetForward();
	fwd2D.Normalise();
	CVector2D camDir2D = camDir;
	camDir2D.Normalise();
	float fwdness = Abs(DotProduct2D(fwd2D, camDir2D));

	// check angle
	float strength;
	if(fwdness > GLARE_FULL_ANGLE)
		strength = 1.0f;
	else if(fwdness > GLARE_MIN_ANGLE)
		strength = (fwdness - GLARE_MIN_ANGLE)/(GLARE_FULL_ANGLE-GLARE_MIN_ANGLE);
	else
		return;
	// check distance
	if(dist > GLARE_FULL_DIST){
		// no max distance
	}else if(dist > GLARE_MIN_DIST)
		strength *= (dist - GLARE_MIN_DIST)/(GLARE_FULL_DIST - GLARE_MIN_DIST);
	else
		return;

	float intens = 0.8f * strength * CWeather::SunGlare;
	int r = intens * (CTimeCycle::GetSunCoreRed() + 2*255)/3.0f;
	int g = intens * (CTimeCycle::GetSunCoreGreen() + 2*255)/3.0f;
	int b = intens * (CTimeCycle::GetSunCoreBlue() + 2*255)/3.0f;

	CColModel *colmodel = GetColModel();
	CCollision::CalculateTrianglePlanes(colmodel);

	int i;
	for(i = 0; i < colmodel->numTriangles-2; i += 2){
		int a1 = colmodel->triangles[i].a;
		int b1 = colmodel->triangles[i].b;
		int c1 = colmodel->triangles[i].c;
		int a2 = colmodel->triangles[i+1].a;
		int b2 = colmodel->triangles[i+1].b;
		int c2 = colmodel->triangles[i+1].c;
		CVector vert1 = colmodel->vertices[a1].Get();
		CVector vert4;
		// Need an upward surface
		if(vert1.z <= 0.0f)
			continue;

		// trying to find a quad here
		int numTri2Verts = 0;
		if(a2 != a1 && a2 != b1 && a2 != c1){
			// a2 is not in tri1
			numTri2Verts++;
			vert4 = colmodel->vertices[a2].Get();
		}
		if(b2 != a1 && b2 != b1 && b2 != c1){
			// b2 is not in tri1
			numTri2Verts++;
			vert4 = colmodel->vertices[b2].Get();
		}
		if(c2 != a1 && c2 != b1 && c2 != c1){
			// c2 is not in tri1
			numTri2Verts++;
			vert4 = colmodel->vertices[c2].Get();
		}
		// Need exactly one vertex from tri2 for a quad with tri1
		if(numTri2Verts != 1)
			continue;

		CVector mid = (vert1 + colmodel->vertices[b1].Get() + colmodel->vertices[c1].Get() + vert4)/4.0f;
		float dy = mid.y - vert1.y;
		float dx = mid.x - vert1.x;
		float dist = 1.4f * Min(Abs(dx), Abs(dy));
		if(dist > 0.6f){
			CVector pos = GetMatrix() * (dist * localGlareVec + mid) + camDir;
			CCoronas::RegisterCorona((uintptr)this + 27 + i,
				r, g, b, 255,
				pos, 0.9f*CWeather::SunGlare, 90.0f,
				CCoronas::TYPE_STAR, CCoronas::FLARE_NONE,
				CCoronas::REFLECTION_OFF, CCoronas::LOSCHECK_OFF,
				CCoronas::STREAK_OFF, 0.0f);
		}
	}
}

void
CVehicle::KillPedsInVehicle(void)
{
	int i;
	if(pDriver){
		CDarkel::RegisterKillByPlayer(pDriver, WEAPONTYPE_EXPLOSION);
		if(pDriver->GetPedState() == PED_DRIVING){
			pDriver->SetDead();
			if(!pDriver->IsPlayer())
				pDriver->FlagToDestroyWhenNextProcessed();
		}else
			pDriver->SetDie();
	}
	for(i = 0; i < m_nNumMaxPassengers; i++){
		if(pPassengers[i]){
			CDarkel::RegisterKillByPlayer(pPassengers[i], WEAPONTYPE_EXPLOSION);
			if(pPassengers[i]->GetPedState() == PED_DRIVING){
				pPassengers[i]->SetDead();
				if(!pPassengers[i]->IsPlayer())
					pPassengers[i]->FlagToDestroyWhenNextProcessed();
			}else
				pPassengers[i]->SetDie();
		}
	}
}

void
DestroyVehicleAndDriverAndPassengers(CVehicle* pVehicle)
{
	if (pVehicle->pDriver) {
		CDarkel::RegisterKillByPlayer(pVehicle->pDriver, WEAPONTYPE_UNIDENTIFIED);
		pVehicle->pDriver->FlagToDestroyWhenNextProcessed();
	}
	for (int i = 0; i < pVehicle->m_nNumMaxPassengers; i++) {
		if (pVehicle->pPassengers[i]) {
			CDarkel::RegisterKillByPlayer(pVehicle->pPassengers[i], WEAPONTYPE_UNIDENTIFIED);
			pVehicle->pPassengers[i]->FlagToDestroyWhenNextProcessed();
		}
	}
	CWorld::Remove(pVehicle);
	delete pVehicle;
}

#ifdef COMPATIBLE_SAVES
void
CVehicle::Save(uint8*& buf)
{
	ZeroSaveBuf(buf, 4);
	WriteSaveBuf(buf, GetRight().x);
	WriteSaveBuf(buf, GetRight().y);
	WriteSaveBuf(buf, GetRight().z);
	ZeroSaveBuf(buf, 4);
	WriteSaveBuf(buf, GetForward().x);
	WriteSaveBuf(buf, GetForward().y);
	WriteSaveBuf(buf, GetForward().z);
	ZeroSaveBuf(buf, 4);
	WriteSaveBuf(buf, GetUp().x);
	WriteSaveBuf(buf, GetUp().y);
	WriteSaveBuf(buf, GetUp().z);
	ZeroSaveBuf(buf, 4);
	WriteSaveBuf(buf, GetPosition().x);
	WriteSaveBuf(buf, GetPosition().y);
	WriteSaveBuf(buf, GetPosition().z);
	ZeroSaveBuf(buf, 16);
	SaveEntityFlags(buf);
	ZeroSaveBuf(buf, 208);
	AutoPilot.Save(buf);
	WriteSaveBuf(buf, m_currentColour1);
	WriteSaveBuf(buf, m_currentColour2);
	ZeroSaveBuf(buf, 2);
	WriteSaveBuf(buf, m_nAlarmState);
	ZeroSaveBuf(buf, 42);
	WriteSaveBuf(buf, m_nNumMaxPassengers);
	ZeroSaveBuf(buf, 3);
	WriteSaveBuf(buf, field_1D0[0]);
	WriteSaveBuf(buf, field_1D0[1]);
	WriteSaveBuf(buf, field_1D0[2]);
	WriteSaveBuf(buf, field_1D0[3]);
	ZeroSaveBuf(buf, 8);
	WriteSaveBuf(buf, m_fSteerAngle);
	WriteSaveBuf(buf, m_fGasPedal);
	WriteSaveBuf(buf, m_fBrakePedal);
	WriteSaveBuf(buf, VehicleCreatedBy);
	uint8 flags = 0;
	if (bIsLawEnforcer) flags |= BIT(0);
	if (bIsLocked) flags |= BIT(3);
	if (bEngineOn) flags |= BIT(4);
	if (bIsHandbrakeOn) flags |= BIT(5);
	if (bLightsOn) flags |= BIT(6);
	if (bFreebies) flags |= BIT(7);
	WriteSaveBuf(buf, flags);
	ZeroSaveBuf(buf, 10);
	WriteSaveBuf(buf, m_fHealth);
	WriteSaveBuf(buf, m_nCurrentGear);
	ZeroSaveBuf(buf, 3);
	WriteSaveBuf(buf, m_fChangeGearTime);
	ZeroSaveBuf(buf, 12);
	WriteSaveBuf(buf, m_nTimeOfDeath);
	ZeroSaveBuf(buf, 2);
	WriteSaveBuf(buf, m_nBombTimer);
	ZeroSaveBuf(buf, 12);
	WriteSaveBuf(buf, m_nDoorLock);
	ZeroSaveBuf(buf, 108);
}

void
CVehicle::Load(uint8*& buf)
{
	CMatrix tmp;
	SkipSaveBuf(buf, 4);
	ReadSaveBuf(&tmp.GetRight().x, buf);
	ReadSaveBuf(&tmp.GetRight().y, buf);
	ReadSaveBuf(&tmp.GetRight().z, buf);
	SkipSaveBuf(buf, 4);
	ReadSaveBuf(&tmp.GetForward().x, buf);
	ReadSaveBuf(&tmp.GetForward().y, buf);
	ReadSaveBuf(&tmp.GetForward().z, buf);
	SkipSaveBuf(buf, 4);
	ReadSaveBuf(&tmp.GetUp().x, buf);
	ReadSaveBuf(&tmp.GetUp().y, buf);
	ReadSaveBuf(&tmp.GetUp().z, buf);
	SkipSaveBuf(buf, 4);
	ReadSaveBuf(&tmp.GetPosition().x, buf);
	ReadSaveBuf(&tmp.GetPosition().y, buf);
	ReadSaveBuf(&tmp.GetPosition().z, buf);
	m_matrix = tmp;
	SkipSaveBuf(buf, 16);
	LoadEntityFlags(buf);
	SkipSaveBuf(buf, 208);
	AutoPilot.Load(buf);
	ReadSaveBuf(&m_currentColour1, buf);
	ReadSaveBuf(&m_currentColour2, buf);
	SkipSaveBuf(buf, 2);
	ReadSaveBuf(&m_nAlarmState, buf);
	SkipSaveBuf(buf, 42);
	ReadSaveBuf(&m_nNumMaxPassengers, buf);
	SkipSaveBuf(buf, 3);
	ReadSaveBuf(&field_1D0[0], buf);
	ReadSaveBuf(&field_1D0[1], buf);
	ReadSaveBuf(&field_1D0[2], buf);
	ReadSaveBuf(&field_1D0[3], buf);
	SkipSaveBuf(buf, 8);
	ReadSaveBuf(&m_fSteerAngle, buf);
	ReadSaveBuf(&m_fGasPedal, buf);
	ReadSaveBuf(&m_fBrakePedal, buf);
	ReadSaveBuf(&VehicleCreatedBy, buf);
	uint8 flags;
	ReadSaveBuf(&flags, buf);
	bIsLawEnforcer = !!(flags & BIT(0));
	bIsLocked = !!(flags & BIT(3));
	bEngineOn = !!(flags & BIT(4));
	bIsHandbrakeOn = !!(flags & BIT(5));
	bLightsOn = !!(flags & BIT(6));
	bFreebies = !!(flags & BIT(7));
	SkipSaveBuf(buf, 10);
	ReadSaveBuf(&m_fHealth, buf);
	ReadSaveBuf(&m_nCurrentGear, buf);
	SkipSaveBuf(buf, 3);
	ReadSaveBuf(&m_fChangeGearTime, buf);
	m_fTransmissionInertiaRatio = 0.0f;
	m_fTransmissionInertiaSmoothing = 0.0f;
	SkipSaveBuf(buf, 12);
	ReadSaveBuf(&m_nTimeOfDeath, buf);
	SkipSaveBuf(buf, 2);
	ReadSaveBuf(&m_nBombTimer, buf);
	SkipSaveBuf(buf, 12);
	ReadSaveBuf(&m_nDoorLock, buf);
	SkipSaveBuf(buf, 108);
}
#endif

// SA CVehicle::GetDummyPositionObjSpace (0x6E1970), verbatim: the dummy
// position in model space, straight out of the model's dummy table.
CVector
CVehicle::GetDummyPositionObjSpace(int32 dummy) const
{
	return ((CVehicleModelInfo*)CModelInfo::GetModelInfo(GetModelIndex()))->m_vehicleStruct.m_avDummyPos[dummy];
}

// SA CVehicle::GetDriverSeatDummyPositionOS (NOTSA in gta-reversed): a boat has
// no seat dummy, so its "driver seat" is the front light dummy.
CVector
CVehicle::GetDriverSeatDummyPositionOS(void) const
{
	return GetDummyPositionObjSpace(IsBoat() ? DUMMY_LIGHT_FRONT_MAIN : DUMMY_SEAT_FRONT);
}

// SA CVehicle::CanBeDriven (0x6D42E0): a trailer, a train that is on a track,
// and an RC vehicle cannot be driven by a ped. Everything else can, as long as
// the model actually has a front seat dummy to put the ped in.
bool
CVehicle::CanBeDriven(void) const
{
	if(IsSubTrailer() || (IsSubTrain() && ((const CTrain*)this)->m_nTrackId != 0) || bIsRCVehicle)
		return false;

	return GetDriverSeatDummyPositionOS().MagnitudeSqr() > 0.0f;
}

// SA CVehicle::DoHeadLightBeam (0x6E0E20), verbatim.
//
// A single additive cone: two narrow base vertices at the lamp, two wide ones
// three units ahead along the light normal, and an apex point 0.2 units along
// it. Alpha fades as the camera moves off the beam axis, so the cone is only
// visible when you are looking roughly into the light.
void
CVehicle::DoHeadLightBeam(CMatrix &vehicleMatrix, bool isRight)
{
	// MAIN lamp pair only; SA looks up DUMMY_LIGHT_FRONT_MAIN here.
	CVector pointModelSpace = GetDummyPositionObjSpace(DUMMY_LIGHT_FRONT_MAIN);

	if(pointModelSpace.x == 0.0f && pointModelSpace.y == 0.0f && pointModelSpace.z == 0.0f)
		return;

	CVector point = vehicleMatrix.GetPosition() + Multiply3x3(pointModelSpace, vehicleMatrix);
	if(!isRight)
		point -= 2.0f * pointModelSpace.x * vehicleMatrix.GetRight();

	CVector pointToCamDir = TheCamera.GetPosition() - point;
	pointToCamDir.Normalise();
	const uint8 alpha = (uint8)((1.0f - Abs(DotProduct(pointToCamDir, vehicleMatrix.GetForward()))) * 32.0f);

	RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, (void*)FALSE);
	RwRenderStateSet(rwRENDERSTATEZTESTENABLE, (void*)TRUE);
	RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)TRUE);
	RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDSRCALPHA);
	RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDONE);
	RwRenderStateSet(rwRENDERSTATESHADEMODE, (void*)rwSHADEMODEGOURAUD);
	RwRenderStateSet(rwRENDERSTATETEXTURERASTER, nil);
	RwRenderStateSet(rwRENDERSTATECULLMODE, (void*)rwCULLMODECULLNONE);
#ifdef LIBRW
	// librw owns the alpha tests behind its own enum; the rwALPHATESTFUNCTION*
	// spellings are RenderWare 3.7 only. rwALPHATESTFUNCTIONGREATER with ref 0
	// is what SA sets here, i.e. "reject a zero alpha pixel".
	rw::SetRenderState(rw::ALPHATESTFUNC, rw::ALPHAGREATEREQUAL);
	rw::SetRenderState(rw::ALPHATESTREF, 1);
#endif

	// SA asks ModelIndices::IsForklift, which VC has no model id for (there is
	// no forklift in the VC vehicle list), so the ordinary 0.15 tilt applies.
	const float angleMult = 0.15f;
	CVector lightNormal = vehicleMatrix.GetForward() - vehicleMatrix.GetUp() * angleMult;
	lightNormal.Normalise();
	CVector lightRight = CrossProduct(lightNormal, pointToCamDir);
	lightRight.Normalise();
	const CVector lightPos = point - vehicleMatrix.GetForward() * 0.1f;

	CVector posn[5];
	posn[0] = lightPos - lightRight * 0.05f;
	posn[1] = lightPos + lightRight * 0.05f;
	posn[2] = lightPos + lightNormal * 3.0f - lightRight * 0.5f;
	posn[3] = lightPos + lightNormal * 3.0f + lightRight * 0.5f;
	posn[4] = lightPos + lightNormal * 0.2f;

	// Base vertices carry the fade, the two far ones are transparent so the cone
	// tapers, the apex is opaque again.
	const uint8 beamAlphas[5] = { alpha, alpha, 0, 0, alpha };

	RwIm3DVertex vertices[5];
	for(int i = 0; i < 5; i++){
		RwIm3DVertexSetRGBA(&vertices[i], 255, 255, 255, beamAlphas[i]);
		RwIm3DVertexSetPos(&vertices[i], posn[i].x, posn[i].y, posn[i].z);
	}

	if(RwIm3DTransform(vertices, 5, nil, rwIM3D_VERTEXRGBA | rwIM3D_VERTEXXYZ)){
		RwImVertexIndex indices[12] = { 0, 1, 4, 1, 3, 4, 2, 3, 4, 0, 2, 4 };
		RwIm3DRenderIndexedPrimitive(rwPRIMTYPETRILIST, indices, 12);
		RwIm3DEnd();
	}

	RwRenderStateSet(rwRENDERSTATETEXTURERASTER, (void*)FALSE);
	RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, (void*)TRUE);
	RwRenderStateSet(rwRENDERSTATEZTESTENABLE, (void*)TRUE);
	RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDSRCALPHA);
	RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDINVSRCALPHA);
	RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)FALSE);
	RwRenderStateSet(rwRENDERSTATECULLMODE, (void*)rwCULLMODECULLBACK);
}

eVehicleAppearance
CVehicle::GetVehicleAppearance(void)
{
	uint32 flags = m_pHandlingData->m_nModelFlags &
		(VEHICLE_HANDLING_MODEL_IS_BOAT |
		 VEHICLE_HANDLING_MODEL_IS_PLANE |
		 VEHICLE_HANDLING_MODEL_IS_HELI |
		 VEHICLE_HANDLING_MODEL_IS_BIKE);

	if(flags <= VEHICLE_HANDLING_MODEL_IS_HELI){
		switch(flags){
		case VEHICLE_HANDLING_MODEL_IS_HELI: return VEHICLE_APPEARANCE_HELI;
		case VEHICLE_HANDLING_MODEL_NONE:    return VEHICLE_APPEARANCE_AUTOMOBILE;
		case VEHICLE_HANDLING_MODEL_IS_BIKE: return VEHICLE_APPEARANCE_BIKE;
		default:                             return VEHICLE_APPEARANCE_NONE;
		}
	}

	if(flags == VEHICLE_HANDLING_MODEL_IS_PLANE)
		return VEHICLE_APPEARANCE_PLANE;

	if(flags != VEHICLE_HANDLING_MODEL_IS_BOAT)
		return VEHICLE_APPEARANCE_NONE;

	return VEHICLE_APPEARANCE_BOAT;
}

bool
IsVehiclePointerValid(CVehicle* pVehicle)
{
	if(pVehicle == nil)
		return false;
	int index = CPools::GetVehiclePool()->GetJustIndex_NoFreeAssert(pVehicle);
#ifdef FIX_BUGS
	if (index < 0 || index >= NUMVEHICLES)
#else
	if (index < 0 || index > NUMVEHICLES)
#endif
		return false;
	// Planes keep no collision list of their own, so they are always usable;
	// every other vehicle has to be in the world to be handed out.
	return pVehicle->m_nVehicleType == VEHICLE_TYPE_FPLANE || pVehicle->m_entryInfoList.first;
}

// SA CVehicle::SetTransmissionRotation (0x6DBBB0).
//
// A live axle is a straight bar between two wheels, so its pose is fully
// determined by where its ends are: the pitch is the angle between the axle
// and the line joining the two wheel bottoms, and the roll is how far the two
// ends differ. The MonsterTruck shows two of these - one drive shaft per axle -
// and the Quadbike one, so this is the shared implementation SA keeps on the
// base class.
void
CVehicle::SetTransmissionRotation(RwFrame *component, float angleL, float angleR, CVector wheelPos, bool isFront)
{
	if(component == nil)
		return;

	CMatrix mat(RwFrameGetMatrix(component));
	CVector savedPos = mat.GetPosition();

	float angleX = -Atan2((angleL + angleR)/2.0f - wheelPos.z, mat.GetPosition().y - wheelPos.y);
	if(isFront)
		angleX += PI;

	mat.SetRotateX(angleX);
	mat.RotateY(Atan2(angleL - angleR, Abs(wheelPos.x) + Abs(wheelPos.x)));
	mat.GetPosition() += savedPos;
	mat.UpdateRW();
}

// SA CVehicle::GetTowHitchPos (0x6DFBA0), the base fallback.
//
// A car that carries no trailer-attach dummy still has a usable hitch: the
// point one metre past the front of its collision box, on the centre line.
bool
CVehicle::GetTowHitchPos(CVector &outPos, bool checkModelInfo, CVehicle *vehicle)
{
	if(!checkModelInfo)
		return false;

	CColModel *colModel = GetColModel();
	if(colModel == nil)
		return false;

	outPos = CVector(0.0f, colModel->boundingBox.max.y + 1.0f, 0.0f);
	outPos = Multiply3x3(GetMatrix(), outPos) + GetPosition();
	return true;
}

// SA CVehicle::GetTowBarPos (0x6DFBE0), the base fallback: one metre behind the
// rear of the collision box.
bool
CVehicle::GetTowBarPos(CVector &outPos, bool checkModelInfo, CVehicle *vehicle)
{
	if(!checkModelInfo)
		return false;

	CColModel *colModel = GetColModel();
	if(colModel == nil)
		return false;

	outPos = CVector(0.0f, colModel->boundingBox.min.y - 1.0f, 0.0f);
	outPos = Multiply3x3(GetMatrix(), outPos) + GetPosition();
	return true;
}

// SA CVehicle::UpdateVehicleLinkImpl (0x6DFD00), reached through the two
// wrappers below.
//
// The coupling is a constraint, not a weld: each frame the solver measures how
// far the bar and the hitch have drifted apart and turns that into a force
// applied at the joint, which is what lets a trailer swing wide in a corner
// instead of tracking exactly behind. The check before it decides whether the
// link is still valid at all - too far apart, or folded back on itself, and the
// trailer has come unhitched.
static void
UpdateVehicleLinkImpl(CVehicle *self, bool isTowing, bool applyFullVelocityAtHookUp, bool applyDistToSpeed)
{
	CVehicle *towingVeh = isTowing ? self : self->m_pTowingVehicle;
	CVehicle *beingTowedVeh = isTowing ? self->m_pVehicleBeingTowed : self;

	CVector towedHitchPos, towingBarPos;

	if(towingVeh && beingTowedVeh &&
	   (isTowing || self->GetStatus() == STATUS_IS_TOWED || self->GetStatus() == STATUS_IS_SIMPLE_TOWED) &&
	   beingTowedVeh->GetTowHitchPos(towedHitchPos, true, towingVeh) &&
	   towingVeh->GetTowBarPos(towingBarPos, true, beingTowedVeh))
	{
		CVector linkDir = towingBarPos - towedHitchPos;
		if(isTowing)
			linkDir = -linkDir;

		if(isTowing ||
		   (1.0f*(CTimer::GetTimeStep() <= 0.7f ? 0.7f : CTimer::GetTimeStep()) >= linkDir.Magnitude() &&
		    DotProduct(towingVeh->GetForward(), self->GetForward()) >= -0.3f &&
		    DotProduct(towingVeh->GetUp(), self->GetUp()) >= 0.0f))
		{
			CVector towedHitchPosWorld = towedHitchPos - beingTowedVeh->GetPosition();
			CVector towingBarPosWorld = towingBarPos - towingVeh->GetPosition();
			CVector towedHitchPosSpeed = beingTowedVeh->GetSpeed(towedHitchPosWorld);
			CVector towingBarPosSpeed = towingVeh->GetSpeed(towingBarPosWorld);
			CVector speedDiffVec = towingBarPosSpeed - towedHitchPosSpeed;
			if(isTowing)
				speedDiffVec = -speedDiffVec;

			if(!applyFullVelocityAtHookUp){
				if(applyDistToSpeed)
					speedDiffVec = (isTowing ? 0.1f : 0.3f)*Max(1.0f, CTimer::GetTimeStep())*linkDir;
				else if(isTowing)
					speedDiffVec *= (1.0f - self->m_fMass/(beingTowedVeh->m_fMass + self->m_fMass))*0.5f;
			}

			CVector thisLinkPosWorld = isTowing ? towingBarPosWorld : towedHitchPosWorld;
			CVector thisComDir = thisLinkPosWorld - Multiply3x3(self->GetMatrix(), self->m_vecCentreOfMass);
			CVector speedDiffDir = speedDiffVec;
			speedDiffDir.Normalise();
			CVector turnForce = self->GetMass(thisComDir, speedDiffDir)*speedDiffVec;

			self->ApplyMoveForce(turnForce);
			self->ApplyTurnForce(turnForce, thisLinkPosWorld);
		}else if(!isTowing){
			self->BreakTowLink();
		}
	}else if(!isTowing){
		self->BreakTowLink();
	}
}

// SA CVehicle::UpdateTrailerLink (0x6DFC50). Called on the trailer: it pulls
// itself towards its tractor's bar.
void
CVehicle::UpdateTrailerLink(bool applyFullVelocityAtHookUp, bool applyDistToSpeed)
{
	UpdateVehicleLinkImpl(this, false, applyFullVelocityAtHookUp, applyDistToSpeed);
}

// SA CVehicle::UpdateTractorLink (0x6E0050). Called on the tractor: it pushes
// its bar towards the trailer's hitch.
void
CVehicle::UpdateTractorLink(bool applyFullVelocityAtHookUp, bool applyDistToSpeed)
{
	UpdateVehicleLinkImpl(this, true, applyFullVelocityAtHookUp, applyDistToSpeed);
}

// SA CVehicle::GetAnimGroup (inlined into the door and entry code, 0x6D0AC0).
//
// The index into the 30-group in-car animation table is a field of the
// handling record, so two vehicles sharing a model but not a handling line can
// animate differently, and a vehicle whose handling line names a group the
// table does not have falls back to group 0 rather than reading past the end.
CVehicleAnimGroup &
CVehicle::GetAnimGroup(void) const
{
	return CVehicleAnimGroupData::GetVehicleAnimGroup(GetAnimGroupId());
}

// ---------------------------------------------------------------------------
// SA vehicle tuning: fitting and removing bought parts
// ---------------------------------------------------------------------------
//
// A car that has been tuned keeps its parts as extra atomics in its clump,
// hanging off the node the part belongs to. The car's own geometry is never
// modified: a bought bumper is a second mesh on the bumper node, and taking it
// off again removes exactly that mesh.
//
// SA identifies a fitted part by the model-info pointer cached on its atomic
// (CVisibilityPlugins::GetModelInfo), so finding and removing one is an atomic
// walk. reVC has no such cache for atomics it creates itself, so the same
// information is carried in two places that are already there: the atomic id
// holds the eVehicleMod slot the part was fitted to, and the model info the
// atomic came from is the one its geometry belongs to. That is enough to tell
// a fitted part from the car's own meshes and from a different part in the
// same slot.

// How a fitted part is tagged. Both fit in the atomic id word, and the render
// callbacks only look at the flag bits.
static inline uint32
UpgradeAtomicTag(int32 modelIndex, int32 slot)
{
	return ((uint32)(modelIndex & 0xFFFF) << 16) | (uint32)(slot & 0xFFFF);
}

static inline int32
UpgradeTaggedSlot(int32 atomicId)
{
	return atomicId & 0xFFFF;
}

static inline int32
UpgradeTaggedModel(int32 atomicId)
{
	return ((uint32)atomicId >> 16) & 0xFFFF;
}

// SA CVehicle::GetReplacementUpgrade's own callback: the first atomic on the
// node that the fitting code stamped is the bought part standing in for the
// car's own mesh.
static RwObject *
FindReplacementUpgradeCB(RwObject *object, void *data)
{
	int32 *found = (int32*)data;
	if(RwObjectGetType(object) != rpATOMIC)
		return object;
	int32 id = CVisibilityPlugins::GetAtomicId((RpAtomic*)object);
	if((id & ATOMIC_FLAG_UPGRADE) == 0)
		return object;
	*found = UpgradeTaggedModel(id);
	return nil;
}

// SA CVehicle::GetUpgrade (0x6D3650) needs to know which model an atomic of the
// clump came from. reVC's clump model infos are reachable, but a part's atomic
// does not point back at its model info, so the fitting code stamps the slot on
// the atomic and remembers the model alongside it in the atomic's id word:
// the low 16 bits are the slot, the high 16 bits the model. Both fit, and the
// render callbacks only look at the flag bits.

// A fitted part is any atomic of the clump whose id was stamped by the fitting
// code. ATOMIC_FLAG_UPGRADE marks it; the tag rides along in the same word, so
// the two tests below also have to ignore the flag bits.
#define ATOMIC_UPGRADE_SLOT_MASK 0xFFFF
#define ATOMIC_UPGRADE_MODEL_MASK 0xFFFF0000u

struct tUpgradeSearch
{
	int32 slot;
	RpAtomic *found;
};

static RpAtomic *
FindUpgradeAtomicCB(RpAtomic *atomic, void *data)
{
	tUpgradeSearch *search = (tUpgradeSearch*)data;
	if(CVisibilityPlugins::GetAtomicId(atomic) & ATOMIC_FLAG_UPGRADE){
		if(UpgradeTaggedSlot(CVisibilityPlugins::GetAtomicId(atomic)) == search->slot){
			search->found = atomic;
			return nil;
		}
	}
	return atomic;
}

RpAtomic *
CVehicle::FindUpgradeAtomic(int32 slot)
{
	if(GetClump() == nil)
		return nil;
	tUpgradeSearch search;
	search.slot = slot;
	search.found = nil;
	RpClumpForAllAtomics(GetClump(), FindUpgradeAtomicCB, &search);
	return search.found;
}

struct tRemoveUpgradeData
{
	int32 slot;
	bool removed;
};

static RpAtomic *
RemoveUpgradeAtomicCB(RpAtomic *atomic, void *data)
{
	tRemoveUpgradeData *remove = (tRemoveUpgradeData*)data;
	if((CVisibilityPlugins::GetAtomicId(atomic) & ATOMIC_FLAG_UPGRADE) == 0)
		return atomic;
	if(UpgradeTaggedSlot(CVisibilityPlugins::GetAtomicId(atomic)) != remove->slot)
		return atomic;

	// The reference the fitting code took on the part's model goes with the
	// atomic, otherwise the model of a part that has been taken off never
	// unloads.
	CBaseModelInfo *partModel = CModelInfo::GetModelInfo(
		UpgradeTaggedModel(CVisibilityPlugins::GetAtomicId(atomic)));
	if(partModel)
		partModel->RemoveRef();

	// The frame is destroyed with the atomic when the atomic still owns it;
	// a fitted part keeps its own frame because the part's model created it.
	RwFrame *frame = RpAtomicGetFrame(atomic);
	RpClumpRemoveAtomic(RpAtomicGetClump(atomic), atomic);
	RpAtomicDestroy(atomic);
	if(frame)
		RwFrameDestroy(frame);
	remove->removed = true;
	return atomic;
}

// Destructor helper: drop the model reference every fitted part's atomic holds.
static RpAtomic *
ReleaseUpgradeRefsCB(RpAtomic *atomic, void *data)
{
	(void)data;
	int32 id = CVisibilityPlugins::GetAtomicId(atomic);
	if((id & ATOMIC_FLAG_UPGRADE) == 0)
		return atomic;
	CBaseModelInfo *mi = CModelInfo::GetModelInfo(UpgradeTaggedModel(id));
	if(mi && mi->GetNumRefs() > 0)
		mi->RemoveRef();
	return atomic;
}

static RpAtomic *
RemoveAllUpgradesAtomicCB(RpAtomic *atomic, void *data)
{
	bool *removed = (bool*)data;
	int32 id = CVisibilityPlugins::GetAtomicId(atomic);
	if((id & ATOMIC_FLAG_UPGRADE) == 0)
		return atomic;
	// The fitting code took a reference on the part's model for this atomic;
	// destroying the atomic without dropping it would pin the model forever.
	CBaseModelInfo *partModel = CModelInfo::GetModelInfo(UpgradeTaggedModel(id));
	if(partModel)
		partModel->RemoveRef();
	RwFrame *frame = RpAtomicGetFrame(atomic);
	RpClumpRemoveAtomic(RpAtomicGetClump(atomic), atomic);
	RpAtomicDestroy(atomic);
	if(frame)
		RwFrameDestroy(frame);
	*removed = true;
	return atomic;
}

// The eVehicleMod kind of a part, or VEHICLE_MOD_NONE when the model is not a
// part at all. Slots are the shop's numbering (eVehicleMod), not the slot in
// the car's own m_anUpgrades table, so a part that fits several slots works.
static int32
GetUpgradeModKind(int32 modelId)
{
	CBaseModelInfo *mi = CModelInfo::GetModelInfo(modelId);
	if(mi == nil || mi->m_nVehicleMod == VEHICLE_MOD_NONE)
		return -1;
	return mi->m_nVehicleMod;
}

// Which node of the car a part hangs on. The model info knows where each
// eVehicleUpgradePosn attaches; a part that carries its own dummy (a wing panel
// hangs on its own node) names the node by its eVehicleMod value instead.
static int32
GetUpgradeParentNode(CVehicleModelInfo *carModel, int32 modKind)
{
	// eVehicleMod -> the eVehicleUpgradePosn whose slot describes the same
	// place on the car. Several eVehicleMod values share a slot: the two halves
	// of a split bonnet both sit on the bonnet, and a wing panel sits on the
	// wing node the slot names.
	switch(modKind){
	case VEHICLE_MOD_UPGRADE_BONNET:
	case VEHICLE_MOD_UPGRADE_BONNET_LEFT_RIGHT:
		return carModel->m_vehicleStruct.m_aUpgrades[UPGRADE_BONNET].m_nodeIndex;
	case VEHICLE_MOD_UPGRADE_SPOILER:
		return carModel->m_vehicleStruct.m_aUpgrades[UPGRADE_SPOILER].m_nodeIndex;
	case VEHICLE_MOD_UPGRADE_FRONT_BULLBAR:
		return carModel->m_vehicleStruct.m_aUpgrades[UPGRADE_FRONTBULLBAR].m_nodeIndex;
	case VEHICLE_MOD_UPGRADE_REAR_BULLBAR:
		return carModel->m_vehicleStruct.m_aUpgrades[UPGRADE_BACKBULLBAR].m_nodeIndex;
	case VEHICLE_MOD_UPGRADE_FRONT_LIGHTS:
		return carModel->m_vehicleStruct.m_aUpgrades[UPGRADE_LIGHTS].m_nodeIndex;
	case VEHICLE_MOD_UPGRADE_ROOF:
		return carModel->m_vehicleStruct.m_aUpgrades[UPGRADE_ROOF].m_nodeIndex;
	case VEHICLE_MOD_UPGRADE_NITRO:
		return carModel->m_vehicleStruct.m_aUpgrades[UPGRADE_NITRO].m_nodeIndex;
	case VEHICLE_MOD_UPGRADE_STEREO:
	case VEHICLE_MOD_UPGRADE_HYDRAULICS:
		// Neither is geometry on the outside of the car: the stereo lives
		// inside, hydraulics are under the body. They hang on the chassis.
		return CAR_CHASSIS;
	case VEHICLE_MOD_REPLACEMENT_CHASSIS:
		return CAR_CHASSIS;
	case VEHICLE_MOD_REPLACEMENT_WHEEL:
		return CAR_WHEEL_RF;
	case VEHICLE_MOD_REPLACEMENT_EXHAUST:
		return CAR_EXHAUST;
	case VEHICLE_MOD_REPLACEMENT_FRONT_BUMPER:
		return CAR_BUMP_FRONT;
	case VEHICLE_MOD_REPLACEMENT_REAR_BUMPER:
		return CAR_BUMP_REAR;
	case VEHICLE_MOD_REPLACEMENT_MISC:
		return CAR_MISC_A;
	default:
		return CAR_NODE_NONE;
	}
}

// The slot of the car's m_anUpgrades table a part fills. The slot is the
// eVehicleMod value the shop uses, which is what SA's AddVehicleUpgrade maps a
// bought part to as well.
static int32
GetUpgradeSlotForModKind(int32 modKind)
{
	return modKind;
}

// SA CVehicle::AddUpgrade (0x6DFA20).
//
// Instantiate one model and hang it on the node its kind names. SA keeps a
// table of the two-atomics-per-part cases (a wing panel is one part with a left
// and a right half both in the same model); reVC's part models are single
// atomics, so one atomic per part is what gets attached.
void
CVehicle::AddUpgrade(int32 modelIndex, int32 upgradeIndex)
{
	CBaseModelInfo *mi = CModelInfo::GetModelInfo(modelIndex);
	if(mi == nil){
		REVC_LOG_WARNING("%s: upgrade model %d does not exist", GetModelInfo()->GetModelName(), modelIndex);
		return;
	}

	CStreaming::RequestVehicleUpgrade(modelIndex, STREAMFLAGS_DEPENDENCY);
	if(!CStreaming::HasVehicleUpgradeLoaded(modelIndex)){
		// Keep the slot: the part is recorded and gets fitted the next time the
		// car is set up, exactly like SA does when a part is still streaming.
		REVC_LOG_INFO("%s: upgrade model %d is not loaded yet", GetModelInfo()->GetModelName(), modelIndex);
		return;
	}

	RwObject *object = mi->CreateInstance();
	if(object == nil || RwObjectGetType(object) != rpATOMIC){
		REVC_LOG_WARNING("%s: upgrade model %d is not an atomic", GetModelInfo()->GetModelName(), modelIndex);
		return;
	}
	RpAtomic *atomic = (RpAtomic*)object;
	mi->AddRef();

	int32 node = GetUpgradeParentNode((CVehicleModelInfo*)GetModelInfo(), upgradeIndex);
	if(IsAutomobile() && upgradeIndex == VEHICLE_MOD_REPLACEMENT_WHEEL){
		// A wheel upgrade is one rim model worn by all four wheels: SA clones
		// it onto every wheel node the car has, so a car with a damaged or
		// missing fourth wheel still gets the three that exist.
		static const int32 wheelNodes[NUM_WHEEL_SLOTS] = { CAR_WHEEL_RF, CAR_WHEEL_RM, CAR_WHEEL_RB, CAR_WHEEL_LF };
		for(int32 w = 0; w < NUM_WHEEL_SLOTS; w++){
			RwFrame *wheelFrame = ((CAutomobile*)this)->m_aCarNodes[wheelNodes[w]];
			if(wheelFrame == nil)
				continue;
			// SA swaps the mesh: the car's own wheel atomic leaves the clump so
			// the rim is what gets drawn, and comes back when the rims come off.
			// The atomic is not destroyed, because it belongs to the model.
			if(m_apHiddenWheelAtomics[w] == nil){
				RwObject *stockObject = GetFirstObject(wheelFrame);
				if(stockObject && RwObjectGetType(stockObject) == rpATOMIC){
					m_apHiddenWheelAtomics[w] = (RpAtomic*)stockObject;
					RpClumpRemoveAtomic(GetClump(), (RpAtomic*)stockObject);
				}
			}
			RwObject *wheelObject = mi->CreateInstance();
			if(wheelObject == nil || RwObjectGetType(wheelObject) != rpATOMIC)
				continue;
			RpAtomic *wheelAtomic = (RpAtomic*)wheelObject;
			mi->AddRef();
			RwFrame *wheelChild = RpAtomicGetFrame(wheelAtomic);
			RpClumpAddAtomic(GetClump(), wheelAtomic);
			if(wheelChild){
				RwFrameRemoveChild(wheelChild);
				RwFrameAddChild(wheelFrame, wheelChild);
				RwFrameUpdateObjects(wheelChild);
			}
			CVisibilityPlugins::SetAtomicId(wheelAtomic,
				(int)UpgradeAtomicTag(modelIndex, GetUpgradeSlotForModKind(upgradeIndex)) | ATOMIC_FLAG_UPGRADE);
			// The rim is a wheel mesh, so it is drawn through the SA wheel
			// callback that applies the model's wheel scale and spin.
			CVisibilityPlugins::SetAtomicRenderCallback(wheelAtomic, CVisibilityPlugins::RenderSAWheelAtomicCB);
		}
		return;
	}

	RwFrame *parentFrame = nil;
	if(IsAutomobile() && node > CAR_NODE_NONE && node < NUM_CAR_NODES)
		parentFrame = ((CAutomobile*)this)->m_aCarNodes[node];
	if(parentFrame == nil)
		parentFrame = RpClumpGetFrame(GetClump());

	// The atomic keeps its own frame - the part was modelled in model space and
	// the frame carries that transform - and the frame is re-parented onto the
	// node, so moving the node moves the part.
	RwFrame *frame = RpAtomicGetFrame(atomic);
	RpClumpAddAtomic(GetClump(), atomic);
	if(frame){
		RwFrameRemoveChild(frame);
		RwFrameAddChild(parentFrame, frame);
		RwFrameUpdateObjects(frame);
	}

	CVisibilityPlugins::SetAtomicId(atomic,
		(int)(UpgradeAtomicTag(modelIndex, GetUpgradeSlotForModKind(upgradeIndex))) | ATOMIC_FLAG_UPGRADE);
	// A part is drawn like a high-detail component; alpha materials need the
	// sorted path so the windows of a fitted part blend with the car's own.
	bool hasAlpha = false;
	RpGeometryForAllMaterials(RpAtomicGetGeometry(atomic), CVehicleModelInfo::HasAlphaMaterialCB, &hasAlpha);
	if(hasAlpha){
		CVisibilityPlugins::SetAtomicRenderCallback(atomic, CVisibilityPlugins::RenderVehicleHiDetailAlphaCB);
		CVisibilityPlugins::SetAtomicFlag(atomic, ATOMIC_FLAG_DRAWLAST);
	}else{
		CVisibilityPlugins::SetAtomicRenderCallback(atomic, CVisibilityPlugins::RenderVehicleHiDetailCB);
	}
}

// SA CVehicle::AddVehicleUpgrade (0x6E3290).
//
// The public entry point: the shop, the garage and the script all call this
// with a model id. Which slot the part fills comes from the part's own model
// info; the three functional upgrades also set their flag and, for nitro, the
// bottle count.
void
CVehicle::AddVehicleUpgrade(int32 modelId)
{
	CBaseModelInfo *mi = CModelInfo::GetModelInfo(modelId);
	if(mi == nil){
		REVC_LOG_WARNING("%s: no model info for upgrade %d", GetModelInfo()->GetModelName(), modelId);
		return;
	}

	int32 modKind = GetUpgradeModKind(modelId);
	if(modKind < 0){
		REVC_LOG_WARNING("%s: model %d is not a vehicle upgrade part",
			GetModelInfo()->GetModelName(), modelId);
		return;
	}

	int32 slot = GetUpgradeSlotForModKind(modKind);
	if(slot < 0 || slot >= NUM_VEHICLE_UPGRADES){
		REVC_LOG_WARNING("%s: upgrade model %d has no slot", GetModelInfo()->GetModelName(), modelId);
		return;
	}

	// Fitting a part into an occupied slot swaps it: the old one goes first, so
	// the car never carries two spoilers.
	if(m_anUpgrades[slot] != -1 && m_anUpgrades[slot] != modelId)
		RemoveVehicleUpgrade(m_anUpgrades[slot]);

	// A part the car does not accept (its model has no such node) is refused
	// rather than hung on the chassis: the shop already hides those slots, but
	// a script can ask for anything.
	CVehicleModelInfo *carModel = (CVehicleModelInfo*)GetModelInfo();
	int32 node = GetUpgradeParentNode(carModel, modKind);
	if(carModel && node > CAR_NODE_NONE && node < NUM_CAR_NODES){
		if(IsAutomobile() && ((CAutomobile*)this)->m_aCarNodes[node] == nil){
			REVC_LOG_WARNING("%s: accepts no part for slot %d", carModel->GetModelName(), slot);
			return;
		}
	}

	m_anUpgrades[slot] = (int16)modelId;

	switch(modKind){
	case VEHICLE_MOD_UPGRADE_HYDRAULICS:
		// SA sets the handling flag and lets the vehicle shake itself; the
		// geometry is a placeholder inside the body.
		bHydraulicInst = true;
		break;
	case VEHICLE_MOD_UPGRADE_NITRO:
		bNosInst = true;
		// SA fills the bottle when the upgrade is bought. 101 is its "full"
		// marker: the counter counts down and refills to it.
		if(m_nNitroBoosts == 0)
			m_nNitroBoosts = 101;
		break;
	case VEHICLE_MOD_UPGRADE_STEREO:
		bUpgradedStereo = true;
		break;
	default:
		break;
	}

	AddUpgrade(modelId, modKind);
}

// SA CVehicle::RemoveVehicleUpgrade (0x6DF930).
void
CVehicle::RemoveVehicleUpgrade(int32 upgradeModelIndex)
{
	CBaseModelInfo *mi = CModelInfo::GetModelInfo(upgradeModelIndex);
	if(mi == nil)
		return;

	int32 modKind = GetUpgradeModKind(upgradeModelIndex);
	if(modKind < 0)
		return;

	int32 slot = GetUpgradeSlotForModKind(modKind);
	if(slot < 0 || slot >= NUM_VEHICLE_UPGRADES)
		return;

	tRemoveUpgradeData remove;
	remove.slot = slot;
	remove.removed = false;
	RpClumpForAllAtomics(GetClump(), RemoveUpgradeAtomicCB, &remove);

	if(m_anUpgrades[slot] == (int16)upgradeModelIndex || remove.removed)
		m_anUpgrades[slot] = -1;

	if(modKind == VEHICLE_MOD_REPLACEMENT_WHEEL){
		// Put the car's own wheel meshes back before the rims go away,
		// otherwise the car would be left rolling on nothing.
		for(int32 w = 0; w < NUM_WHEEL_SLOTS; w++){
			if(m_apHiddenWheelAtomics[w] == nil)
				continue;
			RpClumpAddAtomic(GetClump(), m_apHiddenWheelAtomics[w]);
			m_apHiddenWheelAtomics[w] = nil;
		}
	}

	switch(modKind){
	case VEHICLE_MOD_UPGRADE_HYDRAULICS:
		bHydraulicInst = false;
		break;
	case VEHICLE_MOD_UPGRADE_NITRO:
		bNosInst = false;
		m_nNitroBoosts = 0;
		break;
	case VEHICLE_MOD_UPGRADE_STEREO:
		bUpgradedStereo = false;
		break;
	default:
		break;
	}
}

// SA CVehicle::RemoveUpgrade (0x6D3630). Every atomic carrying the slot goes,
// whether or not it is the one m_anUpgrades names.
void
CVehicle::RemoveUpgrade(int32 upgradeId)
{
	if(GetClump() == nil)
		return;
	tRemoveUpgradeData remove;
	remove.slot = upgradeId;
	remove.removed = false;
	RpClumpForAllAtomics(GetClump(), RemoveUpgradeAtomicCB, &remove);
	if(upgradeId >= 0 && upgradeId < NUM_VEHICLE_UPGRADES)
		m_anUpgrades[upgradeId] = -1;
}

// SA CVehicle::RemoveAllUpgrades (0x6D3AB0).
void
CVehicle::RemoveAllUpgrades(void)
{
	if(GetClump()){
		bool removed = false;
		RpClumpForAllAtomics(GetClump(), RemoveAllUpgradesAtomicCB, &removed);
		// The atomics are gone, so the references the fitting code took on their
		// models have to go too - but the wheels that were hidden for a rim
		// upgrade are not upgrades, and stay hidden until they are restored.
	}
	for(int32 w = 0; w < NUM_WHEEL_SLOTS; w++){
		if(m_apHiddenWheelAtomics[w] == nil)
			continue;
		RpClumpAddAtomic(GetClump(), m_apHiddenWheelAtomics[w]);
		m_apHiddenWheelAtomics[w] = nil;
	}
	for(int32 i = 0; i < NUM_VEHICLE_UPGRADES; i++)
		m_anUpgrades[i] = -1;
	bHydraulicInst = false;
	bNosInst = false;
	bUpgradedStereo = false;
	m_nNitroBoosts = 0;
}

// SA CVehicle::GetUpgrade (0x6D3650).
int32
CVehicle::GetUpgrade(int32 upgradeId)
{
	if(upgradeId < 0 || upgradeId >= NUM_VEHICLE_UPGRADES)
		return -1;

	RpAtomic *atomic = FindUpgradeAtomic(upgradeId);
	if(atomic)
		return UpgradeTaggedModel(CVisibilityPlugins::GetAtomicId(atomic));

	return m_anUpgrades[upgradeId];
}

// SA CVehicle::SetupUpgradesAfterLoad (0x6E3400). A save only carries model
// ids; the clump of a freshly created car has no parts on it, so every recorded
// slot is fitted now and then cleared.
void
CVehicle::SetupUpgradesAfterLoad(void)
{
	for(int32 i = 0; i < NUM_VEHICLE_UPGRADES; i++){
		if(m_anUpgrades[i] == -1)
			continue;
		int32 model = m_anUpgrades[i];
		m_anUpgrades[i] = -1;
		AddVehicleUpgrade(model);
	}
}

// SA CVehicle::AddReplacementUpgrade (0x6D3830). A bought part that replaces
// one of the car's own meshes instead of hanging next to it: the old atomic is
// removed from the node and the new one is hung in its place.
void
CVehicle::AddReplacementUpgrade(int32 modelIndex, int32 nodeId)
{
	CBaseModelInfo *mi = CModelInfo::GetModelInfo(modelIndex);
	if(mi == nil)
		return;
	if(!IsAutomobile())
		return;
	CAutomobile *car = (CAutomobile*)this;
	if(nodeId <= CAR_NODE_NONE || nodeId >= NUM_CAR_NODES || car->m_aCarNodes[nodeId] == nil)
		return;

	CStreaming::RequestVehicleUpgrade(modelIndex, STREAMFLAGS_DEPENDENCY);
	if(!CStreaming::HasVehicleUpgradeLoaded(modelIndex))
		return;

	RwObject *object = mi->CreateInstance();
	if(object == nil || RwObjectGetType(object) != rpATOMIC)
		return;
	RpAtomic *atomic = (RpAtomic*)object;
	mi->AddRef();

	RwFrame *frame = RpAtomicGetFrame(atomic);
	RpClumpAddAtomic(GetClump(), atomic);
	if(frame){
		RwFrameRemoveChild(frame);
		RwFrameAddChild(car->m_aCarNodes[nodeId], frame);
		RwFrameUpdateObjects(frame);
	}
	CVisibilityPlugins::SetAtomicId(atomic,
		(int)(UpgradeAtomicTag(modelIndex, mi->m_nVehicleMod)) | ATOMIC_FLAG_UPGRADE);
	CVisibilityPlugins::SetAtomicRenderCallback(atomic, CVisibilityPlugins::RenderVehicleHiDetailCB);
}

// SA CVehicle::RemoveReplacementUpgrade (0x6D39E0). The car's own part is not
// re-created from the model: the model's clump still holds it, so it is cloned
// back onto the node the bought part was hanging on.
void
CVehicle::RemoveReplacementUpgrade(int32 frameId)
{
	if(!IsAutomobile())
		return;
	CAutomobile *car = (CAutomobile*)this;
	if(frameId <= CAR_NODE_NONE || frameId >= NUM_CAR_NODES || car->m_aCarNodes[frameId] == nil)
		return;

	bool removed = false;
	RpClumpForAllAtomics(GetClump(), RemoveAllUpgradesAtomicCB, &removed);

	CVehicleModelInfo *mi = (CVehicleModelInfo*)GetModelInfo();
	if(mi == nil || mi->m_clump == nil)
		return;

	// Copy the original meshes of that node back out of the model's clump. The
	// model is shared, so its own atomics are cloned rather than moved.
	RwObjectIdAssociation assoc;
	assoc.frame = nil;
	assoc.id = frameId;
	RwFrameForAllChildren(RpClumpGetFrame(mi->m_clump), CClumpModelInfo::FindFrameFromIdCB, &assoc);
	if(assoc.frame == nil)
		return;

	RwFrame *source = assoc.frame;
	RwObject *sourceObject = GetFirstObject(source);
	if(sourceObject && RwObjectGetType(sourceObject) == rpATOMIC){
		RpAtomic *clone = RpAtomicClone((RpAtomic*)sourceObject);
		RpAtomicSetFrame(clone, car->m_aCarNodes[frameId]);
		RpClumpAddAtomic(GetClump(), clone);
	}
}

// SA CVehicle::GetReplacementUpgrade (0x6D3A50). The model of the part fitted
// to a node, or -1 when the node still carries the car's own mesh.
int32
CVehicle::GetReplacementUpgrade(int32 nodeId)
{
	if(nodeId <= CAR_NODE_NONE || nodeId >= NUM_CAR_NODES)
		return -1;

	if(!IsAutomobile())
		return -1;

	RwFrame *node = ((CAutomobile*)this)->m_aCarNodes[nodeId];
	if(node == nil)
		return -1;

	int32 found = -1;
	RwFrameForAllObjects(node, FindReplacementUpgradeCB, &found);
	return found;
}

// ---------------------------------------------------------------------------
// SA vehicle paintjobs (remap textures)
// ---------------------------------------------------------------------------

// SA CVehicle::GetRemapIndex (0x6D0B70).
int32
CVehicle::GetRemapIndex(void)
{
	CVehicleModelInfo *mi = (CVehicleModelInfo*)GetModelInfo();
	if(mi == nil)
		return -1;
	int32 numRemaps = mi->GetNumRemaps();
	if(numRemaps <= 0)
		return -1;
	for(int32 i = 0; i < numRemaps; i++)
		if(mi->m_anRemapTxds[i] == m_nPreviousRemapTxd)
			return i;
	return -1;
}

// SA CVehicle::SetRemapTexDictionary (0x6D0BC0).
//
// Taking a paintjob off (-1) drops the reference on the dictionary that is
// currently applied and forgets its texture. Setting one only records the slot;
// the texture is fetched once the dictionary is loaded, which ApplyRemapTexture
// does before the car is drawn.
void
CVehicle::SetRemapTexDictionary(int32 txdId)
{
	if(txdId == m_nPreviousRemapTxd)
		return;

	if(txdId == -1){
		m_pRemapTexture = nil;
		if(m_nPreviousRemapTxd != -1)
			CTxdStore::RemoveRef(m_nPreviousRemapTxd);
		m_nPreviousRemapTxd = -1;
		m_nRemapTxd = -1;
		return;
	}
	m_nRemapTxd = (int16)txdId;
}

// SA CVehicle::SetRemap (0x6D0C00).
void
CVehicle::SetRemap(int32 remapIndex)
{
	if(remapIndex == -1){
		SetRemapTexDictionary(-1);
		return;
	}
	CVehicleModelInfo *mi = (CVehicleModelInfo*)GetModelInfo();
	if(mi == nil || remapIndex < 0 || remapIndex >= mi->GetNumRemaps())
		return;
	SetRemapTexDictionary(mi->m_anRemapTxds[remapIndex]);
}

// Stream in the paintjob dictionary, take the first texture of it and publish
// it to the model for this frame. SA does exactly this in its render setup: the
// model's editable materials are pointed at ms_pRemapTexture and restored after
// the draw, which is why the texture is a static and not a per-material field.
void
CVehicle::ApplyRemapTexture(void)
{
	CVehicleModelInfo *mi = (CVehicleModelInfo*)GetModelInfo();
	if(mi == nil)
		return;

	if(m_nRemapTxd == -1){
		CVehicleModelInfo::ms_pRemapTexture = nil;
		return;
	}

	if(m_nRemapTxd != m_nPreviousRemapTxd){
		CStreaming::RequestTxd(m_nRemapTxd, STREAMFLAGS_KEEP_IN_MEMORY);
		if(!CStreaming::HasTxdLoaded(m_nRemapTxd))
			return;

		TxdDef *def = CTxdStore::GetSlot(m_nRemapTxd);
		if(def == nil || def->texDict == nil)
			return;

		// The paintjob is the first texture of the dictionary: a remap txd
		// carries one texture, the body skin.
		RwTexture *texture = GetFirstTexture(def->texDict);
		if(texture == nil)
			return;

		// The old dictionary loses its reference only when the new one is in
		// hand, so a car never ends up with no paintjob at all in between.
		if(m_nPreviousRemapTxd != -1 && m_nPreviousRemapTxd != m_nRemapTxd)
			CTxdStore::RemoveRef(m_nPreviousRemapTxd);
		CTxdStore::AddRef(m_nRemapTxd);
		m_pRemapTexture = texture;
		m_nPreviousRemapTxd = m_nRemapTxd;
	}

	CVehicleModelInfo::ms_pRemapTexture = m_pRemapTexture;
}
