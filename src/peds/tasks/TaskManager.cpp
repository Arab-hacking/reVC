#include "common.h"

#include "TaskManager.h"
#include "TaskComplex.h"
#include "TaskSimple.h"

static void
DeleteTaskAndNull(CTask *&task)
{
	delete task;
	task = nil;
}

// Find the first task (or sub-task) that is of the required type
static CTask*
FindFirstTaskOfType(CTask *task, eTaskType type)
{
	for(; task; task = task->GetSubTask())
		if(task->GetTaskType() == type)
			return task;
	return nil;
}

// 0x6816A0
CTaskManager::CTaskManager(CPed *ped)
: m_pPed(ped)
{
	for(int32 i = 0; i < TASK_PRIMARY_MAX; i++)
		m_aPrimaryTasks[i] = nil;
	for(int32 i = 0; i < TASK_SECONDARY_MAX; i++)
		m_aSecondaryTasks[i] = nil;
}

// 0x6816D0
CTaskManager::~CTaskManager()
{
	Flush();
}

// 0x681720
CTask*
CTaskManager::GetActiveTask()
{
	for(int32 i = 0; i < TASK_PRIMARY_MAX; i++)
		if(m_aPrimaryTasks[i])
			return m_aPrimaryTasks[i];
	return nil;
}

int32
CTaskManager::GetActiveTaskIndex() const
{
	for(int32 i = 0; i < TASK_PRIMARY_MAX; i++)
		if(m_aPrimaryTasks[i])
			return i;
	return -1;
}

// 0x681740
CTask*
CTaskManager::FindActiveTaskByType(eTaskType taskType)
{
	// First try the current active task and its sub-tasks
	if(CTask *task = FindFirstTaskOfType(GetActiveTask(), taskType))
		return task;

	// Now try secondaries and their sub-tasks
	for(int32 i = 0; i < TASK_SECONDARY_MAX; i++)
		if(CTask *task = FindFirstTaskOfType(m_aSecondaryTasks[i], taskType))
			return task;

	return nil;
}

// 0x6817D0
CTask*
CTaskManager::FindTaskByType(ePrimaryTasks taskIndex, eTaskType taskId) const
{
	return FindFirstTaskOfType(GetTaskPrimary(taskIndex), taskId);
}

bool
CTaskManager::HasTaskPrimary(const CTask *task)
{
	for(int32 i = 0; i < TASK_PRIMARY_MAX; i++)
		if(m_aPrimaryTasks[i] && m_aPrimaryTasks[i] == task)
			return true;
	return false;
}

// 0x681820
bool
CTaskManager::HasTaskSecondary(const CTask *task)
{
	for(int32 i = 0; i < TASK_SECONDARY_MAX; i++)
		if(m_aSecondaryTasks[i] && m_aSecondaryTasks[i] == task)
			return true;
	return false;
}

// 0x681850
void
CTaskManager::Flush()
{
	for(int32 i = 0; i < TASK_PRIMARY_MAX; i++)
		DeleteTaskAndNull(m_aPrimaryTasks[i]);
	for(int32 i = 0; i < TASK_SECONDARY_MAX; i++)
		DeleteTaskAndNull(m_aSecondaryTasks[i]);
}

// 0x6818A0
void
CTaskManager::FlushImmediately()
{
	for(int32 i = 0; i < TASK_PRIMARY_MAX; i++)
		if(m_aPrimaryTasks[i] && m_aPrimaryTasks[i]->MakeAbortable(m_pPed, ABORT_PRIORITY_IMMEDIATE, nil))
			DeleteTaskAndNull(m_aPrimaryTasks[i]);
	for(int32 i = 0; i < TASK_SECONDARY_MAX; i++)
		if(m_aSecondaryTasks[i] && m_aSecondaryTasks[i]->MakeAbortable(m_pPed, ABORT_PRIORITY_IMMEDIATE, nil))
			DeleteTaskAndNull(m_aSecondaryTasks[i]);
}

