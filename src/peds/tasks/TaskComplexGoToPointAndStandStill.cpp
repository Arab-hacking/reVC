#include "common.h"

#include "TaskComplexGoToPointAndStandStill.h"
#include "Ped.h"
#include "TaskSimpleGoToPoint.h"
#include "TaskSimplePause.h"
#include "TaskSimpleStandStill.h"
#include "Timer.h"

// SA CTaskComplexGoToPointAndStandStill::CTaskComplexGoToPointAndStandStill (0x668120).
CTaskComplexGoToPointAndStandStill::CTaskComplexGoToPointAndStandStill(
	eMoveState moveState,
	const CVector &targetPoint,
	float fRadius,
	float fMoveStateRadius,
	bool bMustOverShootTarget,
	bool bStopExactly,
	bool bTrackingEntity
)
	: m_moveState(moveState)
	, m_bMustOverShootTarget(bMustOverShootTarget)
	, m_bStopExactly(bStopExactly)
	, m_bNewTarget(false)
	, m_bTrackingEntity(bTrackingEntity)
	, m_bWasSuccessful(false)
{
	GoToPoint(targetPoint, fRadius, fMoveStateRadius);

	if(m_bStopExactly){
		m_bMustOverShootTarget = false;
		m_fMoveStateRadius     = 0.0f;
		m_fRadius              = Max(m_moveState <= PEDMOVE_WALK ? 0.5f : 1.0f, m_fRadius);
	}
}

// SA CTaskComplexGoToPointAndStandStill::Clone (0x66CEA0).
CTask*
CTaskComplexGoToPointAndStandStill::Clone() const
{
	return new CTaskComplexGoToPointAndStandStill(m_moveState, m_vecTargetPoint, m_fRadius, m_fMoveStateRadius, m_bMustOverShootTarget, m_bStopExactly);
}

// SA CTaskComplexGoToPointAndStandStill::CreateNextSubTask (0x66DBA0).
CTask*
CTaskComplexGoToPointAndStandStill::CreateNextSubTask(CPed *ped)
{
	if(m_pSubTask == nil)
		return CreateSubTask(TASK_FINISHED, ped);

	switch(m_pSubTask->GetTaskType()){
	case TASK_SIMPLE_STAND_STILL:
		return CreateSubTask(TASK_FINISHED, ped);
	case TASK_COMPLEX_LEAVE_CAR:
		return CreateSubTask(ped->bInVehicle ? TASK_FINISHED : TASK_SIMPLE_GO_TO_POINT, ped);
	case TASK_SIMPLE_GO_TO_POINT:
		m_bWasSuccessful = ((CTaskSimpleGoToPoint*)m_pSubTask)->gotoPointFlags.m_b03 != 0;
		return CreateSubTask(TASK_SIMPLE_STAND_STILL, ped);
	default:
		// SA asserts here; finishing is the safe equivalent.
		return CreateSubTask(TASK_FINISHED, ped);
	}
}

// SA CTaskComplexGoToPointAndStandStill::CreateFirstSubTask (0x66DC40).
CTask*
CTaskComplexGoToPointAndStandStill::CreateFirstSubTask(CPed *ped)
{
	m_bNewTarget = false;
	return CreateSubTask(ped->bInVehicle ? TASK_COMPLEX_LEAVE_CAR : TASK_SIMPLE_GO_TO_POINT, ped);
}

// SA CTaskComplexGoToPointAndStandStill::ControlSubTask (0x668570).
CTask*
CTaskComplexGoToPointAndStandStill::ControlSubTask(CPed *ped)
{
	if(m_bNewTarget){
		if(m_pSubTask->MakeAbortable(ped))
			return CreateFirstSubTask(ped);
	}else if(m_pSubTask->GetTaskType() == TASK_SIMPLE_GO_TO_POINT){
		CTaskSimpleGoToPoint *gotoPointTask = (CTaskSimpleGoToPoint*)m_pSubTask;
		if(m_moveState == PEDMOVE_RUN){
			SelectMoveState(gotoPointTask, ped, m_fMoveStateRadius, 100000000.0f);
		}else if(m_moveState == PEDMOVE_SPRINT){
			SelectMoveState(gotoPointTask, ped, m_fMoveStateRadius, 10.0f);
		}
		return m_pSubTask;
	}
	return m_pSubTask;
}

