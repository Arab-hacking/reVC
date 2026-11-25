#pragma once

// SA CTaskComplexUseSequence: run one of the global CTaskSequences
// sequences by index. The sequence slot is ref-counted; the last user
// out flushes it if it was marked flushable.

#include "TaskComplex.h"

class CTaskComplexUseSequence : public CTaskComplex
{
public:
	int32 m_nSequenceIndex;
	int32 m_nCurrentTaskIndex;
	int32 m_nEndTaskIndex;
	int32 m_nSequenceRepeatedCount;

public:
	CTaskComplexUseSequence();
	explicit CTaskComplexUseSequence(int32 sequenceIndex);
	CTaskComplexUseSequence(const CTaskComplexUseSequence &other);
	~CTaskComplexUseSequence();

	CTask *Clone() const { return new CTaskComplexUseSequence(*this); }
	eTaskType GetTaskType() const { return TASK_COMPLEX_USE_SEQUENCE; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil);
	CTask *CreateNextSubTask(CPed *ped);
	CTask *CreateFirstSubTask(CPed *ped);
	CTask *ControlSubTask(CPed *ped);
};

VALIDATE_SIZE(CTaskComplexUseSequence, 0x1C);
