#include "EventPedToFlee.h"

#include "Entity.h"
#include "Ped.h"

// SA CEventPedToFlee (0x4B73D0 is the clone).

CEventPedToFlee::CEventPedToFlee(CPed *pedToFlee)
: m_PedToFlee(pedToFlee)
{
	if(m_PedToFlee)
		m_PedToFlee->RegisterReference((CEntity**)&m_PedToFlee);
}

CEventPedToFlee::~CEventPedToFlee()
{
	if(m_PedToFlee)
		m_PedToFlee->CleanUpOldReference((CEntity**)&m_PedToFlee);
}

bool
CEventPedToFlee::AffectsPed(CPed *ped)
{
	return ped->m_fHealth > 0.0f;
}
