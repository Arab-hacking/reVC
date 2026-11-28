#ifndef __GTA_TASKSIMPLESTANDSTILL_H__
#define __GTA_TASKSIMPLESTANDSTILL_H__

#include "TaskSimple.h"
#include "TaskTimer.h"

// SA CTaskSimpleStandStill (0x20): stand in the idle stance for a while
// (or forever, when looped). This is the task an idle ped bottoms out in,
// and what CPedIntelligence::ClearTasks parks a ped with.
class CTaskSimpleStandStill : public CTaskSimple
{
public:
	int32 m_nTime;
	CTaskTimer m_timer;
	bool m_bLooped;
	bool m_bUseAnimIdleStance;
	float m_fBlendData;

public:
	static const eTaskType Type = TASK_SIMPLE_STAND_STILL;

	CTaskSimpleStandStill(int32 nTime = 0, bool looped = false, bool bUseAnimIdleStance = false, float fBlendData = 8.0f);
	~CTaskSimpleStandStill() {}

	eTaskType GetTaskType() const { return Type; }
	CTask *Clone() const { return new CTaskSimpleStandStill(m_nTime, m_bLooped, m_bUseAnimIdleStance, m_fBlendData); }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil);
	bool ProcessPed(CPed *ped);
};

VALIDATE_SIZE(CTaskSimpleStandStill, 0x20);

#endif // __GTA_TASKSIMPLESTANDSTILL_H__
