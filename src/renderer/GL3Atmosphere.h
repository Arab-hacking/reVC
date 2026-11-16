#ifndef __GTA_GL3ATMOSPHERE_H__
#define __GTA_GL3ATMOSPHERE_H__

#include <cmath>

// Small, testable part of the GL3 atmosphere. VC determines the actual draw
// distance: the imported timecycle is only allowed to change where fog starts.
namespace GL3Atmosphere
{
extern bool Enabled;

inline float Clamp01(float v)
{
	if(!(v > 0.0f)) return 0.0f;
	return v < 1.0f ? v : 1.0f;
}

inline float MapFogStart(float sourceStart, float sourceFar,
                        float nativeStart, float nativeFar)
{
	// Bad/partial timecycles must not clip the world or make the fog disappear.
	if(!std::isfinite(sourceStart) || !std::isfinite(sourceFar) ||
	   !std::isfinite(nativeStart) || !std::isfinite(nativeFar) ||
	   sourceStart < 0.0f || sourceFar <= sourceStart ||
	   nativeFar < 20.0f)
		return nativeStart;
	float ratio = sourceStart / sourceFar;
	float end = nativeFar - 10.0f;
	float start = Clamp01(ratio) * nativeFar;
	return start < end ? start : end;
}

// A bounded, endpoint-preserving curve for aerial perspective. The same curve
// is used in header.frag; unlike a linear fog ramp it shows haze well before
// the far plane, yet always reaches the sky colour exactly at the far plane.
inline float Visibility(float distance, float fogStart, float fogEnd,
                        float cloudCoverage, float fogginess, float aboveEye = 0.0f)
{
	if(!std::isfinite(distance) || !std::isfinite(fogStart) ||
	   !std::isfinite(fogEnd) || fogEnd <= fogStart)
		return 1.0f;
	float span = fogEnd - fogStart;
	float t = Clamp01((distance - fogStart) / span);
	float weather = Clamp01(0.7f*Clamp01(cloudCoverage) + 0.6f*Clamp01(fogginess));
	float density = 1.0f + 1.2f*weather;
	float curved = -std::expm1(-density*t) / -std::expm1(-density);
	float altitude = Clamp01(aboveEye / span);
	float weight = 0.6f * (1.0f - 0.35f*altitude);
	return 1.0f - (t + (curved - t)*weight);
}
} // namespace GL3Atmosphere

#endif // __GTA_GL3ATMOSPHERE_H__
