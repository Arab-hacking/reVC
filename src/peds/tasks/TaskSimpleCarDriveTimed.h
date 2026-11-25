#pragma once

// SA CTaskSimpleCarDriveTimed: drive, but give up after m_nTime ms
// (-1 = forever, in which case it behaves as CTaskSimpleCarDrive).

#include "TaskSimpleCarDrive.h"

class CTaskSimpleCarDriveTimed : public CTaskSimpleCarDrive
{
public:
	int32 m_nTime;
	CTaskTimer m_nTimer;

public:
	CTaskSimpleCarDriveTimed(CVehicle *vehicle, int32 nTime = -1);

	CTask *Clone() const { return new CTaskSimpleCarDriveTimed(m_pVehicle, m_nTime); }
	eTaskType GetTaskType() const { return TASK_SIMPLE_CAR_DRIVE_TIMED; }
	bool ProcessPed(CPed *ped);
};

VALIDATE_SIZE(CTaskSimpleCarDriveTimed, 0x70);
