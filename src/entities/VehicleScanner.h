#ifndef __GTA_VEHICLESCANNER_H__
#define __GTA_VEHICLESCANNER_H__

#include "EntityScanner.h"

class CPed;
class CVehicle;

// SA CVehicleScanner: the scanner a ped's intelligence uses when it is looking
// for cars - to jack one, to flee from one, to get into one.
//
// The interesting part is the in-vehicle case: a ped sitting in a car already
// knows which vehicle matters, so the scanner points straight at it and skips
// the sector walk entirely, unless the ped belongs to a mission, where the
// surroundings still have to be tracked so a cutscene or a chase can find
// them. That asymmetry is why this is a class and not a flag.
class CVehicleScanner : public CEntityScanner
{
public:
	// Fill the scan with the vehicles near `ped`, or with the vehicle the ped
	// is sitting in.
	void ScanForVehiclesInRange(CPed &ped);

	CVehicle *GetClosestVehicleInRange(void) const;
};

#endif // __GTA_VEHICLESCANNER_H__
