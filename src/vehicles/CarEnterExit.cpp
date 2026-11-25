#include "common.h"

#include "CarEnterExit.h"
#include "Vehicle.h"
#include "Automobile.h"
#include "Ped.h"
#include "ModelInfo.h"
#include "VehicleModelInfo.h"
#include "World.h"
#include "Collision.h"
#include "Timer.h"

// SA CCarEnterExit::CarHasOpenableDoor (0x64EE50). The door id is unused in SA
// too; the answer only depends on whether this ped may unlock this vehicle.
bool
CCarEnterExit::CarHasOpenableDoor(const CVehicle *vehicle, int32 doorId, const CPed *ped)
{
	(void)doorId;
	return ((CVehicle*)vehicle)->CanPedOpenLocks((CPed*)ped);
}

// SA CCarEnterExit::ComputeDoorFlag (0x64E550).
//
// A bike (and a tandem-seat car) has one door per seat side, so the left and
// right "doors" both belong to the same seat: 5 and 10 are the two bit pairs.
// Everything else uses the normal four-door mapping.
int32
CCarEnterExit::ComputeDoorFlag(const CVehicle *vehicle, int32 doorId, bool bCheckVehicleType)
{
	if(bCheckVehicleType && (vehicle->IsBike() || (vehicle->m_pHandlingData && vehicle->m_pHandlingData->m_bTandemSeats))){
		switch(doorId){
		case 8:
		case 10:
		case 18: return 5;
		case 9:
		case 11: return 10;
		default: return 0;
		}
	}

	switch(doorId){
	case 8:  return 4;
	case 9:  return 8;
	case 10:
	case 18: return 1;
	case 11: return 2;
	default: return 0;
	}
}

// SA CCarEnterExit::ComputeSlowJackedPed (0x64F070): whose seat the door opens
// onto. Used by the slow carjack to know who has to be dragged out.
CPed *
CCarEnterExit::ComputeSlowJackedPed(const CVehicle *vehicle, int32 doorId)
{
	if(vehicle->IsBike() || (vehicle->m_pHandlingData && vehicle->m_pHandlingData->m_bTandemSeats)){
		switch(doorId){
		case 8:
		case 10:
		case 18: return vehicle->pDriver;
		case 9:
		case 11: return vehicle->pPassengers[0];
		default: return nil;
		}
	}

	switch(doorId){
	case 8:  return vehicle->pPassengers[0];
	case 9:  return vehicle->pPassengers[2];
	case 10: return vehicle->pDriver;
	case 11: return vehicle->pPassengers[1];
	default: return nil;
	}
}

// SA CCarEnterExit::GetPositionToOpenCarDoor (0x64E740).
//
// The door position is the seat position from the model, pulled outwards by
// the handling's seat offset, then offset once more by the get-in animation's
// own lateral travel so the ped ends up clear of the body.
CVector
CCarEnterExit::GetPositionToOpenCarDoor(const CVehicle *vehicle, int32 doorId)
{
	CVehicleModelInfo *vehModel = (CVehicleModelInfo*)CModelInfo::GetModelInfo(vehicle->GetModelIndex());
	if(vehModel == nil)
		return vehicle->GetPosition();

	float seatOffset;
	CVector doorPos;

	switch(doorId){
	case CAR_DOOR_RF:
	case CAR_DOOR_RR:
		doorPos = doorId == CAR_DOOR_RF ? vehModel->GetFrontSeatPosn() : vehModel->m_positions[CAR_POS_BACKSEAT];
		seatOffset = vehicle->m_pHandlingData ? vehicle->m_pHandlingData->m_fSeatOffsetDistance : 0.3f;
		doorPos.x += seatOffset;
		break;

	case CAR_DOOR_LF:
	case CAR_DOOR_LR:
		doorPos = doorId == CAR_DOOR_LF ? vehModel->GetFrontSeatPosn() : vehModel->m_positions[CAR_POS_BACKSEAT];
		seatOffset = vehicle->m_pHandlingData ? vehicle->m_pHandlingData->m_fSeatOffsetDistance : 0.3f;
		doorPos.x -= seatOffset;
		break;

	default:
		doorPos = vehModel->GetFrontSeatPosn();
		break;
	}

	return Multiply3x3(vehicle->GetMatrix(), doorPos) + vehicle->GetPosition();
}

