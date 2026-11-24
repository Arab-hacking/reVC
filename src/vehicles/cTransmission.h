#ifndef __GTA_CTRANSMISSION_H__
#define __GTA_CTRANSMISSION_H__

#include "tTransmissionGear.h"
#include "tVehicleHandlingFlags.h"

// Highest gear index the handling record can describe. Gear 0 is reverse, so a
// record with nNumberOfGears == MAX_TRANSMISSION_GEARS uses every slot.
enum {
	MAX_TRANSMISSION_GEARS = 6,
};

// Multipliers of the engine model. These are not tunables: they are the
// constants the reference multiplies by, named so the arithmetic in
// cTransmission.cpp stays readable.
const float TRANSMISSION_FREE_ACCELERATION = 0.1f;
const float TRANSMISSION_SMOOTHER_FRAC = 0.85f;
const float TRANSMISSION_AI_CHEAT_MULT = 1.2f;
const float TRANSMISSION_NITROS_MULT = 2.0f;
const float TRANSMISSION_AI_CHEAT_INERTIA_MULT = 0.75f;
const float TRANSMISSION_NITROS_INERTIA_MULT = 0.5f;

enum eTransmissionCheat {
	CHEAT_HANDLING_NONE = 0,
	CHEAT_HANDLING_PERFECT = 1,
	CHEAT_HANDLING_NITROS = 2,
};

class cTransmission
{
public:
	tTransmissionGear m_aGears[MAX_TRANSMISSION_GEARS];	// [0] reverse, [1..] forward
	uint8 m_nDriveType;		// 'F', 'R' or '4'
	uint8 m_nEngineType;	// 'P', 'D' or 'E'
	uint8 m_nNumberOfGears;	// 1..MAX_TRANSMISSION_GEARS-1 forward gears
	uint32 m_nHandlingFlags;	// VEHICLE_HANDLING_1G_BOOST and friends
	float m_fEngineAcceleration;
	float m_fEngineInertia;
	float m_fMaxVelocity;
	float m_fMaxFlatVelocity;	// top speed before the overspeed allowance is added
	float m_fMaxCruiseVelocity;	// AI cruise speed, derived by InitGearRatios
	float m_fMaxReverseVelocity;
	float m_fCurVelocity;

	void InitGearRatios(void);
	float CalculateDriveAcceleration(const float &gasPedal, uint8 &gear, float &time, const float &velocity,
		float *inertiaRatio, float *inertiaSmoothing, bool wheelsOnGround, eTransmissionCheat cheat);
};

#endif // __GTA_CTRANSMISSION_H__
