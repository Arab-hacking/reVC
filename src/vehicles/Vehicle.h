#ifndef __GTA_VEHICLE_H__
#define __GTA_VEHICLE_H__

#include "Physical.h"
#include "AutoPilot.h"
#include "ModelIndices.h"
#include "AnimationId.h"
#include "WeaponType.h"
#include "Collision.h"
#include "cHandlingDataMgr.h"

class CPed;
class CPlayerPed;
class CCopPed;
class CFire;

enum {
	RANDOM_VEHICLE = 1,
	MISSION_VEHICLE = 2,
	PARKED_VEHICLE = 3,
	PERMANENT_VEHICLE = 4,
};

// SA eCarNodes (Enums/eCarNodes.h), verbatim.
//
// The index of an entry here is the *frame's position in the DFF hierarchy*,
// handed out by CVehicleModelInfo::SetFrameIds in table order, so this enum and
// the name table in VehicleModelInfo.cpp have to agree exactly. SA starts at
// CAR_NODE_NONE = 0 (the clump root, which is never a component) and reserves
// index 1 for the chassis, shifting everything else by one relative to the VC
// layout. There is deliberately no VC ordering left behind: the project's
// models are San Andreas ones now.
enum eCarNodes
{
	CAR_NODE_NONE  = 0,
	CAR_CHASSIS    = 1,
	CAR_WHEEL_RF   = 2,
	CAR_WHEEL_RM   = 3,
	CAR_WHEEL_RB   = 4,
	CAR_WHEEL_LF   = 5,
	CAR_WHEEL_LM   = 6,
	CAR_WHEEL_LB   = 7,
	CAR_DOOR_RF    = 8,
	CAR_DOOR_RR    = 9,
	CAR_DOOR_LF    = 10,
	CAR_DOOR_LR    = 11,
	CAR_BUMP_FRONT = 12,
	CAR_BUMP_REAR  = 13,
	CAR_WING_RF    = 14,
	CAR_WING_LF    = 15,
	CAR_BONNET     = 16,
	CAR_BOOT       = 17,
	CAR_WINDSCREEN = 18,
	CAR_EXHAUST    = 19,
	CAR_MISC_A     = 20,
	CAR_MISC_B     = 21,
	CAR_MISC_C     = 22,
	CAR_MISC_D     = 23,
	CAR_MISC_E     = 24,

	NUM_CAR_NODES = 25
};

enum {
	CAR_DOOR_FLAG_UNKNOWN = 0x0,
	CAR_DOOR_FLAG_LF = 0x1,
	CAR_DOOR_FLAG_LR = 0x2,
	CAR_DOOR_FLAG_RF = 0x4,
	CAR_DOOR_FLAG_RR = 0x8
};

enum eCarLock {
	CARLOCK_NOT_USED,
	CARLOCK_UNLOCKED,
	CARLOCK_LOCKED,
	CARLOCK_LOCKOUT_PLAYER_ONLY,
	CARLOCK_LOCKED_PLAYER_INSIDE,
	CARLOCK_LOCKED_INITIALLY,
	CARLOCK_FORCE_SHUT_DOORS,
	CARLOCK_LOCKED_BUT_CAN_BE_DAMAGED
};

enum eBombType
{
	CARBOMB_NONE,
	CARBOMB_TIMED,
	CARBOMB_ONIGNITION,
	CARBOMB_REMOTE,
	CARBOMB_TIMEDACTIVE,
	CARBOMB_ONIGNITIONACTIVE,
};

enum eDoors
{
	DOOR_BONNET = 0,
	DOOR_BOOT,
	DOOR_FRONT_LEFT,
	DOOR_FRONT_RIGHT,
	DOOR_REAR_LEFT,
	DOOR_REAR_RIGHT,
	MAX_DOORS
};

enum ePanels
{
	VEHPANEL_FRONT_LEFT,
	VEHPANEL_FRONT_RIGHT,
	VEHPANEL_REAR_LEFT,
	VEHPANEL_REAR_RIGHT,
	VEHPANEL_WINDSCREEN,
	VEHBUMPER_FRONT,
	VEHBUMPER_REAR,
};

