#include "common.h"

#include "EventHandler.h"
#include "EventGroup.h"
#include "EventPedToChase.h"
#include "EventPedToFlee.h"
#include "EventScriptCommand.h"
#include "Ped.h"
#include "PedIntelligence.h"
#include "TaskComplexSmartFleeEntity.h"
#include "TaskManager.h"

// SA CEventHandler::CEventHandler (0x4C3EE0).
CEventHandler::CEventHandler(CPed *ped)
	: m_Ped(ped)
{
	m_PhysicalResponseTask = nil;
	m_EventResponseTask = nil;
	m_AttackTask = nil;
	m_SayTask = nil;
	m_PartialAnimTask = nil;
}

// SA CEventHandler::IsTemporaryEvent (0x4BC370): collisions and near-misses
// interrupt what a ped is doing without replacing it - the standing
// response is parked and resumed afterwards.
bool
CEventHandler::IsTemporaryEvent(const CEvent &event)
{
	switch(event.GetEventType()){
	case EVENT_VEHICLE_COLLISION:
	case EVENT_PED_COLLISION_WITH_PED:
	case EVENT_PED_COLLISION_WITH_PLAYER:
	case EVENT_PLAYER_COLLISION_WITH_PED:
	case EVENT_OBJECT_COLLISION:
	case EVENT_BUILDING_COLLISION:
	case EVENT_IN_AIR:
	case EVENT_POTENTIAL_WALK_INTO_VEHICLE:
	case EVENT_POTENTIAL_WALK_INTO_BUILDING:
	case EVENT_POTENTIAL_WALK_INTO_OBJECT:
	case EVENT_POTENTIAL_WALK_INTO_PED:
	case EVENT_POTENTIAL_WALK_INTO_FIRE:
	case EVENT_AREA_CODES:
	case EVENT_STUCK_IN_AIR:
	case EVENT_POTENTIAL_GET_RUN_OVER:
	case EVENT_ON_ESCALATOR:
	case EVENT_FIRE_NEARBY:
		return true;
	default:
		return false;
	}
}

// SA CEventHandler::ResetResponse: forget the computed response tasks
// (ownership has passed to the task manager by the time this runs).
void
CEventHandler::ResetResponse()
{
	m_PhysicalResponseTask = nil;
	m_EventResponseTask = nil;
	m_AttackTask = nil;
	m_SayTask = nil;
	m_PartialAnimTask = nil;
}

// SA CEventHandler::Flush (0x4C3790).
void
CEventHandler::Flush()
{
	delete m_PhysicalResponseTask;
	m_PhysicalResponseTask = nil;

	delete m_EventResponseTask;
	m_EventResponseTask = nil;

	delete m_AttackTask;
	m_AttackTask = nil;

	delete m_SayTask;
	m_SayTask = nil;

	delete m_PartialAnimTask;
	m_PartialAnimTask = nil;

	m_History.Flush();
}

// SA CEventHandler::FlushImmediately (0x4C3820).
void
CEventHandler::FlushImmediately()
{
	m_History.Flush();
}

// SA CEventHandler::GetCurrentEventType (0x4B8CC0).
eEventType
CEventHandler::GetCurrentEventType() const
{
	CEvent *event = m_History.GetCurrentEvent();
	return event ? event->GetEventType() : EVENT_NONE;
}

