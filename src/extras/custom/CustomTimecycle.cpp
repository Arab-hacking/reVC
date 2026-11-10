#include "common.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <cmath>

#include "CustomTimecycle.h"
#include "MiniJson.h"
#include "CustomTimecycleShadow.h"
#include "CustomModels.h"
#include "DebugLog.h"
#include "SavehDiag.h"
#include "Timecycle.h"
#ifdef RW_GL3
#include "GL3Atmosphere.h"
#endif

#ifdef CUSTOM_MODELS

#include <string>
#include <vector>

// The JSON reader lives in MiniJson.h, shared with the optimization config.
// It keeps the tolerance this file always had: BOM, // comments, \u escapes.

// ---------------------------------------------------------------------------
// the time cycle itself
// ---------------------------------------------------------------------------

// the day-time slots of the San Andreas time cycle, which the client keeps
static const int CycleHours[8] = { 0, 5, 7, 9, 12, 17, 19, 22 };

// one day-time slot of one weather, with the client's own field names
struct CycleSlot
{
	enum {
		F_AMBIENT_R, F_AMBIENT_G, F_AMBIENT_B,
		F_AMBPHYS_R, F_AMBPHYS_G, F_AMBPHYS_B,
		F_DIR_R, F_DIR_G, F_DIR_B,
		F_SKYTOP_R, F_SKYTOP_G, F_SKYTOP_B,
		F_SKYBOT_R, F_SKYBOT_G, F_SKYBOT_B,
		F_SUNCORE_R, F_SUNCORE_G, F_SUNCORE_B,
		F_SUNCOR_R, F_SUNCOR_G, F_SUNCOR_B,
		F_SUNSIZE, F_SPRITESIZE, F_SPRITEBRGHT,
		F_SHAD, F_LIGHTSHAD, F_POLESHAD,
		F_FARCLIP, F_FOGSTART, F_LIGHTGND,
		F_CLOUD_R, F_CLOUD_G, F_CLOUD_B,
		F_FLUFFBOT_R, F_FLUFFBOT_G, F_FLUFFBOT_B,
		F_WATER_R, F_WATER_G, F_WATER_B, F_WATER_A,
		F_COUNT
	};
	bool has[F_COUNT];
	float v[F_COUNT];

	void Clear(void) { memset(has, 0, sizeof(has)); memset(v, 0, sizeof(v)); }

	void ReadRGB(const minijson::Value *obj, const char *key, int f0) {
		const minijson::Value *a = obj->Find(key);
		if(a == nil || a->type != minijson::Value::ARR || a->items.size() < 3)
			return;
		for(int c = 0; c < 3; c++)
			if(a->items[c].type != minijson::Value::NUM || !std::isfinite(a->items[c].num))
				return;
		has[f0+0] = has[f0+1] = has[f0+2] = true;
		v[f0+0] = (float)a->items[0].num;
		v[f0+1] = (float)a->items[1].num;
		v[f0+2] = (float)a->items[2].num;
	}
	void ReadRGBA(const minijson::Value *obj, const char *key, int f0) {
		const minijson::Value *a = obj->Find(key);
		if(a == nil || a->type != minijson::Value::ARR || a->items.size() < 4)
			return;
		for(int i = 0; i < 4; i++)
			if(a->items[i].type != minijson::Value::NUM || !std::isfinite(a->items[i].num))
				return;
		for(int i = 0; i < 4; i++){
			has[f0+i] = true;
			v[f0+i] = (float)a->items[i].num;
		}
	}
	void ReadNum(const minijson::Value *obj, const char *key, int f) {
		const minijson::Value *a = obj->Find(key);
		if(a == nil || a->type != minijson::Value::NUM || !std::isfinite(a->num))
			return;
		has[f] = true;
		v[f] = (float)a->num;
	}
	void Read(const minijson::Value *obj) {
		ReadRGB(obj, "AmbientRGB", F_AMBIENT_R);
		ReadRGB(obj, "AmbientPhysicalRGB", F_AMBPHYS_R);
		ReadRGB(obj, "DirectionalRGB", F_DIR_R);
		ReadRGB(obj, "SkyTopRGB", F_SKYTOP_R);
		ReadRGB(obj, "SkyBottomRGB", F_SKYBOT_R);
		ReadRGB(obj, "SunCoreRGB", F_SUNCORE_R);
		ReadRGB(obj, "SunCoronaRGB", F_SUNCOR_R);
		ReadRGB(obj, "CloudRGB", F_CLOUD_R);
		ReadRGB(obj, "FluffyBottomRGB", F_FLUFFBOT_R);
		ReadRGBA(obj, "WaterRGBA", F_WATER_R);
		ReadNum(obj, "SunSize", F_SUNSIZE);
		ReadNum(obj, "SpriteSize", F_SPRITESIZE);
		ReadNum(obj, "SpriteBrght", F_SPRITEBRGHT);
		ReadNum(obj, "Shad", F_SHAD);
		ReadNum(obj, "LightShad", F_LIGHTSHAD);
		ReadNum(obj, "PoleShad", F_POLESHAD);
		ReadNum(obj, "FarClip", F_FARCLIP);
		ReadNum(obj, "FogStart", F_FOGSTART);
		ReadNum(obj, "LightGnd", F_LIGHTGND);
	}
};

