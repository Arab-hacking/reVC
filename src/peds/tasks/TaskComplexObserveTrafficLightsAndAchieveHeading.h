#pragma once

// SA CTaskComplexObserveTrafficLightsAndAchieveHeading: face the way
// across, then wait for the pedestrian lights.

#include "TaskComplex.h"

class CTaskComplexObserveTrafficLightsAndAchieveHeading : public CTaskComplex
{
public:
	int32 m_nDurationInMs;
	float m_fTargetHeading;

public:
	CTaskComplexObserveTrafficLightsAndAchieveHeading(int32 durationInMs, float targetHeading);

	CTask *Clone() const { return new CTaskComplexObserveTrafficLightsAndAchieveHeading(m_nDurationInMs, m_fTargetHeading); }
	eTaskType GetTaskType() const { return TASK_COMPLEX_OBSERVE_TRAFFIC_LIGHTS_AND_ACHIEVE_HEADING; }
	CTask *CreateNextSubTask(CPed *ped);
	CTask *CreateFirstSubTask(CPed *ped);
	CTask *ControlSubTask(CPed *ped);
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil);

	CTask *CreateSubTask(eTaskType taskType) const;
};

VALIDATE_SIZE(CTaskComplexObserveTrafficLightsAndAchieveHeading, 0x14);
