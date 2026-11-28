#include "common.h"

#include "PedScanner.h"
#include "Entity.h"
#include "Ped.h"

// SA CPedScanner::ScanForPedsInRange (0x5FFEF0).
void
CPedScanner::ScanForPedsInRange(CPed &ped)
{
	if(ped.Dead())
		return;
	CEntityScanner::ScanForEntitiesInRange(REPEATSECTOR_PEDS, ped);
}

// SA CPedScanner::GetClosestPedInRange (0x5FFF20).
CPed*
CPedScanner::GetClosestPedInRange(void) const
{
	return (CPed*)m_pClosestEntityInRange;
}
