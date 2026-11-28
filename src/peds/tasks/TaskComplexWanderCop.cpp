#include "TaskComplexWanderCop.h"

#include "EventPedToChase.h"
#include "EventPedToFlee.h"
#include "Ped.h"
#include "PedIntelligence.h"
#include "PlayerInfo.h"
#include "PlayerPed.h"
#include "TaskSimpleStandStill.h"
#include "Timer.h"
#include "Vehicle.h"
#include "Wanted.h"
#include "World.h"

// SA CTaskComplexWanderCop. Adaptations for this codebase:
// - CTaskComplexPolicePursuit is not ported yet, so wherever SA would
//   start (or continue) a pursuit the cop keeps wandering instead; the
//   TASK_COMPLEX_POLICE_PURSUIT comparisons remain for the day it lands.
// - VC's CCopPed has no m_bDontPursuit flag, so SA's dont-pursuit
//   stand-still branch is absent.
// - Wanted levels are plain ints here (0 = clean).

CTaskComplexWanderCop::CTaskComplexWanderCop(eMoveState moveState, uint8 dir)
: CTaskComplexWander(moveState, dir, true, 0.5f)
{
	m_pGoToPointAndStandStillTask = nil;
	m_nTimePassedSinceLastLookedForCriminals = 0;
	m_nTimePassedSinceLastLookedForCarAlarmsAndStolenCopCars = 0;
	m_pLastCriminalPedLookedFor = nil;
	m_nScanForStuffTimer.m_bStarted = false;
	m_nSubTaskCreatedTimer.m_bStarted = false;
}

// 0x460D60
CTaskComplexWanderCop::~CTaskComplexWanderCop()
{
	delete m_pGoToPointAndStandStillTask;
}

// 0x674860
CTask*
CTaskComplexWanderCop::CreateNextSubTask(CPed *ped)
{
	if(ped->m_nPedType != PEDTYPE_COP)
		return CTaskComplexWander::CreateNextSubTask(ped);

	if(m_pSubTask->GetTaskType() == TASK_COMPLEX_POLICE_PURSUIT){
		m_nSubTaskCreatedTimer.m_nStartTime = CTimer::GetTimeInMilliseconds();
		m_nSubTaskCreatedTimer.m_nInterval = 3000;
		m_nSubTaskCreatedTimer.m_bStarted = true;
		if(m_pGoToPointAndStandStillTask)
			return m_pGoToPointAndStandStillTask->Clone();
		return CTaskComplexWander::CreateFirstSubTask(ped);
	}

	if(m_pGoToPointAndStandStillTask && m_pSubTask->GetTaskType() == m_pGoToPointAndStandStillTask->GetTaskType())
		return nil;

	if(m_pGoToPointAndStandStillTask)
		return m_pGoToPointAndStandStillTask->Clone();
	return CTaskComplexWander::CreateNextSubTask(ped);
}

// 0x674750
CTask*
CTaskComplexWanderCop::CreateFirstSubTask(CPed *ped)
{
	if(ped->m_nPedType != PEDTYPE_COP)
		return CTaskComplexWander::CreateFirstSubTask(ped);

	if(!ShouldPursuePlayer(ped)){
		if(m_pGoToPointAndStandStillTask)
			return m_pGoToPointAndStandStillTask->Clone();
		return CTaskComplexWander::CreateFirstSubTask(ped);
	}

	// SA returns CTaskComplexPolicePursuit here (not ported yet).
	return CTaskComplexWander::CreateFirstSubTask(ped);
}

// 0x674D80
CTask*
CTaskComplexWanderCop::ControlSubTask(CPed *ped)
{
	if(ped->m_nPedType != PEDTYPE_COP)
		return CTaskComplexWander::ControlSubTask(ped);

	if(!ShouldPursuePlayer(ped)){
		if(m_pSubTask->GetTaskType() == TASK_COMPLEX_POLICE_PURSUIT || m_pGoToPointAndStandStillTask)
			return m_pSubTask;
		return CTaskComplexWander::ControlSubTask(ped);
	}

	if((m_nSubTaskCreatedTimer.m_bStarted && !m_nSubTaskCreatedTimer.IsOutOfTime()) || !m_pSubTask->MakeAbortable(ped))
		return m_pSubTask;

	// SA returns CTaskComplexPolicePursuit here (not ported yet).
	return m_pSubTask;
}

