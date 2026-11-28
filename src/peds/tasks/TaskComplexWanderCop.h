#pragma once

// SA CTaskComplexWanderCop: a wandering cop additionally watches for
// criminals in sight, sounding car alarms and the player in a stolen
// police car, and joins the pursuit when the wanted level allows.
// The pursuit task itself (CTaskComplexPolicePursuit) is not ported
// yet, so the pursuit branches currently keep the cop wandering.

#include "TaskComplexWander.h"
#include "TaskComplexGoToPointAndStandStill.h"
#include "TaskTimer.h"

class CTaskComplexWanderCop : public CTaskComplexWander
{
public:
	CTaskComplexGoToPointAndStandStill *m_pGoToPointAndStandStillTask;
	CTaskTimer m_nScanForStuffTimer;
	uint32 m_nTimePassedSinceLastLookedForCriminals;
	uint32 m_nTimePassedSinceLastLookedForCarAlarmsAndStolenCopCars;
	CPed *m_pLastCriminalPedLookedFor;
	CTaskTimer m_nSubTaskCreatedTimer;

public:
	CTaskComplexWanderCop(eMoveState moveState, uint8 dir);
	~CTaskComplexWanderCop();

	CTask *Clone() const { return new CTaskComplexWanderCop(m_nMoveState, m_nDir); }
	eWanderType GetWanderType() { return WANDER_TYPE_COP; }

	CTask *CreateNextSubTask(CPed *ped);
	CTask *CreateFirstSubTask(CPed *ped);
	CTask *ControlSubTask(CPed *ped);
	void ScanForStuff(CPed *ped);

	void LookForCarAlarms(CPed *ped);
	void LookForStolenCopCars(CPed *ped);
	void LookForCriminals(CPed *ped);
	bool ShouldPursuePlayer(CPed *ped);

private:
	CPed *FindFirstCriminalInSight(CPed *ped) const;
};

VALIDATE_SIZE(CTaskComplexWanderCop, 0x50);