enum eLights
{
	VEHLIGHT_FRONT_LEFT,
	VEHLIGHT_FRONT_RIGHT,
	VEHLIGHT_REAR_LEFT,
	VEHLIGHT_REAR_RIGHT,
};

enum
{
	CAR_PIECE_BONNET = 1,
	CAR_PIECE_BOOT,
	CAR_PIECE_BUMP_FRONT,
	CAR_PIECE_BUMP_REAR,
	CAR_PIECE_DOOR_LF,
	CAR_PIECE_DOOR_RF,
	CAR_PIECE_DOOR_LR,
	CAR_PIECE_DOOR_RR,
	CAR_PIECE_WING_LF,
	CAR_PIECE_WING_RF,
	CAR_PIECE_WING_LR,
	CAR_PIECE_WING_RR,
	CAR_PIECE_WHEEL_LF,
	CAR_PIECE_WHEEL_RF,
	CAR_PIECE_WHEEL_LR,
	CAR_PIECE_WHEEL_RR,
	CAR_PIECE_WINDSCREEN,
};

enum tWheelState
{
	WHEEL_STATE_NORMAL,	// standing still or rolling normally
	WHEEL_STATE_SPINNING,	// rotating but not moving
	WHEEL_STATE_SKIDDING,
	WHEEL_STATE_FIXED,	// not rotating
};

enum eVehicleAppearance
{
	VEHICLE_APPEARANCE_NONE = 0,
	VEHICLE_APPEARANCE_AUTOMOBILE = 1,
	VEHICLE_APPEARANCE_BIKE = 2,
	VEHICLE_APPEARANCE_HELI = 3,
	VEHICLE_APPEARANCE_BOAT = 4,
	VEHICLE_APPEARANCE_PLANE = 5,
};

// TODO: what is this even?
enum eBikeWheelSpecial
{
	BIKE_WHEELSPEC_0,	// both wheels on ground
	BIKE_WHEELSPEC_1,	// rear wheel on ground
	BIKE_WHEELSPEC_2,	// only front wheel on ground
	BIKE_WHEELSPEC_3,	// can't happen
};

enum
{
	ROTOR_TOP = 3,
	ROTOR_FRONT = 4,
	ROTOR_RIGHT = 5,
	ROTOR_LEFT = 7,
	ROTOR_BACK = 8,
	ROTOR_BOTTOM = 9,
};

class CVehicle : public CPhysical
{
public:
	tHandlingData *m_pHandlingData;
	tFlyingHandlingData *m_pFlyingHandlingData;
	CAutoPilot AutoPilot;
	uint8 m_currentColour1;
	uint8 m_currentColour2;
	int8 m_aExtras[2];
	int16 m_nAlarmState;
	int16 m_nRouteSeed;
	CPed *pDriver;
	CPed *pPassengers[8];
	uint8 m_nNumPassengers;
	int8 m_nNumGettingIn;
	int8 m_nGettingInFlags;
	int8 m_nGettingOutFlags;
	uint8 m_nNumMaxPassengers;
	float field_1D0[4];
	CEntity *m_pCurGroundEntity;
	CFire *m_pCarFire;
	float m_fSteerAngle;
	float m_fGasPedal;
	float m_fBrakePedal;
	uint8 VehicleCreatedBy;

	// cf. https://github.com/DK22Pac/plugin-sdk/blob/master/plugin_sa/game_sa/CVehicle.h from R*
	uint8 bIsLawEnforcer: 1; // Is this guy chasing the player at the moment
	uint8 bIsAmbulanceOnDuty: 1; // Ambulance trying to get to an accident
	uint8 bIsFireTruckOnDuty: 1; // Firetruck trying to get to a fire
	uint8 bIsLocked: 1; // Is this guy locked by the script (cannot be removed)
	uint8 bEngineOn: 1; // For sound purposes. Parked cars have their engines switched off (so do destroyed cars)
	uint8 bIsHandbrakeOn: 1; // How's the handbrake doing ?
	uint8 bLightsOn: 1; // Are the lights switched on ?
	uint8 bFreebies: 1; // Any freebies left in this vehicle ?

