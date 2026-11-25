#include "common.h"

#include "EventHandler.h"
#include "EventGroup.h"
#include "Ped.h"
#include "PedIntelligence.h"

// SA CEventHandler::CEventHandler (0x4C3EE0).
CEventHandler::CEventHandler(CPed *ped)
	: m_Ped(ped)
{
	m_EventResponseTask = nil;
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

// SA CEventHandler::HandleEvents (0x4C3F10). The full SA body walks the
// ped's event group, picks the winning event against the history, and
// dispatches to the per-event Compute*Response builders; those builders
// land together with the task types they construct. Until then the handler
// maintains the history and the queue exactly as SA does, and queued
// events age out through the group's own validity rules.
void
CEventHandler::HandleEvents()
{
	m_History.TickStoredEvent(m_Ped);

	if(m_Ped == nil || m_Ped->m_pIntelligence == nil)
		return;

	CEventGroup &group = m_Ped->m_pIntelligence->GetEventGroup();
	group.TickEvents();

	CEvent *event = group.GetHighestPriorityEvent();
	if(event == nil)
		return;

	if(!m_History.TakesPriorityOverCurrentEvent(*event))
		return;

	m_History.RecordCurrentEvent(m_Ped, *event);
}
