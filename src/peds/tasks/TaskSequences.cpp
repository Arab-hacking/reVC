#include "TaskSequences.h"

// SA CTaskSequences statics. In SA these live at fixed addresses and are
// default-constructed by the CRT; here they are ordinary statics, which
// the CTaskComplexSequence default constructor leaves empty.

int32 CTaskSequences::ms_iActiveSequence = 0;
CTaskComplexSequence CTaskSequences::ms_taskSequence[CTaskSequences::NUM_SEQUENCES];
bool CTaskSequences::ms_bIsOpened[CTaskSequences::NUM_SEQUENCES];

// SA CTaskSequences::Init.
void
CTaskSequences::Init()
{
	ms_iActiveSequence = 0;
	for(int i = 0; i < NUM_SEQUENCES; i++)
		ms_bIsOpened[i] = false;
}

// SA CTaskSequences::CleanUpForShutdown.
void
CTaskSequences::CleanUpForShutdown()
{
	for(int i = 0; i < NUM_SEQUENCES; i++)
		ms_taskSequence[i].Flush();
}
