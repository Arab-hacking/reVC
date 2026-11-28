#include "common.h"

#include "PedGroup.h"

#include "Ped.h"
#include "PedGroups.h"
#include "PedIntelligence.h"
#include "Vehicle.h"
#include "math/Vector.h"

// SA CPedGroup, adapted to this codebase (see the header).

CPedGroup::CPedGroup()
: m_groupMembership(*this), m_groupIntelligence(this)
{
	m_pPed = nil;
	m_bMembersEnterLeadersVehicle = true;
	m_bIsMissionGroup = false;
}

// 0x5FB010: distance of the furthest follower from the leader
// (0.0f if there's no leader or no followers)
float
CPedGroup::FindDistanceToFurthestMember()
{
	CPed *leader = GetMembership().GetLeader();
	if(!leader)
		return 0.0f;

	float furthest = 0.0f;
	for(int32 i = 0; i < TOTAL_PED_GROUP_FOLLOWERS; i++){
		CPed *follower = GetMembership().GetMember(i);
		if(!follower)
			continue;
		const float dist = (follower->GetPosition() - leader->GetPosition()).Magnitude();
		if(dist > furthest)
			furthest = dist;
	}
	return furthest;
}

// 0x5FB0A0
float
CPedGroup::FindDistanceToNearestMember(CPed **ppOutNearestMember)
{
	float distSq;
	CPed *nearest = GetMembership().FindClosestFollowerToLeader(&distSq);
	if(nearest){
		if(ppOutNearestMember)
			*ppOutNearestMember = nearest;
		return sqrtf(distSq);
	}
	return 1.0e10f;
}

// 0x5FACD0
CPed*
CPedGroup::GetClosestGroupPed(CPed *ped, float *pOutDistSq)
{
	return GetMembership().GetMemberClosestTo(ped, pOutDistSq);
}

// 0x5FB790
void
CPedGroup::Flush()
{
	m_groupMembership.Flush();
	m_groupIntelligence.Flush();
	m_bIsMissionGroup = false;
}

// 0x5F7DB0
bool
CPedGroup::IsAnyoneUsingCar(const CVehicle *vehicle)
{
	assert(vehicle);

	for(int32 i = 0; i < TOTAL_PED_GROUP_MEMBERS; i++){
		CPed *mem = GetMembership().GetMember(i);
		if(!mem)
			continue;
		if(mem->bInVehicle && mem->m_pMyVehicle == vehicle)
			return true;
		if(mem->GetIntelligence()->GetEnteringVehicle() == vehicle)
			return true;
	}
	return false;
}

// 0x5FC7E0 (group intelligence processing is deferred; see
// CPedGroupIntelligence::Process)
void
CPedGroup::Process()
{
	m_groupMembership.Process();
	m_groupIntelligence.Process();
}

// 0x5FB7D0
void
CPedGroup::RemoveAllFollowers()
{
	GetMembership().RemoveAllFollowers(false);
}

int32
CPedGroup::GetId() const
{
	return CPedGroups::GetGroupId(this);
}

bool
CPedGroup::IsActive() const
{
	const int32 id = GetId();
	return id >= 0 && CPedGroups::ms_activeGroups[id];
}
