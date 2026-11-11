// Host tests for the "TEST GRAPHICS" key tables.
//
// Every check pins a value to the client address it was recovered from in
// `libblackrussia-client.so` (PUNCT ANALYSE v43, graphics chapters 17 and 19).
// The point is to fail loudly if someone "rounds" a table entry or invents a
// gradation the client does not have.
//
// Build: g++ -std=c++11 -o /tmp/testgfx tests/TestGraphicsTests.cpp -Isrc/extras/custom

#include <cstdio>
#include <cstring>
#include "TestGraphics.h"

using namespace revctestgfx;

static int failures = 0;
static int checks = 0;

static void
Check(bool cond, const char *what)
{
	checks++;
	if(!cond){
		printf("FAIL: %s\n", what);
		failures++;
	}
}

static void
CheckInt(int got, int want, const char *what)
{
	checks++;
	if(got != want){
		printf("FAIL: %s (got %d want %d)\n", what, got, want);
		failures++;
	}
}

static bool
Near(float got, float want)
{
	float d = got - want;
	return d < 0.0001f && d > -0.0001f;
}

static void
CheckFloat(float got, float want, const char *what)
{
	checks++;
	if(!Near(got, want)){
		printf("FAIL: %s (got %f want %f)\n", what, got, want);
		failures++;
	}
}

// --- reflections: client 0x1894a34 reads table 0x3d0a70 -------------------
static void
TestEnvMapSize(void)
{
	// The client skips the RT entirely below 1 (getter 0x1894a08 returns NULL),
	// then indexes [0,128,256,512].
	CheckInt(EnvMapSize(0), 0, "refl=0: no render target at all (0x1894a08 -> NULL)");
	CheckInt(EnvMapSize(1), 128, "refl=1: 128x128 (0x3d0a70[1])");
	CheckInt(EnvMapSize(2), 256, "refl=2: 256x256 (0x3d0a70[2])");
	CheckInt(EnvMapSize(3), 512, "refl=3: 512x512 (0x3d0a70[3])");
	CheckInt(EnvMapSize(4), 512, "refl=4: `cmp w9,#3; b.hs` sends it out of range, last entry");
	CheckInt(EnvMapSize(-1), 0, "negative level is treated as off, not as 512");
}

// --- shadows: same table, but the pass itself is the switch --------------
static void
TestShadowLevels(void)
{
	CheckFloat(ShadowStrengthScale(0), 0.0f, "shad=0: strength 0 -> the pass is not rendered");
	CheckFloat(ShadowExtentScale(0), 0.0f, "shad=0: no volume either");
	Check(ShadowStrengthScale(1) < ShadowStrengthScale(2), "shad=1 weaker than shad=2");
	Check(ShadowStrengthScale(2) < ShadowStrengthScale(3), "shad=2 weaker than shad=3");
	CheckFloat(ShadowStrengthScale(3), 1.0f, "shad=3 is the strength the renderer was tuned with");
	CheckFloat(ShadowExtentScale(3), 1.0f, "shad=3 keeps the full sampled volume");
	// A smaller map has to be compensated, not just dimmed.
	Check(ShadowBiasScale(1) > ShadowBiasScale(2), "coarser coverage needs a wider bias");
	Check(ShadowBiasScale(2) > ShadowBiasScale(3), "256px still needs more bias than 512px");
	CheckFloat(ShadowBiasScale(3), 1.0f, "shad=3 keeps the tuned bias");
}

// --- draw distance: client table 0x3d34e0 --------------------------------
static void
TestDrawDistance(void)
{
	CheckFloat(DrawDistanceMultiplier(0), 0.5f, "dd=0 -> 0.5 (0x3d34e0[0])");
	CheckFloat(DrawDistanceMultiplier(1), 0.7f, "dd=1 -> 0.7 (client default)");
	CheckFloat(DrawDistanceMultiplier(2), 0.9f, "dd=2 -> 0.9");
	CheckFloat(DrawDistanceMultiplier(3), 1.1f, "dd=3 -> 1.1");
	// The table has four entries; 4+ must not read past it.
	CheckFloat(DrawDistanceMultiplier(4), 1.1f, "dd=4 falls back to the last valid entry");
	CheckFloat(DrawDistanceMultiplier(99), 1.1f, "dd=99 falls back to the last valid entry");
	CheckFloat(DrawDistanceMultiplier(-3), 0.5f, "negative dd clamps to the first entry");

	// fcsel at 0xde3964 caps the product at 2.2.
	CheckFloat(ClampDistanceMultiplier(1.1f), 1.1f, "1.1 is under the 2.2 cap");
	CheckFloat(ClampDistanceMultiplier(2.2f), 2.2f, "2.2 is the cap itself");
	CheckFloat(ClampDistanceMultiplier(5.0f), 2.2f, "anything above is clamped to 2.2");
	CheckFloat(ClampDistanceMultiplier(0.0f), 0.0f, "zero stays zero");
	CheckFloat(ClampDistanceMultiplier(-1.0f), 0.0f, "negative multiplier is not a distance");

	// The end-to-end product the optimisation stage hands to the renderer:
	// slider * dd, then the client's 2.2 cap. The slider is already clamped to
	// its own 0.80..2.2 range upstream, so nothing below 0.8 * 0.5 = 0.4 can
	// reach the cap from here.
	CheckFloat(ClampDistanceMultiplier(DrawDistanceMultiplier(1)), 0.7f, "client default keeps the world at 0.7x");
	CheckFloat(ClampDistanceMultiplier(1.0f * DrawDistanceMultiplier(3)), 1.1f, "slider 1.0 * 1.1 stays 1.1");
	CheckFloat(ClampDistanceMultiplier(2.2f * DrawDistanceMultiplier(3)), 2.2f, "slider roof * 1.1 is capped at 2.2");
	CheckFloat(ClampDistanceMultiplier(0.8f * DrawDistanceMultiplier(0)), 0.4f, "slider floor * 0.5 = 0.4, below the floor on purpose");
}

