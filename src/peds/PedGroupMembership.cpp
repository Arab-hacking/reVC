#include "common.h"

#include "PedGroupMembership.h"

#include "DecisionMakerTypes.h"
#include "General.h"
#include "Ped.h"
#include "PedGroup.h"
#include "PedIntelligence.h"

// SA CPedGroupMembership, adapted to this codebase (see the header).

CPedGroupMembership::CPedGroupMembership(CPedGroup &group)
: m_group(&group)
{
	for(int32 i = 0; i < TOTAL_PED_GROUP_MEMBERS; i++)
		m_members[i] = nil;
	m_separationRange = 60.0f;
}

CPedGroupMembership::~CPedGroupMembership()
{
	Flush();
}

// 0x5F8020
void
CPedGroupMembership::AddFollower(CPed *ped)
{
	// Peds in the player's group can't drown
	if(CPed *leader = GetLeader()){
		if(leader->IsPlayer())
			ped->bDrownsInWater = false;
	}

	if(IsFollower(ped) || GetLeader() == ped)
		return;

	const int32 memId = FindIdForNewMember();
	assert(memId != -1);
	if(memId == -1)
		return;

	AddMember(ped, memId);
	// SA also hands the ped a random object to hold; there is no
	// CPed::GiveObjectToPedToHold in this codebase yet.
}

// 0x5F6AE0
void
CPedGroupMembership::AddMember(CPed *member, int32 memIdx)
{
	assert(member);
	assert(!m_members[memIdx]);

	m_members[memIdx] = member;
	member->RegisterReference((CEntity**)&m_members[memIdx]);

	if(!member->IsPlayer())
		member->GetIntelligence()->SetPedDecisionMakerType(DECISION_MAKER_PED_GROUPMEMBER);
}

// 0x5FB240
void
CPedGroupMembership::AppointNewLeader()
{
	if(HasLeader())
		return;

	const int32 memId = FindNewLeaderToAppoint();
	if(memId == -1)
		return;

	CPed *leader = m_members[memId];
	RemoveMember(memId); // Must call as it does some cleanup
	AddMember(leader, LEADER_MEM_ID);
}

// 0x5F6A50
int32
CPedGroupMembership::CountMembers()
{
	int32 count = 0;
	for(int32 i = 0; i < TOTAL_PED_GROUP_MEMBERS; i++)
		if(m_members[i])
			count++;
	return count;
}

// 0x5F6AA0
int32
CPedGroupMembership::CountMembersExcludingLeader()
{
	const int32 count = CountMembers();
	return HasLeader() ? count - 1 : count;
}

// 0x5FB160
void
CPedGroupMembership::Flush()
{
	for(int32 i = 0; i < TOTAL_PED_GROUP_MEMBERS; i++)
		if(m_members[i])
			RemoveMember(i);
}

// 0x5F69A0
CPed*
CPedGroupMembership::GetLeader() const
{
	return m_members[LEADER_MEM_ID];
}

// 0x5F69B0
CPed*
CPedGroupMembership::GetMember(int32 memberId)
{
	return m_members[memberId];
}

// 0x5F69D0-ish
int32
CPedGroupMembership::GetMemberId(const CPed *ped) const
{
	for(int32 i = 0; i < TOTAL_PED_GROUP_MEMBERS; i++)
		if(m_members[i] == ped)
			return i;
	return -1;
}

// 0x5F69E0
bool
CPedGroupMembership::IsFollower(const CPed *ped) const
{
	return !IsLeader(ped) && IsMember(ped);
}

// 0x5F69C0
bool
CPedGroupMembership::IsLeader(const CPed *ped) const
{
	return ped && GetLeader() == ped;
}

// 0x5F6A10
bool
CPedGroupMembership::IsMember(const CPed *ped) const
{
	if(!ped)
		return false;
	for(int32 i = 0; i < TOTAL_PED_GROUP_MEMBERS; i++)
		if(m_members[i] == ped)
			return true;
	return false;
}

// 0x5FBA60
void
CPedGroupMembership::Process()
{
	// Remove dead members (except player ped)
	for(int32 i = 0; i < TOTAL_PED_GROUP_MEMBERS; i++){
		CPed *mem = m_members[i];
		if(!mem || mem->m_fHealth > 0.0f) // mem->IsAlive()
			continue;
		if(IsLeader(mem) && mem->IsPlayer()) // Player always stays in its group
			continue;
		RemoveMember(i);
	}

	// If no leader, try appointing a new one
	if(!HasLeader()){
		AppointNewLeader();
		if(!HasLeader()) // Couldn't appoint a new leader
			return;
	}

	// Now that we have a leader, check for separation distance and remove members further than it
	const CVector &leaderPos = GetLeader()->GetPosition();
	for(int32 i = 0; i < TOTAL_PED_GROUP_MEMBERS; i++){
		CPed *mem = m_members[i];
		if(!mem || IsLeader(mem)) // Leader's distance to itself always 0, thus ignore
			continue;
		if(m_separationRange * m_separationRange >= (leaderPos - mem->GetPosition()).MagnitudeSqr())
			continue;
		RemoveMember(i); // Ped is too far
	}
}