// SA CCarEnterExit::GetPositionToOpenCarDoor, ped-aware form.
//
// On a bike the get-in animation itself swings the ped sideways, so the door
// position has to follow the animation rather than the seat. That math needs
// the per-animation offsets CPed already reads (GetPedAnimationOffset is
// local to the ped code and owned by it), so this overload delegates there
// instead of copying it: same numbers, one owner.
CVector
CCarEnterExit::GetPositionToOpenCarDoor(const CVehicle *vehicle, int32 doorId, const CPed *ped)
{
	return CPed::GetPositionToOpenCarDoor((CVehicle*)vehicle, (uint32)doorId, (CPed*)ped);
}

// SA CCarEnterExit::GetNearestCarDoor (0x6528F0).
//
// Pick the passenger door this ped should walk to. The interesting part is the
// first branch: on a bike (or a quad) a running ped that comes at the vehicle
// from its left gets the fast "jump on" side, door 18, instead of a normal
// door. That is the SA behaviour that makes jacking a bike feel different from
// jacking a car.
bool
CCarEnterExit::GetNearestCarDoor(const CPed *ped, const CVehicle *vehicle, CVector &outPos, int32 &doorId)
{
	CVehicle *veh = (CVehicle*)vehicle;
	CPed *p = (CPed*)ped;

	if((veh->IsBike() && !veh->IsSubBMX()) || veh->IsSubQuad()){
		// The jump-on side is only for a ped that is running at the bike from
		// its left, holding a roughly parallel heading, and is either a fast
		// player or an angry pedestrian. Anything else uses a normal door.
		if(!veh->IsOnItsSide()
		&& DotProduct2D(veh->GetRight(), p->GetForward()) > 0.0f
		&& DotProduct2D(veh->GetForward(), p->GetForward()) > 0.86602540f){	// cos(30 deg)
			const bool playerRunning = p->IsPlayer() && p->m_vecMoveSpeed.MagnitudeSqr() > 1.5f*1.5f;
			const bool angryPedRunning = !p->IsPlayer() && p->m_nMoveState == PEDMOVE_RUN && doorId == 0;
			if((playerRunning || angryPedRunning) && doorId == 0){
				if(IsRoomForPedToLeaveCar(veh, 18)){
					doorId = 18;
					outPos = GetPositionToOpenCarDoor(veh, 18);
					return true;
				}
			}
		}
	}

	const CVector posDoorFLeft = GetPositionToOpenCarDoor(veh, CAR_DOOR_LF);
	const CVector posDoorFRight = GetPositionToOpenCarDoor(veh, CAR_DOOR_RF);

	CVector2D pedPos2D = p->GetPosition();
	CVector2D dir2DToDoorFLeft = CVector2D(posDoorFLeft) - pedPos2D;
	CVector2D dir2DToDoorFRight = CVector2D(posDoorFRight) - pedPos2D;

	// If this vehicle is being towed, a door that the vehicle's own body is in
	// front of is unreachable: push that candidate away by a large distance so
	// the other side wins.
	if(veh->m_pVehicleBeingTowed){
		if(dir2DToDoorFLeft.MagnitudeSqr() < dir2DToDoorFRight.MagnitudeSqr()){
			if(IsPathToDoorBlockedByVehicleCollisionModel(p, veh, posDoorFRight))
				dir2DToDoorFRight = CVector2D(999.90002f, 999.90002f);
			else if(IsPathToDoorBlockedByVehicleCollisionModel(p, veh, posDoorFLeft))
				dir2DToDoorFLeft = CVector2D(999.90002f, 999.90002f);
		}
	}

	// A model that asks for the door check has only one usable side.
	if(veh->m_pHandlingData && veh->m_pHandlingData->m_bForceDoorCheck && veh->IsAutomobile()){
		const CAutomobile *aut = (const CAutomobile*)veh;
		if(aut->m_aCarNodes[CAR_DOOR_RF]){
			if(!aut->m_aCarNodes[CAR_DOOR_LF]){
				if(IsRoomForPedToLeaveCar(veh, CAR_DOOR_RF)){
					doorId = CAR_DOOR_RF;
					outPos = posDoorFRight;
					return true;
				}
			}
		}else{
			if(IsRoomForPedToLeaveCar(veh, CAR_DOOR_LF)){
				doorId = CAR_DOOR_LF;
				outPos = posDoorFLeft;
				return true;
			}
		}
		return false;
	}

	// The caller may already have a preference (the AI often does).
	if(doorId != CAR_NODE_NONE && IsRoomForPedToLeaveCar(veh, doorId)){
		switch(doorId){
		case CAR_DOOR_LF:
			outPos = posDoorFLeft;
			return true;
		case CAR_DOOR_RF:
			outPos = posDoorFRight;
			return true;
		default:
			return false;
		}
	}

	// Otherwise pick the nearer side, but never shove the current driver out
	// for a ped that has no business doing so.
	if(veh->pDriver == nil || !veh->pDriver->bDontDragMeOutCar){
		if(veh->bIsBus || dir2DToDoorFRight.MagnitudeSqr() > dir2DToDoorFLeft.MagnitudeSqr()){
			if(IsRoomForPedToLeaveCar(veh, CAR_DOOR_LF)){
				doorId = CAR_DOOR_LF;
				outPos = posDoorFLeft;
				return true;
			}
			if(IsRoomForPedToLeaveCar(veh, CAR_DOOR_RF)){
				doorId = CAR_DOOR_RF;
				outPos = posDoorFRight;
				return true;
			}
		}else{
			if(IsRoomForPedToLeaveCar(veh, CAR_DOOR_RF)){
				const bool passengerOnLeftBlocks =
					veh->HasPassengerAtSeat(0) && !veh->IsBike()
					&& !(veh->m_pHandlingData && veh->m_pHandlingData->m_bTandemSeats)
					&& (veh->pPassengers[0]->bDontDragMeOutCar || veh->IsMissionVehicle())
					&& IsRoomForPedToLeaveCar(veh, CAR_DOOR_LF);
				const bool somebodyGettingInLeft = (veh->m_nGettingInFlags & 4) != 0
					&& IsRoomForPedToLeaveCar(veh, CAR_DOOR_LF);
				if(passengerOnLeftBlocks || somebodyGettingInLeft){
					doorId = CAR_DOOR_LF;
					outPos = posDoorFLeft;
					return true;
				}
				doorId = CAR_DOOR_RF;
				outPos = posDoorFRight;
				return true;
			}
			if(IsRoomForPedToLeaveCar(veh, CAR_DOOR_LF)){
				doorId = CAR_DOOR_LF;
				outPos = posDoorFLeft;
				return true;
			}
		}
	}
	return false;
}

