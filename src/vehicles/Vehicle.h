#ifndef __GTA_VEHICLE_H__
#define __GTA_VEHICLE_H__

#include "Physical.h"
#include "AutoPilot.h"
#include "ModelIndices.h"
#include "AnimationEnums.h"
#include "WeaponType.h"
#include "Collision.h"
#include "cHandlingDataMgr.h"

class CVehicleAnimGroup;

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

// SA CVehicle::NUM_VEHICLE_UPGRADES. The buyable slots a car can wear: bonnet,
// bumper, exhaust, spoiler, the four wheel sets, hydraulics, nitro, stereo and
// the rest of the carmods.dat list. ms_upgradeWheels has one model per rim
// design and CVehicle::m_anUpgrades stores the model fitted to each slot.
//
// SA numbers slots by eVehicleMod, which has seventeen values (the eleven
// upgrades plus six replacement parts), and m_anUpgrades is 15 entries wide
// because hydraulics and stereo arrive as flags rather than as model ids. This
// fork fits every part by model id, so the array is 18 entries - the same width
// as the per-model table CVehicleModelInfo keeps - and the two extra slots hold
// hydraulics and stereo.
enum {
	NUM_VEHICLE_UPGRADES = 18
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

// SA eVehicleOverrideLightsState (Vehicle.h). A script's standing order for a
// vehicle's lamps: it wins over the time of day, and it is also what makes a
// car show its lamps at noon or keep them off at night.
enum eVehicleOverrideLightsState : uint8
{
	NO_CAR_LIGHT_OVERRIDE = 0,
	FORCE_CAR_LIGHTS_OFF  = 1,
	FORCE_CAR_LIGHTS_ON   = 2,
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
	// SA CVehicle::m_nTertiaryColor / m_nQuaternaryColor. The third and fourth
	// paint channels carcols.dat can give a car; a car whose line names only two
	// colours repeats the first pair, so the extra material slots stay in step
	// with the two the model has always painted.
	uint8 m_currentColour3;
	uint8 m_currentColour4;
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
	// SA CVehicle::m_wMiscComponentAngle. The tipping angle of the one moving
	// part a vehicle drives through its handling data - the Dumper's bed, the
	// Packer's ramp. SA stores it as a halfword and the readers compare it frame
	// to frame to detect a new target angle.
	int16 m_wMiscComponentAngle;
	int16 m_wMiscComponentAnglePrev;
	// SA CVehicle::m_fPropRotate. The angle of the one continuously spinning
	// part a vehicle carries - the harvester trailer's reel, the RC bandit's
	// propeller. SA rotates it by the vehicle's own forward speed.
	float m_fPropRotate;
	// SA CPhysical::m_nFakePhysics, carried on the vehicle in SA. Counts the
	// frames a vehicle has been held still by its controller; the trailer uses
	// it to keep its supports animating while it settles.
	uint32 m_nFakePhysics;
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
	// SA CVehicle::vehicleFlags.bLightsOn. What the lamp pass last decided:
	// the lamps are on, so the lamp sheets swap to the lit one. Written by
	// CVehicle::DoVehicleLights from GetVehicleLightsStatus, overridden by a
	// script forcing them on or off.
	uint8 bLightsOn: 1;
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
	uint8 m_bombType : 3;
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
	CEntity* m_pBombRigger;
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
	uint8 m_nOverrideLights;   // eVehicleOverrideLightsState
	// SA CVehicle::ms_forceVehicleLightsOff. With it set no vehicle draws a
	// lamp at all, whatever its own state says - the cutscene code uses it.
	static bool bForceVehicleLightsOff;
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
	// Which source last drove m_fSteerInput: 2 = the mouse, 0 = the pad. This
	// was a file static in ProcessControlInputs, i.e. shared by every car in the
	// world; it belongs to the vehicle whose steering it tracks.
	int16 m_nLastSteerInput;
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
	// SA CVehicle::FindWheelWidth (0x6D0E00 for cars, overridden by the bike
	// family). The wheel geometry is per class, so the base returns the car
	// default and CBike/CBmx override it.
	virtual float FindWheelWidth(bool bRear) { return bRear ? 0.825f : 0.73f; }
	// SA CVehicle::ProcessDrivingAnims (0x6D3D00). Drives the driver's in-car
	// animation set; CAutomobile and CBike override it, CBmx adds the bunny hop
	// on top.
	virtual void ProcessDrivingAnims(CPed *driver, bool blend) {}
	// SA CVehicle::ProcessAI (0x6D31A0). Vehicles that want their own AI hook
	// override this; the base reports "not handled" so the caller falls back to
	// the generic path.
	virtual bool ProcessAI(uint32 &extraHandlingFlags) { return false; }
	// SA CVehicle::m_pCustomCarPlate. The plate texture this vehicle owns when
	// its model carries a scripted plate text; nil when the vehicle simply
	// shares its model's plate texture. Owned by the vehicle and destroyed with
	// it, because a generated plate is per-vehicle while the model is shared.
	RwTexture *m_pCustomCarPlate;
	// The text m_pCustomCarPlate was baked from, when the vehicle has a plate of
	// its own; empty when it shares the model's. Eight characters plus the
	// terminator, the same limit SA puts on a plate.
	char m_szOwnPlateText[9];

