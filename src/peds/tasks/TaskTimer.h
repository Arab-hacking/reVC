#pragma once

#include "common.h"
#include "Timer.h"

// SA CTaskTimer: a millisecond timer tasks use for delays and timeouts.
// (SA reads CTimer::GetTimeInMS; the VC timer calls it
// GetTimeInMilliseconds, same clock.)
class CTaskTimer
{
public:
	uint32 m_nStartTime;
	int32 m_nInterval;
	bool m_bStarted;
	bool m_bStopped;

public:
	CTaskTimer() : m_nStartTime(0), m_nInterval(0), m_bStarted(false), m_bStopped(false) {}

	// 0x420E10
	CTaskTimer(uint32 startTimeMs, uint32 interval)
		: m_nStartTime(startTimeMs), m_nInterval((int32)interval), m_bStarted(true), m_bStopped(false)
	{
	}

	bool Start(int32 interval)
	{
		if(interval >= 0){
			m_nStartTime = CTimer::GetTimeInMilliseconds();
			m_nInterval = interval;
			m_bStarted = true;
			return true;
		}
		return false;
	}

	void StartIfNotAlready(int32 interval)
	{
		if(!m_bStarted){
			m_nStartTime = CTimer::GetTimeInMilliseconds();
			m_nInterval = interval;
			m_bStarted = true;
		}
	}

	bool Pause()
	{
		if(m_bStarted){
			m_bStopped = true;
			m_nInterval -= CTimer::GetTimeInMilliseconds() - m_nStartTime;
			return true;
		}
		return false;
	}

	void Stop()
	{
		m_bStarted = false;
	}

	// Make the timer be finished immediately (IsOutOfTime returns true)
	void SetOutOfTime()
	{
		m_nStartTime = CTimer::GetTimeInMilliseconds();
		m_nInterval = -1;
		m_bStarted = true;
	}

	bool Reset()
	{
		if(m_bStarted){
			if(m_bStopped){
				m_nStartTime = CTimer::GetTimeInMilliseconds();
				m_bStopped = false;
			}
			return true;
		}
		return false;
	}

	void SetAsOutOfTime()
	{
		m_nStartTime = CTimer::GetTimeInMilliseconds();
		m_nInterval = -1;
		m_bStarted = true;
	}

	bool IsStarted() const { return m_bStarted; }

	bool IsOutOfTime();
};

VALIDATE_SIZE(CTaskTimer, 0xC);
