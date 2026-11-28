#include "common.h"

#include "TaskComplexSmartFleePoint.h"

#include "General.h"
#include "Ped.h"
#include "PedIntelligence.h"
#include "TaskComplexSequence.h"
#include "TaskComplexWander.h"
#include "TaskComplexWanderFlee.h"
#include "TaskManager.h"
#include "TaskSimpleRunAnim.h"
#include "TaskSimpleStandStill.h"
#include "TaskSimpleTired.h"
#include "Timer.h"

// SA CTaskComplexSmartFleePoint (0x65BD20). The IK look-ats and the
// panic scream are dropped (no IK chain manager; SA speech contexts not
// ported), and TASK_COMPLEX_LEAVE_ANY_CAR is skipped - a ped starting in
// a vehicle flees on foot here (leave-car chain not ported yet).

CTaskComplexSmartFleePoint::CTaskComplexSmartFleePoint(const CVector &fleePos, bool doScream, float safeDist, int32 fleeTimeMs)
: m_fleePoint(fleePos), m_doScream(doScream), m_safeDist(safeDist), m_fleeTimeMs(fleeTimeMs)
{
	m_initalPos = CVector(0.0f, 0.0f, 0.0f);
	m_moveState = PEDMOVE_SPRINT;
	m_fleeDir = 0;
	m_hasFleePointChanged = false;
	m_timer.Start(m_fleeTimeMs);
}

CTaskComplexSmartFleePoint::CTaskComplexSmartFleePoint(const CTaskComplexSmartFleePoint &other)
: CTaskComplexSmartFleePoint(other.m_fleePoint, other.m_doScream, other.m_safeDist, other.m_fleeTimeMs)
{
}

// 0x65BE00
void
CTaskComplexSmartFleePoint::SetDefaultTaskWanderDir(CPed *ped)
{
	CTask *defaultTask = ped->GetIntelligence()->GetTaskManager().GetTaskPrimary(TASK_PRIMARY_DEFAULT);
	if(defaultTask && defaultTask->GetTaskType() == TASK_COMPLEX_WANDER && m_pSubTask && m_pSubTask->GetTaskType() == TASK_COMPLEX_WANDER)
		((CTaskComplexWander*)defaultTask)->SetPathNodesFrom(*(CTaskComplexWander*)m_pSubTask);
}

// 0x65BE40: the path-network direction pointing away from the flee point.
uint32
CTaskComplexSmartFleePoint::ComputeFleeDir(CPed *ped)
{
	CVector pos = ped->GetPosition();
	return CGeneral::GetNodeHeadingFromVector(pos.x - m_fleePoint.x, pos.y - m_fleePoint.y);
}

// 0x65BE80
CTask*
CTaskComplexSmartFleePoint::CreateSubTask(eTaskType taskType, CPed *ped)
{
	switch(taskType){
	case TASK_COMPLEX_WANDER:
		return new CTaskComplexWanderFlee(m_moveState, m_fleeDir);
	case TASK_SIMPLE_STAND_STILL:
		SetDefaultTaskWanderDir(ped);
		return new CTaskSimpleStandStill(0, false, false, 8.0f);
	case TASK_COMPLEX_SEQUENCE: {
		if(m_moveState != PEDMOVE_RUN)
			return nil;
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

// 0x65C3C0
void
CTaskComplexSmartFleePoint::SetFleePosition(const CVector &fleePos, float safeDist, bool doScream)
{
	if(m_fleePoint != fleePos || m_safeDist != safeDist){
		m_fleePoint = fleePos;
		m_safeDist = safeDist;
		m_hasFleePointChanged = true;
	}
	m_doScream = doScream;
}

// 0x65BDC0
bool
CTaskComplexSmartFleePoint::MakeAbortable(CPed *ped, eAbortPriority priority, const CEvent *event)
{
	if(priority == ABORT_PRIORITY_LEISURE){
		m_fleeTimeMs = -1;
		m_timer.SetOutOfTime();
	}
	return m_pSubTask->MakeAbortable(ped, priority, event);
}

// 0x65C0C0
CTask*
CTaskComplexSmartFleePoint::CreateNextSubTask(CPed *ped)
{
	switch(m_pSubTask->GetTaskType()){
	case TASK_SIMPLE_STAND_STILL:
		return CreateSubTask(TASK_COMPLEX_SEQUENCE, ped);
	case TASK_COMPLEX_SEQUENCE:
		return CreateSubTask(TASK_FINISHED, ped);
	case TASK_COMPLEX_WANDER: // the flee wander finished
		return nil;
	default:
		return nil;
	}
}

// 0x65C140
CTask*
CTaskComplexSmartFleePoint::CreateFirstSubTask(CPed *ped)
{
	m_initalPos = ped->GetPosition();
	m_fleeDir = (uint8)ComputeFleeDir(ped);
	return CreateSubTask(TASK_COMPLEX_WANDER, ped);
}

// 0x65C1E0 (minus the scream and IK look-at; see file header)
CTask*
CTaskComplexSmartFleePoint::ControlSubTask(CPed *ped)
{
	if(m_pSubTask && m_pSubTask->GetTaskType() == TASK_COMPLEX_WANDER){
		CTaskComplexWander *subWander = (CTaskComplexWander*)m_pSubTask;
		subWander->m_nMoveState = m_moveState;

		if(m_hasFleePointChanged){
			m_hasFleePointChanged = false;
			if(m_timer.m_bStarted){
				m_timer.m_nStartTime = CTimer::GetTimeInMilliseconds();
				m_timer.m_nInterval = m_fleeTimeMs;
				m_timer.m_bStarted = true;
			}
			uint8 dir = (uint8)ComputeFleeDir(ped);
			if(m_fleeDir != dir){
				m_fleeDir = dir;
				subWander->SetDirection(dir);
			}
		}else if(m_timer.IsOutOfTime()){
			return CreateSubTask(TASK_SIMPLE_STAND_STILL, ped);
		}else{
			CVector pedPos = ped->GetPosition();
			CVector distToFlee = pedPos - m_fleePoint;
			CVector distFromStart = pedPos - m_initalPos;
			if(distToFlee.MagnitudeSqr() > m_safeDist * m_safeDist && distFromStart.MagnitudeSqr() > m_safeDist * m_safeDist)
				return CreateSubTask(TASK_SIMPLE_STAND_STILL, ped);
		}
	}
	return m_pSubTask;
}
