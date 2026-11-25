#pragma once

// SA CTaskComplexCrossRoadLookAndAchieveHeading: face the way across the
// road, playing the look-both-ways animation.

#include "TaskComplex.h"

class CTaskComplexCrossRoadLookAndAchieveHeading : public CTaskComplex
{
public:
	int32 m_nDuration;
	float m_fTargetHeading;

public:
	CTaskComplexCrossRoadLookAndAchieveHeading(int32 durationInMs, float fTargetHeading);

	CTask *Clone() const { return new CTaskComplexCrossRoadLookAndAchieveHeading(m_nDuration, m_fTargetHeading); }
	eTaskType GetTaskType() const { return TASK_COMPLEX_CROSS_ROAD_LOOK_AND_ACHIEVE_HEADING; }
	CTask *CreateNextSubTask(CPed *ped);
	CTask *CreateFirstSubTask(CPed *ped) { return CreateSubTask(TASK_SIMPLE_ACHIEVE_HEADING, ped); }
	CTask *ControlSubTask(CPed *ped) { (void)ped; return m_pSubTask; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil);

	CTask *CreateSubTask(eTaskType taskType, CPed *ped);
};

VALIDATE_SIZE(CTaskComplexCrossRoadLookAndAchieveHeading, 0x14);
