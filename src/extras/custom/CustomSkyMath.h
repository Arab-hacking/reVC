#ifndef REVC_CUSTOM_SKY_MATH_H
#define REVC_CUSTOM_SKY_MATH_H

#include <cmath>

namespace customsky {

enum Phase {
	PHASE_RASV = 0,  // Dawn  (BR_Sky_Rasv / BR_Sky_Rasv_bl)
	PHASE_DAY = 1,   // Day   (BR_Sky_Day  / BR_Sky_Day_bl)
	PHASE_ZAK = 2,   // Dusk  (BR_Sky_Zak  / BR_Sky_Zak_bl)
	PHASE_NIGHT = 3, // Night (BR_Sky_Night / BR_Sky_Night_bl)
	PHASE_COUNT = 4
};

struct BlendState {
	Phase base;
	Phase overlay;
	float alpha; // [0..1]
};

inline float Smooth01(float x)
{
	if(!std::isfinite(x) || x <= 0.0f) return 0.0f;
	if(x >= 1.0f) return 1.0f;
	return x * x * (3.0f - 2.0f * x);
}

inline BlendState EvaluateSkyBlend(int hour, int minute, float second = 0.0f)
{
	float h = (float)hour + (float)minute / 60.0f + second / 3600.0f;
	if(!std::isfinite(h)) h = 12.0f;
	h = std::fmod(h, 24.0f);
	if(h < 0.0f) h += 24.0f;

	BlendState out = { PHASE_DAY, PHASE_DAY, 0.0f };
	if(h >= 4.5f && h < 5.5f){
		out.base = PHASE_NIGHT;
		out.overlay = PHASE_RASV;
		out.alpha = Smooth01(h - 4.5f);
	}else if(h >= 5.5f && h < 7.5f){
		out.base = PHASE_RASV;
		out.overlay = PHASE_RASV;
		out.alpha = 0.0f;
	}else if(h >= 7.5f && h < 8.5f){
		out.base = PHASE_RASV;
		out.overlay = PHASE_DAY;
		out.alpha = Smooth01(h - 7.5f);
	}else if(h >= 8.5f && h < 17.5f){
		out.base = PHASE_DAY;
		out.overlay = PHASE_DAY;
		out.alpha = 0.0f;
	}else if(h >= 17.5f && h < 18.5f){
		out.base = PHASE_DAY;
		out.overlay = PHASE_ZAK;
		out.alpha = Smooth01(h - 17.5f);
	}else if(h >= 18.5f && h < 20.5f){
		out.base = PHASE_ZAK;
		out.overlay = PHASE_ZAK;
		out.alpha = 0.0f;
	}else if(h >= 20.5f && h < 21.5f){
		out.base = PHASE_ZAK;
		out.overlay = PHASE_NIGHT;
		out.alpha = Smooth01(h - 20.5f);
	}else{
		out.base = PHASE_NIGHT;
		out.overlay = PHASE_NIGHT;
		out.alpha = 0.0f;
	}
	return out;
}

inline int StepClockHour(int hour, int delta)
{
	int next = (hour + delta) % 24;
	if(next < 0) next += 24;
	return next;
}

// Key sky presets: 00:00 (Night), 06:00 (Rasv), 12:00 (Day), 19:00 (Zak).
inline int StepPresetHour(int hour, int direction)
{
	static const int presets[4] = { 0, 6, 12, 19 };
	int h = StepClockHour(hour, 0);
	if(direction >= 0){
		for(int i = 0; i < 4; i++)
			if(presets[i] > h)
				return presets[i];
		return presets[0];
	}else{
		for(int i = 3; i >= 0; i--)
			if(presets[i] < h)
				return presets[i];
		return presets[3];
	}
}

inline float ComputeNormalizedSkyScale(float centerLength, float radius, float targetRadius = 50.0f)
{
	if(!std::isfinite(centerLength) || centerLength < 0.0f) centerLength = 0.0f;
	if(!std::isfinite(radius) || radius <= 0.0f) return 1.0f;
	float extent = centerLength + radius;
	if(extent <= 1.0e-3f || !std::isfinite(extent))
		return 1.0f;
	return targetRadius / extent;
}

} // namespace customsky

#endif
