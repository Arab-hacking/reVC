#ifndef __GTA_TASKCOMPLEXFALLANDGETUP_H__
#define __GTA_TASKCOMPLEXFALLANDGETUP_H__

#include "TaskComplex.h"
#include "AnimationEnums.h"

// Go down, stay down a moment, get back up: the polite way to be
// knocked off a bike or out of a moving car you failed to enter.
class CTaskComplexFallAndGetUp : public CTaskComplex
{
public:
	AnimationId m_nFallAnimId;
	AssocGroupId m_nFallAnimGroup;
	int32 m_nFallDownTime;

public:
	static const eTaskType Type = TASK_COMPLEX_FALL_AND_GET_UP;

	CTaskComplexFallAndGetUp(AnimationId nFallAnimId, AssocGroupId nFallAnimGroup, int32 nFallDownTime);
	CTaskComplexFallAndGetUp(int32 nDir, int32 nFallDownTime); // 0 front, 1 right, 2 back, 3 left
	CTaskComplexFallAndGetUp(const CTaskComplexFallAndGetUp &other);
	~CTaskComplexFallAndGetUp() {}

	CTask *CreateSubTask(eTaskType taskType);
	bool IsFalling();
	void SetDownTime(int32 nTime);

	CTask *Clone() const { return new CTaskComplexFallAndGetUp(*this); }
	eTaskType GetTaskType() const { return Type; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil);
	CTask *CreateFirstSubTask(CPed *ped);
	CTask *CreateNextSubTask(CPed *ped);
	CTask *ControlSubTask(CPed *ped) { return m_pSubTask; }
};

VALIDATE_SIZE(CTaskComplexFallAndGetUp, 0x18);

#endif // __GTA_TASKCOMPLEXFALLANDGETUP_H__
