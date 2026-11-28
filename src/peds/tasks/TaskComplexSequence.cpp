#include "common.h"

#include "TaskComplexSequence.h"

// SA CTaskComplexSequence::CTaskComplexSequence.
CTaskComplexSequence::CTaskComplexSequence()
{
	m_CurrTaskIdx = 0;
	for(int i = 0; i < 8; i++)
		m_Tasks[i] = nil;
	m_RepeatMode = DONT_REPEAT;
	m_RepeatedCnt = 0;
	m_bFlushTasks = false;
	m_RefCnt = 0;
}

// SA CTaskComplexSequence::CTaskComplexSequence (0x5F6710): a copy holds
// clones of the prototypes, never the originals.
CTaskComplexSequence::CTaskComplexSequence(const CTaskComplexSequence &other)
{
	m_CurrTaskIdx = other.m_CurrTaskIdx;
	m_RepeatMode = other.m_RepeatMode;
	m_RepeatedCnt = other.m_RepeatedCnt;
	m_bFlushTasks = other.m_bFlushTasks;
	m_RefCnt = 0;
	for(int i = 0; i < 8; i++)
		m_Tasks[i] = other.m_Tasks[i] ? other.m_Tasks[i]->Clone() : nil;
}

// SA CTaskComplexSequence::~CTaskComplexSequence.
CTaskComplexSequence::~CTaskComplexSequence()
{
	Flush();
}

// SA CTaskComplexSequence::Flush.
void
CTaskComplexSequence::Flush()
{
	for(int i = 0; i < 8; i++){
		delete m_Tasks[i];
		m_Tasks[i] = nil;
	}
	m_CurrTaskIdx = 0;
	m_RepeatMode = DONT_REPEAT;
	m_RepeatedCnt = 0;
}

// SA CTaskComplexSequence::CreateNextSubTask.
CTask*
CTaskComplexSequence::CreateNextSubTask(CPed *ped)
{
	return CreateNextSubTask(ped, m_CurrTaskIdx, m_RepeatedCnt);
}

// SA CTaskComplexSequence::CreateFirstSubTask.
CTask*
CTaskComplexSequence::CreateFirstSubTask(CPed *ped)
{
	(void)ped;
	CTask *curr = m_Tasks[m_CurrTaskIdx];
	return curr ? curr->Clone() : nil;
}

// SA CTaskComplexSequence::ControlSubTask: a sequence never interferes
// with the task it is running.
CTask*
CTaskComplexSequence::ControlSubTask(CPed *ped)
{
	(void)ped;
	return m_pSubTask;
}

// SA CTaskComplexSequence::AddTask: first empty slot wins; a task that
// does not fit is deleted rather than leaked.
void
CTaskComplexSequence::AddTask(CTask *task)
{
	assert(task);

	for(int i = 0; i < 8; i++){
		if(m_Tasks[i] == nil){
			m_Tasks[i] = task;
			return;
		}
	}

	delete task;
}

// SA CTaskComplexSequence::AddTask (indexed).
void
CTaskComplexSequence::AddTask(int32 sequenceIdx, CTask *task)
{
	assert(task);
	if(sequenceIdx < 0 || sequenceIdx >= 8){
		delete task;
		return;
	}
	delete m_Tasks[sequenceIdx];
	m_Tasks[sequenceIdx] = task;
}

// SA CTaskComplexSequence::CreateNextSubTask (0x632D60 in SA numbering):
// advance, clone the next prototype, wrap if repeating.
CTask*
CTaskComplexSequence::CreateNextSubTask(CPed *ped, int32 &taskIndex, int32 &repeatCount) const
{
	(void)ped;

	taskIndex++;
	bool bSeqEnd = taskIndex == 8 || m_Tasks[taskIndex] == nil;

	if(m_RepeatMode == FOREVER){
		if(bSeqEnd){
			taskIndex = 0;
			repeatCount++;
		}
		return m_Tasks[taskIndex] ? m_Tasks[taskIndex]->Clone() : nil;
	}
	return bSeqEnd ? nil : m_Tasks[taskIndex]->Clone();
}

// SA CTaskComplexSequence::Contains.
bool
CTaskComplexSequence::Contains(eTaskType taskType) const
{
	for(int i = 0; i < 8; i++)
		if(m_Tasks[i] && m_Tasks[i]->GetTaskType() == taskType)
			return true;
	return false;
}
