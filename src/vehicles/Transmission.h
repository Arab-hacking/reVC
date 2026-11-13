#ifndef __GTA_TRANSMISSION_H__
#define __GTA_TRANSMISSION_H__

// SA keeps the per-gear ratios inline in the transmission block (five forward
// gears plus reverse), where VC precomputed absolute velocities per gear. The
// struct keeps the VC field names because the audio code still reads them, but
// InitGearRatios derives them from the ratios below.
struct tGear
{
	float fMaxVelocity;		// top speed of this gear (m/s, port units)
	float fShiftUpVelocity;		// upshift point
	float fShiftDownVelocity;	// downshift point
};

class cTransmission
{
public:
	// Gear 0 is reverse, 1..5 are the five forward gears the client's data has.
	tGear Gears[6];
	// SA's per-gear ratios, straight from the file: the transmission block keeps
	// 5 forward ratios. The client's handling data has no ratios of its own, so
	// these are a smooth geometric spread over nNumberOfGears; what matters is
	// that they are derived from the file's gear count and evaluated the SA way
	// (ratio * engine acceleration), not from VC's pre-baked velocities.
	float GearRatios[5];
	char nDriveType;
	char nEngineType;
	int8 nNumberOfGears;
	// VC's cTransmission kept a truncated copy of the handling flags here and
	// the gear code read its bits; the client keeps the full masks on the
	// handling record (ModelFlags/HandlingFlags), and 1G/2G boost is a bit of
	// HandlingFlags. The field mirrors that mask so the gear code can read it.
	uint32 Flags;
	// Client column 14 (fEngineInertia). SA uses it as the divisor of the
	// engine's angular acceleration; this port uses it to damp the acceleration
	// curve in CalculateDriveAcceleration.
	float fEngineInertia;
	float fEngineAcceleration;
	float fMaxVelocity;
	float fMaxCruiseVelocity;
	float fMaxReverseVelocity;
	float fCurVelocity;

	void InitGearRatios(void);
	void CalculateGearForSimpleCar(float speed, uint8 &gear);
	float CalculateDriveAcceleration(const float &gasPedal, uint8 &gear, float &time, const float &velocity, bool cheat);
};

#endif // __GTA_TRANSMISSION_H__
