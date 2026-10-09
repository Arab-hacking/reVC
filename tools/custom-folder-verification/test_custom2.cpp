// Test rig for the BlackRussia timecycle port: runs the real
// CustomTimecycle.cpp (JSON reader, slot grid, presets, the expansion into
// the game's 24 hours) against the client's own data files, headless.
//
//   TC_DATA_DIR=<common folder> ./test_custom2
//
// Everything the module logs goes through the DebugLog stub below, so the
// rig output shows exactly what the game would log.
#include "common.h"

#include <stdarg.h>
#include <sys/stat.h>
#include <string>
#include <vector>

#include "CustomTimecycle.h"
#include "CustomModels.h"
#include "Timecycle.h"

static int failures = 0;

static void
check(bool ok, const char *what)
{
	printf("%s: %s\n", ok ? "ok  " : "FAIL", what);
	if(!ok)
		failures++;
}

// the game's tables, pre-filled with a marker so untouched cells show up
#define MARK 111

#define DEFINE_TC(name) uint8 CTimeCycle::name[NUMHOURS][NUMWEATHERS]
#define DEFINE_TC16(name) int16 CTimeCycle::name[NUMHOURS][NUMWEATHERS]
#define DEFINE_TC8(name) int8 CTimeCycle::name[NUMHOURS][NUMWEATHERS]
DEFINE_TC(m_nAmbientRed); DEFINE_TC(m_nAmbientGreen); DEFINE_TC(m_nAmbientBlue);
DEFINE_TC(m_nAmbientRed_Obj); DEFINE_TC(m_nAmbientGreen_Obj); DEFINE_TC(m_nAmbientBlue_Obj);
DEFINE_TC(m_nAmbientRed_Bl); DEFINE_TC(m_nAmbientGreen_Bl); DEFINE_TC(m_nAmbientBlue_Bl);
DEFINE_TC(m_nAmbientRed_Obj_Bl); DEFINE_TC(m_nAmbientGreen_Obj_Bl); DEFINE_TC(m_nAmbientBlue_Obj_Bl);
DEFINE_TC(m_nDirectionalRed); DEFINE_TC(m_nDirectionalGreen); DEFINE_TC(m_nDirectionalBlue);
DEFINE_TC(m_nSkyTopRed); DEFINE_TC(m_nSkyTopGreen); DEFINE_TC(m_nSkyTopBlue);
DEFINE_TC(m_nSkyBottomRed); DEFINE_TC(m_nSkyBottomGreen); DEFINE_TC(m_nSkyBottomBlue);
DEFINE_TC(m_nSunCoreRed); DEFINE_TC(m_nSunCoreGreen); DEFINE_TC(m_nSunCoreBlue);
DEFINE_TC(m_nSunCoronaRed); DEFINE_TC(m_nSunCoronaGreen); DEFINE_TC(m_nSunCoronaBlue);
DEFINE_TC8(m_fSunSize); DEFINE_TC8(m_fSpriteSize); DEFINE_TC8(m_fSpriteBrightness);
DEFINE_TC(m_nShadowStrength); DEFINE_TC(m_nLightShadowStrength); DEFINE_TC(m_nPoleShadowStrength);
DEFINE_TC16(m_fFogStart); DEFINE_TC16(m_fFarClip);
DEFINE_TC(m_fLightsOnGroundBrightness);
DEFINE_TC(m_nLowCloudsRed); DEFINE_TC(m_nLowCloudsGreen); DEFINE_TC(m_nLowCloudsBlue);
DEFINE_TC(m_nFluffyCloudsTopRed); DEFINE_TC(m_nFluffyCloudsTopGreen); DEFINE_TC(m_nFluffyCloudsTopBlue);
DEFINE_TC(m_nFluffyCloudsBottomRed); DEFINE_TC(m_nFluffyCloudsBottomGreen); DEFINE_TC(m_nFluffyCloudsBottomBlue);
DEFINE_TC(m_fBlurRed); DEFINE_TC(m_fBlurGreen); DEFINE_TC(m_fBlurBlue);
DEFINE_TC(m_fWaterRed); DEFINE_TC(m_fWaterGreen); DEFINE_TC(m_fWaterBlue); DEFINE_TC(m_fWaterAlpha);

