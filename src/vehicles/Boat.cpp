#include "common.h"

#include "main.h"
#include "General.h"
#include "Timecycle.h"
#include "Weather.h"
#include "cHandlingDataMgr.h"
#include "CarAI.h"
#include "CarCtrl.h"
#include "RwHelper.h"
#include "ModelIndices.h"
#include "VisibilityPlugins.h"
#include "DMAudio.h"
#include "Camera.h"
#include "Darkel.h"
#include "Explosion.h"
#include "Particle.h"
#include "ParticleObject.h"
#include "WaterLevel.h"
#include "Floater.h"
#include "World.h"
#include "Stats.h"
#include "Pools.h"
#include "Pad.h"
#include "Boat.h"
#include "AnimBlendAssociation.h"
#include "RpAnimBlend.h"
#include "Record.h"
#include "Shadows.h"
#include "Wanted.h"
#include "SaveBuf.h"
#include "ControllerConfig.h"

#define INVALID_ORIENTATION (-9999.99f)

float CBoat::MAX_WAKE_LENGTH = 50.0f;
float CBoat::MIN_WAKE_INTERVAL = 2.0f;
float CBoat::WAKE_LIFETIME = 150.0f;

float fShapeLength = 0.4f;
float fShapeTime = 0.05f;
float fRangeMult = 0.6f;
float fTimeMult = 1.2f/CBoat::WAKE_LIFETIME;

static const float BOAT_STEER_SMOOTH_RATE = 0.2f;
static const float BOAT_MOUSE_STEER_SCALE = -0.0035f;
static const float BOAT_STEER_DAMP_THRESHOLD = 0.5f;
static const float BOAT_STEER_DAMP_BASE = 0.985f;

CBoat *CBoat::apFrameWakeGeneratingBoats[4];

const uint32 CBoat::nSaveStructSize =
#ifdef COMPATIBLE_SAVES
	1216;
#else
	sizeof(CBoat);
#endif

CBoat::CBoat(int mi, uint8 owner) : CVehicle(owner)
{
	CVehicleModelInfo *minfo = (CVehicleModelInfo*)CModelInfo::GetModelInfo(mi);
	m_nVehicleType = VEHICLE_TYPE_BOAT;
m_nVehicleSubType = VEHICLE_TYPE_BOAT;
	m_nPadID = 0;
	m_fMovingRotation = 0.0f;
	m_fMovingSpeed = 0.0f;
	m_skimmerThingTimer = 0.0f;
	m_nPoliceShoutTimer = CTimer::GetTimeInMilliseconds();
	SetModelIndex(mi);

	m_pHandlingData = mod_HandlingManager.GetVehiclePointer((uint32)minfo->m_handlingId);
	m_pFlyingHandlingData = mod_HandlingManager.GetFlyingPointer((uint32)minfo->m_handlingId);
	m_pBoatHandlingData = mod_HandlingManager.GetBoatPointer((uint32)minfo->m_handlingId);
	minfo->ChooseVehicleColour(m_currentColour1, m_currentColour2, m_currentColour3, m_currentColour4, 1);

	m_fMass = m_pHandlingData->m_fMass;
	m_fTurnMass = m_pHandlingData->m_fTurnMass / 2.0f;
	m_vecCentreOfMass = m_pHandlingData->m_vecCentreOfMass;
	m_fAirResistance = (float)minfo->GetColModel()->boundingBox.max.x * (float)minfo->GetColModel()->boundingBox.max.z / m_fMass;
	m_fElasticity = 0.1f;
	m_fBuoyancyConstant = m_pHandlingData->m_fBuoyancyConstant;
	m_fSteerAngle = 0.0f;
	m_fGasPedal = 0.0f;
	m_EngineSpeed = 0.0f;
	m_PropellerAngle = 0.0f;
	m_fBrakePedal = 0.0f;

	m_boat_unused3 = false;

	m_fVolumeUnderWater = 7.0f;
	m_fPrevVolumeUnderWater = 7.0f;
	m_vecBuoyancePoint = CVector(0.0f, 0.0f, 0.0f);

	m_nDeltaVolumeUnderWater = 0;
	bBoatInWater = true;
	bPropellerInWater = true;

	bIsInWater = true;

	m_phys_unused1 = 0.0f;
	m_boat_unused2 = 0;
	m_bIsAnchored = true;
	m_fOrientation = INVALID_ORIENTATION;
	bTouchingWater = true;
	m_fDamage = 0.0f;
	m_pSetOnFireEntity = nil;
	m_NumWakeCoords = 0;

	for (int16 i = 0; i < ARRAY_SIZE(m_WakePtCounters); i++)
		m_WakePtCounters[i] = 0.0f;

	m_nAmmoInClip = 20;

	// SA CBoat::CBoat (0x6EFF30), verbatim. Note the argument order: SA passes
	// the open angle first and the closed angle second, so MARQUIS gets
	// (+PI/10, -PI/10) - the flap hangs at the negative limit.
	m_OldMoveSpeed = CVector(0.0f, 0.0f, 0.0f);
	m_OldTurnSpeed = CVector(0.0f, 0.0f, 0.0f);
	if(GetModelIndex() == MI_MARQUIS)
		m_boom.Init(PI/10.0f, -PI/10.0f, DOOR_AXIS_NEG_Y, DOOR_AXIS_Y, DOOR_EXTRA_NONE);
	else
		m_boom.Init(TWOPI/10.0f, -TWOPI/10.0f, DOOR_AXIS_NEG_X, DOOR_AXIS_Y, DOOR_EXTRA_NONE);
}

void
CBoat::SetModelIndex(uint32 id)
{
	CVehicle::SetModelIndex(id);
	SetupModelNodes();
}

void
CBoat::GetComponentWorldPosition(int32 component, CVector &pos)
{
	pos = *RwMatrixGetPos(RwFrameGetLTM(m_aBoatNodes[component]));
}