// 0x6702B0
void
CTaskComplexWanderCop::ScanForStuff(CPed *ped)
{
	if(!m_nScanForStuffTimer.m_bStarted){
		m_nScanForStuffTimer.m_nStartTime = CTimer::GetTimeInMilliseconds();
		m_nScanForStuffTimer.m_nInterval = 50;
		m_nScanForStuffTimer.m_bStarted = true;
	}

	if(GetTaskType() == TASK_COMPLEX_POLICE_PURSUIT || !m_nScanForStuffTimer.m_bStarted)
		return;

	if(m_nScanForStuffTimer.m_bStopped){
		m_nScanForStuffTimer.m_nStartTime = CTimer::GetTimeInMilliseconds();
		m_nScanForStuffTimer.m_bStopped = false;
	}

	if(CTimer::GetTimeInMilliseconds() >= (uint32)(m_nScanForStuffTimer.m_nStartTime + m_nScanForStuffTimer.m_nInterval)){
		m_nScanForStuffTimer.m_nInterval = 50;
		m_nScanForStuffTimer.m_nStartTime = CTimer::GetTimeInMilliseconds();
		m_nScanForStuffTimer.m_bStarted = true;

		if(CTimer::GetTimeInMilliseconds() >= m_nTimePassedSinceLastLookedForCarAlarmsAndStolenCopCars){
			LookForCarAlarms(ped);
			LookForStolenCopCars(ped);
		}

		if(CTimer::GetTimeInMilliseconds() >= m_nTimePassedSinceLastLookedForCriminals)
			LookForCriminals(ped);
	}
}

// 0x66B1B0
void
CTaskComplexWanderCop::LookForCarAlarms(CPed *ped)
{
	CVehicle *vehicle = FindPlayerVehicle();
	if(vehicle == nil || !vehicle->IsAutomobile())
		return;

	if(!vehicle->m_nAlarmState || vehicle->m_nAlarmState == -1 || vehicle->GetStatus() == STATUS_WRECKED)
		return;

	CVector dist = ped->GetPosition() - vehicle->GetPosition();
	if(dist.MagnitudeSqr() < 20.0f * 20.0f)
		FindPlayerPed()->SetWantedLevelNoDrop(1);
}

// 0x66B290
void
CTaskComplexWanderCop::LookForStolenCopCars(CPed *ped)
{
	(void)ped;

	CPlayerPed *player = FindPlayerPed();
	if(player == nil || player->Wanted() == nil)
		return;

	if(player->Wanted()->GetWantedLevel() == 0 && player->m_pMyVehicle){
		if(player->m_pMyVehicle->bIsLawEnforcer)
			player->SetWantedLevelNoDrop(1);
	}
}

// Sighting test extracted from SA's LookForCriminals (0x66B300).
CPed*
CTaskComplexWanderCop::FindFirstCriminalInSight(CPed *ped) const
{
	CEntity **entities = ped->GetIntelligence()->GetPedEntities();
	for(int i = 0; i < CEntityScanner::MAX_NUM_ENTITIES; i++){
		CPed *candidate = (CPed*)entities[i];
		if(candidate == nil)
			continue;

		bool isGangster = candidate->m_nPedType >= PEDTYPE_GANG1 && candidate->m_nPedType <= PEDTYPE_GANG8;
		if(!isGangster){
			if(candidate->m_nPedType != PEDTYPE_CRIMINAL || candidate == m_pLastCriminalPedLookedFor)
				continue;
		}

		CTask *active = candidate->GetIntelligence()->GetTaskManager().GetActiveTask();
		if(active == nil || active->GetTaskType() != GetTaskType())
			continue;

		CVector candidatePos = candidate->GetPosition();
		CVector pedPos = ped->GetPosition();
		CVector distance = candidatePos - pedPos;
		if(distance.MagnitudeSqr() >= 10.0f * 10.0f)
			continue;

		if(DotProduct(distance, ped->GetForward()) <= 0.0f)
			continue;

		if(!CWorld::GetIsLineOfSightClear(pedPos, candidatePos, true, false, false, true, false, false, false))
			continue;

		return candidate;
	}
	return nil;
}

// 0x66B300
void
CTaskComplexWanderCop::LookForCriminals(CPed *ped)
{
	CPed *criminal = FindFirstCriminalInSight(ped);
	if(criminal == nil)
		return;

	CEventPedToChase eventPedToChase(criminal);
	if(ped->GetIntelligence())
		ped->GetIntelligence()->GetEventGroup().Add(&eventPedToChase, false);

	CEventPedToFlee eventPedToFlee(ped);
	if(criminal->GetIntelligence())
		criminal->GetIntelligence()->GetEventGroup().Add(&eventPedToFlee, false);

	// 30 seconds wait for next check
	m_nTimePassedSinceLastLookedForCriminals = CTimer::GetTimeInMilliseconds() + 30000;
	m_pLastCriminalPedLookedFor = criminal;
}

// 0x66B160. SA additionally asks the wanted system whether this cop may
// join the pursuit (CanCopJoinPursuit); VC has no such gate.
bool
CTaskComplexWanderCop::ShouldPursuePlayer(CPed *ped)
{
	(void)ped;

	CWanted *wanted = FindPlayerPed()->Wanted();
	if(wanted->GetWantedLevel() == 0)
		return false;

	if(m_pSubTask && m_pSubTask->GetTaskType() == TASK_COMPLEX_POLICE_PURSUIT)
		return false;

	return true;
}
