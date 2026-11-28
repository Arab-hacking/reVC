#ifndef __GTA_DECISION_H__
#define __GTA_DECISION_H__

#include "eTaskType.h"

// SA CDecision: one row of the decision-maker tables - everything a ped might
// do in response to one kind of event, with how likely each option is against
// four kinds of source (neutral, player, friend, threat), in and out of a
// vehicle.
//
// A decision that leads to TASK_INVALID everywhere means "ignore this event".

enum eDecisionTypes
{
	DEFAULT_DECISION_MAKER = 0,
	PLAYER_DECISION_MAKER = 1
};

enum eDecisionRelationship
{
	DECISION_RELATIONSHIP_NEUTRAL = 0,
	DECISION_RELATIONSHIP_PLAYER = 1,
	DECISION_RELATIONSHIP_FRIEND = 2,
	DECISION_RELATIONSHIP_THREAT = 3
};

class CDecision
{
public:
	static const int MAX_NUM_DECISIONS = 41; // 0x29
	static const int MAX_NUM_CHOICES = 6;

	// [choice]
	eTaskType m_Tasks[MAX_NUM_CHOICES];
	// [choice][relationship]
	uint8 m_Probs[MAX_NUM_CHOICES][4];
	// [choice][in/out of vehicle]
	bool m_Bools[MAX_NUM_CHOICES][2];

public:
	CDecision();

	void SetDefault();
	void From(const CDecision &rhs);
	void Set(int32 tasks[MAX_NUM_CHOICES], float probs[MAX_NUM_CHOICES][4], int32 bools[MAX_NUM_CHOICES][2], float facialProbs[MAX_NUM_CHOICES]);
};

VALIDATE_SIZE(CDecision, 0x3C);

#endif // __GTA_DECISION_H__