void
CBoat::ProcessControl(void)
{
	bool onLand = m_fDamageImpulse > 0.0f && m_vecDamageNormal.z > 0.1f;

	PruneWakeTrail();
	ProcessDelayedExplosion();

	// A burnt boat loses buoyancy until it has burnt down to the buoyancy the
	// scorched hull can still carry.
	float massCheck = m_fMass*0.008f*100.0f/125.0f;
	if(bRenderScorched && massCheck < m_fBuoyancyConstant)
		m_fBuoyancyConstant -= m_fMass*0.001f*0.008f;

#ifdef FIX_BUGS
	if(FindPlayerPed() && FindPlayerPed()->m_pWanted->GetWantedLevel() > 0 && GetModelIndex() == MI_PREDATOR && pDriver && IsPolicePedModel(pDriver->GetModelIndex())) {
#else
	if(FindPlayerPed()->m_pWanted->GetWantedLevel() > 0 && GetModelIndex() == MI_PREDATOR){
#endif
		CVehicle *playerVeh = FindPlayerVehicle();
		if(playerVeh && playerVeh->GetVehicleAppearance() == VEHICLE_APPEARANCE_BOAT &&
		   (AutoPilot.m_nCarMission == MISSION_RAMPLAYER_FARAWAY || 
		    AutoPilot.m_nCarMission == MISSION_RAMPLAYER_CLOSE || 
		    AutoPilot.m_nCarMission == MISSION_BLOCKPLAYER_FARAWAY || 
		    AutoPilot.m_nCarMission == MISSION_BLOCKPLAYER_CLOSE || 
		    AutoPilot.m_nCarMission == MISSION_ATTACKPLAYER) &&
		   CTimer::GetTimeInMilliseconds() > m_nPoliceShoutTimer){
			DMAudio.PlayOneShot(m_audioEntityId, SOUND_PED_VCPA_PLAYER_FOUND, 0.0f);
			m_nPoliceShoutTimer = CTimer::GetTimeInMilliseconds() + 4500 + (CGeneral::GetRandomNumber()&0xFFF);
		}
	}

	int r, g, b;
	RwRGBA dropColor = { 0, 0, 0, 0 };
	RwRGBA splashColor, jetColor;
	r = 127.5f*(CTimeCycle::GetAmbientRed_Obj() + 0.5f*CTimeCycle::GetDirectionalRed());
	g = 127.5f*(CTimeCycle::GetAmbientGreen_Obj() + 0.5f*CTimeCycle::GetDirectionalGreen());
	b = 127.5f*(CTimeCycle::GetAmbientBlue_Obj() + 0.5f*CTimeCycle::GetDirectionalBlue());
	r = Clamp(r, 0, 255);
	g = Clamp(g, 0, 255);
	b = Clamp(b, 0, 255);
	splashColor.red = r;
	splashColor.green = g;
	splashColor.blue = b;
	splashColor.alpha = CGeneral::GetRandomNumberInRange(160, 196);

	r = 229.5f*(CTimeCycle::GetAmbientRed() + 0.85f*CTimeCycle::GetDirectionalRed());
	g = 229.5f*(CTimeCycle::GetAmbientGreen() + 0.85f*CTimeCycle::GetDirectionalGreen());
	b = 229.5f*(CTimeCycle::GetAmbientBlue() + 0.85f*CTimeCycle::GetDirectionalBlue());
	r = Clamp(r, 0, 255);
	g = Clamp(g, 0, 255);
	b = Clamp(b, 0, 255);
	jetColor.red = r;
	jetColor.green = g;
	jetColor.blue = b;
	jetColor.alpha = CGeneral::GetRandomNumberInRange(196, 228);

	CGeneral::GetRandomNumber();	// unused

	UpdateClumpAlpha();
	ProcessCarAlarm();

	switch(GetStatus()){
	case STATUS_PLAYER:
		m_bIsAnchored = false;
		m_fOrientation = INVALID_ORIENTATION;
		ProcessControlInputs(0);
		if(GetModelIndex() == MI_PREDATOR)
			DoFixedMachineGuns();

		if (!CRecordDataForChase::IsRecording())
			DoDriveByShootings();
		break;
	case STATUS_SIMPLE:
		m_bIsAnchored = false;
		m_fOrientation = INVALID_ORIENTATION;
		CCarAI::UpdateCarAI(this);
		CPhysical::ProcessControl();
		bBoatInWater = true;
		bPropellerInWater = true;
		bIsInWater = true;
		return;
	case STATUS_PHYSICS:
		m_bIsAnchored = false;
		m_fOrientation = INVALID_ORIENTATION;
		CCarAI::UpdateCarAI(this);
		CCarCtrl::SteerAICarWithPhysics(this);
		break;
	case STATUS_ABANDONED:
	case STATUS_WRECKED:
		bBoatInWater = true;
		bPropellerInWater = true;
		bIsInWater = true;
		m_fSteerAngle = 0.0;
		bIsHandbrakeOn = false;
		m_fBrakePedal = 0.5f;
		m_fGasPedal = 0.0f;
		if((GetPosition() - FindPlayerCentreOfWorld_NoSniperShift()).Magnitude() > 150.0f){
			m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f);
			m_vecTurnSpeed = CVector(0.0f, 0.0f, 0.0f);
			return;
		}
		break;
	default: break;
	}

	// SA CBoat::ProcessControl (0x6F1D2C): drive the propeller speed towards the
	// engine band. The Skimmer idles at the standard speed; a boat under throttle
	// closes the gap five times faster, so the propeller spins up sharply and
	// coasts down slowly, which is what the eye expects.
	if(GetStatus() == STATUS_PLAYER || GetStatus() == STATUS_PLAYER_REMOTE || GetStatus() == STATUS_PHYSICS){
		float fSTDPropSpeed = 0.0f;
		float fROCPropSpeed = CPlane::PLANE_ROC_PROP_SPEED;
		if(GetModelIndex() == MI_SKIMMER){
			fSTDPropSpeed = CPlane::PLANE_STD_PROP_SPEED;
		}else if(m_fGasPedal != 0.0f){
			fROCPropSpeed *= 5.0f;
		}

		if(m_fGasPedal == 0.0f){
			m_EngineSpeed += (fSTDPropSpeed - m_EngineSpeed) * CTimer::GetTimeStep() * fROCPropSpeed;
		}else if(m_fGasPedal < 0.0f){
			fSTDPropSpeed = (CPlane::PLANE_STD_PROP_SPEED - 0.05f) * m_fGasPedal + CPlane::PLANE_STD_PROP_SPEED;
			m_EngineSpeed += (fSTDPropSpeed - m_EngineSpeed) * CTimer::GetTimeStep() * fROCPropSpeed;
		}else{
			fSTDPropSpeed = (CPlane::PLANE_MAX_PROP_SPEED - CPlane::PLANE_STD_PROP_SPEED) * m_fGasPedal + CPlane::PLANE_STD_PROP_SPEED;
			m_EngineSpeed += (fSTDPropSpeed - m_EngineSpeed) * CTimer::GetTimeStep() * fROCPropSpeed;
		}
	}else if(m_EngineSpeed > 0.0f){
		m_EngineSpeed *= 0.95f;
	}

	float collisionDamage = m_pHandlingData->m_fCollisionDamageMultiplier * m_fDamageImpulse;
#ifdef FIX_BUGS
	if(GetStatus() == STATUS_PLAYER && CStats::GetPercentageProgress() >= 100.0f)
		collisionDamage *= 0.5f;
	if (collisionDamage > 25.0f && GetStatus() != STATUS_WRECKED && !bCollisionProof) {
#else
	if(collisionDamage > 25.0f && GetStatus() != STATUS_WRECKED){
#endif
		float prevHealth = m_fHealth;
		if(prevHealth >= 250.0f){
#ifndef FIX_BUGS
			// if collisionDamage < 50 we actually increase health here...
			if(GetStatus() == STATUS_PLAYER && CStats::GetPercentageProgress() >= 100.0f)
				collisionDamage *= 0.5f;
#endif
			if(this == FindPlayerVehicle()){
				if(bTakeLessDamage)
					m_fHealth -= (collisionDamage-25.0f)/6.0f;
				else
					m_fHealth -= (collisionDamage-25.0f)/2.0f;
			}else{
				if(collisionDamage > 60.0f && pDriver)
					pDriver->Say(SOUND_PED_ANNOYED_DRIVER);
				if(bTakeLessDamage)
					m_fHealth -= (collisionDamage-25.0f)/12.0f;
				else
					m_fHealth -= (collisionDamage-25.0f)/4.0f;
			}

			if(m_fHealth <= 0.0f && prevHealth > 0.0f){
				m_fHealth = 1.0f;
				m_pSetOnFireEntity = m_pDamageEntity;
			}
		}
	}

	// Damage particles
	if(m_fHealth <= 460.0f && GetStatus() != STATUS_WRECKED &&
	   Abs(GetPosition().x - TheCamera.GetPosition().x) < 200.0f &&
	   Abs(GetPosition().y - TheCamera.GetPosition().y) < 200.0f){
		float speedSq = m_vecMoveSpeed.MagnitudeSqr();
		CVector smokeDir = 0.8f*m_vecMoveSpeed;
		CVector smokePos;
		switch(GetModelIndex()){
		case MI_SPEEDER:
			smokePos = CVector(0.4f, -2.4f, 0.8f);
			smokeDir += 0.05f*GetRight();
			smokeDir.z += 0.2f*m_vecMoveSpeed.z;
			break;
		case MI_REEFER:
			smokePos = CVector(2.0f, -1.0f, 0.5f);
			smokeDir += 0.07f*GetRight();
			break;
		case MI_PREDATOR:
		default:
			smokePos = CVector(-1.5f, -0.5f, 1.2f);
			smokeDir += -0.08f*GetRight();
			break;
		}

		smokePos = GetMatrix() * smokePos;

		// On fire
		if(m_fHealth < 250.0f){
			CParticle::AddParticle(PARTICLE_CARFLAME, smokePos,
				CVector(0.0f, 0.0f, CGeneral::GetRandomNumberInRange(2.25f/200.0f, 0.09f)),
				nil, 0.9f);
			CVector smokePos2 = smokePos;
			smokePos2.x += CGeneral::GetRandomNumberInRange(-2.25f/4.0f, 2.25f/4.0f);
			smokePos2.y += CGeneral::GetRandomNumberInRange(-2.25f/4.0f, 2.25f/4.0f);
			smokePos2.z += CGeneral::GetRandomNumberInRange(2.25f/4.0f, 2.25f);
			CParticle::AddParticle(PARTICLE_ENGINE_SMOKE2, smokePos2, CVector(0.0f, 0.0f, 0.0f));

			m_fDamage += CTimer::GetTimeStepInMilliseconds();
			if(m_fDamage > 5000.0f)
				BlowUpCar(m_pSetOnFireEntity);
		}

		if(speedSq < 0.25f && (CTimer::GetFrameCounter() + m_randomSeed) & 1)
			CParticle::AddParticle(PARTICLE_ENGINE_STEAM, smokePos, smokeDir);
		if(speedSq < 0.25f && m_fHealth <= 390.0f)
			CParticle::AddParticle(PARTICLE_ENGINE_SMOKE, smokePos, 1.25f*smokeDir);
	}

	bool bSeparateTurnForce = bHasHitWall;
	CPhysical::ProcessControl();

	CVector buoyanceImpulse(0.0f, 0.0f, 0.0f);
	CVector buoyancePoint(0.0f, 0.0f, 0.0f);
	if(mod_Buoyancy.ProcessBuoyancyBoat(this, m_fBuoyancyConstant, &buoyancePoint, &buoyanceImpulse, bSeparateTurnForce)){
		// Process boat in water
		if(0.1f * m_fMass * GRAVITY*CTimer::GetTimeStep() < buoyanceImpulse.z){
			bBoatInWater = true;
			bIsInWater = true;
			if (GetUp().z < -0.6f && Abs(GetMoveSpeed().x) < 0.05 && Abs(GetMoveSpeed().y) < 0.05) {
				bIsDrowning = true;
				if (pDriver){
					pDriver->bTouchingWater = true;
					pDriver->InflictDamage(nil, WEAPONTYPE_DROWNING, CTimer::GetTimeStep(), PEDPIECE_TORSO, 0);
				}
			}
			else
				bIsDrowning = false;
		}else{
			bBoatInWater = false;
			bIsInWater = false;
			bIsDrowning = false;
		}

		m_fVolumeUnderWater = mod_Buoyancy.m_volumeUnderWater;
		m_vecBuoyancePoint = buoyancePoint;
		if(GetModelIndex() == MI_SKIMMER && GetUp().z < -0.5f && Abs(m_vecMoveSpeed.x) < 0.2f && Abs(m_vecMoveSpeed.y) < 0.2f)
			ApplyMoveForce(0.03f*buoyanceImpulse);
		else
			ApplyMoveForce(buoyanceImpulse);
		if(bSeparateTurnForce)
			ApplyTurnForce(0.4f*buoyanceImpulse, buoyancePoint);

		// TODO: what is this?
		if(GetModelIndex() == MI_SKIMMER)
			if(m_skimmerThingTimer != 0.0f ||
			   GetForward().z < -0.5f && GetUp().z > -0.5f && m_vecMoveSpeed.z < -0.15f &&
			   buoyanceImpulse.z > 0.01f*m_fMass * GRAVITY*CTimer::GetTimeStep() &&
			   buoyanceImpulse.z < 0.4f*m_fMass * GRAVITY*CTimer::GetTimeStep()){
				float turnImpulse = -0.00017f*GetForward().z*buoyanceImpulse.z * m_fMass*CTimer::GetTimeStep();
				ApplyTurnForce(turnImpulse*GetForward(), GetUp());
				bBoatInWater = false;
				//BUG? aren't we forgetting the timestep here?
				float moveImpulse = -0.5f*DotProduct(m_vecMoveSpeed, GetForward()) * m_fMass;
				ApplyMoveForce(moveImpulse*GetForward());
				if(m_skimmerThingTimer == 0.0f)
					m_skimmerThingTimer = CTimer::GetTimeInMilliseconds() + 300.0f;
				else if(m_skimmerThingTimer < CTimer::GetTimeInMilliseconds())
					m_skimmerThingTimer = 0.0f;
			}

		if(!onLand && bBoatInWater && GetUp().z > 0.0f){
			float impulse = m_vecMoveSpeed.MagnitudeSqr()*m_pBoatHandlingData->m_fAqPlaneForce*buoyanceImpulse.z*CTimer::GetTimeStep()*0.5f;
			if(GetModelIndex() == MI_SKIMMER)
				impulse *= 1.0f + m_fGasPedal;
			else if(m_fGasPedal > 0.05f)
				impulse *= m_fGasPedal;
			else
				impulse = 0.0f;
			impulse = Min(impulse, GRAVITY*m_pBoatHandlingData->m_fAqPlaneLimit*m_fMass*CTimer::GetTimeStep());
			ApplyMoveForce(impulse*GetUp());
			ApplyTurnForce(impulse*GetUp(), buoyancePoint - m_pBoatHandlingData->m_fAqPlaneOffset*GetForward());
		}

		// Handle boat moving forward
		float fwdSpeed = 1.0f;
		if(Abs(m_fGasPedal) > 0.05f || (fwdSpeed = m_vecMoveSpeed.Magnitude2D()) > 0.01f){
			if(bBoatInWater && fwdSpeed > 0.05f)
				AddWakePoint(GetPosition());

			float steerFactor = 1.0f;
			if(GetStatus() == STATUS_PLAYER){
				float steerLoss = DotProduct(m_vecMoveSpeed, GetForward())*m_pHandlingData->m_fTractionBias;
				if(CPad::GetPad(0)->GetHandBrake())
					steerLoss *= 0.5f;
				steerFactor -= steerLoss;
				steerFactor = Clamp(steerFactor, 0.0f, 1.0f);
			}

			CVector boundMin = GetColModel()->boundingBox.min;
			CVector propeller(0.0f, boundMin.y*m_pBoatHandlingData->m_fThrustY, boundMin.z*m_pBoatHandlingData->m_fThrustZ);
			propeller = Multiply3x3(GetMatrix(), propeller);
			CVector propellerWorld = GetPosition() + propeller;

			float steerSin = Sin(-m_fSteerAngle * steerFactor);
			float steerCos = Cos(-m_fSteerAngle * steerFactor);
			float waterLevel;
			CWaterLevel::GetWaterLevel(propellerWorld, &waterLevel, true);
			if(propellerWorld.z-0.5f < waterLevel){
				float propellerDepth = waterLevel - (propellerWorld.z - 0.5f);
				if(propellerDepth > 1.0f)
					propellerDepth = 1.0f;
				else
					propellerDepth = SQR(propellerDepth);
				bPropellerInWater = true;

				bool bSlowAhead = false;
				if(Abs(m_fGasPedal) > 0.01f && GetModelIndex() != MI_SKIMMER){
					if(Abs(m_fGasPedal) < 0.05f)
						bSlowAhead = true;

					CVector forceDir = Multiply3x3(GetMatrix(), CVector(-steerSin, steerCos, -Abs(m_fSteerAngle)));
					CVector force = propellerDepth * m_fGasPedal * 40.0f * m_pHandlingData->m_Transmission.m_fEngineAcceleration * m_pHandlingData->m_fMass * forceDir;
					if(force.z > 0.2f)
						force.z = SQR(1.2f - force.z) + 0.2f;
					if(onLand){
						if(m_fGasPedal < 0.0f){
							force.x *= 5.0f;
							force.y *= 5.0f;
						}
						if(force.z < 0.0f)
							force.z = 0.0f;
						ApplyMoveForce(force * CTimer::GetTimeStep());
					}else{
						ApplyMoveForce(force * CTimer::GetTimeStep());
						ApplyTurnForce(force * CTimer::GetTimeStep(), propeller - m_pBoatHandlingData->m_fThrustAppZ*GetUp());
						float rightForce = -DotProduct(GetRight(), force)*m_pHandlingData->m_fTractionMultiplier;
						ApplyTurnForce(rightForce*GetRight() * CTimer::GetTimeStep(), GetUp());
					}

					// Spray some particles
					CVector jetDir = -0.04f * force;
					if(m_fGasPedal > 0.0f){
						if(GetStatus() == STATUS_PLAYER){
							CVector sternPos = GetColModel()->boundingBox.min;
							sternPos.x = 0.0f;
							sternPos.z = 0.0f;
							sternPos = Multiply3x3(GetMatrix(), sternPos);

							CVector wakePos = GetPosition() + sternPos;
							// no actual particles for player...
						}else if(IsVisible() && ((CTimer::GetFrameCounter() + m_randomSeed) & 1) &&
						         CVisibilityPlugins::GetDistanceSquaredFromCamera(&propellerWorld) < SQR(70.0f * TheCamera.GenerationDistMultiplier)){
							jetDir.z = 0.015f;
							jetDir.x *= 3.5f;
							jetDir.y *= 3.5f;
							propellerWorld.z += 0.5f;
							
							CParticle::AddParticle(PARTICLE_BOAT_SPLASH, propellerWorld, jetDir, nil, 1.25f, jetColor,
								CGeneral::GetRandomNumberInRange(0, 5),
								CGeneral::GetRandomNumberInRange(0, 90), 1, 500);
							
							CParticle::AddParticle(PARTICLE_CAR_SPLASH, propellerWorld, 0.75f * jetDir, nil, 0.5f, splashColor,
								CGeneral::GetRandomNumberInRange(0, 30),
								CGeneral::GetRandomNumberInRange(0, 45), 3, 500);
						}
					}
				}else
					bSlowAhead = true;

				if(!onLand && bSlowAhead){
					float force = m_pHandlingData->m_fTractionLoss*DotProduct(m_vecMoveSpeed, GetForward());
					force = Min(force, 0.01f*m_fTurnMass);
					if(m_fGasPedal > 0.01f){
						if(GetStatus() == STATUS_PLAYER)
							force *= (0.55f - Abs(m_fGasPedal)) * 1.3f;
						else
							force *= (0.55f - Abs(m_fGasPedal)) * 2.5f;
					}
					if(m_fGasPedal < 0.0f && force > 0.0f || m_fGasPedal > 0.0f && force < 0.0f)
						force *= -1.0f;
					CVector propellerForce = propellerDepth * Multiply3x3(GetMatrix(), force*CVector(-steerSin, 0.0f, 0.0f));
					ApplyMoveForce(propellerForce * CTimer::GetTimeStep());
					ApplyTurnForce(propellerForce * CTimer::GetTimeStep(), propeller);
					float rightForce = -steerSin * force * 0.75f/steerFactor * Max(CTimer::GetTimeStep(), 0.01f);
					ApplyTurnForce(GetRight() * rightForce, GetUp());
				}
			}else
				bPropellerInWater = false;

			if(m_pHandlingData->m_fSuspensionBiasBetweenFrontAndRear != 0.0f){
				CVector right = CrossProduct(GetForward(), CVector(0.0f, 0.0f, 1.0f));
				float rightSpeed = DotProduct(m_vecMoveSpeed, right);
				float impulse = 0.1f*m_pHandlingData->m_fSuspensionBiasBetweenFrontAndRear * m_fMass * m_fVolumeUnderWater * rightSpeed * CTimer::GetTimeStep();
				ApplyMoveForce(right - impulse * 0.3f * CVector(-right.y, right.x, 0.0f));
			}

			if(GetStatus() == STATUS_PLAYER && CPad::GetPad(0)->GetHandBrake()){
				float fwdSpeed = DotProduct(m_vecMoveSpeed, GetForward());
				if(fwdSpeed > 0.0f){
					float impulse = -0.1f*m_pHandlingData->m_fSuspensionLowerLimit * m_fMass * m_fVolumeUnderWater * fwdSpeed * CTimer::GetTimeStep();
					ApplyMoveForce(impulse * GetForward());
				}
			}
		}

		// The native sector grid is not a physical boundary for boats.

		if(!onLand && bBoatInWater && !bSeparateTurnForce)
			ApplyWaterResistance();

		if((GetModelIndex() != MI_SKIMMER || m_skimmerThingTimer == 0.0f) && !bSeparateTurnForce){
			// No idea what exactly is going on here besides drag in YZ
			float fx = Pow(m_pBoatHandlingData->m_vecTurnRes.x, CTimer::GetTimeStep());
			float fy = Pow(m_pBoatHandlingData->m_vecTurnRes.y, CTimer::GetTimeStep());
			float fz = Pow(m_pBoatHandlingData->m_vecTurnRes.z, CTimer::GetTimeStep());
			m_vecTurnSpeed = Multiply3x3(m_vecTurnSpeed, GetMatrix());	// invert - to local space
			// TODO: figure this out
			float magic = 1.0f/(1000.0f * SQR(m_vecTurnSpeed.x) + 1.0f) * fx;
			m_vecTurnSpeed.y *= fy;
			m_vecTurnSpeed.z *= fz;
			float forceUp = (magic - 1.0f) * m_vecTurnSpeed.x * m_fTurnMass;
			m_vecTurnSpeed = Multiply3x3(GetMatrix(), m_vecTurnSpeed);	// back to world
			CVector com = Multiply3x3(GetMatrix(), m_vecCentreOfMass);
			ApplyTurnForce(forceUp*GetUp(), com + GetForward());
		}

		m_nDeltaVolumeUnderWater = (m_fVolumeUnderWater-m_fPrevVolumeUnderWater)*10000;

		// Falling into water
		if(!onLand && bBoatInWater && GetUp().z > 0.0f){
			float splashVol = m_nDeltaVolumeUnderWater*m_pBoatHandlingData->m_fWaveAudioMult;
			if(splashVol > 200.0f)
				DMAudio.PlayOneShot(m_audioEntityId, SOUND_CAR_SPLASH, splashVol);

			if(m_nDeltaVolumeUnderWater > 200){
				float speedUp = m_vecMoveSpeed.MagnitudeSqr() * m_nDeltaVolumeUnderWater * 0.001f;
				if(speedUp + m_vecMoveSpeed.z > m_pHandlingData->m_fBrakeDeceleration)
					speedUp = m_pHandlingData->m_fBrakeDeceleration - m_vecMoveSpeed.z;
				if(speedUp < 0.0f) speedUp = 0.0f;
				float speedFwd = DotProduct(m_vecMoveSpeed, GetForward());
				speedFwd *= -m_nDeltaVolumeUnderWater * 0.01f * m_pHandlingData->m_fBrakeBias;
				CVector speed = speedFwd*GetForward() + CVector(0.0f, 0.0f, speedUp);
				CVector splashImpulse = speed * m_fMass;
				ApplyMoveForce(splashImpulse);
				ApplyTurnForce(splashImpulse, buoyancePoint);
			}
		}

		// Splashes
		float speed = m_vecMoveSpeed.Magnitude();
		if(speed > 0.05f && GetUp().x > 0.0f && !TheCamera.GetLookingForwardFirstPerson() && IsVisible() &&
		   (AutoPilot.m_nCarMission != MISSION_CRUISE || (CTimer::GetFrameCounter()&2) == 0)){
			CVector splashPos, splashDir;
			float splashSize, front, waterLevel;

			switch(GetModelIndex()){
			case MI_RIO:
				splashSize = speed;
				front = 0.9f * GetColModel()->boundingBox.max.y;
				splashDir = -0.5f * m_vecMoveSpeed;
				splashDir.z += 0.25f*speed;
				splashDir += 0.35f*speed*GetRight();
				splashPos = GetPosition() + 1.85f*GetRight() + front*GetForward();
				splashPos.z += 0.5f;
				break;
			case MI_SQUALO:
				splashSize = speed;
				front = 0.75f * GetColModel()->boundingBox.max.y;
				splashDir = -0.125f * m_vecMoveSpeed;
				splashDir.z += 0.15f*speed;
				splashDir += 0.25f*speed*GetRight();
				splashPos = GetPosition() + m_vecBuoyancePoint + 0.5f*GetRight() + front*GetForward();
				splashPos.z += 0.5f;
				break;
			case MI_REEFER:
				splashSize = speed;
				front = 0.75f * GetColModel()->boundingBox.max.y;
				splashDir = -0.5f * m_vecMoveSpeed;
				splashDir.z += 0.15f*speed;
				splashDir += 0.5f*speed*GetRight();
				splashPos = GetPosition() + m_vecBuoyancePoint + 1.3f*GetRight() + front*GetForward();
				break;
			case MI_COASTG:
				splashSize = 0.25f*speed;
				front = 0.8f * GetColModel()->boundingBox.max.y;
				splashDir = 0.165f * m_vecMoveSpeed;
				splashDir.z += 0.25f*speed;
				splashDir += 0.15f*speed*GetRight();
				splashPos = GetPosition() + 0.65f*GetRight() + front*GetForward();
				splashPos.z += 0.5f;
				break;
			case MI_DINGHY:
				splashSize = 0.25f*speed;
				front = 0.9f * GetColModel()->boundingBox.max.y;
				splashDir = 0.35f * m_vecMoveSpeed;
				splashDir.z += 0.25f*speed;
				splashDir += 0.25f*speed*GetRight();
				splashPos = GetPosition() + 0.6f*GetRight() + front*GetForward();
				splashPos.z += 0.5f;
				break;
			default:
				splashSize = speed;
				front = 0.9f * GetColModel()->boundingBox.max.y;
				splashDir = -0.5f * m_vecMoveSpeed;
				splashDir.z += 0.25f*speed;
				splashDir += 0.35f*speed*GetRight();
				splashPos = GetPosition() + m_vecBuoyancePoint + 0.5f*GetRight() + front*GetForward();
				break;
			}
			if(splashSize > 0.75f) splashSize = 0.75f;
			if(AutoPilot.m_nCarMission == MISSION_CRUISE)
				splashDir *= 1.5f;
			static float lifeMult = 1000.0f;
			static float lifeBase = 300.0f;
			splashDir.z += 0.0003f*m_nDeltaVolumeUnderWater;
			CWaterLevel::GetWaterLevel(splashPos, &waterLevel, true);
			if(splashPos.z-waterLevel < 3.0f &&
			   CVisibilityPlugins::GetDistanceSquaredFromCamera(&splashPos) < SQR(70.0f * TheCamera.GenerationDistMultiplier)){
				splashPos.z = waterLevel + 0.1f;
				CParticle::AddParticle(PARTICLE_CAR_SPLASH, splashPos, 0.75f*splashDir, nil, splashSize+0.1f,  splashColor,
					CGeneral::GetRandomNumberInRange(0.0f, 10.0f), CGeneral::GetRandomNumberInRange(0.0f, 90.0f),
					1, lifeBase + splashDir.z*lifeMult);
				CParticle::AddParticle(PARTICLE_BOAT_SPLASH, splashPos, splashDir, nil, splashSize,  jetColor,
					CGeneral::GetRandomNumberInRange(0.0f, 0.4f), CGeneral::GetRandomNumberInRange(0.0f, 45.0f),
					0, lifeBase + splashDir.z*lifeMult);
			}

			switch(GetModelIndex()){
			case MI_RIO:
				splashDir = -0.5f * m_vecMoveSpeed;
				splashDir.z += 0.25f*speed;
				splashDir -= 0.35f*speed*GetRight();
				splashPos = GetPosition() - 1.85f*GetRight() + front*GetForward();
				splashPos.z += 0.5f;
				break;
			case MI_SQUALO:
				splashDir = -0.125f * m_vecMoveSpeed;
				splashDir.z += 0.15f*speed;
				splashDir -= 0.25f*speed*GetRight();
				splashPos = GetPosition() + m_vecBuoyancePoint - 0.5f*GetRight() + front*GetForward();
				splashPos.z += 0.5f;
				break;
			case MI_REEFER:
				splashDir = -0.5f * m_vecMoveSpeed;
				splashDir.z += 0.15f*speed;
				splashDir -= 0.5f*speed*GetRight();
				splashPos = GetPosition() + m_vecBuoyancePoint - 1.3f*GetRight() + front*GetForward();
				break;
			case MI_COASTG:
				splashDir = 0.165f * m_vecMoveSpeed;
				splashDir.z += 0.25f*speed;
				splashDir -= 0.15f*speed*GetRight();
				splashPos = GetPosition() - 0.65f*GetRight() + front*GetForward();
				splashPos.z += 0.5f;
				break;
			case MI_DINGHY:
				splashDir = 0.35f * m_vecMoveSpeed;
				splashDir.z += 0.25f*speed;
				splashDir -= 0.25f*speed*GetRight();
				splashPos = GetPosition() - 0.6f*GetRight() + front*GetForward();
				splashPos.z += 0.5f;
				break;
			default:
				splashDir = -0.5f * m_vecMoveSpeed;
				splashDir.z += 0.25f*speed;
				splashDir -= 0.35f*speed*GetRight();
				splashPos = GetPosition() + m_vecBuoyancePoint - 0.5f*GetRight() + front*GetForward();
				break;
			}
			if(AutoPilot.m_nCarMission == MISSION_CRUISE)
				splashDir *= 1.5f;
			splashDir.z += 0.0003f*m_nDeltaVolumeUnderWater;
			CWaterLevel::GetWaterLevel(splashPos, &waterLevel, true);
			if(splashPos.z-waterLevel < 3.0f &&
			   CVisibilityPlugins::GetDistanceSquaredFromCamera(&splashPos) < SQR(70.0f * TheCamera.GenerationDistMultiplier)){
				splashPos.z = waterLevel + 0.1f;
				CParticle::AddParticle(PARTICLE_CAR_SPLASH, splashPos, 0.75f*splashDir, nil, splashSize+0.1f,  splashColor,
					CGeneral::GetRandomNumberInRange(0.0f, 10.0f), CGeneral::GetRandomNumberInRange(0.0f, 90.0f),
					1, lifeBase + splashDir.z*lifeMult);
				CParticle::AddParticle(PARTICLE_BOAT_SPLASH, splashPos, splashDir, nil, splashSize,  jetColor,
					CGeneral::GetRandomNumberInRange(0.0f, 0.4f), CGeneral::GetRandomNumberInRange(0.0f, 45.0f),
					0, lifeBase + splashDir.z*lifeMult);
			}
		}

		// Spray waterdrops on screen
		if(TheCamera.GetLookingForwardFirstPerson() && FindPlayerVehicle() && FindPlayerVehicle()->IsBoat() &&
		   m_nDeltaVolumeUnderWater > 0 && numWaterDropOnScreen < 20){
			CVector dropPos;
			CVector dropDir(CGeneral::GetRandomNumberInRange(-0.25f, 0.25f), CGeneral::GetRandomNumberInRange(1.0f, 0.75f), 0.0f);

			int frm = CGeneral::GetRandomNumber() & 1;
			if(TheCamera.m_CameraAverageSpeed < 0.35f){
				dropPos.x = CGeneral::GetRandomNumberInRange(50, (int)SCREEN_WIDTH-50);
				dropPos.y = CGeneral::GetRandomNumberInRange(50, (int)SCREEN_HEIGHT-50);
			}else{
				dropPos.x = CGeneral::GetRandomNumberInRange(200, (int)SCREEN_WIDTH-200);
				dropPos.y = CGeneral::GetRandomNumberInRange(150, (int)SCREEN_HEIGHT-150);
			}
			dropPos.z = 1.0f;

			if(TheCamera.m_CameraAverageSpeed > 0.35f){
				if((int)SCREEN_WIDTH / 2 < dropPos.x)
					dropPos.x += CGeneral::GetRandomNumberInRange(0.35f, TheCamera.m_CameraAverageSpeed)*7.5f;
				else
					dropPos.x -= CGeneral::GetRandomNumberInRange(0.35f, TheCamera.m_CameraAverageSpeed)*7.5f;

				if((int)SCREEN_HEIGHT / 2 < dropPos.y)
					dropPos.y += CGeneral::GetRandomNumberInRange(0.35f, TheCamera.m_CameraAverageSpeed)*7.5f;
				else
					dropPos.y -= CGeneral::GetRandomNumberInRange(0.35f, TheCamera.m_CameraAverageSpeed)*7.5f;
			}

			if(CParticle::AddParticle(PARTICLE_WATERDROP, dropPos, dropDir, nil,
					CGeneral::GetRandomNumberInRange(0.1f, 0.15f), dropColor, 0, 0, frm))
				numWaterDropOnScreen++;
		}

		if(m_fPrevVolumeUnderWater == 0.0f && m_fVolumeUnderWater > 0.0f && GetModelIndex() == MI_SKIMMER){
			CVector splashDir(0.0f, 0.0f, 0.25f*speed);
			CVector splashPos = GetPosition();
			float level;
			CWaterLevel::GetWaterLevel(splashPos, &level, true);
			splashPos.z = level;
			CParticleObject::AddObject(POBJECT_CAR_WATER_SPLASH, splashPos, splashDir, 0.0f, 65, splashColor, true);
		}

		m_fPrevVolumeUnderWater = m_fVolumeUnderWater;
	}else{
		bBoatInWater = false;
		bIsInWater = false;
#ifdef FIX_BUGS
		bIsDrowning = false;
#endif
	}

	if(m_bIsAnchored){
		m_vecMoveSpeed.x = 0.0f;
		m_vecMoveSpeed.y = 0.0f;

		if(m_fOrientation == INVALID_ORIENTATION){
			m_fOrientation = GetForward().Heading();
		}else{
			// is this some inlined CPlaceable method?
			CVector pos = GetPosition();
			GetMatrix().RotateZ(m_fOrientation - GetForward().Heading());
			GetMatrix().SetTranslateOnly(pos);
		}
	}
}

void
CBoat::ProcessControlInputs(uint8 pad)
{
	m_nPadID = Min(pad, (uint8)3);
	CPad *padState = CPad::GetPad(pad);

	m_fBrakePedal = Clamp(m_fBrakePedal + (padState->GetBrake()/255.0f - m_fBrakePedal)*0.1f, 0.0f, 1.0f);
	if(m_fBrakePedal >= 0.05f){
		m_fGasPedal = m_fBrakePedal * -0.3f;
	}else{
		m_fBrakePedal = 0.0f;
		m_fGasPedal = padState->GetAccelerate()/255.0f;
	}

	// The reference also suppresses the mouse rudder in its mouse look mode;
	// this port has no such mode, so the mouse always steers here.
	int16 steerInput = padState->GetSteeringLeftRight();
	if(CCamera::m_bUseMouse3rdPerson && !CVehicle::m_bDisableMouseSteering){
		if(CPad::NewMouseControllerState.x == 0.0f && m_fRawSteerAngle == 0.0f ||
		   CVehicle::m_nLastControlInput != MOUSE || steerInput != 0){
			m_fRawSteerAngle += (-steerInput/128.0f - m_fRawSteerAngle) * BOAT_STEER_SMOOTH_RATE * CTimer::GetTimeStep();
			CVehicle::m_nLastControlInput = KEYBOARD;
		}else{
			m_fRawSteerAngle += CPad::NewMouseControllerState.x * BOAT_MOUSE_STEER_SCALE;
			CVehicle::m_nLastControlInput = MOUSE;

			// the rudder is damped close to the centre line
			if(Abs(m_fRawSteerAngle) < BOAT_STEER_DAMP_THRESHOLD)
				m_fRawSteerAngle *= Pow(BOAT_STEER_DAMP_BASE, CTimer::GetTimeStep());
		}
	}else{
		m_fRawSteerAngle += (-steerInput/128.0f - m_fRawSteerAngle) * BOAT_STEER_SMOOTH_RATE * CTimer::GetTimeStep();
		CVehicle::m_nLastControlInput = KEYBOARD;
	}

	m_fRawSteerAngle = Clamp(m_fRawSteerAngle, -1.0f, 1.0f);
	float signedSquare = m_fRawSteerAngle < 0.0f ? -SQR(m_fRawSteerAngle) : SQR(m_fRawSteerAngle);
	m_fSteerAngle = m_pHandlingData->m_fSteeringLock * DEGTORAD(signedSquare);
}

float fSeaPlaneWaterResistance = 30.0f;

void
CBoat::ApplyWaterResistance(void)
{
	// TODO: figure out how this works
	float resistance = 0.001f * m_pHandlingData->m_fSuspensionForceLevel * SQR(m_fVolumeUnderWater) * m_fMass;
	if(GetModelIndex() == MI_SKIMMER)
		resistance *= fSeaPlaneWaterResistance;
	float fwdSpeed = DotProduct(GetMoveSpeed(), GetForward());
	float magic = (SQR(fwdSpeed) + 0.05f) * resistance + 1.0f;
	magic = Abs(magic);
	float fx = Pow(m_pBoatHandlingData->m_vecMoveRes.x/magic, 0.5f*CTimer::GetTimeStep());
	float fy = Pow(m_pBoatHandlingData->m_vecMoveRes.y/magic, 0.5f*CTimer::GetTimeStep());
	float fz = Pow(m_pBoatHandlingData->m_vecMoveRes.z/magic, 0.5f*CTimer::GetTimeStep());

	m_vecMoveSpeed = Multiply3x3(m_vecMoveSpeed, GetMatrix());	// invert - to local space
	m_vecMoveSpeed.x *= fx;
	m_vecMoveSpeed.y *= fy;
	m_vecMoveSpeed.z *= fz;
	float force = (fy - 1.0f) * m_vecMoveSpeed.y * m_fMass;
	m_vecMoveSpeed = Multiply3x3(GetMatrix(), m_vecMoveSpeed);	// back to world

	ApplyTurnForce(force*GetForward(), -GetUp());

	if(m_vecMoveSpeed.z > 0.0f)
		m_vecMoveSpeed.z *= fz;
	else
		m_vecMoveSpeed.z *= (1.0f - fz)*0.5f + fz;
}

RwObject*
GetBoatAtomicObjectCB(RwObject *object, void *data)
{
	RpAtomic *atomic = (RpAtomic*)object;
	assert(RwObjectGetType(object) == rpATOMIC);
	if(RpAtomicGetFlags(atomic) & rpATOMICRENDER)
		*(RpAtomic**)data = atomic;
	return object;

	
}

void
CBoat::BlowUpCar(CEntity *culprit)
{
	RpAtomic *atomic;
	RwFrame *frame;
	RwMatrix *matrix;
	CObject *obj;

	if(!bCanBeDamaged)
		return;

	// explosion pushes vehicle up
	m_vecMoveSpeed.z += 0.13f;
	SetStatus(STATUS_WRECKED);
	bRenderScorched = true;

	m_fHealth = 0.0;
	m_nBombTimer = 0;
	TheCamera.CamShake(0.7f, GetPosition().x, GetPosition().y, GetPosition().z);

	KillPedsInVehicle();

	bEngineOn = false;
	bLightsOn = false;
	ChangeLawEnforcerState(false);

	CExplosion::AddExplosion(this, culprit, EXPLOSION_BOAT, GetPosition(), 0);
	CDarkel::RegisterCarBlownUpByPlayer(this);
	if(m_aBoatNodes[BOAT_MOVING] == nil)
		return;

	// much like CAutomobile::SpawnFlyingComponent from here on

	atomic = nil;
	RwFrameForAllObjects(m_aBoatNodes[BOAT_MOVING], GetBoatAtomicObjectCB, &atomic);
	if(atomic == nil)
		return;

	obj = new CObject();
	if(obj == nil)
		return;

	obj->SetModelIndexNoCreate(MI_CAR_WHEEL);

	// object needs base model
	obj->RefModelInfo(GetModelIndex());

	// create new atomic
	matrix = RwFrameGetLTM(m_aBoatNodes[BOAT_MOVING]);
	frame = RwFrameCreate();
	atomic = RpAtomicClone(atomic);
	*RwFrameGetMatrix(frame) = *matrix;
	RpAtomicSetFrame(atomic, frame);
	CVisibilityPlugins::SetAtomicRenderCallback(atomic, nil);
	obj->AttachToRwObject((RwObject*)atomic);
	obj->bDontStream = true;

	// init object
	obj->m_fMass = 10.0f;
	obj->m_fTurnMass = 25.0f;
	obj->m_fAirResistance = 0.99f;
	obj->m_fElasticity = 0.1f;
	obj->m_fBuoyancyConstant = obj->m_fMass*GRAVITY/0.75f;
	obj->ObjectCreatedBy = TEMP_OBJECT;
	obj->SetIsStatic(false);
	obj->bIsPickup = false;

	// life time
	CObject::nNoTempObjects++;
	obj->m_nEndOfLifeTime = CTimer::GetTimeInMilliseconds() + 20000;

	obj->m_vecMoveSpeed = m_vecMoveSpeed;
	if(GetUp().z > 0.0f)
		obj->m_vecMoveSpeed.z = 0.3f;
	else
		obj->m_vecMoveSpeed.z = 0.0f;

	obj->m_vecTurnSpeed = m_vecTurnSpeed*2.0f;
	obj->m_vecTurnSpeed.x = 0.5f;

	// push component away from boat
	CVector dist = obj->GetPosition() - GetPosition();
	dist.Normalise();
	if(GetUp().z > 0.0f)
		dist += GetUp();
	obj->GetMatrix().GetPosition() += dist;

	CWorld::Add(obj);

	atomic = nil;
	RwFrameForAllObjects(m_aBoatNodes[BOAT_MOVING], GetBoatAtomicObjectCB, &atomic);
	if(atomic)
		RpAtomicSetFlags(atomic, 0);
}

// SA CBoat::ProcessBoatNodeRendering (inlined at 0x6F1180). Turns one propeller
// node about its Y axis and sets the blade texture alpha, so a slow propeller is
// drawn as distinct blades and a fast one blurs into a disc.
void
CBoat::ProcessBoatNodeRendering(eBoatNodes node, float rotation, uint8 alpha)
{
	RwFrame *frame = m_aBoatNodes[node];
	if(frame == nil)
		return;

	CMatrix mat(RwFrameGetMatrix(frame));
	CVector pos = mat.GetPosition();
	mat.SetRotateY(rotation);
	mat.Translate(pos);
	mat.UpdateRW();

	RpAtomic *atomic = nil;
	RwFrameForAllObjects(frame, GetBoatAtomicObjectCB, &atomic);
	if(atomic)
		SetComponentAtomicAlpha(atomic, alpha);
}

void
CBoat::PreRender(void)
{
	CMatrix matrix;
	CVector pos;
	RpAtomic *atomic;

	if(GetModelIndex() == MI_SKIMMER){
		m_fMovingRotation += m_fMovingSpeed*CTimer::GetTimeStep();
		if(m_fMovingRotation > TWOPI) m_fMovingRotation -= TWOPI;
		int alpha = (1.0f - Min(2.0f*m_fMovingSpeed*8.0f/PI, 1.0f))*255.0f;
		if(GetStatus() == STATUS_PLAYER || GetStatus() == STATUS_PLAYER_REMOTE || GetStatus() == STATUS_PLAYER_PLAYBACKFROMBUFFER){
			if(m_aBoatNodes[BOAT_RUDDER]){
				float sine = Sin(m_fSteerAngle);
				matrix.Attach(RwFrameGetMatrix(m_aBoatNodes[BOAT_RUDDER]));
				pos = matrix.GetPosition();
				matrix.SetRotate(0.0f, 0.0f, -m_fSteerAngle);
				matrix.Rotate(0.0f, DEGTORAD(22.0f)*sine, 0.0f);
				matrix.Translate(pos);
				matrix.UpdateRW();
			}
			if(m_aBoatNodes[BOAT_FLAP_LEFT]){
				matrix.Attach(RwFrameGetMatrix(m_aBoatNodes[BOAT_FLAP_LEFT]));
				pos = matrix.GetPosition();
				matrix.SetRotateX(-m_fSteerAngle);
				matrix.Translate(pos);
				matrix.UpdateRW();
			}
			if(m_aBoatNodes[BOAT_FLAP_RIGHT]){
				matrix.Attach(RwFrameGetMatrix(m_aBoatNodes[BOAT_FLAP_RIGHT]));
				pos = matrix.GetPosition();
				matrix.SetRotateX(m_fSteerAngle);
				matrix.Translate(pos);
				matrix.UpdateRW();
			}
			// FIX: Planes can also be controlled with GetCarGunUpDown
#ifdef FIX_BUGS
			static float steeringUpDown = 0.0f;
			if(!CCamera::bFreeCam || (CCamera::bFreeCam && !CPad::IsAffectedByController))
				steeringUpDown += ((Abs(CPad::GetPad(0)->GetCarGunUpDown()) > 1.0f ? (-CPad::GetPad(0)->GetCarGunUpDown()/128.0f) : (-CPad::GetPad(0)->GetSteeringUpDown()/128.0f)) - steeringUpDown) * Min(1.f, CTimer::GetTimeStep()/5.f);
			else
				steeringUpDown = -CPad::GetPad(0)->GetSteeringUpDown()/128.0f;
#else
			float steeringUpDown = -CPad::GetPad(0)->GetSteeringUpDown()/128.0f;
#endif
			if(m_aBoatNodes[BOAT_REARFLAP_LEFT]){
				matrix.Attach(RwFrameGetMatrix(m_aBoatNodes[BOAT_REARFLAP_LEFT]));
				pos = matrix.GetPosition();
				matrix.SetRotateX(steeringUpDown);
				matrix.Translate(pos);
				matrix.UpdateRW();
			}
			if(m_aBoatNodes[BOAT_REARFLAP_RIGHT]){
				matrix.Attach(RwFrameGetMatrix(m_aBoatNodes[BOAT_REARFLAP_RIGHT]));
				pos = matrix.GetPosition();
				matrix.SetRotateX(steeringUpDown);
				matrix.Translate(pos);
				matrix.UpdateRW();
			}
		}
		if(m_aBoatNodes[BOAT_MOVING]){
			matrix.Attach(RwFrameGetMatrix(m_aBoatNodes[BOAT_MOVING]));
			pos = matrix.GetPosition();
			matrix.SetRotateY(m_fMovingRotation);
			matrix.Translate(pos);
			matrix.UpdateRW();

			atomic = nil;
			RwFrameForAllObjects(m_aBoatNodes[BOAT_MOVING], GetBoatAtomicObjectCB, &atomic);
			if(atomic)
				SetComponentAtomicAlpha(atomic, alpha);
		}
		if(m_aBoatNodes[BOAT_WINDSCREEN]){
			matrix.Attach(RwFrameGetMatrix(m_aBoatNodes[BOAT_WINDSCREEN]));
			pos = matrix.GetPosition();
			matrix.SetRotateY(-m_fMovingRotation);
			matrix.Translate(pos);
			matrix.UpdateRW();

			atomic = nil;
			RwFrameForAllObjects(m_aBoatNodes[BOAT_WINDSCREEN], GetBoatAtomicObjectCB, &atomic);
			if(atomic)
				SetComponentAtomicAlpha(atomic, Max(150-alpha, 0));
		}
		CShadows::StoreShadowForVehicle(this, VEH_SHD_TYPE_SEAPLANE);
	}else if(GetModelIndex() == MI_COASTG || GetModelIndex() == MI_DINGHY || GetModelIndex() == MI_RIO ||
	         GetModelIndex() == MI_SQUALO || GetModelIndex() == MI_MARQUIS){
		if(m_aBoatNodes[BOAT_RUDDER]){
			matrix.Attach(RwFrameGetMatrix(m_aBoatNodes[BOAT_RUDDER]));
			pos = matrix.GetPosition();
			matrix.SetRotateZ(-m_fSteerAngle);
			matrix.Translate(pos);
			matrix.UpdateRW();
		}
		if(m_aBoatNodes[BOAT_REARFLAP_LEFT]){
			matrix.Attach(RwFrameGetMatrix(m_aBoatNodes[BOAT_REARFLAP_LEFT]));
			pos = matrix.GetPosition();
			matrix.SetRotateZ(-m_fSteerAngle);
			matrix.Translate(pos);
			matrix.UpdateRW();
		}
		if(m_aBoatNodes[BOAT_REARFLAP_RIGHT]){
			matrix.Attach(RwFrameGetMatrix(m_aBoatNodes[BOAT_REARFLAP_RIGHT]));
			pos = matrix.GetPosition();
			matrix.SetRotateZ(-m_fSteerAngle);
			matrix.Translate(pos);
			matrix.UpdateRW();
		}
	}

	// SA CBoat::PreRender (0x6F1180): the propellers. m_EngineSpeed is a rad/s
	// figure that ProcessControl() drives towards the PLANE_*_PROP_SPEED band;
	// the static pair spins twice as fast as the engine and the moving pair at
	// engine speed in the opposite directions, which is what makes the two
	// blades on each shaft look like they are geared differently. The alpha
	// trick blurs the blades into a disc at speed: a slow propeller gets a
	// transparent texture so the individual blades stay visible.
	{
		const float fPropSpeed = Min(1.0f, m_EngineSpeed * (32.0f / TWOPI));
		uint8 ucTransparency = (uint8)((1.0f - fPropSpeed) * 255.0f);

		m_PropellerAngle += m_EngineSpeed * CTimer::GetTimeStep();
		while(m_PropellerAngle > TWOPI)
			m_PropellerAngle -= TWOPI;

		ProcessBoatNodeRendering(BOAT_STATIC_PROP,   m_PropellerAngle * 2.0f,  ucTransparency);
		ProcessBoatNodeRendering(BOAT_STATIC_PROP_2, m_PropellerAngle * -2.0f, ucTransparency);

		ucTransparency = (ucTransparency >= 150 ? 0 : 150 - ucTransparency);
		ProcessBoatNodeRendering(BOAT_MOVING_PROP,   -m_PropellerAngle, ucTransparency);
		ProcessBoatNodeRendering(BOAT_MOVING_PROP_2,  m_PropellerAngle, ucTransparency);
	}

	// SA CBoat::ProcessControl (0x6F1D2C): the swinging flap is MARQUIS only.
	if(GetModelIndex() == MI_MARQUIS){
		if(m_aBoatNodes[BOAT_FLAP_LEFT]){
			matrix.Attach(RwFrameGetMatrix(m_aBoatNodes[BOAT_FLAP_LEFT]));
			CVector posCopy = matrix.GetPosition();
			// The hinge position in world space, which is what the acceleration
			// at the hinge has to be measured against.
			CVector vecTransformed = Multiply3x3(posCopy, GetMatrix());

			m_boom.Process(this, m_OldMoveSpeed, m_OldTurnSpeed, vecTransformed);
			CVector rot = m_boom.GetRotation();
			matrix.SetRotateKeepPos(rot.x, rot.y, rot.z);
			matrix.UpdateRW();
		}
	}

	if(GetModelIndex() == MI_RIO){
		// That little wind propeller
		if(m_aBoatNodes[BOAT_FLAP_RIGHT]){
			matrix.Attach(RwFrameGetMatrix(m_aBoatNodes[BOAT_FLAP_RIGHT]));
			pos = matrix.GetPosition();

			float flapHeading = matrix.GetForward().Heading();
			float boatHeading = GetForward().Heading();
			float rot = -DEGTORAD(45.0f) - (flapHeading + boatHeading);
			// eh what?
			rot = CGeneral::LimitRadianAngle(rot);
			if(rot > HALFPI) rot = PI;
			else if(rot < -HALFPI) rot = -PI;
			rot = Clamp(rot, -DEGTORAD(63.0f), DEGTORAD(63.0f));
			m_fMovingSpeed += (0.008f * CWeather::Wind + 0.002f) * rot;
			m_fMovingSpeed *= Pow(0.9985f, CTimer::GetTimeStep())/(500.0f*SQR(m_fMovingSpeed) + 1.0f);

			matrix.SetRotateZ(flapHeading + m_fMovingSpeed*CTimer::GetTimeStep());
			matrix.Translate(pos);
			matrix.UpdateRW();
		}
		if(m_aBoatNodes[BOAT_MOVING]){
			matrix.Attach(RwFrameGetMatrix(m_aBoatNodes[BOAT_MOVING]));
			pos = matrix.GetPosition();
			matrix.SetRotateY(m_fMovingRotation);
			matrix.Translate(pos);
			matrix.UpdateRW();

			CVector wind  = CVector(0.707f, 0.707f, 0.0f) * (CWeather::Wind + 0.15f)*0.4f;
			m_fMovingRotation += (m_vecMoveSpeed + wind).Magnitude()*CTimer::GetTimeStep();
		}
	}else if(GetModelIndex() == MI_PREDATOR || GetModelIndex() == MI_REEFER){
		if (m_aBoatNodes[BOAT_MOVING] != nil) {
			matrix.Attach(RwFrameGetMatrix(m_aBoatNodes[BOAT_MOVING]));

			CVector pos = matrix.GetPosition();
			matrix.SetRotateZ(m_fMovingRotation);
			matrix.Translate(pos);

			matrix.UpdateRW();
			if (CVehicle::bWheelsOnlyCheat) {
				RpAtomicRender((RpAtomic*)GetFirstObject(m_aBoatNodes[BOAT_MOVING]));
			}
		}
		m_fMovingRotation += 0.02f * CTimer::GetTimeStep();
	}

	// SA CBoat::ProcessControl (0x6F1D8F): snapshot the chassis motion for the
	// next frame's door spring. m_vecMoveFriction/m_vecTurnFriction are reVC's
	// names for SA's m_vecFrictionMoveSpeed/m_vecFrictionTurnSpeed.
	m_OldMoveSpeed = m_vecMoveSpeed + m_vecMoveFriction;
	m_OldTurnSpeed = m_vecTurnSpeed + m_vecTurnFriction;
}

RwIm3DVertex KeepWaterOutVertices[4];
RwImVertexIndex KeepWaterOutIndices[6];

void
CBoat::Render()
{
	((CVehicleModelInfo*)CModelInfo::GetModelInfo(GetModelIndex()))->SetVehicleColour(m_currentColour1, m_currentColour2);
	m_nSetPieceExtendedRangeTime = CTimer::GetTimeInMilliseconds() + 3000;
	if (!CVehicle::bWheelsOnlyCheat)
		CEntity::Render();
#ifdef NEW_RENDERER
	if(!gbNewRenderer)
#endif
	RenderWaterOutPolys();	// not separate function in VC
}

void
CBoat::RenderWaterOutPolys(void)
{
	if(GetModelIndex() == MI_SKIMMER)
		return;
	KeepWaterOutIndices[0] = 0;
	KeepWaterOutIndices[1] = 2;
	KeepWaterOutIndices[2] = 1;
	KeepWaterOutIndices[3] = 1;
	KeepWaterOutIndices[4] = 2;
	KeepWaterOutIndices[5] = 3;
	RwIm3DVertexSetRGBA(&KeepWaterOutVertices[0], 255, 255, 255, 255);
	RwIm3DVertexSetRGBA(&KeepWaterOutVertices[1], 255, 255, 255, 255);
	RwIm3DVertexSetRGBA(&KeepWaterOutVertices[2], 255, 255, 255, 255);
	RwIm3DVertexSetRGBA(&KeepWaterOutVertices[3], 255, 255, 255, 255);
	switch (GetModelIndex()) {
	case MI_RIO:
		RwIm3DVertexSetPos(&KeepWaterOutVertices[0], -1.3f, -1.016f, 0.51f);
		RwIm3DVertexSetPos(&KeepWaterOutVertices[1],  1.3f, -1.016f, 0.51f);
		RwIm3DVertexSetPos(&KeepWaterOutVertices[2], -1.3f, -2.832f, 0.51f);
		RwIm3DVertexSetPos(&KeepWaterOutVertices[3],  1.3f, -2.832f, 0.51f);
		break;
	case MI_SQUALO:
		RwIm3DVertexSetPos(&KeepWaterOutVertices[0], -1.222f,  2.004f, 1.409f);
		RwIm3DVertexSetPos(&KeepWaterOutVertices[1],  1.222f,  2.004f, 1.409f);
		RwIm3DVertexSetPos(&KeepWaterOutVertices[2], -1.24f,  -1.367f, 0.846f);
		RwIm3DVertexSetPos(&KeepWaterOutVertices[3],  1.24f,  -1.367f, 0.846f);
		break;
	case MI_SPEEDER:
		RwIm3DVertexSetPos(&KeepWaterOutVertices[0], -1.15f, 3.61f, 1.03f);
		RwIm3DVertexSetPos(&KeepWaterOutVertices[1],  1.15f, 3.61f, 1.03f);
		RwIm3DVertexSetPos(&KeepWaterOutVertices[2], -1.15f, 0.06f, 1.03f);
		RwIm3DVertexSetPos(&KeepWaterOutVertices[3],  1.15f, 0.06f, 1.03f);
		break;
	case MI_REEFER:
		RwIm3DVertexSetPos(&KeepWaterOutVertices[0], -1.9f,   2.83f, 1.0f);
		RwIm3DVertexSetPos(&KeepWaterOutVertices[1],  1.9f,   2.83f, 1.0f);
		RwIm3DVertexSetPos(&KeepWaterOutVertices[2], -1.66f, -4.48f, 0.83f);
		RwIm3DVertexSetPos(&KeepWaterOutVertices[3],  1.66f, -4.48f, 0.83f);
		break;
	case MI_PREDATOR:
		RwIm3DVertexSetPos(&KeepWaterOutVertices[0], -1.45f,   1.9f, 0.96f);
		RwIm3DVertexSetPos(&KeepWaterOutVertices[1],  1.45f,   1.9f, 0.96f);
		RwIm3DVertexSetPos(&KeepWaterOutVertices[2], -1.45f, -3.75f, 0.96f);
		RwIm3DVertexSetPos(&KeepWaterOutVertices[3],  1.45f, -3.75f, 0.96f);
		break;
	case MI_TROPIC:
		RwIm3DVertexSetPos(&KeepWaterOutVertices[0], -1.886f, -2.347f, 0.787f);
		RwIm3DVertexSetPos(&KeepWaterOutVertices[1],  1.886f, -2.347f, 0.787f);
		RwIm3DVertexSetPos(&KeepWaterOutVertices[2], -1.886f, -4.67f,  0.842f);
		RwIm3DVertexSetPos(&KeepWaterOutVertices[3],  1.886f, -4.67f,  0.842f);
		break;
	case MI_COASTG:
		RwIm3DVertexSetPos(&KeepWaterOutVertices[0], -0.663f, 3.565f, 0.382f);
		RwIm3DVertexSetPos(&KeepWaterOutVertices[1],  0.663f, 3.565f, 0.382f);
		RwIm3DVertexSetPos(&KeepWaterOutVertices[2], -1.087f, 0.83f,  0.381f);
		RwIm3DVertexSetPos(&KeepWaterOutVertices[3],  1.087f, 0.83f,  0.381f);
		break;
	case MI_DINGHY:
		RwIm3DVertexSetPos(&KeepWaterOutVertices[0], -0.797f,  1.641f, 0.573f);
		RwIm3DVertexSetPos(&KeepWaterOutVertices[1],  0.797f,  1.641f, 0.573f);
		RwIm3DVertexSetPos(&KeepWaterOutVertices[2], -0.865f, -1.444f, 0.509f);
		RwIm3DVertexSetPos(&KeepWaterOutVertices[3],  0.865f, -1.444f, 0.509f);
		break;
	case MI_MARQUIS:
		RwIm3DVertexSetPos(&KeepWaterOutVertices[0], -1.246f, -1.373f, 0.787f);
		RwIm3DVertexSetPos(&KeepWaterOutVertices[1],  1.246f, -1.373f, 0.787f);
		RwIm3DVertexSetPos(&KeepWaterOutVertices[2], -1.023f, -5.322f, 0.787f);
		RwIm3DVertexSetPos(&KeepWaterOutVertices[3],  1.023f, -5.322f, 0.787f);
		break;
	default:
		return;
	}
	KeepWaterOutVertices[0].u = 0.0f;
	KeepWaterOutVertices[0].v = 0.0f;
	KeepWaterOutVertices[1].u = 1.0f;
	KeepWaterOutVertices[1].v = 0.0f;
	KeepWaterOutVertices[2].u = 0.0f;
	KeepWaterOutVertices[2].v = 1.0f;
	KeepWaterOutVertices[3].u = 1.0f;
	KeepWaterOutVertices[3].v = 1.0f;
#ifdef NEW_RENDERER
	if(!gbNewRenderer)
#endif
{
	RwRenderStateSet(rwRENDERSTATETEXTURERASTER, gpWaterRaster);
	RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)TRUE);
	RwRenderStateSet(rwRENDERSTATEFOGENABLE, (void*)FALSE);
	RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDZERO);
	RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDONE);
}
	if (!CVehicle::bWheelsOnlyCheat && RwIm3DTransform(KeepWaterOutVertices, 4, GetMatrix().m_attachment, rwIM3D_VERTEXUV)) {
		RwIm3DRenderIndexedPrimitive(rwPRIMTYPETRILIST, KeepWaterOutIndices, 6);
		RwIm3DEnd();
	}
	bool drawAnotherRect = false;
	if(GetModelIndex() == MI_COASTG){
		drawAnotherRect = true;
		RwIm3DVertexSetPos(&KeepWaterOutVertices[0], -1.087f,  0.831f, 0.381f);
		RwIm3DVertexSetPos(&KeepWaterOutVertices[1],  1.087f,  0.831f, 0.381f);
		RwIm3DVertexSetPos(&KeepWaterOutVertices[2], -1.097f, -2.977f, 0.381f);
		RwIm3DVertexSetPos(&KeepWaterOutVertices[3],  1.097f, -2.977f, 0.381f);
	}
	if(drawAnotherRect){
		KeepWaterOutVertices[0].u = 0.0f;
		KeepWaterOutVertices[0].v = 0.0f;
		KeepWaterOutVertices[1].u = 1.0f;
		KeepWaterOutVertices[1].v = 0.0f;
		KeepWaterOutVertices[2].u = 0.0f;
		KeepWaterOutVertices[2].v = 1.0f;
		KeepWaterOutVertices[3].u = 1.0f;
		KeepWaterOutVertices[3].v = 1.0f;
		if (!CVehicle::bWheelsOnlyCheat && RwIm3DTransform(KeepWaterOutVertices, 4, GetMatrix().m_attachment, rwIM3D_VERTEXUV)) {
			RwIm3DRenderIndexedPrimitive(rwPRIMTYPETRILIST, KeepWaterOutIndices, 6);
			RwIm3DEnd();
		}
	}