// SA CCarEnterExit::GetNearestCarPassengerDoor (0x650BB0).
//
// Where GetNearestCarDoor serves the driver's side, this one fills every
// passenger seat the model has. The three flags are the ones the caller can
// turn off: on a bike the tandem seat may or may not count as taken, the door
// may or may not need to be enterable, and the space next to it may or may not
// have to be clear.
//
// gta-reversed leaves this reversed= false (it is still a raw call there), so
// this is written from the Vice City implementation reVC already carried plus
// the SA flag semantics; it is the same set of doors in the same order.
bool
CCarEnterExit::GetNearestCarPassengerDoor(const CPed *ped, const CVehicle *vehicle, CVector *outVec, int32 *doorId,
	bool checkIfOccupiedTandemSeat, bool checkIfDoorIsEnterable, bool checkIfRoomToGetIn)
{
	CVehicle *veh = (CVehicle*)vehicle;
	CPed *p = (CPed*)ped;

	if(outVec == nil || doorId == nil)
		return false;

	CVehicleModelInfo *vehModel = (CVehicleModelInfo*)CModelInfo::GetModelInfo(veh->GetModelIndex());
	if(vehModel == nil)
		return false;

	// A bus has a single passenger door, at the back on the left.
	if(veh->bIsBus){
		*doorId = CAR_DOOR_LR;
		*outVec = GetPositionToOpenCarDoor(veh, CAR_DOOR_LR, p);
		return true;
	}

	CVector rfPos = CVector(0.0f, 0.0f, 0.0f);
	CVector lrPos = CVector(0.0f, 0.0f, 0.0f);
	CVector rrPos = CVector(0.0f, 0.0f, 0.0f);
	bool canEnter = false;

	CVector2D rfPosDist(999.0f, 999.0f);
	CVector2D lrPosDist(999.0f, 999.0f);
	CVector2D rrPosDist(999.0f, 999.0f);

	// The test each candidate door has to pass, with the caller's flags folded
	// in. A tandem seat counts as occupied only when asked to.
	auto DoorIsUsable = [&](int32 door, bool seatTaken) -> bool {
		if(seatTaken)
			return false;
		if(checkIfOccupiedTandemSeat && ComputeSlowJackedPed(veh, door) != nil)
			return false;
		if(checkIfDoorIsEnterable && !CarHasOpenableDoor(veh, door, p))
			return false;
		if(checkIfRoomToGetIn && !IsRoomForPedToLeaveCar(veh, door))
			return false;
		return true;
	};

	if(veh->IsBike()){
		if((veh->m_nGettingInFlags & CAR_DOOR_FLAG_LR) == 0 && DoorIsUsable(CAR_DOOR_LR, veh->pPassengers[0] != nil)){
			lrPos = GetPositionToOpenCarDoor(veh, CAR_DOOR_LR, p);
			canEnter = true;
			lrPosDist = CVector2D(lrPos) - CVector2D(p->GetPosition());
		}
		if((veh->m_nGettingInFlags & CAR_DOOR_FLAG_RR) == 0 && DoorIsUsable(CAR_DOOR_RR, veh->pPassengers[0] != nil)){
			rrPos = GetPositionToOpenCarDoor(veh, CAR_DOOR_RR, p);
			canEnter = true;
			rrPosDist = CVector2D(rrPos) - CVector2D(p->GetPosition());
		}
	}else if((veh->m_nGettingInFlags & CAR_DOOR_FLAG_RF) == 0 && DoorIsUsable(CAR_DOOR_RF, veh->pPassengers[0] != nil)){
		rfPos = GetPositionToOpenCarDoor(veh, CAR_DOOR_RF, p);
		canEnter = true;
		rfPosDist = CVector2D(rfPos) - CVector2D(p->GetPosition());
	}

	if(vehModel->m_numDoors == 4){
		if((veh->m_nGettingInFlags & CAR_DOOR_FLAG_LR) == 0 && DoorIsUsable(CAR_DOOR_LR, veh->pPassengers[1] != nil)){
			lrPos = GetPositionToOpenCarDoor(veh, CAR_DOOR_LR, p);
			canEnter = true;
			lrPosDist = CVector2D(lrPos) - CVector2D(p->GetPosition());
		}
		if((veh->m_nGettingInFlags & CAR_DOOR_FLAG_RR) == 0 && DoorIsUsable(CAR_DOOR_RR, veh->pPassengers[2] != nil)){
			rrPos = GetPositionToOpenCarDoor(veh, CAR_DOOR_RR, p);
			canEnter = true;
			rrPosDist = CVector2D(rrPos) - CVector2D(p->GetPosition());
		}

		// Nothing reachable: the occupants may be able to slide up a seat.
		if(!canEnter)
			veh->ShufflePassengersToMakeSpace();
	}

	// Pick the closest of the candidates, defaulting to the front right.
	CVector2D nextToCompare = rfPosDist;
	*outVec = rfPos;
	*doorId = CAR_DOOR_RF;

	if(lrPosDist.MagnitudeSqr() < nextToCompare.MagnitudeSqr()){
		*doorId = CAR_DOOR_LR;
		*outVec = lrPos;
		nextToCompare = lrPosDist;
	}
	if(rrPosDist.MagnitudeSqr() < nextToCompare.MagnitudeSqr()){
		*doorId = CAR_DOOR_RR;
		*outVec = rrPos;
	}

	return canEnter;
}

