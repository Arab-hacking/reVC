#pragma once

// SA CPedGroupMembership: the slots of one ped group - followers in
// slots 0..6, the leader in slot 7 (the array is not contiguous; empty
// slots are nil). Members are registered references, so a member that
// dies or is removed from the world drops out of its slot on its own.
// While in a group a ped thinks with the group-member decision maker;
// leaving restores whatever it had before.
//
// Adaptations for this codebase: VC's CEntity::RegisterReference member
// functions instead of SA's statics, health > 0 stands in for
// CPed::IsAlive, CharCreatedBy/MISSION_CHAR for SA's PED_MISSION, and
// the SA extras that have no local counterpart yet are dropped - the
// random object-to-hold (CPed::GiveObjectToPedToHold), the radar-blip
// clear on removal, and the bNeverLeavesGroup/bHasGroupDriveTask ped
// flags.

class CPed;
class CPedGroup;

const int32 TOTAL_PED_GROUP_MEMBERS = 8;
// -1 because every ped group has a leader, and the leader cannot be a follower
const int32 TOTAL_PED_GROUP_FOLLOWERS = TOTAL_PED_GROUP_MEMBERS - 1;

class CPedGroupMembership
{
public:
	static const int32 LEADER_MEM_ID = TOTAL_PED_GROUP_MEMBERS - 1; // leader's member ID

public:
	explicit CPedGroupMembership(CPedGroup &group);
	~CPedGroupMembership();

	// Rockstar's implementation of operator= (0x5F7FE0)
	void From(const CPedGroupMembership &obj) { *this = obj; }

	// Add a new follower to the group. If there's no more room, they aren't added
	void AddFollower(CPed *ped);

	// Set a new leader (should be called only if there is no leader)
	void AppointNewLeader();

	// Count of members
	int32 CountMembers();

	// Count of followers
	int32 CountMembersExcludingLeader();

	void Flush();

	// Update the states
	void Process();

	CPed *GetLeader() const;
	void SetLeader(CPed *ped);
	bool HasLeader() const { return GetLeader() != nil; }
	bool IsLeader(const CPed *ped) const;
	CPed *GetMember(int32 memberId);
	int32 GetMemberId(const CPed *ped) const;
	bool IsFollower(const CPed *ped) const;
	bool IsMember(const CPed *ped) const;

	// Remove all followers of this group (all members except the leader)
	void RemoveAllFollowers(bool bCreatedByGameOnly);
	void RemoveMember(int32 memberID);
	void RemoveMember(CPed *ped);
	void RemoveNFollowers(int32 count);

	// Get a random ped from the group. Might return nil.
	CPed *GetRandom();

	// Whether AddFollower can be called to add a new follower
	bool CanAddFollower();

	// Closest member to the ped (may be nil, in which case the distance
	// is invalid), and its squared distance from the ped
	CPed *GetMemberClosestTo(CPed *ped, float *pOutDistSq, bool includeLeader = true);
	CPed *FindClosestFollowerToLeader(float *pOutDistSq);
	CPed *GetFirstAvailableMember();

private:
	int32 FindNewLeaderToAppoint() const;
	int32 FindIdForNewMember() const;

	// Add a member by placing it at the given index in the m_members array
	void AddMember(CPed *member, int32 memIdx);

private:
	CPedGroup *m_group;
	CPed *m_members[TOTAL_PED_GROUP_MEMBERS]; // m_members[LEADER_MEM_ID] is the leader
	float m_separationRange;
};

VALIDATE_SIZE(CPedGroupMembership, 0x28);
