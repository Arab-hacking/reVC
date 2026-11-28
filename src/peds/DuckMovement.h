#ifndef REVC_DUCK_MOVEMENT_H
#define REVC_DUCK_MOVEMENT_H

#include <cmath>

// The lateral command is latched until the roll animation finishes. The
// longitudinal command is rate limited, including changes of direction.
inline void
UpdateDuckMoveCommand(float &x, float &y, float desiredX, float desiredY, float timeStep)
{
	if (x != 0.0f)
		return;

	const float maxChange = timeStep > 0.0f ? timeStep * 0.07f : 0.0f;
	const float delta = desiredY - y;
	if (delta > maxChange)
		y += maxChange;
	else if (delta < -maxChange)
		y -= maxChange;
	else
		y = desiredY;

	if (std::fabs(desiredY) < 0.1f && std::fabs(desiredX) >= 0.9f) {
		y = 0.0f;
		x = desiredX < 0.0f ? -1.0f : 1.0f;
	}
}

#endif
