#ifndef __GTA_EVENTDEATH_H__
#define __GTA_EVENTDEATH_H__

#include "Event.h"

// The ped has died. Raised on the ped himself so his task system hears
// about it (and by tasks that watch for it, e.g. a death mid-exit).
class CEventDeath : public CEvent
{
public:
	bool m_bDrowning;
	uint32 m_deathTimeInMs;

public:
	CEventDeath(bool bDrowning);
	CEventDeath(bool bDrowning, uint32 deathTimeInMs);
	~CEventDeath() {}

	eEventType GetEventType() const { return EVENT_DEATH; }
	int32 GetEventPriority() const { return 73; }
	int32 GetLifeTime() { return 0; }
	CEvent *Clone() const { return new CEventDeath(m_bDrowning, m_deathTimeInMs); }
	bool AffectsPed(CPed *ped) { return true; }

	uint32 GetDeathTime() const { return m_deathTimeInMs; }
	bool HasDrowned() const { return m_bDrowning; }
};

VALIDATE_SIZE(CEventDeath, 0x14);

#endif // __GTA_EVENTDEATH_H__