// SA CEventHandler::HandleEvents (0x4C3F10): pick the group's winning
// event, make sure it outranks what the ped is already answering, stop
// the running task's timers if it can be aborted, then compute and
// install the response. Events whose classes are not ported yet fall
// through ComputeEventResponseTask without a response task and are
// consumed the same way SA consumes a null response.
void
CEventHandler::HandleEvents()
{
	m_History.TickStoredEvent(m_Ped);

	if(m_Ped == nil || m_Ped->m_pIntelligence == nil)
		return;

	CTask *lastAbortedTask = m_History.GetAbortedTask();
	CPedIntelligence *intelligence = m_Ped->m_pIntelligence;
	CTaskManager &taskMgr = intelligence->GetTaskManager();
	CTask *primaryTask = intelligence->GetActivePrimaryTask();
	CTask *tempEventRespTask = taskMgr.GetTemporaryEventResponseTask();
	CTask *persistentEventRespTask = taskMgr.GetPresistentEventResponseTask();

	CEventGroup &group = intelligence->GetEventGroup();
	CEvent *priorityEvent = group.GetHighestPriorityEvent();

	group.TickEvents();

	if(lastAbortedTask == nil)
		m_History.RecordAbortedTask(primaryTask && lastAbortedTask == primaryTask ? primaryTask : nil);

	if(tempEventRespTask == nil){
		if(persistentEventRespTask == nil)
			m_History.RecordAbortedTask(nil);
		m_History.ClearTempEvent();
	}

	if(persistentEventRespTask == nil)
		m_History.ClearNonTempEvent();

	if(priorityEvent){
		if(priorityEvent->GetEventType() == EVENT_DAMAGE)
			m_Ped->SetIsStatic(false);

		CEvent *currEvent = m_History.GetCurrentEvent();
		if(currEvent && currEvent->GetEventType() == priorityEvent->GetEventType()){
			group.Remove(priorityEvent);
			group.RemoveInvalidEvents(false);
			group.Reorganise();
			if(primaryTask == nil){
				m_History.RecordAbortedTask(nil);
				m_History.ClearAllEvents();
			}
			return;
		}

		if(m_History.TakesPriorityOverCurrentEvent(*priorityEvent)){
			CTask *activeTask = taskMgr.GetActiveTask();

			bool hasStoppedTimers = false;
			if(activeTask == nil || activeTask->MakeAbortable(m_Ped, ABORT_PRIORITY_URGENT, priorityEvent)){
				taskMgr.StopTimers(priorityEvent);
				hasStoppedTimers = true;
			}

			if(primaryTask){
				if(!hasStoppedTimers){
					priorityEvent->UnTick();
					activeTask->MakeAbortable(m_Ped, ABORT_PRIORITY_LEISURE, priorityEvent);
					m_History.RecordAbortedTask(activeTask);
					group.RemoveInvalidEvents(false);
					group.Reorganise();
					return;
				}
				m_History.RecordAbortedTask(nil);
			}else if(lastAbortedTask){
				m_History.RecordAbortedTask(nil);
			}else if(!hasStoppedTimers){
				priorityEvent->UnTick();
				group.RemoveInvalidEvents(false);
				group.Reorganise();
				return;
			}

			ComputeEventResponseTask(priorityEvent, lastAbortedTask);
			if(m_EventResponseTask)
				m_History.RecordCurrentEvent(m_Ped, *priorityEvent);
			SetEventResponseTask(*priorityEvent);
			group.Remove(priorityEvent);
		}

		group.RemoveInvalidEvents(false);
		group.Reorganise();
	}else if(primaryTask == nil){
		m_History.RecordAbortedTask(nil);
		m_History.ClearAllEvents();
	}
}

// SA CEventHandler::SetEventResponseTask (0x4BC600): install whatever
// the Compute*Response builders produced - physical response first,
// then the event response (temporary events park the standing one),
// then the secondary attack/say/partial-anim tasks. (The facial
// expression hand-off needs the facial task, not ported yet.)
void
CEventHandler::SetEventResponseTask(const CEvent &event)
{
	CPedIntelligence *intelligence = m_Ped->m_pIntelligence;
	CTaskManager &taskMgr = intelligence->GetTaskManager();

	if(m_PhysicalResponseTask)
		intelligence->AddTaskPhysResponse(m_PhysicalResponseTask, 1);

	if(m_EventResponseTask){
		if(IsTemporaryEvent(event)){
			intelligence->AddTaskEventResponseTemp(m_EventResponseTask, 0);
		}else{
			intelligence->AddTaskEventResponseTemp(nil, 0);
			intelligence->AddTaskEventResponseNonTemp(m_EventResponseTask, 0);
		}
	}

	if(m_AttackTask){
		CTask *attack = taskMgr.GetTaskSecondary(TASK_SECONDARY_ATTACK);
		if(attack)
			attack->MakeAbortable(m_Ped);
		taskMgr.SetTaskSecondary(m_AttackTask, TASK_SECONDARY_ATTACK);
	}

	if(m_SayTask)
		taskMgr.SetTaskSecondary(m_SayTask, TASK_SECONDARY_SAY);

	if(m_PartialAnimTask)
		taskMgr.SetTaskSecondary(m_PartialAnimTask, TASK_SECONDARY_PARTIAL_ANIM);

	ResetResponse();
}

