#include "common.h"

#include "TaskComplexEnterCarAsDriver.h"

CTaskComplexEnterCarAsDriver::CTaskComplexEnterCarAsDriver(CVehicle *targetVehicle)
	: CTaskComplexEnterCar(targetVehicle, true, false, false, false)
{
}

CTaskComplexEnterCarAsDriver::CTaskComplexEnterCarAsDriver(CVehicle *targetVehicle, eMoveState moveState)
	: CTaskComplexEnterCarAsDriver(targetVehicle)
{
	m_MoveState = moveState;
}

CTaskComplexEnterCarAsDriver::CTaskComplexEnterCarAsDriver(const CTaskComplexEnterCarAsDriver &other)
	: CTaskComplexEnterCarAsDriver(other.m_Car)
{
	m_MoveState = other.m_MoveState;
}
