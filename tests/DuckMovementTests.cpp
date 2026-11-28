#include "../src/peds/DuckMovement.h"

#include <cassert>
#include <cmath>

static bool Near(float a, float b) { return std::fabs(a - b) < 0.0001f; }

int main()
{
	float x = 0.0f, y = 0.0f;
	UpdateDuckMoveCommand(x, y, 0.0f, -1.0f, 1.0f);
	assert(Near(y, -0.07f) && Near(x, 0.0f));
	UpdateDuckMoveCommand(x, y, 0.0f, 1.0f, 1.0f);
	assert(Near(y, 0.0f));
	UpdateDuckMoveCommand(x, y, 0.0f, 0.03f, 1.0f);
	assert(Near(y, 0.03f));
	UpdateDuckMoveCommand(x, y, 0.0f, -1.0f, 0.0f);
	assert(Near(y, 0.03f));
	UpdateDuckMoveCommand(x, y, -0.9f, 0.09f, 1.0f);
	assert(Near(x, -1.0f) && Near(y, 0.0f));
	UpdateDuckMoveCommand(x, y, 1.0f, 1.0f, 100.0f);
	assert(Near(x, -1.0f) && Near(y, 0.0f));
	x = 0.0f;
	UpdateDuckMoveCommand(x, y, 1.0f, 0.1f, 1.0f);
	assert(Near(x, 0.0f) && Near(y, 0.07f));
	UpdateDuckMoveCommand(x, y, 1.0f, 0.0f, 1.0f);
	assert(Near(x, 1.0f) && Near(y, 0.0f));
}
