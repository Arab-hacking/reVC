#ifndef __GTA_TRANSMISSION_H__
#define __GTA_TRANSMISSION_H__

struct tGear
{
	float fMaxVelocity;
	float fShiftUpVelocity;
	float fShiftDownVelocity;
};

enum eTransmissionCheat {
	TRANSMISSION_CHEAT_NONE,
	TRANSMISSION_CHEAT_PERFECT,
	TRANSMISSION_CHEAT_NITROS,
};
enum {
	TRANSMISSION_1G_BOOST = 1,
	TRANSMISSION_2G_BOOST = 2,
};

class cTransmission
{
public:
	// Gear 0 is reverse, 1-5 are forward
	tGear Gears[6];
	char nDriveType;
	char nEngineType;
	int8 nNumberOfGears;
	uint32 Flags;
	float fEngineAcceleration;
	float fEngineInertia = 1.0f;
	float fMaxVelocity;
	float fMaxCruiseVelocity;
	float fMaxReverseVelocity;
	float fCurVelocity;

	void InitGearRatios(void);
	void CalculateGearForSimpleCar(float speed, uint8 &gear);
	float CalculateDriveAcceleration(const float &gasPedal, uint8 &gear, float &time, const float &velocity, float *inertiaRatio,
	    float *inertiaSmoothing, bool wheelsOnGround, eTransmissionCheat cheat);
};

#endif // __GTA_TRANSMISSION_H__
