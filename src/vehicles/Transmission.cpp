// Adapted from gta-reversed cTransmission.cpp, SA 0x6D0460/0x6D0530/0x6D05E0.
// Reference commit: 56a67498c822c7bf6fe07f663ddef7a6be3504f1.
// Original reverse engineering: GTA Community / gta-reversed contributors.
#include "common.h"
#include "Timer.h"
#include "Transmission.h"
#include <algorithm>
#include <cmath>

namespace {
bool ValidTransmission(const cTransmission &t)
{
    return t.nNumberOfGears >= 1 && t.nNumberOfGears <= 5 &&
        std::isfinite(t.fMaxVelocity) && t.fMaxVelocity > 0.0f &&
        std::isfinite(t.fMaxReverseVelocity) && t.fMaxReverseVelocity < 0.0f &&
        std::isfinite(t.fEngineAcceleration) && t.fEngineAcceleration >= 0.0f &&
        std::isfinite(t.fEngineInertia) && t.fEngineInertia >= 0.0f;
}
}

void
cTransmission::InitGearRatios(void)
{
    memset(Gears, 0, sizeof(Gears));
    if(!ValidTransmission(*this)) return;
    const float halfGearVelocity = 0.5f * fMaxVelocity / nNumberOfGears;
    const float maxGearVelocity = fMaxVelocity - halfGearVelocity;
    for(int i = 1; i <= nNumberOfGears; ++i){
        tGear &gear = Gears[i];
        const tGear &previous = Gears[i-1];
        gear.fMaxVelocity = i * maxGearVelocity / nNumberOfGears + halfGearVelocity;
        const float difference = gear.fMaxVelocity - previous.fMaxVelocity;
        if(i == nNumberOfGears)
            gear.fShiftUpVelocity = fMaxVelocity;
        else{
            Gears[i+1].fShiftDownVelocity = 0.42f * difference + previous.fMaxVelocity;
            gear.fShiftUpVelocity = 0.6667f * difference + previous.fMaxVelocity;
        }
    }
    Gears[0].fMaxVelocity = fMaxReverseVelocity;
    Gears[0].fShiftUpVelocity = -0.01f;
    Gears[0].fShiftDownVelocity = fMaxReverseVelocity;
    Gears[1].fShiftDownVelocity = -0.01f;
}

void
cTransmission::CalculateGearForSimpleCar(float speed, uint8 &gear)
{
    if(!ValidTransmission(*this) || !std::isfinite(speed)) return;
    gear = std::min<uint8>(gear, nNumberOfGears);
    fCurVelocity = speed;
    if(speed > Gears[gear].fShiftUpVelocity){
        if(gear < nNumberOfGears) ++gear;
    }else if(speed < Gears[gear].fShiftDownVelocity && gear > 0)
        --gear;
}

