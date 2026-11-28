#include "common.h"

#include "EventGroup.h"
#include "EventEditableResponse.h"
#include "EventScriptCommand.h"
#include "Ped.h"
#include "PedIntelligence.h"

// SA CEventGroup::CEventGroup (0x4AB330).
CEventGroup::CEventGroup(CPed *ped)
	: m_pPed(ped)
{
	m_count = 0;
	for(int i = 0; i < TOTAL_EVENTS_PER_EVENTGROUP; i++)
		m_events[i] = nil;
}

// SA CEventGroup::~CEventGroup (0x4AB360).
CEventGroup::~CEventGroup()
{
	Flush();
}

// SA CEventGroup::Add (0x4AB420): an event earns its place in the queue by
// first passing through the decision maker (for editable-response events):
// the response is computed here, at add time, and an event nobody would
// respond to - or that does not affect this ped - never enters the queue.
// What enters is a clone; the caller keeps ownership of the original.
//
// The stealth-damage refinement (no respected-friends inform for stealth
// hits) lands with CEventDamage.
CEvent*
CEventGroup::Add(CEvent *event, bool bValid)
{
	if(m_pPed){
		if(event->HasEditableResponse()){
			CEventEditableResponse *eventResp = (CEventEditableResponse*)event;

			const bool bIsEventDamage          = event->GetEventType() == EVENT_DAMAGE;
			const bool bInformGroup            = eventResp->ComputeResponseTaskOfType(m_pPed, TASK_SIMPLE_INFORM_GROUP);
			const bool bInformRespectedFriends = eventResp->ComputeResponseTaskOfType(m_pPed, TASK_SIMPLE_INFORM_RESPECTED_FRIENDS);
			const bool bTriggerLookAt          = eventResp->ComputeResponseTaskOfType(m_pPed, TASK_SIMPLE_LOOK_AT_ENTITY_OR_COORD);

			eventResp->ComputeResponseTaskType(m_pPed, false);

			const bool bAddToEventGroup = eventResp->WillRespond() || (bIsEventDamage && eventResp->m_bAddToEventGroup);

			if(!event->AffectsPed(m_pPed))
				return nil;
			if(bInformGroup)
				eventResp->InformGroup(m_pPed);
			if(bInformRespectedFriends)
				eventResp->InformRespectedFriends(m_pPed);
			if(bTriggerLookAt)
				eventResp->TriggerLookAt(m_pPed);
			eventResp->InformVehicleOccupants(m_pPed);
			if(!bAddToEventGroup)
				return nil;
		}else{
			if(!event->AffectsPed(m_pPed))
				return nil;
		}
		if(m_pPed->m_pIntelligence)
			m_pPed->m_pIntelligence->RecordEventForScript(event->GetEventType(), event->GetEventPriority());
	}

	if(m_count >= TOTAL_EVENTS_PER_EVENTGROUP)
		return nil;

	CEvent *clonedEvent = event->Clone();
	clonedEvent->m_bValid = bValid;
	if(m_pPed)
		clonedEvent->ReportCriminalEvent(m_pPed);
	m_events[m_count++] = clonedEvent;

	return clonedEvent;
}

// SA CEventGroup::HasScriptCommandOfTaskType (0x4AB840).
bool
CEventGroup::HasScriptCommandOfTaskType(eTaskType taskId)
{
	for(int i = 0; i < m_count; i++){
		CEvent *event = m_events[i];
		if(event == nil || event->GetEventType() != EVENT_SCRIPT_COMMAND)
			continue;
		CEventScriptCommand *scriptCmd = (CEventScriptCommand*)event;
		if(scriptCmd->m_task && scriptCmd->m_task->GetTaskType() == taskId)
			return true;
	}
	return false;
}

// SA CEventGroup::HasEventOfType (0x4AB5E0).
bool
CEventGroup::HasEventOfType(CEvent *event) const
{
	return GetEventOfType(event->GetEventType()) != nil;
}

