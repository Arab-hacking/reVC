#pragma once

#include "common.h"
#include "eTaskType.h"

// SA CTask (0x61A340). Every piece of ped behaviour in SA is a task: a
// CTaskSimple does one thing until it reports finished, a CTaskComplex
// owns a sub-task and decides what runs next. Tasks are allocated from
// CTaskPool, never from the heap, so a task pointer stays meaningful for
// as long as the task is alive.
enum eAbortPriority
{
	ABORT_PRIORITY_LEISURE = 0,
	ABORT_PRIORITY_URGENT,
	ABORT_PRIORITY_IMMEDIATE
};

class CEvent;
class CPed;
class CTaskSimple;
class CTaskComplex;

class CTask
{
	friend class CTaskComplex;

public:
	static bool IsGoToTask(CTask *task);
	static bool IsTaskPtr(CTask *task);

	void *operator new(size_t size);
	void operator delete(void *object);

	CTask() : m_Parent(nil) {}
	virtual ~CTask() {}

	//! @return A clone of this task
	virtual CTask *Clone() const = 0;

	//! @return The sub-task (nil for simple tasks)
	virtual CTask *GetSubTask() const = 0;

	//! @brief If this task is of base type CTaskSimple
	virtual bool IsSimple() const = 0;

	//! @brief If this task is of base type CTaskComplex
	bool IsComplex() const { return !IsSimple(); }

	//! @return This task's type
	virtual eTaskType GetTaskType() const = 0;

	//! @brief Stop internal timers due to some event (most tasks ignore this)
	virtual void StopTimer(const CEvent *event);

	/*!
	 * @brief Potentially causes the task to finish earlier than it would
	 *
	 * @param ped The ped this task belongs to
	 * @param priority How urgently the task has to go away
	 * @param event The event the task is aborted for
	 * @return If the task was made abortable
	 */
	virtual bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil) = 0;

	CTaskSimple *AsSimple() { assert(IsSimple()); return (CTaskSimple*)this; }
	CTaskComplex *AsComplex() { assert(IsComplex()); return (CTaskComplex*)this; }

	//! @return The parent of this task (always a CTaskComplex)
	CTaskComplex *GetParent() const { return m_Parent; }

protected:
	CTaskComplex *m_Parent;
};

VALIDATE_SIZE(CTask, 0x8);
