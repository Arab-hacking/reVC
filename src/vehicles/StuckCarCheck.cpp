#include "common.h"

#include "StuckCarCheck.h"
#include "Vehicle.h"
#include "Automobile.h"
#include "Bike.h"
#include "Pools.h"
#include "Timer.h"
#include "Camera.h"
#include "World.h"
#include "PathFind.h"
#include "Script.h"

// SA CStuckCarCheck::Init (0x4639E0).
void
CStuckCarCheck::Init(void)
{
	for(int32 i = 0; i < STUCK_CAR_CHECK_SIZE; i++)
		ResetArrayElement(m_aStuckCars[i]);
}

// NOTSA helper, same values gta-reversed uses.
void
CStuckCarCheck::ResetArrayElement(tStuckCar &car)
{
	car.LastPos = CVector(-5000.0f, -5000.0f, -5000.0f);
	car.CarHandle = -1;
	car.LastChecked = -1;
	car.StuckRadius = 0.0f;
	car.CheckTime = 0;
	car.CarStuck = false;
	car.WarpCar = false;
	car.WarpIfStuck = false;
	car.WarpIfFlipped = false;
	car.WarpIfInWater = false;
	car.NumberOfNodesToCheck = 0;
}

// SA declares this overload taking an index; it is the one the pool iteration
// uses. It must not be confused with ResetArrayElement(tStuckCar&).
void
CStuckCarCheck::ResetArrayElement(uint16 index)
{
	if(index < STUCK_CAR_CHECK_SIZE)
		ResetArrayElement(m_aStuckCars[index]);
}

// SA CStuckCarCheck::AddCarToCheck (0x465970).
void
CStuckCarCheck::AddCarToCheck(int32 carHandle, float stuckRadius, uint32 time, bool warpCar,
	bool stuck, bool flipped, bool inWater, int8 numberOfNodesToCheck)
{
	CVehicle *vehicle = CPools::GetVehicle(carHandle);
	if(vehicle == nil)
		return;

	for(int32 i = 0; i < STUCK_CAR_CHECK_SIZE; i++){
		tStuckCar &car = m_aStuckCars[i];
		if(car.CarHandle != -1)
			continue;

		car.CarHandle = carHandle;
		car.LastPos = vehicle->GetPosition();
		car.LastChecked = CTimer::GetTimeInMilliseconds();
		car.StuckRadius = stuckRadius;
		car.CheckTime = (int32)time;
		car.WarpCar = warpCar;
		car.WarpIfStuck = stuck;
		car.WarpIfFlipped = flipped;
		car.CarStuck = false;
		car.WarpIfInWater = inWater;
		car.NumberOfNodesToCheck = numberOfNodesToCheck;
		return;
	}
}

// SA CStuckCarCheck::AttemptToWarpVehicle (0x463A60).
//
// The whole point is that the player must not see it happen: if the camera can
// see the destination, or anything the mission cares about is sitting there,
// the warp is refused and the caller tries another node.
bool
CStuckCarCheck::AttemptToWarpVehicle(CVehicle *vehicle, CVector *origin, float orientation)
{
	if(TheCamera.IsSphereVisible(*origin, 4.0f))
		return false;

	// A four metre cube around the destination, for anything mission-critical.
	int16 outCount = 0;
	const CVector offset(4.0f, 4.0f, 4.0f);
	CWorld::FindMissionEntitiesIntersectingCube(*origin - offset, *origin + offset, &outCount, 2, nil, true, true, true);
	if(outCount != 0)
		return false;

	// reVC has no CCarCtrl::SetCoordsOfScriptCar, which in SA resets the car's
	// physics state as well as its transform. Doing the transform and clearing
	// the motion by hand is the equivalent here: a warped car must not keep the
	// velocity it had when it got wedged.
	vehicle->SetPosition(*origin);
	vehicle->SetHeading(DEGTORAD(orientation));
	vehicle->m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f);
	vehicle->m_vecTurnSpeed = CVector(0.0f, 0.0f, 0.0f);
	return true;
}

// SA CStuckCarCheck::ClearStuckFlagForCar (0x463C40).
void
CStuckCarCheck::ClearStuckFlagForCar(int32 carHandle)
{
	for(int32 i = 0; i < STUCK_CAR_CHECK_SIZE; i++)
		if(m_aStuckCars[i].CarHandle == carHandle)
			m_aStuckCars[i].CarStuck = false;
}

