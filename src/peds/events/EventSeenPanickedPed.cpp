#include "common.h"

#include "EventSeenPanickedPed.h"

#include "Entity.h"
#include "EventHandler.h"
#include "EventHandlerHistory.h"
#include "Ped.h"
#include "PedIntelligence.h"

// SA CEventSeenPanickedPed.

// 0x4B2080
CEventSeenPanickedPed::CEventSeenPanickedPed(CPed *ped)
: CEventEditableResponse()
{
	m_ped = ped;
	if(m_ped)
		m_ped->RegisterReference((CEntity**)&m_ped);
}

CEventSeenPanickedPed::~CEventSeenPanickedPed()
{
	if(m_ped)
		m_ped->CleanUpOldReference((CEntity**)&m_ped);
}

// 0x4B53C0
bool
CEventSeenPanickedPed::AffectsPed(CPed *ped)
{
	if(!ped->IsPlayer() && m_ped && m_ped != ped){
		CEvent *currentEvent = m_ped->GetIntelligence()->GetEventHandler().GetHistory().GetCurrentEvent();
		if(currentEvent && currentEvent->GetSourceEntity()){
			CVector distance = ped->GetPosition() - m_ped->GetPosition();
			return distance.MagnitudeSqr() < 100.0f;
		}
	}
	return false;
}