	uint8 bIsVan: 1; // Is this vehicle a van (doors at back of vehicle)
	uint8 bIsBus: 1; // Is this vehicle a bus
	uint8 bIsBig: 1; // Is this vehicle a bus
	uint8 bLowVehicle: 1; // Need this for sporty type cars to use low getting-in/out anims
	uint8 bComedyControls : 1; // Will make the car hard to control (hopefully in a funny way)
	uint8 bWarnedPeds : 1; // Has scan and warn peds of danger been processed?
	uint8 bCraneMessageDone : 1; // A crane message has been printed for this car allready
	uint8 bExtendedRange : 1; // This vehicle needs to be a bit further away to get deleted

	uint8 bTakeLessDamage : 1; // This vehicle is stronger (takes about 1/4 of damage)
	uint8 bIsDamaged : 1; // This vehicle has been damaged and is displaying all its components
	uint8 bHasBeenOwnedByPlayer : 1;// To work out whether stealing it is a crime
	uint8 bFadeOut : 1; // Fade vehicle out
	uint8 bIsBeingCarJacked : 1; // Fade vehicle out
	uint8 bCreateRoadBlockPeds : 1; // If this vehicle gets close enough we will create peds (coppers or gang members) round it
	uint8 bCanBeDamaged : 1; // Set to FALSE during cut scenes to avoid explosions
	uint8 bUsingSpecialColModel : 1;// Is player vehicle using special collision model, stored in player strucure

	uint8 bOccupantsHaveBeenGenerated : 1;  // Is true if the occupants have already been generated. (Shouldn't happen again)
	uint8 bGunSwitchedOff : 1;  // Level designers can use this to switch off guns on boats
	uint8 bVehicleColProcessed : 1;// Has ProcessEntityCollision been processed for this car?
	uint8 bIsCarParkVehicle : 1; // Car has been created using the special CAR_PARK script command
	uint8 bHasAlreadyBeenRecorded : 1; // Used for replays
	uint8 bPartOfConvoy : 1;
	uint8 bHeliMinimumTilt : 1; // This heli should have almost no tilt really
	uint8 bAudioChangingGear : 1; // sounds like vehicle is changing gear

	uint8 bIsDrowning : 1; // is vehicle occupants taking damage in water (i.e. vehicle is dead in water)
	uint8 bTyresDontBurst : 1; // If this is set the tyres are invincible
	uint8 bCreatedAsPoliceVehicle : 1;// True if this guy was created as a police vehicle (enforcer, policecar, miamivice car etc)
	uint8 bRestingOnPhysical : 1; // Dont go static cause car is sitting on a physical object that might get removed
	uint8 bParking : 1;
	uint8 bCanPark : 1;
#if (!defined GTA_PS2 || defined FIX_BUGS)
	uint8 m_bombType : 3;
#endif
	uint8 bDriverLastFrame : 1;
	// SA CVehicle::vehicleFlags.bIsRCVehicle. Needed by CanBeDriven, which is
	// what decides whether a ped may take the wheel.
	uint8 bIsRCVehicle : 1;