// 0x681920
void
CTaskManager::SetNextSubTask(CTaskComplex *ofTask)
{
	if(!ofTask)
		return;

	assert(ofTask->IsComplex());

	for(CTask *task = ofTask; task; task = task->GetParent()){
		if(CTask *next = task->AsComplex()->CreateNextSubTask(m_pPed)){
			task->AsComplex()->SetSubTask(next);
			AddSubTasks(next);
			return;
		}

		// If no subtask is created, the task is considered done.
		// So we go to its parent and create its next subtask.
		task->AsComplex()->SetSubTask(nil); // This also deletes the previous sub-task
	}
}

// 0x681970
CTask*
CTaskManager::GetLastTaskOf(CTask *task)
{
	CTask *last = task;
	for(; task; task = task->GetSubTask())
		last = task;
	return last;
}

// 0x6819A0
void
CTaskManager::StopTimers(const CEvent *event)
{
	for(int32 i = 0; i < TASK_PRIMARY_MAX; i++)
		if(m_aPrimaryTasks[i])
			m_aPrimaryTasks[i]->StopTimer(event);
}

// 0x681A30
void
CTaskManager::AddSubTasks(CTask *toTask)
{
	if(!toTask)
		return;

	for(CTask *task = toTask; !task->IsSimple();){
		CTaskComplex *ctask = task->AsComplex();
		assert(CTask::IsTaskPtr(task)); // Tasks sometimes get deleted, which causes pure virtual calls otherwise
		if(CTask *sub = ctask->CreateFirstSubTask(m_pPed)){
			ctask->SetSubTask(sub);
			task = sub; // Go on creating the subtask of the created task
		}else{ // No subtask created, so we can stop here
			if(CTaskComplex *parent = ctask->GetParent())
				SetNextSubTask(parent);
			break;
		}
	}
}

// 0x681A80
void
CTaskManager::ParentsControlChildren(CTask *parent)
{
	for(CTask *task = parent; task && !task->IsSimple();){
		CTaskComplex *ctask = task->AsComplex();
		CTask *oldSubTask = ctask->GetSubTask();

		CTask *newSubTask = ctask->ControlSubTask(m_pPed);

		if(newSubTask != oldSubTask){
			oldSubTask->MakeAbortable(m_pPed);
			ctask->SetSubTask(newSubTask);
			AddSubTasks(newSubTask);
			return;
		}

		task = task->GetSubTask();
	}
}

void
CTaskManager::AbortFirstPrimaryTaskIn(std::initializer_list<ePrimaryTasks> slots, CPed *ped, eAbortPriority priority, const CEvent *event)
{
	for(ePrimaryTasks slot : slots){
		if(CTask *task = GetTaskPrimary(slot)){
			task->MakeAbortable(ped, priority, event);
			break;
		}
	}
}

// 0x681BD0
void
CTaskManager::ClearTaskEventResponse()
{
	// The nontemp response is only present if the temp one is, as it is
	// not deleted otherwise.
	const ePrimaryTasks idxs[2] = { TASK_PRIMARY_EVENT_RESPONSE_TEMP, TASK_PRIMARY_EVENT_RESPONSE_NONTEMP };
	for(int32 i = 0; i < 2; i++){
		if(m_aPrimaryTasks[idxs[i]]){
			DeleteTaskAndNull(m_aPrimaryTasks[idxs[i]]);
			break;
		}
	}
}

CTask*
CTaskManager::GetPresistentEventResponseTask() const
{
	const ePrimaryTasks idxs[2] = { TASK_PRIMARY_PHYSICAL_RESPONSE, TASK_PRIMARY_EVENT_RESPONSE_NONTEMP };
	for(int32 i = 0; i < 2; i++)
		if(CTask *t = GetTaskPrimary(idxs[i]))
			return t;
	return nil;
}

CTask*
CTaskManager::GetTemporaryEventResponseTask() const
{
	return GetTaskPrimary(TASK_PRIMARY_EVENT_RESPONSE_TEMP);
}

