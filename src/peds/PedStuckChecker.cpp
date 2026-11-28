#include "common.h"

#include "PedStuckChecker.h"
#include "Ped.h"
#include "PedIntelligence.h"
#include "Task.h"

// Not yet reversed in gta-reversed (0x602C00); reconstructed from the
// field layout and its one caller. The checker remembers the last point
// the ped moved freely at: while the ped stays within m_radius of it and
// its active task wants it somewhere else, it counts as stuck. With the
// default radius of 0 any displacement at all counts as movement, so the
// checker then only fires for a ped that is perfectly stationary while
// trying to go somewhere.
bool
CPedStuckChecker::TestPedStuck(CPed *ped, CEventGroup *eventGroup)
{
	CVector dist = ped->GetPosition() - m_lastNonStuckPoint;
	if(dist.MagnitudeSqr() > (float)m_radius * (float)m_radius){
		m_lastNonStuckPoint = ped->GetPosition();
		m_state = m_state == PED_STUCK_STATE_STUCK ? PED_STUCK_STATE_WAS_STUCK : PED_STUCK_STATE_NONE;
		return false;
	}

	CTask *simplest = ped->m_pIntelligence->GetTaskManager().GetSimplestActiveTask();
	if(simplest && CTask::IsGoToTask(simplest)){
		m_state = PED_STUCK_STATE_STUCK;
		return true;
	}

	m_state = PED_STUCK_STATE_NONE;
	return false;
}
