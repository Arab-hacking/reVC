#include "TaskSimpleCarDriveTimed.h"

#include "Ped.h"

// SA CTaskSimpleCarDriveTimed (0x5FF940).

CTaskSimpleCarDriveTimed::CTaskSimpleCarDriveTimed(CVehicle *vehicle, int32 nTime)
: CTaskSimpleCarDrive(vehicle, nil, false)
{
	m_nTime = nTime;
}

// 0x46F610
bool
CTaskSimpleCarDriveTimed::ProcessPed(CPed *ped)
{
	m_nTimer.StartIfNotAlready(m_nTime);
	if(m_nTimer.IsOutOfTime())
		return true;
	return CTaskSimpleCarDrive::ProcessPed(ped);
}
