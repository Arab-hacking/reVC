#pragma once

// SA CEventPedToChase: "that ped over there is worth chasing" - posted
// to a cop's event group when he spots a criminal (CTaskComplexWanderCop)
// and answered by the decision maker with a pursuit/fight response.
// The ped pointer is a registered reference (VC's CEntity::RegisterReference):
// it nulls itself if the target dies first, standing in for SA's CPed::Ref.

#include "Event.h"

class CPed;

class CEventPedToChase : public CEvent
{
public:
	CPed *m_PedToChase;

public:
	CEventPedToChase(CPed *pedToChase);
	~CEventPedToChase();

	eEventType GetEventType() const { return EVENT_PED_TO_CHASE; }
	int32 GetEventPriority() const { return 1; }
	int32 GetLifeTime() { return 0; }
	CEvent *Clone() const { return new CEventPedToChase(m_PedToChase); }
	bool AffectsPed(CPed *ped);
	CEntity *GetSourceEntity() const { return (CEntity*)m_PedToChase; }

	CPed *GetPedToChase() const { return m_PedToChase; }
};

VALIDATE_SIZE(CEventPedToChase, 0x10);
