#include "common.h"

#include "DecisionMaker.h"

// SA CDecisionMaker::CDecisionMaker (0x6042A0).
CDecisionMaker::CDecisionMaker()
{
	for(int i = 0; i < DM_TOTAL_NUM_EVENTS; i++)
		m_aDecisions[i].SetDefault();
}
