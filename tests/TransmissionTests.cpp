// Numerical tests of the SA transmission implementation. No RW/GPU assets: the
// real cTransmission.cpp is compiled and driven with hand-computed reference
// values taken from gta-reversed (0x6D0460 / 0x6D0530 / 0x6D05E0).
#include "common.h"
#include "Timer.h"
#include "cTransmission.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <limits>
#include <initializer_list>

float CTimer::ms_fTimeStep = 1.0f;

static void Check(bool ok, const char *why)
{
	if(!ok){
		std::fprintf(stderr, "cTransmissionTests: %s\n", why);
		std::abort();
	}
}

static void Near(float a, float b, const char *why)
{
	if(!(std::fabs(a - b) < 0.00001f)){
		std::fprintf(stderr, "cTransmissionTests: %s (got %.8f, want %.8f)\n", why, a, b);
		std::abort();
	}
}

static cTransmission Transmission(uint8 gears = 5)
{
	cTransmission t;
	memset(&t, 0, sizeof(t));
	t.m_nNumberOfGears = gears;
	t.m_nDriveType = 'R';
	t.m_nEngineType = 'P';
	t.m_fEngineAcceleration = 0.02f;
	t.m_fEngineInertia = 10.0f;
	t.m_fMaxVelocity = 1.0f;
	t.m_fMaxReverseVelocity = -0.2f;
	t.InitGearRatios();
	return t;
}

static float Drive(cTransmission &t, uint8 &gear, float speed, float gas = 1.0f,
	eTransmissionCheat cheat = CHEAT_HANDLING_NONE, float *ratio = nil, float *smooth = nil,
	bool grounded = true)
{
	float count = 123.0f;
	float result = t.CalculateDriveAcceleration(gas, gear, count, speed, ratio, smooth, grounded, cheat);
	Check(count == 123.0f, "the gear-change counter is left untouched");
	return result;
}

// InitGearRatios must reproduce the half-gear offset of the reference: naive
// spacing would give 0.2, 0.4, ... 1.0 instead.
static void TestGearSpacing(void)
{
	cTransmission t = Transmission();
	const float maxima[] = { -0.2f, 0.28f, 0.46f, 0.64f, 0.82f, 1.0f };
	for(int i = 0; i < 6; i++)
		Near(t.m_aGears[i].m_MaxVelocity, maxima[i], "SA half-gear spacing");

	Near(t.m_aGears[1].m_ChangeUpVelocity, 0.28f * 0.6667f, "first upshift threshold");
	Near(t.m_aGears[2].m_ChangeDownVelocity, 0.28f * 0.42f, "second downshift threshold");
	Near(t.m_aGears[1].m_ChangeDownVelocity, -0.01f, "reverse to first threshold");
	Near(t.m_aGears[0].m_ChangeUpVelocity, -0.01f, "first to reverse threshold");
	Near(t.m_aGears[5].m_ChangeUpVelocity, 1.0f, "top gear is capped at max velocity");
}

static void TestSimpleTrafficShift(void)
{
	cTransmission t = Transmission();
	uint8 gear = 5;
	t.CalculateGearForSimpleCar(100.0f, gear);
	Check(gear == 5, "simple shift cannot exceed the top gear");
	gear = 0;
	t.CalculateGearForSimpleCar(-100.0f, gear);
	Check(gear == 0, "simple shift cannot underflow reverse");
	gear = 255;
	t.CalculateGearForSimpleCar(1.0f, gear);
	Check(gear == 5, "a corrupt restored gear is bounded before indexing");
}

static void TestTorque(void)
{
	cTransmission t = Transmission();
	uint8 gear = 1;
	Near(Drive(t, gear, 0.0f), 0.032f, "first gear drive acceleration");
	Near(Drive(t, gear, 0.1f), 0.032f, "torque does not depend on speed the way VC did");

	t.m_nHandlingFlags = VEHICLE_HANDLING_1G_BOOST;
	gear = 1;
	Near(Drive(t, gear, 0.0f), 0.048f, "1G boost curve");
	t.m_nHandlingFlags = VEHICLE_HANDLING_2G_BOOST;
	gear = 1;
	Near(Drive(t, gear, 0.0f), 0.04f, "2G boost curve");
	t.m_nHandlingFlags = 0;

	gear = 1;
	Near(Drive(t, gear, 0.0f, 1.0f, CHEAT_HANDLING_PERFECT), 0.0384f, "perfect handling multiplier");
	gear = 1;
	Near(Drive(t, gear, 0.0f, 1.0f, CHEAT_HANDLING_NITROS), 0.064f, "nitros multiplier");
	gear = 0;
	Near(Drive(t, gear, -0.05f, -1.0f), -0.036f, "reverse torque");
}

