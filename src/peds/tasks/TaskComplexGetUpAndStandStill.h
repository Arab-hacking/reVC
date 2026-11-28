#ifndef __GTA_TASKCOMPLEXGETUPANDSTANDSTILL_H__
#define __GTA_TASKCOMPLEXGETUPANDSTANDSTILL_H__

#include "TaskComplex.h"

// Get up off the ground and stand still, like a man.
class CTaskComplexGetUpAndStandStill : public CTaskComplex
{
public:
	static const eTaskType Type = TASK_COMPLEX_GET_UP_AND_STAND_STILL;

	CTaskComplexGetUpAndStandStill() {}
	CTaskComplexGetUpAndStandStill(const CTaskComplexGetUpAndStandStill &other) {}
	~CTaskComplexGetUpAndStandStill() {}

	CTask *CreateSubTask(eTaskType taskType);

	CTask *Clone() const { return new CTaskComplexGetUpAndStandStill(); }
	eTaskType GetTaskType() const { return Type; }
	CTask *CreateFirstSubTask(CPed *ped);
	CTask *CreateNextSubTask(CPed *ped);
	CTask *ControlSubTask(CPed *ped) { return m_pSubTask; }
};

VALIDATE_SIZE(CTaskComplexGetUpAndStandStill, 0xC);

#endif // __GTA_TASKCOMPLEXGETUPANDSTANDSTILL_H__
