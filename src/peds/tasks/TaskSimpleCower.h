#ifndef __GTA_TASKSIMPLECOWER_H__
#define __GTA_TASKSIMPLECOWER_H__

#include "TaskSimpleRunAnim.h"

// SA CTaskSimpleCower: cower with hands over the head until the animation
// ends. Not interruptable - a cowering ped stays cowering.
class CTaskSimpleCower : public CTaskSimpleRunAnim
{
public:
	static const eTaskType Type = TASK_SIMPLE_COWER;

	CTaskSimpleCower() : CTaskSimpleRunAnim(ANIM_GROUP_DEFAULT, ANIM_ID_HANDSCOWER, 4.0f, Type, "Cower", false) {}
	~CTaskSimpleCower() {}

	CTask *Clone() const { return new CTaskSimpleCower(); }
	virtual bool IsInterruptable(const CPed *ped) { return false; }
};

#endif // __GTA_TASKSIMPLECOWER_H__