// the slot grid of one weather
struct SlotGrid
{
	bool present[8];		// the slots the client carries
	CycleSlot slots[8];
};

// the client has 22 weathers, the game four: the first four apply
#define BR_WEATHERS_USED 4

static bool
ParseTimecycle(const std::vector<uint8> &data, SlotGrid grids[BR_WEATHERS_USED], int &weathersOut)
{
	minijson::Value root;
	std::string err;
	if(!minijson::Parse(data, root, err) || root.type != minijson::Value::ARR){
		CUSTOM_LOG("timecycle: timecyc.json could not be parsed (%s)\n", err.c_str());
		return false;
	}

	for(int w = 0; w < BR_WEATHERS_USED; w++)
		for(int sI = 0; sI < 8; sI++)
			grids[w].slots[sI].Clear();
	memset(grids, 0, sizeof(SlotGrid) * BR_WEATHERS_USED);

	int weathers = root.items.size() < BR_WEATHERS_USED ? (int)root.items.size() : BR_WEATHERS_USED;
	int slotsRead = 0;

	for(int w = 0; w < weathers; w++){
		const minijson::Value &jw = root.items[w];
		if(jw.type != minijson::Value::ARR)
			continue;
		int n = jw.items.size() < 8 ? (int)jw.items.size() : 8;
		for(int sI = 0; sI < n; sI++){
			if(jw.items[sI].type != minijson::Value::OBJ)
				continue;
			grids[w].slots[sI].Read(&jw.items[sI]);
			grids[w].present[sI] = true;
			slotsRead++;
		}
	}
	weathersOut = weathers;
	if(slotsRead == 0){
		CUSTOM_LOG("timecycle: timecyc.json holds no weathers this game can use\n");
		return false;
	}
	return true;
}

// One field of one weather, interpolated over the client's day-time slots.
// Missing entries borrow the adjacent slot. Invalid values never reach casts
// or camera/fog state (JSON can contain non-finite numbers via strtod).
static bool
SampleField(const SlotGrid &grid, int h, int f, float &val)
{
	int s0, s1;
	float t;
	if(h >= CycleHours[7]){
		s0 = 7; s1 = 0;
		t = (h - CycleHours[7]) / (float)(24 - CycleHours[7]);
	}else{
		int i = 0;
		while(i < 6 && !(h >= CycleHours[i] && h < CycleHours[i+1]))
			i++;
		s0 = i; s1 = i+1;
		t = (h - CycleHours[i]) / (float)(CycleHours[i+1] - CycleHours[i]);
	}
	bool has0 = grid.present[s0] && grid.slots[s0].has[f];
	bool has1 = grid.present[s1] && grid.slots[s1].has[f];
	if(!has0 && !has1)
		return false;
	float a = has0 ? grid.slots[s0].v[f] : grid.slots[s1].v[f];
	float b = has1 ? grid.slots[s1].v[f] : grid.slots[s0].v[f];
	val = a + (b - a) * t;
	return std::isfinite(val);
}

