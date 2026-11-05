#pragma once

#include <cctype>
#include <cstdint>
#include <cstring>

namespace customveh {

struct VehicleMatInput {
	bool isEditablePaint;
	uint8_t r, g, b, a;
	const char *frameName;
	const char *texName;
	float rawStructSpecular;
	bool hasMatFxEnv;
	float rawMatFxCoef;
	bool hasSaEnv;
	float saShininess;
	bool hasSaSpec;
	float saSpecularity;
};

inline float Clamp01(float v, float lo, float hi)
{
	return v < lo ? lo : (v > hi ? hi : v);
}

inline float Max2(float a, float b)
{
	return a > b ? a : b;
}

inline bool ContainsTokenCI(const char *haystack, const char *needle)
{
	if(!haystack || !needle || !*needle)
		return false;
	const size_t nlen = std::strlen(needle);
	for(const char *p = haystack; *p; ++p){
		size_t i = 0;
		while(i < nlen && p[i] &&
		      std::tolower((unsigned char)p[i]) == std::tolower((unsigned char)needle[i]))
			++i;
		if(i == nlen)
			return true;
	}
	return false;
}

inline bool IsEditableCarColorMarker(uint8_t r, uint8_t g, uint8_t b, int &outSlot)
{
	// VC/SA primary (#3CFF00) and SA tertiary/primary (#00FFFF)
	if((r == 0x3C && g == 0xFF && b == 0x00) ||
	   (r == 0x00 && g == 0xFF && b == 0xFF)){
		outSlot = 1;
		return true;
	}
	// VC/SA secondary (#FF00AF) and SA quaternary/secondary (#FF00FF)
	if((r == 0xFF && g == 0x00 && b == 0xAF) ||
	   (r == 0xFF && g == 0x00 && b == 0xFF)){
		outSlot = 2;
		return true;
	}
	outSlot = 0;
	return false;
}

inline bool IsVehicleLightMarker(uint8_t r, uint8_t g, uint8_t b)
{
	return (r == 255 && g == 175 && b == 0) ||
	       (r == 0 && g == 255 && b == 200) ||
	       (r == 185 && g == 255 && b == 0) ||
	       (r == 255 && g == 60 && b == 0);
}

inline bool IsTireMaterial(const char *frameName, const char *texName)
{
	(void)frameName;
	return ContainsTokenCI(texName, "tire") ||
	       ContainsTokenCI(texName, "tyre") ||
	       ContainsTokenCI(texName, "tread") ||
	       ContainsTokenCI(texName, "pokrish") ||
	       ContainsTokenCI(texName, "rezina") ||
	       ContainsTokenCI(texName, "shina");
}

inline bool IsMatteInteriorOrUnderbody(const char *frameName, const char *texName)
{
	if(ContainsTokenCI(frameName, "interior") ||
	   ContainsTokenCI(frameName, "salon") ||
	   ContainsTokenCI(frameName, "cockpit") ||
	   ContainsTokenCI(frameName, "seat") ||
	   ContainsTokenCI(frameName, "dash") ||
	   ContainsTokenCI(frameName, "steer") ||
	   ContainsTokenCI(frameName, "underbody") ||
	   ContainsTokenCI(frameName, "shadow") ||
	   ContainsTokenCI(frameName, "_vlo") ||
	   ContainsTokenCI(frameName, "engine") ||
	   ContainsTokenCI(frameName, "exhaust"))
		return true;

	if(ContainsTokenCI(texName, "interior") ||
	   ContainsTokenCI(texName, "salon") ||
	   ContainsTokenCI(texName, "cockpit") ||
	   ContainsTokenCI(texName, "seat") ||
	   ContainsTokenCI(texName, "leather") ||
	   ContainsTokenCI(texName, "fabric") ||
	   ContainsTokenCI(texName, "carpet") ||
	   ContainsTokenCI(texName, "torpedo") ||
	   ContainsTokenCI(texName, "underbody") ||
	   ContainsTokenCI(texName, "shadow") ||
	   ContainsTokenCI(texName, "dirt") ||
	   ContainsTokenCI(texName, "mud") ||
	   ContainsTokenCI(texName, "nomer") ||
	   ContainsTokenCI(texName, "plate") ||
	   ContainsTokenCI(texName, "engine") ||
	   ContainsTokenCI(texName, "exhaust") ||
	   ContainsTokenCI(texName, "radiator"))
		return true;

	return false;
}

inline bool IsGlassMaterial(const char *frameName, const char *texName, uint8_t alpha, bool isEditablePaint)
{
	if(isEditablePaint)
		return false;
	if(alpha > 0 && alpha < 245)
		return true;
	if(ContainsTokenCI(frameName, "windscreen") ||
	   ContainsTokenCI(frameName, "window") ||
	   ContainsTokenCI(frameName, "glass") ||
	   ContainsTokenCI(frameName, "steklo") ||
	   ContainsTokenCI(frameName, "lobov"))
		return true;
	if(ContainsTokenCI(texName, "windscreen") ||
	   ContainsTokenCI(texName, "window") ||
	   ContainsTokenCI(texName, "glass") ||
	   ContainsTokenCI(texName, "steklo"))
		return true;
	return false;
}

inline bool IsLikelyBodyShellPaint(const char *frameName, const char *texName, uint8_t alpha)
{
	if(alpha < 245)
		return false;
	if(ContainsTokenCI(texName, "underbody"))
		return false;
	const bool paintTex =
		ContainsTokenCI(texName, "remap") ||
		ContainsTokenCI(texName, "kuzov") ||
		ContainsTokenCI(texName, "carpaint") ||
		ContainsTokenCI(texName, "paint") ||
		ContainsTokenCI(texName, "vinyl") ||
		ContainsTokenCI(texName, "livery") ||
		ContainsTokenCI(texName, "body");
	if(!paintTex)
		return false;
	if(!frameName || !*frameName)
		return true;
	return ContainsTokenCI(frameName, "chassis") ||
	       ContainsTokenCI(frameName, "body") ||
	       ContainsTokenCI(frameName, "bonnet") ||
	       ContainsTokenCI(frameName, "boot") ||
	       ContainsTokenCI(frameName, "door") ||
	       ContainsTokenCI(frameName, "bump") ||
	       ContainsTokenCI(frameName, "wing") ||
	       ContainsTokenCI(frameName, "fender") ||
	       ContainsTokenCI(frameName, "roof") ||
	       ContainsTokenCI(frameName, "hood") ||
	       ContainsTokenCI(frameName, "trunk") ||
	       ContainsTokenCI(frameName, "spoiler") ||
	       ContainsTokenCI(frameName, "skirt") ||
	       ContainsTokenCI(frameName, "extra");
}

inline float ComputeVehicleMaterialSpecular(const VehicleMatInput &in)
{
	if(IsTireMaterial(in.frameName, in.texName))
		return 0.0f;

	const float sStruct = (in.rawStructSpecular > 0.0f && in.rawStructSpecular <= 2.0f) ? in.rawStructSpecular : 0.0f;
	const float sMatFx = (in.hasMatFxEnv && in.rawMatFxCoef > 0.0f) ? in.rawMatFxCoef : 0.0f;
	const float sSaEnv = (in.hasSaEnv && in.saShininess > 0.0f) ? Clamp01(in.saShininess * 3.5f, 0.0f, 1.0f) : 0.0f;
	const float sSaSpec = (in.hasSaSpec && in.saSpecularity > 0.0f) ? Clamp01(in.saSpecularity * 0.65f, 0.0f, 1.0f) : 0.0f;
	const float explicitSpec = Max2(Max2(sStruct, sMatFx), Max2(sSaEnv, sSaSpec));

	if(in.isEditablePaint)
		return Clamp01(Max2(explicitSpec, 0.78f), 0.78f, 0.84f);

	if(IsVehicleLightMarker(in.r, in.g, in.b)){
		if(in.a > 0 && in.a < 245)
			return Clamp01(Max2(explicitSpec, 0.45f), 0.35f, 0.75f);
		return 0.0f;
	}

	if(IsMatteInteriorOrUnderbody(in.frameName, in.texName))
		return 0.0f;

	if(ContainsTokenCI(in.frameName, "wheel")){
		if(ContainsTokenCI(in.texName, "chrome") || ContainsTokenCI(in.texName, "hrom"))
			return 0.92f;
		if(explicitSpec > 0.01f)
			return Clamp01(explicitSpec, 0.15f, 0.82f);
		return 0.0f;
	}

	if(IsGlassMaterial(in.frameName, in.texName, in.a, in.isEditablePaint))
		return Clamp01(Max2(explicitSpec, 0.82f), 0.78f, 0.92f);

	if(ContainsTokenCI(in.texName, "chrome") ||
	   ContainsTokenCI(in.texName, "mirror") ||
	   ContainsTokenCI(in.texName, "hrom"))
		return Clamp01(Max2(explicitSpec, 0.92f), 0.90f, 0.96f);

	if(IsLikelyBodyShellPaint(in.frameName, in.texName, in.a))
		return Clamp01(Max2(explicitSpec, 0.76f), 0.74f, 0.84f);

	return Clamp01(explicitSpec, 0.0f, 0.95f);
}

} // namespace customveh
