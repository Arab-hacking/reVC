#ifndef __GTA_HANDLINGMGR_H__
#define __GTA_HANDLINGMGR_H__

#include "Transmission.h"

// Handling data, read the way the Black Russia client reads it.
//
// Source: VEHICLES-v3, chapter 2, function HandlingMgr::LoadHandlingData
// @ 0x1804c78 (947 instructions) and the data file the client ships
// (data/handling.cfg: 567 cars, 16 boats, 23 bikes, 24 flying, 2 cranes).
// The 597 the chapter quotes is a different build; on this file it was
// inflated by the UTF-8 BOM line, see LoadHandlingData.
//
// Deliberately NOT a Vice City parser any more:
//   * there is no fixed list of 106 VC handling names. A record is found by the
//     name column that every record stores, so a custom handling.cfg with any
//     names works and the ids are simply file order;
//   * the first data columns are fMass, fTurnMass, fDragMult — VC put
//     Dimensions.x/y/z there and derived fTurnMass from them;
//   * fEngineInertia (column 14) exists and is kept;
//   * fields VC never had: suspension high speed damping, suspension upper
//     limit as a real column, the per-record anim group id and fHighLightsAngle;
//   * model flags and handling flags are two separate hex columns and, unlike
//     VC, are *not* OR-ed together into one mask.
//
// Record layouts (bytes, from the analysis of the loader):
//   car     0x104, 600 max      boat   0x54, 50 max
//   bike    0x58, 50 max        flying 0x64, 50 max
//   crane   0x8c, std::vector on the client (fixed array here)

// Game-side limits, taken from the loader's own comparisons:
//   0x1804eec  cmp w26, #0x258   600     > "hard limit for number of handlings"
//   0x1804e90/0x1805278/0x1805064 cmp w26, #0x32   50 for boats/flying/bikes
// Same meaning as VC's NUMHANDLINGS/NUMBIKEHANDLINGS/... but these no longer
// index a fixed name table.
#define HANDLING_MAX_CARS    600
#define HANDLING_MAX_BIKES   50
#define HANDLING_MAX_FLYING  50
#define HANDLING_MAX_BOATS   50

// Flags of the two hex columns. Names and bit values follow the field
// description block at the top of data/handling.cfg: the file writes them as
// hex digits, one nibble per group, so a bit is the digit's value inside its
// nibble (1, 2, 4, 8).
enum eModelFlags
{
	// 1st digit
	MODEL_IS_VAN               = 0x0000001,
	MODEL_IS_BUS               = 0x0000002,
	MODEL_IS_LOW               = 0x0000004,
	MODEL_IS_BIG               = 0x0000008,
	// 2nd digit
	MODEL_REVERSE_BONNET       = 0x0000010,
	MODEL_HANGING_BOOT         = 0x0000020,
	MODEL_TAILGATE_BOOT        = 0x0000040,
	MODEL_NOSWING_BOOT         = 0x0000080,
	// 3rd digit
	MODEL_NO_DOORS             = 0x0000100,
	MODEL_TANDEM_SEATS         = 0x0000200,
	MODEL_SIT_IN_BOAT          = 0x0000400,
	MODEL_CONVERTIBLE          = 0x0000800,
	// 4th digit
	MODEL_NO_EXHAUST           = 0x0001000,
	MODEL_DOUBLE_EXHAUST       = 0x0002000,
	MODEL_NO1FPS_LOOK_BEHIND   = 0x0004000,
	MODEL_FORCE_DOOR_CHECK     = 0x0008000,
	// 5th digit
	MODEL_AXLE_F_NOTILT        = 0x0010000,
	MODEL_AXLE_F_SOLID         = 0x0020000,
	MODEL_AXLE_F_MCPHERSON     = 0x0040000,
	MODEL_AXLE_F_REVERSE       = 0x0080000,
	// 6th digit
	MODEL_AXLE_R_NOTILT        = 0x0100000,
	MODEL_AXLE_R_SOLID         = 0x0200000,
	MODEL_AXLE_R_MCPHERSON     = 0x0400000,
	MODEL_AXLE_R_REVERSE       = 0x0800000,
	// 7th digit
	MODEL_IS_BIKE              = 0x1000000,
	MODEL_IS_HELI              = 0x2000000,
	MODEL_IS_PLANE             = 0x4000000,
	MODEL_IS_BOAT              = 0x8000000,
	// 8th digit
	MODEL_BOUNCE_PANELS        = 0x10000000,
	MODEL_DOUBLE_RWHEELS       = 0x20000000,
	MODEL_FORCE_GROUND_CLEARANCE = 0x40000000,
	MODEL_IS_HATCHBACK         = 0x80000000,
};