	int8 m_numPedsUseItAsCover;
	uint8 m_nAmmoInClip;    // Used to make the guns on boat do a reload (20 by default)
	int8 m_nPacManPickupsCarried;
	uint8 m_nRoadblockType;
	float m_fHealth;           // 1000.0f = full health. 250.0f = fire. 0 -> explode
	uint8 m_nCurrentGear;
	float m_fChangeGearTime;
	// SA engine inertia state belongs to each instance, not the shared handling.
	float m_fTransmissionInertiaRatio;
	float m_fTransmissionInertiaSmoothing;
#if (!defined GTA_PS2 || defined FIX_BUGS)
	CEntity* m_pBombRigger;
#endif
	uint32 m_nSetPieceExtendedRangeTime;
	// SA CVehicle::m_nRenderLightsFlags: which lamps actually registered a
	// corona this frame. CBike::Render reads m_bRightFront to decide whether to
	// draw the headlight cone, so this has to be filled during light processing.
	union {
		uint8 m_nRenderLightsFlags;
		struct {
			uint8 m_bRightFront : 1;
			uint8 m_bLeftFront  : 1;
			uint8 m_bRightRear  : 1;
			uint8 m_bLeftRear   : 1;
		} m_renderLights;
	};
	uint32 m_nGunFiringTime;    // last time when gun on vehicle was fired (used on boats)
	uint32 m_nTimeOfDeath;
	uint16 m_nTimeBlocked;
	int16 m_nBombTimer;        // goes down with each frame
	CEntity *m_pBlowUpEntity;
	float m_fMapObjectHeightAhead;	// front Z?
	float m_fMapObjectHeightBehind;	// rear Z?
	eCarLock m_nDoorLock;
	int8 m_nLastWeaponDamage; // see eWeaponType, -1 if no damage
	CEntity *m_pLastDamageEntity;
	// SA CVehicle::m_pTowingVehicle / m_pVehicleBeingTowed. A tractor points at
	// its trailer and the trailer back at the tractor, so both ends can answer
	// "am I attached, and to what".
	CVehicle *m_pTowingVehicle;
	CVehicle *m_pVehicleBeingTowed;
	uint8 m_nRadioStation;
	uint8 m_bRainAudioCounter;
	uint8 m_bRainSamplesCounter;
	uint32 m_nCarHornTimer;
	uint8 m_nCarHornPattern;
	bool m_bSirenOrAlarm;
	uint8 m_nCarHornDelay;
	int8 m_comedyControlState;
	CStoredCollPoly m_aCollPolys[2];     // poly which is under front/rear part of car
	float m_fSteerInput;
	float m_fRawSteerAngle;
	eVehicleType m_nVehicleType;
	eVehicleType m_nVehicleSubType;	// differs from the type only for vehicles that behave as two classes, e.g. the vortex

	static void *operator new(size_t) throw();
	static void *operator new(size_t sz, int slot) throw();
	static void operator delete(void*, size_t) throw();
	static void operator delete(void*, int) throw();

	CVehicle(void) {}	// FAKE
	CVehicle(uint8 CreatedBy);
	~CVehicle(void);
	// from CEntity
	void SetModelIndex(uint32 id);
	bool SetupLighting(void);
	void RemoveLighting(bool);
	void FlagToDestroyWhenNextProcessed(void) {}

	virtual void ProcessControlInputs(uint8) {}
	virtual void GetComponentWorldPosition(int32 component, CVector &pos) {}
	virtual bool IsComponentPresent(int32 component) { return false; }
	virtual void SetComponentRotation(int32 component, CVector rotation) {}
	virtual void OpenDoor(int32, eDoors door, float) {}
	virtual void ProcessOpenDoor(uint32, uint32, float) {}
	virtual bool IsDoorReady(eDoors door) { return false; }
	virtual bool IsDoorFullyOpen(eDoors door) { return false; }
	virtual bool IsDoorClosed(eDoors door) { return false; }
	virtual bool IsDoorMissing(eDoors door) { return false; }
	virtual bool IsDoorReady(uint32 door) { return false; }
	virtual bool IsDoorMissing(uint32 door) { return false; }
	virtual bool IsDoorClosed(uint32 door) { return false; }
	virtual bool IsDoorFullyOpen(uint32 door) { return false; }
	virtual bool IsOpenTopCar(void) { return false; }
	virtual void RemoveRefsToVehicle(CEntity *ent) {}
	virtual void BlowUpCar(CEntity *ent) {}
	virtual bool SetUpWheelColModel(CColModel *colModel) { return false; }
	virtual void BurstTyre(uint8 tyre, bool applyForces) {}
	virtual bool IsRoomForPedToLeaveCar(uint32 component, CVector *forcedDoorPos) { return false; }
	virtual bool IsClearToDriveAway(void);
	virtual float GetHeightAboveRoad(void);
	virtual void PlayCarHorn(void) {}
#ifdef COMPATIBLE_SAVES
	virtual void Save(uint8*& buf);
	virtual void Load(uint8*& buf);
#endif

	eVehicleAppearance GetVehicleAppearance(void);

	// SA CVehicle::DoHeadLightBeam (0x6E0E20). A five-vertex additive cone
	// anchored at the headlight dummy and aimed along the bonnet. In SA only
	// CBike::Render calls it; cars get the corona and the point light and
	// nothing else, so this is not wired into CAutomobile.
	void DoHeadLightBeam(CMatrix &vehicleMatrix, bool isRight);

