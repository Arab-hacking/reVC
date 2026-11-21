#include "common.h"

#include "UpsideDownCarCheck.h"
#include "Vehicle.h"
#include "Automobile.h"
#include "Bike.h"
#include "Pools.h"
#include "Timer.h"

// SA CUpsideDownCarCheck::Init (inlined, see 0x468DCB).
void
CUpsideDownCarCheck::Init(void)
{
	for(int32 i = 0; i < 6; i++)
		m_aUpsideDownCars[i].Clear();
}

// SA CUpsideDownCarCheck::IsCarUpsideDown (handle overload).
bool
CUpsideDownCarCheck::IsCarUpsideDown(int32 carHandle)
{
	const CVehicle *vehicle = CPools::GetVehicle(carHandle);
	return IsCarUpsideDown(vehicle);
}

// SA CUpsideDownCarCheck::IsCarUpsideDown (0x463830).
//
// The contact-wheel count is the real test in SA, and it only applies when the
// occupants cannot simply step out: a car you can get out of is not trapping
// anybody, however it is leaning.
bool
CUpsideDownCarCheck::IsCarUpsideDown(const CVehicle *vehicle)
{
	if(vehicle == nil)
		return false;

	auto GetNumContactWheels = [vehicle]() -> uint32 {
		switch(vehicle->m_nVehicleType){
		case VEHICLE_TYPE_BIKE:       return ((const CBike*)vehicle)->m_nWheelsOnGround;
		case VEHICLE_TYPE_AUTOMOBILE: return ((const CAutomobile*)vehicle)->m_nWheelsOnGround;
		default: return 0;
		}
	};

	if(!((CVehicle*)vehicle)->CanPedExitCar(false)){
		const CVector &up = ((CVehicle*)vehicle)->GetUp();
		// "Not totally upside down": leaning past the point where four wheels
		// carry it, but not flat on the roof either.
		return (up.z < 0.3f && GetNumContactWheels() < 4)
		    || up.z < 0.0f;
	}

	return false;
}

// SA CUpsideDownCarCheck::UpdateTimers (0x4655E0).
void
CUpsideDownCarCheck::UpdateTimers(void)
{
	for(int32 i = 0; i < 6; i++){
		UpsideDownCar &car = m_aUpsideDownCars[i];
		if(car.m_nHandle == 0)
			continue;

		const CVehicle *vehicle = CPools::GetVehicle(car.m_nHandle);
		if(vehicle){
			if(IsCarUpsideDown(vehicle))
				car.m_nTime += CTimer::GetTimeStepInMilliseconds();
			else
				car.m_nTime = 0;
		}else{
			car.m_nHandle = 0;
		}
	}
}

// SA CUpsideDownCarCheck::AreAnyCarsUpsideDown (0x4638A0). Unused in SA too,
// kept because the script opcode that asks this exists.
bool
CUpsideDownCarCheck::AreAnyCarsUpsideDown(void)
{
	for(int32 i = 0; i < 6; i++)
		if(m_aUpsideDownCars[i].m_nHandle >= 0 && m_aUpsideDownCars[i].m_nTime >= UPSIDE_DOWN_CAR_MIN_TIME)
			return true;
	return false;
}

// SA CUpsideDownCarCheck::AddCarToCheck (0x4638D0).
//
// Note this does not break after claiming a free slot: SA keeps filling every
// free slot, so a single add can occupy several. That is what the original
// does and the behaviour is relied on by nothing, but it is preserved rather
// than "fixed" silently.
void
CUpsideDownCarCheck::AddCarToCheck(int32 carHandle)
{
	for(int32 i = 0; i < 6; i++){
		UpsideDownCar &car = m_aUpsideDownCars[i];
		if(car.m_nHandle < 0){
			car.m_nHandle = carHandle;
			car.m_nTime = 0;
		}
	}
}

// SA CUpsideDownCarCheck::RemoveCarFromCheck (0x463910).
void
CUpsideDownCarCheck::RemoveCarFromCheck(int32 carHandle)
{
	for(int32 i = 0; i < 6; i++)
		if(m_aUpsideDownCars[i].m_nHandle == carHandle)
			m_aUpsideDownCars[i].Clear();
}

// SA CUpsideDownCarCheck::HasCarBeenUpsideDownForAWhile (0x463940).
bool
CUpsideDownCarCheck::HasCarBeenUpsideDownForAWhile(int32 carHandle)
{
	for(int32 i = 0; i < 6; i++)
		if(m_aUpsideDownCars[i].m_nHandle == carHandle)
			return m_aUpsideDownCars[i].m_nTime > UPSIDE_DOWN_CAR_MIN_TIME;
	return false;
}
