#pragma once

// SA CTaskComplexWanderCriminal: criminals wander and periodically look
// for cars worth stealing.

#include "TaskComplexWander.h"
#include "TaskTimer.h"

class CTaskComplexWanderCriminal : public CTaskComplexWander
{
public:
	CTaskTimer m_TaskTimer;
	uint32 m_nMinNextScanTime;

public:
	CTaskComplexWanderCriminal(eMoveState moveState, uint8 dir, bool bWanderSensibly = true);

	CTask *Clone() const { return new CTaskComplexWanderCriminal(m_nMoveState, m_nDir, m_bWanderSensibly); }
	eWanderType GetWanderType() { return WANDER_TYPE_CRIMINAL; }
	void ScanForStuff(CPed *ped);

	void LookForCarsToSteal(CPed *ped);
};

VALIDATE_SIZE(CTaskComplexWanderCriminal, 0x38);
