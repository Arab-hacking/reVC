#pragma once

// SA CPedTaskPair: one ped's pending group-assigned task inside
// CPedGroupIntelligence - who it is for, the task itself (owned by the
// pair), the secondary slot it belongs in when it is a secondary task,
// and the match bookkeeping the group intelligence uses to pair tasks
// with members.

#include "TaskManager.h" // eSecondaryTask

class CPed;
class CTask;

class CPedTaskPair
{
public:
	CPed *Ped;
	CTask *Task;
	eSecondaryTask Slot;
	int32 MatchID;
	bool UsedTask;

	CPedTaskPair()
	{
		Ped = nil;
		Task = nil;
		Slot = TASK_SECONDARY_INVALID;
		MatchID = 0;
		UsedTask = false;
	}

	void Flush();
};

VALIDATE_SIZE(CPedTaskPair, 0x14);