template<typename T>
static void
ExpandField(T arr[NUMHOURS][NUMWEATHERS], int w, const SlotGrid &grid, int f,
            float scale, float lo, float hi)
{
	for(int h = 0; h < NUMHOURS; h++){
		float val;
		if(!SampleField(grid, h, f, val))
			continue;
		val *= scale;
		if(val < lo) val = lo;
		if(val > hi) val = hi;
		arr[h][w] = (T)(int)lroundf(val);
	}
}

// the values live in the game's tables now: hour rows 0..23, weather columns
static void
ExpandGrids(const SlotGrid grids[BR_WEATHERS_USED], int weathers, bool atmosphere)
{
	for(int w = 0; w < weathers; w++){
		const SlotGrid &g = grids[w];
		if(!g.present[0])
			continue;
		const bool *has = g.slots[0].has;

		if(has[CycleSlot::F_AMBIENT_R]){
			ExpandField(CTimeCycle::m_nAmbientRed, w, g, CycleSlot::F_AMBIENT_R, 1.0f, 0, 255);
			ExpandField(CTimeCycle::m_nAmbientGreen, w, g, CycleSlot::F_AMBIENT_G, 1.0f, 0, 255);
			ExpandField(CTimeCycle::m_nAmbientBlue, w, g, CycleSlot::F_AMBIENT_B, 1.0f, 0, 255);
		}
		if(has[CycleSlot::F_AMBPHYS_R]){
			ExpandField(CTimeCycle::m_nAmbientRed_Obj, w, g, CycleSlot::F_AMBPHYS_R, 1.0f, 0, 255);
			ExpandField(CTimeCycle::m_nAmbientGreen_Obj, w, g, CycleSlot::F_AMBPHYS_G, 1.0f, 0, 255);
			ExpandField(CTimeCycle::m_nAmbientBlue_Obj, w, g, CycleSlot::F_AMBPHYS_B, 1.0f, 0, 255);
			// the client has one ambient; the bloom variants follow it
			ExpandField(CTimeCycle::m_nAmbientRed_Bl, w, g, CycleSlot::F_AMBIENT_R, 1.0f, 0, 255);
			ExpandField(CTimeCycle::m_nAmbientGreen_Bl, w, g, CycleSlot::F_AMBIENT_G, 1.0f, 0, 255);
			ExpandField(CTimeCycle::m_nAmbientBlue_Bl, w, g, CycleSlot::F_AMBIENT_B, 1.0f, 0, 255);
			ExpandField(CTimeCycle::m_nAmbientRed_Obj_Bl, w, g, CycleSlot::F_AMBPHYS_R, 1.0f, 0, 255);
			ExpandField(CTimeCycle::m_nAmbientGreen_Obj_Bl, w, g, CycleSlot::F_AMBPHYS_G, 1.0f, 0, 255);
			ExpandField(CTimeCycle::m_nAmbientBlue_Obj_Bl, w, g, CycleSlot::F_AMBPHYS_B, 1.0f, 0, 255);
		}
		if(has[CycleSlot::F_DIR_R]){
			ExpandField(CTimeCycle::m_nDirectionalRed, w, g, CycleSlot::F_DIR_R, 1.0f, 0, 255);
			ExpandField(CTimeCycle::m_nDirectionalGreen, w, g, CycleSlot::F_DIR_G, 1.0f, 0, 255);
			ExpandField(CTimeCycle::m_nDirectionalBlue, w, g, CycleSlot::F_DIR_B, 1.0f, 0, 255);
		}
		// Reconcile SA/BR night timecyc anomalies (AmbientRGB ~0, AmbientPhysicalRGB ~249, DirectionalRGB 255 at midnight)
		// so night buildings are clearly readable under moonlight while dynamic objects/vehicles match the night scene.
		for(int h = 0; h < NUMHOURS; h++){
			float nightWeight = 0.0f;
			if(h <= 4 || h >= 22)
				nightWeight = 1.0f;
			else if(h == 5 || h == 21)
				nightWeight = 0.75f;
			else if(h == 6 || h == 20)
				nightWeight = 0.35f;
			if(nightWeight <= 0.0f)
				continue;

			const float minAmbR = 44.0f * nightWeight;
			const float minAmbG = 50.0f * nightWeight;
			const float minAmbB = 68.0f * nightWeight;
			if(CTimeCycle::m_nAmbientRed[h][w] < (uint8)minAmbR)
				CTimeCycle::m_nAmbientRed[h][w] = CTimeCycle::m_nAmbientRed_Bl[h][w] = (uint8)minAmbR;
			if(CTimeCycle::m_nAmbientGreen[h][w] < (uint8)minAmbG)
				CTimeCycle::m_nAmbientGreen[h][w] = CTimeCycle::m_nAmbientGreen_Bl[h][w] = (uint8)minAmbG;
			if(CTimeCycle::m_nAmbientBlue[h][w] < (uint8)minAmbB)
				CTimeCycle::m_nAmbientBlue[h][w] = CTimeCycle::m_nAmbientBlue_Bl[h][w] = (uint8)minAmbB;

			// Clamp daytime-bright AmbientPhysicalRGB (157..249 at 0h..5h/22h) down to cool night bounce light
			const float targetObjR = 58.0f + 55.0f * (1.0f - nightWeight);
			const float targetObjG = 64.0f + 58.0f * (1.0f - nightWeight);
			const float targetObjB = 82.0f + 62.0f * (1.0f - nightWeight);
			if(CTimeCycle::m_nAmbientRed_Obj[h][w] > (uint8)targetObjR)
				CTimeCycle::m_nAmbientRed_Obj[h][w] = CTimeCycle::m_nAmbientRed_Obj_Bl[h][w] = (uint8)targetObjR;
			if(CTimeCycle::m_nAmbientGreen_Obj[h][w] > (uint8)targetObjG)
				CTimeCycle::m_nAmbientGreen_Obj[h][w] = CTimeCycle::m_nAmbientGreen_Obj_Bl[h][w] = (uint8)targetObjG;
			if(CTimeCycle::m_nAmbientBlue_Obj[h][w] > (uint8)targetObjB)
				CTimeCycle::m_nAmbientBlue_Obj[h][w] = CTimeCycle::m_nAmbientBlue_Obj_Bl[h][w] = (uint8)targetObjB;

			// Tone down [255,255,255] midnight DirectionalRGB to cool moonlight
			const float maxDirR = 46.0f + 160.0f * (1.0f - nightWeight);
			const float maxDirG = 54.0f + 160.0f * (1.0f - nightWeight);
			const float maxDirB = 78.0f + 160.0f * (1.0f - nightWeight);
			if(CTimeCycle::m_nDirectionalRed[h][w] > (uint8)maxDirR)
				CTimeCycle::m_nDirectionalRed[h][w] = (uint8)maxDirR;
			if(CTimeCycle::m_nDirectionalGreen[h][w] > (uint8)maxDirG)
				CTimeCycle::m_nDirectionalGreen[h][w] = (uint8)maxDirG;
			if(CTimeCycle::m_nDirectionalBlue[h][w] > (uint8)maxDirB)
				CTimeCycle::m_nDirectionalBlue[h][w] = (uint8)maxDirB;
		}
		if(atmosphere){ // only GL3; native sky and clouds otherwise
			if(has[CycleSlot::F_SKYTOP_R]){
				ExpandField(CTimeCycle::m_nSkyTopRed, w, g, CycleSlot::F_SKYTOP_R, 1.0f, 0, 255);
				ExpandField(CTimeCycle::m_nSkyTopGreen, w, g, CycleSlot::F_SKYTOP_G, 1.0f, 0, 255);
				ExpandField(CTimeCycle::m_nSkyTopBlue, w, g, CycleSlot::F_SKYTOP_B, 1.0f, 0, 255);
			}
			if(has[CycleSlot::F_SKYBOT_R]){
				ExpandField(CTimeCycle::m_nSkyBottomRed, w, g, CycleSlot::F_SKYBOT_R, 1.0f, 0, 255);
				ExpandField(CTimeCycle::m_nSkyBottomGreen, w, g, CycleSlot::F_SKYBOT_G, 1.0f, 0, 255);
				ExpandField(CTimeCycle::m_nSkyBottomBlue, w, g, CycleSlot::F_SKYBOT_B, 1.0f, 0, 255);
			}
			if(has[CycleSlot::F_SUNCORE_R]){
				ExpandField(CTimeCycle::m_nSunCoreRed, w, g, CycleSlot::F_SUNCORE_R, 1.0f, 0, 255);
				ExpandField(CTimeCycle::m_nSunCoreGreen, w, g, CycleSlot::F_SUNCORE_G, 1.0f, 0, 255);
				ExpandField(CTimeCycle::m_nSunCoreBlue, w, g, CycleSlot::F_SUNCORE_B, 1.0f, 0, 255);
			}
			if(has[CycleSlot::F_SUNCOR_R]){
				ExpandField(CTimeCycle::m_nSunCoronaRed, w, g, CycleSlot::F_SUNCOR_R, 1.0f, 0, 255);
				ExpandField(CTimeCycle::m_nSunCoronaGreen, w, g, CycleSlot::F_SUNCOR_G, 1.0f, 0, 255);
				ExpandField(CTimeCycle::m_nSunCoronaBlue, w, g, CycleSlot::F_SUNCOR_B, 1.0f, 0, 255);
			}
		}
		auto expandShadowField = [&](uint8 table[NUMHOURS][NUMWEATHERS], int fieldIdx, bool enforceDayFloor){
			bool hasArr[8];
			float valArr[8];
			for(int sI = 0; sI < 8; sI++){
				hasArr[sI] = g.slots[sI].has[fieldIdx];
				valArr[sI] = g.slots[sI].v[fieldIdx];
			}
			float scale = 1.0f;
			if(CustomTimecycleShadow::DetermineShadowFieldScale(g.present, hasArr, valArr, scale)){
				ExpandField(table, w, g, fieldIdx, scale, 0, 255);
			}
			if(enforceDayFloor){
				for(int h = 0; h < NUMHOURS; h++){
					float dirPeak = Max(Max((float)CTimeCycle::m_nDirectionalRed[h][w],
					                        (float)CTimeCycle::m_nDirectionalGreen[h][w]),
					                        (float)CTimeCycle::m_nDirectionalBlue[h][w]);
					table[h][w] = (uint8)CustomTimecycleShadow::EnsureDaytimeShadowFloor(
						table[h][w], h, w, dirPeak);
				}
			}
		};
		expandShadowField(CTimeCycle::m_nShadowStrength, CycleSlot::F_SHAD, true);
		expandShadowField(CTimeCycle::m_nLightShadowStrength, CycleSlot::F_LIGHTSHAD, false);
		expandShadowField(CTimeCycle::m_nPoleShadowStrength, CycleSlot::F_POLESHAD, false);
#ifdef ANDROID
		// On Android, apply mobile timecyc.json FarClip, FogStart, SpriteSize,
		// SpriteBrght, SunSize, and WaterRGBA directly as in the mobile client.
		if(has[CycleSlot::F_FARCLIP]){
			ExpandField(CTimeCycle::m_fFarClip, w, g, CycleSlot::F_FARCLIP, 1.0f, 150.0f, 1500.0f);
		}
		if(has[CycleSlot::F_FOGSTART]){
			for(int h = 0; h < NUMHOURS; h++){
				float start;
				if(!SampleField(g, h, CycleSlot::F_FOGSTART, start))
					continue;
				float maxStart = (float)CTimeCycle::m_fFarClip[h][w] - 25.0f;
				if(start < -200.0f) start = -200.0f;
				if(start > maxStart) start = maxStart;
				CTimeCycle::m_fFogStart[h][w] = (int16)lroundf(start);
			}
		}
		if(has[CycleSlot::F_SUNSIZE])
			ExpandField(CTimeCycle::m_fSunSize, w, g, CycleSlot::F_SUNSIZE, 10.0f, 1.0f, 127.0f);
		if(has[CycleSlot::F_SPRITESIZE])
			ExpandField(CTimeCycle::m_fSpriteSize, w, g, CycleSlot::F_SPRITESIZE, 10.0f, 1.0f, 127.0f);
		if(has[CycleSlot::F_SPRITEBRGHT])
			ExpandField(CTimeCycle::m_fSpriteBrightness, w, g, CycleSlot::F_SPRITEBRGHT, 10.0f, 2.5f, 127.0f);
		if(has[CycleSlot::F_WATER_R]){
			ExpandField(CTimeCycle::m_fWaterRed, w, g, CycleSlot::F_WATER_R, 1.0f, 0.0f, 255.0f);
			ExpandField(CTimeCycle::m_fWaterGreen, w, g, CycleSlot::F_WATER_G, 1.0f, 0.0f, 255.0f);
			ExpandField(CTimeCycle::m_fWaterBlue, w, g, CycleSlot::F_WATER_B, 1.0f, 0.0f, 255.0f);
			ExpandField(CTimeCycle::m_fWaterAlpha, w, g, CycleSlot::F_WATER_A, 1.0f, 0.0f, 255.0f);
		}
#elif defined(RW_GL3)
		if(atmosphere && has[CycleSlot::F_FOGSTART] && has[CycleSlot::F_FARCLIP]){
			// Import the source fog *ratio*, not its far clip. Replacing VC's
			// far distance would clip the user's map and its streaming objects.
			for(int h = 0; h < NUMHOURS; h++){
				float start, far;
				if(!SampleField(g, h, CycleSlot::F_FOGSTART, start) ||
				   !SampleField(g, h, CycleSlot::F_FARCLIP, far))
					continue;
				float nativeStart = CTimeCycle::m_fFogStart[h][w];
				float nativeFar = CTimeCycle::m_fFarClip[h][w];
				CTimeCycle::m_fFogStart[h][w] = (int16)lroundf(
					GL3Atmosphere::MapFogStart(start, far, nativeStart, nativeFar));
			}
		}
#endif
		if(has[CycleSlot::F_LIGHTGND])
			ExpandField(CTimeCycle::m_fLightsOnGroundBrightness, w, g, CycleSlot::F_LIGHTGND, 10.0f, 0, 255);
		if(atmosphere){
			if(has[CycleSlot::F_CLOUD_R]){
				// the client has one cloud colour where this game has low clouds
				// and fluffy tops; both take it
				ExpandField(CTimeCycle::m_nLowCloudsRed, w, g, CycleSlot::F_CLOUD_R, 1.0f, 0, 255);
				ExpandField(CTimeCycle::m_nLowCloudsGreen, w, g, CycleSlot::F_CLOUD_G, 1.0f, 0, 255);
				ExpandField(CTimeCycle::m_nLowCloudsBlue, w, g, CycleSlot::F_CLOUD_B, 1.0f, 0, 255);
				ExpandField(CTimeCycle::m_nFluffyCloudsTopRed, w, g, CycleSlot::F_CLOUD_R, 1.0f, 0, 255);
				ExpandField(CTimeCycle::m_nFluffyCloudsTopGreen, w, g, CycleSlot::F_CLOUD_G, 1.0f, 0, 255);
				ExpandField(CTimeCycle::m_nFluffyCloudsTopBlue, w, g, CycleSlot::F_CLOUD_B, 1.0f, 0, 255);
			}
			if(has[CycleSlot::F_FLUFFBOT_R]){
				ExpandField(CTimeCycle::m_nFluffyCloudsBottomRed, w, g, CycleSlot::F_FLUFFBOT_R, 1.0f, 0, 255);
				ExpandField(CTimeCycle::m_nFluffyCloudsBottomGreen, w, g, CycleSlot::F_FLUFFBOT_G, 1.0f, 0, 255);
				ExpandField(CTimeCycle::m_nFluffyCloudsBottomBlue, w, g, CycleSlot::F_FLUFFBOT_B, 1.0f, 0, 255);
			}
		}
	}
}

