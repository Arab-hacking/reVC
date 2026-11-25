#pragma once

// SA CTaskSimpleGoToPointFine: walk to a point with a continuously
// variable speed - the move animation is a blend of idle/walk/run/
// sprint weighted by m_fMoveRatio. This is what makes a ped ease into
// and out of corners on a node route instead of snapping between move
// states (driven by CTaskComplexFollowNodeRoute's blending).

#include "TaskSimpleGoTo.h"

class CEntity;

class CTaskSimpleGoToPointFine : public CTaskSimpleGoTo
{
public:
	float m_fMoveRatio;

public:
	CTaskSimpleGoToPointFine(float moveRatio, CVector targetPoint, float fRadius = 0.5f, CEntity *entity = nil);

	CTask *Clone() const;
	eTaskType GetTaskType() const { return TASK_SIMPLE_GO_TO_POINT_FINE; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil);
	bool ProcessPed(CPed *ped);

	void Finish(CPed *ped);
	void SetBlendedMoveAnim(CPed *ped);
	void SetTargetPos(CVector posn);
	void SetMoveRatio(float value) { m_fMoveRatio = value; }
	static float BaseRatio(eMoveState moveState);
};

VALIDATE_SIZE(CTaskSimpleGoToPointFine, 0x24);
