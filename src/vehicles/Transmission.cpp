#include "common.h"

#include "Timer.h"
#include "HandlingMgr.h"
#include "Transmission.h"

// Constants of SA's gearbox (cTransmission.h in gta-reversed).
#define TRANSMISSION_FREE_ACCELERATION 0.1f
#define TRANSMISSION_SMOOTHER_FRAC     0.85f

// 0x6D0460
void
cTransmission::InitGearRatios(void)
{
	// SA spaces the gears evenly over the top speed: the first gear reaches
	// half a gear's worth of velocity, each following gear adds a full gear's
	// worth, and the last one lands on fMaxVelocity.
	memset(Gears, 0, sizeof(Gears));

	float averageHalfGearVelocity = 0.5f * fMaxVelocity / nNumberOfGears;
	float maxGearVelocity = fMaxVelocity - averageHalfGearVelocity;

	for(uint8 i = 1; i <= nNumberOfGears; i++){
		tTransmissionGear *gear = &Gears[i];
		tTransmissionGear *prev = &Gears[i-1];

		gear->MaxVelocity = (float)i * maxGearVelocity / nNumberOfGears + averageHalfGearVelocity;

		float velocityDifference = gear->MaxVelocity - prev->MaxVelocity;

		if(i >= nNumberOfGears){
			gear->ChangeUpVelocity = fMaxVelocity;
		}else{
			Gears[i+1].ChangeDownVelocity = 0.42f * velocityDifference + prev->MaxVelocity;
			gear->ChangeUpVelocity = 0.6667f * velocityDifference + prev->MaxVelocity;
		}
	}

	Gears[0].MaxVelocity = fMaxReverseVelocity;
	Gears[0].ChangeUpVelocity = -0.01f;
	Gears[0].ChangeDownVelocity = fMaxReverseVelocity;
	Gears[1].ChangeDownVelocity = -0.01f;
}

// 0x6D0530
void
cTransmission::CalculateGearForSimpleCar(float speed, uint8 &gear)
{
	fCurVelocity = speed;

	tTransmissionGear *g = &Gears[gear];
	if(speed > g->ChangeUpVelocity){
		if(gear < nNumberOfGears)
			gear++;
	}else if(speed < g->ChangeDownVelocity){
		if(gear > 0)
			gear--;
	}
}

// 0x6D05E0
float
cTransmission::CalculateDriveAcceleration(const float &gasPedal, uint8 &gear, float &velocity,
                                          float *a6, float *a7, uint8 wheelsOnGround, bool nitros)
{
	float currentVelocity = velocity;
	if(currentVelocity < fMaxReverseVelocity)
		return 0.0f;

	while(currentVelocity <= fMaxVelocity){
		fCurVelocity = currentVelocity;

		tTransmissionGear *g = &Gears[gear];
		bool accelerate = false;
		bool shiftToLowerGear = false;
		bool skipInertia = false;

		if(currentVelocity > g->ChangeUpVelocity){
			if(gear == 0 && gasPedal <= 0.0f)
				accelerate = true;
			else
				gear++;
		}else{
			if(currentVelocity >= g->ChangeDownVelocity || gear == 0 || (gear == 1 && gasPedal >= 0.0f))
				accelerate = true;
			shiftToLowerGear = true;
		}

		if(accelerate){
			float speedMultiplier;
			float nitrosMultiplier;

			if(nNumberOfGears == 1){
				speedMultiplier = 1.0f;
				nitrosMultiplier = 1.0f;
			}else if(gear >= 1){
				// SA: the gear's pull is the remaining fraction of the gear
				// range, squared, times 3 (5 with 1G boost, 4 with 2G boost).
				float gearNumber = 1.0f - ((float)gear - 1.0f) / ((float)nNumberOfGears - 1.0f);
				gearNumber *= gearNumber;
				if(HandlingFlags & HANDLING_1G_BOOST)
					speedMultiplier = gearNumber * 5.0f;
				else if(HandlingFlags & HANDLING_2G_BOOST)
					speedMultiplier = gearNumber * 4.0f;
				else
					speedMultiplier = gearNumber * 3.0f;
				speedMultiplier += 1.0f;
				nitrosMultiplier = 1.0f;
			}else{
				// reverse gear
				speedMultiplier = 4.5f;
				nitrosMultiplier = 1.0f;
			}

			if(nitros)
				nitrosMultiplier = 2.0f;

			// 0.4 is SA's global acceleration scale (ConvertDataToGameUnits
			// multiplies the engine acceleration by 0.4 before anything else).
			float driveAcceleration = speedMultiplier * fEngineAcceleration * nitrosMultiplier * 0.4f
			                        * gasPedal * CTimer::GetTimeStep();

			if(a6 && a7 && !skipInertia){
				if(wheelsOnGround){
					// Position of the current velocity inside the gear's span.
					float currentDownVelocityDiff;
					float upDownVelocityDiff;
					float maxVelocityChange = fMaxVelocity / (float)nNumberOfGears * (1.0f/3.0f);

					if(gear){
						if(gear == 1){
							currentDownVelocityDiff = maxVelocityChange + currentVelocity;
							upDownVelocityDiff = maxVelocityChange + Gears[1].ChangeUpVelocity;
						}else{
							currentDownVelocityDiff = currentVelocity - g->ChangeDownVelocity;
							upDownVelocityDiff = g->ChangeUpVelocity - g->ChangeDownVelocity;
						}
					}else{
						// reverse
						currentDownVelocityDiff = maxVelocityChange - currentVelocity;
						upDownVelocityDiff = maxVelocityChange - Gears[0].ChangeDownVelocity;
					}

					float velocityDiffRatio = currentDownVelocityDiff / upDownVelocityDiff;
					float inertiaMultiplier = velocityDiffRatio - *a6;
					float acceleration = 1.0f - inertiaMultiplier * fEngineInertia;
					acceleration = Clamp(acceleration, 0.1f, 1.0f);

					*a6 = velocityDiffRatio;
					*a7 = acceleration * (1.0f - TRANSMISSION_SMOOTHER_FRAC) + TRANSMISSION_SMOOTHER_FRAC * *a7;

					driveAcceleration *= *a7;
				}else{
					// Wheels in the air: the engine spins up freely.
					*a6 += Abs(gasPedal) / fEngineInertia * CTimer::GetTimeStep() * TRANSMISSION_FREE_ACCELERATION;
					*a6 = Min(*a6, 1.0f);
					*a7 = 0.1f;
				}
			}

			float gearMaxVelocity = g->MaxVelocity;
			float changeInVelocity;

			if(gearMaxVelocity >= 0.0f || currentVelocity >= gearMaxVelocity){
				if(gearMaxVelocity <= 0.0f)
					return driveAcceleration;
				if(currentVelocity <= gearMaxVelocity)
					return driveAcceleration;
				changeInVelocity = currentVelocity - gearMaxVelocity;
			}else{
				changeInVelocity = gearMaxVelocity - currentVelocity;
			}

			// Within 0.05 of the gear's top the engine cuts out smoothly.
			driveAcceleration *= (1.0f - Min(changeInVelocity / 0.05f, 1.0f));
			return driveAcceleration;
		}

		if(shiftToLowerGear)
			gear--;

		// SA drops the inertia history once the loop shifts again, so a gear
		// change inside the same frame is not damped twice.
		a6 = nil;
		a7 = nil;
		wheelsOnGround = 0;
		nitros = false;

		if(currentVelocity < fMaxReverseVelocity)
			return 0.0f;
	}

	return 0.0f;
}
