#include "common.h"

#include "CustomZones.h"

#include <cstdlib>
#include <cstring>
#include <vector>

#include "Clock.h"
#include "CustomModels.h"
#include "DebugLog.h"
#include "PlayerInfo.h"
#include "Timer.h"
#include "Weather.h"

namespace {

struct ZoneDef {
	float minX, minY, minZ, maxX, maxY, maxZ;
	float brightness;
	float transition;	// seconds to blend brightness in; 0 = instant
	int weather;		// eWeather value, or -1 to leave weather alone
	int clockH, clockM;	// pinned time of day, or clockH < 0 to leave it
};

std::vector<ZoneDef> s_zones;
bool s_loaded = false;
const ZoneDef *s_current = nil;
float s_brightness = 1.0f;

// data/weather.json snow block
bool s_snowEnabled = false;
char s_snowTexture[64] = "";
uint8 s_snowColour[4] = { 255, 255, 255, 255 };
float s_snowAmount = 0.0f;
float s_snowTile = 1.0f;

const char *
FindKey(const char *begin, const char *end, const char *key)
{
	const size_t klen = strlen(key);
	if(end - begin < (ptrdiff_t)klen) return nil;
	for(const char *p = begin; p + klen <= end; p++)
		if(memcmp(p, key, klen) == 0)
			return p;
	return nil;
}

bool
ParseNumber(const char *&p, const char *end, float &out)
{
	while(p < end && (*p == ' ' || *p == '\t' || *p == ',' || *p == ':' ||
	                  *p == '[' || *p == '\r' || *p == '\n'))
		p++;
	if(p >= end) return false;
	char *stop = nil;
	out = (float)strtod(p, &stop);
	if(stop == p) return false;
	p = stop;
	return true;
}

bool
KeyNumber(const char *begin, const char *end, const char *key, float &out)
{
	const char *p = FindKey(begin, end, key);
	if(p == nil) return false;
	p += strlen(key);
	return ParseNumber(p, end, out);
}

bool
KeyArray(const char *begin, const char *end, const char *key, float *out, int count)
{
	const char *p = FindKey(begin, end, key);
	if(p == nil) return false;
	p = FindKey(p, end, "[");
	if(p == nil) return false;
	p++;
	for(int i = 0; i < count; i++)
		if(!ParseNumber(p, end, out[i]))
			return false;
	return true;
}

void
ParseMapZonesJson(const uint8 *data, size_t size)
{
	const char *base = (const char*)data;
	const char *end = base + size;
	const char *p = base;
	while(p < end){
		const char *obj = FindKey(p, end, "{");
		if(obj == nil) break;
		const char *objEnd = FindKey(obj, end, "}");
		if(objEnd == nil) break;
		ZoneDef z;
		memset(&z, 0, sizeof(z));
		z.brightness = 1.0f;
		z.transition = 0.0f;
		z.weather = -1;
		z.clockH = -1;
		z.clockM = 0;
		float minv[3], maxv[3], clockv[2], f;
		if(KeyArray(obj, objEnd, "\"min\"", minv, 3) &&
		   KeyArray(obj, objEnd, "\"max\"", maxv, 3)){
			z.minX = minv[0] < maxv[0] ? minv[0] : maxv[0];
			z.minY = minv[1] < maxv[1] ? minv[1] : maxv[1];
			z.minZ = minv[2] < maxv[2] ? minv[2] : maxv[2];
			z.maxX = minv[0] < maxv[0] ? maxv[0] : minv[0];
			z.maxY = minv[1] < maxv[1] ? maxv[1] : minv[1];
			z.maxZ = minv[2] < maxv[2] ? maxv[2] : minv[2];
			if(KeyNumber(obj, objEnd, "\"brightness\"", f)) z.brightness = f;
			if(KeyNumber(obj, objEnd, "\"transition\"", f)) z.transition = f;
			if(KeyNumber(obj, objEnd, "\"weather\"", f)) z.weather = (int)f;
			if(KeyArray(obj, objEnd, "\"clock\"", clockv, 2)){
				z.clockH = (int)clockv[0];
				z.clockM = (int)clockv[1];
			}
			s_zones.push_back(z);
		}
		p = objEnd + 1;
	}
}

void
ParseMapZon(const uint8 *data, size_t size)
{
	// The game's zone file is a list of boxes between zone/end markers:
	// six floats per line (x1 y1 z1 x2 y2 z2), anything else is ignored.
	const char *p = (const char*)data;
	const char *end = p + size;
	while(p < end){
		float v[6];
		const char *q = p;
		int got = 0;
		while(got < 6 && ParseNumber(q, end, v[got])) got++;
		if(got == 6){
			ZoneDef z;
			memset(&z, 0, sizeof(z));
			z.minX = v[0] < v[3] ? v[0] : v[3];
			z.minY = v[1] < v[4] ? v[1] : v[4];
			z.minZ = v[2] < v[5] ? v[2] : v[5];
			z.maxX = v[0] < v[3] ? v[3] : v[0];
			z.maxY = v[1] < v[4] ? v[4] : v[1];
			z.maxZ = v[2] < v[5] ? v[5] : v[2];
			z.brightness = 1.0f;
			z.weather = -1;
			z.clockH = -1;
			s_zones.push_back(z);
			p = q;
		}
		while(p < end && *p != '\n') p++;
		if(p < end) p++;
	}
}

void
ParseWeatherJson(const uint8 *data, size_t size)
{
	const char *base = (const char*)data;
	const char *end = base + size;
	const char *snow = FindKey(base, end, "\"snow\"");
	if(snow == nil) return;
	const char *win = snow + 4096 < end ? snow + 4096 : end;
	const char *en = FindKey(snow, win, "\"enabled\"");
	if(en != nil){
		const char *colon = FindKey(en, win, ":");
		if(colon != nil){
			while(colon < win && (*colon == ':' || *colon == ' ' || *colon == '\t')) colon++;
			s_snowEnabled = colon + 4 <= win && memcmp(colon, "true", 4) == 0;
		}
	}
	const char *tex = FindKey(snow, win, "\"texture\"");
	if(tex != nil){
		const char *q1 = FindKey(tex, win, "\"");
		// skip the key's closing quote pair: find the value string after ':'
		const char *colon = q1 != nil ? FindKey(q1, win, ":") : nil;
		const char *v1 = colon != nil ? FindKey(colon, win, "\"") : nil;
		const char *v2 = v1 != nil ? FindKey(v1 + 1, win, "\"") : nil;
		if(v2 != nil){
			size_t n = (size_t)(v2 - (v1 + 1));
			if(n >= sizeof(s_snowTexture)) n = sizeof(s_snowTexture) - 1;
			memcpy(s_snowTexture, v1 + 1, n);
			s_snowTexture[n] = '\0';
		}
	}
	float col[4];
	if(KeyArray(snow, win, "\"color\"", col, 4))
		for(int i = 0; i < 4; i++)
			s_snowColour[i] = (uint8)(col[i] < 0.0f ? 0 : (col[i] > 255.0f ? 255 : (int)col[i]));
	KeyNumber(snow, win, "\"amount\"", s_snowAmount);
	KeyNumber(snow, win, "\"tile\"", s_snowTile);
	if(s_snowEnabled && s_snowTexture[0] == '\0')
		s_snowEnabled = false;
}

void
Load(void)
{
	if(s_loaded) return;
	s_loaded = true;
	std::vector<uint8> bytes;
	if(CCustomModels::ReadCommonDataFile("data/mapzones.json", bytes) && !bytes.empty()){
		bytes.push_back('\0');
		ParseMapZonesJson(bytes.data(), bytes.size());
	}else if(CCustomModels::ReadCommonDataFile("data/map.zon", bytes) && !bytes.empty()){
		bytes.push_back('\0');
		ParseMapZon(bytes.data(), bytes.size());
	}
	if(!s_zones.empty())
		REVC_LOG_INFO("mapzones: %u zone(s) loaded", (unsigned)s_zones.size());
	bytes.clear();
	if(CCustomModels::ReadCommonDataFile("data/weather.json", bytes) && !bytes.empty()){
		bytes.push_back('\0');
		ParseWeatherJson(bytes.data(), bytes.size());
		REVC_LOG_INFO("weather.json: snow %s (texture '%s', amount %.2f, tile %.2f)",
			s_snowEnabled ? "enabled" : "disabled", s_snowTexture, s_snowAmount, s_snowTile);
	}
}

} // namespace

