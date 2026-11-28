// Exercise the SA task manager with synthetic tasks: chain building,
// per-tick processing, finishing, aborting and pool lifetime. No game
// code beyond the task core is linked.
#include "common.h"

#include "MemoryMgr.h"
#include "Task.h"
#include "TaskComplex.h"
#include "TaskManager.h"
#include "TaskPool.h"
#include "TaskSimple.h"

#include <cstdio>
#include <cstdlib>

static void
Check(bool condition, const char *description)
{
	if(!condition){
		fprintf(stderr, "Task manager test failed: %s\n", description);
		abort();
	}
}

void
re3_assert(const char *expr, const char*, unsigned int, const char*)
{
	fprintf(stderr, "Assertion failed: %s\n", expr);
	abort();
}

void*
MemoryMgrMalloc(size_t size)
{
	return malloc(size);
}

void
MemoryMgrFree(void *ptr)
{
	free(ptr);
}

// Simple task that finishes after a fixed number of ticks.
class CTaskSimpleCount : public CTaskSimple
{
public:
	static const eTaskType Type = TASK_SIMPLE_STAND_STILL;

	int32 ticksLeft;
	int32 *processCount;
	bool abortable;

	CTaskSimpleCount(int32 ticks, int32 *counter, bool canAbort = true)
		: ticksLeft(ticks), processCount(counter), abortable(canAbort) {}

	CTask *Clone() const { return new CTaskSimpleCount(ticksLeft, processCount, abortable); }
	eTaskType GetTaskType() const { return Type; }
	bool MakeAbortable(CPed*, eAbortPriority, const CEvent*) { return abortable; }
	bool ProcessPed(CPed*)
	{
		(*processCount)++;
		return --ticksLeft <= 0;
	}
};

// Complex task that hands out numStages one-tick simple tasks in a row.
class CTaskComplexChain : public CTaskComplex
{
public:
	static const eTaskType Type = TASK_COMPLEX_GET_UP_AND_STAND_STILL;

	int32 stage;
	int32 numStages;
	int32 *processCount;

	CTaskComplexChain(int32 stages, int32 *counter)
		: stage(0), numStages(stages), processCount(counter) {}

	CTask *Clone() const { return new CTaskComplexChain(numStages, processCount); }
	eTaskType GetTaskType() const { return Type; }
	CTask *CreateFirstSubTask(CPed*)
	{
		stage = 0;
		return new CTaskSimpleCount(1, processCount);
	}
	CTask *CreateNextSubTask(CPed*)
	{
		if(++stage < numStages)
			return new CTaskSimpleCount(1, processCount);
		return nil;
	}
	CTask *ControlSubTask(CPed*) { return GetSubTask(); }
};

int
main(void)
{
	int32 processed = 0;
	CTaskManager mgr(nil);
	Check(mgr.GetActiveTask() == nil, "no active task initially");

	// Setting a primary task builds the whole chain at once.
	CTaskComplexChain *chain = new CTaskComplexChain(3, &processed);
	Check(CTask::IsTaskPtr(chain), "task comes from the pool");
	mgr.SetTask(chain, TASK_PRIMARY_DEFAULT);
	Check(mgr.GetActiveTask() == chain, "primary task is active");
	Check(mgr.GetSimplestActiveTask() &&
		mgr.GetSimplestActiveTask()->GetTaskType() == TASK_SIMPLE_STAND_STILL,
		"chain ends in the simple task");
	Check(mgr.Find<CTaskSimpleCount>() != nil, "find nested simple task");
	Check(mgr.FindTaskByType(TASK_PRIMARY_DEFAULT, TASK_COMPLEX_GET_UP_AND_STAND_STILL) == chain,
		"find root in its slot");

	// Three one-tick stages: the tree advances one stage per tick and is
	// deleted when the complex task runs out of sub-tasks.
	for(int32 tick = 0; tick < 3; tick++)
		mgr.ManageTasks();
	Check(processed == 3, "every stage processed exactly once");
	Check(mgr.GetActiveTask() == nil, "finished tree removed from its slot");
	Check(!CTask::IsTaskPtr(chain), "finished task returned to the pool");

	// Secondary tasks tick alongside and finish on their own schedule.
	int32 secProcessed = 0;
	mgr.SetTaskSecondary(new CTaskSimpleCount(2, &secProcessed), TASK_SECONDARY_ATTACK);
	mgr.ManageTasks();
	Check(secProcessed == 1 && mgr.GetTaskSecondary(TASK_SECONDARY_ATTACK) != nil,
		"secondary still running after one tick");
	mgr.ManageTasks();
	Check(secProcessed == 2 && mgr.GetTaskSecondary(TASK_SECONDARY_ATTACK) == nil,
		"secondary removed when finished");

	// A task that refuses to abort survives FlushImmediately but not Flush.
	int32 stubbornProcessed = 0;
	CTaskSimpleCount *stubborn = new CTaskSimpleCount(100, &stubbornProcessed, false);
	mgr.SetTask(stubborn, TASK_PRIMARY_PRIMARY);
	mgr.FlushImmediately();
	Check(mgr.GetTaskPrimary(TASK_PRIMARY_PRIMARY) == stubborn,
		"unabortable task survives immediate flush");
	mgr.Flush();
	Check(mgr.GetTaskPrimary(TASK_PRIMARY_PRIMARY) == nil &&
		!CTask::IsTaskPtr(stubborn), "flush deletes anyway");

	puts("SA task manager tests OK");
	return 0;
}
