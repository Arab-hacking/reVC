#include "common.h"

#include "EventHandler.h"
#include "EventDamage.h"
#include "EventGroup.h"
#include "EventGunShot.h"
#include "EventGunShotWhizzedBy.h"
#include "EventPedToChase.h"
#include "EventPedToFlee.h"
#include "EventScriptCommand.h"
#include "EventSeenPanickedPed.h"
#include "General.h"
#include "Ped.h"
#include "PedIntelligence.h"
#include "TaskComplexFleeEntity.h"
#include "TaskComplexSeekEntity.h"
#include "TaskComplexSmartFleeEntity.h"
#include "TaskComplexTurnToFaceEntityOrCoord.h"
#include "TaskManager.h"
#include "TaskSimpleDuck.h"

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
	case EVENT_SEEN_PANICKED_PED:
		ComputeSeenPanickedPedResponse((CEventSeenPanickedPed*)event, activeTask, simplestTask);
		break;
	case EVENT_SHOT_FIRED:
		ComputeShotFiredResponse((CEventGunShot*)event, activeTask, simplestTask);
		break;
	case EVENT_SHOT_FIRED_WHIZZED_BY:
		ComputeShotFiredWhizzedByResponse((CEventGunShotWhizzedBy*)event, activeTask, simplestTask);
		break;
	default:
		(void)abortedTaskEventResponse;
		break;
	}
}

// SA CEventHandler::ComputeDamageResponse (0x4C0170), subset: the
// physical-response half of SA's version (CTaskComplexDie,
// CTaskSimpleBeHit, CTaskComplexFallAndGetUp, CTaskSimpleChoking, the
// bike knock-off) is not ported yet - and locally CPed::InflictDamage
// has already applied the damage, death included, before the event is
// answered. What remains, and what is ported here, is SA's flow down to
// the personality response: group-less damage goes straight there,
// body-part removal runs for grouped damage, and a killing blow leaves
// nothing to respond with.
void
CEventHandler::ComputeDamageResponse(CEventDamage *e, CTask *tactive, CTask *tsimplest, CTask *abortedTaskEventResponse)
{
	(void)tactive; (void)tsimplest; (void)abortedTaskEventResponse;

	if(!e->m_bAddToEventGroup){
		ComputeDamagePersonalityResponse(e);
		return;
	}

	e->ProcessDamage(m_Ped);

	if(e->HasKilledPed() || m_Ped->m_fHealth < 1.0f){
		m_EventResponseTask = nil;
		return;
	}

	if(!e->m_pSourceEntity){
		m_EventResponseTask = nil;
		return;
	}

	ComputeDamagePersonalityResponse(e);
}

// SA's DoComputeDamageAndResponseForPersonality inside
// ComputeDamageResponse: answer the damager according to the ped's
// personality (the decision maker's chosen task), unless the ped is
// already answering this same damage or is already reacting to an
// attack.
void
CEventHandler::ComputeDamagePersonalityResponse(CEventDamage *e)
{
	if(m_Ped->IsPlayer())
		return;

	CEntity *srcEntity = e->GetSourceEntity();
	if(srcEntity == nil || !srcEntity->IsPed())
		return;

	CEvent *currentEvent = m_History.GetCurrentEvent();
	if(currentEvent && currentEvent->GetEventType() == EVENT_DAMAGE
	&& e->IsSameEventForAI(*(CEventDamage*)currentEvent))
		return;

	CTaskManager &taskMgr = m_Ped->m_pIntelligence->GetTaskManager();
	if(taskMgr.FindTaskOfType(TASK_COMPLEX_REACT_TO_ATTACK, true))
		return;

	ComputePersonalityResponseToDamage(e, (CPed*)srcEntity);
}