void
CCustomZones::Update(void)
{
	Load();
	const CVector pos = FindPlayerCoors();
	const ZoneDef *inside = nil;
	for(size_t i = 0; i < s_zones.size(); i++){
		const ZoneDef &z = s_zones[i];
		if(pos.x >= z.minX && pos.x <= z.maxX &&
		   pos.y >= z.minY && pos.y <= z.maxY &&
		   pos.z >= z.minZ && pos.z <= z.maxZ){
			inside = &z;
			break;
		}
	}
	if(inside != s_current){
		if(inside != nil && inside->weather >= 0 && inside->weather < WEATHER_TOTAL)
			CWeather::ForceWeatherNow((int16)inside->weather);
		s_current = inside;
	}
	if(s_current != nil && s_current->clockH >= 0)
		CClock::SetGameClock((uint8)s_current->clockH, (uint8)s_current->clockM);
	const float target = s_current != nil ? s_current->brightness : 1.0f;
	const float transition = s_current != nil ? s_current->transition : 0.0f;
	if(transition <= 0.0f){
		s_brightness = target;
	}else{
		float step = CTimer::GetTimeStepInSeconds() / transition;
		if(step > 1.0f) step = 1.0f;
		s_brightness += (target - s_brightness) * step;
	}
}

float
CCustomZones::CurrentBrightness(void)
{
	return s_brightness;
}

bool
CCustomZones::SnowEnabled(void)
{
	Load();
	return s_snowEnabled;
}

const char *
CCustomZones::SnowTexture(void)
{
	Load();
	return s_snowTexture;
}

void
CCustomZones::SnowColour(uint8 &r, uint8 &g, uint8 &b, uint8 &a)
{
	Load();
	r = s_snowColour[0];
	g = s_snowColour[1];
	b = s_snowColour[2];
	a = s_snowColour[3];
}

float
CCustomZones::SnowAmount(void)
{
	Load();
	return s_snowAmount;
}

float
CCustomZones::SnowTile(void)
{
	Load();
	return s_snowTile;
}
