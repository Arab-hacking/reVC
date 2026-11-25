#ifndef __GTA_TASKSIMPLEPAUSE_H__
#define __GTA_TASKSIMPLEPAUSE_H__

#include "TaskSimple.h"
#include "TaskTimer.h"

// SA CTaskSimplePause (0x18): do nothing for a while. Complex tasks use
// it as a beat between sub-tasks.
class CTaskSimplePause : public CTaskSimple
{
public:
	static const eTaskType Type = TASK_SIMPLE_PAUSE;

	explicit CTaskSimplePause(int32 time = 0);
	CTaskSimplePause(const CTaskSimplePause &other);
	~CTaskSimplePause() {}

	CTask *Clone() const { return new CTaskSimplePause(*this); }
	eTaskType GetTaskType() const { return Type; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil);
	bool ProcessPed(CPed *ped);

private:
	CTaskTimer m_Timer;
	int32 m_PauseIntervalMs;
};

VALIDATE_SIZE(CTaskSimplePause, 0x18);

#endif // __GTA_TASKSIMPLEPAUSE_H__
