#pragma once

// SA CTaskComplexObserveTrafficLights: stand at the kerb (with the odd
// road-cross look) until the lights say pedestrians may cross.

#include "TaskComplex.h"
#include "TaskTimer.h"

class CTaskComplexObserveTrafficLights : public CTaskComplex
{
public:
	CTaskTimer m_Timer;

public:
	CTaskComplexObserveTrafficLights() {}

	CTask *Clone() const { return new CTaskComplexObserveTrafficLights(); }
	eTaskType GetTaskType() const { return TASK_COMPLEX_OBSERVE_TRAFFIC_LIGHTS; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil) { (void)ped; (void)priority; (void)event; return true; }
	CTask *CreateNextSubTask(CPed *ped);
	CTask *CreateFirstSubTask(CPed *ped) { return CreateNextSubTask(ped); }
	CTask *ControlSubTask(CPed *ped);
};

VALIDATE_SIZE(CTaskComplexObserveTrafficLights, 0x18);