// SA CEventHandler::ComputePersonalityResponseToDamage (0x4BFB60),
// subset: the cases whose tasks are not ported yet (the kill tasks,
// flee-any-means, the car-mission tasks, scripted attractors) fall back
// to smart flee - SA's own fallback for the kill cases - and the speech
// lines are skipped (the SA speech contexts are not ported yet).
void
CEventHandler::ComputePersonalityResponseToDamage(CEventDamage *e, CPed *src)
{
	m_EventResponseTask = nil;

	if(src == nil)
		return;

	switch(e->GetTaskId()){
	case TASK_COMPLEX_KILL_CRIMINAL:
		if(src->IsPlayer())
			break;
		// CTaskComplexKillCriminal is not ported yet; SA flees when a
		// kill task is not appropriate, and that is the branch we have.
		m_EventResponseTask = new CTaskComplexSmartFleeEntity(src, false, 60.0f);
		break;
	case TASK_COMPLEX_KILL_PED_ON_FOOT:
	case TASK_COMPLEX_FLEE_ANY_MEANS:
	case TASK_COMPLEX_SMART_FLEE_ENTITY:
	case TASK_COMPLEX_FLEE_ENTITY:
		// The kill and flee-any-means tasks are not ported yet; smart
		// flee is SA's fallback for the kill case and the direct answer
		// for the flee cases.
		m_EventResponseTask = new CTaskComplexSmartFleeEntity(src, false, 60.0f);
		break;
	case TASK_SIMPLE_DUCK_FOREVER:
		m_EventResponseTask = new CTaskSimpleDuck(DUCK_STANDALONE, 57599, -1);
		break;
	case TASK_SIMPLE_DUCK:
		if(CTaskSimpleDuck *duckTask = m_Ped->GetIntelligence()->GetTaskDuck())
			m_EventResponseTask = duckTask;
		else
			m_EventResponseTask = new CTaskSimpleDuck(DUCK_STANDALONE, (uint16)CGeneral::GetRandomNumberInRange(2000, 5000));
		break;
	case TASK_COMPLEX_KILL_PED_ON_FOOT_STEALTH: // task not ported yet
	case TASK_SIMPLE_BE_DAMAGED:
	case TASK_NONE:
	default:
		break;
	}
}

// SA CEventHandler::ComputeSeenPanickedPedResponse (0x4C35F0): duck, or
// run from whatever the current event is about - SA reads the source
// from this ped's own event history (the panicked ped's AffectsPed
// already made sure there is one in the picture).
void
CEventHandler::ComputeSeenPanickedPedResponse(CEventSeenPanickedPed *e, CTask *tactive, CTask *tsimplest)
{
	(void)tactive; (void)tsimplest;
	m_EventResponseTask = nil;

	if(!e->GetSourceEntity())
		return;
	CEvent *currEvent = m_History.GetCurrentEvent();
	if(!currEvent)
		return;
	CEntity *currEventSrc = currEvent->GetSourceEntity();
	if(!currEventSrc)
		return;

	switch(e->GetTaskId()){
	case TASK_SIMPLE_DUCK_FOREVER:
		m_EventResponseTask = new CTaskSimpleDuck(DUCK_STANDALONE, 57599, -1);
		break;
	case TASK_COMPLEX_FLEE_ENTITY:
		m_EventResponseTask = new CTaskComplexFleeEntity(currEventSrc, true, 45.0f);
		break;
	case TASK_COMPLEX_SMART_FLEE_ENTITY:
		m_EventResponseTask = new CTaskComplexSmartFleeEntity(currEventSrc, false, 45.0f);
		break;
	default:
		break;
	}
}