// SA CEventGroup::GetHighestPriorityEvent (0x4AB7C0). Script commands only
// displace a strictly higher priority; everything else wins ties, so the
// newest event of equal weight is the one answered.
CEvent*
CEventGroup::GetHighestPriorityEvent()
{
	CEvent *theEvent = nil;
	int32 highestPriority = -1;
	for(int i = 0; i < m_count; i++){
		CEvent *event = m_events[i];
		if(event == nil)
			continue;
		int32 eventPriority = event->GetEventPriority();
		bool bIsPriorityGreater = false;
		if(event->GetEventType() == EVENT_SCRIPT_COMMAND){
			if(eventPriority > highestPriority)
				bIsPriorityGreater = true;
		}else if(eventPriority >= highestPriority){
			bIsPriorityGreater = true;
		}
		if(bIsPriorityGreater && event->AffectsPed(m_pPed)){
			highestPriority = eventPriority;
			theEvent = event;
		}
	}
	return theEvent;
}

// SA CEventGroup::TickEvents (0x4AB6D0).
void
CEventGroup::TickEvents()
{
	for(int i = 0; i < m_count; i++)
		if(m_events[i])
			m_events[i]->Tick();
}

// SA CEventGroup::HasEvent (0x4AB6A0).
bool
CEventGroup::HasEvent(CEvent *event)
{
	for(int i = 0; i < m_count; i++)
		if(m_events[i] == event)
			return true;
	return false;
}

// SA CEventGroup::Remove (0x4AB5A0): leaves a hole; Reorganise compacts.
void
CEventGroup::Remove(CEvent *event)
{
	for(int i = 0; i < m_count; i++){
		if(m_events[i] == event){
			delete m_events[i];
			m_events[i] = nil;
			return;
		}
	}
}

// SA CEventGroup::RemoveInvalidEvents (0x4AB760).
void
CEventGroup::RemoveInvalidEvents(bool bRemoveNonScriptCommandEvents)
{
	for(int i = 0; i < m_count; i++){
		CEvent *event = m_events[i];
		if(event == nil)
			continue;
		if(event->IsValid(m_pPed)){
			if(!bRemoveNonScriptCommandEvents || event->GetEventType() == EVENT_SCRIPT_COMMAND)
				continue;
		}
		delete m_events[i];
		m_events[i] = nil;
	}
}

// SA CEventGroup::Reorganise (0x4AB700): compact the queue and re-count.
void
CEventGroup::Reorganise()
{
	CEvent *old[TOTAL_EVENTS_PER_EVENTGROUP];
	int32 cnt = m_count;
	for(int i = 0; i < TOTAL_EVENTS_PER_EVENTGROUP; i++)
		old[i] = m_events[i];
	m_count = 0;
	for(int32 i = 0; i < cnt; i++){
		if(old[i])
			m_events[m_count++] = old[i];
	}
}

// SA CEventGroup::Flush (0x4AB370). With bAvoidFlushingTaskComplexBeInGroup
// (SA's keep-join-group flag), a script command carrying
// TASK_COMPLEX_BE_IN_GROUP survives the flush: group membership must not be
// dropped by an intelligence reset.
void
CEventGroup::Flush(bool bAvoidFlushingTaskComplexBeInGroup)
{
	CEvent *scriptCmdBeInGroup = nil;
	if(bAvoidFlushingTaskComplexBeInGroup){
		for(int i = 0; i < m_count; i++){
			CEvent *event = m_events[i];
			if(event == nil || event->GetEventType() != EVENT_SCRIPT_COMMAND)
				continue;
			CEventScriptCommand *scriptCmd = (CEventScriptCommand*)event;
			if(scriptCmd->m_task && scriptCmd->m_task->GetTaskType() == TASK_COMPLEX_BE_IN_GROUP){
				scriptCmdBeInGroup = event;
				m_events[i] = nil;
				break;
			}
		}
	}

	for(int i = 0; i < m_count; i++){
		delete m_events[i];
		m_events[i] = nil;
	}

	m_count = 0;
	if(scriptCmdBeInGroup)
		m_events[m_count++] = scriptCmdBeInGroup;
}

// SA CEventGroup::GetEventOfType (0x4AB650).
CEvent*
CEventGroup::GetEventOfType(eEventType type) const
{
	for(int i = 0; i < m_count; i++)
		if(m_events[i] && m_events[i]->GetEventType() == type)
			return m_events[i];
	return nil;
}
