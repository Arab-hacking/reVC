#ifndef __GTA_TASKSIMPLENONE_H__
#define __GTA_TASKSIMPLENONE_H__

#include "TaskSimple.h"

// SA CTaskSimpleNone (0x8): the empty task - finishes immediately. SA uses
// it wherever a task slot must be filled but do nothing.
class CTaskSimpleNone : public CTaskSimple
{
public:
	static const eTaskType Type = TASK_NONE;

	CTaskSimpleNone() {}
	~CTaskSimpleNone() {}

	eTaskType GetTaskType() const { return Type; }
	CTask *Clone() const { return new CTaskSimpleNone(); }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil) { return true; }
	bool ProcessPed(CPed *ped) { return true; }
};

VALIDATE_SIZE(CTaskSimpleNone, 0x8);

#endif // __GTA_TASKSIMPLENONE_H__