#ifdef NEW_RENDERER
	if(!gbNewRenderer)
#endif
{
	RwRenderStateSet(rwRENDERSTATEFOGENABLE, (void*)TRUE);
	RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDSRCALPHA);
	RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDINVSRCALPHA);
}
}

void
CBoat::Teleport(CVector v)
{
	CWorld::Remove(this);
	SetPosition(v);
	SetOrientation(0.0f, 0.0f, 0.0f);
	SetMoveSpeed(0.0f, 0.0f, 0.0f);
	SetTurnSpeed(0.0f, 0.0f, 0.0f);
	CWorld::Add(this);
}

// unused
bool
CBoat::IsSectorAffectedByWake(CVector2D sector, float fSize, CBoat **apBoats)
{
	uint8 numVerts = 0;
	
	if ( apFrameWakeGeneratingBoats[0] == NULL )
		return false;
	
	for ( int32 i = 0; i < 4; i++ )
	{
		CBoat *pBoat = apFrameWakeGeneratingBoats[i];
		if ( !pBoat )
			break;
		
		for ( int j = 0; j < pBoat->m_NumWakeCoords; j++ )
		{
			 float fDist = (WAKE_LIFETIME - pBoat->m_WakePtCounters[j]) * fShapeTime + float(j) * fShapeLength + fSize;
			 
			 if ( Abs(pBoat->m_WakeCoords[j].x - sector.x) < fDist
				&& Abs(pBoat->m_WakeCoords[i].y - sector.y) < fDist )
			 {
				 apBoats[numVerts] = pBoat;
				 numVerts = 1; // += ?
				 break;
			 }
		}
	}
	
	return numVerts != 0;
}

