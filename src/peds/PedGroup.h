#pragma once

// SA CPedGroup: one ped group - its membership, its group
// intelligence, and the queries the rest of the game asks about it.
// The player-command and teleport helpers (PlayerGaveCommand_Attack/
// Gather, Teleport) are not ported yet: they need CEventGroupEvent,
// CFormation/CPedList and the follow-leader-formation offsets.

#include "PedGroupIntelligence.h"
#include "PedGroupMembership.h"

class CVector;
class CPed;
class CVehicle;

class CPedGroup
{
public:
	CPedGroup();
	~CPedGroup() {}

	// Find follower closest to the leader
	float FindDistanceToNearestMember(CPed **ppOutNearestMember);

	// Clear state
	void Flush();

	// Find member closest to ped
	CPed *GetClosestGroupPed(CPed *ped, float *pOutDistance);

	// Find distance of the furthest follower to the leader
	float FindDistanceToFurthestMember();

	// Is anyone from this group using the given car
	bool IsAnyoneUsingCar(const CVehicle *vehicle);

	// Update routine
	void Process();

	// Remove all followers of the group (all members, excluding the leader)
	void RemoveAllFollowers();

	// Get id of this group
	int32 GetId() const;
	bool IsActive() const;

	CPedGroupIntelligence &GetIntelligence() { return m_groupIntelligence; }
	CPedGroupMembership &GetMembership() { return m_groupMembership; }
	const CPedGroupMembership &GetMembership() const { return m_groupMembership; }

public:
	CPed *m_pPed;
	bool m_bMembersEnterLeadersVehicle;
	CPedGroupMembership m_groupMembership;
	CPedGroupIntelligence m_groupIntelligence;
	bool m_bIsMissionGroup;
};

VALIDATE_SIZE(CPedGroup, 0x2D4);
