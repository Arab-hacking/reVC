#ifndef __GTA_TASKSIMPLEGOTOPOINT_H__
#define __GTA_TASKSIMPLEGOTOPOINT_H__

#include "TaskSimpleGoTo.h"

// SA CTaskSimpleGoToPoint (0x30): walk/run/sprint in a straight line to a
// point and stop inside its radius. This is the movement primitive the
// complex go-to, wander and seek tasks are built from.
class CTaskSimpleGoToPoint : public CTaskSimpleGoTo
{
public:
	union {
		struct {
			uint8 m_bMoveTowardsTargetPoint : 1;
			uint8 m_b02 : 1;
			uint8 m_b03 : 1;
			uint8 m_b04 : 1;
			uint8 m_b05 : 1;
		} gotoPointFlags;
		uint8 m_GoToPointFlags;
	};
	CVector m_vecLastPedPos;

public:
	static const eTaskType Type = TASK_SIMPLE_GO_TO_POINT;

	CTaskSimpleGoToPoint(eMoveState moveState, const CVector &targetPoint, float fRadius = 0.5f, bool bMoveTowardsTargetPoint = false, bool a6 = false);
	~CTaskSimpleGoToPoint() {}

	eTaskType GetTaskType() const { return Type; }
	CTask *Clone() const { return new CTaskSimpleGoToPoint(m_moveState, m_vecTargetPoint, m_fRadius, gotoPointFlags.m_bMoveTowardsTargetPoint, gotoPointFlags.m_b04); }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil);
	bool ProcessPed(CPed *ped);

	// bDontCheckRadius is always false in SA.
	void UpdatePoint(const CVector &targetPosition, float fRadius = 0.5f, bool bDontCheckRadius = false);

	bool WasTaskSuccessful(CPed *ped) const;
};

VALIDATE_SIZE(CTaskSimpleGoToPoint, 0x30);

#endif // __GTA_TASKSIMPLEGOTOPOINT_H__