enum eHandlingFlags
{
	// 1st digit
	HANDLING_1G_BOOST             = 0x0000001,
	HANDLING_2G_BOOST             = 0x0000002,
	HANDLING_NPC_ANTI_ROLL        = 0x0000004,
	HANDLING_NPC_NEUTRAL_HANDL    = 0x0000008,
	// 2nd digit
	HANDLING_NO_HANDBRAKE         = 0x0000010,
	HANDLING_STEER_REARWHEELS     = 0x0000020,
	HANDLING_HB_REARWHEEL_STEER   = 0x0000040,
	HANDLING_ALT_STEER_OPT        = 0x0000080,
	// 3rd digit
	HANDLING_WHEEL_F_NARROW2      = 0x0000100,
	HANDLING_WHEEL_F_NARROW       = 0x0000200,
	HANDLING_WHEEL_F_WIDE         = 0x0000400,
	HANDLING_WHEEL_F_WIDE2        = 0x0000800,
	// 4th digit
	HANDLING_WHEEL_R_NARROW2      = 0x0001000,
	HANDLING_WHEEL_R_NARROW       = 0x0002000,
	HANDLING_WHEEL_R_WIDE         = 0x0004000,
	HANDLING_WHEEL_R_WIDE2        = 0x0008000,
	// 5th digit
	HANDLING_HYDRAULIC_GEOM       = 0x0010000,
	HANDLING_HYDRAULIC_INST       = 0x0020000,
	HANDLING_HYDRAULIC_NONE       = 0x0040000,
	HANDLING_NOS_INST             = 0x0080000,
	// 6th digit
	HANDLING_OFFROAD_ABILITY      = 0x0100000,
	HANDLING_OFFROAD_ABILITY2     = 0x0200000,
	HANDLING_HALOGEN_LIGHTS       = 0x0400000,
	HANDLING_PROC_REARWHEEL_1ST   = 0x0800000,
	// 7th digit
	HANDLING_USE_MAXSP_LIMIT      = 0x1000000,
	HANDLING_LOW_RIDER            = 0x2000000,
	HANDLING_STREET_RACER         = 0x4000000,
	HANDLING_OPENING_HEADLIGHTS   = 0x8000000,
	// 8th digit
	HANDLING_ACTIVE_SPOILER       = 0x10000000,
};

struct tHandlingData
{
	// The client keeps the name in the record (+0x00, 24 bytes) and the ordinal
	// in +0x18. Both are here: the name is what the record is looked up by.
	char     Name[24];
	int32    nIndex;

	// Column -> field, from the client's jump table (chapter 2.7 and the
	// per-column dump in the analysis data). Note that the file order and the
	// record order differ: columns 4..6 are the centre of mass, columns 9 and 10
	// jump back to traction loss/bias, and columns 21/22 are the suspension
	// levels, which VC had in the middle of the block.
	float    fMass;                    // col 1  +0x1c
	float    fTurnMass;                // col 2  +0x24
	float    fDragMult;                // col 3  +0x28
	CVector  CentreOfMass;             // cols 4..6 +0x2c/+0x30/+0x34
	int32    nPercentSubmerged;        // col 7  u8  +0x38
	float    fTractionMultiplier;      // col 8  +0x40
	float    fTractionLoss;            // col 9  +0xbc
	float    fTractionBias;            // col 10 +0xc0
	cTransmission Transmission;        // cols 11..16
	//   nNumberOfGears      col 11 u8   +0x8e
	//   fMaxVelocity        col 12      transmission block
	//   fEngineAcceleration col 13
	//   fEngineInertia      col 14
	//   nDriveType          col 15 char +0x8c
	//   nEngineType         col 16 char +0x8d
	float    fBrakeDeceleration;       // col 17 +0x94
	float    fBrakeBias;               // col 18 +0x98
	bool     bABS;                     // col 19 u8  +0xb4
	float    fSteeringLock;            // col 20 +0x9c
	float    fSuspensionForceLevel;    // col 21 +0xc8
	float    fSuspensionDampingLevel;  // col 22 +0xcc
	float    fSuspensionUpperLimit;    // col 24 +0xd0
	float    fSuspensionLowerLimit;    // col 25 +0xd4
	float    fSuspensionBias;          // col 26 +0xd8
	float    fSuspensionAntidiveMultiplier; // col 27 +0xdc
	float    fSeatOffsetDistance;      // col 28 +0xec
	float    fCollisionDamageMultiplier; // col 29 +0xe0
	int32    nMonetaryValue;           // col 30 i32 +0xf0
	uint32   ModelFlags;               // col 31 hex +0xe4
	uint32   HandlingFlags;            // col 32 hex -> +0x90
	uint8    FrontLights;              // col 33 u8  +0xf4
	uint8    RearLights;               // col 34 u8  +0xf5
	float    fHighLightsAngle;         // col 36 +0xf8 (radians in the file)
	uint8    nAnimGroup;               // col 37 u8  +0xfc
	float    fSuspensionHighSpdComDamp;// col 38 +0x100 (VC never had it)
	// Column 23 and column 35 have no record field in the client either: their
	// jump table entries point straight back at the token loop.

