#pragma once

#include "common.h"
#include "Task.h"

#include <initializer_list>

// SA CTaskManager (0x6816A0). Owns a ped's task trees: five primary slots
// in priority order (physical response first, default last) and six
// secondary slots for things that run alongside (attack, duck, speech,
// facial, partial anims, IK). ManageTasks ticks the active primary tree
// and every secondary tree once per ped update.

enum ePrimaryTasks // array indices
{
	TASK_PRIMARY_INVALID = -1,

	TASK_PRIMARY_PHYSICAL_RESPONSE = 0,
	TASK_PRIMARY_EVENT_RESPONSE_TEMP,
	TASK_PRIMARY_EVENT_RESPONSE_NONTEMP,
	TASK_PRIMARY_PRIMARY,
	TASK_PRIMARY_DEFAULT,
	TASK_PRIMARY_MAX
};

enum eSecondaryTask // array indices
{
	TASK_SECONDARY_INVALID = (int32)-1,

	TASK_SECONDARY_ATTACK = 0, // want duck to be after attack
	TASK_SECONDARY_DUCK,       // because attack controls ducking movement
	TASK_SECONDARY_SAY,
	TASK_SECONDARY_FACIAL_COMPLEX,
	TASK_SECONDARY_PARTIAL_ANIM,
	TASK_SECONDARY_IK,
	TASK_SECONDARY_MAX
};

class CTaskManager
{
public:
	CTask *m_aPrimaryTasks[TASK_PRIMARY_MAX];
	CTask *m_aSecondaryTasks[TASK_SECONDARY_MAX];
	CPed *m_pPed;

public:
	explicit CTaskManager(CPed *ped);
	~CTaskManager();

	// 0x681720: first present primary task, in priority order
	CTask *GetActiveTask();

	// Index of the active primary task, or -1
	int32 GetActiveTaskIndex() const;

	// 0x681740: find the first task (or sub-task) of the given type,
	// searching the active primary tree first, then the secondaries
	CTask *FindActiveTaskByType(eTaskType taskType);

	// 0x6817D0: like FindActiveTaskByType, but only within one primary slot
	CTask *FindTaskByType(ePrimaryTasks taskIndex, eTaskType taskType) const;

	// 0x681810
	CTask *GetTaskSecondary(eSecondaryTask taskIndex) { return m_aSecondaryTasks[taskIndex]; }

	bool HasTaskPrimary(const CTask *task);
	bool HasTaskSecondary(const CTask *task);

	// 0x681850: delete all tasks without asking them first
	void Flush();

	// 0x6818A0: try aborting all tasks, delete the ones that give in
	void FlushImmediately();

	// 0x681920
	void SetNextSubTask(CTaskComplex *task);

	// 0x6819A0
	void StopTimers(const CEvent *event);

	// 0x6819D0: the simple task at the end of the active primary chain
	CTask *GetSimplestActiveTask() { return GetLastTaskOf(GetActiveTask()); }

	// 0x681A00
	CTask *GetLastTaskOf(ePrimaryTasks taskIndex) const { return GetLastTaskOf(GetTaskPrimary(taskIndex)); }

	// 0x681970
	static CTask *GetLastTaskOf(CTask *task);

	// 0x681A30
	void AddSubTasks(CTask *task);

	// 0x681A80
	void ParentsControlChildren(CTask *task);

	// 0x681AF0
	void SetTask(CTask *task, ePrimaryTasks taskIndex) { ChangeTaskInSlot(m_aPrimaryTasks[taskIndex], task); }

	// 0x681B60
	void SetTaskSecondary(CTask *task, eSecondaryTask taskIndex) { ChangeTaskInSlot(m_aSecondaryTasks[taskIndex], task); }

	// Abort the first task found in any of the given primary slots
	void AbortFirstPrimaryTaskIn(std::initializer_list<ePrimaryTasks> idxs, CPed *ped, eAbortPriority priority = ABORT_PRIORITY_LEISURE, const CEvent *event = nil);

	// 0x681BD0: clear the event-response primary tasks
	void ClearTaskEventResponse();

	// 0x681C10
	void ManageTasks();

	CTask *GetTaskPrimary(int32 taskIndex) const { return m_aPrimaryTasks[taskIndex]; }

	CTask *GetPresistentEventResponseTask() const;
	CTask *GetTemporaryEventResponseTask() const;

	// Find by SA type id; activeOnly searches the active tree and the
	// secondaries, otherwise the primary slots in SA's response order.
	CTask *FindTaskOfType(eTaskType type, bool activeOnly);

	template<typename T>
	T *Find(bool activeOnly = true) { return (T*)FindTaskOfType(T::Type, activeOnly); }

	template<typename T>
	bool Has(bool activeOnly = true) { return FindTaskOfType(T::Type, activeOnly) != nil; }

	template<typename T>
	T *GetActiveTaskAs()
	{
		CTask *task = GetActiveTask();
		return task && task->GetTaskType() == T::Type ? (T*)task : nil;
	}

	template<typename T>
	T *GetSimplestActiveTaskAs()
	{
		CTask *task = GetSimplestActiveTask();
		return task && task->GetTaskType() == T::Type ? (T*)task : nil;
	}

	// Apply a function to every root task (primary and secondary)
	template<typename F>
	void ApplyToRootTasks(F fn)
	{
		for(int32 i = 0; i < TASK_PRIMARY_MAX; i++)
			fn(m_aPrimaryTasks[i]);
		for(int32 i = 0; i < TASK_SECONDARY_MAX; i++)
			fn(m_aSecondaryTasks[i]);
	}

protected:
	// Helper of SetTask/SetTaskSecondary; taskInSlot may be nulled out
	void ChangeTaskInSlot(CTask *&taskInSlot, CTask *changeTo);

private:
	enum ProcessResult
	{
		TT_FINISHED, // the task tree has finished
		TT_SUCCESS,  // there is currently an active task
		TT_ADVANCED  // new sub-tasks were created, task still active
	};
	ProcessResult ProcessTaskTree(CTask *root, bool bIsPrimary);
};

VALIDATE_SIZE(CTaskManager, 0x30);
