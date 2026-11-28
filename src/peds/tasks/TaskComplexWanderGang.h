#pragma once

// SA CTaskComplexWanderGang: gang members wander like everyone else but
// never scan for stuff; the timer only gates gang recruitment
// (CanJoinGang). ScanForStuff is a NOP in SA too.

#include "TaskComplexWander.h"
#include "TaskTimer.h"

class CTaskComplexWanderGang : public CTaskComplexWander
{
public:
	CTaskTimer m_TaskTimer;
	uint32 m_NextScanTime;

public:
	CTaskComplexWanderGang(eMoveState moveState, uint8 dir, uint32 scanTime, bool bWanderSensibly, float fTargetRadius);

	CTask *Clone() const { return new CTaskComplexWanderGang(m_nMoveState, m_nDir, m_NextScanTime, m_bWanderSensibly, 0.5f); }
	eWanderType GetWanderType() { return WANDER_TYPE_GANG; }
	void ScanForStuff(CPed *ped);

	bool CanJoinGang() { return m_TaskTimer.IsOutOfTime(); }
};

VALIDATE_SIZE(CTaskComplexWanderGang, 0x38);
