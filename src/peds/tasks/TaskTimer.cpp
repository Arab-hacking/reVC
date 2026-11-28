#include "common.h"

#include "TaskTimer.h"
#include "Timer.h"

// 0x420E30
bool
CTaskTimer::IsOutOfTime()
{
	if(!m_bStarted)
		return false;

	if(m_bStopped){
		m_nStartTime = CTimer::GetTimeInMilliseconds();
		m_bStopped = false;
	}

	return CTimer::GetTimeInMilliseconds() >= (m_nStartTime + m_nInterval);
}
