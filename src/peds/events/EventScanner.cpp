#include "common.h"

#include "EventScanner.h"
#include "Ped.h"
#include "Timer.h"

uint32 CEventScanner::m_sDeadPedWalkingTimer;

// SA CEventScanner::CEventScanner (0x600D60).
CEventScanner::CEventScanner()
{
	m_nNextScanTime = 0;
}

// SA CEventScanner::Clear (0x600D80): forget pending scan state; the
// sub-scanners' timers restart on their next scan.
void
CEventScanner::Clear()
{
	m_nNextScanTime = 0;
}

// SA CEventScanner::ScanForEvents (0x607E30). Upstream has not reversed the
// throttling wrapper; reconstructed: event scanning runs at most every
// 100 ms per ped, staggered by ped so the whole crowd never scans on the
// same frame. The actual sub-scans live in ScanForEventsNow.
void
CEventScanner::ScanForEvents(CPed &ped)
{
	uint32 now = CTimer::GetTimeInMilliseconds();
	if(now < m_nNextScanTime)
		return;
	m_nNextScanTime = now + 100;
	ScanForEventsNow(ped, false);
}

// SA CEventScanner::ScanForEventsNow (0x6053D0): run every sub-scanner over
// the entities the ped's entity scanners collected. The sub-scanners turn
// proximity and acquaintance into events on the ped's event group; each
// lands together with the event classes it produces.
void
CEventScanner::ScanForEventsNow(const CPed &ped, bool bDontScan)
{
	(void)ped;
	if(bDontScan)
		return;
}