// SA CCarEnterExit::IsCarDoorReady (0x64ED90).
bool
CCarEnterExit::IsCarDoorReady(const CVehicle *vehicle, int32 doorId)
{
	CVehicle *veh = (CVehicle*)vehicle;
	return veh->IsDoorReady((uint32)doorId) || veh->IsDoorFullyOpen((uint32)doorId);
}

// SA CCarEnterExit::IsPathToDoorBlockedByVehicleCollisionModel (0x651210).
//
// Note what the name means: it returns *false* when a collision sphere of the
// vehicle sits on the line from the ped to the door, because then the ped can
// already reach the door. It returns true only when nothing of the vehicle is
// in the way, which is the case where the ped has to walk around it.
bool
CCarEnterExit::IsPathToDoorBlockedByVehicleCollisionModel(const CPed *ped, const CVehicle *vehicle, const CVector &pos)
{
	const CColModel *colModel = vehicle->GetColModel();
	if(colModel == nil)
		return false;

	const CMatrix vehMatInv = Invert(vehicle->GetMatrix());
	const CColLine line(vehMatInv * ped->GetPosition(), vehMatInv * pos);

	for(int32 i = 0; i < colModel->numSpheres; i++)
		if(CCollision::TestLineSphere(line, colModel->spheres[i]))
			return false;

	return true;
}

// SA CCarEnterExit::IsRoomForPedToLeaveCar (0x6504C0). The vehicle classes own
// the actual test because a bike and a car swing their doors differently.
bool
CCarEnterExit::IsRoomForPedToLeaveCar(const CVehicle *vehicle, int32 doorId)
{
	return ((CVehicle*)vehicle)->IsRoomForPedToLeaveCar((uint32)doorId, nil);
}
