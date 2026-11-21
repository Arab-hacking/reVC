#ifndef __GTA_GARAGES_H__
#define __GTA_GARAGES_H__
#include "audio_enums.h"
#include "Camera.h"
#include "config.h"
#include "Lists.h"
#include "Matrix.h"

class CVehicle;

enum eGarageState
{
	GS_FULLYCLOSED,
	GS_OPENED,
	GS_CLOSING,
	GS_OPENING,
	GS_OPENEDCONTAINSCAR,
	GS_CLOSEDCONTAINSCAR,
	GS_AFTERDROPOFF,
};

enum eGarageType
{
	GARAGE_NONE,
	GARAGE_MISSION,
	GARAGE_BOMBSHOP1,
	GARAGE_BOMBSHOP2,
	GARAGE_BOMBSHOP3,
	GARAGE_RESPRAY,
	GARAGE_COLLECTORSITEMS,
	GARAGE_COLLECTSPECIFICCARS,
	GARAGE_COLLECTCARS_1,
	GARAGE_COLLECTCARS_2,
	GARAGE_COLLECTCARS_3,
	GARAGE_FORCARTOCOMEOUTOF,
	GARAGE_60SECONDS,
	GARAGE_CRUSHER,
	GARAGE_MISSION_KEEPCAR,
	GARAGE_FOR_SCRIPT_TO_OPEN,
	GARAGE_HIDEOUT_ONE,
	GARAGE_HIDEOUT_TWO,
	GARAGE_HIDEOUT_THREE,
	GARAGE_FOR_SCRIPT_TO_OPEN_AND_CLOSE,
	GARAGE_KEEPS_OPENING_FOR_SPECIFIC_CAR,
	GARAGE_MISSION_KEEPCAR_REMAINCLOSED,
	GARAGE_COLLECTCARS_4,
	GARAGE_FOR_SCRIPT_TO_OPEN_FOR_CAR,
	GARAGE_HIDEOUT_FOUR,
	GARAGE_HIDEOUT_FIVE,
	GARAGE_HIDEOUT_SIX,
	GARAGE_HIDEOUT_SEVEN,
	GARAGE_HIDEOUT_EIGHT,
	GARAGE_HIDEOUT_NINE,
	GARAGE_HIDEOUT_TEN,
	GARAGE_HIDEOUT_ELEVEN,
	GARAGE_HIDEOUT_TWELVE
};

enum
{
	TOTAL_COLLECTCARS_GARAGES = 4,
	TOTAL_HIDEOUT_GARAGES = 12,
	TOTAL_COLLECTCARS_CARS = 6
};

// SA CStoredCar (Garage.h, 0x40 bytes), ported from gta-reversed.
//
// One car as a garage remembers it while the player is away. SA keeps the
// model, where it was, and everything that makes the car what it is: the
// colours (four of them, since SA paints four material slots), the upgrades
// fitted to it, the paintjob, the nitrous level, the extras it was built with
// and the bomb. The heading is stored as three packed bytes rather than as a
// vector, because a car only ever yaws and one byte per axis is all SA kept.
//
// StoreCar/RestoreCar are the only things that touch a vehicle; everything else
// is plain data, which is what lets the whole array be written into a save
// file as-is.
class CStoredCar
{
public:
	enum {
		FLAG_BULLETPROOF = 0x1,
		FLAG_FIREPROOF = 0x2,
		FLAG_EXPLOSIONPROOF = 0x4,
		FLAG_COLLISIONPROOF = 0x8,
		FLAG_MELEEPROOF = 0x10,
		// SA keeps the proofing flags in their own word and puts the extras in
		// this one: whether the car keeps its bombs, and which doors it has.
		FLAG_HAS_BOMB = 0x20,
		FLAG_NO_DOORS = 0x40,
	};

	// Number of upgrade slots SA stores per car. A car never has more than this
	// many parts fitted, and the array is written to the save file verbatim.
	//
	// CVehicle::m_anUpgrades is 18 entries wide because it is indexed by
	// eVehicleMod, which also covers the replacement parts. The fifteen slots
	// the save format has hold eVehicleMod 0..14 - every part a shop can sell,
	// hydraulics, nitrous and stereo included - so nothing buyable is lost by
	// keeping the file's own width.
	enum { NUM_CAR_MODS = 15 };

