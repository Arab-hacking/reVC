#ifndef __GTA_TASKSIMPLECARSETPEDOUT_H__
#define __GTA_TASKSIMPLECARSETPEDOUT_H__

#include "TaskSimple.h"

class CVehicle;
class CPed;

// Take the ped out of the vehicle for good: clear him from his seat,
// give him his collision and his weapon back, put him on the ground
// clear of the car's collision, and hand him his on-foot task.
class CTaskSimpleCarSetPedOut : public CTaskSimple
{
public:
	CVehicle *m_pTargetVehicle;
	int32 m_nTargetDoor;
	bool m_bSwitchOffEngine;
	bool m_bWarpingOutOfCar;
	bool m_bFallingOutOfCar; /// Jumping or falling off car or bike
	bool m_bKnockedOffBike;
	uint8 m_nDoorFlagsToClear;
	uint8 m_nNumGettingInToClear;

public:
	static const eTaskType Type = TASK_SIMPLE_CAR_SET_PED_OUT;

	CTaskSimpleCarSetPedOut(CVehicle *targetVehicle, int32 nTargetDoor, bool bSwitchOffEngine);
	CTaskSimpleCarSetPedOut(CVehicle *targetVehicle, int32 nTargetDoor, bool bSwitchOffEngine, bool warpingOutOfCar);
	CTaskSimpleCarSetPedOut(const CTaskSimpleCarSetPedOut &other);
	~CTaskSimpleCarSetPedOut();

	static void PositionPedOutOfCollision(CPed *ped, CVehicle *veh, int32 door);

	CTask *Clone() const { return new CTaskSimpleCarSetPedOut(*this); }
	eTaskType GetTaskType() const { return Type; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil) { return false; }
	bool ProcessPed(CPed *ped);
};

VALIDATE_SIZE(CTaskSimpleCarSetPedOut, 0x18);

#endif // __GTA_TASKSIMPLECARSETPEDOUT_H__
