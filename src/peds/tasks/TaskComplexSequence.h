#ifndef __GTA_TASKCOMPLEXSEQUENCE_H__
#define __GTA_TASKCOMPLEXSEQUENCE_H__

#include "TaskComplex.h"

// SA CTaskComplexSequence (0x40): run a fixed list of tasks one after
// another. The sequence owns prototype tasks and hands the task manager a
// clone of each in turn, so the same sequence object can be re-run (and
// shared, through CTaskComplexUseSequence, by script code).
class CTaskComplexSequence : public CTaskComplex
{
public:
	enum eRepeatMode : uint32 {
		DONT_REPEAT,
		FOREVER,
	};

	int32 m_CurrTaskIdx;
	CTask *m_Tasks[8];
	eRepeatMode m_RepeatMode;
	int32 m_RepeatedCnt;
	bool m_bFlushTasks;
	uint32 m_RefCnt;

public:
	static const eTaskType Type = TASK_COMPLEX_SEQUENCE;

	CTaskComplexSequence();
	CTaskComplexSequence(const CTaskComplexSequence &other);
	~CTaskComplexSequence();

	eTaskType GetTaskType() const { return Type; }
	CTask *Clone() const { return new CTaskComplexSequence(*this); }
	CTask *CreateNextSubTask(CPed *ped);
	CTask *CreateFirstSubTask(CPed *ped);
	CTask *ControlSubTask(CPed *ped);

	void AddTask(CTask *task);
	void AddTask(int32 sequenceIdx, CTask *task);
	CTask *CreateNextSubTask(CPed *ped, int32 &taskIndex, int32 &repeatCount) const;
	void Flush();
	bool Contains(eTaskType taskType) const;
	void SetCanBeEmptied(bool flush) { m_bFlushTasks = flush; }
	void DeRegister() { if(m_RefCnt) m_RefCnt--; }
	void SetRepeatMode(bool repeat) { m_RepeatMode = repeat ? FOREVER : DONT_REPEAT; }
	bool IsActive() const { return m_Tasks[0] != nil; }
};

VALIDATE_SIZE(CTaskComplexSequence, 0x40);

#endif // __GTA_TASKCOMPLEXSEQUENCE_H__
