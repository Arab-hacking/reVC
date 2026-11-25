// SA transmission: reference gta-reversed cTransmission.cpp (0x6D0460, 0x6D0530,
// 0x6D05E0) at 56a67498c822c7bf6fe07f663ddef7a6be3504f1, adapted to reVC.
// Original reverse engineering: GTA Community / gta-reversed contributors.
#include "common.h"

#include "cTransmission.h"
#include "Timer.h"

// The reference keeps the per-frame acceleration and its intermediate products
// in engine-wide static slots. That is safe in SA only because the game drives
// one car at a time through this code; it is not safe here, where a remote
// vehicle may be stepped between two frames of a local one. Every value that
// is only read and written inside one call is a local below - the arithmetic is
// unchanged, only the lifetime is.

namespace {

// Guard for data that never occurs in a valid handling.cfg but can arrive from
// a hand-edited or truncated file. A rejected record produces no force instead
// of propagating NaN into the physical simulation.
bool
IsUsable(const cTransmission &t)
{
	return t.m_nNumberOfGears >= 1 && t.m_nNumberOfGears <= MAX_TRANSMISSION_GEARS - 1 &&
		IsFinite(t.m_fMaxVelocity) && t.m_fMaxVelocity > 0.0f &&
		IsFinite(t.m_fMaxReverseVelocity) && t.m_fMaxReverseVelocity < 0.0f &&
		IsFinite(t.m_fEngineAcceleration) && t.m_fEngineAcceleration >= 0.0f &&
		IsFinite(t.m_fEngineInertia) && t.m_fEngineInertia >= 0.0f;
}

}

void
cTransmission::InitGearRatios(void)
{
	int i;

	for(i = 0; i < MAX_TRANSMISSION_GEARS; i++)
		m_aGears[i].m_MaxVelocity = m_aGears[i].m_ChangeUpVelocity = m_aGears[i].m_ChangeDownVelocity = 0.0f;

	if(!IsUsable(*this))
		return;

	float averageHalfGearVelocity = 0.5f * m_fMaxVelocity / m_nNumberOfGears;
	float maxGearVelocity = m_fMaxVelocity - averageHalfGearVelocity;

	for(i = 1; i <= m_nNumberOfGears; i++){
		tTransmissionGear *gear = &m_aGears[i];
		tTransmissionGear *previousGear = &m_aGears[i - 1];

		gear->m_MaxVelocity = (float(i) * maxGearVelocity / m_nNumberOfGears) + averageHalfGearVelocity;
		float velocityDifference = gear->m_MaxVelocity - previousGear->m_MaxVelocity;
		if(i >= m_nNumberOfGears){
			gear->m_ChangeUpVelocity = m_fMaxVelocity;
		}else{
			m_aGears[i + 1].m_ChangeDownVelocity = 0.42f * velocityDifference + previousGear->m_MaxVelocity;
			gear->m_ChangeUpVelocity = 0.6667f * velocityDifference + previousGear->m_MaxVelocity;
		}
	}

	m_aGears[0].m_MaxVelocity = m_fMaxReverseVelocity;
	m_aGears[0].m_ChangeUpVelocity = -0.01f;
	m_aGears[0].m_ChangeDownVelocity = m_fMaxReverseVelocity;
	m_aGears[1].m_ChangeDownVelocity = -0.01f;
}