// SA CEventHandler::ComputeEventResponseTask. Only the events whose
// classes exist in this codebase dispatch anywhere yet; the rest are
// answered with silence exactly like a null SA response.
void
CEventHandler::ComputeEventResponseTask(CEvent *event, CTask *abortedTaskEventResponse)
{
	m_PhysicalResponseTask = nil;
	m_EventResponseTask = nil;
	m_AttackTask = nil;
	m_SayTask = nil;
	m_PartialAnimTask = nil;

	CTaskManager &taskMgr = m_Ped->m_pIntelligence->GetTaskManager();
	CTask *activeTask = taskMgr.GetActiveTask();
	CTask *simplestTask = activeTask ? taskMgr.GetSimplestActiveTask() : nil;

	switch(event->GetEventType()){
	case EVENT_SCRIPT_COMMAND:
		ComputeScriptCommandResponse((CEventScriptCommand*)event, activeTask, simplestTask);
		break;
	case EVENT_PED_TO_CHASE:
		ComputePedToChaseResponse((CEventPedToChase*)event, activeTask, simplestTask);
		break;
	case EVENT_PED_TO_FLEE:
		ComputePedToFleeResponse((CEventPedToFlee*)event, activeTask, simplestTask);
		break;
	default:
		(void)abortedTaskEventResponse;
		break;
	}
}

// SA CEventHandler::ComputeScriptCommandResponse: install the carried
// task at the primary slot it asks for. If the task currently there
// refuses to abort, the command is re-queued and tried again later.
// (The CPedScriptedTaskRecord association is not ported yet.)
void
CEventHandler::ComputeScriptCommandResponse(CEventScriptCommand *event, CTask *activeTask, CTask *simplestTask)
{
	(void)activeTask; (void)simplestTask;

	CTaskManager &taskMgr = m_Ped->m_pIntelligence->GetTaskManager();
	ePrimaryTasks taskIndex = event->m_primaryTaskIndex == TASK_PRIMARY_PRIMARY
		? TASK_PRIMARY_PRIMARY : TASK_PRIMARY_DEFAULT;
	CTask *task = taskMgr.GetTaskPrimary(taskIndex);
	if(task && !task->MakeAbortable(m_Ped, ABORT_PRIORITY_URGENT, event)){
		task->MakeAbortable(m_Ped, ABORT_PRIORITY_LEISURE, event);
		m_Ped->m_pIntelligence->GetEventGroup().Add(event);
	}else{
		CTask *clonedTask = event->CloneScriptTask();
		taskMgr.ClearTaskEventResponse();
		taskMgr.SetTask(clonedTask, taskIndex);
	}
}

// SA CEventHandler::ComputePedToChaseResponse (0x4C1910). SA answers
// with CTaskComplexSeekEntity; the seek task chain is not ported yet,
// so a spotted criminal is noted by the history but not yet chased.
void
CEventHandler::ComputePedToChaseResponse(CEventPedToChase *event, CTask *activeTask, CTask *simplestTask)
{
	(void)event; (void)activeTask; (void)simplestTask;
	m_EventResponseTask = nil;
}

// SA CEventHandler::ComputePedToFleeResponse (0x4B9B50). (SA also makes
// the ped plead - CTX_GLOBAL_CRIMINAL_PLEAD is not in this codebase's
// speech-context set yet.)
void
CEventHandler::ComputePedToFleeResponse(CEventPedToFlee *event, CTask *activeTask, CTask *simplestTask)
{
	(void)activeTask; (void)simplestTask;

	if(event->GetPedToFlee() == nil){
		m_EventResponseTask = nil;
		return;
	}
	m_EventResponseTask = new CTaskComplexSmartFleeEntity(event->GetPedToFlee(), true, 100.0f, -1);
}
