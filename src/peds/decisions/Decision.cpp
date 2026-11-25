#include "common.h"

#include "Decision.h"

// SA CDecision::CDecision (0x6042C0): a fresh row ignores everything.
CDecision::CDecision()
{
	SetDefault();
}

// SA CDecision::SetDefault (0x6042E0).
void
CDecision::SetDefault()
{
	for(int i = 0; i < MAX_NUM_CHOICES; i++){
		m_Tasks[i] = TASK_INVALID;
		for(int j = 0; j < 4; j++)
			m_Probs[i][j] = 0;
		for(int j = 0; j < 2; j++)
			m_Bools[i][j] = false;
	}
}

// SA CDecision::From (0x604330): copy a whole row.
void
CDecision::From(const CDecision &rhs)
{
	*this = rhs;
}

// SA CDecision::Set (0x604350). The facial chances are accepted but not
// stored on the row, matching SA.
void
CDecision::Set(int32 tasks[MAX_NUM_CHOICES], float probs[MAX_NUM_CHOICES][4], int32 bools[MAX_NUM_CHOICES][2], float facialProbs[MAX_NUM_CHOICES])
{
	for(int i = 0; i < MAX_NUM_CHOICES; i++){
		m_Tasks[i] = (eTaskType)tasks[i];
		for(int j = 0; j < 4; j++)
			m_Probs[i][j] = (uint8)probs[i][j];
		for(int j = 0; j < 2; j++)
			m_Bools[i][j] = bools[i][j] != 0;
	}
}
