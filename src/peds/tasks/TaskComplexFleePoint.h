#pragma once

// SA CTaskComplexFleePoint: sprint away from a position until at a safe
// distance (or the time limit runs out), then look around and catch your
// breath. If the ped starts in a vehicle SA first leaves the car; the
// leave-car chain is not ported yet, so fleeing starts on foot either
// way here.

#include "TaskComplex.h"
#include "TaskTimer.h"
#include "Vector.h"

class CTaskComplexFleePoint : public CTaskComplex
{
public:
	CVector m_fleeFromPos;
	CVector m_fleeToPos;
	int32 m_durationMs;
	CTaskTimer m_timer;
	float m_safeDist;
	bool m_bScream;
	bool m_newFleePosWasSet;

public:
	CTaskComplexFleePoint(const CVector &point, bool bScream, float safeDist, int32 timeLimit);
	CTaskComplexFleePoint(const CTaskComplexFleePoint &other);

	CTask *Clone() const { return new CTaskComplexFleePoint(*this); }
	eTaskType GetTaskType() const { return TASK_COMPLEX_FLEE_POINT; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil);
	CTask *CreateNextSubTask(CPed *ped);
	CTask *CreateFirstSubTask(CPed *ped);
	CTask *ControlSubTask(CPed *ped);

	CTask *CreateSubTask(eTaskType taskType);
	void SetFleePosition(const CVector &fleePos, float safeDistance, bool scream);
	void ComputeTargetPoint(const CPed *ped);
};

VALIDATE_SIZE(CTaskComplexFleePoint, 0x38);
