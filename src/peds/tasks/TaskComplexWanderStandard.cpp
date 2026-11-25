#include "common.h"

#include "TaskComplexWanderStandard.h"
#include "Timer.h"

// SA CTaskComplexWanderStandard::CTaskComplexWanderStandard (0x48E4F0).
CTaskComplexWanderStandard::CTaskComplexWanderStandard(eMoveState moveState, uint8 dir, bool bWanderSensibly)
	: CTaskComplexWander(moveState, dir, bWanderSensibly)
	, m_nMinNextScanTime(0)
{
}

// SA CTaskComplexWanderStandard::ScanForStuff (0x672600): a 50 ms scan
// tick, gated by a per-ped minimum interval so the whole crowd does not
// scan on the same frame.
void
CTaskComplexWanderStandard::ScanForStuff(CPed *ped)
{
	if(!m_TaskTimer.m_bStarted){
		m_TaskTimer.m_nStartTime = CTimer::GetTimeInMilliseconds();
		m_TaskTimer.m_nInterval  = 50;
		m_TaskTimer.m_bStarted   = true;
	}

	if(CTimer::GetTimeInMilliseconds() < m_nMinNextScanTime)
		return;

	if(m_TaskTimer.m_bStopped){
		m_TaskTimer.m_nStartTime = CTimer::GetTimeInMilliseconds();
		m_TaskTimer.m_bStopped   = false;
	}

	if(CTimer::GetTimeInMilliseconds() >= m_TaskTimer.m_nStartTime + (uint32)m_TaskTimer.m_nInterval){
		m_TaskTimer.m_nInterval  = 50;
		m_TaskTimer.m_nStartTime = CTimer::GetTimeInMilliseconds();
		m_TaskTimer.m_bStarted   = true;
		if(!LookForGangMembers(ped) && !LookForChatPartners(ped))
			LookForSexyCars(ped);
		SetNextMinScanTime(ped);
	}
}

// The LookFor* scans (0x66AED0 WillChat, 0x66AFD0 LookForSexyCars,
// 0x66FDA0 LookForChatPartners, 0x670100 LookForGangMembers) are not yet
// reversed upstream; until they are, a wandering civilian spots nothing
// and just keeps walking.
bool
CTaskComplexWanderStandard::WillChat(const CPed &first, const CPed &second)
{
	(void)first; (void)second;
	return false;
}

// SA CTaskComplexWanderStandard::SetNextMinScanTime (0x66AF60). Also
// unreversed; reconstructed as a flat five-second gate, which is what
// the field exists for.
void
CTaskComplexWanderStandard::SetNextMinScanTime(CPed *ped)
{
	(void)ped;
	m_nMinNextScanTime = CTimer::GetTimeInMilliseconds() + 5000;
}

bool
CTaskComplexWanderStandard::LookForSexyCars(CPed *ped)
{
	(void)ped;
	return false;
}

bool
CTaskComplexWanderStandard::LookForChatPartners(CPed *ped)
{
	(void)ped;
	return false;
}

bool
CTaskComplexWanderStandard::LookForGangMembers(CPed *ped)
{
	(void)ped;
	return false;
}
