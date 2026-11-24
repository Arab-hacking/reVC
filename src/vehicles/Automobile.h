#ifndef __GTA_AUTOMOBILE_H__
#define __GTA_AUTOMOBILE_H__

#include "Vehicle.h"
#include "DamageManager.h"
#include "Door.h"
#include "Skidmarks.h"

class CObject;

enum {
	CARWHEEL_FRONT_LEFT,
	CARWHEEL_REAR_LEFT,
	CARWHEEL_FRONT_RIGHT,
	CARWHEEL_REAR_RIGHT
};


class CAutomobile : public CVehicle
{
public:
	CDamageManager Damage;
	CDoor Doors[6];
	// Rest orientation of each swinging door node. Vanilla cars model their
	// doors axis aligned, but converted models bake a rotation into the frame,
	// so the door angle has to be applied on top of this instead of replacing
	// it. Captured the first time the door swings.
	CMatrix m_aDoorBaseRot[6];
	bool m_bDoorBaseRotValid[6];
	RwFrame *m_aCarNodes[NUM_CAR_NODES];
	CColPoint m_aWheelColPoints[4];
	float m_aSuspensionSpringRatio[4];
	float m_aSuspensionSpringRatioPrev[4];
	float m_aWheelTimer[4];		// set to 4.0 when wheel is touching ground, then decremented
	float m_auto_unused1;
	eSkidmarkType m_aWheelSkidmarkType[4];
	bool m_aWheelSkidmarkBloody[4];
	bool m_aWheelSkidmarkUnk[4];
	float m_aWheelRotation[4];
	float m_aWheelPosition[4];
	float m_aWheelSpeed[4];
	uint8 m_auto_unused2;
#if (defined GTA_PS2 && !defined FIX_BUGS)
	uint8 m_bombType : 3;
#endif
	uint8 bTaxiLight : 1;
	uint8 bFixedColour : 1;
	uint8 bBigWheels : 1;
	uint8 bNotDamagedUpsideDown : 1;
	uint8 bMoreResistantToDamage : 1;
	uint8 bTankDetonateCars : 1;
	uint8 bStuckInSand : 1;
	uint8 bHeliDestroyed : 1;
#if (defined GTA_PS2 && !defined FIX_BUGS)
	CEntity* m_pBombRigger;
#endif
	int16 m_doingBurnout;
	uint16 m_hydraulicState;
	uint32 m_nBusDoorTimerEnd;
	uint32 m_nBusDoorTimerStart;
	float m_aSuspensionSpringLength[4];
	float m_aSuspensionLineLength[4];
	float m_fHeightAboveRoad;
	float m_fTraction;
	float m_fTireTemperature;
	// Nitro state machine, 1.0 = idle with a full bottle, a small negative value
	// = firing, climbing back to 1.0 while the bottle refills. SA reuses
	// m_fTireTemperature for this, which makes one value mean two things: tyre
	// heat (read by the traction code) and bottle state (read by the nitro code).
	// Splitting them keeps the tyre model readable from a snapshot and lets a
	// remote car report its bottle without lying about its tyres.
	float m_fNitroState;
	// When the water-splash particle bursts of a car standing in water are due,
	// as absolute CTimer::GetTimeInMilliseconds values; 0 means "not scheduled".
	// Both were statics in ProcessBuoyancy, so a second car in the water reset
	// the first one's timers.
	uint32 m_nWaterCirclesTime;
	uint32 m_nRaindropsTime;
	float m_fOrientation;	// for heli and plane go-to
	float m_fPlaneSteer;	// related to the above
	float m_fVelocityChangeForAudio;
	float m_randomValues[6];	// used for what?
	float m_fFireBlowUpTimer;
	CPhysical *m_aGroundPhysical[4];	// physicals touching wheels
	CVector m_aGroundOffset[4];		// from ground object to colpoint
	CEntity *m_pSetOnFireEntity;
	float m_weaponDoorTimerLeft;	// still don't know what exactly this is
	float m_weaponDoorTimerRight;
	float m_fCarGunLR;
	float m_fCarGunUD;
	float m_fHeliOrientation;
	float m_fPropellerRotation;
	uint8 stuff4[4];
	uint8 m_nWheelsOnGround;
	uint8 m_nDriveWheelsOnGround;
	uint8 m_nDriveWheelsOnGroundPrev;
	float m_fGasPedalAudio;
	tWheelState m_aWheelState[4];

	static bool m_sAllTaxiLights;

	CAutomobile(int32 id, uint8 CreatedBy);

	// from CEntity
	void SetModelIndex(uint32 id);
	void ProcessControl(void);
	void Teleport(CVector v);
	void PreRender(void);
	void Render(void);

