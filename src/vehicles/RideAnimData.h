#ifndef __GTA_RIDEANIMDATA_H__
#define __GTA_RIDEANIMDATA_H__

#include "AnimManager.h"

// SA CRideAnimData (0x1C bytes). What the rider's body is doing relative to the
// machine: the animation group in use, how far the bars are turned, how far the
// torso leans side to side and front to back. CBike keeps one of these; so does
// CQuadBike, because a quad is steered by a rider who leans just like a biker
// even though the chassis is a car.
class CRideAnimData
{
public:
	AssocGroupId AnimGroup;
	float BarSteerAngle;
	float LeanAngle;
	float DesiredLeanAngle;
	float LeanFwd;
	float AnimLeanLeft;
	float AnimLeanFwd;

	CRideAnimData(void)
	 : AnimGroup(ANIM_GROUP_DEFAULT), BarSteerAngle(0.0f), LeanAngle(0.0f),
	   DesiredLeanAngle(0.0f), LeanFwd(0.0f), AnimLeanLeft(0.0f), AnimLeanFwd(0.0f)
	{
	}

	CRideAnimData(AssocGroupId animGroup)
	 : AnimGroup(animGroup), BarSteerAngle(0.0f), LeanAngle(0.0f),
	   DesiredLeanAngle(0.0f), LeanFwd(0.0f), AnimLeanLeft(0.0f), AnimLeanFwd(0.0f)
	{
	}
};

#endif // __GTA_RIDEANIMDATA_H__
