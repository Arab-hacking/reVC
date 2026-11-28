#include "common.h"

#include "DecisionMakerTypes.h"
#include "Ped.h"
#include "PedIntelligence.h"
#include "General.h"

uint16 CDecisionMakerTypes::ScriptReferenceIndex[NUM_TYPES];
bool CDecisionMakerTypes::m_IsActive[NUM_TYPES];

static CDecisionMakerTypes s_DecisionMakerTypes;

// SA CDecisionMakerTypes::CDecisionMakerTypes: every table starts empty -
// "no response" - and every event starts unmapped; the data loader fills both.
CDecisionMakerTypes::CDecisionMakerTypes()
{
	m_NoOfDecisionMakers = 0;
	for(int i = 0; i < EVENT_TOTAL_NUM_EVENTS; i++)
		m_EventIndices[i] = DM_EVENT_UNDEFINED;
	for(uint32 i = 0; i < NUM_TYPES; i++){
		ScriptReferenceIndex[i] = 0;
		m_IsActive[i] = false;
	}
}

// SA CDecisionMakerTypes::GetInstance (0x4684F0).
CDecisionMakerTypes*
CDecisionMakerTypes::GetInstance()
{
	return &s_DecisionMakerTypes;
}

// SA CDecisionMakerTypes::AddDecisionMaker (0x607050): every registered table
// is active; built-ins are numbered in declaration order, mission tables
// take slots after the built ones.
int32
CDecisionMakerTypes::AddDecisionMaker(CDecisionMaker *decisionMaker, eDecisionTypes decisionMakerType, bool bDecisionMakerForMission)
{
	(void)decisionMakerType;
	int32 index = bDecisionMakerForMission ? DECISION_MAKER_COUNT_GAME + m_NoOfDecisionMakers : m_NoOfDecisionMakers;
	if(index < 0 || index >= DECISION_MAKER_COUNT_TOTAL)
		return -1;
	m_IsActive[index] = true;
	m_NoOfDecisionMakers++;
	ScriptReferenceIndex[index]++;
	return index;
}

// SA CDecisionMakerTypes::RemoveDecisionMaker (0x6043A0).
void
CDecisionMakerTypes::RemoveDecisionMaker(eDecisionTypes dm)
{
	if(dm >= 0 && dm < DECISION_MAKER_COUNT_TOTAL)
		m_IsActive[dm] = false;
}

// SA CDecisionMakerTypes::MakeDecision (0x606E70). Upstream has not reversed
// this yet; the behaviour below is reconstructed from the table format and
// the wiki description of the algorithm: walk the row's choices in order,
// roll against each one's probability for the relationship and the
// in-vehicle state, and take the first response that fires and is neither
// avoided nor filtered out. TASK_INVALID everywhere is "ignore the event".
void
CDecisionMakerTypes::MakeDecision(CPed *ped, eEventType eventType, int32 eventSourceType, bool bIsPedInVehicle, eTaskType taskTypeToAvoid1, eTaskType taskTypeToAvoid2, eTaskType taskTypeToAvoid3, eTaskType taskTypeToSeek, bool bUseInGroupDecisionMaker, int16 &taskType, int16 &facialTaskType)
{
	taskType = (int16)TASK_INVALID;
	facialTaskType = (int16)TASK_INVALID;

	if(eventType < 0 || eventType >= EVENT_TOTAL_NUM_EVENTS)
		return;
	int32 dmEvent = m_EventIndices[eventType];
	if(dmEvent < 0 || dmEvent >= DM_TOTAL_NUM_EVENTS)
		return;

	// The decision maker that answers is the ped's group table when the
	// event came through its group, otherwise the ped's own - or the
	// built-in random-ped table while a ped has no intelligence yet.
	int32 dmIndex = bUseInGroupDecisionMaker ? DECISION_MAKER_GROUP_RANDOM_PASSIVE : DECISION_MAKER_PED_RANDOM1;
	if(ped != nil && ped->m_pIntelligence != nil){
		int32 pedDm = ped->m_pIntelligence->m_nDecisionMakerType;
		int32 groupDm = ped->m_pIntelligence->m_nDecisionMakerTypeInGroup;
		if(bUseInGroupDecisionMaker && groupDm >= 0 && groupDm < DECISION_MAKER_COUNT_TOTAL)
			dmIndex = groupDm;
		else if(!bUseInGroupDecisionMaker && pedDm >= 0 && pedDm < DECISION_MAKER_COUNT_TOTAL)
			dmIndex = pedDm;
	}

	const CDecision &decision = m_DecisionMakers[dmIndex].m_aDecisions[dmEvent];

	int32 relationship = eventSourceType;
	if(relationship < DECISION_RELATIONSHIP_NEUTRAL || relationship > DECISION_RELATIONSHIP_THREAT)
		relationship = DECISION_RELATIONSHIP_NEUTRAL;
	int32 vehicleIndex = bIsPedInVehicle ? 1 : 0;

	// A task the caller asked to seek wins outright if the row offers it.
	if(taskTypeToSeek != TASK_INVALID){
		for(int i = 0; i < CDecision::MAX_NUM_CHOICES; i++){
			if(decision.m_Tasks[i] == taskTypeToSeek && decision.m_Bools[i][vehicleIndex]){
				taskType = (int16)decision.m_Tasks[i];
				return;
			}
		}
	}

	for(int i = 0; i < CDecision::MAX_NUM_CHOICES; i++){
		eTaskType candidate = decision.m_Tasks[i];
		if(candidate == TASK_INVALID)
			continue;
		if(!decision.m_Bools[i][vehicleIndex])
			continue;
		if(candidate == taskTypeToAvoid1 || candidate == taskTypeToAvoid2 || candidate == taskTypeToAvoid3)
			continue;
		if(CGeneral::GetRandomNumberInRange(0, 100) < decision.m_Probs[i][relationship]){
			taskType = (int16)candidate;
			return;
		}
	}
}

