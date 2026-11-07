#pragma once

#include <cmath>
#include <stdint.h>

namespace MobileTouchMath {

inline float
ClampF(float v, float lo, float hi)
{
	return (v < lo) ? lo : ((v > hi) ? hi : v);
}

inline float
MinF(float a, float b)
{
	return (a < b) ? a : b;
}

inline float
MaxF(float a, float b)
{
	return (a > b) ? a : b;
}

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

struct MobileScreenMetrics {
	float screenW;
	float screenH;
	float pxPerDp;
	float dpUnitPx;
	float widthDp;
	float heightDp;
	float shortSideDp;

	float safeLeftPx;
	float safeTopPx;
	float safeRightPx;
	float safeBottomPx;

	float marginLeftPx;
	float marginRightPx;
	float marginTopPx;
	float marginBottomPx;

	float hudUnitPx;
	float radarLeftPx;
	float radarTopPx;
	float radarSizePx;

	float Dp(float dp) const { return dp * dpUnitPx; }
};

inline MobileScreenMetrics
ComputeScreenMetrics(int screenW, int screenH,
                     float density, float dpi,
                     int safeLeft, int safeTop, int safeRight, int safeBottom)
{
	MobileScreenMetrics m;
	m.screenW = (screenW > 1) ? (float)screenW : 1280.0f;
	m.screenH = (screenH > 1) ? (float)screenH : 720.0f;

	const float shortPx = MinF(m.screenW, m.screenH);
	if (density > 0.4f) {
		m.pxPerDp = density;
	} else if (dpi > 40.0f) {
		m.pxPerDp = dpi / 160.0f;
	} else {
		// Typical landscape phone height is ~380dp
		m.pxPerDp = shortPx / 380.0f;
	}
	m.pxPerDp = ClampF(m.pxPerDp, 0.75f, 4.5f);

	m.widthDp = m.screenW / m.pxPerDp;
	m.heightDp = m.screenH / m.pxPerDp;
	m.shortSideDp = MinF(m.widthDp, m.heightDp);

	// True 1dp on phones (350..480dp short side), proportional shrink on tiny
	// split-screen windows (<350dp), and bounded ergonomic boost on tablets (>480dp).
	if (m.shortSideDp < 350.0f) {
		m.dpUnitPx = m.pxPerDp * ClampF(m.shortSideDp / 350.0f, 0.55f, 1.0f);
	} else if (m.shortSideDp > 480.0f) {
		const float tabletBoost = ClampF(1.0f + (m.shortSideDp - 480.0f) * 0.0006f, 1.0f, 1.24f);
		m.dpUnitPx = m.pxPerDp * tabletBoost;
	} else {
		m.dpUnitPx = m.pxPerDp;
	}

	m.safeLeftPx   = ClampF((float)safeLeft,   0.0f, m.screenW * 0.16f);
	m.safeRightPx  = ClampF((float)safeRight,  0.0f, m.screenW * 0.16f);
	m.safeTopPx    = ClampF((float)safeTop,    0.0f, m.screenH * 0.14f);
	m.safeBottomPx = ClampF((float)safeBottom, 0.0f, m.screenH * 0.14f);

	m.marginLeftPx   = MaxF(m.safeLeftPx   + m.Dp(12.0f), m.Dp(18.0f));
	m.marginRightPx  = MaxF(m.safeRightPx  + m.Dp(12.0f), m.Dp(18.0f));
	m.marginTopPx    = MaxF(m.safeTopPx    + m.Dp(8.0f),  m.Dp(12.0f));
	m.marginBottomPx = MaxF(m.safeBottomPx + m.Dp(10.0f), m.Dp(14.0f));

	const float ps2UnitY = m.screenH / 448.0f;
	m.hudUnitPx = ClampF(m.Dp(0.95f), ps2UnitY * 0.72f, ps2UnitY * 1.22f);

	m.radarSizePx = ClampF(m.Dp(94.0f), shortPx * 0.18f, shortPx * 0.26f);
	m.radarLeftPx = m.marginLeftPx + m.Dp(4.0f);
	m.radarTopPx  = m.marginTopPx + m.Dp(4.0f);

	return m;
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

inline void
EvalMapPinch(float prevAx, float prevAy, float prevBx, float prevBy,
             float currAx, float currAy, float currBx, float currBy,
             float minDistPx, float minMapSize, float maxMapSize,
             float *inOutMapSize, float *inOutCenterX, float *inOutCenterY)
{
	if (!inOutMapSize || !inOutCenterX || !inOutCenterY)
		return;

	const float prevMidX = 0.5f * (prevAx + prevBx);
	const float prevMidY = 0.5f * (prevAy + prevBy);
	const float currMidX = 0.5f * (currAx + currBx);
	const float currMidY = 0.5f * (currAy + currBy);

	const float pdx = prevBx - prevAx;
	const float pdy = prevBy - prevAy;
	const float cdx = currBx - currAx;
	const float cdy = currBy - currAy;
	const float prevDist = std::sqrt(pdx * pdx + pdy * pdy);
	const float currDist = std::sqrt(cdx * cdx + cdy * cdy);

	const float oldSize = MaxF(*inOutMapSize, minMapSize);
	float newSize = oldSize;
	if (prevDist >= minDistPx && currDist >= minDistPx) {
		const float zoomFactor = ClampF(currDist / prevDist, 0.80f, 1.25f);
		newSize = ClampF(oldSize * zoomFactor, minMapSize, maxMapSize);
	}

	const float ratio = newSize / oldSize;
	*inOutMapSize = newSize;
	*inOutCenterX = currMidX - (prevMidX - *inOutCenterX) * ratio;
	*inOutCenterY = currMidY - (prevMidY - *inOutCenterY) * ratio;
}

inline void
ComputeMenuHorizontalSpan(float screenW, float screenH,
                          float safeLeftPx, float safeRightPx,
                          float *outLeftPx, float *outRightPx)
{
	const float w = (screenW > 1.0f) ? screenW : 1280.0f;
	const float h = (screenH > 1.0f) ? screenH : 720.0f;
	const float minPad = (h / 448.0f) * 14.0f;
	const float left = MaxF(ClampF(safeLeftPx, 0.0f, w * 0.16f), minPad);
	const float right = w - MaxF(ClampF(safeRightPx, 0.0f, w * 0.16f), minPad);
	if (outLeftPx) *outLeftPx = left;
	if (outRightPx) *outRightPx = MaxF(left + 1.0f, right);
}

inline float
MenuVirtualToScreenX(float vx, float screenW, float screenH,
                     float safeLeftPx, float safeRightPx)
{
	float left = 0.0f, right = screenW;
	ComputeMenuHorizontalSpan(screenW, screenH, safeLeftPx, safeRightPx, &left, &right);
	return left + (vx / 640.0f) * (right - left);
}

inline void
ComputeDynamicStickOrigin(float touchX, float touchY,
                          const MobileScreenMetrics &m, float stickRadius,
                          float *outBaseX, float *outBaseY)
{
	const float r = MaxF(stickRadius, 1.0f);
	const float minX = m.marginLeftPx + r;
	const float maxX = MaxF(minX, m.screenW * 0.42f);
	const float minY = MaxF(m.radarTopPx + m.radarSizePx + m.Dp(12.0f) + r, m.screenH * 0.34f);
	const float maxY = MaxF(minY, m.screenH - m.marginBottomPx - r);
	if (outBaseX) *outBaseX = ClampF(touchX, minX, maxX);
	if (outBaseY) *outBaseY = ClampF(touchY, minY, maxY);
}

inline bool
IsInDynamicStickZone(float touchX, float touchY, const MobileScreenMetrics &m)
{
	const float topGuard = MaxF(m.radarTopPx + m.radarSizePx + m.Dp(8.0f), m.screenH * 0.30f);
	return touchX >= 0.0f && touchX <= m.screenW * 0.44f &&
	       touchY >= topGuard && touchY <= m.screenH;
}

struct SprintJumpDoubleTap {
	uint32_t lastDownMs = 0;
	bool hasPrevDown = false;
	int jumpPulseFrames = 0;

	bool OnPress(uint32_t nowMs, uint32_t doubleTapWindowMs = 320)
	{
		if (hasPrevDown && (nowMs - lastDownMs) <= doubleTapWindowMs) {
			hasPrevDown = false;
			lastDownMs = 0;
			jumpPulseFrames = 2;
			return true;
		}
		hasPrevDown = true;
		lastDownMs = nowMs;
		return false;
	}

	void Reset()
	{
		lastDownMs = 0;
		hasPrevDown = false;
		jumpPulseFrames = 0;
	}

	bool ConsumeJumpFrame()
	{
		if (jumpPulseFrames > 0) {
			jumpPulseFrames--;
			return true;
		}
		return false;
	}
};

} // namespace MobileTouchMath