// SA CTaskComplexGoToPointAndStandStill::GoToPoint (0x46FE60).
void
CTaskComplexGoToPointAndStandStill::GoToPoint(const CVector &targetPoint, float fRadius, float fMoveStateRadius, bool bUpdateTargetEvenIfItsTheSame)
{
	if(bUpdateTargetEvenIfItsTheSame || m_vecTargetPoint != targetPoint || m_fMoveStateRadius != fMoveStateRadius){
		m_vecTargetPoint = targetPoint;
		m_fRadius = fRadius;
		m_fMoveStateRadius = fMoveStateRadius;
		m_bNewTarget = true;
	}
}

// SA CTaskComplexGoToPointAndStandStill::SelectMoveState (0x668250): the
// closer the ped gets, the slower it approaches.
void
CTaskComplexGoToPointAndStandStill::SelectMoveState(CTaskSimpleGoToPoint *gotoPointTask, CPed *ped, float fMoveStateRadius, float fRunOrSprintRadius)
{
	CVector2D dist = gotoPointTask->m_vecTargetPoint - ped->GetPosition();
	const float dist2DSq = dist.MagnitudeSqr();
	gotoPointTask->m_moveState = dist2DSq >= fMoveStateRadius * fMoveStateRadius
		? dist2DSq >= fRunOrSprintRadius * fRunOrSprintRadius ? PEDMOVE_SPRINT : PEDMOVE_RUN
		: PEDMOVE_WALK;
}

// SA CTaskComplexGoToPointAndStandStill::CreateSubTask (0x6682D0).
CTask*
CTaskComplexGoToPointAndStandStill::CreateSubTask(eTaskType taskType, CPed *ped)
{
	switch(taskType){
	case TASK_FINISHED:
		return nil;
	case TASK_SIMPLE_GO_TO_POINT: {
		CTaskSimpleGoToPoint *gotoPoint = new CTaskSimpleGoToPoint(m_moveState, m_vecTargetPoint, m_fRadius, false, m_bMustOverShootTarget);

		if(m_bTrackingEntity)
			gotoPoint->gotoPointFlags.m_b05 = true;

		if(m_moveState == PEDMOVE_RUN)
			SelectMoveState(gotoPoint, ped, m_fMoveStateRadius, 100000000.0f);
		else if(m_moveState == PEDMOVE_SPRINT)
			SelectMoveState(gotoPoint, ped, m_fMoveStateRadius, 10.0f);

		return gotoPoint;
	}
	case TASK_COMPLEX_LEAVE_CAR:
		// CTaskComplexLeaveCar is not ported yet, so a ped in a vehicle
		// cannot start this task; SA creates the leave-car task here.
		return nil;
	case TASK_SIMPLE_PAUSE:
		return new CTaskSimplePause(1);
	case TASK_SIMPLE_STAND_STILL: {
		if(m_bStopExactly && m_pSubTask && m_pSubTask->GetTaskType() == TASK_SIMPLE_GO_TO_POINT){
			CVector vecDistance = m_vecTargetPoint - ped->GetPosition();
			float fDotProduct = DotProduct(vecDistance, ped->GetForward());
			float fBlendDelta = 8.0f;
			// SA reads m_vecAnimMovingShiftLocal.y; reVC's equivalent
			// per-frame anim shift is m_vecAnimMoveDelta.
			if(ped->m_vecAnimMoveDelta.y >= 0.01f){
				fBlendDelta = 50.0f / ((fDotProduct / (ped->m_vecAnimMoveDelta.y * 0.5f) - 1.0f) * CTimer::GetTimeStep());
				if(fDotProduct <= 0.01f || fBlendDelta > 16.0f)
					fBlendDelta = 16.0f;
			}
			return new CTaskSimpleStandStill(2000, false, true, fBlendDelta);
		}
		return new CTaskSimpleStandStill(1, false, false, 8.0f);
	}
	default:
		// SA asserts here; finishing is the safe equivalent.
		return nil;
	}
}
