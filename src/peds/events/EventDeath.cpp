#include "common.h"

#include "EventDeath.h"

#include "Timer.h"

CEventDeath::CEventDeath(bool bDrowning)
	: m_bDrowning(bDrowning), m_deathTimeInMs(CTimer::GetTimeInMilliseconds())
{
}

CEventDeath::CEventDeath(bool bDrowning, uint32 deathTimeInMs)
	: m_bDrowning(bDrowning), m_deathTimeInMs(deathTimeInMs)
{
}
