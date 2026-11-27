#ifndef __GTA_TASKCOMPLEXCARSLOWBEDRAGGEDOUTANDSTANDUP_H__
#define __GTA_TASKCOMPLEXCARSLOWBEDRAGGEDOUTANDSTANDUP_H__

#include "TaskComplex.h"
#include "AnimationEnums.h"

class CVehicle;

// Dragged out of the car and left standing afterwards - the
// victim-side answer to a slow drag-out.
class CTaskComplexCarSlowBeDraggedOutAndStandUp : public CTaskComplex
{
public:
	CVehicle *m_Vehicle;
	int32 m_Door;

public:
	static const eTaskType Type = TASK_COMPLEX_CAR_SLOW_BE_DRAGGED_OUT_AND_STAND_UP;

	CTaskComplexCarSlowBeDraggedOutAndStandUp(CVehicle *vehicle, int32 door);
	CTaskComplexCarSlowBeDraggedOutAndStandUp(const CTaskComplexCarSlowBeDraggedOutAndStandUp &other);
	~CTaskComplexCarSlowBeDraggedOutAndStandUp();

	CTask *CreateSubTask(eTaskType taskType, CPed *ped);
	CTask *CreateLeaveCarSubTask(CPed *ped) const;

	CTask *Clone() const { return new CTaskComplexCarSlowBeDraggedOutAndStandUp(*this); }
	eTaskType GetTaskType() const { return Type; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil);
	CTask *CreateNextSubTask(CPed *ped);
	CTask *CreateFirstSubTask(CPed *ped);
	CTask *ControlSubTask(CPed *ped) { return m_pSubTask; }
};

VALIDATE_SIZE(CTaskComplexCarSlowBeDraggedOutAndStandUp, 0x14);

#endif // __GTA_TASKCOMPLEXCARSLOWBEDRAGGEDOUTANDSTANDUP_H__
