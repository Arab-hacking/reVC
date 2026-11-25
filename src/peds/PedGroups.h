#pragma once

// SA CPedGroups: the registry of the game's ped groups - eight slots,
// created and destroyed on demand (in SA mainly by the script's group
// commands and the player's own group). A group with no members left
// removes itself in Process.

#include "PedGroup.h"

class CPedGroups
{
public:
	static const int32 NUM_PED_GROUPS = 8;

	static uint16 ScriptReferenceIndex[NUM_PED_GROUPS];
	static bool ms_activeGroups[NUM_PED_GROUPS];
	static bool ms_bIsPlayerOnAMission;
	static uint32 ms_iNoOfPlayerKills;
	static CPedGroup ms_groups[NUM_PED_GROUPS];

public:
	static int32 AddGroup();
	static void RemoveGroup(int32 groupID);
	static void RemoveAllFollowersFromGroup(int32 groupId);

	static void Init();

	static void RegisterKillByPlayer();

	static void CleanUpForShutDown();

	static bool IsGroupLeader(CPed *ped);

	static CPedGroup *GetPedsGroup(const CPed *ped);
	static int32 GetGroupId(const CPedGroup *pedGroup);

	static void Process();

	static bool AreInSameGroup(const CPed *ped1, const CPed *ped2);
	static bool IsInPlayersGroup(const CPed *ped);

	static CPedGroup &GetGroup(int32 groupId) { return ms_groups[groupId]; }
};
