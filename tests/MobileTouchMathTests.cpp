#include "MobileTouchMath.h"
#include <cassert>
#include <cmath>

int main()
{
	using namespace MobileTouchMath;

	// Exact & case-insensitive matches for user's 185 mobile VC HUD textures
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

	// Analog stick deadzone, walk/run gradient, and axis sign (-128 up/left, +127 down/right)
	float kx = 0.0f, ky = 0.0f;
	int16_t ax = 0, ay = 0;

	EvalStick(3.0f, -4.0f, 100.0f, 0.14f, &kx, &ky, &ax, &ay);
	assert(ax == 0 && ay == 0);

	// Full forward push (screen up -> dy < 0 -> LeftStickY == -127)
	EvalStick(0.0f, -150.0f, 100.0f, 0.14f, &kx, &ky, &ax, &ay);
	assert(std::fabs(kx) < 0.001f && std::fabs(ky + 100.0f) < 0.001f);
	assert(ax == 0 && ay == -127);

	// Gentle push (~35% radius -> walking speed < 60)
	EvalStick(38.0f, 0.0f, 100.0f, 0.14f, &kx, &ky, &ax, &ay);
	assert(ax > 0 && ax < 60 && ay == 0);

	return 0;
}
