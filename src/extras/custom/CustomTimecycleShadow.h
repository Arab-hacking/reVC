#ifndef __GTA_CUSTOMTIMECYCLESHADOW_H__
#define __GTA_CUSTOMTIMECYCLESHADOW_H__

#include <cmath>

namespace CustomTimecycleShadow
{

// Mobile/BR timecyc.json files frequently store "Shad": 0 across all 8 day slots
// (because legacy planar blob shadows were disabled in favor of shader shadow maps)
// or store normalized [0..1] floats. Importing literal zeros into VC's table would
// disable both CShadows and GL3 directional shadow strength for all 24 hours.
inline bool DetermineShadowFieldScale(const bool present[8], const bool hasField[8], const float values[8], float &outScale)
{
	bool any = false;
	float maxVal = 0.0f;
	for(int i = 0; i < 8; i++){
		if(!present[i] || !hasField[i] || !std::isfinite(values[i]))
			continue;
		any = true;
		if(values[i] > maxVal)
			maxVal = values[i];
	}
	if(!any || !(maxVal > 0.0f)){
		outScale = 0.0f;
		return false; // preserve VC's native shadow curve instead of zeroing shadows
	}
	outScale = (maxVal <= 2.0f) ? 180.0f : 1.0f;
	return true;
}

inline int EnsureDaytimeShadowFloor(int importedStrength, int hour, int weather, float dirPeakByte)
{
	int s = importedStrength;
	if(s < 0) s = 0;
	if(s > 255) s = 255;
	// Clear/sunny weathers with bright sun directional light must retain visible daytime shadows
	if((weather == 0 || weather == 1) && hour >= 7 && hour <= 18 && dirPeakByte >= 64.0f && s < 115)
		return 115;
	return s;
}

} // namespace CustomTimecycleShadow

#endif // __GTA_CUSTOMTIMECYCLESHADOW_H__
