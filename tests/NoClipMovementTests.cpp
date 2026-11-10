#include "../src/core/NoClipMovement.h"

#include <cassert>
#include <cmath>

static bool Near(float a, float b) { return std::fabs(a - b) < 0.001f; }

int main()
{
	NoClipVector d = NoClipMoveDirection(0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f);
	assert(Near(d.x, 0.0f) && Near(d.y, 1.0f) && Near(d.z, 0.0f));
	d = NoClipMoveDirection(0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
	assert(Near(d.x, 1.0f) && Near(d.y, 0.0f));
	d = NoClipMoveDirection(0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f);
	assert(Near(std::sqrt(d.x*d.x + d.y*d.y + d.z*d.z), 1.0f));
	assert(Near(d.x, d.y) && Near(d.y, d.z));
	d = NoClipMoveDirection(0.0f, 0.6f, 0.8f, 1.0f, 0.0f, 0.0f);
	assert(Near(d.y, 0.6f) && Near(d.z, 0.8f));
	d = NoClipMoveDirection(0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f);
	assert(Near(d.x, 1.0f) && Near(d.z, 0.0f));
	d = NoClipMoveDirection(0.0f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f);
	assert(Near(d.z, -1.0f));
	d = NoClipMoveDirection(0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	assert(Near(d.x, 0.0f) && Near(d.y, 0.0f) && Near(d.z, 0.0f));
	assert(Near(NoClipClamp(-4100.0f, -3970.0f, 3970.0f), -3970.0f));
	assert(Near(NoClipClamp(4200.0f, -3970.0f, 3970.0f), 3970.0f));
	assert(Near(NoClipClamp(50.0f, -3970.0f, 3970.0f), 50.0f));
}