static void
FillMarkers(void)
{
#define FILL(name) memset(CTimeCycle::name, MARK, sizeof(CTimeCycle::name))
#define FILL16(name) do { \
	for(int h = 0; h < NUMHOURS; h++) \
		for(int w = 0; w < NUMWEATHERS; w++) \
			CTimeCycle::name[h][w] = MARK; \
} while(0)
	FILL(m_nAmbientRed); FILL(m_nAmbientGreen); FILL(m_nAmbientBlue);
	FILL(m_nAmbientRed_Obj); FILL(m_nAmbientGreen_Obj); FILL(m_nAmbientBlue_Obj);
	FILL(m_nAmbientRed_Bl); FILL(m_nAmbientGreen_Bl); FILL(m_nAmbientBlue_Bl);
	FILL(m_nAmbientRed_Obj_Bl); FILL(m_nAmbientGreen_Obj_Bl); FILL(m_nAmbientBlue_Obj_Bl);
	FILL(m_nDirectionalRed); FILL(m_nDirectionalGreen); FILL(m_nDirectionalBlue);
	FILL(m_nSkyTopRed); FILL(m_nSkyTopGreen); FILL(m_nSkyTopBlue);
	FILL(m_nSkyBottomRed); FILL(m_nSkyBottomGreen); FILL(m_nSkyBottomBlue);
	FILL(m_nSunCoreRed); FILL(m_nSunCoreGreen); FILL(m_nSunCoreBlue);
	FILL(m_nSunCoronaRed); FILL(m_nSunCoronaGreen); FILL(m_nSunCoronaBlue);
	FILL(m_fSunSize); FILL(m_fSpriteSize); FILL(m_fSpriteBrightness);
	FILL(m_nShadowStrength); FILL(m_nLightShadowStrength); FILL(m_nPoleShadowStrength);
	FILL16(m_fFogStart); FILL16(m_fFarClip);
	FILL(m_fLightsOnGroundBrightness);
	FILL(m_nLowCloudsRed); FILL(m_nLowCloudsGreen); FILL(m_nLowCloudsBlue);
	FILL(m_nFluffyCloudsTopRed); FILL(m_nFluffyCloudsTopGreen); FILL(m_nFluffyCloudsTopBlue);
	FILL(m_nFluffyCloudsBottomRed); FILL(m_nFluffyCloudsBottomGreen); FILL(m_nFluffyCloudsBottomBlue);
	FILL(m_fBlurRed); FILL(m_fBlurGreen); FILL(m_fBlurBlue);
	FILL(m_fWaterRed); FILL(m_fWaterGreen); FILL(m_fWaterBlue); FILL(m_fWaterAlpha);
}

int
main(int argc, char *argv[])
{
	printf("== custom timecycle, against the client's own data files ==\n");

	const char *dir = getenv("TC_DATA_DIR");
	if(dir == nil || dir[0] == '\0')
		dir = ".";
	setenv("TC_DATA_DIR", dir, 1);

	FillMarkers();
	CCustomTimecycle::Apply();

	// weather 0, the hour-0 slot (the client's own values)
	check(CTimeCycle::m_nAmbientRed[0][0] == 22 &&
	      CTimeCycle::m_nAmbientGreen[0][0] == 22 &&
	      CTimeCycle::m_nAmbientBlue[0][0] == 22,
	      "w0 h0 ambient is the client's [22,22,22]");
	check(CTimeCycle::m_fFarClip[0][0] == 400, "w0 h0 far clip is the client's 400");
	check(CTimeCycle::m_fFogStart[0][0] == 100, "w0 h0 fog start is the client's 100");
	check(CTimeCycle::m_nSkyTopGreen[0][0] == 23 && CTimeCycle::m_nSkyTopBlue[0][0] == 24,
	      "w0 h0 sky top is the client's [0,23,24]");

	// weather 0, the hour-12 slot (values land on the slot hour unchanged)
	check(CTimeCycle::m_fFarClip[12][0] == 800, "w0 h12 far clip is the client's 800");
	check(CTimeCycle::m_nSkyTopRed[12][0] == 68 && CTimeCycle::m_nSkyTopGreen[12][0] == 117 &&
	      CTimeCycle::m_nSkyTopBlue[12][0] == 210,
	      "w0 h12 sky top is the client's [68,117,210]");
	check(CTimeCycle::m_nDirectionalRed[12][0] == 255, "w0 h12 directional is the client's 255");

	// an hour between two slots is interpolated: ambient runs 5 (h9) -> 11 (h12)
	check(CTimeCycle::m_nAmbientRed[10][0] == 7 && CTimeCycle::m_nAmbientRed[11][0] == 9,
	      "w0 h10/h11 ambient interpolates between the h9 and h12 slots");
	// the h0..h5 slots both carry far clip 400, so the hours between stay 400
	check(CTimeCycle::m_fFarClip[3][0] == 400, "w0 h3 far clip matches the flat 400..400 slots");
	// and the day wraps: h23 blends h22 (600) into h0 (400)
	check(CTimeCycle::m_fFarClip[23][0] == 500, "w0 h23 far clip blends into the next day's h0");

	// the server-driven presets must NOT leak into the day cycle
	check(CTimeCycle::m_nAmbientRed[5][1] == 6 && CTimeCycle::m_nAmbientGreen[5][1] == 20,
	      "w1 h5 ambient stays the base cycle's [6,20,20] (presets are logged, not applied)");
	check(CTimeCycle::m_nSkyTopGreen[5][1] == 0, "w1 h5 sky top stays the base cycle's [0,0,0]");
	check(CTimeCycle::m_nSkyBottomRed[5][1] == 113, "w1 h5 sky bottom stays the base cycle's [113,113,113]");

	// weathers beyond the first four keep the game's own tables
	check(CTimeCycle::m_nAmbientRed[0][6] == MARK && CTimeCycle::m_fFarClip[12][6] == MARK,
	      "weather slots the game does not map are untouched");

	// without the data files nothing changes
	setenv("TC_DATA_DIR", "/tmp/empty_tc_dir", 1);
	mkdir("/tmp/empty_tc_dir", 0755);
	FillMarkers();
	CCustomTimecycle::Apply();
	check(CTimeCycle::m_nAmbientRed[0][0] == MARK && CTimeCycle::m_fFarClip[0][0] == MARK,
	      "without the client's files the game's own timecycle stays");

	printf("== %s (%d failure(s)) ==\n", failures ? "FAILED" : "all tests passed", failures);
	return failures ? 1 : 0;
}