	// ---- Tuning (SA CVehicle upgrade API) --------------------------------
	//
	// SA CVehicle::m_anUpgrades. One model id per slot NUM_VEHICLE_UPGRADES,
	// -1 when the slot is empty. Written by AddVehicleUpgrade and by the
	// garage when a stored car is put back together; SetupUpgradesAfterLoad
	// replays it after a save is loaded, because the atomics themselves are
	// not saved.
	int16 m_anUpgrades[NUM_VEHICLE_UPGRADES];
	// SA CVehicle::m_nNitroBoosts. Nitro bottles left, counting down from 101
	// (SA's "full" marker) as the bottle is used; 0 means the tank is empty.
	uint8 m_nNitroBoosts;
	// SA CVehicle::vehicleFlags.bHydraulicInst / bNosInst / bUpgradedStereo.
	// The functional half of the three upgrades that are not geometry: they
	// stay set when the part is fitted and are what the driving code checks.
	uint8 bHydraulicInst : 1;
	uint8 bNosInst : 1;
	uint8 bUpgradedStereo : 1;
	// SA CVehicle::m_nRemapTxd / m_nPreviousRemapTxd. The paintjob texture
	// dictionary asked for and the one currently applied. They differ for the
	// one frame a swap takes, which is why both exist.
	int16 m_nRemapTxd;
	int16 m_nPreviousRemapTxd;
	// SA CVehicle::m_pRemapTexture. The first texture of the applied paintjob
	// dictionary, published to CVehicleModelInfo::ms_pRemapTexture while this
	// vehicle is drawn.
	RwTexture *m_pRemapTexture;

	// SA CVehicle::AddUpgrade replaces a wheel's mesh rather than adding a
	// second rim: the car's own wheel atomics are taken out of the clump (which
	// stops them being drawn but keeps them alive) and put back when the rims
	// come off. One entry per wheel node.
	enum { NUM_WHEEL_SLOTS = 4 };
	RpAtomic *m_apHiddenWheelAtomics[NUM_WHEEL_SLOTS];

	// The atomic of a fitted part for the given eVehicleMod slot, or nil.
	// SA keeps the same information in CVisibilityPlugins::GetModelInfo on the
	// atomic; reVC stamps the slot and model into the atomic id instead.
	RpAtomic *FindUpgradeAtomic(int32 slot);