// The custom/common.zip timecyc.json wins; the loose file is a read fallback.
// Game-root common/, other ZIPs, weather presets, water and PostFX are never
// imported. Sky/cloud/sun colours and scaled fog start are GL3-only.
#ifdef ANDROID
namespace {
struct ColorcycleTable {
	bool loaded;
	int numWeathers;
	float rows[24][8][12];
};
static ColorcycleTable s_colorcycle = { false, 0, {} };

static void
LoadColorcycleDat(void)
{
	s_colorcycle.loaded = false;
	s_colorcycle.numWeathers = 0;
	std::vector<uint8> raw;
	if(!CCustomModels::ReadDataFile("data/colorcycle.dat", raw) || raw.empty())
		return;

	std::string text((const char*)raw.data(), raw.size());
	std::vector<float> vals;
	vals.reserve(23 * 8 * 12);
	const char *p = text.c_str();
	while(*p){
		while(*p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
			p++;
		if(!*p) break;
		if(*p == '#' || (*p == '/' && p[1] == '/')){
			while(*p && *p != '\n') p++;
			continue;
		}
		char *endp = nil;
		float v = std::strtof(p, &endp);
		if(endp == p){
			while(*p && *p != '\n') p++;
			continue;
		}
		if(std::isfinite(v))
			vals.push_back(v);
		p = endp;
	}

	int totalSlots = (int)(vals.size() / 12);
	int weathers = totalSlots / 8;
	if(weathers <= 0)
		return;
	if(weathers > 24)
		weathers = 24;

	float maxDiag = 0.0f;
	for(int w = 0; w < weathers; w++){
		for(int s = 0; s < 8; s++){
			const float *src = &vals[(w * 8 + s) * 12];
			for(int k = 0; k < 12; k++)
				s_colorcycle.rows[w][s][k] = src[k];
			if(src[0] > maxDiag) maxDiag = src[0];
			if(src[5] > maxDiag) maxDiag = src[5];
			if(src[10] > maxDiag) maxDiag = src[10];
		}
	}
	// SA/mobile colorcycle.dat uses PS2 2x colour scale (~2.0 diagonal); normalize to 1x.
	if(maxDiag > 1.35f){
		for(int w = 0; w < weathers; w++){
			for(int s = 0; s < 8; s++){
				float *r = s_colorcycle.rows[w][s];
				r[0] *= 0.5f; r[1] *= 0.5f; r[2] *= 0.5f;
				r[4] *= 0.5f; r[5] *= 0.5f; r[6] *= 0.5f;
				r[8] *= 0.5f; r[9] *= 0.5f; r[10] *= 0.5f;
			}
		}
	}
	s_colorcycle.numWeathers = weathers;
	s_colorcycle.loaded = true;
	CUSTOM_LOG("common.bpc data/colorcycle.dat: %d weather(s) x 8 time slots loaded\n", weathers);
}

static void
SampleColorcycleWeather(int w, float fHour, float out12[12])
{
	if(w < 0) w = 0;
	if(w >= s_colorcycle.numWeathers) w = s_colorcycle.numWeathers - 1;
	while(fHour < 0.0f) fHour += 24.0f;
	while(fHour >= 24.0f) fHour -= 24.0f;

	int s0 = 7, s1 = 0;
	float t = 0.0f;
	if(fHour >= (float)CycleHours[7]){
		s0 = 7; s1 = 0;
		t = (fHour - (float)CycleHours[7]) / (float)(24 - CycleHours[7]);
	}else{
		int i = 0;
		while(i < 6 && !(fHour >= (float)CycleHours[i] && fHour < (float)CycleHours[i + 1]))
			i++;
		s0 = i; s1 = i + 1;
		t = (fHour - (float)CycleHours[i]) / (float)(CycleHours[i + 1] - CycleHours[i]);
	}
	if(t < 0.0f) t = 0.0f;
	if(t > 1.0f) t = 1.0f;
	for(int k = 0; k < 12; k++)
		out12[k] = s_colorcycle.rows[w][s0][k] + (s_colorcycle.rows[w][s1][k] - s_colorcycle.rows[w][s0][k]) * t;
}
} // namespace

void
CCustomTimecycle::EvaluateColorGrade(float fHour, int oldWeather, int newWeather, float weatherInterp, float outRows12[12])
{
	static const float id12[12] = { 1,0,0,0, 0,1,0,0, 0,0,1,0 };
	if(!s_colorcycle.loaded || s_colorcycle.numWeathers <= 0){
		memcpy(outRows12, id12, sizeof(id12));
		return;
	}
	float a[12], b[12];
	SampleColorcycleWeather(oldWeather, fHour, a);
	SampleColorcycleWeather(newWeather, fHour, b);
	float wi = weatherInterp < 0.0f ? 0.0f : (weatherInterp > 1.0f ? 1.0f : weatherInterp);
	for(int k = 0; k < 12; k++)
		outRows12[k] = a[k] + (b[k] - a[k]) * wi;
}
#endif

void
CCustomTimecycle::Apply(void)
{
#ifdef ANDROID
	// data/colorcycle.dat feeds the 3x4 grading matrix uploaded as u_colorGrade
	// by Renderer::ConstructRenderList(). Load it once; without the file the
	// matrix stays identity and the mobile shaders are unaffected.
	static bool colorcycleTried = false;
	if(!colorcycleTried){
		colorcycleTried = true;
		LoadColorcycleDat();
	}
#endif
	std::vector<uint8> data;
	bool fromCommon = false;
	if(!CCustomModels::ReadDataFile("data/timecyc.json", data, &fromCommon))
		return; // keep the game's native TIMECYC.DAT

	SlotGrid grids[BR_WEATHERS_USED];
	int weathers = 0;
	if(!ParseTimecycle(data, grids, weathers)) return;
	bool atmosphere = false;
#ifdef RW_GL3
	atmosphere = GL3Atmosphere::Enabled;
#endif
	ExpandGrids(grids, weathers, atmosphere);
	CUSTOM_LOG("%s: %d weather(s); ambient/directional light and shadow strengths applied; %s; water and PostFX unchanged\n",
		fromCommon ? "custom/common.zip:data/timecyc.json" : "custom/data/timecyc.json", weathers,
		atmosphere ? "GL3 sky, sun and cloud colours, scaled fog start" : "native sky and fog");
}

#else // CUSTOM_MODELS

void
CCustomTimecycle::Apply(void)
{
}

#ifdef ANDROID
void
CCustomTimecycle::EvaluateColorGrade(float, int, int, float, float outRows12[12])
{
	static const float id12[12] = { 1,0,0,0, 0,1,0,0, 0,0,1,0 };
	memcpy(outRows12, id12, sizeof(id12));
}
#endif

#endif // CUSTOM_MODELS
