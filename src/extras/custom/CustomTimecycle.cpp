#include "common.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "CustomTimecycle.h"
#include "CustomModels.h"
#include "DebugLog.h"
#include "SavehDiag.h"
#include "Timecycle.h"

#ifdef CUSTOM_MODELS

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// a small JSON reader - only what the client's files need: objects, arrays,
// numbers, strings, booleans, whitespace, a BOM at the front
// ---------------------------------------------------------------------------

struct JValue
{
	enum Type { NIL, BOOL, NUM, STR, ARR, OBJ };
	Type type;
	bool b;
	double num;
	std::string str;
	std::vector<JValue> arr;
	std::vector<std::string> keys;	// parallel to the members of an object
	std::vector<JValue> vals;

	JValue() : type(NIL), b(false), num(0) {}

	const JValue *Find(const char *key) const {
		if(type != OBJ) return nil;
		for(size_t i = 0; i < keys.size(); i++)
			if(keys[i] == key)
				return &vals[i];
		return nil;
	}
	int NumInt(int def) const { return type == NUM ? (int)lround(num) : def; }
	float NumFloat(float def) const { return type == NUM ? (float)num : def; }
};

struct JParser
{
	const char *p, *end;
	std::string err;

	void SkipWs(void) {
		while(p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' ||
		                  *p == '\xef' || *p == '\xbb' || *p == '\xbf'))
			p++;
		// tolerate // line comments (hand-edited files might have them)
		if(p+1 < end && p[0] == '/' && p[1] == '/'){
			while(p < end && *p != '\n')
				p++;
			SkipWs();
		}
	}

	bool Parse(JValue &out) {
		SkipWs();
		if(p >= end){ err = "unexpected end"; return false; }
		char c = *p;
		if(c == '{') return ParseObj(out);
		if(c == '[') return ParseArr(out);
		if(c == '"') { out.type = JValue::STR; return ParseStr(out.str); }
		if(c == 't' && end-p >= 4 && strncmp(p, "true", 4) == 0){ p += 4; out.type = JValue::BOOL; out.b = true; return true; }
		if(c == 'f' && end-p >= 5 && strncmp(p, "false", 5) == 0){ p += 5; out.type = JValue::BOOL; out.b = false; return true; }
		if(c == 'n' && end-p >= 4 && strncmp(p, "null", 4) == 0){ p += 4; out.type = JValue::NIL; return true; }
		char *e;
		double v = strtod(p, &e);
		if(e == p){ err = "bad value"; return false; }
		out.type = JValue::NUM;
		out.num = v;
		p = e;
		return true;
	}
	bool ParseObj(JValue &out) {
		out.type = JValue::OBJ;
		p++;	// {
		SkipWs();
		if(p < end && *p == '}'){ p++; return true; }
		for(;;){
			SkipWs();
			if(p >= end || *p != '"'){ err = "expected a key"; return false; }
			std::string key;
			if(!ParseStr(key)) return false;
			SkipWs();
			if(p >= end || *p != ':'){ err = "expected ':'"; return false; }
			p++;
			JValue v;
			if(!Parse(v)) return false;
			out.keys.push_back(key);
			out.vals.push_back(v);
			SkipWs();
			if(p < end && *p == '}'){ p++; return true; }
			if(p >= end || *p != ','){ err = "expected ',' or '}'"; return false; }
			p++;
		}
	}
	bool ParseArr(JValue &out) {
		out.type = JValue::ARR;
		p++;	// [
		SkipWs();
		if(p < end && *p == ']'){ p++; return true; }
		for(;;){
			JValue v;
			if(!Parse(v)) return false;
			out.arr.push_back(v);
			SkipWs();
			if(p < end && *p == ']'){ p++; return true; }
			if(p >= end || *p != ','){ err = "expected ',' or ']'"; return false; }
			p++;
		}
	}
	bool ParseStr(std::string &out) {
		p++;	// "
		out.clear();
		while(p < end && *p != '"'){
			if(*p == '\\' && p+1 < end){
				p++;
				switch(*p){
				case 'n': out += '\n'; break;
				case 't': out += '\t'; break;
				case 'r': out += '\r'; break;
				case 'b': case 'f': break;
				case 'u': {
					if(end-p >= 5){
						wchar_t w = (wchar_t)strtol(std::string(p+1, p+5).c_str(), nil, 16);
						// a UTF-8 downcast is enough for the names in there
						if(w < 0x80) out += (char)w;
						else if(w < 0x800){ out += (char)(0xC0 | (w >> 6)); out += (char)(0x80 | (w & 0x3F)); }
						else { out += (char)(0xE0 | (w >> 12)); out += (char)(0x80 | ((w >> 6) & 0x3F)); out += (char)(0x80 | (w & 0x3F)); }
						p += 4;
					}
					break;
				}
				default: out += *p; break;
				}
				p++;
			}else
				out += *p++;
		}
		if(p >= end){ err = "unterminated string"; return false; }
		p++;	// "
		return true;
	}
};