	// SA CVehicle::AddVehicleUpgrade (0x6E3290). Fit a bought part by model id.
	void AddVehicleUpgrade(int32 modelId);
	// SA CVehicle::RemoveVehicleUpgrade (0x6DF930). Take the part of the same
	// slot off again.
	void RemoveVehicleUpgrade(int32 upgradeModelIndex);
	// SA CVehicle::AddUpgrade (0x6DFA20). Attach one fitment of a part: the
	// model to instantiate and the eVehicleMod slot it belongs to.
	void AddUpgrade(int32 modelIndex, int32 upgradeIndex);
	// SA CVehicle::RemoveUpgrade (0x6D3630). Remove every atomic carrying the
	// given upgrade id.
	void RemoveUpgrade(int32 upgradeId);
	// SA CVehicle::RemoveAllUpgrades (0x6D3AB0).
	void RemoveAllUpgrades(void);
	// SA CVehicle::GetUpgrade (0x6D3650). The model id fitted to a slot, or -1.
	// The three functional upgrades report the model SA reports for them,
	// because their geometry is a placeholder the driving code never looks at.
	int32 GetUpgrade(int32 upgradeId);
	// SA CVehicle::AddReplacementUpgrade (0x6D3830). Swap one of the car's own
	// parts for a bought one that replaces it (bumper, exhaust, chassis).
	void AddReplacementUpgrade(int32 modelIndex, int32 nodeId);
	// SA CVehicle::RemoveReplacementUpgrade (0x6D39E0). Put the car's own part
	// back.
	void RemoveReplacementUpgrade(int32 frameId);
	// SA CVehicle::GetReplacementUpgrade (0x6D3A50).
	int32 GetReplacementUpgrade(int32 nodeId);
	// SA CVehicle::SetupUpgradesAfterLoad (0x6E3400). Replay m_anUpgrades; the
	// slots are consumed as they are fitted.
	void SetupUpgradesAfterLoad(void);
	// True when this vehicle has the functional hydraulics upgrade - SA reaches
	// the same answer through m_aUpgrades, this fork keeps a flag as well
	// because the driving code asks once per frame.
	bool HasHydraulics(void) const { return bHydraulicInst; }
	// True when this vehicle can be driven on its nitro bottles.
	bool HasNitro(void) const { return bNosInst; }

	// SA CVehicle::GetRemapIndex (0x6D0B70). Position of the applied paintjob
	// in the model's remap list, or -1.
	int32 GetRemapIndex(void);
	// SA CVehicle::SetRemapTexDictionary (0x6D0BC0).
	void SetRemapTexDictionary(int32 txdId);
	// SA CVehicle::SetRemap (0x6D0C00). Paintjob by index into the model's list;
	// -1 removes the paintjob.
	void SetRemap(int32 remapIndex);
	// Applies the pending remap texture to the model's editable materials and
	// hands it to the model for the duration of one Render. Called by
	// SetupRender, and again when the texture was swapped.
	void ApplyRemapTexture(void);

	// SA CVehicle::SetupRender (0x6D64F0) and CVehicle::ResetAfterRender
	// (0x6D0E20). The pair brackets the drawing of one vehicle: SetupRender
	// paints the model with this car's four colours, publishes its paintjob,
	// copies its lamp flags out and swaps the materials that carry state;
	// ResetAfterRender puts every one of them back. SA calls them from
	// CRenderer::RenderOneNonRoad, around the entity's own Render and the
	// alpha pass, which is why they live here and not inside a vehicle's
	// Render: the sorted alpha list draws atomics of the same vehicle after
	// Render has returned, and those atomics must still wear the swapped
	// materials.
	void SetupRender(void);
	void ResetAfterRender(void);

	// ---- State API for an external controller ---------------------------
	//
	// Everything above is what the game itself calls. The accessors below are
	// the surface a second driver - a network peer, a replay player, a test
	// harness - needs in order to read and write a vehicle without reaching
	// into its fields and without going through CPad. Two rules keep them honest:
	//
	//  * a setter writes only the state it names. It does not read the pad, does
	//    not play a sound and does not spawn a particle. A remote vehicle must be
	//    able to hold a steering angle without a pad existing for it;
	//  * a getter is const and returns the state the physics actually uses, so
	//    a snapshot taken after ProcessControl is what the next frame integrates.
	//
	// The game keeps writing the same fields directly from ProcessControlInputs;
	// these are a second door onto the same state, not a parallel copy of it.

	// Transform. CPlaceable already has GetMatrix/SetMatrix/GetPosition/
	// SetPosition/GetHeading; these add the two operations a snapshot needs and
	// that had no entry point: writing a full pose in one step and reading it
	// back without an aliasing reference.
	CVector GetMoveSpeedState(void) const { return m_vecMoveSpeed; }
	void SetMoveSpeedState(const CVector &speed) { m_vecMoveSpeed = speed; }
	CVector GetTurnSpeedState(void) const { return m_vecTurnSpeed; }
	void SetTurnSpeedState(const CVector &speed) { m_vecTurnSpeed = speed; }
	// Position, orientation (about the world axes, radians) and both velocities
	// in one call. This is the whole of a vehicle's pose, and the only thing a
	// peer has to send for a vehicle it does not simulate.
	void SetPose(const CVector &pos, const CVector &orientation,
		const CVector &moveSpeed, const CVector &turnSpeed)
	{
		SetPosition(pos);
		SetOrientation(orientation.x, orientation.y, orientation.z);
		m_vecMoveSpeed = moveSpeed;
		m_vecTurnSpeed = turnSpeed;
	}