// 0x5FB190
void
CPedGroupMembership::RemoveAllFollowers(bool bCreatedByGameOnly)
{
	for(int32 i = 0; i < TOTAL_PED_GROUP_MEMBERS; i++){
		CPed *mem = m_members[i];
		if(!mem || IsLeader(mem)) // Leader isn't a follower
			continue;
		if(bCreatedByGameOnly && mem->CharCreatedBy == MISSION_CHAR)
			continue;
		RemoveMember(i);
	}
}

// 0x5FB1D0
void
CPedGroupMembership::RemoveNFollowers(int32 count)
{
	if(count == 0) // Nothing to do
		return;
	for(int32 i = 0; i < TOTAL_PED_GROUP_MEMBERS; i++){
		CPed *mem = m_members[i];
		if(!mem || IsLeader(mem)) // Leader isn't a follower
			continue;
		if(mem->CharCreatedBy == MISSION_CHAR)
			continue;
		RemoveMember(i);
		if(!--count)
			break;
	}
}

// 0x5F80D0
void
CPedGroupMembership::RemoveMember(int32 memIdx)
{
	CPed *mem = m_members[memIdx];
	assert(mem);

	mem->CleanUpOldReference((CEntity**)&m_members[memIdx]); // Sets the slot to nil

	if(mem->IsPlayer())
		return;

	mem->GetIntelligence()->RestorePedDecisionMakerType();

	if(CPed *leader = GetLeader()){
		if(leader->IsPlayer())
			mem->bDrownsInWater = true;
	}
}

// 0x5FB210
void
CPedGroupMembership::RemoveMember(CPed *ped)
{
	const int32 id = GetMemberId(ped);
	if(id != -1)
		RemoveMember(id);
}

// 0x5FB9C0
void
CPedGroupMembership::SetLeader(CPed *ped)
{
	assert(ped);

	// Remove member (just in case he's in the group already)
	RemoveMember(ped);

	// Remove the current leader (if any)
	if(GetLeader())
		RemoveMember(GetLeader());

	// Now add member as the leader
	AddMember(ped, LEADER_MEM_ID);
	// SA also hands the ped a random object to hold; not ported yet.
}

CPed*
CPedGroupMembership::GetMemberClosestTo(CPed *ped, float *pOutDistSq, bool includeLeader)
{
	const CVector &pedPos = ped->GetPosition();
	float closestDistSq = 3.4028235e38f; // FLT_MAX
	CPed *closest = nil;
	const int32 numSlots = includeLeader ? TOTAL_PED_GROUP_MEMBERS : TOTAL_PED_GROUP_FOLLOWERS;
	for(int32 i = 0; i < numSlots; i++){
		CPed *mem = m_members[i];
		if(!mem || mem == ped)
			continue;
		const float distSq = (pedPos - mem->GetPosition()).MagnitudeSqr();
		if(closestDistSq > distSq){
			closestDistSq = distSq;
			closest = mem;
		}
	}
	if(pOutDistSq)
		*pOutDistSq = closestDistSq;
	return closest;
}

CPed*
CPedGroupMembership::FindClosestFollowerToLeader(float *pOutDistSq)
{
	if(CPed *leader = GetLeader())
		return GetMemberClosestTo(leader, pOutDistSq);
	if(pOutDistSq)
		*pOutDistSq = 0.0f;
	return nil;
}

CPed*
CPedGroupMembership::GetFirstAvailableMember()
{
	for(int32 i = 0; i < TOTAL_PED_GROUP_MEMBERS; i++)
		if(m_members[i])
			return m_members[i];
	return nil;
}

int32
CPedGroupMembership::FindNewLeaderToAppoint() const
{
	for(int32 i = 0; i < TOTAL_PED_GROUP_MEMBERS; i++)
		if(m_members[i])
			return i;
	return -1;
}

int32
CPedGroupMembership::FindIdForNewMember() const
{
	for(int32 i = 0; i < TOTAL_PED_GROUP_MEMBERS; i++)
		if(!m_members[i])
			return i;
	return -1;
}

// NOTSA (SA uses CGeneral::RandomChoice over the member slots)
CPed*
CPedGroupMembership::GetRandom()
{
	const int32 count = CountMembers();
	if(count == 0)
		return nil;
	int32 pick = CGeneral::GetRandomNumberInRange(0, count);
	for(int32 i = 0; i < TOTAL_PED_GROUP_MEMBERS; i++){
		if(m_members[i] && pick-- == 0)
			return m_members[i];
	}
	return nil;
}

bool
CPedGroupMembership::CanAddFollower()
{
	return TOTAL_PED_GROUP_MEMBERS - 1 >= CountMembers(); // - 1 to compensate for leader
}
