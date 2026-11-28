#include "common.h"

#include "EventGlobalGroup.h"

#include "Event.h"
#include "Ped.h"
#include "PedIntelligence.h"

// SA CEventGlobalGroup (0x4ABA50 for the accessor).

CEventGlobalGroup*
GetEventGlobalGroup()
{
	static CEventGlobalGroup *globalEvents = nil;

	if(globalEvents)
		return globalEvents;

	globalEvents = new CEventGlobalGroup(nil);
	return globalEvents;
}

// 0x4AB900
float
CEventGlobalGroup::GetSoundLevel(CEntity *entity, CVector &position)
{
	float soundLevel = 0.0f;
	for(int32 i = 0; i < m_count; i++){
		CEvent *event = m_events[i];
		if(event->GetSourceEntity() == entity || !entity){
			float eventSoundLevel = event->GetSoundLevel(entity, position);
			if(eventSoundLevel > 0.0f)
				soundLevel += CEvent::CalcSoundLevelIncrement(soundLevel, eventSoundLevel);
		}
	}
	return soundLevel;
}

// 0x4AB9C0
void
CEventGlobalGroup::AddEventsToPed(CPed *ped)
{
	if(ped->GetIntelligence() == nil)
		return;
	for(int32 i = 0; i < m_count; i++){
		CEvent *clonedEvent = m_events[i]->Clone();
		ped->GetIntelligence()->GetEventGroup().Add(clonedEvent, false);
		delete clonedEvent;
	}
}
