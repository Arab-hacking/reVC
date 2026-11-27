#ifndef __GTA_TASKUTILITYLINEUPPEDWITHCAR_H__
#define __GTA_TASKUTILITYLINEUPPEDWITHCAR_H__

#include "Vector.h"

class CAnimBlendAssociation;
class CPed;
class CVehicle;

// Helper shared by the vehicle entry/exit tasks: while an entry or exit
// animation plays, it keeps the ped glued to the (possibly moving) door
// he is using - sliding him from the door-open position towards the seat
// as the animation progresses and turning him to face the car's way.
// Created with the offset from the ped to the door-open position, a time
// budget for the line-up (600 ms) and the door index; ProcessPed is then
// called once per frame from the owning task's SetPedPosition.
class CTaskUtilityLineUpPedWithCar
{
public:
	CVector m_Offset;
	float m_fDoorOpenPosZ;
	int32 m_fTime;
	int32 m_nDoorOpenPosType;
	int32 m_nDoorIdx;

public:
	CTaskUtilityLineUpPedWithCar(const CVector &offset, int32 time, int32 doorOpenPosType, int32 doorIdx);
	~CTaskUtilityLineUpPedWithCar() {}

	CVector GetLocalPositionToOpenCarDoor(CVehicle *vehicle, float animProgress, CAnimBlendAssociation *assoc);
	CVector GetPositionToOpenCarDoor(CVehicle *vehicle, float animProgress, CAnimBlendAssociation *assoc);
	bool ProcessPed(CPed *ped, CVehicle *vehicle, CAnimBlendAssociation *assoc);
};

VALIDATE_SIZE(CTaskUtilityLineUpPedWithCar, 0x1C);

#endif // __GTA_TASKUTILITYLINEUPPEDWITHCAR_H__