	// SA helper: the light dummy in model space. DUMMY_LIGHT_FRONT_MAIN for the
	// primary lamp pair, DUMMY_LIGHT_FRONT_SECONDARY for the second one.
	CVector GetDummyPositionObjSpace(int32 dummy) const;

	bool IsVehicleTypeValid(void) const { return m_nVehicleType != VEHICLE_TYPE_IGNORE; }
	bool IsAutomobile(void) const { return m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE; }
	bool IsMonsterTruck(void) const { return m_nVehicleType == VEHICLE_TYPE_MTRUCK; }
	bool IsQuad(void) const { return m_nVehicleType == VEHICLE_TYPE_QUAD; }
	bool IsHeli(void) const { return m_nVehicleType == VEHICLE_TYPE_HELI; }
	bool IsPlane(void) const { return m_nVehicleType == VEHICLE_TYPE_PLANE; }
	bool IsBoat(void) const { return m_nVehicleType == VEHICLE_TYPE_BOAT; }
	bool IsTrain(void) const { return m_nVehicleType == VEHICLE_TYPE_TRAIN; }
	bool IsFakeAircraft(void) const { return m_nVehicleType == VEHICLE_TYPE_FHELI || m_nVehicleType == VEHICLE_TYPE_FPLANE; }
	bool IsBike(void) const { return m_nVehicleType == VEHICLE_TYPE_BIKE; }
	bool IsBMX(void) const { return m_nVehicleType == VEHICLE_TYPE_BMX; }
	bool IsTrailer(void) const { return m_nVehicleType == VEHICLE_TYPE_TRAILER; }

	bool IsSubVehicleTypeValid(void) const { return m_nVehicleSubType != VEHICLE_TYPE_IGNORE; }
	bool IsSubAutomobile(void) const { return m_nVehicleSubType == VEHICLE_TYPE_AUTOMOBILE; }
	bool IsSubMonsterTruck(void) const { return m_nVehicleSubType == VEHICLE_TYPE_MTRUCK; }
	bool IsSubQuad(void) const { return m_nVehicleSubType == VEHICLE_TYPE_QUAD; }
	bool IsSubHeli(void) const { return m_nVehicleSubType == VEHICLE_TYPE_HELI; }
	bool IsSubPlane(void) const { return m_nVehicleSubType == VEHICLE_TYPE_PLANE; }
	bool IsSubBoat(void) const { return m_nVehicleSubType == VEHICLE_TYPE_BOAT; }
	bool IsSubTrain(void) const { return m_nVehicleSubType == VEHICLE_TYPE_TRAIN; }
	bool IsSubFakeAircraft(void) const { return m_nVehicleSubType == VEHICLE_TYPE_FHELI || m_nVehicleSubType == VEHICLE_TYPE_FPLANE; }
	bool IsSubBike(void) const { return m_nVehicleSubType == VEHICLE_TYPE_BIKE; }
	bool IsSubBMX(void) const { return m_nVehicleSubType == VEHICLE_TYPE_BMX; }
	bool IsSubTrailer(void) const { return m_nVehicleSubType == VEHICLE_TYPE_TRAILER; }

	// The handling record carries the real class of the physics, which can be
	// narrower than the runtime type (an "RC bandit" is an automobile that the
	// handling line marks as a bike-like body).
	bool IsRealBike(void) const { return m_pHandlingData->m_bIsBike; }
	bool IsRealHeli(void) const { return m_pHandlingData->m_bIsHeli; }
	bool IsRealPlane(void) const { return m_pHandlingData->m_bIsPlane; }
	bool IsRealBoat(void) const { return m_pHandlingData->m_bIsBoat; }

	bool DoBladeCollision(CVector pos, CMatrix &matrix, int16 rotorType, float radius, float damageMult);
	bool BladeColSectorList(CPtrList &list, CColModel &rotorColModel, CMatrix &matrix, int16 rotorType, float damageMult);

