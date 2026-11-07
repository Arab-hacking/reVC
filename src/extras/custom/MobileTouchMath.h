#pragma once

#include <cmath>
#include <stdint.h>

namespace MobileTouchMath {

inline char
AsciiLower(char c)
{
	return (c >= 'A' && c <= 'Z') ? (char)(c + ('a' - 'A')) : c;
}

inline bool
EqualsIgnoreCase(const char *a, const char *b)
{
	if (!a || !b)
		return false;
	while (*a && *b) {
		if (AsciiLower(*a++) != AsciiLower(*b++))
			return false;
	}
	return *a == '\0' && *b == '\0';
}

inline bool
HasHudPrefix(const char *s)
{
	return s && AsciiLower(s[0]) == 'h' && AsciiLower(s[1]) == 'u' &&
	       AsciiLower(s[2]) == 'd' && s[3] == '_';
}

inline bool
MatchesHudTextureName(const char *candidateInTxd, const char *requestedName)
{
	if (!candidateInTxd || !requestedName)
		return false;
	if (EqualsIgnoreCase(candidateInTxd, requestedName))
		return true;
	if (HasHudPrefix(requestedName) && EqualsIgnoreCase(candidateInTxd, requestedName + 4))
		return true;
	if (HasHudPrefix(candidateInTxd) && EqualsIgnoreCase(candidateInTxd + 4, requestedName))
		return true;
	return false;
}

inline void
EvalStick(float dx, float dy, float maxRadius, float deadzoneFrac,
          float *outKnobOffsetX, float *outKnobOffsetY,
          int16_t *outAxisX, int16_t *outAxisY)
{
	const float maxR = (maxRadius > 1.0f) ? maxRadius : 1.0f;
	const float dist = std::sqrt(dx * dx + dy * dy);

	if (dist > maxR) {
		if (outKnobOffsetX) *outKnobOffsetX = (dx / dist) * maxR;
		if (outKnobOffsetY) *outKnobOffsetY = (dy / dist) * maxR;
	} else {
		if (outKnobOffsetX) *outKnobOffsetX = dx;
		if (outKnobOffsetY) *outKnobOffsetY = dy;
	}

	const float dead = maxR * deadzoneFrac;
	float normX = 0.0f;
	float normY = 0.0f;
	if (dist > dead && maxR > dead) {
		float eff = (dist - dead) / (maxR - dead);
		if (eff > 1.0f)
			eff = 1.0f;
		normX = (dx / dist) * eff;
		normY = (dy / dist) * eff;
	}

	auto clampAxis = [](float v) -> int16_t {
		int32_t iv = (int32_t)(v * 127.0f);
		if (iv < -128) iv = -128;
		if (iv > 127) iv = 127;
		return (int16_t)iv;
	};
	if (outAxisX) *outAxisX = clampAxis(normX);
	if (outAxisY) *outAxisY = clampAxis(normY);
}

} // namespace MobileTouchMath
