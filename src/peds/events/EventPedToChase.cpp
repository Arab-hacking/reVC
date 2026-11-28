#include "EventPedToChase.h"

#include "Entity.h"
#include "Ped.h"

// SA CEventPedToChase.

CEventPedToChase::CEventPedToChase(CPed *pedToChase)
: m_PedToChase(pedToChase)
{
	if(m_PedToChase)
		m_PedToChase->RegisterReference((CEntity**)&m_PedToChase);
}

CEventPedToChase::~CEventPedToChase()
{
	if(m_PedToChase)
		m_PedToChase->CleanUpOldReference((CEntity**)&m_PedToChase);
}

bool
CEventPedToChase::AffectsPed(CPed *ped)
{
	return ped->m_fHealth > 0.0f;
}
