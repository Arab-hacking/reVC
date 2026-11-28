#pragma once

#include "common.h"
#include "Event.h"
#include "eTaskType.h"

class CPed;

#define TOTAL_EVENTS_PER_EVENTGROUP 16

// SA CEventGroup (0x4AB340): a ped's pending event queue. Scanners and
// other peds add events here; events are cloned on add (the caller keeps
// owning the original), editable-response events compute what the ped
// would do about them at add time, and the event handler consumes the
// queue each tick.
class CEventGroup
{
public:
	CPed *m_pPed;
	int32 m_count;
	CEvent *m_events[TOTAL_EVENTS_PER_EVENTGROUP];

public:
	explicit CEventGroup(CPed *ped);
	virtual ~CEventGroup();

	CEvent *Add(CEvent *event, bool bValid = false);
	bool HasScriptCommandOfTaskType(eTaskType taskId);
	bool HasEventOfType(CEvent *event) const;
	bool HasEvent(CEvent *event);
	CEvent *GetHighestPriorityEvent();
	void TickEvents();
	void Remove(CEvent *event);
	void RemoveInvalidEvents(bool bRemoveNonScriptCommandEvents);

	// 0x4AB700: re-count the number of events and fill holes in the queue
	void Reorganise();
	void Flush(bool bAvoidFlushingTaskComplexBeInGroup = false);
	CEvent *GetEventOfType(eEventType type) const;

	int32 GetNumEventsInQueue() const { return m_count; }
};

VALIDATE_SIZE(CEventGroup, 0x4C);
