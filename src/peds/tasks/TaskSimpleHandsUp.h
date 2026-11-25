#ifndef __GTA_TASKSIMPLEHANDSUP_H__
#define __GTA_TASKSIMPLEHANDSUP_H__

#include "TaskSimpleRunTimedAnim.h"

// SA CTaskSimpleHandsUp: hands up for a given time - surrender, robbery
// victims, arrested peds.
class CTaskSimpleHandsUp : public CTaskSimpleRunTimedAnim
{
public:
	static const eTaskType Type = TASK_SIMPLE_HANDS_UP;

	CTaskSimpleHandsUp(uint32 duration) : CTaskSimpleRunTimedAnim(ANIM_GROUP_DEFAULT, ANIM_ID_HANDSUP, 4.0f, -4.0f, duration, Type, "HandsUp", false) {}
	CTaskSimpleHandsUp(const CTaskSimpleHandsUp &other) : CTaskSimpleRunTimedAnim(other) {}
	~CTaskSimpleHandsUp() {}

	CTask *Clone() const { return new CTaskSimpleHandsUp(*this); }
	virtual bool IsInterruptable(const CPed *ped) { return false; }
};

#endif // __GTA_TASKSIMPLEHANDSUP_H__
