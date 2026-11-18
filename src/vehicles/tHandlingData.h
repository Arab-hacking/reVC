#ifndef __GTA_THANDLINGDATA_H__
#define __GTA_THANDLINGDATA_H__

#include "cTransmission.h"
#include "tVehicleHandlingModelFlags.h"
#include "tVehicleHandlingFlags.h"

enum eVehicleLightsSize : uint8 {
	LIGHTS_LONG,
	LIGHTS_SMALL,
	LIGHTS_BIG,
	LIGHTS_TALL
};

// One line of handling.cfg. Field order and units follow the reference record:
// the parser in cHandlingDataMgr feeds it column by column, and
// ConvertDataToGameUnits() turns the file units into game units afterwards.
struct tHandlingData
{
	int32 m_nVehicleId;

	float m_fMass;				// 1.0 to 50000.0
	float m_fMassRecip;			// 1.0 / m_fMass, derived
	float m_fTurnMass;	// SA column, or derived from the VC collision box
	float m_fDragMult;
	CVector m_vecCentreOfMass;	// 1.0 to 50000.0
	uint8 m_nPercentSubmerged;	// 10 to 120, above 100 the vehicle sinks
	float m_fBuoyancyConstant;	// derived from mass and m_nPercentSubmerged

	float m_fTractionMultiplier;	// 0.5 to 2.0
	cTransmission m_Transmission;

	float m_fBrakeDeceleration;	// 0.1 to 10.0
	float m_fBrakeBias;			// 0.0 to 1.0
	int8 m_bABS;
	float m_fSteeringLock;		// 10.0 to 40.0
	float m_fTractionLoss;
	float m_fTractionBias;
	float m_fSuspensionForceLevel;
	float m_fSuspensionDampingLevel;
	float m_fSuspensionHighSpdComDamp;	// 0 or 200+ for very bouncy vehicles
	float m_fSuspensionUpperLimit;
	float m_fSuspensionLowerLimit;
	float m_fSuspensionBiasBetweenFrontAndRear;
	float m_fSuspensionAntiDiveMultiplier;
	float m_fCollisionDamageMultiplier;	// 0.2 to 5.0

	// Model flags: shape and bodywork. The bitfields are the canonical view of
	// the same word that the `MB`/`HB` handling columns carry.
	union {
		eVehicleHandlingModelFlags m_nModelFlags;
		struct {
			uint32 m_bIsVan : 1;
			uint32 m_bIsBus : 1;
			uint32 m_bIsLow : 1;
			uint32 m_bIsBig : 1;
			uint32 m_bReverseBonnet : 1;
			uint32 m_bHangingBoot : 1;
			uint32 m_bTailgateBoot : 1;
			uint32 m_bNoswingBoot : 1;

			uint32 m_bNoDoors : 1;
			uint32 m_bTandemSeats : 1;
			uint32 m_bSitInBoat : 1;
			uint32 m_bConvertible : 1;
			uint32 m_bNoExhaust : 1;
			uint32 m_bDoubleExhaust : 1;
			uint32 m_bNo1fpsLookBehind : 1;
			uint32 m_bForceDoorCheck : 1;

			uint32 m_bAxleFNotlit : 1;
			uint32 m_bAxleFSolid : 1;
			uint32 m_bAxleFMcpherson : 1;
			uint32 m_bAxleFReverse : 1;
			uint32 m_bAxleRNotlit : 1;
			uint32 m_bAxleRSolid : 1;
			uint32 m_bAxleRMcpherson : 1;
			uint32 m_bAxleRReverse : 1;

			uint32 m_bIsBike : 1;
			uint32 m_bIsHeli : 1;
			uint32 m_bIsPlane : 1;
			uint32 m_bIsBoat : 1;
			uint32 m_bBouncePanels : 1;
			uint32 m_bDoubleRwheels : 1;
			uint32 m_bForceGroundClearance : 1;
			uint32 m_bIsHatchback : 1;
		};
	};

	// Handling flags: chassis behaviour.
	union {
		eVehicleHandlingFlags m_nHandlingFlags;
		struct {
			uint32 m_b1gBoost : 1;
			uint32 m_b2gBoost : 1;
			uint32 m_bNpcAntiRoll : 1;
			uint32 m_bNpcNeutralHandl : 1;
			uint32 m_bNoHandbrake : 1;
			uint32 m_bSteerRearwheels : 1;
			uint32 m_bHbRearwheelSteer : 1;
			uint32 m_bAltSteerOpt : 1;

			uint32 m_bWheelFNarrow2 : 1;
			uint32 m_bWheelFNarrow : 1;
			uint32 m_bWheelFWide : 1;
			uint32 m_bWheelFWide2 : 1;
			uint32 m_bWheelRNarrow2 : 1;
			uint32 m_bWheelRNarrow : 1;
			uint32 m_bWheelRWide : 1;
			uint32 m_bWheelRWide2 : 1;

			uint32 m_bHydraulicGeom : 1;
			uint32 m_bHydraulicInst : 1;
			uint32 m_bHydraulicNone : 1;
			uint32 m_bNosInst : 1;
			uint32 m_bOffroadAbility : 1;
			uint32 m_bOffroadAbility2 : 1;
			uint32 m_bHalogenLights : 1;
			uint32 m_bProcRearwheelFirst : 1;

			uint32 m_bUseMaxspLimit : 1;
			uint32 m_bLowRider : 1;
			uint32 m_bStreetRacer : 1;
			uint32 m_bSwingingChassis : 1;
		};
	};

	float m_fSeatOffsetDistance;	// ped seat position offset towards centre of car
	int32 m_nMonetaryValue;		// 1 to 100000
	eVehicleLightsSize m_nFrontLights;
	eVehicleLightsSize m_nRearLights;
	uint8 m_nAnimGroup;

	// Vice City records carry the collision box instead of a drag multiplier
	// and a turn mass. It is kept so ConvertVCDataToGameUnits() can solve the
	// VC drag curve; SA loads leave it at zero.
	CVector m_vecVCDimension;

};

#endif // __GTA_THANDLINGDATA_H__