// unused
float
CBoat::IsVertexAffectedByWake(CVector vecVertex, CBoat *pBoat)
{
	for ( int i = 0; i < pBoat->m_NumWakeCoords; i++ )
	{
		float fMaxDist = (WAKE_LIFETIME - pBoat->m_WakePtCounters[i]) * fShapeTime + float(i) * fShapeLength;
		
		CVector2D vecDist = pBoat->m_WakeCoords[i] - CVector2D(vecVertex);
		
		float fDist = vecDist.MagnitudeSqr();
		
		if ( fDist < SQR(fMaxDist) )
			return 1.0f - Min(fRangeMult * Sqrt(fDist / SQR(fMaxDist)) + (WAKE_LIFETIME - pBoat->m_WakePtCounters[i]) * fTimeMult, 1.0f);
	}

	return 0.0f;
}

void
CBoat::SetupModelNodes()
{
	int i;
	for(i = 0; i < ARRAY_SIZE(m_aBoatNodes); i++)
		m_aBoatNodes[i] = nil;
	CClumpModelInfo::FillFrameArray(GetClump(), m_aBoatNodes);
}

void
CBoat::FillBoatList()
{
	int16 frameId = 0;
	
	apFrameWakeGeneratingBoats[0] = nil;
	apFrameWakeGeneratingBoats[1] = nil;
	apFrameWakeGeneratingBoats[2] = nil;
	apFrameWakeGeneratingBoats[3] = nil;
	CVector2D camPos = TheCamera.GetPosition();
	CVector2D camFwd = TheCamera.GetForward();
	float camDist = camFwd.Magnitude();
	if(camDist > 0.0f)
		camFwd /= camDist;
	for (int i = CPools::GetVehiclePool()->GetSize() - 1; i >= 0; i--) {
		CBoat *boat = (CBoat *)(CPools::GetVehiclePool()->GetSlot(i));
		if (boat && boat->m_nVehicleType == VEHICLE_TYPE_BOAT) {
			if (boat->m_NumWakeCoords != 0) {
				CVector2D camToBoat = CVector2D(boat->GetPosition()) - camPos;
				float distToCam = DotProduct2D(camFwd, camToBoat);
				if(distToCam > 100.0f || distToCam < -15.0f)
					continue;
				float distSq = camToBoat.MagnitudeSqr();
				if(distSq > SQR(70.0f))
					continue;
				if (frameId >= ARRAY_SIZE(apFrameWakeGeneratingBoats)) {
					float nearest = 999999.88f;
					int16 frameId2 = -1;
					for (int16 j = 0; j < ARRAY_SIZE(apFrameWakeGeneratingBoats); j++) {
						float tmpDistSq = (CVector2D(apFrameWakeGeneratingBoats[j]->GetPosition()) - camPos).MagnitudeSqr();
						if (tmpDistSq < nearest) {
							nearest = tmpDistSq;
							frameId2 = j;
						}
					}

					if (frameId2 != -1 &&
					    (distSq < nearest || boat->GetStatus() == STATUS_PLAYER))
						apFrameWakeGeneratingBoats[frameId2] = boat;
				} else {
					apFrameWakeGeneratingBoats[frameId++] = boat;
				}
			}
		}
	}
}

