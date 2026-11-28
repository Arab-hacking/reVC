#include "common.h"

#include "TaskSimplePause.h"

// SA CTaskSimplePause::CTaskSimplePause (0x48E750).
CTaskSimplePause::CTaskSimplePause(int32 pauseIntervalMs)
	: m_PauseIntervalMs(pauseIntervalMs)
{
}

// SA CTaskSimplePause::CTaskSimplePause (0x48E780).
CTaskSimplePause::CTaskSimplePause(const CTaskSimplePause &other)
	: CTaskSimplePause(other.m_PauseIntervalMs)
{
}

// SA CTaskSimplePause::MakeAbortable (0x48E810): a pause gives up at once.
bool
CTaskSimplePause::MakeAbortable(CPed *ped, eAbortPriority priority, const CEvent *event)
{
	(void)ped; (void)priority; (void)event;
	m_Timer.SetAsOutOfTime();
	return true;
}

// SA CTaskSimplePause::ProcessPed (0x48E830).
bool
CTaskSimplePause::ProcessPed(CPed *ped)
{
	(void)ped;
	if(!m_Timer.IsStarted() && !m_Timer.Start(m_PauseIntervalMs))
		return false;
	m_Timer.Reset();
	return m_Timer.IsOutOfTime();
}