float
cTransmission::CalculateDriveAcceleration(const float &gasPedal, uint8 &gear,
    float &gearChangeCount, const float &velocity, float *inertiaRatio,
    float *inertiaSmoothing, bool wheelsOnGround, eTransmissionCheat cheat)
{
    // SA leaves this argument unused. Keep it for the vehicle/audio interface.
    (void)gearChangeCount;
    if(!ValidTransmission(*this) || !std::isfinite(gasPedal) ||
       !std::isfinite(velocity) || !std::isfinite(CTimer::GetTimeStep())) return 0.0f;
    if(velocity < fMaxReverseVelocity || velocity > fMaxVelocity) return 0.0f;
    gear = std::min<uint8>(gear, nNumberOfGears);
    // Iterative SA shifting, bounded against malformed ratios. No shared static
    // scratch state and no recursive VC acceleration path.
    for(int shifts = 0; shifts <= nNumberOfGears+1; ++shifts){
        fCurVelocity = velocity;
        const tGear &ratio = Gears[gear];
        bool accelerate;
        if(velocity > ratio.fShiftUpVelocity){
            accelerate = gear == 0 && gasPedal <= 0.0f;
            if(!accelerate){
                if(gear == nNumberOfGears) return 0.0f;
                ++gear;
            }
        }else{
            accelerate = velocity >= ratio.fShiftDownVelocity || gear == 0 ||
                (gear == 1 && gasPedal >= 0.0f);
            if(!accelerate) --gear;
        }
        if(!accelerate){
            // SA drops inertia/cheat overrides for the shift iteration.
            inertiaRatio = inertiaSmoothing = nil;
            wheelsOnGround = false;
            cheat = TRANSMISSION_CHEAT_NONE;
            continue;
        }
        float speedMultiplier;
        if(nNumberOfGears == 1)
            speedMultiplier = 1.0f;
        else if(gear > 0){
            const float gearNumber = 1.0f - (float)(gear-1)/(nNumberOfGears-1);
            const float boost = (Flags & TRANSMISSION_1G_BOOST) ? 5.0f :
                (Flags & TRANSMISSION_2G_BOOST) ? 4.0f : 3.0f;
            speedMultiplier = gearNumber * gearNumber * boost + 1.0f;
        }else
            speedMultiplier = 4.5f;
        const float cheatMultiplier = cheat == TRANSMISSION_CHEAT_PERFECT ? 1.2f : 1.0f;
        const float nitrosMultiplier = cheat == TRANSMISSION_CHEAT_NITROS ? 2.0f : 1.0f;
        float acceleration = speedMultiplier * cheatMultiplier * fEngineAcceleration *
            nitrosMultiplier * 0.4f * gasPedal * CTimer::GetTimeStep();
        if(inertiaRatio && inertiaSmoothing){
            if(wheelsOnGround){
                const float maxChange = fMaxVelocity / nNumberOfGears / 3.0f;
                const float currentDiff = gear == 0 ? maxChange-velocity :
                    gear == 1 ? maxChange+velocity : velocity-ratio.fShiftDownVelocity;
                const float span = gear == 0 ? maxChange-Gears[0].fShiftDownVelocity :
                    gear == 1 ? maxChange+Gears[1].fShiftUpVelocity :
                    ratio.fShiftUpVelocity-ratio.fShiftDownVelocity;
                if(span <= 0.0f || !std::isfinite(span)) return 0.0f;
                const float newRatio = currentDiff/span;
                float difference = newRatio - *inertiaRatio;
                if(cheat == TRANSMISSION_CHEAT_PERFECT) difference *= 0.75f;
                else if(cheat == TRANSMISSION_CHEAT_NITROS) difference *= 0.5f;
                const float multiplier = std::max(0.1f, std::min(1.0f, 1.0f-difference*fEngineInertia));
                *inertiaRatio = newRatio;
                *inertiaSmoothing = multiplier * 0.15f + 0.85f * *inertiaSmoothing;
                acceleration *= *inertiaSmoothing;
            }else{
                // Zero inertia is allowed in SA data: use the saturated limit,
                // avoiding a divide-by-zero (including zero throttle / zero inertia).
                const float delta = fEngineInertia > 0.0f ?
                    std::fabs(gasPedal)/fEngineInertia * CTimer::GetTimeStep() * 0.1f :
                    (gasPedal != 0.0f ? 1.0f : 0.0f);
                *inertiaRatio = std::min(1.0f, *inertiaRatio + delta);
                *inertiaSmoothing = 0.1f;
            }
        }
        const float maxVelocity = ratio.fMaxVelocity * cheatMultiplier;
        float excess = 0.0f;
        if(ratio.fMaxVelocity > 0.0f && velocity > maxVelocity)
            excess = velocity-maxVelocity;
        else if(ratio.fMaxVelocity < 0.0f && velocity < maxVelocity)
            excess = maxVelocity-velocity;
        acceleration *= 1.0f - std::min(excess/0.05f, 1.0f);
        return acceleration;
    }
    return 0.0f;
}