	CVector m_vPosn;
	uint32 m_nHandlingFlags;
	uint8 m_nStoredCarFlags;
	uint8 _pad0;
	uint16 m_wModelIndex;
	int16 m_awCarMods[NUM_CAR_MODS];
	uint8 m_nPrimaryColor;
	uint8 m_nSecondaryColor;
	uint8 m_nTertiaryColor;
	uint8 m_nQuaternaryColor;
	uint8 m_nRadiostation;
	uint8 m_anCompsToUse[2];
	uint8 m_nBombType;
	uint8 m_nPaintJob;
	uint8 m_nNitroBoosts;
	uint8 m_nPackedForwardX;
	uint8 m_nPackedForwardY;
	uint8 m_nPackedForwardZ;
	uint8 _pad1;

	void Init(void) { m_wModelIndex = 0; }
	void Clear(void) { m_wModelIndex = 0; }
	bool HasCar(void) const { return m_wModelIndex != 0; }
	const CStoredCar &operator=(const CStoredCar& other);
	void StoreCar(CVehicle*);
	CVehicle* RestoreCar();

	// The save file stores a car as a fixed 40-byte record, field by field. It
	// is not a memcpy of this structure: the record was written by the original
	// game, so its layout is a file format and cannot follow the structure when
	// the structure changes.
	enum { SAVE_RECORD_SIZE = 0x40 };
	void WriteSaveRecord(uint8 *&buf) const;
	void ReadSaveRecord(uint8 *&buf);
};

VALIDATE_SIZE(CStoredCar, 0x40);

// SA CVehicleSaveStructure (0x68 bytes), ported from gta-reversed
// (source/game_sa/VehicleSaveStructure.{h,cpp}).
//
// The part of a vehicle SA writes into a save: where it is and how it is
// pointing, its four colours, the alarm, how many seats it has, what the driver
// was doing with the pedals, and the two words of vehicle flags. Construct
// copies a live vehicle into the structure, Extract pushes it back; that pair
// is the whole class, and the save code does the serialising.
class CVehicleSaveStructure
{
public:
	CMatrix m_matrix;
	uint8 field_48;
	uint8 m_nPrimaryColor;
	uint8 m_nSecondaryColor;
	uint8 m_nTertiaryColor;
	uint8 m_nQuaternaryColor;
	uint16 m_nAlarmState;
	uint8 m_nMaxPassengers;
	float m_fSteerAngle;
	float m_fGasPedal;
	float m_fBrakePedal;
	uint32 m_nVehicleUpperFlags;
	uint32 m_nVehicleLowerFlags;

	// SA CVehicleSaveStructure::Construct (0x5D2330).
	void Construct(CVehicle *v);
	// SA CVehicleSaveStructure::Extract (0x5D23B0).
	void Extract(CVehicle *v);
};
VALIDATE_SIZE(CVehicleSaveStructure, 0x68);

#define SWITCH_GARAGE_DISTANCE_CLOSE 40.0f

class CGarage
{
public:
	uint8 m_eGarageType;
	uint8 m_eGarageState;
	uint8 m_nMaxStoredCars;
	bool field_2; // unused
	bool m_bClosingWithoutTargetCar;
	bool m_bDeactivated;
	bool m_bResprayHappened;
	int32 m_nTargetModelIndex;
	CEntity *m_pDoor1;
	CEntity *m_pDoor2;
	uint8 m_bDoor1PoolIndex;
	uint8 m_bDoor2PoolIndex;
	bool m_bDoor1IsDummy;
	bool m_bDoor2IsDummy;
	bool m_bRecreateDoorOnNextRefresh;
	bool m_bRotatedDoor;
	bool m_bCameraFollowsPlayer;
	CVector2D m_vecCorner1;
	float m_fInfZ;
	CVector2D m_vDir1;
	CVector2D m_vDir2;
	float m_fSupZ;
	float m_fDir1Len;
	float m_fDir2Len;
	float m_fInfX;
	float m_fSupX;
	float m_fInfY;
	float m_fSupY;
	float m_fDoorPos;
	float m_fDoorHeight;
	float m_fDoor1X;
	float m_fDoor1Y;
	float m_fDoor2X;
	float m_fDoor2Y;
	float m_fDoor1Z;
	float m_fDoor2Z;
	uint32 m_nTimeToStartAction;
	uint8 m_bCollectedCarsState;
	CVehicle *m_pTarget;
	CStoredCar m_sStoredCar; // not needed

