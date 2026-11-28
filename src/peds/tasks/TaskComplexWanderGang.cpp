#include "TaskComplexWanderGang.h"

#include "Timer.h"

// SA CTaskComplexWanderGang (0x66F5C0).
CTaskComplexWanderGang::CTaskComplexWanderGang(eMoveState moveState, uint8 dir, uint32 scanTime, bool bWanderSensibly, float fTargetRadius)
: CTaskComplexWander(moveState, dir, bWanderSensibly, fTargetRadius)
{
	m_NextScanTime = scanTime;
	m_TaskTimer.m_nInterval = (int32)scanTime;
	m_TaskTimer.m_nStartTime = CTimer::GetTimeInMilliseconds();
	m_TaskTimer.m_bStarted = true;
}

// SA CTaskComplexWanderGang::ScanForStuff (0x66F640): NOP.
void
CTaskComplexWanderGang::ScanForStuff(CPed *ped)
{
	(void)ped;
}
