#include "common.h"

#include "TaskComplexSmartFleeEntity.h"

#include "Entity.h"
#include "EventGlobalGroup.h"
#include "EventSeenPanickedPed.h"
#include "Ped.h"
#include "TaskComplexSmartFleePoint.h"

// SA CTaskComplexSmartFleeEntity (0x65C430). The interesting-event
// record and IK look-at are dropped (those systems are not ported).

CTaskComplexSmartFleeEntity::CTaskComplexSmartFleeEntity(CEntity *fleeEntity, bool scream, float safeDistance, int32 fleeTime,
	int32 posCheckPeriod, float posChangeTolerance, eMoveState moveState)
: m_fleeFrom(fleeEntity), m_time(fleeTime), m_safeDistance(safeDistance), m_pedScream(scream),
  m_posCheckPeriod(posCheckPeriod), m_posChangeTolerance(posChangeTolerance), m_moveState(moveState)
{
	m_pos = CVector(0.0f, 0.0f, 0.0f);
	if(m_fleeFrom)
		m_fleeFrom->RegisterReference(&m_fleeFrom);
}

CTaskComplexSmartFleeEntity::CTaskComplexSmartFleeEntity(const CTaskComplexSmartFleeEntity &other)
: CTaskComplexSmartFleeEntity(other.m_fleeFrom, other.m_pedScream, other.m_safeDistance, other.m_time,
	other.m_posCheckPeriod, other.m_posChangeTolerance)
{
}

CTaskComplexSmartFleeEntity::~CTaskComplexSmartFleeEntity()
{
	if(m_fleeFrom)
		m_fleeFrom->CleanUpOldReference(&m_fleeFrom);
}

// 0x65C680
CTask*
CTaskComplexSmartFleeEntity::CreateNextSubTask(CPed *ped)
{
	(void)ped;
	if(m_pSubTask && m_pSubTask->GetTaskType() == TASK_COMPLEX_SMART_FLEE_POINT)
		return CreateSubTask(TASK_FINISHED);
	return nil;
}

// 0x65C6F0
CTask*
CTaskComplexSmartFleeEntity::CreateFirstSubTask(CPed *ped)
{
	(void)ped;
	if(!m_fleeFrom)
		return nil;
	return CreateSubTask(TASK_COMPLEX_SMART_FLEE_POINT);
}

// 0x65C780
CTask*
CTaskComplexSmartFleeEntity::ControlSubTask(CPed *ped)
{
	if(m_fleeFrom){
		if(m_pSubTask && m_pSubTask->GetTaskType() == TASK_COMPLEX_SMART_FLEE_POINT){
			CTaskComplexSmartFleePoint *fleePointTask = (CTaskComplexSmartFleePoint*)m_pSubTask;
			fleePointTask->m_moveState = m_moveState;

			if(m_posCheckTimer.IsOutOfTime()){
				m_posCheckTimer.Start(m_posCheckPeriod);
				CVector currPos = m_fleeFrom->GetPosition();
				CVector dist = currPos - m_pos;
				if(dist.MagnitudeSqr() >= m_posChangeTolerance * m_posChangeTolerance){
					m_pos = currPos;
					fleePointTask->SetFleePosition(m_pos, m_safeDistance, m_pedScream);

					switch(m_moveState){
					case PEDMOVE_JOG:
					case PEDMOVE_RUN:
					case PEDMOVE_SPRINT: {
						// SA: a running ped spreads panic to bystanders
						CEventSeenPanickedPed seenPanicked(ped);
						GetEventGlobalGroup()->Add(&seenPanicked, false);
						break;
					}
					default:
						break;
					}
				}
			}
		}
	}else{
		m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_LEISURE, nil);
	}
	return m_pSubTask;
}

// 0x65C530
CTask*
CTaskComplexSmartFleeEntity::CreateSubTask(eTaskType taskType)
{
	switch(taskType){
	case TASK_COMPLEX_SMART_FLEE_POINT: {
		m_posCheckTimer.Start(m_posCheckPeriod);
		m_pos = m_fleeFrom->GetPosition();
		return new CTaskComplexSmartFleePoint(m_pos, m_pedScream, m_safeDistance, m_time);
	}
	case TASK_FINISHED:
		return nil;
	default:
		return nil;
	}
}