static void TestDirectionChange(void)
{
	cTransmission t = Transmission();
	uint8 gear = 0;
	Near(Drive(t, gear, 0.0f, 1.0f, CHEAT_HANDLING_NITROS), 0.032f, "a direction change drops the cheat");
	Check(gear == 1, "reverse shifts to first on positive throttle");

	gear = 1;
	Near(Drive(t, gear, -0.05f, -1.0f), -0.036f, "first shifts to reverse on negative throttle");
	Check(gear == 0, "reverse selected");

	gear = 5;
	Drive(t, gear, 0.0f);
	Check(gear == 1, "multi-step downshift is bounded");

	gear = 1;
	Drive(t, gear, 0.9f);
	Check(gear == 5, "multi-step upshift is bounded");
}

static void TestLimits(void)
{
	cTransmission t = Transmission();
	uint8 gear = 1;
	Near(Drive(t, gear, 1.1f), 0.0f, "forward hard limit");
	gear = 0;
	Near(Drive(t, gear, -0.3f, -1.0f), 0.0f, "reverse hard limit");
	gear = 1;
	Near(Drive(t, gear, 0.0f, 0.0f), 0.0f, "zero throttle produces no force");
}

static void TestInertia(void)
{
	cTransmission t = Transmission();
	uint8 gear = 1;
	Near(Drive(t, gear, 0.0f, 1.0f, CHEAT_HANDLING_NONE, nil, nil, false), 0.032f,
		"inertia pointers are optional");

	float ratio = 0.0f, smooth = 1.0f;
	gear = 1;
	float accelerated = Drive(t, gear, 0.0f, 1.0f, CHEAT_HANDLING_NONE, &ratio, &smooth, true);
	Near(smooth, 0.865f, "SA inertia smoothing");
	Near(accelerated, 0.032f * 0.865f, "inertia scales the drive force");
	Check(ratio > 0.0f, "inertia captures the normalised engine speed");

	float otherRatio = 0.0f, otherSmooth = 1.0f;
	gear = 1;
	Drive(t, gear, 0.0f, 1.0f, CHEAT_HANDLING_NONE, &otherRatio, &otherSmooth, false);
	Near(otherRatio, 0.01f, "free-rev ratio increment");
	Near(otherSmooth, 0.1f, "airborne smoothing state");
	Near(smooth, 0.865f, "per-instance inertia state is independent");

	CTimer::SetTimeStep(0.5f);
	gear = 1;
	Near(Drive(t, gear, 0.0f), 0.016f, "force scales with the timestep");
	CTimer::SetTimeStep(1.0f);
}

static void TestDegenerateData(void)
{
	cTransmission t = Transmission();
	float ratio = 0.0f, smooth = 1.0f;
	uint8 gear = 1;

	t.m_fEngineInertia = 0.0f;
	Drive(t, gear, 0.0f, 0.0f, CHEAT_HANDLING_NONE, &ratio, &smooth, false);
	Check(std::isfinite(ratio), "zero inertia and zero throttle stay finite");
	gear = 1;
	Drive(t, gear, 0.0f, 1.0f, CHEAT_HANDLING_NONE, &ratio, &smooth, false);
	Near(ratio, 1.0f, "zero inertia free-rev saturates instead of dividing by zero");

	// Single forward gear: the curve collapses to a constant.
	t = Transmission(1);
	gear = 1;
	Near(Drive(t, gear, 0.0f), 0.008f, "one forward gear");
	gear = 0;
	Near(Drive(t, gear, -0.05f, -1.0f), -0.008f, "single-speed reverse");

	// Malformed gear counts are rejected rather than indexing out of bounds.
	for(int count : {0, 6, 127}){
		t = Transmission((uint8)count);
		gear = 255;
		Near(Drive(t, gear, 0.0f), 0.0f, "malformed gear count rejected");
	}

	t = Transmission();
	gear = 1;
	Near(Drive(t, gear, std::numeric_limits<float>::quiet_NaN()), 0.0f, "NaN speed rejected");
	Near(Drive(t, gear, 0.0f, std::numeric_limits<float>::infinity()), 0.0f,
		"infinite throttle rejected");
}

// Every supported gear count, both throttle directions and a speed sweep: the
// result must be finite and the selected gear in range, which is what keeps a
// malformed record from wedging the simulation loop.
static void TestSweep(void)
{
	for(int count = 1; count <= 5; count++){
		cTransmission t = Transmission((uint8)count);
		for(int initial = 0; initial <= count; initial++)
			for(int i = -20; i <= 100; i++)
				for(float gas : { -1.0f, 0.0f, 1.0f }){
					uint8 gear = (uint8)initial;
					float force = Drive(t, gear, i * 0.01f, gas);
					Check(std::isfinite(force), "swept force stays finite");
					Check(gear <= count, "swept gear stays in range");
				}
	}
}

int main(void)
{
	TestGearSpacing();
	TestSimpleTrafficShift();
	TestTorque();
	TestDirectionChange();
	TestLimits();
	TestInertia();
	TestDegenerateData();
	TestSweep();
	std::puts("cTransmissionTests: ALL PASSED");
	return 0;
}
