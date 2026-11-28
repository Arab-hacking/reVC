#pragma once

// SA CTaskSequences: the global bank of 64 script task sequences.
// Game code opens a sequence, adds tasks to the active one, closes it,
// and later hands peds a CTaskComplexUseSequence pointing at its slot.

#include "TaskComplexSequence.h"

class CTaskSequences
{
public:
	static const int32 NUM_SEQUENCES = 64;

	static int32 ms_iActiveSequence;
	static CTaskComplexSequence ms_taskSequence[NUM_SEQUENCES];
	static bool ms_bIsOpened[NUM_SEQUENCES];

	static void Init();
	static void CleanUpForShutdown();
};
