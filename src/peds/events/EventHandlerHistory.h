#pragma once

#include "common.h"
#include "TaskTimer.h"
#include "eEventType.h"

class CEvent;
class CTask;
class CPed;

// SA CEventHandlerHistory: remembers which events the ped is currently
// responding to (a temporary one and a persistent one) and the event it
// responded to last, so new events have to earn their interruption.
class CEventHandlerHistory
{
public:
	CEventHandlerHistory() : m_AbortedTask(nil), m_CurrentNonTempEvent(nil), m_CurrentTempEvent(nil), m_StoredEventActive(nil) {}
	~CEventHandlerHistory();

	void ClearAllEvents();
	void ClearNonTempEvent();
	void ClearTempEvent();
	void ClearStoredActiveEvent();
	void Flush();
	CEvent *GetCurrentEvent() const { return m_CurrentTempEvent ? m_CurrentTempEvent : m_CurrentNonTempEvent; }
	int32 GetCurrentEventPriority() const;
	CEvent *GetStoredActiveEvent() const { return m_StoredEventActive; }
	bool IsRespondingToEvent(eEventType eventType) const;
	void RecordCurrentEvent(CPed *ped, CEvent &event);
	void StoreActiveEvent();
	bool TakesPriorityOverCurrentEvent(CEvent &event) const;
	void TickStoredEvent(CPed *ped);

	void RecordAbortedTask(CTask *task) { m_AbortedTask = task; }
	CTask *GetAbortedTask() const { return m_AbortedTask; }

private:
	CTask *m_AbortedTask; // active task aborted last tick; only null-checked
	CEvent *m_CurrentNonTempEvent;
	CEvent *m_CurrentTempEvent;
	CEvent *m_StoredEventActive;
	CTaskTimer m_StoreTimer;
};

VALIDATE_SIZE(CEventHandlerHistory, 0x1C);
