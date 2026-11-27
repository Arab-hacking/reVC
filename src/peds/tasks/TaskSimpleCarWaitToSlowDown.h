#ifndef __GTA_TASKSIMPLECARWAITTOSLOWDOWN_H__
#define __GTA_TASKSIMPLECARWAITTOSLOWDOWN_H__

#include "TaskSimple.h"

class CVehicle;

// Wait (while the autopilot brakes under us) until the car is slow
// enough for the ped to step, or jump, out of it.
class CTaskSimpleCarWaitToSlowDown : public CTaskSimple
{
public:
	enum SlowDownType : int32 {
		DONT_WAIT,
		SLOW_ENOUGH_TO_STEP,
		SLOW_ENOUGH_TO_STEP_OR_JUMP
	};

	CVehicle *m_vehicle;
	SlowDownType m_type;

public:
	static const eTaskType Type = TASK_SIMPLE_CAR_WAIT_TO_SLOW_DOWN;

	CTaskSimpleCarWaitToSlowDown(CVehicle *targetVehicle, SlowDownType type);
	CTaskSimpleCarWaitToSlowDown(const CTaskSimpleCarWaitToSlowDown &other);
	~CTaskSimpleCarWaitToSlowDown();

	CTask *Clone() const { return new CTaskSimpleCarWaitToSlowDown(*this); }
	eTaskType GetTaskType() const { return Type; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil);
	bool ProcessPed(CPed *ped);
};

VALIDATE_SIZE(CTaskSimpleCarWaitToSlowDown, 0x10);

#endif // __GTA_TASKSIMPLECARWAITTOSLOWDOWN_H__