// The wake is a ribbon the boat leaves behind: one quad per pair of trail
// points, its width and alpha derived from how fast the boat was going when
// that point was laid down (m_WakeBoatSpeed) and how much of its lifetime is
// left. Both graders fade the ribbon out as the boat gets away from the camera,
// so distant boats stop drawing long trails across the water.
void
CBoat::RenderWakePoints(void)
{
	if(m_NumWakeCoords <= 1)
		return;

	// A boat without collision has no hull height to size the ribbon from.
	CColModel *colModel = GetColModel();
	if(colModel == nil)
		return;
	float maxWidth = colModel->boundingBox.max.z * 0.65f;
	if(GetModelIndex() == MI_SKIMMER)
		maxWidth *= 0.4f;

	// A fast point spreads out over time: the trail widens as it ages.
	auto width = [&](uint16 ptIdx) -> float {
		float t = (WAKE_LIFETIME - m_WakePtCounters[ptIdx]) *
			((m_WakeBoatSpeed[ptIdx] / 25.0f + 0.5f) / WAKE_LIFETIME);
		return maxWidth + maxWidth * t;
	};

	// Fade with age, and again with distance from the camera so a boat that is
	// far away stops painting a trail across the whole screen.
	const float boatToCamDist2D = (CVector2D(TheCamera.GetPosition()) - CVector2D(GetPosition())).Magnitude();
	auto alpha = [&](uint16 ptIdx) -> float {
		float a = (1.0f - (float)ptIdx / (float)m_NumWakeCoords) * 160.0f;
		// Ramp the first three points in so the ribbon does not start with a hard
		// edge at the stern. FIX_BUGS uses (ptIdx + 1); the shipped code would
		// give the first point zero alpha, which is visibly wrong now that it is
		// the point nearest the hull.
		if(ptIdx < 3){
#ifdef FIX_BUGS
			a *= (float)(ptIdx + 1) / 3.0f;
#else
			a *= (float)ptIdx / 3.0f;
#endif
		}
		a *= (float)m_WakeBoatSpeed[ptIdx] / 100.0f + 0.15f;
		if(boatToCamDist2D > MAX_WAKE_LENGTH)
			a *= (MAX_WAKE_LENGTH + 30.0f - boatToCamDist2D) / 30.0f;
		return a;
	};

	// The reference draws the ribbon 3 cm below the water surface, which it can
	// hardcode because San Andreas has sea level at Z = 0. This data set is the
	// Vice City map, where the water sits at 5.97, so ask the water for the
	// height of the surface the boat is floating on and offset from that.
	float wakeZ = -0.03f;
	float waterZ;
	if(CWaterLevel::GetWaterLevelNoWaves(GetPosition().x, GetPosition().y, GetPosition().z, &waterZ))
		wakeZ = waterZ - 0.03f;

	float prevWidth = width(0);
	float prevAlpha = alpha(0);
	// Direction from the point before the previous one to the previous one;
	// the first segment has no such direction yet, so it starts along the hull.
	CVector2D prevDir = CVector2D(GetForward().x, GetForward().y);

	for(uint16 i = 1; i < m_NumWakeCoords; i++){
		const CVector2D &currPos = m_WakeCoords[i];
		const CVector2D &prevPos = m_WakeCoords[i - 1];

		CVector2D currToPrevDir = prevPos - currPos;
		float distToPrevSq = currToPrevDir.MagnitudeSqr();
		if(distToPrevSq >= SQR(3.0f)){
			float invLen = 1.0f / Sqrt(distToPrevSq);
			currToPrevDir.x *= invLen;
			currToPrevDir.y *= invLen;
		}

		float currAlpha = alpha(i);
		float currWidth = width(i);

		// A gap in the trail means the boat teleported or was out of the water;
		// drawing across it would paint a stripe over the whole scene.
		if(distToPrevSq <= SQR(13.0f)){
			// Right-hand perpendicular of each direction, scaled to width.
			CVector2D prevPerp(prevDir.y, -prevDir.x);
			CVector2D currPerp(currToPrevDir.y, -currToPrevDir.x);

			CVector2D prevA = prevPos - prevPerp * prevWidth;
			CVector2D prevB = prevPos + prevPerp * prevWidth;
			CVector2D currA = currPos - currPerp * currWidth;
			CVector2D currB = currPos + currPerp * currWidth;

			CWaterLevel::RenderWakeSegment(prevA, prevB, currB, currA,
				prevWidth, currWidth, prevAlpha, currAlpha, wakeZ);
		}

		prevWidth = currWidth;
		prevAlpha = currAlpha;
		prevDir = currToPrevDir;
	}
}

