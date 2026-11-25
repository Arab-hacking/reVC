#pragma once

// SA CEventGlobalGroup: the one event group that belongs to no ped -
// a broadcast box. Firing a gun, a ped panicking, and similar
// world-level happenings are posted here, and once per frame
// CWorld::Process hands a copy to every moving ped (AddEventsToPed)
// and then flushes the box. AddEventsToGroup (the CPedGroup side of
// the hand-out) lands with the group-intelligence event machinery.

#include "EventGroup.h"

class CEntity;
class CPed;

class CEventGlobalGroup : public CEventGroup
{
public:
	CEventGlobalGroup(CPed *ped) : CEventGroup(ped) {}
	~CEventGlobalGroup() {}

	float GetSoundLevel(CEntity *entity, CVector &position);
	void AddEventsToPed(CPed *ped);
};

CEventGlobalGroup *GetEventGlobalGroup();
