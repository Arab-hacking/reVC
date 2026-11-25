#pragma once

#include "EntityScanner.h"

class CPed;

// SA CPedScanner: scans for peds in the owner's hearing/seeing range.
class CPedScanner : public CEntityScanner
{
public:
	void ScanForPedsInRange(CPed &ped);
	CPed *GetClosestPedInRange() const;
};
