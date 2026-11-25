#pragma once

#include "EntityScanner.h"

class CPed;
class CVehicle;

// SA CVehicleScanner: scans for vehicles in the owner's range.
class CVehicleScanner : public CEntityScanner
{
public:
	void ScanForVehiclesInRange(CPed &ped);
	CVehicle *GetClosestVehicleInRange() const;
};