	// Controls. Getters first: they report what the last ProcessControlInputs
	// left behind, which is what the physics and the audio both read.
	float GetGasPedal(void) const { return m_fGasPedal; }
	float GetBrakePedal(void) const { return m_fBrakePedal; }
	// The pedal that is actually being pressed: gas and brake are separate
	// fields but only ever one of them is live (see ProcessControlInputs), so
	// this is the value a remote peer should be shown.
	float GetActivePedal(void) const { return m_fBrakePedal > m_fGasPedal ? m_fBrakePedal : m_fGasPedal; }
	float GetSteerAngle(void) const { return m_fSteerAngle; }
	// The normalised [-1, 1] steering input the angle is derived from. Kept
	// separately because the angle also carries the handling record's lock and
	// the squared response curve, neither of which a peer should have to undo.
	float GetSteerInput(void) const { return m_fSteerInput; }
	bool IsHandbrakeOn(void) const { return bIsHandbrakeOn; }

	void SetGasPedal(float gas) { m_fGasPedal = gas; }
	void SetBrakePedal(float brake) { m_fBrakePedal = brake; }
	// Gas and brake together, clamped to [0, 1] as the pad path does. This is
	// the setter a remote peer needs: it is exactly the pair the drivetrain
	// consumes, and nothing else.
	void SetPedals(float gas, float brake)
	{
		m_fGasPedal = Clamp(gas, 0.0f, 1.0f);
		m_fBrakePedal = Clamp(brake, 0.0f, 1.0f);
	}
	// Steering from the normalised input. The response curve (cubed, signed)
	// and the handling record's lock are applied here so a peer sends the same
	// [-1, 1] the pad produces and the wheels end up where the local car's do.
	void SetSteerInput(float input);
	void SetHandbrakeOn(bool on) { bIsHandbrakeOn = on; }
	// Sets the wheel angle directly, bypassing the response curve. For replaying
	// a recorded state, where the angle - not the input - is what was stored.
	void SetSteerAngleDirect(float angle) { m_fSteerAngle = angle; }

	// Body. m_fHealth is the one number the damage model is built on; the
	// individual parts live in CDamageManager (see CAutomobile::GetDamageManager
	// and the per-part helpers it exposes).
	float GetHealth(void) const { return m_fHealth; }
	void SetHealth(float health) { m_fHealth = health; }
	// Fuel is not simulated in this fork (SA does not simulate it either); the
	// vehicle it belongs to is described by its health and its engine state.

	// On-board systems.
	bool IsEngineOn(void) const { return bEngineOn; }
	void SetEngineOn(bool on) { bEngineOn = on; }
	bool AreLightsOn(void) const { return bLightsOn; }
	void SetLightsOn(bool on) { bLightsOn = on; }
	// A script's standing order on the lamps, in SA's three states. Note that
	// AreLightsOn is the *result* of the last lamp pass, not the order: with
	// FORCE_CAR_LIGHTS_OFF the pass turns the lamps off on the next frame,
	// while the order itself stays until it is cleared.
	eVehicleOverrideLightsState GetOverrideLights(void) const
	{
		return (eVehicleOverrideLightsState)m_nOverrideLights;
	}
	void SetOverrideLights(eVehicleOverrideLightsState state)
	{
		m_nOverrideLights = (uint8)state;
	}
	bool IsSirenOrAlarmOn(void) const { return m_bSirenOrAlarm; }
	void SetSirenOrAlarm(bool on) { m_bSirenOrAlarm = on; }
	// How long the horn has been sounding, in milliseconds; 0 means silent.
	uint32 GetHornTimer(void) const { return m_nCarHornTimer; }
	void SetHornTimer(uint32 timer) { m_nCarHornTimer = timer; }
	uint8 GetHornPattern(void) const { return m_nCarHornPattern; }
	void SetHornPattern(uint8 pattern) { m_nCarHornPattern = pattern; }
	// The radio station is a byte index into the audio system's station table;
	// RADIO_OFF silences it. Stored on the vehicle so it survives a change of
	// driver, which is what lets a peer hear the same station the owner chose.
	uint8 GetRadioStation(void) const { return m_nRadioStation; }
	void SetRadioStation(uint8 station) { m_nRadioStation = station; }
	// Nitro: bottles left (0 = empty) and whether the bottle is installed at
	// all. Setting the count on a car without the fitment does nothing, because
	// the driving code only looks at it when bNosInst is set.
	uint8 GetNitroBoosts(void) const { return m_nNitroBoosts; }
	void SetNitroBoosts(uint8 bottles) { m_nNitroBoosts = bottles; }
	// Nitro is implemented by CAutomobile; the base answers "no bottle fitted",
	// so a controller can ask any vehicle without a type test.
	virtual bool IsNitroActive(void) const;
	virtual void SetNitroActive(bool active);
	// Hydraulics: fitted or not, and the height the suspension is being pumped
	// to. The fitted flag is what the driving code gates on.
	bool AreHydraulicsInstalled(void) const { return bHydraulicInst; }
	void SetHydraulicsInstalled(bool installed) { bHydraulicInst = installed; }
	// The pump state the handling drives. CAutomobile owns the suspension that
	// consumes it, so the height itself is set there; this is the flag half that
	// every vehicle type shares.
	bool IsUpgradedStereoInstalled(void) const { return bUpgradedStereo; }

