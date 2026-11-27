#ifndef __GTA_TASKCOMPLEXENTERCARASDRIVER_H__
#define __GTA_TASKCOMPLEXENTERCARASDRIVER_H__

#include "TaskComplexEnterCar.h"

// Enter the vehicle to drive it.
class CTaskComplexEnterCarAsDriver : public CTaskComplexEnterCar
{
public:
	static const eTaskType Type = TASK_COMPLEX_ENTER_CAR_AS_DRIVER;

	explicit CTaskComplexEnterCarAsDriver(CVehicle *targetVehicle);
	CTaskComplexEnterCarAsDriver(CVehicle *targetVehicle, eMoveState moveState);
	CTaskComplexEnterCarAsDriver(const CTaskComplexEnterCarAsDriver &other);
	~CTaskComplexEnterCarAsDriver() {}

	CTask *Clone() const { return new CTaskComplexEnterCarAsDriver(*this); }
	eTaskType GetTaskType() const { return Type; }
};

VALIDATE_SIZE(CTaskComplexEnterCarAsDriver, 0x50);

#endif // __GTA_TASKCOMPLEXENTERCARASDRIVER_H__
