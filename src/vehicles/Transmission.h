#ifndef __GTA_TRANSMISSION_H__
#define __GTA_TRANSMISSION_H__

// cTransmission, as GTA:SA has it (gta-reversed, cTransmission.cpp 0x6D0460 /
// 0x6D05E0 and tTransmissionGear). The port used to carry VC's version, which
// kept a pre-baked absolute velocity table per gear and multiplied the engine
// acceleration by hand-tuned per-gear constants; SA instead derives the gear
// from the gear *number* and drives the engine through the inertia history
// below. There is no per-gear ratio array in SA — the ratio is computed from
// the current gear on the fly — and the client's handling.cfg carries no
// ratios either, so nothing is invented here.
//
// Units. The handling file is written in m/s^2 and km/h; everything below is in
// the port's per-1/50 s units, converted once in
// cHandlingDataMgr::ConvertDataToGameUnits:
//   m_EngineAcceleration *= 1/50^2          (acceleration per tick per tick)
//   m_MaxVelocity       *= 0.277778/50      (metres per tick)
//   m_fBrakeDeceleration *= 1/50^2

struct tTransmissionGear
{
	float MaxVelocity;        // top of this gear, metres per tick
	float ChangeUpVelocity;   // above this the gearbox shifts up
	float ChangeDownVelocity; // below this it shifts down
};

class cTransmission
{
public:
	// Gear 0 is reverse, 1..5 forward (SA has at most 6).
	tTransmissionGear Gears[6];
	uint8 nDriveType;      // 'F' / 'R' / '4'
	uint8 nEngineType;     // 'P' / 'D' / 'E'
	uint8 nNumberOfGears;  // 1..6

	// SA keeps the handling flags on the transmission block: the 1G/2G boost
	// bits are read from here while driving.
	uint32 HandlingFlags;

	float fEngineAcceleration;
	float fEngineInertia;
	float fMaxVelocity;      // total (after the 1.2 factor)
	float fMaxFlatVelocity;  // drag-limited flat speed before the 1.2 factor
	float fMaxReverseVelocity;
	float fCurVelocity;
	// The autopilot's cruise speed. SA carries it on the vehicle's handling
	// record as m_fMaxFlatVelocity; the AI code here reads it by this name, so
	// it is kept alongside and filled with the same value.
	float fMaxCruiseVelocity;

	void InitGearRatios(void);
	void CalculateGearForSimpleCar(float speed, uint8 &gear);

	// a6/a7 are SA's inertia history, owned by the vehicle (m_fInertiaValue1/2):
	// a6 is the previous gear-position ratio, a7 the damped acceleration
	// multiplier carried between frames. They may be null, in which case the
	// engine reaches its gear target instantly.
	float CalculateDriveAcceleration(const float &gasPedal, uint8 &gear, float &velocity,
	                                 float *a6, float *a7, uint8 wheelsOnGround, bool nitros);
};

#endif // __GTA_TRANSMISSION_H__
