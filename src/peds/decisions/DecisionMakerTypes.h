#ifndef __GTA_DECISIONMAKERTYPES_H__
#define __GTA_DECISIONMAKERTYPES_H__

#include "Decision.h"
#include "DecisionMaker.h"
#include "eTaskType.h"
#include "eEventType.h"

class CPed;
class CPedGroup;

// SA eDecisionMakerType: which table a ped thinks with. Built-ins come first;
// the mission makers follow, filled from data when a mission asks for one.
enum eDecisionMakerType : int32
{
	DECISION_MAKER_UNKNOWN                 = -1,

	DECISION_MAKER_PED_GROUPMEMBER         = 0,
	DECISION_MAKER_PED_COP                 = 1,
	DECISION_MAKER_PED_RANDOM1             = 2,
	DECISION_MAKER_PED_RANDOM2             = 3,
	DECISION_MAKER_PED_RANDOM3             = 4,
	DECISION_MAKER_PED_FIREMAN             = 5,
	DECISION_MAKER_PED_EMPTY               = 6,
	DECISION_MAKER_PED_INDOORS             = 7,

	DECISION_MAKER_GROUP_RANDOM_AGGRESSIVE = 8,
	DECISION_MAKER_GROUP_RANDOM_PASSIVE    = 9,

	DECISION_MAKER_COUNT_GAME              = 10,

	DECISION_MAKER_MISSION0                = 10,
	DECISION_MAKER_MISSION1                = 11,
	DECISION_MAKER_MISSION2                = 12,
	DECISION_MAKER_MISSION3                = 13,
	DECISION_MAKER_MISSION4                = 14,
	DECISION_MAKER_MISSION5                = 15,
	DECISION_MAKER_MISSION6                = 16,
	DECISION_MAKER_MISSION7                = 17,
	DECISION_MAKER_MISSION8                = 18,
	DECISION_MAKER_MISSION9                = 19,

	DECISION_MAKER_COUNT_TOTAL             = 20
};

// SA CDecisionMakerTypes (singleton): every decision-maker table the game has
// loaded, plus the mapping from event types to table rows. The tables come
// from data (decision/*.dat in SA) through CDecisionMakerTypesFileLoader;
// unfilled rows are TASK_INVALID, which the machinery reads as "no response".
//
// The instance is a plain static here; SA keeps it behind a pointer it owns.
class CDecisionMakerTypes
{
public:
	static const uint32 NUM_TYPES = 20;

	static uint16 ScriptReferenceIndex[NUM_TYPES];
	static bool m_IsActive[NUM_TYPES];

	int32 m_NoOfDecisionMakers;
	CDecisionMaker m_DecisionMakers[DECISION_MAKER_COUNT_TOTAL];
	int32 m_EventIndices[EVENT_TOTAL_NUM_EVENTS];
	CDecisionMaker m_DefaultRandomPedDecisionMaker;
	CDecisionMaker m_DefaultMissionPedDecisionMaker;
	CDecisionMaker m_DefaultPlayerPedDecisionMaker;
	CDecisionMaker m_DefaultRandomPedGroupDecisionMaker;
	CDecisionMaker m_DefaultMissionPedGroupDecisionMaker;

	CDecisionMakerTypes();

	static CDecisionMakerTypes *GetInstance();

	int32 AddDecisionMaker(CDecisionMaker *decisionMaker, eDecisionTypes decisionMakerType, bool bDecisionMakerForMission);
	void MakeDecision(CPed *ped, eEventType eventType, int32 eventSourceType, bool bIsPedInVehicle, eTaskType taskTypeToAvoid1, eTaskType taskTypeToAvoid2, eTaskType taskTypeToAvoid3, eTaskType taskTypeToSeek, bool bUseInGroupDecisionMaker, int16 &taskType, int16 &facialTaskType);
	void RemoveDecisionMaker(eDecisionTypes dm);
	eTaskType MakeDecision(CPedGroup *pedGroup, eEventType eventType, int32 eventSourceType, bool bIsPedInVehicle, eTaskType taskId1, eTaskType taskId2, eTaskType taskId3, eTaskType taskId4);
	void AddEventResponse(int32 decisionMakerIndex, eEventType eventType, eTaskType taskId, float responseChances[], int32 flags[]);
	void FlushDecisionMakerEventResponse(int32 decisionMakerIndex, eEventType eventType);
	void LoadEventIndices();
};

#endif // __GTA_DECISIONMAKERTYPES_H__