	// Derived values, computed once after the file is read; not columns.
	float    fInvMass;
	float    fBuoyancy;
	// VC derived the air-resistance factor from Dimensions.x*Dimensions.z and the
	// centre-of-mass offset from Dimension.z. The client file has no dimensions;
	// fDragMult carries the drag and these two keep the call sites honest about
	// where the numbers come from now.
	float    fAirResistanceFactor;
	float    fApproxHeight;
};

struct tBikeHandlingData
{
	char     Name[24];
	int32    nIndex;
	// cols 1..15, linear, +0x1c .. +0x54
	float    fLeanFwdCOM;
	float    fLeanFwdForce;
	float    fLeanBakCOM;
	float    fLeanBackForce;
	float    fMaxLean;
	float    fFullAnimLean;
	float    fDesLean;
	float    fSpeedSteer;
	float    fSlipSteer;
	float    fNoPlayerCOMz;
	float    fWheelieAng;
	float    fStoppieAng;
	float    fWheelieSteer;
	float    fWheelieStabMult;
	float    fStoppieStabMult;
};

struct tBoatHandlingData
{
	char     Name[24];
	int32    nIndex;
	// cols 1..14, +0x1c .. +0x50, with the last column landing back inside at
	// +0x38 (between columns 7 and 8). Assigned by name, so the oddity does not
	// leak into the code.
	float    fThrustY;
	float    fThrustZ;
	float    fThrustAppZ;
	float    fAqPlaneForce;
	float    fAqPlaneLimit;
	float    fAqPlaneOffset;
	float    fWaveAudioMult;
	float    fLook_L_R_BehindCamHeight;
	CVector  vecMoveRes;
	CVector  vecTurnRes;
};

struct tFlyingHandlingData
{
	char     Name[24];
	int32    nIndex;
	// cols 1..11 and 15..21; columns 12, 13 and 14 are parsed and dropped by the
	// client (their table entries point at the loop head).
	float    fThrust;
	float    fThrustFallOff;
	float    fYaw;
	float    fYawStab;
	float    fSideSlip;
	float    fRoll;
	float    fRollStab;
	float    fPitch;
	float    fPitchStab;
	float    fFormLift;
	float    fAttackLift;
	CVector  vecTurnRes;
	CVector  vecSpeedRes;
	// Column 21 (+0x60) is a scalar the client stores and nothing in this port
	// read before; the flight model needs a per-axis damping multiplier where VC
	// used one float, and this is the only scalar left in the flying record, so
	// it serves as that "move resistance" until the flight model is rewritten.
	float    fMoveRes;
};

// Cranes: one sscanf of 29 fields on the client, std::vector storage.
struct tCraneHandlingData
{
	char     Name[24];
	int32    nIndex;
	float    Values[27];
	int32    nValue;
};

class CVehicle;

class cHandlingDataMgr
{
	float field_0;	// unused it seems
public:
	float fWheelFriction;	// wheel related
private:
	float field_8;	//
	float field_C;	// unused it seems
	float field_10;	//

public:
	cHandlingDataMgr(void);
	void Initialise(void);
	void LoadHandlingData(void);
	void ConvertDataToGameUnits(tHandlingData *handling);
	void ConvertBikeDataToGameUnits(tBikeHandlingData *handling);