	void OpenThisGarage();
	void CloseThisGarage();
	bool IsOpen() { return m_eGarageState == GS_OPENED || m_eGarageState == GS_OPENEDCONTAINSCAR; }
	bool IsClosed() { return m_eGarageState == GS_FULLYCLOSED; }
	bool IsUsed() { return m_eGarageType != GARAGE_NONE; }
	void Update();
	float GetGarageCenterX() { return (m_fInfX + m_fSupX) / 2; }
	float GetGarageCenterY() { return (m_fInfY + m_fSupY) / 2; }
	bool IsFar()
	{ 
#ifdef FIX_BUGS
		return Abs(TheCamera.GetPosition().x - GetGarageCenterX()) > SWITCH_GARAGE_DISTANCE_CLOSE ||
			Abs(TheCamera.GetPosition().y - GetGarageCenterY()) > SWITCH_GARAGE_DISTANCE_CLOSE;
#else
		return Abs(TheCamera.GetPosition().x - m_fInfX) > SWITCH_GARAGE_DISTANCE_CLOSE ||
			Abs(TheCamera.GetPosition().y - m_fInfY) > SWITCH_GARAGE_DISTANCE_CLOSE;
#endif
	}
	void TidyUpGarageClose();
	void TidyUpGarage();
	void RefreshDoorPointers(bool);
	void UpdateCrusherAngle();
	void UpdateDoorsHeight();
	bool IsEntityEntirelyInside3D(CEntity*, float);
	bool IsEntityEntirelyOutside(CEntity*, float);
	float CalcDistToGarageRectangleSquared(float, float);
	float CalcSmallestDistToGarageDoorSquared(float, float);
	bool IsAnyOtherCarTouchingGarage(CVehicle* pException);
	bool IsStaticPlayerCarEntirelyInside();
	bool IsPlayerOutsideGarage();
	bool IsAnyCarBlockingDoor();
	void CenterCarInGarage(CVehicle*);
	bool DoesCraigNeedThisCar(int32);
	bool MarkThisCarAsCollectedForCraig(int32);
	bool HasCraigCollectedThisCar(int32);
	bool IsGarageEmpty();
	void UpdateCrusherShake(float, float);
	int32 CountCarsWithCenterPointWithinGarage(CEntity* pException);
	void RemoveCarsBlockingDoorNotInside();
	void StoreAndRemoveCarsForThisHideout(CStoredCar*, int32);
	bool RestoreCarsForThisHideout(CStoredCar*);
	bool IsEntityTouching3D(CEntity*);
	bool EntityHasASphereWayOutsideGarage(CEntity*, float);
	bool IsAnyOtherPedTouchingGarage(CPed* pException);
	void BuildRotatedDoorMatrix(CEntity*, float);
	void FindDoorsEntities();
	void FindDoorsEntitiesSectorList(CPtrList&, bool);
	void PlayerArrestedOrDied();
	bool Does60SecondsNeedThisCarAtAll(int mi);
	bool Does60SecondsNeedThisCar(int mi);
	void MarkThisCarAsCollectedFor60Seconds(int mi);
	bool IsPlayerEntirelyInsideGarage();

	bool IsPointInsideGarage(CVector);
	bool IsPointInsideGarage(CVector, float);
	void ThrowCarsNearDoorOutOfGarage(CVehicle*);

	int32 FindMaxNumStoredCarsForGarage() { return Min(NUM_GARAGE_STORED_CARS, m_nMaxStoredCars); }

};

class CGarages
{
	enum {
		MESSAGE_LENGTH = 8,
	};
public:
	static int32 BankVansCollected;
	static bool BombsAreFree;
	static bool RespraysAreFree;
	static int32 CarsCollected;
	static int32 CarTypesCollected[TOTAL_COLLECTCARS_GARAGES];
	static int32 CrushedCarId;
	static uint32 LastTimeHelpMessage;
	static int32 MessageNumberInString;
	static char MessageIDString[MESSAGE_LENGTH];
	static int32 MessageNumberInString2;
	static uint32 MessageStartTime;
	static uint32 MessageEndTime;
	static uint32 NumGarages;
	static bool PlayerInGarage;
	static int32 PoliceCarsCollected;
	static CGarage aGarages[NUM_GARAGES];
	static CStoredCar aCarsInSafeHouses[TOTAL_HIDEOUT_GARAGES][NUM_GARAGE_STORED_CARS];
	static bool bCamShouldBeOutisde;