// Draws every boat trail for this frame. The water renderer owns the wake
// texture and the trail buffer, so the pass lives there; this is the entry
// point that keeps the per-boat work in CBoat.
void
CBoat::RenderAllWakePointBoats(void)
{
	FillBoatList();
	for(int32 i = 0; i < ARRAY_SIZE(apFrameWakeGeneratingBoats); i++){
		CBoat *boat = apFrameWakeGeneratingBoats[i];
		if(boat == nil)
			break;
		boat->RenderWakePoints();
	}
}

void
CBoat::PruneWakeTrail(void)
{
	int i;

	for(i = 0; i < ARRAY_SIZE(m_WakePtCounters); i++){
		if(m_WakePtCounters[i] <= 0.0f)
			break;
		if(m_WakePtCounters[i] <= CTimer::GetTimeStep()){
			m_WakePtCounters[i] = 0.0f;
			break;
		}
		m_WakePtCounters[i] -= CTimer::GetTimeStep();
	}
	m_NumWakeCoords = i;
}

void
CBoat::AddWakePoint(CVector point)
{
	uint8 intensity = m_vecMoveSpeed.Magnitude() * 100.0f;

	// the first point just starts the trail
	if(m_WakePtCounters[0] <= 0.0f){
		m_WakeCoords[0] = point;
		m_WakeBoatSpeed[0] = intensity;
		m_WakePtCounters[0] = WAKE_LIFETIME;
		m_NumWakeCoords = 1;
		return;
	}

	// too close to the last point, the trail would break up
	if((CVector2D(GetPosition()) - m_WakeCoords[0]).MagnitudeSqr() <= SQR(MIN_WAKE_INTERVAL))
		return;

	// how long a trail a boat of this kind may leave
	uint16 maxWakePoints = 31;
	if(GetStatus() != STATUS_PLAYER){
		if(VehicleCreatedBy == MISSION_VEHICLE)
			maxWakePoints = 20;
		else
			maxWakePoints = 15;
	}

	uint16 numWakePoints = Min((uint16)m_NumWakeCoords, maxWakePoints);
	for(int i = numWakePoints; i > 0; i--){
		m_WakeCoords[i] = m_WakeCoords[i-1];
		m_WakeBoatSpeed[i] = m_WakeBoatSpeed[i-1];
		m_WakePtCounters[i] = m_WakePtCounters[i-1];
	}

	m_WakeCoords[0] = point;
	m_WakeBoatSpeed[0] = intensity;
	m_WakePtCounters[0] = WAKE_LIFETIME;
	if(m_NumWakeCoords < ARRAY_SIZE(m_WakePtCounters))
		m_NumWakeCoords++;
}

