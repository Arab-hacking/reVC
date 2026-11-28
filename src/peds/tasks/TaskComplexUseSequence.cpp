#include "TaskComplexUseSequence.h"

#include "TaskComplexSequence.h"
#include "TaskSequences.h"

// SA CTaskComplexUseSequence. The EVENT_DAMAGE special case in SA's
// MakeAbortable (dropping the sequence when the killing blow lands)
// needs CEventDamage, which is not ported yet, so it is absent here.

// 0x635450
CTaskComplexUseSequence::CTaskComplexUseSequence()
: m_nSequenceIndex(-1), m_nCurrentTaskIndex(0), m_nEndTaskIndex(-1), m_nSequenceRepeatedCount(0)
{
}

CTaskComplexUseSequence::CTaskComplexUseSequence(int32 sequenceIndex)
: m_nSequenceIndex(sequenceIndex), m_nCurrentTaskIndex(0), m_nEndTaskIndex(-1), m_nSequenceRepeatedCount(0)
{
	CTaskSequences::ms_taskSequence[m_nSequenceIndex].m_RefCnt++;
}

CTaskComplexUseSequence::CTaskComplexUseSequence(const CTaskComplexUseSequence &other)
: m_nSequenceIndex(other.m_nSequenceIndex), m_nCurrentTaskIndex(0), m_nEndTaskIndex(-1), m_nSequenceRepeatedCount(0)
{
	CTaskSequences::ms_taskSequence[m_nSequenceIndex].m_RefCnt++;
	m_nCurrentTaskIndex = other.m_nCurrentTaskIndex;
	m_nEndTaskIndex = other.m_nEndTaskIndex;
}

CTaskComplexUseSequence::~CTaskComplexUseSequence()
{
	if(m_nSequenceIndex != -1){
		CTaskComplexSequence *sequence = &CTaskSequences::ms_taskSequence[m_nSequenceIndex];
		bool bFinalReference = sequence->m_RefCnt == 1;
		sequence->m_RefCnt--;
		if(bFinalReference && sequence->m_bFlushTasks){
			sequence->m_bFlushTasks = false;
			sequence->Flush();
		}
	}
}

bool
CTaskComplexUseSequence::MakeAbortable(CPed *ped, eAbortPriority priority, const CEvent *event)
{
	if(m_pSubTask == nil)
		return true;
	return m_pSubTask->MakeAbortable(ped, priority, event);
}

CTask*
CTaskComplexUseSequence::CreateNextSubTask(CPed *ped)
{
	if(m_nSequenceIndex == -1)
		return nil;
	CTaskComplexSequence *sequence = &CTaskSequences::ms_taskSequence[m_nSequenceIndex];
	return sequence->CreateNextSubTask(ped, m_nCurrentTaskIndex, m_nSequenceRepeatedCount);
}

CTask*
CTaskComplexUseSequence::CreateFirstSubTask(CPed *ped)
{
	(void)ped;

	if(m_nSequenceIndex == -1)
		return nil;

	CTask *firstSubTask = nil;
	CTask *currentSequenceTask = CTaskSequences::ms_taskSequence[m_nSequenceIndex].m_Tasks[m_nCurrentTaskIndex];
	if(currentSequenceTask)
		firstSubTask = currentSequenceTask->Clone();

	if(m_nEndTaskIndex != -1){
		if(firstSubTask && firstSubTask->GetTaskType() == TASK_COMPLEX_USE_SEQUENCE){
			CTaskComplexUseSequence *sequence = (CTaskComplexUseSequence*)firstSubTask;
			sequence->m_nCurrentTaskIndex = m_nEndTaskIndex;
			sequence->m_nEndTaskIndex = -1;
		}
	}
	return firstSubTask;
}

CTask*
CTaskComplexUseSequence::ControlSubTask(CPed *ped)
{
	(void)ped;
	return m_pSubTask;
}
