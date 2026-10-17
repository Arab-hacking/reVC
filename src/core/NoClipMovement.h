#ifndef __GTA_NOCLIPMOVEMENT_H__
#define __GTA_NOCLIPMOVEMENT_H__

#include <cmath>

struct NoClipVector {
	float x, y, z;
};

// W/S follow the view (including its pitch), A/D strafe horizontally, and
// Space/Ctrl provide vertical motion. Normalize diagonals to a fixed speed.
inline NoClipVector
NoClipMoveDirection(float viewX, float viewY, float viewZ,
                    float forward, float strafe, float vertical)
{
	float viewLength = std::sqrt(viewX*viewX + viewY*viewY + viewZ*viewZ);
	if(viewLength < 0.0001f){
		viewX = 0.0f; viewY = 1.0f; viewZ = 0.0f;
	}else{
		viewX /= viewLength; viewY /= viewLength; viewZ /= viewLength;
	}
	float horizontal = std::sqrt(viewX*viewX + viewY*viewY);
	float rightX = horizontal < 0.0001f ? 1.0f : viewY / horizontal;
	float rightY = horizontal < 0.0001f ? 0.0f : -viewX / horizontal;
	NoClipVector direction = {
		viewX*forward + rightX*strafe,
		viewY*forward + rightY*strafe,
		viewZ*forward + vertical
	};
	float length = std::sqrt(direction.x*direction.x + direction.y*direction.y + direction.z*direction.z);
	if(length > 0.0001f){
		direction.x /= length;
		direction.y /= length;
		direction.z /= length;
	}
	return direction;
}

inline float
NoClipClamp(float value, float minValue, float maxValue)
{
	return value < minValue ? minValue : value > maxValue ? maxValue : value;
}

#endif