CTask*
CTaskManager::FindTaskOfType(eTaskType type, bool activeOnly)
{
	if(activeOnly)
		return FindActiveTaskByType(type);

	// Based on CPedIntelligence::FindTaskByType
	const ePrimaryTasks idxs[4] = { TASK_PRIMARY_DEFAULT, TASK_PRIMARY_PRIMARY, TASK_PRIMARY_EVENT_RESPONSE_TEMP, TASK_PRIMARY_EVENT_RESPONSE_NONTEMP };
	for(int32 i = 0; i < 4; i++)
		if(CTask *task = FindTaskByType(idxs[i], type))
			return task;
	return nil;
}

void
CTaskManager::ChangeTaskInSlot(CTask *&taskInSlot, CTask *changeTo)
{
	// Nothing to do in case they're the same
	if(taskInSlot == changeTo)
		return;

	// Delete current
	delete taskInSlot;

	// Set the task in the slot, and add its subtasks
	taskInSlot = changeTo;
	AddSubTasks(taskInSlot);

	// Task trees must always end with a simple task, so if that's not
	// the case delete it immediately.
	if(taskInSlot && !GetLastTaskOf(taskInSlot)->IsSimple()){
		delete taskInSlot;
		taskInSlot = nil;
	}
}

CTaskManager::ProcessResult
CTaskManager::ProcessTaskTree(CTask *root, bool bIsPrimary)
{
	ParentsControlChildren(root);

	CTask *lastTask = GetLastTaskOf(root);

	// The task tree can only be "terminated" with a simple task (a complex
	// task must always have a sub-task). If the last task is complex, that
	// means creating all sub-tasks (see AddSubTasks) failed, i.e. the task
	// tree has finished and the root can be deleted.
	if(!lastTask->IsSimple()){
		if(!bIsPrimary)
			return TT_FINISHED;

		// Try setting the next sub-task
		SetNextSubTask(lastTask->GetParent());

		// If that succeeded, the last task has changed
		lastTask = GetLastTaskOf(root);

		// A complex task in this case means the primary task has finished
		if(lastTask->IsComplex())
			return TT_FINISHED;
	}

	// Check if the simple task has finished
	if(!lastTask->AsSimple()->ProcessPed(m_pPed))
		return TT_SUCCESS;

	// It has, try creating the next sub-task
	SetNextSubTask(lastTask->GetParent());

	// Check if the task tree has finished (see SetNextSubTask)
	if(!root->GetSubTask())
		return TT_FINISHED;

	// New sub-tasks were successfully created
	return TT_ADVANCED;
}

// 0x681C10
void
CTaskManager::ManageTasks()
{
	// Process the active primary task's tree
	int32 activeTaskIdx = GetActiveTaskIndex();
	if(activeTaskIdx != -1){
		CTask *&activePrimaryTask = m_aPrimaryTasks[activeTaskIdx];

		if(!GetLastTaskOf(activePrimaryTask)->IsSimple()){
			delete activePrimaryTask;
			activePrimaryTask = nil;
			return;
		}

		// The 10 here is just an arbitrary iteration limit
		for(int32 i = 0; i < 10; i++){
			ProcessResult res = ProcessTaskTree(activePrimaryTask, true);

			if(res == TT_FINISHED){
				delete activePrimaryTask;
				activePrimaryTask = nil;
			}

			if(res != TT_ADVANCED)
				break;
		}
	}

	// Process secondary task trees
	for(int32 i = 0; i < TASK_SECONDARY_MAX; i++){
		CTask *&secondaryTask = m_aSecondaryTasks[i];
		if(!secondaryTask)
			continue;
		ProcessResult res;
		while((res = ProcessTaskTree(secondaryTask, false)) == TT_ADVANCED)
			;
		if(res == TT_FINISHED){
			delete secondaryTask;
			secondaryTask = nil;
		}
	}
}