	// longitudinal/lateral slip limits of a wheel, from the executable
	static constexpr float WS_TRAC_FRAC_LIMIT = 0.3f;
	static constexpr float WS_ALREADY_SPINNING_LOSS = 0.2f;
	void ProcessWheel(CVector &wheelFwd, CVector &wheelRight, CVector &wheelContactSpeed, CVector &wheelContactPoint,
		int32 wheelsOnGround, float thrust, float brake, float adhesion, int8 wheelId, float &wheelSpeedUnused, tWheelState &wheelState, uint16 wheelStatus);
	void ProcessBikeWheel(CVector &wheelFwd, CVector &wheelRight, CVector &wheelContactSpeed, CVector &wheelContactPoint,
		int32 wheelsOnGround, float thrust, float brake, float adhesion, float destabTraction, int8 wheelId, float *wheelSpeed, tWheelState *wheelState, eBikeWheelSpecial special, uint16 wheelStatus);
	void ExtinguishCarFire(void);
	void ProcessDelayedExplosion(void);
	float ProcessWheelRotation(tWheelState state, const CVector &fwd, const CVector &speed, float radius);
	int FindTyreNearestPoint(float x, float y);
	bool IsLawEnforcementVehicle(void);
	void ChangeLawEnforcerState(uint8 enable);
	bool UsesSiren(void);
	bool IsVehicleNormal(void);
	bool CarHasRoof(void);
	bool IsUpsideDown(void);
	bool IsOnItsSide(void);
	bool CanBeDeleted(void);
	// SA CVehicle helpers the car-enter/exit code is written against. reVC
	// stores the seats as pDriver/pPassengers/m_nNumMaxPassengers, so these are
	// thin wrappers over that layout, not reimplementations.
	bool HasDriver(void) const { return pDriver != nil; }
	bool HasPassengerAtSeat(int32 seat) const { return pPassengers[seat] != nil; }
	CPed *GetPassenger(int32 seat) const { return pPassengers[seat]; }
	int32 GetMaxPassengerSeats(void) const { return m_nNumMaxPassengers; }
	// SA CVehicle::GetAnimGroupId: the anim group comes from the handling data,
	// not from the ped, so a bike and a car in the same block differ.
	uint8 GetAnimGroupId(void) const { return m_pHandlingData ? m_pHandlingData->m_nAnimGroup : 0; }
	// SA CVehicle::CanBeDriven: a trailer, a train on a track, or an RC car
	// cannot be driven by a ped; everything else can, provided the model has a
	// driver seat dummy.
	bool CanBeDriven(void) const;
	// SA CVehicle::GetDriverSeatDummyPositionOS (0x6D42B0): model-space position
	// of the driver's seat. A boat has no seat dummy, so SA uses its front light
	// dummy there.
	CVector GetDriverSeatDummyPositionOS(void) const;
	// SA CVehicle::IsMissionVehicle.
	bool IsMissionVehicle(void) const { return VehicleCreatedBy == MISSION_VEHICLE; }
	// SA CVehicle::SetGettingInFlags/SetGettingOutFlags (0x6D30F0/0x6D3020) are
	// OR-merges, not assignments - which is why QuitEnteringCar can add a flag
	// without clearing the others.
	void SetGettingInFlags(uint8 doorFlag) { m_nGettingInFlags |= doorFlag; }
	void SetGettingOutFlags(uint8 doorFlag) { m_nGettingOutFlags |= doorFlag; }
	bool CanPedOpenLocks(CPed *ped);
	bool CanDoorsBeDamaged(void);
	bool CanPedEnterCar(void);
	bool CanPedExitCar(bool jumpExit);
	bool CanPedJumpOutCar(void);
	bool CanPedJumpOutCar(CPed *ped);
	bool CanPedJumpOffBike(void);
	// do these two actually return something?
	CPed *SetUpDriver(void);
	CPed *SetupPassenger(int n);
	void SetDriver(CPed *driver);
	bool AddPassenger(CPed *passenger);
	bool AddPassenger(CPed *passenger, uint8 n);
	void RemovePassenger(CPed *passenger);
	void RemoveDriver(void);
	bool IsDriver(CPed *ped);
	bool IsDriver(int32 model);
	bool IsPassenger(CPed *ped);
	bool IsPassenger(int32 model);
	void UpdatePassengerList(void);
	void ProcessCarAlarm(void);
	bool IsSphereTouchingVehicle(float sx, float sy, float sz, float radius);
	bool ShufflePassengersToMakeSpace(void);
	void MakeNonDraggedPedsLeaveVehicle(CPed *ped1, CPed *ped2, CPlayerPed *&player, CCopPed *&cop);
	void InflictDamage(CEntity *damagedBy, eWeaponType weaponType, float damage, CVector pos = CVector(0.0f, 0.0f, 0.0f));
	void DoFixedMachineGuns(void);
	void FireFixedMachineGuns(void);
	void ActivateBomb(void);
	void ActivateBombWhenEntered(void);
	void KillPedsInVehicle(void);