	// Lookup by name, like the client's own name search: records keep the name
	// they were read with. Returns -1 when the name is not in the file; the
	// caller decides what to fall back to.
	int32 GetHandlingId(const char *name);
	int32 GetBikeHandlingId(const char *name);
	int32 GetFlyingHandlingId(const char *name);
	int32 GetBoatHandlingId(const char *name);
	int32 GetCraneHandlingId(const char *name);

	tHandlingData *GetHandlingData(int32 id)
	{
		if(id < 0 || id >= (int32)NumCars) return &HandlingData[0];
		return &HandlingData[id];
	}
	// The client's GetBikePointer/GetFlyingPointer/GetBoatPointer take a *car*
	// record id, take that record's name and return the record of their own kind
	// that has the same name (0x18064e4 / 0x18060e0 / 0x18062e4: umaddl by 0x104
	// into the car array, then strncmp(bikeRecord.name, carRecord.name, 24)).
	// The first record of the kind is the fallback when the name is absent.
	tBikeHandlingData *GetBikePointer(int32 id)
	{
		if(NumBikes == 0) return &BikeHandlingData[0];
		if(id >= 0 && id < (int32)NumCars){
			const char *name = HandlingData[id].Name;
			for(uint32 i = 0; i < NumBikes; i++)
				if(strncmp(BikeHandlingData[i].Name, name, sizeof(HandlingData[id].Name)) == 0)
					return &BikeHandlingData[i];
		}
		return &BikeHandlingData[0];
	}
	tFlyingHandlingData *GetFlyingPointer(int32 id)
	{
		if(NumFlying == 0) return &FlyingHandlingData[0];
		if(id >= 0 && id < (int32)NumCars){
			const char *name = HandlingData[id].Name;
			for(uint32 i = 0; i < NumFlying; i++)
				if(strncmp(FlyingHandlingData[i].Name, name, sizeof(HandlingData[id].Name)) == 0)
					return &FlyingHandlingData[i];
		}
		return &FlyingHandlingData[0];
	}
	tBoatHandlingData *GetBoatPointer(int32 id)
	{
		if(NumBoats == 0) return &BoatHandlingData[0];
		if(id >= 0 && id < (int32)NumCars){
			const char *name = HandlingData[id].Name;
			for(uint32 i = 0; i < NumBoats; i++)
				if(strncmp(BoatHandlingData[i].Name, name, sizeof(HandlingData[id].Name)) == 0)
					return &BoatHandlingData[i];
		}
		return &BoatHandlingData[0];
	}
	tCraneHandlingData *GetCranePointer(int32 id)
	{
		if(id < 0 || id >= (int32)NumCranes) return &CraneHandlingData[0];
		return &CraneHandlingData[id];
	}

	bool HasRearWheelDrive(int32 id) { return GetHandlingData(id)->Transmission.nDriveType != 'F'; }
	bool HasFrontWheelDrive(int32 id) { return GetHandlingData(id)->Transmission.nDriveType != 'R'; }

	// Numbers actually read from the file. The arrays are sized at the client's
	// own limits so an id taken from a save or a script can never run past them.
	uint32 NumCars, NumBikes, NumFlying, NumBoats, NumCranes;

	// Diagnostics: how many model names failed to resolve, for the log.
	void ReportMissing(void) { nMissing++; }
	int32 CountMissing(void) const { return nMissing; }

private:
	tHandlingData       HandlingData[HANDLING_MAX_CARS];
	tBikeHandlingData   BikeHandlingData[HANDLING_MAX_BIKES];
	tFlyingHandlingData FlyingHandlingData[HANDLING_MAX_FLYING];
	tBoatHandlingData   BoatHandlingData[HANDLING_MAX_BOATS];

	// The client keeps cranes in a std::vector; a fixed array is used here
	// because the size is bounded anyway and the manager lives for the session.
	tCraneHandlingData  CraneHandlingData[HANDLING_MAX_CARS];

	int32 nMissing;
};
extern cHandlingDataMgr mod_HandlingManager;

#endif // __GTA_HANDLINGMGR_H__
