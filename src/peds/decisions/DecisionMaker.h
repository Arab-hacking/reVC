#ifndef __GTA_DECISIONMAKER_H__
#define __GTA_DECISIONMAKER_H__

#include "Decision.h"
#include "eDecisionMakerEvents.h"

// SA CDecisionMaker (0x99C): the complete set of responses for one class of
// ped - one row for every kind of thing that might happen to it.
class CDecisionMaker
{
public:
	CDecision m_aDecisions[DM_TOTAL_NUM_EVENTS];

	CDecisionMaker();
};

VALIDATE_SIZE(CDecisionMaker, 0x99C);

#endif // __GTA_DECISIONMAKER_H__
