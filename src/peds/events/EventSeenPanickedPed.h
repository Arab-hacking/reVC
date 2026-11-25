#pragma once

// SA CEventSeenPanickedPed: "that ped over there is panicking" -
// posted to the global event group by the fleeing ped itself (the
// flee tasks), so bystanders can panic too. Only affects a ped when
// the panicked ped is itself answering an event with a source (i.e.
// fleeing *from* something) and is close by.

#include "EventEditableResponse.h"

class CEventSeenPanickedPed : public CEventEditableResponse
{
public:
	CPed *m_ped;

public:
	explicit CEventSeenPanickedPed(CPed *ped);
	~CEventSeenPanickedPed();

	eEventType GetEventType() const override { return EVENT_SEEN_PANICKED_PED; }
	int32 GetEventPriority() const override { return 13; }
	int32 GetLifeTime() override { return 0; }
	bool AffectsPed(CPed *ped) override;
	CEntity *GetSourceEntity() const override { return (CEntity*)m_ped; }
	CEventSeenPanickedPed *CloneEditable() const override { return new CEventSeenPanickedPed(m_ped); }
};

VALIDATE_SIZE(CEventSeenPanickedPed, 0x18);