// SA CDecisionMakerTypes::MakeDecision, group form (0x606F80). Reconstructed
// the same way: a group answers as a whole, first firing choice wins.
eTaskType
CDecisionMakerTypes::MakeDecision(CPedGroup *pedGroup, eEventType eventType, int32 eventSourceType, bool bIsPedInVehicle, eTaskType taskId1, eTaskType taskId2, eTaskType taskId3, eTaskType taskId4)
{
	(void)pedGroup;
	int16 taskType, facialTaskType;
	CPed *ped = nil;
	MakeDecision(ped, eventType, eventSourceType, bIsPedInVehicle, taskId1, taskId2, taskId3, taskId4, true, taskType, facialTaskType);
	return (eTaskType)taskType;
}

// SA CDecisionMakerTypes::AddEventResponse (0x6044C0): register one task as
// a possible response to one event in one table, with its chances against
// the four relationships and its in/out-of-vehicle flags.
void
CDecisionMakerTypes::AddEventResponse(int32 decisionMakerIndex, eEventType eventType, eTaskType taskId, float responseChances[], int32 flags[])
{
	if(decisionMakerIndex < 0 || decisionMakerIndex >= DECISION_MAKER_COUNT_TOTAL)
		return;
	if(eventType < 0 || eventType >= EVENT_TOTAL_NUM_EVENTS)
		return;
	int32 dmEvent = m_EventIndices[eventType];
	if(dmEvent < 0 || dmEvent >= DM_TOTAL_NUM_EVENTS)
		return;

	CDecision &decision = m_DecisionMakers[decisionMakerIndex].m_aDecisions[dmEvent];
	for(int i = 0; i < CDecision::MAX_NUM_CHOICES; i++){
		if(decision.m_Tasks[i] == TASK_INVALID || decision.m_Tasks[i] == taskId){
			decision.m_Tasks[i] = taskId;
			for(int j = 0; j < 4; j++)
				decision.m_Probs[i][j] = (uint8)responseChances[j];
			decision.m_Bools[i][0] = flags[0] != 0;
			decision.m_Bools[i][1] = flags[1] != 0;
			return;
		}
	}
}

// SA CDecisionMakerTypes::FlushDecisionMakerEventResponse (0x604490).
void
CDecisionMakerTypes::FlushDecisionMakerEventResponse(int32 decisionMakerIndex, eEventType eventType)
{
	if(decisionMakerIndex < 0 || decisionMakerIndex >= DECISION_MAKER_COUNT_TOTAL)
		return;
	if(eventType < 0 || eventType >= EVENT_TOTAL_NUM_EVENTS)
		return;
	int32 dmEvent = m_EventIndices[eventType];
	if(dmEvent < 0 || dmEvent >= DM_TOTAL_NUM_EVENTS)
		return;
	m_DecisionMakers[decisionMakerIndex].m_aDecisions[dmEvent].SetDefault();
}

