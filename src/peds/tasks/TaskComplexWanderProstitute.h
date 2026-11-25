#pragma once

// SA CTaskComplexWanderProstitute: a standard wanderer; its ScanForStuff
// (looking for clients/cars) is not reversed upstream.

#include "TaskComplexWanderStandard.h"

class CTaskComplexWanderProstitute : public CTaskComplexWanderStandard
{
public:
	uint32 m_nStartTimeInMs;

public:
	CTaskComplexWanderProstitute(eMoveState moveState, uint8 dir, bool bWanderSensibly = true);

	CTask *Clone() const { return new CTaskComplexWanderProstitute(m_nMoveState, m_nDir, m_bWanderSensibly); }
	eWanderType GetWanderType() { return WANDER_TYPE_PROSTITUTE; }
	CTask *CreateFirstSubTask(CPed *ped) { return CTaskComplexWander::CreateFirstSubTask(ped); }
	void ScanForStuff(CPed *ped);
};

VALIDATE_SIZE(CTaskComplexWanderProstitute, 0x3C);