float
cTransmission::CalculateDriveAcceleration(const float &gasPedal, uint8 &gear, float &time, const float &velocity,
	float *inertiaRatio, float *inertiaSmoothing, bool wheelsOnGround, eTransmissionCheat cheat)
{
	// SA ignores the gear-change counter. It stays in the signature because the
	// vehicle and audio code passes it through.
	(void)time;

	if(!IsUsable(*this) || !IsFinite(gasPedal) || !IsFinite(velocity) || !IsFinite(CTimer::GetTimeStep()))
		return 0.0f;

	// Per-call state. These were file statics; nothing outside this function
	// reads them, so making them locals changes no arithmetic and makes two
	// vehicles stepped in the same frame independent of each other.
	float driveAcceleration;
	float cheatMultiplier;
	float currentVelocity;

	if(velocity < m_fMaxReverseVelocity || velocity > m_fMaxVelocity)
		return 0.0f;

	if(gear >= MAX_TRANSMISSION_GEARS)
		gear = MAX_TRANSMISSION_GEARS - 1;

	// Reference shifting loop. The iteration count is bounded so that a record
	// with inconsistent thresholds cannot spin; valid data needs at most
	// m_nNumberOfGears steps.
	for(int shift = 0; shift <= MAX_TRANSMISSION_GEARS; shift++){
		currentVelocity = velocity;
		m_fCurVelocity = currentVelocity;

		tTransmissionGear *current = &m_aGears[gear];
		bool accelerate = false;
		bool shiftToLowerGear = false;

		if(currentVelocity > current->m_ChangeUpVelocity){
			if(gear == 0 && gasPedal <= 0.0f){
				accelerate = true;
			}else{
				if(gear == MAX_TRANSMISSION_GEARS - 1)
					return 0.0f;
				gear++;
			}
		}else{
			if(currentVelocity >= current->m_ChangeDownVelocity
			|| gear == 0
			|| (gear == 1 && gasPedal >= 0.0f))
				accelerate = true;
			shiftToLowerGear = true;
		}

		if(accelerate){
			float speedMultiplier = 0.0f;
			float nitrosMultiplier = 0.0f;

			if(m_nNumberOfGears == 1){
				speedMultiplier = 1.0f;
				nitrosMultiplier = 1.0f;
			}else if(gear >= 1){
				float gearNumber = 1.0f - (float(gear) - 1.0f) / (float(m_nNumberOfGears) - 1.0f);
				gearNumber *= gearNumber;
				if(m_nHandlingFlags & VEHICLE_HANDLING_1G_BOOST)
					speedMultiplier = gearNumber * 5.0f;
				else if(m_nHandlingFlags & VEHICLE_HANDLING_2G_BOOST)
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

			cheatMultiplier = 1.0f;
			if(cheat == CHEAT_HANDLING_PERFECT)
				cheatMultiplier = TRANSMISSION_AI_CHEAT_MULT;
			else if(cheat == CHEAT_HANDLING_NITROS)
				nitrosMultiplier = TRANSMISSION_NITROS_MULT;

			driveAcceleration = speedMultiplier * (cheatMultiplier * m_fEngineAcceleration) * nitrosMultiplier *
				0.4f * gasPedal * CTimer::GetTimeStep();

			if(inertiaRatio && inertiaSmoothing){
				if(wheelsOnGround){
					float currentDownVelocityDiff = 0.0f;
					float upDownVelocityDiff = 0.0f;
					float maxVelocityChange = m_fMaxVelocity / float(m_nNumberOfGears) * (1.0f / 3.0f);

					if(gear){
						if(gear == 1){
							currentDownVelocityDiff = maxVelocityChange + currentVelocity;
							upDownVelocityDiff = maxVelocityChange + m_aGears[1].m_ChangeUpVelocity;
						}else{
							currentDownVelocityDiff = currentVelocity - current->m_ChangeDownVelocity;
							upDownVelocityDiff = current->m_ChangeUpVelocity - current->m_ChangeDownVelocity;
						}
					}else{
						// reverse gear
						currentDownVelocityDiff = maxVelocityChange - currentVelocity;
						upDownVelocityDiff = maxVelocityChange - m_aGears[0].m_ChangeDownVelocity;
					}

					if(!IsFinite(upDownVelocityDiff) || upDownVelocityDiff <= 0.0f)
						return 0.0f;

					float velocityDiffRatio = currentDownVelocityDiff / upDownVelocityDiff;
					float inertiaMultiplier = velocityDiffRatio - *inertiaRatio;

					if(cheat == CHEAT_HANDLING_PERFECT)
						inertiaMultiplier *= TRANSMISSION_AI_CHEAT_INERTIA_MULT;
					else if(cheat == CHEAT_HANDLING_NITROS)
						inertiaMultiplier *= TRANSMISSION_NITROS_INERTIA_MULT;

					float acceleration = 1.0f - inertiaMultiplier * m_fEngineInertia;
					if(acceleration < 0.1f)
						acceleration = 0.1f;
					else if(acceleration > 1.0f)
						acceleration = 1.0f;

					*inertiaRatio = velocityDiffRatio;
					*inertiaSmoothing = acceleration * (1.0f - TRANSMISSION_SMOOTHER_FRAC) +
						TRANSMISSION_SMOOTHER_FRAC * *inertiaSmoothing;
					driveAcceleration *= *inertiaSmoothing;
				}else{
					// Zero inertia is valid data in SA and would divide by zero
					// here; the reference saturates immedately in that case.
					float delta = m_fEngineInertia > 0.0f
						? Abs(gasPedal) / m_fEngineInertia * CTimer::GetTimeStep() * TRANSMISSION_FREE_ACCELERATION
						: (gasPedal != 0.0f ? 1.0f : 0.0f);
					*inertiaRatio += delta;
					if(*inertiaRatio > 1.0f)
						*inertiaRatio = 1.0f;
					*inertiaSmoothing = 0.1f;
				}
			}

			float gearMaxVelocity = current->m_MaxVelocity;
			float gearCheatMaxVelocity = cheatMultiplier * gearMaxVelocity;
			float changeInVelocity = 0.0f;

			if(gearMaxVelocity >= 0.0f || currentVelocity >= gearCheatMaxVelocity){
				if(gearMaxVelocity <= 0.0f)
					return driveAcceleration;
				if(currentVelocity <= gearCheatMaxVelocity)
					return driveAcceleration;
				changeInVelocity = currentVelocity - gearCheatMaxVelocity;
			}else{
				changeInVelocity = gearCheatMaxVelocity - currentVelocity;
			}

			float overspeed = changeInVelocity / 0.05f;
			if(overspeed > 1.0f)
				overspeed = 1.0f;
			driveAcceleration *= 1.0f - overspeed;
			return driveAcceleration;
		}

		if(shiftToLowerGear && gear > 0)
			gear--;

		// A shift happened: the frame's torque and inertia inputs die with the
		// old gear, exactly as in the reference loop.
		inertiaRatio = nil;
		inertiaSmoothing = nil;
		wheelsOnGround = false;
		cheat = CHEAT_HANDLING_NONE;

		if(currentVelocity < m_fMaxReverseVelocity)
			return 0.0f;
	}

	return 0.0f;
}
