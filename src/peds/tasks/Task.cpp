#include "common.h"

#include "Task.h"
#include "TaskPool.h"

// 0x61A5A0
void*
CTask::operator new(size_t size)
{
	assert(size <= TASK_POOL_SLOT_SIZE);
	return GetTaskPool()->New();
}

// 0x61A5B0
void
CTask::operator delete(void *object)
{
	GetTaskPool()->Delete((CTask*)object);
}

// 0x421180
void
CTask::StopTimer(const CEvent *event)
{
	// NOP in SA: only a handful of timed tasks override this.
}

// 0x61A360
bool
CTask::IsGoToTask(CTask *task)
{
	switch(task->GetTaskType()){
	case TASK_SIMPLE_GO_TO_POINT:
	case TASK_SIMPLE_GO_TO_POINT_FINE:
		return true;
	default:
		return false;
	}
}

// 0x61A4B0
bool
CTask::IsTaskPtr(CTask *task)
{
	return task && GetTaskPool()->IsObjectValid(task);
}
