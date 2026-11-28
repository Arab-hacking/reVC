#include <cassert>
#include <cmath>
#include <initializer_list>
#include <limits>

#include "../src/renderer/GL3Atmosphere.h"

static bool Near(float a, float b, float eps = 1.0e-5f)
{
	return std::fabs(a - b) < eps;
}

int main()
{
	using namespace GL3Atmosphere;

	// Match the BR fog interval to the *native* VC far clip, never replace it.
	assert(Near(MapFogStart(750, 1500, 180, 600), 300));
	assert(Near(MapFogStart(3090, 3100, 180, 600), 590));
	assert(Near(MapFogStart(0, 1500, 180, 600), 0));
	assert(Near(MapFogStart(750, 0, 180, 600), 180));
	assert(Near(MapFogStart(1600, 1500, 180, 600), 180));
	assert(Near(MapFogStart(std::numeric_limits<float>::quiet_NaN(), 1500, 180, 600), 180));
	assert(Near(MapFogStart(750, std::numeric_limits<float>::infinity(), 180, 600), 180));

	// Aerial perspective begins only at the fog start and meets the sky at far.
	for(float cloud : {0.0f, 0.5f, 1.0f}){
		assert(Near(Visibility(0, 180, 600, cloud, 0), 1));
		assert(Near(Visibility(180, 180, 600, cloud, 0), 1));
		assert(Near(Visibility(600, 180, 600, cloud, 0), 0));
		assert(Near(Visibility(900, 180, 600, cloud, 0), 0));
		float previous = 1.0f;
		for(float d = 190; d <= 600; d += 10){
			float visible = Visibility(d, 180, 600, cloud, 0);
			assert(std::isfinite(visible) && visible >= 0 && visible <= previous);
			previous = visible;
		}
	}
	assert(Visibility(400, 180, 600, 1, 1) < Visibility(400, 180, 600, 0, 0));
	assert(Visibility(400, 180, 600, 0, 0, 150) > Visibility(400, 180, 600, 0, 0));
	assert(Near(Visibility(400, 600, 180, 1, 1), 1));
	return 0;
}