	// Customisation: the four paint channels in carcols.dat order, the paintjob,
	// and the plate.
	uint8 GetPrimaryColour(void) const { return m_currentColour1; }
	uint8 GetSecondaryColour(void) const { return m_currentColour2; }
	uint8 GetTertiaryColour(void) const { return m_currentColour3; }
	uint8 GetQuaternaryColour(void) const { return m_currentColour4; }
	void GetColours(uint8 &c1, uint8 &c2, uint8 &c3, uint8 &c4) const
	{
		c1 = m_currentColour1;
		c2 = m_currentColour2;
		c3 = m_currentColour3;
		c4 = m_currentColour4;
	}
	// Writes the four channels without touching the model. The caller is
	// responsible for making the model show them (SetVehicleColour on the model
	// info) - a remote car repaints itself from its own snapshot, not from this
	// instance's material state.
	void SetColours(uint8 c1, uint8 c2, uint8 c3, uint8 c4)
	{
		m_currentColour1 = c1;
		m_currentColour2 = c2;
		m_currentColour3 = c3;
		m_currentColour4 = c4;
	}
	// Paintjob by index into the model's remap list, -1 for none.
	int32 GetPaintjob(void) { return GetRemapIndex(); }
	void SetPaintjob(int32 remapIndex) { SetRemap(remapIndex); }
	// Plate text. The vehicle's own plate texture wins when it exists (it was
	// baked from a script text); otherwise the text comes from the model, which
	// is shared. Writing here builds this vehicle a texture of its own, so a
	// custom plate stays with the car it was set on.
	bool SetPlateText(const char *text);
	// The plate text this vehicle currently shows, or nil. The model's pending
	// text is one-shot (it is consumed by the first vehicle built on the model),
	// so a vehicle that was given a custom plate keeps its own copy here; a
	// vehicle without one reports the generated text the model baked in, which
	// is not recoverable as a string and therefore reported as nil.
	const char *GetPlateText(void) const { return m_szOwnPlateText[0] ? m_szOwnPlateText : nil; }

	// Towing. A tractor and its trailer point at each other; m_pTowingVehicle is
	// the car in front, m_pVehicleBeingTowed the one behind.
	CVehicle *GetTowingVehicle(void) const { return m_pTowingVehicle; }
	CVehicle *GetVehicleBeingTowed(void) const { return m_pVehicleBeingTowed; }
	// Links the two halves without moving anything. The physics link is set up
	// by SetTowLink/UpdateTrailerLink; this only records the relationship, which
	// is what a peer has to transmit before either side steps.
	void SetTowPointers(CVehicle *towing, CVehicle *towed)
	{
		m_pTowingVehicle = towing;
		m_pVehicleBeingTowed = towed;
	}

