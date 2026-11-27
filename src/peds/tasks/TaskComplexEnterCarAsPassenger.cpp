#include "common.h"

#include "TaskComplexEnterCarAsPassenger.h"

CTaskComplexEnterCarAsPassenger::CTaskComplexEnterCarAsPassenger(CVehicle *targetVehicle, int32 nTargetSeat, bool bCarryOnAfterFallingOff)
	: CTaskComplexEnterCar(targetVehicle, false, false, false, bCarryOnAfterFallingOff)
{
	m_TargetSeat = nTargetSeat;
}

CTaskComplexEnterCarAsPassenger::CTaskComplexEnterCarAsPassenger(const CTaskComplexEnterCarAsPassenger &other)
	: CTaskComplexEnterCarAsPassenger(other.m_Car, other.m_TargetSeat, other.m_bCarryOnAfterFallingOff)
{
	m_MoveState = other.m_MoveState;
}