	void SetComponentAtomicAlpha(RpAtomic *atomic, int32 alpha);
	void UpdateClumpAlpha(void);

	static void HeliDustGenerate(CEntity *heli, float radius, float ground, int rnd);
	void DoSunGlare(void);

	bool IsAlarmOn(void) { return m_nAlarmState != 0 && m_nAlarmState != -1 && GetStatus() != STATUS_WRECKED; }
	CVehicleModelInfo* GetModelInfo() { return (CVehicleModelInfo*)CModelInfo::GetModelInfo(GetModelIndex()); }
	bool IsTaxi(void) { return GetModelIndex() == MI_TAXI || GetModelIndex() == MI_CABBIE || GetModelIndex() == MI_ZEBRA || GetModelIndex() == MI_KAUFMAN; }
	bool IsLimo(void) { return GetModelIndex() == MI_STRETCH || GetModelIndex() == MI_LOVEFIST; }

	static bool bWheelsOnlyCheat;
	static bool bCheat3;
	static bool bCheat4;
	static bool bCheat5;
	static bool bCheat8;
	static bool bCheat9;
	static bool bCheat10;
	static bool bHoverCheat;
	static bool bAllTaxisHaveNitro;
	static bool m_bDisableMouseSteering;
	static int32 m_nLastControlInput;
	// Extra air resistance inside the cull zones that carry the flag.
	static constexpr float m_fAirResistanceMult = 2.5f;
	static bool bDisableRemoteDetonation;
	static bool bDisableRemoteDetonationOnContact;
#ifndef MASTER
	static bool m_bDisplayHandlingInfo;
#endif
};

void DestroyVehicleAndDriverAndPassengers(CVehicle* pVehicle);
bool IsVehiclePointerValid(CVehicle* pVehicle);

// Names of functions below are made up by us.

// Used in III and VC.
inline int8 GetCarDoorFlag(int32 carnode) {
	switch (carnode) {
	case CAR_DOOR_LF:
		return CAR_DOOR_FLAG_LF;
	case CAR_DOOR_LR:
		return CAR_DOOR_FLAG_LR;
	case CAR_DOOR_RF:
		return CAR_DOOR_FLAG_RF;
	case CAR_DOOR_RR:
		return CAR_DOOR_FLAG_RR;
	default:
		return CAR_DOOR_FLAG_UNKNOWN;
	}
}

// VC. Accounts the case numMaxPassengers == 0, only for m_nGettingInFlags.
inline int8 GetEnterCarDoorFlag(int32 carnode, uint8 numMaxPassengers) {
	switch (carnode) {
	case CAR_DOOR_RF:
		return CAR_DOOR_FLAG_RF;
	case CAR_DOOR_RR:
		return CAR_DOOR_FLAG_RR;
	case CAR_DOOR_LF:
		if (numMaxPassengers != 0)
			return CAR_DOOR_FLAG_LF;
		else
			return CAR_DOOR_FLAG_LF | CAR_DOOR_FLAG_LR;
	case CAR_DOOR_LR:
		if (numMaxPassengers != 0)
			return CAR_DOOR_FLAG_LR;
		else
			return CAR_DOOR_FLAG_LF | CAR_DOOR_FLAG_LR;
	default:
		return CAR_DOOR_FLAG_UNKNOWN;
	}
}

#endif // __GTA_VEHICLE_H__
