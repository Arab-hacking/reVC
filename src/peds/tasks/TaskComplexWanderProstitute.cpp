#include "TaskComplexWanderProstitute.h"

#include "Timer.h"

// SA CTaskComplexWanderProstitute (0x672690).
CTaskComplexWanderProstitute::CTaskComplexWanderProstitute(eMoveState moveState, uint8 dir, bool bWanderSensibly)
: CTaskComplexWanderStandard(moveState, dir, bWanderSensibly)
{
	m_nStartTimeInMs = CTimer::GetTimeInMilliseconds();
}

// SA CTaskComplexWanderProstitute::ScanForStuff is not reversed upstream
// (plugin call); it looks for clients among nearby cars, which needs the
// seek-car task chain. NOP until then.
void
CTaskComplexWanderProstitute::ScanForStuff(CPed *ped)
{
	(void)ped;
}
