#include "common.h"

#include "EventHandlerHistory.h"
#include "EventHandler.h"
#include "Event.h"

// SA CEventHandlerHistory::~CEventHandlerHistory (0x4BCA00).
CEventHandlerHistory::~CEventHandlerHistory()
{
	Flush();
}

// SA CEventHandlerHistory::ClearAllEvents (0x4BCA40).
void
CEventHandlerHistory::ClearAllEvents()
{
	ClearTempEvent();
	ClearNonTempEvent();
}

// SA CEventHandlerHistory::ClearNonTempEvent (0x4BCA70).
void
CEventHandlerHistory::ClearNonTempEvent()
{
	delete m_CurrentNonTempEvent;
	m_CurrentNonTempEvent = nil;
}

// SA CEventHandlerHistory::ClearTempEvent (0x4BCA90).
void
CEventHandlerHistory::ClearTempEvent()
{
	delete m_CurrentTempEvent;
	m_CurrentTempEvent = nil;
}

// SA CEventHandlerHistory::ClearStoredActiveEvent (0x4BCAB0).
void
CEventHandlerHistory::ClearStoredActiveEvent()
{
	delete m_StoredEventActive;
	m_StoredEventActive = nil;
}

// SA CEventHandlerHistory::Flush (0x4BCAD0).
void
CEventHandlerHistory::Flush()
{
	ClearAllEvents();
	ClearStoredActiveEvent();
}

// SA CEventHandlerHistory::GetCurrentEventPriority (0x4BCAF0).
int32
CEventHandlerHistory::GetCurrentEventPriority() const
{
	if(m_CurrentTempEvent)
		return m_CurrentTempEvent->GetEventPriority();
	if(m_CurrentNonTempEvent)
		return m_CurrentNonTempEvent->GetEventPriority();
	return -1;
}

// SA CEventHandlerHistory::IsRespondingToEvent (0x4BCB10). EVENT_NONE asks
// "responding to anything that sticks".
bool
CEventHandlerHistory::IsRespondingToEvent(eEventType eventType) const
{
	if(eventType == EVENT_NONE)
		return m_CurrentNonTempEvent != nil || m_StoredEventActive != nil;

	bool isNonTempEvent = false;
	if(m_CurrentNonTempEvent && m_CurrentNonTempEvent->GetEventType() == eventType)
		isNonTempEvent = true;

	if(m_StoredEventActive == nil || m_StoredEventActive->GetEventType() != eventType)
		return isNonTempEvent;

	return true;
}

// SA CEventHandlerHistory::RecordCurrentEvent (0x4BCB60): remember what the
// ped is answering. A temporary event parks the standing response for two
// seconds rather than discarding it; a non-temporary one replaces it.
void
CEventHandlerHistory::RecordCurrentEvent(CPed *ped, CEvent &event)
{
	(void)ped;
	if(event.GetEventType() == EVENT_SCRIPT_COMMAND)
		return;
	if(CEventHandler::IsTemporaryEvent(event)){
		if(m_CurrentNonTempEvent){
			StoreActiveEvent();
			m_CurrentNonTempEvent = nil;
		}
		ClearTempEvent();
		m_CurrentTempEvent = event.Clone();
	}else{
		ClearNonTempEvent();
		m_CurrentNonTempEvent = event.Clone();
		ClearStoredActiveEvent();
		ClearTempEvent();
	}
}

// SA CEventHandlerHistory::StoreActiveEvent (0x4BCC00).
void
CEventHandlerHistory::StoreActiveEvent()
{
	delete m_StoredEventActive;
	m_StoredEventActive = m_CurrentNonTempEvent;
	m_StoreTimer.Start(2000);
}

// SA CEventHandlerHistory::TakesPriorityOverCurrentEvent (0x4BCC30).
bool
CEventHandlerHistory::TakesPriorityOverCurrentEvent(CEvent &event) const
{
	if(m_CurrentNonTempEvent)
		return event.TakesPriorityOver(*m_CurrentNonTempEvent);
	if(m_CurrentTempEvent == nil)
		return true;
	if(CEventHandler::IsTemporaryEvent(event))
		return event.TakesPriorityOver(*m_CurrentTempEvent);
	if(!event.TakesPriorityOver(*m_CurrentTempEvent))
		return false;
	if(m_StoredEventActive == nil || event.TakesPriorityOver(*m_StoredEventActive))
		return true;
	return false;
}

// SA CEventHandlerHistory::TickStoredEvent (0x4BCC90): once the temporary
// interruption is over, the parked response becomes current again.
void
CEventHandlerHistory::TickStoredEvent(CPed *ped)
{
	(void)ped;
	if(m_StoredEventActive && m_CurrentTempEvent == nil){
		m_CurrentNonTempEvent = m_StoredEventActive;
		m_StoredEventActive = nil;
	}
}