// SA CEventHandler::ComputeShotFiredResponse (0x4BC710), subset: the
// kill tasks (KillPedOnFoot/KillCriminal/stealth) and the car-mission
// flee are not ported yet, so those answers fall back to smart flee -
// SA's own fallback when a kill task is not appropriate.
void
CEventHandler::ComputeShotFiredResponse(CEventGunShot *e, CTask *tactive, CTask *tsimplest)
{
	(void)tactive; (void)tsimplest;
	m_EventResponseTask = nil;

	if(!e->m_firedBy)
		return;
	CPed *firedByPed = e->m_firedBy->IsPed() ? (CPed*)e->m_firedBy : nil;

	switch(e->GetTaskId()){
	case TASK_COMPLEX_TURN_TO_FACE_ENTITY:
		m_EventResponseTask = new CTaskComplexTurnToFaceEntityOrCoord(e->m_firedBy, 0.5f);
		break;
	case TASK_COMPLEX_KILL_PED_ON_FOOT:
		if(!firedByPed || firedByPed->m_nPedType == m_Ped->m_nPedType)
			break;
		m_EventResponseTask = new CTaskComplexSmartFleeEntity(e->m_firedBy, false, 60.0f);
		break;
	case TASK_COMPLEX_KILL_CRIMINAL:
		if(!firedByPed || firedByPed->m_nPedType == m_Ped->m_nPedType || firedByPed->IsPlayer())
			break;
		m_EventResponseTask = new CTaskComplexSmartFleeEntity(e->m_firedBy, false, 60.0f);
		break;
	case TASK_COMPLEX_SMART_FLEE_ENTITY:
		m_EventResponseTask = new CTaskComplexSmartFleeEntity(e->m_firedBy, true, 60.0f);
		break;
	case TASK_COMPLEX_CAR_DRIVE_MISSION_FLEE_SCENE: // task not ported yet
		m_EventResponseTask = new CTaskComplexSmartFleeEntity(e->m_firedBy, false, 60.0f);
		break;
	case TASK_SIMPLE_DUCK_FOREVER:
		m_EventResponseTask = new CTaskSimpleDuck(DUCK_STANDALONE, 0x967F, -1);
		break;
	case TASK_SIMPLE_DUCK:
		if(m_Ped->GetIntelligence()->GetTaskDuck(true))
			m_EventResponseTask = new CTaskSimpleDuck(DUCK_STANDALONE, 3000, -1);
		break;
	case TASK_COMPLEX_KILL_PED_ON_FOOT_STEALTH: // task not ported yet
	case TASK_NONE:
	default:
		break;
	}
}

// SA CEventHandler::ComputeShotFiredWhizzedByResponse (0x4BBE30): a
// shot whizzed by - get (or stay) ducked while they keep coming.
void
CEventHandler::ComputeShotFiredWhizzedByResponse(CEventGunShotWhizzedBy *e, CTask *tactive, CTask *tsimplest)
{
	(void)tactive;
	m_EventResponseTask = nil;

	if(!e->GetSourceEntity())
		return;

	switch(e->GetTaskId()){
	case TASK_SIMPLE_DUCK_WHILE_SHOTS_WHIZZING: {
		if(tsimplest && tsimplest->GetTaskType() == TASK_SIMPLE_DUCK){
			CTaskSimpleDuck *duckTask = (CTaskSimpleDuck*)tsimplest;
			if(duckTask->m_ShotWhizzingCounter >= 0)
				duckTask->RestartTask(m_Ped);
			break;
		}
		if(CTaskSimpleDuck *duckTask = m_Ped->GetIntelligence()->GetTaskDuck()){
			if(duckTask->m_ShotWhizzingCounter >= 0)
				duckTask->RestartTask(m_Ped);
			break;
		}
		m_EventResponseTask = new CTaskSimpleDuck(DUCK_STANDALONE_WEAPON_CROUCH, (uint16)CGeneral::GetRandomNumberInRange(3000, 5000), 1000);
		break;
	}
	case TASK_NONE:
	default:
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

// SA CEventHandler::ComputePedToChaseResponse (0x4C1910).
void
CEventHandler::ComputePedToChaseResponse(CEventPedToChase *event, CTask *activeTask, CTask *simplestTask)
{
	(void)activeTask; (void)simplestTask;

	if(event->GetPedToChase() == nil){
		m_EventResponseTask = nil;
		return;
	}
	m_EventResponseTask = new CTaskComplexSeekEntity(event->GetPedToChase(), 30000, 1000, 1.0f, 2.0f, 2.0f, true, true);
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
