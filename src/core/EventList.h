#ifndef __GTA_EVENTLIST_H__
#define __GTA_EVENTLIST_H__

class CEntity;
class CPed;

enum eScriptEventType
{
	SCRIPT_EVENT_NULL,
	SCRIPT_EVENT_ASSAULT,
	SCRIPT_EVENT_RUN_REDLIGHT,
	SCRIPT_EVENT_ASSAULT_POLICE,
	SCRIPT_EVENT_GUNSHOT,
	SCRIPT_EVENT_INJURED_PED,
	SCRIPT_EVENT_DEAD_PED,
	SCRIPT_EVENT_FIRE,
	SCRIPT_EVENT_STEAL_CAR,
	SCRIPT_EVENT_HIT_AND_RUN,
	SCRIPT_EVENT_HIT_AND_RUN_COP,
	SCRIPT_EVENT_SHOOT_PED,
	SCRIPT_EVENT_SHOOT_COP,
	SCRIPT_EVENT_EXPLOSION,
	SCRIPT_EVENT_PED_SET_ON_FIRE,
	SCRIPT_EVENT_COP_SET_ON_FIRE,
	SCRIPT_EVENT_CAR_SET_ON_FIRE,
	SCRIPT_EVENT_ASSAULT_NASTYWEAPON,
	SCRIPT_EVENT_ASSAULT_NASTYWEAPON_POLICE,
	SCRIPT_EVENT_UNK, // Not on SA it seems
	SCRIPT_EVENT_ICECREAM,
	SCRIPT_EVENT_ATM,
	SCRIPT_EVENT_SHOPSTALL,
	SCRIPT_EVENT_SHOPWINDOW,
	SCRIPT_EVENT_LAST_EVENT
};

enum eEventEntity
{
	EVENT_ENTITY_NONE,
	EVENT_ENTITY_PED,
	EVENT_ENTITY_VEHICLE,
	EVENT_ENTITY_OBJECT
};

struct CScriptEvent
{
	eScriptEventType type;
	eEventEntity entityType;
	int32 entityRef;
	CPed *criminal;
	CVector posn;
	uint32 timeout;
	int32 state;
};

class CEventList
{
	static int32 ms_nFirstFreeSlotIndex;
public:
	static void Initialise(void);
	static void Update(void);
	static void RegisterEvent(eScriptEventType type, eEventEntity entityType, CEntity *ent, CPed *criminal, int32 timeout);
	static void RegisterEvent(eScriptEventType type, CVector posn, int32 timeout);
	static bool GetEvent(eScriptEventType type, int32 *event);
	static void ClearEvent(int32 event);
	static bool FindClosestEvent(eScriptEventType type, CVector posn, int32 *event);
	static void ReportCrimeForEvent(eScriptEventType type, intptr, bool);
};

extern CScriptEvent gaEvent[NUMEVENTS];

#endif // __GTA_EVENTLIST_H__
