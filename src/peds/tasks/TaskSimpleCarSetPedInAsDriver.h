#ifndef __GTA_TASKSIMPLECARSETPEDINASDRIVER_H__
#define __GTA_TASKSIMPLECARSETPEDINASDRIVER_H__

#include "TaskSimple.h"

class CAnimBlendAssociation;
class CTaskUtilityLineUpPedWithCar;
class CVehicle;

// Put the ped in the driver's seat, here and now, with no animation:
// the get-in (or shuffle) that brought him here has done the visual
// work already.
class CTaskSimpleCarSetPedInAsDriver : public CTaskSimple
{
public:
	bool m_bIsFinished;
	CAnimBlendAssociation *m_pAnim;
	CVehicle *m_pTargetVehicle;
	CTaskUtilityLineUpPedWithCar *m_pUtility;
	bool m_bWarpingInToCar;
	uint8 m_nDoorFlagsToClear;
	uint8 m_nNumGettingInToClear;

public:
	static const eTaskType Type = TASK_SIMPLE_CAR_SET_PED_IN_AS_DRIVER;

	CTaskSimpleCarSetPedInAsDriver(CVehicle *targetVehicle, CTaskUtilityLineUpPedWithCar *utility = nil);
	CTaskSimpleCarSetPedInAsDriver(CVehicle *targetVehicle, bool warpingInToCar, CTaskUtilityLineUpPedWithCar *utility = nil);
	CTaskSimpleCarSetPedInAsDriver(const CTaskSimpleCarSetPedInAsDriver &other);
	~CTaskSimpleCarSetPedInAsDriver();

	CTask *Clone() const { return new CTaskSimpleCarSetPedInAsDriver(*this); }
	eTaskType GetTaskType() const { return Type; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil) { return false; }
	bool ProcessPed(CPed *ped);
};

VALIDATE_SIZE(CTaskSimpleCarSetPedInAsDriver, 0x1C);

#endif // __GTA_TASKSIMPLECARSETPEDINASDRIVER_H__