	// from CPhysical
	int32 ProcessEntityCollision(CEntity *ent, CColPoint *colpoints);

	// from CVehicle
	void ProcessControlInputs(uint8);
	void GetComponentWorldPosition(int32 component, CVector &pos);
	bool IsComponentPresent(int32 component);
	void SetComponentRotation(int32 component, CVector rotation);
	void OpenDoor(int32 component, eDoors door, float openRatio);
	void ProcessOpenDoor(uint32, uint32, float);
	bool IsDoorReady(eDoors door);
	bool IsDoorFullyOpen(eDoors door);
	bool IsDoorClosed(eDoors door);
	bool IsDoorMissing(eDoors door);
	bool IsDoorReady(uint32 door);
	bool IsDoorMissing(uint32 door);
	bool IsDoorClosed(uint32 door);
	bool IsDoorFullyOpen(uint32 door);
	bool IsOpenTopCar(void);
	void RemoveRefsToVehicle(CEntity *ent);
	void BlowUpCar(CEntity *ent);
	bool SetUpWheelColModel(CColModel *colModel);
	void BurstTyre(uint8 tyre, bool applyForces);
	bool IsRoomForPedToLeaveCar(uint32 component, CVector *doorOffset);
	float GetHeightAboveRoad(void);

	void FireTruckControl(void);
	void TankControl(void);
	void HydraulicControl(void);
	// SA CAutomobile::NitrousControl (0x6A3EA0). Runs the nitro bottle: reads
	// the fire key while the car is driving, spends the bottle, and pushes the
	// car. boost > 0 fills the bottle, boost < 0 empties it, 0 is the per-frame
	// update the control loop calls.
	void NitrousControl(int8 boost);
	// True while the bottle is open. Reads the state machine above, so a
	// controller can show the flame without touching the tyre field.
	bool IsNitroActive(void) const;
	// Opens or closes the bottle from outside (a peer, a script, the debug UI).
	// opening fills the state machine to "firing"; closing resets it to idle.
	void SetNitroActive(bool active);
	// SA CAutomobile::DoNitroEffect (0x6A3E60). The visual half: an exhaust
	// flame out of the two exhaust dummy positions, scaled by power.
	void DoNitroEffect(float power);
	// SA CAutomobile::StopNitroEffect (0x6A3E60). Ends the flame.
	void StopNitroEffect(void);
	void VehicleDamage(float impulse, uint16 damagedPiece);
	void ProcessBuoyancy(void);
	void DoDriveByShootings(void);
	void DoHoverSuspensionRatios(void);
	int32 RcbanditCheckHitWheels(void);
	int32 RcbanditCheck1CarWheels(CPtrList &list);
	void PlaceOnRoadProperly(void);
	void dmgDrawCarCollidingParticles(const CVector &pos, float amount);
	void AddDamagedVehicleParticles(void);
	int32 AddWheelDirtAndWater(CColPoint *colpoint, uint32 belowEffectSpeed);
	// Applies the per-wheel suspension compression the damage model and the
	// ground surface ask for: a missing wheel hangs, a burst one shakes with
	// speed, sand and rail beds push the wheel up.
	void DoBurstAndSoftGroundRatios(void);
	// Pushes the car up on its springs and dampers and rolls it back onto its
	// wheels when it is resting on its side at low speed.
	void ProcessSuspension(void);
	// How much of the tyre's grip the wheel actually gets: the more the spring
	// is compressed, the more weight sits on it. Offroad handling makes loose
	// ground less punishing.
	float GetWheelAdhesionScale(int wheel, bool front);
	void ResetSuspension(void);
	void SetupSuspensionLines(void);
	void ScanForCrimes(void);
	void BlowUpCarsInPath(void);
	void SetBusDoorTimer(uint32 timer, uint8 type);
	void ProcessAutoBusDoors(void);
	void ProcessSwingingDoor(int32 component, eDoors door);
	void SetupDamageAfterLoad(void);
	CObject *SpawnFlyingComponent(int32 component, uint32 type);
	CObject *RemoveBonnetInPedCollision(void);
	void SetPanelDamage(int32 component, ePanels panel, bool noFlyingComponents = false);
	void SetBumperDamage(int32 component, ePanels panel, bool noFlyingComponents = false);
	void SetDoorDamage(int32 component, eDoors door, bool noFlyingComponents = false);

	// ---- Damage state API (see the block in Vehicle.h) ------------------
	//
	// The damage model has two layers: the parts (door, panel, bumper, light,
	// wheel, engine) and the geometry flags that make the model show them. The
	// game writes both from the collision code; a controller sending a state
	// needs to write both too, which is why Set*Damage below are the same
	// functions the collisions call rather than a shortcut that only moves the
	// bits.