	// SA CVehicle::CustomCarPlate_TextureCreate (0x6D10E0). Build this
	// vehicle's own plate texture if its model is carrying a pending plate
	// text, otherwise take a reference on the model's shared one.
	bool CustomCarPlate_TextureCreate(CVehicleModelInfo *model);
	// SA CVehicle::CustomCarPlate_TextureDestroy (0x6D1150).
	void CustomCarPlate_TextureDestroy(void);

	// SA CVehicle::GetAnimGroup (0x6D0AC0). The vehicle's entry in the 30-group
	// in-car animation table. The index comes from the handling record, not from
	// the model: handling.cfg carries it as the last field of each vehicle line.
	CVehicleAnimGroup &GetAnimGroup(void) const;

	// SA CVehicle::SetTransmissionRotation (0x6DBBB0). Tilts a live axle so its
	// two ends follow the wheels: the average of the two wheel travels sets the
	// pitch against the wheel line, the difference the roll. Used by the
	// MonsterTruck's two exposed driveshafts and the Quadbike's chain.
	void SetTransmissionRotation(RwFrame *component, float angleL, float angleR, CVector wheelPos, bool isFront);
	virtual bool IsRoomForPedToLeaveCar(uint32 component, CVector *forcedDoorPos) { return false; }
	virtual bool IsClearToDriveAway(void);
	virtual float GetHeightAboveRoad(void);
	virtual void PlayCarHorn(void) {}
	// SA CVehicle towing interface (0x6DFBA0 / 0x6DFBE0 / 0x6DD070 / 0x6DD0C0).
	// Every vehicle has both halves of a coupling: the bar at its rear and the
	// hitch at its front. Only the classes that can be towed or tow override
	// them; the base answers "no such point", which is what makes a normal car
	// uncoupleable.
	virtual bool GetTowHitchPos(CVector &outPos, bool checkModelInfo, CVehicle *vehicle);
	virtual bool GetTowBarPos(CVector &outPos, bool checkModelInfo, CVehicle *vehicle);
	virtual bool SetTowLink(CVehicle *tractor, bool setMyPosToTowBar) { return false; }
	virtual bool BreakTowLink(void) { return false; }
	// SA CVehicle::UpdateTractorLink / UpdateTrailerLink (0x6E0050 / 0x6DFC50),
	// both thin wrappers over the same solver.
	void UpdateTractorLink(bool applyFullVelocityAtHookUp, bool applyDistToSpeed);
	void UpdateTrailerLink(bool applyFullVelocityAtHookUp, bool applyDistToSpeed);

	eVehicleAppearance GetVehicleAppearance(void);

	// ---- Optical lights (SA CVehicle::DoVehicleLights family) -----------
	//
	// SA draws a car's lamps in one place, CVehicle::DoVehicleLights
	// (0x6E1A60), and keeps the answer - which of the four lamps actually lit
	// - in m_renderLights. That answer is read twice: by the render setup to
	// swap the lamp sheets (CVehicleModelInfo::SetupLightFlags) and by
	// CBike::Render to decide on the headlight beam. The functions below are
	// that one place, split the way SA splits it, so what gets drawn and what
	// the material pass does cannot disagree.
	//
	// SA eVehicleLightsFlags (Vehicle.h). The flags say how the lamp pass is
	// to be run for the vehicle, not what state the vehicle is in.
	enum eVehicleLightsFlags
	{
		VEHICLE_LIGHTS_NONE          = 0,
		VEHICLE_LIGHTS_TWIN          = 1,  // both sides of a lamp pair lit
		VEHICLE_LIGHTS_IGNORE_DAMAGE = 4,  // draw even a smashed lamp
		VEHICLE_LIGHTS_DISABLE_FRONT = 16, // no front lamps this pass
		VEHICLE_LIGHTS_DISABLE_REAR  = 32, // no rear lamps this pass
	};

	// SA eVehicleLightId (Vehicle.h): the first and the second lamp dummy of
	// a pair, i.e. "headlights"/"headlights2" and "taillights"/"taillights2".
	enum eVehicleLightId
	{
		VEHICLE_LIGHT_MAIN      = 0,
		VEHICLE_LIGHT_SECONDARY = 1,
	};