	static void Init(void);
#ifndef PS2
	static void Shutdown(void);
#endif
	static void Update(void);

	static int16 AddOne(float X1, float Y1, float Z1, float X2, float Y2, float X3, float Y3, float Z2, uint8 type, int32 targetId);
	static void ChangeGarageType(int16, uint8, int32);
	static void PrintMessages(void);
	static void TriggerMessage(const char* text, int16, uint16 time, int16);
	static void SetTargetCarForMissonGarage(int16, CVehicle*);
	static bool HasCarBeenDroppedOffYet(int16);
	static void DeActivateGarage(int16);
	static void ActivateGarage(int16);
	static int32 QueryCarsCollected(int16);
	static bool HasImportExportGarageCollectedThisCar(int16, int8);
	static bool IsGarageOpen(int16);
	static bool IsGarageClosed(int16);
	static bool HasThisCarBeenCollected(int16, uint8);
	static void OpenGarage(int16 garage) { aGarages[garage].OpenThisGarage(); }
	static void CloseGarage(int16 garage) { aGarages[garage].CloseThisGarage(); }
	static bool HasResprayHappened(int16);
	static void SetGarageDoorToRotate(int16);
	static void SetLeaveCameraForThisGarage(int16);
	static bool IsThisCarWithinGarageArea(int16, CEntity*);
	static bool HasCarBeenCrushed(int32);
	static bool IsPointInAGarageCameraZone(CVector);
	static bool CameraShouldBeOutside(void);
	static void GivePlayerDetonator(void);
	static void PlayerArrestedOrDied(void);
	static bool IsPointWithinHideOutGarage(Const CVector&);
	static bool IsPointWithinAnyGarage(Const CVector&);
	static void SetAllDoorsBackToOriginalHeight(void);
	static void Save(uint8* buf, uint32* size);
	static void Load(uint8* buf, uint32 size);
	static bool IsModelIndexADoor(uint32 id);
	static void SetFreeBombs(bool bValue) { BombsAreFree = bValue; }
	static void SetFreeResprays(bool bValue) { RespraysAreFree = bValue; }
	static void StopCarFromBlowingUp(CAutomobile*);
	static void SetMaxNumStoredCarsForGarage(int16 garage, uint8 num) { aGarages[garage].m_nMaxStoredCars = num; }

	static bool IsCarSprayable(CVehicle*);
	static float FindDoorHeightForMI(int32);
	static void CloseHideOutGaragesBeforeSave(void);
	static int32 CountCarsInHideoutGarage(uint8);
	static int32 GetBombTypeForGarageType(uint8 type) { return type - GARAGE_BOMBSHOP1 + 1; }
	static int32 GetCarsCollectedIndexForGarageType(uint8 type)
	{
		switch (type) {
		case GARAGE_COLLECTCARS_1: return 0;
		case GARAGE_COLLECTCARS_2: return 1;
		case GARAGE_COLLECTCARS_3: return 2;
		case GARAGE_COLLECTCARS_4: return 3;
		default: assert(0);
		}
		return 0;
	}
	static int32 FindSafeHouseIndexForGarageType(uint8 type)
	{
		switch (type) {
		case GARAGE_HIDEOUT_ONE: return 0;
		case GARAGE_HIDEOUT_TWO: return 1;
		case GARAGE_HIDEOUT_THREE: return 2;
		case GARAGE_HIDEOUT_FOUR: return 3;
		case GARAGE_HIDEOUT_FIVE: return 4;
		case GARAGE_HIDEOUT_SIX: return 5;
		case GARAGE_HIDEOUT_SEVEN: return 6;
		case GARAGE_HIDEOUT_EIGHT: return 7;
		case GARAGE_HIDEOUT_NINE: return 8;
		case GARAGE_HIDEOUT_TEN: return 9;
		case GARAGE_HIDEOUT_ELEVEN: return 10;
		case GARAGE_HIDEOUT_TWELVE: return 11;
		}
		return -1;
	}
	static bool IsThisGarageTypeSafehouse(uint8 type) { return FindSafeHouseIndexForGarageType(type) >= 0; }

};

#endif // __GTA_GARAGES_H__
