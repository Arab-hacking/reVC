#pragma once

// SA CTaskComplexSmartFleePoint: flee a position by wandering the path
// network away from it (CTaskComplexWanderFlee), keeping the wander
// direction pointed away from the threat until the ped is far enough
// from both the threat and where it started, or the time runs out.

#include "PedType.h"
#include "TaskComplex.h"
#include "TaskTimer.h"
#include "Vector.h"

class CTaskComplexSmartFleePoint : public CTaskComplex
{
public:
	CVector m_initalPos;
	CVector m_fleePoint;
	bool m_doScream;
	int32 m_fleeTimeMs;
	float m_safeDist;
	eMoveState m_moveState;
	CTaskTimer m_timer;
	uint8 m_fleeDir;
	bool m_hasFleePointChanged;

public:
	CTaskComplexSmartFleePoint(const CVector &fleePos, bool doScream, float safeDist, int32 fleeTimeMs);
	CTaskComplexSmartFleePoint(const CTaskComplexSmartFleePoint &other);

	CTask *Clone() const { return new CTaskComplexSmartFleePoint(*this); }
	eTaskType GetTaskType() const { return TASK_COMPLEX_SMART_FLEE_POINT; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil);
	CTask *CreateNextSubTask(CPed *ped);
	CTask *CreateFirstSubTask(CPed *ped);
	CTask *ControlSubTask(CPed *ped);

	void SetDefaultTaskWanderDir(CPed *ped);
	uint32 ComputeFleeDir(CPed *ped);
	CTask *CreateSubTask(eTaskType taskType, CPed *ped);
	void SetFleePosition(const CVector &fleePos, float safeDist, bool doScream);
};

VALIDATE_SIZE(CTaskComplexSmartFleePoint, 0x44);