// SA CDecisionMakerTypes::LoadEventIndices (0x600840): map each event type to
// the decision-table row that answers it. Rows without an event equivalent
// stay unmapped and can never fire. Reconstructed from the two enumerations,
// which are parallel for the events both systems name.
void
CDecisionMakerTypes::LoadEventIndices()
{
	for(int i = 0; i < EVENT_TOTAL_NUM_EVENTS; i++)
		m_EventIndices[i] = DM_EVENT_UNDEFINED;

	m_EventIndices[EVENT_DRAGGED_OUT_CAR]          = DM_EVENT_DRAGGED_OUT_CAR;
	m_EventIndices[EVENT_KNOCK_OFF_BIKE]           = DM_EVENT_KNOCK_OFF_BIKE;
	m_EventIndices[EVENT_DAMAGE]                   = DM_EVENT_DAMAGE;
	m_EventIndices[EVENT_DEAD_PED]                 = DM_EVENT_DEAD_PED;
	m_EventIndices[EVENT_POTENTIAL_GET_RUN_OVER]   = DM_EVENT_POTENTIAL_GET_RUN_OVER;
	m_EventIndices[EVENT_POTENTIAL_WALK_INTO_PED]  = DM_EVENT_POTENTIAL_WALK_INTO_PED;
	m_EventIndices[EVENT_SHOT_FIRED]               = DM_EVENT_SHOT_FIRED;
	m_EventIndices[EVENT_PED_ENTERED_MY_VEHICLE]   = DM_EVENT_PED_ENTERED_MY_VEHICLE;
	m_EventIndices[EVENT_SEXY_PED]                 = DM_EVENT_SEXY_PED;
	m_EventIndices[EVENT_ATTRACTOR]                = DM_EVENT_ATTRACTOR;
	m_EventIndices[EVENT_VEHICLE_THREAT]           = DM_EVENT_VEHICLE_THREAT;
	m_EventIndices[EVENT_GUN_AIMED_AT]             = DM_EVENT_GUN_AIMED_AT;
	m_EventIndices[EVENT_ACQUAINTANCE_PED_HATE]    = DM_EVENT_ACQUAINTANCE_PED_HATE;
	m_EventIndices[EVENT_ACQUAINTANCE_PED_DISLIKE] = DM_EVENT_ACQUAINTANCE_PED_DISLIKE;
	m_EventIndices[EVENT_ACQUAINTANCE_PED_LIKE]    = DM_EVENT_ACQUAINTANCE_PED_LIKE;
	m_EventIndices[EVENT_ACQUAINTANCE_PED_RESPECT] = DM_EVENT_ACQUAINTANCE_PED_RESPECT;
	m_EventIndices[EVENT_VEHICLE_DAMAGE_WEAPON]    = DM_EVENT_VEHICLE_DAMAGE_WEAPON;
	m_EventIndices[EVENT_SPECIAL]                  = DM_EVENT_SPECIAL;
	m_EventIndices[EVENT_GOT_KNOCKED_OVER_BY_CAR]  = DM_EVENT_GOT_KNOCKED_OVER_BY_CAR;
	m_EventIndices[EVENT_POTENTIAL_WALK_INTO_OBJECT] = DM_EVENT_POTENTIAL_WALK_INTO_OBJECT;
	m_EventIndices[EVENT_POTENTIAL_WALK_INTO_FIRE]   = DM_EVENT_POTENTIAL_WALK_INTO_FIRE;
	m_EventIndices[EVENT_SCRIPTED_ATTRACTOR]       = DM_EVENT_SCRIPTED_ATTRACTOR;
	m_EventIndices[EVENT_SHOT_FIRED_WHIZZED_BY]    = DM_EVENT_SHOT_FIRED_WHIZZED_BY;
	m_EventIndices[EVENT_LOW_ANGER_AT_PLAYER]      = DM_EVENT_LOW_ANGER_AT_PLAYER;
	m_EventIndices[EVENT_HIGH_ANGER_AT_PLAYER]     = DM_EVENT_HIGH_ANGER_AT_PLAYER;
	m_EventIndices[EVENT_HEALTH_REALLY_LOW]        = DM_EVENT_HEALTH_REALLY_LOW;
	m_EventIndices[EVENT_HEALTH_LOW]               = DM_EVENT_HEALTH_LOW;
	m_EventIndices[EVENT_LEADER_ENTERED_CAR_AS_DRIVER] = DM_EVENT_LEADER_ENTERED_CAR_AS_DRIVER;
	m_EventIndices[EVENT_LEADER_EXITED_CAR_AS_DRIVER]  = DM_EVENT_LEADER_EXITED_CAR_AS_DRIVER;
	m_EventIndices[EVENT_POTENTIAL_WALK_INTO_VEHICLE]  = DM_EVENT_POTENTIAL_WALK_INTO_VEHICLE;
	m_EventIndices[EVENT_FIRE_NEARBY]              = DM_EVENT_FIRE_NEARBY;
	m_EventIndices[EVENT_SOUND_LOUD]               = DM_EVENT_SOUND_LOUD;
	m_EventIndices[EVENT_SOUND_QUIET]              = DM_EVENT_SOUND_QUIET;
	m_EventIndices[EVENT_ACQUAINTANCE_PED_HATE_BADLY_LIT] = DM_EVENT_ACQUAINTANCE_PED_HATE_BADLY_LIT;
	m_EventIndices[EVENT_SEEN_PANICKED_PED]        = DM_EVENT_SEEN_PANICKED_PED;
	m_EventIndices[EVENT_PLAYER_COMMAND_TO_GROUP]  = DM_EVENT_PLAYER_COMMAND_TO_GROUP;
	m_EventIndices[EVENT_SEEN_COP]                 = DM_EVENT_SEEN_COP;
	m_EventIndices[EVENT_VEHICLE_DAMAGE_COLLISION] = DM_EVENT_VEHICLE_DAMAGE_COLLISION;
	m_EventIndices[EVENT_DANGER]                   = DM_EVENT_DANGER;
	m_EventIndices[EVENT_PLAYER_COMMAND_TO_GROUP_GATHER] = DM_EVENT_PLAYER_COMMAND_TO_GROUP_GATHER;
	m_EventIndices[EVENT_VEHICLE_ON_FIRE]          = DM_EVENT_VEHICLE_ON_FIRE;
}
