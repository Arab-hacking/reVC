#pragma once

// SA CTaskSimpleTired: play the idle-tired animation for a while - what
// a ped does after a sprint (and at the end of a flee).

#include "TaskSimple.h"

class CAnimBlendAssociation;

class CTaskSimpleTired : public CTaskSimple
{
public:
	uint32 m_TiredDurationMs;
	uint32 m_AnimStartMs;
	uint32 m_TiredDurationMs_Copy;
	bool m_AnimHasStarted;
	bool m_NeedsToStartAnim;
	CAnimBlendAssociation *m_TiredAnim;

public:
	explicit CTaskSimpleTired(uint32 tiredDurationMs);

	CTask *Clone() const { return new CTaskSimpleTired(m_TiredDurationMs); }
	eTaskType GetTaskType() const { return TASK_SIMPLE_TIRED; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil);
	bool ProcessPed(CPed *ped);
	void StartAnim(CPed *ped);
};

VALIDATE_SIZE(CTaskSimpleTired, 0x1C);