static bool
JsonParse(const std::vector<uint8> &data, JValue &out, std::string &err)
{
	// skip a UTF-8 BOM
	size_t off = 0;
	if(data.size() >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF)
		off = 3;
	JParser jp;
	jp.p = (const char*)data.data() + off;
	jp.end = (const char*)data.data() + data.size();
	if(!jp.Parse(out)){
		err = jp.err;
		return false;
	}
	return true;
}

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

	void ReadRGB(const JValue *obj, const char *key, int f0) {
		const JValue *a = obj->Find(key);
		if(a == nil || a->type != JValue::ARR || a->arr.size() < 3)
			return;
		has[f0+0] = has[f0+1] = has[f0+2] = true;
		v[f0+0] = a->arr[0].NumFloat(0);
		v[f0+1] = a->arr[1].NumFloat(0);
		v[f0+2] = a->arr[2].NumFloat(0);
	}
	void ReadRGBA(const JValue *obj, const char *key, int f0) {
		const JValue *a = obj->Find(key);
		if(a == nil || a->type != JValue::ARR || a->arr.size() < 4)
			return;
		for(int i = 0; i < 4; i++){
			has[f0+i] = true;
			v[f0+i] = a->arr[i].NumFloat(0);
		}
	}
	void ReadNum(const JValue *obj, const char *key, int f) {
		const JValue *a = obj->Find(key);
		if(a == nil || a->type != JValue::NUM)
			return;
		has[f] = true;
		v[f] = (float)a->num;
	}
	void Read(const JValue *obj) {
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
	JValue root;
	std::string err;
	if(!JsonParse(data, root, err) || root.type != JValue::ARR){
		CUSTOM_LOG("timecycle: timecyc.json could not be parsed (%s)\n", err.c_str());
		return false;
	}

	for(int w = 0; w < BR_WEATHERS_USED; w++)
		for(int sI = 0; sI < 8; sI++)
			grids[w].slots[sI].Clear();
	memset(grids, 0, sizeof(SlotGrid) * BR_WEATHERS_USED);

	int weathers = root.arr.size() < BR_WEATHERS_USED ? (int)root.arr.size() : BR_WEATHERS_USED;
	int slotsRead = 0;

	for(int w = 0; w < weathers; w++){
		const JValue &jw = root.arr[w];
		if(jw.type != JValue::ARR)
			continue;
		int n = jw.arr.size() < 8 ? (int)jw.arr.size() : 8;
		for(int sI = 0; sI < n; sI++){
			if(jw.arr[sI].type != JValue::OBJ)
				continue;
			grids[w].slots[sI].Read(&jw.arr[sI]);
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

// "timecyclePresetN.json": the client's server-driven weather presets (whole
// alternative sky/lighting states a script can switch to, not pieces of the
// day cycle). In this game nothing switches them on, so they are logged and
// left out - the day cycle comes from timecyc.json alone.
static void
LogPreset(const JValue &root, const char *name)
{
	const JValue *jw = root.Find("Weather");
	const JValue *jp = root.Find("TimePeriod");
	int weather = jw ? jw->NumInt(-1) : -1;
	int period = jp ? jp->NumInt(-1) : -1;
	if(weather < 0 || weather >= BR_WEATHERS_USED || period < 0){
		CUSTOM_LOG("timecycle: %s - weather %d / period %d not applicable\n", name, weather, period);
		return;
	}
	(void)period;
	const JValue *top = root.Find("SkyTopRGB");
	const JValue *bot = root.Find("SkyBottomRGB");
	CUSTOM_LOG("timecycle: %s - weather %d, preset %d (server-driven, not part of the day cycle, not applied)\n",
		name, weather, period);
	(void)top; (void)bot;
}

// one field of one weather, interpolated over the day on the client's slot
// hours; where the client carries nothing, the game's own value stays
template<typename T>
static void
ExpandField(T arr[NUMHOURS][NUMWEATHERS], int w, const SlotGrid &grid, int f,
            float scale, float lo, float hi)
{
	for(int h = 0; h < NUMHOURS; h++){
		int s0, s1;
		float t;
		if(h >= CycleHours[7]){
			// the day wraps: hour 22..23 blends into hour 0 of the next one
			s0 = 7; s1 = 0;
			t = (h - CycleHours[7]) / (float)(24 - CycleHours[7]);
		}else{
			int i = 0;
			while(i < 6 && !(h >= CycleHours[i] && h < CycleHours[i+1]))
				i++;
			s0 = i;
			s1 = i+1;
			t = (h - CycleHours[i]) / (float)(CycleHours[i+1] - CycleHours[i]);
		}
		bool has0 = grid.present[s0] && grid.slots[s0].has[f];
		bool has1 = grid.present[s1] && grid.slots[s1].has[f];
		if(!has0 && !has1)
			continue;
		float a = has0 ? grid.slots[s0].v[f] : grid.slots[s1].v[f];
		float b = has1 ? grid.slots[s1].v[f] : grid.slots[s0].v[f];
		float val = (a + (b - a) * t) * scale;
		if(val < lo) val = lo;
		if(val > hi) val = hi;
		arr[h][w] = (T)(int)lroundf(val);
	}
}

// the values live in the game's tables now: hour rows 0..23, weather columns
static void
ExpandGrids(const SlotGrid grids[BR_WEATHERS_USED], int weathers)
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
		if(has[CycleSlot::F_SUNSIZE])
			ExpandField(CTimeCycle::m_fSunSize, w, g, CycleSlot::F_SUNSIZE, 10.0f, -128, 127);
		if(has[CycleSlot::F_SPRITESIZE])
			ExpandField(CTimeCycle::m_fSpriteSize, w, g, CycleSlot::F_SPRITESIZE, 10.0f, -128, 127);
		if(has[CycleSlot::F_SPRITEBRGHT])
			ExpandField(CTimeCycle::m_fSpriteBrightness, w, g, CycleSlot::F_SPRITEBRGHT, 10.0f, -128, 127);
		if(has[CycleSlot::F_SHAD])
			ExpandField(CTimeCycle::m_nShadowStrength, w, g, CycleSlot::F_SHAD, 1.0f, 0, 255);
		if(has[CycleSlot::F_LIGHTSHAD])
			ExpandField(CTimeCycle::m_nLightShadowStrength, w, g, CycleSlot::F_LIGHTSHAD, 1.0f, 0, 255);
		if(has[CycleSlot::F_POLESHAD])
			ExpandField(CTimeCycle::m_nPoleShadowStrength, w, g, CycleSlot::F_POLESHAD, 1.0f, 0, 255);
		if(has[CycleSlot::F_FARCLIP])
			ExpandField(CTimeCycle::m_fFarClip, w, g, CycleSlot::F_FARCLIP, 1.0f, 0, 32767);
		if(has[CycleSlot::F_FOGSTART])
			ExpandField(CTimeCycle::m_fFogStart, w, g, CycleSlot::F_FOGSTART, 1.0f, -32768, 32767);
		if(has[CycleSlot::F_LIGHTGND])
			ExpandField(CTimeCycle::m_fLightsOnGroundBrightness, w, g, CycleSlot::F_LIGHTGND, 10.0f, 0, 255);
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
		if(has[CycleSlot::F_WATER_R]){
			ExpandField(CTimeCycle::m_fWaterRed, w, g, CycleSlot::F_WATER_R, 1.0f, 0, 255);
			ExpandField(CTimeCycle::m_fWaterGreen, w, g, CycleSlot::F_WATER_G, 1.0f, 0, 255);
			ExpandField(CTimeCycle::m_fWaterBlue, w, g, CycleSlot::F_WATER_B, 1.0f, 0, 255);
			ExpandField(CTimeCycle::m_fWaterAlpha, w, g, CycleSlot::F_WATER_A, 1.0f, 0, 255);
		}
	}
}

// "weather.json" - the snow and friends of the client; logged, not used yet
static void
LogWeatherJson(void)
{
	std::vector<uint8> data;
	if(!CCustomModels::ReadDataFile("data/weather.json", data))
		return;
	JValue root;
	std::string err;
	if(!JsonParse(data, root, err) || root.type != JValue::OBJ)
		return;
	const JValue *snow = root.Find("snow");
	if(snow){
		const JValue *en = snow->Find("enabled");
		CUSTOM_LOG("weather: the client's snow is %s (this game has no snow layer)\n",
			en && en->type == JValue::BOOL && en->b ? "enabled" : "disabled");
	}
}

void
CCustomTimecycle::Apply(void)
{
	std::vector<uint8> data;
	if(!CCustomModels::ReadDataFile("data/timecyc.json", data)){
		CUSTOM_LOG("timecycle: no data/timecyc.json in the custom folder - the game's own TIMECYC.DAT stays\n");
		return;
	}

	SlotGrid grids[BR_WEATHERS_USED];
	int weathers = 0;
	if(!ParseTimecycle(data, grids, weathers))
		return;

	// the client's server-driven presets are logged, not applied
	int presets = 0;
	for(int i = 1; i <= 16; i++){
		char rel[64];
		snprintf(rel, sizeof(rel), "data/timecyclePresets/timecyclePreset%d.json", i);
		std::vector<uint8> pdata;
		if(!CCustomModels::ReadDataFile(rel, pdata))
			continue;
		JValue root;
		std::string err;
		if(!JsonParse(pdata, root, err) || root.type != JValue::OBJ){
			CUSTOM_LOG("timecycle: %s could not be parsed (%s)\n", rel, err.c_str());
			continue;
		}
		LogPreset(root, rel);
		presets++;
	}

	ExpandGrids(grids, weathers);
	CUSTOM_LOG("timecycle: timecyc.json applied - %d weather(s), %d server preset file(s) logged; sky, sun, fog, far clip, ambient and water follow the client now\n",
		weathers, presets);
	CUSTOM_LOG("timecycle: note - the client's PostFX1/PostFX2 layers have no counterpart in this game and are skipped\n");
	LogWeatherJson();
}

#else // CUSTOM_MODELS

void
CCustomTimecycle::Apply(void)
{
}

#endif // CUSTOM_MODELS
