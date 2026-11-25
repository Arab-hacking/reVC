#pragma once

#include "common.h"
#include "Vector.h"

class CPed;
class CEventGroup;

enum ePedStuckState
{
	PED_STUCK_STATE_NONE = 0,
	PED_STUCK_STATE_STUCK = 1,
	PED_STUCK_STATE_WAS_STUCK = 2
};

// SA CPedStuckChecker: watches whether a ped that wants to move is
// actually getting anywhere. CPedIntelligence::ProcessFirst consults it
// before running collision scanning.
class CPedStuckChecker
{
public:
	CVector m_lastNonStuckPoint;
	uint16 m_radius;
	ePedStuckState m_state;

public:
	CPedStuckChecker() : m_radius(0), m_state(PED_STUCK_STATE_NONE) {}

	bool TestPedStuck(CPed *ped, CEventGroup *eventGroup);

	ePedStuckState GetState() const { return m_state; }
};

VALIDATE_SIZE(CPedStuckChecker, 0x10);
