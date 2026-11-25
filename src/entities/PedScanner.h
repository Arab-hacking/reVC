#ifndef __GTA_PEDSCANNER_H__
#define __GTA_PEDSCANNER_H__

#include "EntityScanner.h"

class CPed;

// SA CPedScanner: the scanner a ped's intelligence uses to keep track of the
// peds around it - threats, friends, targets. A dead ped scans nothing.
class CPedScanner : public CEntityScanner
{
public:
	void ScanForPedsInRange(CPed &ped);

	CPed *GetClosestPedInRange(void) const;
};

#endif // __GTA_PEDSCANNER_H__