// --- effect: one `cbnz` over the whole post block ------------------------
static void
TestEffects(void)
{
	Check(!EffectsEnabled(0), "effect=0 skips the whole post block (0xe787d4)");
	Check(EffectsEnabled(1), "effect=1 is the client default and runs the block");
	Check(EffectsEnabled(2), "effect=2 still runs the block");
	Check(EffectsEnabled(3), "effect=3 still runs the block");
}

// --- defaults: chapter 18.5 ---------------------------------------------
static void
TestDefaults(void)
{
	Settings s = Settings::Defaults();
	CheckInt(s.shadows, 0, "shadow_quality_new defaults to 0");
	CheckInt(s.reflections, 0, "reflection_on_car_new defaults to 0");
	CheckInt(s.drawDistance, 1, "distance_new defaults to 1 (not 0)");
	CheckInt(s.effects, 1, "effect_quality_new defaults to 1");
	CheckInt(s.maxFps, 60, "maxFps defaults to 60");
}

// --- Apply(): what the renderer actually receives ------------------------
static void
TestApply(void)
{
	Settings s = Settings::Defaults();
	Applied a = Apply(s);

	// Defaults are "no shadows, no reflections" - the two things chapter 18
	// proved are off out of the box.
	CheckInt(a.shadowLevel, 0, "default: shadow pass off");
	CheckFloat(a.shadowStrength, 0.0f, "default: shadow strength 0");
	CheckInt(a.envMapSize, 0, "default: no reflection RT");
	CheckFloat(a.distanceMultiplier, 0.7f, "default: dd=1 -> 0.7");
	Check(a.effectsEnabled, "default: post block runs");
	CheckInt(a.maxFps, 60, "default: 60 ceiling");

	s.shadows = 3;
	s.reflections = 3;
	s.drawDistance = 3;
	a = Apply(s);
	CheckInt(a.shadowLevel, 3, "shad=3 reaches the renderer as level 3");
	CheckFloat(a.shadowStrength, 1.0f, "shad=3 -> full strength");
	CheckInt(a.envMapSize, 512, "refl=3 -> 512x512");
	CheckFloat(a.distanceMultiplier, 1.1f, "dd=3 -> 1.1");

	// Out-of-range values must not reach the renderer as-is.
	s.shadows = 17;
	s.reflections = -4;
	s.drawDistance = 9;
	a = Apply(s);
	CheckInt(a.shadowLevel, 3, "shad=17 clamps to 3");
	CheckInt(a.envMapSize, 0, "refl=-4 clamps to off");
	CheckFloat(a.distanceMultiplier, 1.1f, "dd=9 clamps to the last table entry");
}

// --- menu round-trip ----------------------------------------------------
static void
TestMenuClamps(void)
{
	CheckInt(ClampLevel(-1), 0, "a level below 0 becomes 0");
	CheckInt(ClampLevel(0), 0, "0 stays 0");
	CheckInt(ClampLevel(3), 3, "3 stays 3");
	CheckInt(ClampLevel(4), 3, "4 clamps to 3 (four-entry table)");
	CheckInt(ClampLevel(127), 3, "int8 garbage clamps to 3");

	CheckInt(CycleMaxFps(0), 30, "maxFps cycle: off -> 30");
	CheckInt(CycleMaxFps(30), 60, "maxFps cycle: 30 -> 60");
	CheckInt(CycleMaxFps(60), 90, "maxFps cycle: 60 -> 90");
	CheckInt(CycleMaxFps(90), 120, "maxFps cycle: 90 -> 120");
	CheckInt(CycleMaxFps(120), 0, "maxFps cycle wraps back to off");
	CheckInt(CycleMaxFps(77), 0, "an unknown ceiling restarts the cycle");

	Check(strcmp(MaxFpsText(0), "FEM_OFF") == 0, "0 is labelled OFF, not 0");
	Check(strcmp(MaxFpsText(30), "FEM_30") == 0, "30 has its own label");
	Check(strcmp(MaxFpsText(120), "FEM_120") == 0, "120 has its own label");
	Check(strcmp(MaxFpsText(77), "FEM_OFF") == 0, "unknown ceiling falls back to OFF");
}

int
main(void)
{
	TestEnvMapSize();
	TestShadowLevels();
	TestDrawDistance();
	TestEffects();
	TestDefaults();
	TestApply();
	TestMenuClamps();

	if(failures == 0)
		printf("test graphics: %d checks ok\n", checks);
	else
		printf("test graphics: %d of %d checks FAILED\n", failures, checks);
	return failures != 0;
}