// SA CStuckCarCheck::HasCarBeenStuckForAWhile (0x463C00).
bool
CStuckCarCheck::HasCarBeenStuckForAWhile(int32 carHandle)
{
	for(int32 i = 0; i < STUCK_CAR_CHECK_SIZE; i++)
		if(m_aStuckCars[i].CarHandle == carHandle && m_aStuckCars[i].CarStuck)
			return true;
	return false;
}

// SA CStuckCarCheck::IsCarInStuckCarArray (0x463C70).
bool
CStuckCarCheck::IsCarInStuckCarArray(int32 carHandle)
{
	for(int32 i = 0; i < STUCK_CAR_CHECK_SIZE; i++)
		if(m_aStuckCars[i].CarHandle == carHandle)
			return true;
	return false;
}

// SA CStuckCarCheck::Process (0x465680).
//
// Three stages per watched car: refresh the stuck flag on its own timer, decide
// whether any of the warp conditions holds, then find somewhere to put it back
// - either the script-parked route the autopilot was following, or the nearest
// path nodes, nearest first.
void
CStuckCarCheck::Process(void)
{
	const uint32 now = CTimer::GetTimeInMilliseconds();

	for(int32 i = 0; i < STUCK_CAR_CHECK_SIZE; i++){
		tStuckCar &car = m_aStuckCars[i];
		if(car.CarHandle < 0)
			continue;

		CVehicle *vehicle = CPools::GetVehicle(car.CarHandle);
		if(vehicle == nil){
			ResetArrayElement(car);
			continue;
		}

		// A car with nobody in it cannot be "stuck"; the timer restarts so a
		// car that was abandoned and re-entered is judged afresh.
		if(vehicle->pDriver == nil){
			car.LastPos = vehicle->GetPosition();
			car.LastChecked = (int32)now;
			continue;
		}

		if(now > (uint32)(car.LastChecked + car.CheckTime)){
			const CVector curPos = vehicle->GetPosition();
			const CVector distMoved = curPos - car.LastPos;

			car.CarStuck = distMoved.Magnitude() < car.StuckRadius;
			car.LastPos = curPos;
			car.LastChecked = (int32)now;
		}

		if(!car.WarpCar)
			continue;

		bool shouldWarp = false;
		if(car.WarpIfStuck && car.CarStuck)
			shouldWarp = true;
		if(car.WarpIfFlipped && CTheScripts::UpsideDownCars.IsCarUpsideDown(vehicle))
			shouldWarp = true;
		// SA tests physicalFlags.bSubmergedInWater; reVC keeps the same bit as
		// bIsInWater on CPhysical.
		if(car.WarpIfInWater && vehicle->bIsInWater)
			shouldWarp = true;

		if(!shouldWarp)
			continue;

		// Never warp a car the player can see.
		if(TheCamera.IsSphereVisible(vehicle->GetBoundCentre(), vehicle->GetColModel()->boundingSphere.radius))
			continue;

		// A negative node count means "use the route the car was following".
		// reVC addresses path nodes by index, not by pointer, so the autopilot's
		// next route node is looked up directly.
		if(car.NumberOfNodesToCheck < 0){
			const int32 routeNode = vehicle->AutoPilot.m_nNextRouteNode;
			if(routeNode >= 0){
				const CVector newCoords = ThePaths.FindNodeCoorsForScript(routeNode);
				const float newHeading = ThePaths.FindNodeOrientationForCarPlacement(routeNode);
				AttemptToWarpVehicle(vehicle, (CVector*)&newCoords, newHeading);
			}
			continue;
		}

		for(int32 node = 0; node < car.NumberOfNodesToCheck; node++){
			// SA walks the nearest vehicle nodes, nearest first, and takes the
			// first one it can actually warp to. FindNthNodeClosestToCoors
			// already means "the Nth closest", so each pass marks one off.
			const int32 nodeId = ThePaths.FindNthNodeClosestToCoors(car.LastPos, PATH_CAR,
				999999.88f, false, true, node, false);
			if(nodeId < 0)
				continue;
			const CVector newCoords = ThePaths.FindNodeCoorsForScript(nodeId);
			const float newHeading = ThePaths.FindNodeOrientationForCarPlacement(nodeId);
			if(AttemptToWarpVehicle(vehicle, (CVector*)&newCoords, newHeading))
				break;
		}
	}
}

// SA CStuckCarCheck::RemoveCarFromCheck (0x463B80).
void
CStuckCarCheck::RemoveCarFromCheck(int32 carHandle)
{
	for(int32 i = 0; i < STUCK_CAR_CHECK_SIZE; i++)
		if(m_aStuckCars[i].CarHandle == carHandle)
			ResetArrayElement(m_aStuckCars[i]);
}