	virtual CDamageManager *GetDamageManager(void) { return &Damage; }
	virtual const CDamageManager *GetDamageManager(void) const { return &Damage; }
	// The typed reference is what the car's own code uses; the pair above is
	// the CVehicle interface the lamp pass goes through.
	CDamageManager &GetDamage(void) { return Damage; }
	const CDamageManager &GetDamage(void) const { return Damage; }

	// Per-part setters. Each takes the part's own status, so a peer sends the
	// same values GetPartStatus reports back.
	void SetDoorStatus(eDoors door, eDoorStatus status) { Damage.SetDoorStatus(door, status); }
	eDoorStatus GetDoorStatus(eDoors door) const { return Damage.GetDoorStatus(door); }
	void SetPanelStatus(ePanels panel, uint32 status) { Damage.SetPanelStatus(panel, status); }
	int32 GetPanelStatus(ePanels panel) const { return Damage.GetPanelStatus(panel); }
	// The windscreen is a panel in the damage model (VEHPANEL_WINDSCREEN) and a
	// light at the same time - the collision code sets both. This pair keeps the
	// two writes together so a snapshot cannot carry one without the other.
	void SetWindscreenStatus(uint32 status);
	int32 GetWindscreenStatus(void) const { return Damage.GetPanelStatus(VEHPANEL_WINDSCREEN); }
	void SetLightStatus(eLights light, uint32 status) { Damage.SetLightStatus(light, status); }
	int32 GetLightStatus(eLights light) const { return Damage.GetLightStatus(light); }
	void SetWheelStatus(int32 wheel, uint32 status);
	int32 GetWheelStatus(int32 wheel) const { return Damage.GetWheelStatus(wheel); }
	void SetEngineStatus(uint32 status) { Damage.SetEngineStatus(status); }
	int32 GetEngineStatus(void) const { return Damage.GetEngineStatus(); }

	// Hydraulics. m_hydraulicState is the pump's target height, 0 (down) to 100
	// (up), and the four spring ratios are what the suspension does about it.
	// The pump itself is driven by the pad in HydraulicControl, which is why the
	// height is per instance state and the input is not stored here.
	uint16 GetHydraulicState(void) const { return m_hydraulicState; }
	void SetHydraulicState(uint16 state) { m_hydraulicState = state > 100 ? 100 : state; }
	float GetSuspensionRatio(int32 wheel) const
	{
		return wheel >= 0 && wheel < 4 ? m_aSuspensionSpringRatio[wheel] : 0.0f;
	}
	// Writes the four ratios. The suspension reads them next step, so this is
	// how a peer shows a remote car that is riding low without simulating it.
	void SetSuspensionRatio(int32 wheel, float ratio)
	{
		if(wheel >= 0 && wheel < 4)
			m_aSuspensionSpringRatio[wheel] = Clamp(ratio, 0.0f, 1.0f);
	}

	// The whole damage state in one go: the six door statuses, the seven panel
	// statuses, the four lights, the four wheels and the engine. This is what a
	// peer transmits; applying it is idempotent, so a late packet is harmless.
	struct DamageState {
		uint8 doors[MAX_DOORS];
		uint8 panels[7];	// ePanels: four wings, windscreen, two bumpers
		uint8 lights[4];	// eLights
		uint8 wheels[4];
		uint8 engine;
	};
	void GetDamageState(DamageState &out) const;
	void SetDamageState(const DamageState &in);

	void SetHeliOrientation(float orient) { m_fHeliOrientation = orient; }
	void ClearHeliOrientation(void) { m_fHeliOrientation = -1.0f; }

	void Fix(void);
	void SetComponentVisibility(RwFrame *frame, uint32 flags);
	void SetupModelNodes(void);
	void SetTaxiLight(bool light);
	bool GetAllWheelsOffGround(void);
	void HideAllComps(void);
	void ShowAllComps(void);
	void ReduceHornCounter(void);

	void PopBoot(void);
	void PopBootUsingPhysics(void);
	void CloseAllDoors(void);
	CPed *KnockPedOutCar(eWeaponType weapon, uint16 door, CPed *ped);

#ifdef COMPATIBLE_SAVES
	virtual void Save(uint8*& buf);
	virtual void Load(uint8*& buf);
#endif
	static const uint32 nSaveStructSize;

	static void SetAllTaxiLights(bool set);
};

extern CVector vecHunterGunPos;

#endif // __GTA_AUTOMOBILE_H__