	// SA CVehicle::DoVehicleLights (0x6E1A60). Decides whether the lamps are
	// on, draws the coronas, the light shadows and the point lights, and fills
	// m_renderLights with what was actually lit. This replaces the VC family
	// (separate headlight and taillight blocks inside CAutomobile::PreRender),
	// which differed in colour, size, damage handling, the second lamp dummy
	// and the reversing lights.
	void DoVehicleLights(CMatrix &vehicleMatrix, eVehicleLightsFlags flags);
	// The flags DoVehicleLights is called with for this vehicle on an ordinary
	// frame: twin lamps for a car whose two sides have separate geometry, plus
	// the two disables for a vehicle that has no lamps at its front or rear.
	eVehicleLightsFlags GetVehicleLightsFlags(void) const;
	// SA CVehicle::GetVehicleLightsStatus (0x6D55C0). Whether the car's lamps
	// *should* be on: night, poor weather, or the shadows the cull zone puts on
	// the road. The result is cached in bLightsOn by DoVehicleLights.
	bool GetVehicleLightsStatus(void) const;

	// SA CVehicle::DoLightEffectImpl (0x6E1780): one lamp of one pair. Returns true when the lamp registered a corona, which
	// is what the caller stores in m_renderLights.
	bool DoLightEffect(eVehicleLightId lightId, bool isFront, CMatrix &vehicleMatrix,
		bool isRight, bool disabledOrAlarm, bool staticEmission);
	// SA CVehicle::DoHeadLightEffect (0x6E0A50) / DoTailLightEffect (0x6E1780).
	bool DoHeadLightEffect(eVehicleLightId lightId, CMatrix &vehicleMatrix, bool isRight,
		bool disabledOrAlarm);
	bool DoTailLightEffect(eVehicleLightId lightId, CMatrix &vehicleMatrix, bool isRight,
		bool disabledOrAlarm, bool staticEmission);
	// SA CVehicle::DoReverseLightEffect. The white reversing lamps, drawn only
	// when the car is in reverse and its lamps are off: with the lamps on the
	// reversing quad is already in the lit sheet.
	void DoReverseLightEffect(CMatrix &vehicleMatrix);
	// SA CVehicle::DoHeadLightReflection (0x6E1720) and its two entry points.
	void DoHeadLightReflection(CMatrix &vehicleMatrix, eVehicleLightsFlags flags,
		bool includeLeft, bool includeRight);

	// The lamp dummy of a light id in model space: DUMMY_LIGHT_FRONT_MAIN /
	// DUMMY_LIGHT_FRONT_SECONDARY / DUMMY_LIGHT_REAR_MAIN /
	// DUMMY_LIGHT_REAR_SECONDARY.
	static int32 GetDummyFromLightId(eVehicleLightId lightId, bool isFront);

	// SA CDamageManager::GetLightStatus needs a damage manager, and only an
	// automobile has one. The lamp pass asks through here so a bike's lamp -
	// which is part of its bodywork and cannot be smashed separately - is
	// treated as intact instead of dereferencing nothing.
	virtual class CDamageManager *GetDamageManager(void) { return nil; }
	virtual const class CDamageManager *GetDamageManager(void) const { return nil; }

	// SA CVehicle::DoHeadLightBeam (0x6E0E20). A five-vertex additive cone
	// anchored at one of the two headlight dummies and aimed along the bonnet.
	// In SA only CBike::Render calls it; cars get the corona and the point
	// light and nothing else, so this is not wired into CAutomobile.
	void DoHeadLightBeam(eVehicleLightId lightId, CMatrix &vehicleMatrix, bool isRight);

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
	// SA CVehicle::ClearGettingOutFlags: the clearing twin of the above.
	void ClearGettingOutFlags(uint8 doorFlag) { m_nGettingOutFlags &= ~doorFlag; }
	// SA CVehicle::ClearGettingInFlags: same for the getting-in flags.
	void ClearGettingInFlags(uint8 doorFlag) { m_nGettingInFlags &= ~doorFlag; }
	bool CanPedOpenLocks(CPed *ped);
	bool CanDoorsBeDamaged(void);
	bool CanPedEnterCar(void);
	bool CanPedExitCar(bool jumpExit);
	bool CanPedJumpOutCar(void);
	bool CanPedJumpOutCar(CPed *ped);
	bool CanPedJumpOffBike(void);
	// SA CVehicle::CanPedStepOutCar (0x6D1F30): slow and steady enough
	// for a ped to step out rather than jump.
	bool CanPedStepOutCar(bool bIgnoreSpeedUpright) const;
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
