#include "common.h"

#include "PedGroups.h"

#include "Ped.h"
#include "Script.h"

// SA CPedGroups. Several of these are plugin calls in the SA
// reference (AddGroup, GetPedsGroup, AreInSameGroup, ...); the bodies
// here are reconstructions from their SA call sites and semantics.

uint16 CPedGroups::ScriptReferenceIndex[NUM_PED_GROUPS];
bool CPedGroups::ms_activeGroups[NUM_PED_GROUPS];
bool CPedGroups::ms_bIsPlayerOnAMission;
uint32 CPedGroups::ms_iNoOfPlayerKills;
CPedGroup CPedGroups::ms_groups[NUM_PED_GROUPS];

// 0x5FB800 (reconstructed): claim the first free slot; -1 if none
int32
CPedGroups::AddGroup()
{
	for(int32 i = 0; i < NUM_PED_GROUPS; i++){
		if(!ms_activeGroups[i]){
			ms_activeGroups[i] = true;
			ms_groups[i].Flush();
			return i;
		}
	}
	return -1;
}

// 0x5FB870
void
CPedGroups::RemoveGroup(int32 groupId)
{
	if(!ms_activeGroups[groupId])
		return;
	ms_activeGroups[groupId] = false;
	ms_groups[groupId].Flush();
}

// 0x5FB8A0 (reconstructed)
void
CPedGroups::RemoveAllFollowersFromGroup(int32 groupId)
{
	GetGroup(groupId).RemoveAllFollowers();
}

// 0x5FB8C0 (reconstructed)
void
CPedGroups::Init()
{
	for(int32 i = 0; i < NUM_PED_GROUPS; i++){
		ms_activeGroups[i] = false;
		ms_groups[i].Flush();
		ScriptReferenceIndex[i] = 0;
	}
	ms_bIsPlayerOnAMission = false;
	ms_iNoOfPlayerKills = 0;
}

// 0x5F7E30
void
CPedGroups::RegisterKillByPlayer()
{
	if(ms_bIsPlayerOnAMission)
		ms_iNoOfPlayerKills++;
}

// 0x5FB930 (reconstructed)
void
CPedGroups::CleanUpForShutDown()
{
	for(int32 i = 0; i < NUM_PED_GROUPS; i++)
		RemoveGroup(i);
}

// 0x5F7E40 (reconstructed)
bool
CPedGroups::IsGroupLeader(CPed *ped)
{
	for(int32 i = 0; i < NUM_PED_GROUPS; i++){
		if(ms_activeGroups[i] && ms_groups[i].GetMembership().IsLeader(ped))
			return true;
	}
	return false;
}

// 0x5F7E80 (reconstructed)
CPedGroup*
CPedGroups::GetPedsGroup(const CPed *ped)
{
	for(int32 i = 0; i < NUM_PED_GROUPS; i++){
		if(ms_activeGroups[i] && ms_groups[i].GetMembership().IsMember(ped))
			return &ms_groups[i];
	}
	return nil;
}

// 0x5F7EE0 (reconstructed)
int32
CPedGroups::GetGroupId(const CPedGroup *pedGroup)
{
	for(int32 i = 0; i < NUM_PED_GROUPS; i++)
		if(&ms_groups[i] == pedGroup)
			return i;
	return -1;
}

static void
SetGroupsDecisionMakerType(eDecisionMakerType dm)
{
	for(int32 i = 0; i < CPedGroups::NUM_PED_GROUPS; i++){
		if(!CPedGroups::ms_activeGroups[i])
			continue;
		CPedGroup &group = CPedGroups::GetGroup(i);
		if(!group.m_bIsMissionGroup) // Mission groups keep the decision maker the script gave them
			group.GetIntelligence().SetGroupDecisionMakerType(dm);
	}
}

// 0x5FC800
void
CPedGroups::Process()
{
	for(int32 i = 0; i < NUM_PED_GROUPS; i++){
		if(!ms_activeGroups[i])
			continue;
		ms_groups[i].Process();
		if(!ms_groups[i].GetMembership().CountMembers())
			RemoveGroup(i);
	}

	if(CTheScripts::IsPlayerOnAMission() && !ms_bIsPlayerOnAMission){ // Player started a mission
		ms_iNoOfPlayerKills = 0;
		SetGroupsDecisionMakerType(DECISION_MAKER_GROUP_RANDOM_PASSIVE);
	}else if(!CTheScripts::IsPlayerOnAMission() && ms_bIsPlayerOnAMission){ // Player ended a mission
		ms_iNoOfPlayerKills = 0;
		SetGroupsDecisionMakerType(DECISION_MAKER_UNKNOWN);
	}else if(CTheScripts::IsPlayerOnAMission() && ms_iNoOfPlayerKills == 8){
		SetGroupsDecisionMakerType(DECISION_MAKER_UNKNOWN);
	}else if(CTheScripts::IsPlayerOnAMission()){
		SetGroupsDecisionMakerType(DECISION_MAKER_GROUP_RANDOM_PASSIVE);
	}

	const bool bWasOnAMission = ms_bIsPlayerOnAMission;
	ms_bIsPlayerOnAMission = CTheScripts::IsPlayerOnAMission();
	if(!bWasOnAMission && ms_bIsPlayerOnAMission)
		ms_iNoOfPlayerKills = 0;
}

// 0x5F7F40 (reconstructed): two groupless peds are not "in the same group"
bool
CPedGroups::AreInSameGroup(const CPed *ped1, const CPed *ped2)
{
	CPedGroup *group = GetPedsGroup(ped1);
	return group && group->GetMembership().IsMember(ped2);
}

// 0x5F7F10 (reconstructed)
bool
CPedGroups::IsInPlayersGroup(const CPed *ped)
{
	CPedGroup *group = GetPedsGroup(ped);
	if(!group)
		return false;
	CPed *leader = group->GetMembership().GetLeader();
	return leader && leader->IsPlayer();
}
