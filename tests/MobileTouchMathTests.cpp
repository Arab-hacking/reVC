#include "MobileTouchMath.h"
#include <cassert>
#include <cmath>

int main()
{
	using namespace MobileTouchMath;

	// 1. Exact & case-insensitive matches for user's 185 mobile VC HUD textures
	assert(MatchesHudTextureName("hud_break", "hud_break"));
	assert(MatchesHudTextureName("HUD_BREAK", "hud_break"));
	assert(MatchesHudTextureName("hud_Nipple", "hud_nipple"));
	assert(MatchesHudTextureName("hud_Brass-Knuckles", "hud_brass-knuckles"));
	assert(MatchesHudTextureName("hud_zoom+", "hud_zoom+"));
	assert(MatchesHudTextureName("hud_cam-toggle", "hud_cam-toggle"));
	assert(MatchesHudTextureName("hud_heliDrop", "hud_helidrop"));
	assert(MatchesHudTextureName("hud_Cellphone", "hud_cellphone"));

	// Optional "hud_" prefix tolerance in both directions
	assert(MatchesHudTextureName("punch", "hud_punch"));
	assert(MatchesHudTextureName("hud_punch", "punch"));
	assert(!MatchesHudTextureName("hud_punch", "hud_shoot"));

	// 2. Analog stick deadzone, walk/run gradient, and axis sign (-128 up/left, +127 down/right)
	float kx = 0.0f, ky = 0.0f;
	int16_t ax = 0, ay = 0;

	EvalStick(3.0f, -4.0f, 100.0f, 0.14f, &kx, &ky, &ax, &ay);
	assert(ax == 0 && ay == 0);

	EvalStick(0.0f, -150.0f, 100.0f, 0.14f, &kx, &ky, &ax, &ay);
	assert(std::fabs(kx) < 0.001f && std::fabs(ky + 100.0f) < 0.001f);
	assert(ax == 0 && ay == -127);

	EvalStick(38.0f, 0.0f, 100.0f, 0.14f, &kx, &ky, &ax, &ay);
	assert(ax > 0 && ax < 60 && ay == 0);

	// 3. DP Screen Metrics across 720p, 1080p notch phone, 1440p flagship, split-screen & tablet
	const MobileScreenMetrics hd   = ComputeScreenMetrics(1600, 720,  2.0f,  320.0f, 0,  0, 0,  0);
	const MobileScreenMetrics fhd  = ComputeScreenMetrics(2400, 1080, 2.75f, 440.0f, 88, 0, 44, 0);
	const MobileScreenMetrics qhd  = ComputeScreenMetrics(3200, 1440, 4.0f,  640.0f, 0,  0, 0,  0);

	// Physical 48dp button size stays identical in dp across 720p, 1080p, and 1440p phones
	assert(std::fabs(hd.Dp(48.0f)  / hd.pxPerDp  - 48.0f) < 0.01f);
	assert(std::fabs(fhd.Dp(48.0f) / fhd.pxPerDp - 48.0f) < 0.01f);
	assert(std::fabs(qhd.Dp(48.0f) / qhd.pxPerDp - 48.0f) < 0.01f);

	// Camera cutout safe-inset pushes left/right margins and radar clear of the punch-hole
	assert(fhd.marginLeftPx >= 88.0f + fhd.Dp(12.0f) - 0.01f);
	assert(fhd.radarLeftPx > 88.0f);
	assert(fhd.radarSizePx >= 1080.0f * 0.18f && fhd.radarSizePx <= 1080.0f * 0.26f);

	// Compact split-screen window (<350dp short side) scales down proportionally
	const MobileScreenMetrics split = ComputeScreenMetrics(960, 480, 2.5f, 400.0f, 0, 0, 0, 0);
	assert(split.shortSideDp < 200.0f);
	assert(split.dpUnitPx < split.pxPerDp);

	// 10" Tablet (>480dp short side) gets bounded ergonomic boost (<= 1.24x)
	const MobileScreenMetrics tab = ComputeScreenMetrics(2560, 1600, 2.0f, 320.0f, 0, 0, 0, 0);
	assert(tab.dpUnitPx > tab.pxPerDp && tab.dpUnitPx <= tab.pxPerDp * 1.25f);

	// Fallback DPI when density == 0
	const MobileScreenMetrics fbDpi = ComputeScreenMetrics(1920, 1080, 0.0f, 320.0f, 0, 0, 0, 0);
	assert(std::fabs(fbDpi.pxPerDp - 2.0f) < 0.01f);

	// 4. Pause Map 2-finger pinch-zoom + midpoint pan
	float mapSize = 400.0f;
	float cx = 1200.0f;
	float cy = 540.0f;
	// Spread fingers by 1.20x centered at (900, 400): map point under (900, 400) must stay at (900, 400)
	const float uBefore = (900.0f - cx) / mapSize;
	const float vBefore = (400.0f - cy) / mapSize;
	EvalMapPinch(
		800.0f, 400.0f, 1000.0f, 400.0f,
		780.0f, 400.0f, 1020.0f, 400.0f,
		20.0f, 200.0f, 2000.0f,
		&mapSize, &cx, &cy);
	assert(std::fabs(mapSize - 480.0f) < 0.01f);
	const float uAfter = (900.0f - cx) / mapSize;
	const float vAfter = (400.0f - cy) / mapSize;
	assert(std::fabs(uBefore - uAfter) < 1e-4f);
	assert(std::fabs(vBefore - vAfter) < 1e-4f);

	// 5. Widescreen Smartphone Menu Coordinate Adaptation (20:9 + camera cutout insets)
	float mLeft = 0.0f, mRight = 0.0f;
	ComputeMenuHorizontalSpan(2400.0f, 1080.0f, 88.0f, 44.0f, &mLeft, &mRight);
	assert(mLeft >= 88.0f - 0.01f);
	assert(mRight <= 2400.0f - 44.0f + 0.01f);
	// Uses full widescreen width (> 2200px on 2400px phone instead of cramped 1440px 4:3 box)
	assert((mRight - mLeft) > 2200.0f);
	const float rowLeft  = MenuVirtualToScreenX(40.0f,  2400.0f, 1080.0f, 88.0f, 44.0f);
	const float rowMid   = MenuVirtualToScreenX(320.0f, 2400.0f, 1080.0f, 88.0f, 44.0f);
	const float rowRight = MenuVirtualToScreenX(600.0f, 2400.0f, 1080.0f, 88.0f, 44.0f);
	assert(rowLeft > mLeft && rowLeft < 300.0f);
	assert(rowRight > 2100.0f && rowRight < mRight);
	assert(std::fabs(rowMid - 0.5f * (mLeft + mRight)) < 0.01f);

	// 6. Run Button: Hold = Sprint (no jump), Double-Tap = Jump pulse
	SprintJumpDoubleTap runBtn;
	assert(!runBtn.OnPress(1000));          // 1st press (tap or hold): sprint only, no jump
	assert(!runBtn.ConsumeJumpFrame());
	assert(!runBtn.OnPress(1500));          // Slow 2nd press (500ms > 320ms): no jump
	assert(!runBtn.ConsumeJumpFrame());
	assert(runBtn.OnPress(1720));           // Quick double-tap (220ms <= 320ms): JUMP!
	assert(runBtn.ConsumeJumpFrame());      // Frame 1 jump pulse
	assert(runBtn.ConsumeJumpFrame());      // Frame 2 jump pulse
	assert(!runBtn.ConsumeJumpFrame());     // Pulse finished
	assert(!runBtn.OnPress(1900));          // Immediate 3rd tap starts fresh pair (no accidental triple jump)
	assert(runBtn.OnPress(2100));           // 4th tap completes second double-tap -> JUMP again!

	// 7. Dynamic Transparent Stick: left-zone activation & safe boundary clamping
	const float stickR = fhd.Dp(56.0f);
	// Radar area in top-left is excluded from dynamic stick zone
	assert(!IsInDynamicStickZone(fhd.radarLeftPx + 20.0f, fhd.radarTopPx + 20.0f, fhd));
	// Right half of screen is camera look zone, NOT stick zone
	assert(!IsInDynamicStickZone(1600.0f, 750.0f, fhd));
	// Lower-left movement zone activates dynamic stick right at finger position
	assert(IsInDynamicStickZone(420.0f, 760.0f, fhd));
	float dynX = 0.0f, dynY = 0.0f;
	ComputeDynamicStickOrigin(420.0f, 760.0f, fhd, stickR, &dynX, &dynY);
	assert(std::fabs(dynX - 420.0f) < 0.01f && std::fabs(dynY - 760.0f) < 0.01f);
	// Touch right against the left cutout edge clamps stick base so ring stays fully visible
	ComputeDynamicStickOrigin(15.0f, 1070.0f, fhd, stickR, &dynX, &dynY);
	assert(dynX >= fhd.marginLeftPx + stickR - 0.01f);
	assert(dynY <= fhd.screenH - fhd.marginBottomPx - stickR + 0.01f);

	return 0;
}
