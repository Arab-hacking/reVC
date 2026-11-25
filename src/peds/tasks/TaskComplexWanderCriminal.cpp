#include "TaskComplexWanderCriminal.h"

#include "General.h"
#include "Timer.h"

// SA CTaskComplexWanderCriminal. LookForCarsToSteal is not reversed
// upstream and needs the car-stealing task chain (enter car as driver),
// which is not ported yet; the scan cadence below is reconstructed in
// the same shape as the other wander scans.

CTaskComplexWanderCriminal::CTaskComplexWanderCriminal(eMoveState moveState, uint8 dir, bool bWanderSensibly)
: CTaskComplexWander(moveState, dir, bWanderSensibly, 0.5f)
{
	m_nMinNextScanTime = 0;
	m_TaskTimer.m_bStarted = false;
}

// SA CTaskComplexWanderCriminal::ScanForStuff (0x66B430 in SA numbering):
// a 50 ms tick that fires LookForCarsToSteal once the (randomised) gate
// opens.
void
CTaskComplexWanderCriminal::ScanForStuff(CPed *ped)
{
	if(!m_TaskTimer.m_bStarted){
		m_nMinNextScanTime = CTimer::GetTimeInMilliseconds() + (uint32)CGeneral::GetRandomNumberInRange(0, 30000);
		m_TaskTimer.Start(50);
	}

	if(m_TaskTimer.m_bStopped){
		m_TaskTimer.m_nStartTime = CTimer::GetTimeInMilliseconds();
		m_TaskTimer.m_bStopped = false;
	}

	if(CTimer::GetTimeInMilliseconds() >= (uint32)(m_TaskTimer.m_nStartTime + m_TaskTimer.m_nInterval)){
		m_TaskTimer.m_nStartTime = CTimer::GetTimeInMilliseconds();
		m_TaskTimer.m_nInterval = 50;
		m_TaskTimer.m_bStarted = true;
		if(CTimer::GetTimeInMilliseconds() >= m_nMinNextScanTime)
			LookForCarsToSteal(ped);
	}
}

// Not reversed upstream (plugin call in SA); car stealing needs the
// enter-car tasks. Until then a scan only re-arms the gate.
void
CTaskComplexWanderCriminal::LookForCarsToSteal(CPed *ped)
{
	(void)ped;
	m_nMinNextScanTime = CTimer::GetTimeInMilliseconds() + 5000;
}
