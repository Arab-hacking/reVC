#pragma once

// SA CEventPedToFlee: the mirror of CEventPedToChase - posted to the
// chased ped so his decision maker can answer with a flee response.

#include "Event.h"

class CPed;

class CEventPedToFlee : public CEvent
{
public:
	CPed *m_PedToFlee;

public:
	CEventPedToFlee(CPed *pedToFlee);
	~CEventPedToFlee();

	eEventType GetEventType() const { return EVENT_PED_TO_FLEE; }
	int32 GetEventPriority() const { return 2; }
	int32 GetLifeTime() { return 0; }
	CEvent *Clone() const { return new CEventPedToFlee(m_PedToFlee); }
	bool AffectsPed(CPed *ped);
	CEntity *GetSourceEntity() const { return (CEntity*)m_PedToFlee; }

	CPed *GetPedToFlee() const { return m_PedToFlee; }
};

VALIDATE_SIZE(CEventPedToFlee, 0x10);
