#include "common.h"

#include "Timer.h"
#include "HandlingMgr.h"
#include "Transmission.h"

void
cTransmission::InitGearRatios(void)
{
	int i;
	float velocityDiff;

	// SA-style block: five forward ratios, reverse is gear 0. The client's
	// handling data carries the gear *count* and the top speed but no ratios, so
	// these are a linear fall-off from 1.0 (first gear) to 0.38 (top gear). The
	// absolute per-gear velocities below are what the gear-changing code and the
	// audio pitch use, and their spacing is the same as before, so shift
	// behaviour does not change; what changes is that the gear the car is in now
	// comes from the ratio, the way SA computes it, instead of VC's pre-baked
	// per-gear velocity table.
	for(i = 0; i < 5; i++)
		GearRatios[i] = 0.0f;

	for(i = 0; i < nNumberOfGears && i < 5; i++)
		GearRatios[i] = 1.0f - 0.62f*(float)i/(float)Max(nNumberOfGears - 1, 1);

	memset(Gears, 0, sizeof(Gears));

	// Absolute per-gear velocities from the ratios; the audio code still reads
	// these (it converts them into a pitch), so they have to stay meaningful.
	for(i = 1; i <= nNumberOfGears && i < 6; i++){
		Gears[i].fMaxVelocity = GearRatios[i-1] * fMaxVelocity;

		velocityDiff = Gears[i].fMaxVelocity - Gears[i-1].fMaxVelocity;

		if(i >= nNumberOfGears){
			Gears[i].fShiftUpVelocity = fMaxVelocity;
		}else{
			Gears[i+1].fShiftDownVelocity = velocityDiff*0.42f + Gears[i-1].fMaxVelocity;
			Gears[i].fShiftUpVelocity = velocityDiff*0.6667f + Gears[i-1].fMaxVelocity;
		}
	}

	// Reverse gear
	Gears[0].fMaxVelocity = fMaxReverseVelocity;
	Gears[0].fShiftUpVelocity = -0.01f;
	Gears[0].fShiftDownVelocity = fMaxReverseVelocity;

	Gears[1].fShiftDownVelocity = -0.01f;
}

void
cTransmission::CalculateGearForSimpleCar(float speed, uint8 &gear)
{
	static tGear *pGearRatio;

	pGearRatio = &Gears[gear];
	fCurVelocity = speed;
	if(speed > pGearRatio->fShiftUpVelocity)
		gear++;
	else if(speed < pGearRatio->fShiftDownVelocity){
		if(gear - 1 < 0)
			gear = 0;
		else
			gear--;
	}
}

float
cTransmission::CalculateDriveAcceleration(const float &gasPedal, uint8 &gear, float &time, const float &velocity, bool cheat)
{
	static float fAcceleration = 0.0f;
	static float fVelocity;
	static float fCheat;
	static tGear *pGearRatio;

	fVelocity = velocity;
	if(fVelocity < fMaxReverseVelocity){
		fVelocity = fMaxReverseVelocity;
		return 0.0f;
	}
	if(fVelocity > fMaxVelocity){
		fVelocity = fMaxVelocity;
		return 0.0f;
	}
	fCurVelocity = fVelocity;

	assert(gear <= nNumberOfGears);

	pGearRatio = &Gears[gear];
	if(fVelocity > pGearRatio->fShiftUpVelocity){
		if(gear != 0 || gasPedal > 0.0f){
			gear++;
			return CalculateDriveAcceleration(gasPedal, gear, time, fVelocity, false);
		}
	}else if(fVelocity < pGearRatio->fShiftDownVelocity && gear != 0){
		if(gear != 1 || gasPedal < 0.0f){
			gear--;
			return CalculateDriveAcceleration(gasPedal, gear, time, fVelocity, false);
		}
	}

	float speedMul, accelMul;

	if(gear < 1){
		// going reverse
		accelMul = (Flags & HANDLING_2G_BOOST) ? 2.0f : 1.0f;
		speedMul = -1.0f;
	}else if(nNumberOfGears <= 1){
		accelMul = 1.0f;
		speedMul = 1.0f;
	}else{
		// The gear ratio drives the force: this is the SA model, where the
		// acceleration available in a gear is the engine acceleration multiplied
		// by that gear's ratio, instead of VC's hand-tuned per-gear multipliers.
		speedMul = 3.0f*SQR(GearRatios[gear-1]) + 1.0f;
		if(Flags & HANDLING_2G_BOOST){
			accelMul = (GearRatios[gear-1] > 0.9f) ? ((Flags & HANDLING_1G_BOOST) ? 2.0f : 1.6f) : 1.0f;
		}else if(Flags & HANDLING_1G_BOOST && GearRatios[gear-1] > 0.9f){
			accelMul = 2.0f;
		}else
			accelMul = 1.0f;
	}

	// fEngineInertia damps how fast the engine can reach a new gear's target: 0
	// is "no damping" (the file's 5.0 default behaves like the old code), high
	// values make the car feel heavier in the lower gears.
	float inertiaMul = 1.0f;
	if(fEngineInertia > 0.0f)
		inertiaMul = 1.0f/(1.0f + fEngineInertia*0.004f);
	accelMul *= inertiaMul;

	if(cheat)
		fCheat = 1.2f;
	else
		fCheat = 1.0f;
	float targetVelocity = Gears[gear].fMaxVelocity*speedMul*fCheat;
	float accel = (targetVelocity - fVelocity) * (fEngineAcceleration*accelMul) / Abs(targetVelocity);
	if(Abs(fVelocity) < Abs(Gears[gear].fMaxVelocity*fCheat))
		fAcceleration = gasPedal * accel * CTimer::GetTimeStep();
	else
		fAcceleration = 0.0f;
	return fAcceleration;
}
