#ifndef __GTA_TASKCOMPLEXGOTOPOINTANDSTANDSTILL_H__
#define __GTA_TASKCOMPLEXGOTOPOINTANDSTANDSTILL_H__

#include "TaskComplex.h"
#include "PedType.h"
#include "Vector.h"

class CTaskSimpleGoToPoint;

// SA CTaskComplexGoToPointAndStandStill (0x28): go to a point, then stand
// there briefly. The workhorse "move here" task for scripts and AI.
class CTaskComplexGoToPointAndStandStill : public CTaskComplex
{
public:
	eMoveState m_moveState;
	CVector m_vecTargetPoint;
	float m_fRadius;          // Ped stops moving after it is within the radius of the point.
	float m_fMoveStateRadius; // The ped will either sprint, run, or walk depending on how far it is from the radius
	bool m_bMustOverShootTarget;
	bool m_bStopExactly;
	bool m_bNewTarget;
	bool m_bTrackingEntity;
	bool m_bWasSuccessful;

public:
	static const eTaskType Type = TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL;

	CTaskComplexGoToPointAndStandStill(
		eMoveState moveState,
		const CVector &targetPoint,
		float fRadius = 1.5f,
		float fMoveStateRadius = 2.0f,
		bool bMustOverShootTarget = false,
		bool bStopExactly = false,
		bool bTrackingEntity = false
	);
	~CTaskComplexGoToPointAndStandStill() {}

	eTaskType GetTaskType() const { return Type; }
	CTask *Clone() const;
	CTask *CreateNextSubTask(CPed *ped);
	CTask *CreateFirstSubTask(CPed *ped);
	CTask *ControlSubTask(CPed *ped);

	void GoToPoint(const CVector &targetPoint, float fRadius = 1.5f, float fMoveStateRadius = 2.0f, bool bUpdateTargetEvenIfItsTheSame = false);
	void SelectMoveState(CTaskSimpleGoToPoint *gotoPointTask, CPed *ped, float fMoveStateRadius, float fRunOrSprintRadius);
	CTask *CreateSubTask(eTaskType taskType, CPed *ped);
};

VALIDATE_SIZE(CTaskComplexGoToPointAndStandStill, 0x28);

#endif // __GTA_TASKCOMPLEXGOTOPOINTANDSTANDSTILL_H__
