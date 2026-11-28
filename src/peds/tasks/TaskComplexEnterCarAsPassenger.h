#ifndef __GTA_TASKCOMPLEXENTERCARASPASSENGER_H__
#define __GTA_TASKCOMPLEXENTERCARASPASSENGER_H__

#include "TaskComplexEnterCar.h"

// Enter the vehicle to ride in it.
class CTaskComplexEnterCarAsPassenger : public CTaskComplexEnterCar
{
public:
	static const eTaskType Type = TASK_COMPLEX_ENTER_CAR_AS_PASSENGER;

	CTaskComplexEnterCarAsPassenger(CVehicle *targetVehicle, int32 nTargetSeat = 0, bool bCarryOnAfterFallingOff = false);
	CTaskComplexEnterCarAsPassenger(const CTaskComplexEnterCarAsPassenger &other);
	~CTaskComplexEnterCarAsPassenger() {}

	CTask *Clone() const { return new CTaskComplexEnterCarAsPassenger(*this); }
	eTaskType GetTaskType() const { return Type; }
};

VALIDATE_SIZE(CTaskComplexEnterCarAsPassenger, 0x50);

#endif // __GTA_TASKCOMPLEXENTERCARASPASSENGER_H__
