#include "common.h"

#include "PedTaskPair.h"

#include "Task.h"

// SA CPedTaskPair::Flush (0x5E95B0): drop the ped and delete the task.
void
CPedTaskPair::Flush()
{
	Ped = nil;
	delete Task;
	Task = nil;
	Slot = TASK_SECONDARY_INVALID;
}