void
CBoat::DoDriveByShootings(void)
{
	CAnimBlendAssociation *anim = nil;
	CPlayerInfo* playerInfo = ((CPlayerPed*)pDriver)->GetPlayerInfoForThisPlayerPed();
	if (playerInfo && !playerInfo->m_bDriveByAllowed)
		return;

	CWeapon *weapon = pDriver->GetWeapon();
	if(CWeaponInfo::GetWeaponInfo(weapon->m_eWeaponType)->m_nWeaponSlot != 5)
		return;

	weapon->Update(pDriver->m_audioEntityId, nil);

	bool lookingLeft = false;
	bool lookingRight = false;
	if(TheCamera.Cams[TheCamera.ActiveCam].Mode == CCam::MODE_TOPDOWN ||
	   TheCamera.m_bObbeCinematicCarCamOn){
		if(CPad::GetPad(0)->GetLookLeft())
			lookingLeft = true;
		if(CPad::GetPad(0)->GetLookRight())
			lookingRight = true;
	}else{
		if(TheCamera.Cams[TheCamera.ActiveCam].LookingLeft)
			lookingLeft = true;
		if(TheCamera.Cams[TheCamera.ActiveCam].LookingRight)
			lookingRight = true;
	}

	if(lookingLeft || lookingRight){
		if(lookingLeft){
			anim = RpAnimBlendClumpGetAssociation(pDriver->GetClump(), ANIM_ID_DRIVEBY_R);
			if(anim)
				anim->blendDelta = -1000.0f;
			anim = RpAnimBlendClumpGetAssociation(pDriver->GetClump(), ANIM_ID_DRIVEBY_L);
			if(anim == nil || anim->blendDelta < 0.0f)
				anim = CAnimManager::AddAnimation(pDriver->GetClump(), ASSOCGRP_DEFAULT, ANIM_ID_DRIVEBY_L);
		}else if(pDriver->m_pMyVehicle->pPassengers[0] == nil || TheCamera.Cams[TheCamera.ActiveCam].Mode == CCam::MODE_1STPERSON){
			anim = RpAnimBlendClumpGetAssociation(pDriver->GetClump(), ANIM_ID_DRIVEBY_L);
			if(anim)
				anim->blendDelta = -1000.0f;
			anim = RpAnimBlendClumpGetAssociation(pDriver->GetClump(), ANIM_ID_DRIVEBY_R);
			if(anim == nil || anim->blendDelta < 0.0f)
				anim = CAnimManager::AddAnimation(pDriver->GetClump(), ASSOCGRP_DEFAULT, ANIM_ID_DRIVEBY_R);
		}

		if (!anim || !anim->IsRunning()) {
			if (CPad::GetPad(0)->GetCarGunFired() && CTimer::GetTimeInMilliseconds() > weapon->m_nTimer) {
				weapon->FireFromCar(this, lookingLeft, true);
				weapon->m_nTimer = CTimer::GetTimeInMilliseconds() + 70;
			}
		}
	}else{
		weapon->Reload();
		anim = RpAnimBlendClumpGetAssociation(pDriver->GetClump(), ANIM_ID_DRIVEBY_L);
		if(anim)
			anim->blendDelta = -1000.0f;
		anim = RpAnimBlendClumpGetAssociation(pDriver->GetClump(), ANIM_ID_DRIVEBY_R);
		if(anim)
			anim->blendDelta = -1000.0f;
	}
}

#ifdef COMPATIBLE_SAVES
void
CBoat::Save(uint8*& buf)
{
	CVehicle::Save(buf);
	ZeroSaveBuf(buf, 1216 - 672);
}

void
CBoat::Load(uint8*& buf)
{
	CVehicle::Load(buf);
	SkipSaveBuf(buf, 1216 - 672);
}
#endif
