#ifndef __GTA_TASKSIMPLECARSETPEDSLOWDRAGGEDOUT_H__
#define __GTA_TASKSIMPLECARSETPEDSLOWDRAGGEDOUT_H__

#include "TaskSimple.h"

class CVehicle;

// The victim's counterpart to the slow drag-out: taken out of his
// seat, stood up at the door the jacker used, and handed his on-foot
// task.
class CTaskSimpleCarSetPedSlowDraggedOut : public CTaskSimple
{
public:
	CVehicle *m_Vehicle;
	int32 m_Door;

public:
	static const eTaskType Type = TASK_SIMPLE_CAR_SET_PED_SLOW_DRAGGED_OUT;

	CTaskSimpleCarSetPedSlowDraggedOut(CVehicle *target, int32 door);
	CTaskSimpleCarSetPedSlowDraggedOut(const CTaskSimpleCarSetPedSlowDraggedOut &other);
	~CTaskSimpleCarSetPedSlowDraggedOut();

	CTask *Clone() const { return new CTaskSimpleCarSetPedSlowDraggedOut(*this); }
	eTaskType GetTaskType() const { return Type; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil) { return false; }
	bool ProcessPed(CPed *ped);
};

VALIDATE_SIZE(CTaskSimpleCarSetPedSlowDraggedOut, 0x10);

#endif // __GTA_TASKSIMPLECARSETPEDSLOWDRAGGEDOUT_H__
