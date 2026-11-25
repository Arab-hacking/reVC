#include "TaskComplexFleeEntity.h"

#include "Entity.h"
#include "EventGlobalGroup.h"
#include "EventSeenPanickedPed.h"
#include "Ped.h"
#include "TaskComplexFleePoint.h"
#include "Timer.h"

// SA CTaskComplexFleeEntity (0x65B930). The interesting-event record and
// IK look-at at creation are dropped (neither system is ported).

CTaskComplexFleeEntity::CTaskComplexFleeEntity(CEntity *fleeEntity, bool scream, float safeDistance, int32 fleeTime,
	int32 posCheckPeriod, float posChangeTolerance)
: m_entity(fleeEntity), m_time(fleeTime), m_safeDistance(safeDistance), m_pedScream(scream),
  m_posCheckPeriod(posCheckPeriod), m_posChangeTolerance(posChangeTolerance)
{
	m_pos = CVector(0.0f, 0.0f, 0.0f);
	if(m_entity)
		m_entity->RegisterReference(&m_entity);
}

CTaskComplexFleeEntity::CTaskComplexFleeEntity(const CTaskComplexFleeEntity &other)
: CTaskComplexFleeEntity(other.m_entity, other.m_pedScream, other.m_safeDistance, other.m_time,
	other.m_posCheckPeriod, other.m_posChangeTolerance)
{
}

CTaskComplexFleeEntity::~CTaskComplexFleeEntity()
{
	if(m_entity)
		m_entity->CleanUpOldReference(&m_entity);
}

// 0x65BA30
CTask*
CTaskComplexFleeEntity::CreateSubTask(eTaskType taskType)
{
	switch(taskType){
	case TASK_COMPLEX_FLEE_POINT: {
		m_posCheckTimer.Start(m_posCheckPeriod);
		m_pos = m_entity->GetPosition();
		return new CTaskComplexFleePoint(m_pos, m_pedScream, m_safeDistance, m_time);
	}
	case TASK_FINISHED:
		return nil;
	default:
		return nil;
	}
}

// 0x65BAC0
CTask*
CTaskComplexFleeEntity::CreateNextSubTask(CPed *ped)
{
	(void)ped;
	if(m_entity && (m_pSubTask == nil || m_pSubTask->GetTaskType() != TASK_COMPLEX_FLEE_POINT))
		return nil;
	return CreateSubTask(TASK_FINISHED);
}

// 0x65BB00
CTask*
CTaskComplexFleeEntity::CreateFirstSubTask(CPed *ped)
{
	(void)ped;
	if(!m_entity)
		return nil;
	return CreateSubTask(TASK_COMPLEX_FLEE_POINT);
}

// 0x65BBB0: keep the inner flee aimed at the entity's current position;
// if it moved enough since the last check, restart the flee at it. (SA
// also posts CEventSeenPanickedPed to the global group here; that event
// is not ported yet.)
CTask*
CTaskComplexFleeEntity::ControlSubTask(CPed *ped)
{
	if(m_entity){
		if(m_pSubTask && m_pSubTask->GetTaskType() == TASK_COMPLEX_FLEE_POINT){
			if(m_posCheckTimer.IsOutOfTime()){
				m_posCheckTimer.Start(m_posCheckPeriod);
				CVector newPos = m_entity->GetPosition();
				CVector dist = newPos - m_pos;
				if(dist.MagnitudeSqr() > m_posChangeTolerance * m_posChangeTolerance){
					m_pos = newPos;
					((CTaskComplexFleePoint*)m_pSubTask)->SetFleePosition(newPos, m_safeDistance, m_pedScream);

					// SA: a fleeing ped spreads panic to bystanders
					CEventSeenPanickedPed seenPanicked(ped);
					GetEventGlobalGroup()->Add(&seenPanicked, false);
				}
			}
		}
	}else{
		m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_LEISURE, nil);
	}
	return m_pSubTask;
}
