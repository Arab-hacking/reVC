#include "TaskComplexFleePoint.h"

#include "General.h"
#include "Ped.h"
#include "TaskComplexGoToPointAndStandStill.h"
#include "TaskComplexSequence.h"
#include "TaskSimpleGoToPoint.h"
#include "TaskSimpleRunAnim.h"
#include "TaskSimpleTired.h"
#include "Timer.h"
#include "Vector2D.h"

// SA CTaskComplexFleePoint (0x65B390). The IK look-ats and the panic
// scream are dropped (no IK chain manager; SA speech contexts not
// ported), and TASK_COMPLEX_LEAVE_ANY_CAR is replaced by starting the
// flee on foot (leave-car chain not ported yet).

CTaskComplexFleePoint::CTaskComplexFleePoint(const CVector &fleeFromPos, bool bScream, float safeDist, int32 durationMs)
: m_fleeFromPos(fleeFromPos), m_safeDist(safeDist), m_durationMs(durationMs), m_bScream(bScream)
{
	m_fleeToPos = CVector(0.0f, 0.0f, 0.0f);
	m_newFleePosWasSet = false;
	m_timer.Start(durationMs);
}

CTaskComplexFleePoint::CTaskComplexFleePoint(const CTaskComplexFleePoint &other)
: CTaskComplexFleePoint(other.m_fleeFromPos, other.m_bScream, other.m_safeDist, other.m_durationMs)
{
}

// 0x65B460
CTask*
CTaskComplexFleePoint::CreateSubTask(eTaskType taskType)
{
	switch(taskType){
	case TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL:
		return new CTaskComplexGoToPointAndStandStill(PEDMOVE_SPRINT, m_fleeToPos, 0.5f, 2.0f, false, false);
	case TASK_SIMPLE_GO_TO_POINT:
		return new CTaskSimpleGoToPoint(PEDMOVE_SPRINT, m_fleeToPos, 0.5f, false, false);
	case TASK_COMPLEX_SEQUENCE: {
		CTaskComplexSequence *seq = new CTaskComplexSequence();
		seq->AddTask(new CTaskSimpleRunAnim(ANIM_GROUP_DEFAULT, ANIM_ID_FLEE_LKAROUND_01, 4.0f, false));
		seq->AddTask(new CTaskSimpleTired(2000));
		return seq;
	}
	case TASK_FINISHED:
		return nil;
	default:
		return nil;
	}
}

// 0x65B7B0
void
CTaskComplexFleePoint::SetFleePosition(const CVector &fleePos, float safeDistance, bool scream)
{
	if(m_fleeFromPos != fleePos){
		m_fleeFromPos = fleePos;
		m_safeDist = safeDistance;
		m_newFleePosWasSet = true;
	}
	m_bScream = scream;
}

// 0x65B820
void
CTaskComplexFleePoint::ComputeTargetPoint(const CPed *ped)
{
	const CVector &pedPos = ped->GetPosition();
	float angleRad = CGeneral::GetRandomNumberInRange(-0.33f, 0.33f);

	CVector2D dir2d(pedPos.x - m_fleeFromPos.x, pedPos.y - m_fleeFromPos.y);
	dir2d.Normalise();
	// rotate the away-direction by the random spread angle
	float cosA = cosf(angleRad);
	float sinA = sinf(angleRad);
	CVector2D dir(dir2d.x * cosA - dir2d.y * sinA, dir2d.x * sinA + dir2d.y * cosA);

	int32 spread = CGeneral::GetRandomNumberInRange(3, 6);

	m_fleeToPos = pedPos;
	m_fleeToPos.x += (dir.x + dir.y) * spread;
	m_fleeToPos.y += (dir.x - dir.y) * spread;
}

// 0x65B420
bool
CTaskComplexFleePoint::MakeAbortable(CPed *ped, eAbortPriority priority, const CEvent *event)
{
	if(priority == ABORT_PRIORITY_LEISURE){
		m_durationMs = -1;
		m_timer.m_nStartTime = CTimer::GetTimeInMilliseconds();
		m_timer.m_nInterval = -1;
		m_timer.m_bStarted = true;
	}
	return m_pSubTask->MakeAbortable(ped, priority, event);
}

// 0x65D150
CTask*
CTaskComplexFleePoint::CreateNextSubTask(CPed *ped)
{
	switch(m_pSubTask->GetTaskType()){
	case TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL:
		return CreateSubTask(TASK_COMPLEX_SEQUENCE);
	case TASK_COMPLEX_SEQUENCE:
		return CreateSubTask(TASK_FINISHED);
	case TASK_SIMPLE_GO_TO_POINT: {
		ComputeTargetPoint(ped);
		CVector dist = ped->GetPosition() - m_fleeToPos;
		return CreateSubTask(
			dist.MagnitudeSqr() < m_safeDist * m_safeDist
				? TASK_SIMPLE_GO_TO_POINT
				: TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL);
	}
	default:
		return nil;
	}
}

// 0x65D100
CTask*
CTaskComplexFleePoint::CreateFirstSubTask(CPed *ped)
{
	ComputeTargetPoint(ped);
	return CreateSubTask(TASK_SIMPLE_GO_TO_POINT);
}

// 0x65B660 (minus the IK look-at and scream; see file header)
CTask*
CTaskComplexFleePoint::ControlSubTask(CPed *ped)
{
	switch(m_pSubTask->GetTaskType()){
	case TASK_SIMPLE_GO_TO_POINT:
	case TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL:
		break;
	default:
		return m_pSubTask;
	}

	if(m_newFleePosWasSet){
		m_newFleePosWasSet = false;
		m_timer.Start(m_durationMs);
		return CreateFirstSubTask(ped);
	}else if(m_timer.IsOutOfTime()){
		return CreateSubTask(TASK_COMPLEX_SEQUENCE);
	}

	return m_pSubTask;
}
