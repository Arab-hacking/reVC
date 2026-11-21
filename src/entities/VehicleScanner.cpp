#include "common.h"

#include "VehicleScanner.h"
#include "Entity.h"
#include "Ped.h"
#include "Vehicle.h"

// SA CVehicleScanner::ScanForVehiclesInRange (0x5FFE60), verbatim.
//
// A dead ped scans nothing. A ped in a vehicle short-circuits: the answer is
// the vehicle it is in, taken as a reference so it cannot be deleted under the
// ped's feet. Only a mission ped also runs the surrounding scan, because the
// script may need to know about the traffic around it.
void
CVehicleScanner::ScanForVehiclesInRange(CPed &ped)
{
	if(ped.Dead())
		return;

	if(ped.m_pMyVehicle != nil && ped.bInVehicle){
		if(ped.CharCreatedBy == MISSION_CHAR)
			CEntityScanner::ScanForEntitiesInRange(REPEATSECTOR_VEHICLES, ped);

		if(m_pClosestEntityInRange)
			m_pClosestEntityInRange->CleanUpOldReference(&m_pClosestEntityInRange);
		m_pClosestEntityInRange = ped.m_pMyVehicle;
		if(m_pClosestEntityInRange)
			m_pClosestEntityInRange->RegisterReference(&m_pClosestEntityInRange);
	}else{
		CEntityScanner::ScanForEntitiesInRange(REPEATSECTOR_VEHICLES, ped);
	}
}

CVehicle *
CVehicleScanner::GetClosestVehicleInRange(void) const
{
	return (CVehicle*)m_pClosestEntityInRange;
}
