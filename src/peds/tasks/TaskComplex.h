#pragma once

#include "Task.h"

class CPed;

// SA CTaskComplex: a task that owns one sub-task at a time. The manager
// creates the first sub-task on assignment, calls ControlSubTask every
// tick to let the task react, and asks for the next sub-task whenever the
// current one finishes.
class CTaskComplex : public CTask
{
public:
	CTaskComplex() : m_pSubTask(nil) {}
	~CTaskComplex()
	{
		delete m_pSubTask;
		m_pSubTask = nil;
	}

	CTask *GetSubTask() const { return m_pSubTask; }
	bool IsSimple() const { return false; }

	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil)
	{
		return m_pSubTask->MakeAbortable(ped, priority, event);
	}

	void SetSubTask(CTask *subTask)
	{
		if(m_pSubTask == subTask)
			return;
		delete m_pSubTask;
		m_pSubTask = subTask;
		if(subTask)
			subTask->m_Parent = this;
	}

	/*!
	 * @brief Create the next sub-task after the previous has finished
	 * @return The new sub-task, or nil if this task has finished
	 */
	virtual CTask *CreateNextSubTask(CPed *ped) = 0;

	/*!
	 * @brief Create the first sub-task; called once after task creation
	 * @return The sub-task, or nil to indicate the task has finished
	 */
	virtual CTask *CreateFirstSubTask(CPed *ped) = 0;

	/*!
	 * @brief Control this task's sub-task; called each update tick
	 * @return The old sub-task if it stays, a new one to replace it
	 *         (the old is aborted first), or nil if this task finished
	 */
	virtual CTask *ControlSubTask(CPed *ped) = 0;

protected:
	CTask *m_pSubTask;
};

VALIDATE_SIZE(CTaskComplex, 0xC);
