#ifndef __GTA_TASKSIMPLECARSETPEDINASPASSENGER_H__
#define __GTA_TASKSIMPLECARSETPEDINASPASSENGER_H__

#include "TaskSimple.h"

class CAnimBlendAssociation;
class CTaskUtilityLineUpPedWithCar;
class CVehicle;

// Put the ped in the passenger seat belonging to the door he entered
// through, with no animation of its own.
class CTaskSimpleCarSetPedInAsPassenger : public CTaskSimple
{
public:
	bool m_bIsFinished;
	CAnimBlendAssociation *m_pAnim;
	CVehicle *m_pTargetVehicle;
	int32 m_nTargetDoor;
	CTaskUtilityLineUpPedWithCar *m_pUtility;
	bool m_bWarpingInToCar;
	uint8 m_nDoorFlagsToClear;
	uint8 m_nNumGettingInToClear;

public:
	static const eTaskType Type = TASK_SIMPLE_CAR_SET_PED_IN_AS_PASSENGER;

	CTaskSimpleCarSetPedInAsPassenger(CVehicle *targetVehicle, int32 nTargetDoor, CTaskUtilityLineUpPedWithCar *utility = nil);
	CTaskSimpleCarSetPedInAsPassenger(CVehicle *targetVehicle, int32 nTargetDoor, bool warpingInToCar, CTaskUtilityLineUpPedWithCar *utility = nil);
	CTaskSimpleCarSetPedInAsPassenger(const CTaskSimpleCarSetPedInAsPassenger &other);
	~CTaskSimpleCarSetPedInAsPassenger();

	CTask *Clone() const { return new CTaskSimpleCarSetPedInAsPassenger(*this); }
	eTaskType GetTaskType() const { return Type; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil) { return false; }
	bool ProcessPed(CPed *ped);
};

VALIDATE_SIZE(CTaskSimpleCarSetPedInAsPassenger, 0x20);

#endif // __GTA_TASKSIMPLECARSETPEDINASPASSENGER_H__
