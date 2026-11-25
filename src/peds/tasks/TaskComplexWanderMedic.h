#ifndef __GTA_TASKCOMPLEXWANDERMEDIC_H__
#define __GTA_TASKCOMPLEXWANDERMEDIC_H__

#include "TaskComplexWander.h"

// SA CTaskComplexWanderMedic: medics wander like civilians but look for
// nothing - their real work arrives as events (injured peds), not scans.
class CTaskComplexWanderMedic : public CTaskComplexWander
{
public:
	CTaskComplexWanderMedic(eMoveState moveState, uint8 dir, bool bWanderSensibly = true)
		: CTaskComplexWander(moveState, dir, bWanderSensibly) {}

	eWanderType GetWanderType() { return WANDER_TYPE_MEDIC; }
	CTask *Clone() const { return new CTaskComplexWanderMedic(m_nMoveState, m_nDir, m_bWanderSensibly); }
	void ScanForStuff(CPed *ped) { (void)ped; }
};

#endif // __GTA_TASKCOMPLEXWANDERMEDIC_H__
