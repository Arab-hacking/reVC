#include "common.h"

#include "VehicleAnimGroupData.h"

#include "CarEnterExit.h"
#include "Vehicle.h"
#include "Automobile.h"
#include "Bike.h"
#include "Boat.h"
#include "Ped.h"
#include "PedIntelligence.h"
#include "TaskComplexLeaveCar.h"
#include "PlayerPed.h"
#include "ModelInfo.h"
#include "VehicleModelInfo.h"
#include "AnimManager.h"
#include "AnimBlendAssociation.h"
#include "RpAnimBlend.h"
#include "World.h"
#include "Collision.h"
#include "EventList.h"
#include "Timer.h"
#include "Pad.h"
#include "Script.h"
#include "Fire.h"
#include "Streaming.h"
#include "Pools.h"
#include "Camera.h"

float CCarEnterExit::ms_fMaxSpeed_CanDragPedOut = 0.1f;
float CCarEnterExit::ms_fMaxSpeed_PlayerCanDragPedOut = 0.2f;

bool CCarEnterExit::ms_bPedOffsetsCalculated = false;

CVector CCarEnterExit::ms_vecPedGetUpAnimOffset;
CVector CCarEnterExit::ms_vecPedBedLAnimOffset;
CVector CCarEnterExit::ms_vecPedBedRAnimOffset;
CVector CCarEnterExit::ms_vecPedDeskAnimOffset;
CVector CCarEnterExit::ms_vecPedChairAnimOffset;
CVector CCarEnterExit::ms_vecPedQuickDraggedOutCarAnimOffset;

void
CCarEnterExit::Initialise(void)
{
	ms_fMaxSpeed_CanDragPedOut = 0.1f;
	ms_fMaxSpeed_PlayerCanDragPedOut = 0.2f;
	ms_bPedOffsetsCalculated = false;
	ms_vecPedGetUpAnimOffset = CVector(0.0f, 0.0f, 0.0f);
	ms_vecPedBedLAnimOffset = CVector(0.0f, 0.0f, 0.0f);
	ms_vecPedBedRAnimOffset = CVector(0.0f, 0.0f, 0.0f);
	ms_vecPedDeskAnimOffset = CVector(0.0f, 0.0f, 0.0f);
	ms_vecPedChairAnimOffset = CVector(0.0f, 0.0f, 0.0f);
	ms_vecPedQuickDraggedOutCarAnimOffset = CVector(0.0f, 0.0f, 0.0f);
}

// The animation set the occupants of a vehicle play through, resolved the way
// SA resolves it: the model names one of the 30 vehicle animation groups, and
// that group picks between its two candidate AssocGroupIds per animation using
// the eighteen flags of its `^` line. Resolving per clip matters here because
// the bike ride pose and the bike passenger pose can come from different
// candidates of the same group.
static AssocGroupId
GetVehicleAnimGroupFor(const CVehicle *veh, AnimationId animId)
{
	if(veh == nil)
		return ANIM_GROUP_STDCARAMIMS;
	// The group index comes from the handling record (see handling.cfg's `^`
	// lines), which is what SA reads here as well.
	const int32 groupId = veh->GetAnimGroupId();
	if(groupId >= CVehicleAnimGroupData::NUM_VEH_ANIM_GROUPS)
		return ANIM_GROUP_STDCARAMIMS;
	return CVehicleAnimGroupData::GetGroupForAnim((AssocGroupId)groupId, animId);
}

// SA CCarEnterExit::AddInCarAnim (0x64F720), verbatim. Note the inverted
// "bAsDriver" in SA: the driver branch is the one that runs when the flag is
// set, but the boat and low-vehicle special cases are the *driver's* seats.
void
CCarEnterExit::AddInCarAnim(const CVehicle *vehicle, CPed *ped, bool bAsDriver)
{
	AssocGroupId grpId;
	AnimationId animId;

	if(bAsDriver){
		if(vehicle->IsBike()){
			animId = ANIM_ID_BIKE_RIDE;
			// SA resolves this through the bike's own ride-anim data,
			// not the ^ vehicle groups (those live in the 351-offset
			// space and have no BIKE_RIDE slot to resolve through;
			// passing 194 into them came back empty for every bike).
			grpId = ((const CBike*)vehicle)->m_bikeAnimType;
		}else if(vehicle->IsBoat() && vehicle->m_pHandlingData && vehicle->m_pHandlingData->m_bSitInBoat){
			grpId = ANIM_GROUP_DEFAULT;
			animId = ANIM_ID_DRIVE_BOAT;
		}else if(vehicle->bLowVehicle){
			grpId = ANIM_GROUP_DEFAULT;
			animId = ANIM_ID_CAR_LSIT;
		}else{
			grpId = ANIM_GROUP_DEFAULT;
			animId = ANIM_ID_CAR_SIT;
		}
	}else{
		if(vehicle->IsBike()){
			animId = ANIM_ID_BIKE_RIDE;
			// As for the driver, above: the bike's own rider group.
			grpId = ((const CBike*)vehicle)->m_bikeAnimType;
		}else if(vehicle->bLowVehicle){
			grpId = ANIM_GROUP_DEFAULT;
			animId = ANIM_ID_CAR_SITPLO;
		}else{
			grpId = ANIM_GROUP_DEFAULT;
			animId = ANIM_ID_CAR_SITP;
		}
	}

	CAnimBlendAssociation *sitAssoc = CAnimManager::BlendAnimation(ped->GetClump(), grpId, animId, 1000.0f);
	if(vehicle->IsBike())
		REVC_LOG_INFO("Bike anim: sit anim=%d group=%d clip=%s", animId, grpId, sitAssoc ? "found" : "MISSING");
	ped->StopNonPartialAnims();
}

// SA CCarEnterExit::CarHasDoorToClose (0x64EE10).
bool
CCarEnterExit::CarHasDoorToClose(const CVehicle *vehicle, int32 doorId)
{
	CVehicle *veh = (CVehicle*)vehicle;
	return !veh->IsDoorMissing((uint32)doorId) && !veh->IsDoorClosed((uint32)doorId);
}

// SA CCarEnterExit::CarHasDoorToOpen (0x64EDD0).
bool
CCarEnterExit::CarHasDoorToOpen(const CVehicle *vehicle, int32 doorId)
{
	CVehicle *veh = (CVehicle*)vehicle;
	return !veh->IsDoorMissing((uint32)doorId) && !veh->IsDoorFullyOpen((uint32)doorId);
}

// SA CCarEnterExit::CarHasOpenableDoor (0x64EE50). The door id is unused in SA
// too; the answer only depends on whether this ped may unlock this vehicle.
bool
CCarEnterExit::CarHasOpenableDoor(const CVehicle *vehicle, int32 doorId, const CPed *ped)
{
	(void)doorId;
	return ((CVehicle*)vehicle)->CanPedOpenLocks((CPed*)ped);
}

// SA CCarEnterExit::CarHasPartiallyOpenDoor (0x64EE70).
bool
CCarEnterExit::CarHasPartiallyOpenDoor(const CVehicle *vehicle, int32 doorId)
{
	CVehicle *veh = (CVehicle*)vehicle;
	return !veh->IsDoorMissing((uint32)doorId)
	    && !veh->IsDoorFullyOpen((uint32)doorId)
	    && !veh->IsDoorClosed((uint32)doorId);
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

// SA CCarEnterExit::ComputeOppositeDoorFlag (0x64E610).
int32
CCarEnterExit::ComputeOppositeDoorFlag(const CVehicle *vehicle, int32 doorId, bool bCheckVehicleType)
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
	case 8:  return 1;
	case 9:  return 2;
	case 10:
	case 18: return 4;
	case 11: return 8;
	default: return 0;
	}
}

// SA CCarEnterExit::ComputePassengerIndexFromCarDoor (0x64F1E0).
//
// Note which doors map to which passenger index: door 9 (front right) is the
// *first* passenger only in a two-door car; in a four-door it is the third.
// Doors 10/18 are the driver's, and never reach here.
int32
CCarEnterExit::ComputePassengerIndexFromCarDoor(const CVehicle *vehicle, int32 doorId)
{
	if(vehicle->IsBike() || (vehicle->m_pHandlingData && vehicle->m_pHandlingData->m_bTandemSeats)){
		switch(doorId){
		case 9:
		case 11: return 0;
		default: return -1;
		}
	}

	switch(doorId){
	case 8:  return 0;
	case 9:  return 2;
	case 11: return 1;
	default: return -1;
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

// SA CCarEnterExit::ComputeTargetDoorToEnterAsPassenger (0x64F190).
//
// A bus has one door for everybody (the rear left one), so all passengers use
// it. Otherwise a bike puts its single passenger on door 11, and a car walks
// the seats front-left, front-right, rear-left.
int32
CCarEnterExit::ComputeTargetDoorToEnterAsPassenger(const CVehicle *vehicle, int32 passengerNum)
{
	if(vehicle->bIsBus)
		return 8;

	switch(passengerNum){
	case 0: return (vehicle->IsBike() || (vehicle->m_pHandlingData && vehicle->m_pHandlingData->m_bTandemSeats)) ? 11 : 8;
	case 1: return 11;
	case 2: return 9;
	default: return -1;
	}
}

// SA CCarEnterExit::ComputeTargetDoorToExit (0x64F110).
int32
CCarEnterExit::ComputeTargetDoorToExit(const CVehicle *vehicle, const CPed *ped)
{
	if(vehicle->pDriver == ped)
		return 10;

	// SA looks the ped up among the passengers and reuses the enter-side
	// mapping. reVC stores the seats in pPassengers[], so search it.
	for(int32 i = 0; i < vehicle->m_nNumMaxPassengers; i++)
		if(vehicle->pPassengers[i] == ped)
			return ComputeTargetDoorToEnterAsPassenger(vehicle, i);

	return -1;
}

// SA CCarEnterExit::GetPositionToOpenCarDoor (0x64E740).
//
// The door position is the seat position from the model, pulled outwards by
// the handling's seat offset. Callers that have a ped should use the
// ped-aware overload below instead: only that one also applies the get-in
// animation's lateral travel, without which the point is unreachable
// (it sits inside the car's collision).
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

	const CVector *driverDraggedOutOffset = veh->pDriver ? &ms_vecPedQuickDraggedOutCarAnimOffset : nil;
	const CVector *psgrDraggedOutOffset = veh->HasPassengerAtSeat(0) ? &ms_vecPedQuickDraggedOutCarAnimOffset : nil;

	if((veh->IsBike() && !veh->IsSubBMX()) || veh->IsSubQuad()){
		driverDraggedOutOffset = nil;
		psgrDraggedOutOffset = nil;

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
					outPos = GetPositionToOpenCarDoor(veh, 18, p);
					return true;
				}
			}
		}
	}else if(veh->bIsBus || veh->bLowVehicle){
		driverDraggedOutOffset = nil;
		psgrDraggedOutOffset = nil;
	}

	// The stand point must include the get-in animation's lateral travel
	// (the ped-aware form): the seat-plus-seat-offset point alone sits
	// inside the car's collision, ~0.85m from where a ped can actually
	// stand, and the seek arrival radius is 0.5m - the ped hovered at
	// the door forever and never started the get-in.
	const CVector posDoorFLeft = GetPositionToOpenCarDoor(veh, CAR_DOOR_LF, p);
	const CVector posDoorFRight = GetPositionToOpenCarDoor(veh, CAR_DOOR_RF, p);

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
				if(IsRoomForPedToLeaveCar(veh, CAR_DOOR_RF, driverDraggedOutOffset)){
					doorId = CAR_DOOR_RF;
					outPos = posDoorFRight;
					return true;
				}
			}
		}else{
			if(IsRoomForPedToLeaveCar(veh, CAR_DOOR_LF, driverDraggedOutOffset)){
				doorId = CAR_DOOR_LF;
				outPos = posDoorFLeft;
				return true;
			}
		}
		return false;
	}

	// The caller may already have a preference (the AI often does).
	if(doorId != CAR_NODE_NONE && IsRoomForPedToLeaveCar(veh, doorId, driverDraggedOutOffset)){
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
			if(IsRoomForPedToLeaveCar(veh, CAR_DOOR_LF, driverDraggedOutOffset)){
				doorId = CAR_DOOR_LF;
				outPos = posDoorFLeft;
				return true;
			}
			if(IsRoomForPedToLeaveCar(veh, CAR_DOOR_RF, driverDraggedOutOffset)){
				doorId = CAR_DOOR_RF;
				outPos = posDoorFRight;
				return true;
			}
		}else{
			if(IsRoomForPedToLeaveCar(veh, CAR_DOOR_RF, driverDraggedOutOffset)){
				const bool passengerOnLeftBlocks =
					veh->HasPassengerAtSeat(0) && !veh->IsBike()
					&& !(veh->m_pHandlingData && veh->m_pHandlingData->m_bTandemSeats)
					&& (veh->pPassengers[0]->bDontDragMeOutCar || veh->IsMissionVehicle())
					&& IsRoomForPedToLeaveCar(veh, CAR_DOOR_LF, driverDraggedOutOffset);
				const bool somebodyGettingInLeft = (veh->m_nGettingInFlags & 4) != 0
					&& IsRoomForPedToLeaveCar(veh, CAR_DOOR_LF, driverDraggedOutOffset);
				if(passengerOnLeftBlocks || somebodyGettingInLeft){
					doorId = CAR_DOOR_LF;
					outPos = posDoorFLeft;
					return true;
				}
				doorId = CAR_DOOR_RF;
				outPos = posDoorFRight;
				return true;
			}
			if(IsRoomForPedToLeaveCar(veh, CAR_DOOR_LF, driverDraggedOutOffset)){
				doorId = CAR_DOOR_LF;
				outPos = posDoorFLeft;
				return true;
			}
		}
	}
	(void)psgrDraggedOutOffset;
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
		if(checkIfRoomToGetIn && !IsRoomForPedToLeaveCar(veh, door, nil))
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

// SA CCarEnterExit::IsCarDoorInUse (0x64EC90).
//
// The getting-in and getting-out flag bytes are per-cluster, not per-door, so
// this only tells whether *somebody* is already using the door's side.
bool
CCarEnterExit::IsCarDoorInUse(const CVehicle *vehicle, int32 firstDoorId, int32 secondDoorId)
{
	auto CheckIsDoorInUse = [vehicle](int32 door) -> bool {
		auto CheckInOutFlags = [vehicle](uint32 n) -> bool {
			const uint8 flag = 1 << n;
			return (flag & vehicle->m_nGettingInFlags) || (flag & vehicle->m_nGettingOutFlags);
		};
		switch(door){
		case 8:  return CheckInOutFlags(2);
		case 9:  return CheckInOutFlags(3);
		case 10:
		case 18: return CheckInOutFlags(0);
		case 11: return CheckInOutFlags(1);
		default: return false;
		}
	};
	return CheckIsDoorInUse(firstDoorId) || CheckIsDoorInUse(secondDoorId);
}

// SA CCarEnterExit::IsCarDoorReady (0x64ED90).
bool
CCarEnterExit::IsCarDoorReady(const CVehicle *vehicle, int32 doorId)
{
	CVehicle *veh = (CVehicle*)vehicle;
	// A bike (and a boat) has no door the damage manager can lose or jam, and
	// the base CVehicle door queries answer false to everything - so the
	// entry gate refused bikes for good (debug-13: pcj600 door 8, ready=0 on
	// every attempt). The door ids ComputeDoorFlag hands out for a bike name
	// a seat side, not a door, and a seat is always ready.
	if(veh->IsBike() || veh->IsBoat())
		return true;
	return veh->IsDoorReady((uint32)doorId) || veh->IsDoorFullyOpen((uint32)doorId);
}

// SA CCarEnterExit::IsCarQuickJackPossible (0x64EF00). SA returns false
// unconditionally here, with a comment saying that returning anything else
// breaks CTaskComplexEnterCar. Kept as-is rather than "improved".
bool
CCarEnterExit::IsCarQuickJackPossible(CVehicle *vehicle, int32 doorId, const CPed *ped)
{
	(void)vehicle;
	(void)doorId;
	(void)ped;
	return false;
}

// SA CCarEnterExit::IsCarSlowJackRequired (0x64EF70): is the seat this door
// opens onto already taken?
bool
CCarEnterExit::IsCarSlowJackRequired(const CVehicle *vehicle, int32 doorId)
{
	if(vehicle->IsBike() || (vehicle->m_pHandlingData && vehicle->m_pHandlingData->m_bTandemSeats)){
		switch(doorId){
		case 8:
		case 10:
		case 18: return vehicle->HasDriver();
		case 9:
		case 11: return vehicle->HasPassengerAtSeat(0);
		default: return false;
		}
	}

	const uint8 group = vehicle->GetAnimGroupId();
	if(group == ANIM_GROUP_COACHCARANIMS || group == ANIM_GROUP_BUSCARANIMS){
		switch(doorId){
		case 8:  return false;
		case 10: return vehicle->HasDriver();
		default: return false;
		}
	}

	switch(doorId){
	case 8:  return vehicle->HasPassengerAtSeat(0);
	case 9:  return vehicle->HasPassengerAtSeat(2);
	case 10: return vehicle->HasDriver();
	case 11: return vehicle->HasPassengerAtSeat(1);
	default: return false;
	}
}

// SA CCarEnterExit::IsClearToDriveAway (0x6509B0): is there anything directly
// in front of the vehicle that it would hit if it pulled away now?
bool
CCarEnterExit::IsClearToDriveAway(const CVehicle *vehicle)
{
	const CVector &vehPos = vehicle->GetPosition();
	const float bbSizeY = vehicle->GetColModel()->boundingBox.max.y - vehicle->GetColModel()->boundingBox.min.y;
	CEntity *hitEntity = nil;
	CColPoint hitCP;
	const bool hit = CWorld::ProcessLineOfSight(vehPos + ((CVehicle*)vehicle)->GetForward() * bbSizeY, vehPos,
		hitCP, hitEntity, true, true, false, false, false, true, true, false);
	return !hit || hitEntity == vehicle;
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

// SA CCarEnterExit::IsPedHealthy (0x64EEE0).
bool
CCarEnterExit::IsPedHealthy(CPed *ped)
{
	return ped->m_fHealth > 0.0f;
}

// SA CCarEnterExit::IsPlayerToQuitCarEnter (0x64F240, unreversed in the
// reference): has the player changed his mind about getting into this
// vehicle? He has once the entry has been going for a moment and he taps
// the enter/exit input again; the tap that started the entry must not
// itself count, hence the grace time.
bool
CCarEnterExit::IsPlayerToQuitCarEnter(const CPed *ped, const CVehicle *vehicle, int32 startTime, CTask *task)
{
	if (ped == nil || vehicle == nil || !ped->IsPlayer())
		return false;
	if ((int32)CTimer::GetTimeInMilliseconds() - startTime < 500)
		return false;
	CPad *pad = CPad::GetPad(0);
	if (pad->ExitVehicleJustDown())
		return true;
	// Steering away from the car cancels too, as it did when the
	// seek was a ped state the player controller could drop.
	return pad->GetLeftStickXJustDown() || pad->GetLeftStickYJustDown() ||
		pad->GetDPadUpJustDown() || pad->GetDPadDownJustDown() ||
		pad->GetDPadLeftJustDown() || pad->GetDPadRightJustDown();
}

// SA CCarEnterExit::IsRoomForPedToLeaveCar (0x6504C0). The vehicle classes own
// the actual test because a bike and a car swing their doors differently.
bool
CCarEnterExit::IsRoomForPedToLeaveCar(const CVehicle *vehicle, int32 doorId, const CVector *offset)
{
	return ((CVehicle*)vehicle)->IsRoomForPedToLeaveCar((uint32)doorId, (CVector*)offset);
}

// SA CCarEnterExit::IsVehicleHealthy (0x64EEC0).
bool
CCarEnterExit::IsVehicleHealthy(const CVehicle *vehicle)
{
	return vehicle->GetStatus() != STATUS_WRECKED;
}

// SA CCarEnterExit::IsVehicleStealable (0x6510D0). This is the "can I just take
// this car" test the AI runs before deciding to carjack: no aircraft, no
// mission or parked-owned vehicles unless it is already mine, nothing burning
// or wrecked, nothing on its roof, and a clear road ahead.
bool
CCarEnterExit::IsVehicleStealable(const CVehicle *vehicle, const CPed *ped)
{
	if(vehicle->m_nVehicleSubType == VEHICLE_TYPE_PLANE || vehicle->m_nVehicleSubType == VEHICLE_TYPE_HELI)
		return false;

	if(vehicle->m_nVehicleType != VEHICLE_TYPE_AUTOMOBILE && vehicle->m_nVehicleType != VEHICLE_TYPE_BIKE)
		return false;

	if(ped->m_pMyVehicle != vehicle){
		switch(vehicle->VehicleCreatedBy){
		case RANDOM_VEHICLE:
		case PARKED_VEHICLE:
			break;
		default:
			return false;
		}
	}

	// SA reads the global check, the same one the script and the stuck-car
	// timer drive. A temporary here would always answer "not upside down",
	// because a fresh instance has no timers running.
	if(CTheScripts::UpsideDownCars.IsCarUpsideDown((CVehicle*)vehicle))
		return false;

	if(!((CVehicle*)vehicle)->CanBeDriven())
		return false;

	if(((CVehicle*)vehicle)->IsLawEnforcementVehicle())
		return false;

	if(vehicle->pDriver){
		CPed *drvr = vehicle->pDriver;
		// SA also refuses when the driver was created by a mission or is
		// friendly with this ped. reVC has no mission-created ped flag and no
		// acquaintance table, so only the player case can be answered here -
		// the AI's own "is this ped a threat" checks cover the rest.
		if(drvr->IsPlayer())
			return false;
	}

	if(vehicle->m_pCarFire != nil)
		return false;

	if(vehicle->m_fHealth <= 600.0f)
		return false;

	if(((CVehicle*)vehicle)->IsUpsideDown() || ((CVehicle*)vehicle)->IsOnItsSide())
		return false;

	if(!IsClearToDriveAway(vehicle))
		return false;

	return true;
}

// SA CCarEnterExit::MakeUndraggedDriverPedLeaveCar (0x64F600). SA posts a
// CEventDraggedOutCar; our event response for it is not ported yet, so the
// driver is handed the leave task directly.
void
CCarEnterExit::MakeUndraggedDriverPedLeaveCar(const CVehicle *vehicle, const CPed *pedGettingIn)
{
	CVehicle *veh = (CVehicle*)vehicle;
	if(veh->pDriver == nil)
		return;

	CPed *driver = veh->pDriver;
	(void)pedGettingIn;

	veh->bIsBeingCarJacked = true;
	if(driver->IsPlayer())
		return;	// the player decides for themselves

	driver->GetIntelligence()->AddTaskPrimaryMaybeInGroup(
		new CTaskComplexLeaveCar(veh, CAR_DOOR_LF, 0, true, false), true);
}

// SA CCarEnterExit::MakeUndraggedPassengerPedsLeaveCar (0x64F540).
void
CCarEnterExit::MakeUndraggedPassengerPedsLeaveCar(const CVehicle *targetVehicle, const CPed *draggedPed, const CPed *ped)
{
	(void)draggedPed;
	(void)ped;

	CVehicle *veh = (CVehicle*)targetVehicle;
	if(veh == nil)
		return;

	for(int32 i = 0; i < veh->m_nNumMaxPassengers; i++){
		CPed *psgr = veh->pPassengers[i];
		if(psgr == nil || psgr->IsPlayer())
			continue;

		const int32 doorId = ComputeTargetDoorToEnterAsPassenger(veh, i);
		if(doorId < 0)
			continue;
		psgr->GetIntelligence()->AddTaskPrimaryMaybeInGroup(
			new CTaskComplexLeaveCar(veh, doorId, 0, true, false), true);
	}
}

// SA CCarEnterExit::QuitEnteringCar (0x650130).
void
CCarEnterExit::QuitEnteringCar(CPed *ped, CVehicle *vehicle, int32 doorId, bool bCarWasBeingJacked)
{
	RemoveGetInAnims(ped);
	ped->RestartNonPartialAnims();

	if(!RpAnimBlendClumpGetAssociation(((CPed*)ped)->GetClump(), ANIM_ID_IDLE))
		CAnimManager::BlendAnimation(((CPed*)ped)->GetClump(), ((CPed*)ped)->m_animGroup, ANIM_ID_IDLE, 1000.0f);

	if(bCarWasBeingJacked)
		vehicle->bIsBeingCarJacked = true;

	vehicle->m_nNumGettingIn--;

	if(vehicle->IsBike() || (vehicle->m_pHandlingData && vehicle->m_pHandlingData->m_bTandemSeats)){
		switch(doorId){
		case 8:
		case 10:
			vehicle->m_nGettingInFlags |= 5;
			break;
		case 9:
		case 11:
			vehicle->m_nGettingInFlags |= 10;
			break;
		}
		vehicle->bIsBig = false;
	}else{
		switch(doorId){
		case 8:
			vehicle->m_nGettingInFlags |= 4;
			break;
		case 9:
			vehicle->m_nGettingInFlags |= 8;
			break;
		case 10:
			vehicle->m_nGettingInFlags |= vehicle->m_nNumMaxPassengers ? 1 : 3;
			break;
		case 11:
			vehicle->m_nGettingInFlags |= vehicle->m_nNumMaxPassengers ? 2 : 3;
			break;
		}
	}

	ped->bUsesCollision = false;
}

// SA CCarEnterExit::RemoveCarSitAnim (0x64F680).
void
CCarEnterExit::RemoveCarSitAnim(const CPed *ped)
{
	CAnimBlendAssociation *anim;
	for(anim = RpAnimBlendClumpGetFirstAssociation(((CPed*)ped)->GetClump(), ANIMATION_SECONDARY_TASK_ANIM);
	    anim;
	    anim = RpAnimBlendGetNextAssociation(anim, ANIMATION_SECONDARY_TASK_ANIM)){
		// ANIMATION_IS_BLEND_AUTO_REMOVE is SA's flag; the association stores
		// the internal equivalent, which SAFlagsToAssocFlags maps to
		// ANIMATION_IS_BLEND_AUTO_REMOVE.
		anim->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE;
		anim->m_BlendDelta = -1000.0f;
	}
	CAnimManager::BlendAnimation(((CPed*)ped)->GetClump(), ((CPed*)ped)->m_animGroup, ANIM_ID_IDLE, 1000.0f);
}

// SA CCarEnterExit::RemoveGetInAnims (0x64F6E0).
void
CCarEnterExit::RemoveGetInAnims(const CPed *ped)
{
	CAnimBlendAssociation *anim;
	for(anim = RpAnimBlendClumpGetFirstAssociation(((CPed*)ped)->GetClump(), ANIMATION_IS_PARTIAL);
	    anim;
	    anim = RpAnimBlendGetNextAssociation(anim, ANIMATION_IS_PARTIAL)){
		// ANIMATION_IS_BLEND_AUTO_REMOVE is SA's flag; the association stores
		// the internal equivalent, which SAFlagsToAssocFlags maps to
		// ANIMATION_IS_BLEND_AUTO_REMOVE.
		anim->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE;
		anim->m_BlendDelta = -1000.0f;
	}
}

// SA CCarEnterExit::SetAnimOffsetForEnterOrExitVehicle (0x64F860). The get-up
// and interior offsets are read out of the animation data once, on the first
// call, and reused afterwards. SA streams the two interior blocks in for that,
// which is what keeps this off the loading path.
void
CCarEnterExit::SetAnimOffsetForEnterOrExitVehicle(void)
{
	if(ms_bPedOffsetsCalculated)
		return;

	const char *animBlockNames[2] = { "int_house", "int_office" };
	int32 animBlockIdxs[2];
	for(int32 i = 0; i < 2; i++)
		animBlockIdxs[i] = CAnimManager::GetAnimationBlockIndex(animBlockNames[i]);

	// reVC addresses animation blocks through CStreaming's anim offset rather
	// than SA's IFPToModelId, so the request goes through RequestAnim. The
	// block is then already resident and can be referenced.
	for(int32 i = 0; i < 2; i++){
		if(animBlockIdxs[i] < 0)
			continue;
		CStreaming::RequestAnim(animBlockIdxs[i], STREAMFLAGS_KEEP_IN_MEMORY);
	}
	CStreaming::LoadAllRequestedModels(false);

	for(int32 i = 0; i < 2; i++){
		if(animBlockIdxs[i] < 0)
			continue;
		CAnimManager::AddAnimBlockRef(animBlockIdxs[i]);
	}

	// The dragged-out offset is a constant in SA, not an animation read.
	ms_vecPedQuickDraggedOutCarAnimOffset = CVector(-1.841797f, -0.3261719f, -0.01269531f);

	for(int32 i = 0; i < 2; i++){
		if(animBlockIdxs[i] < 0)
			continue;
		CAnimManager::RemoveAnimBlockRef(animBlockIdxs[i]);
	}

	ms_bPedOffsetsCalculated = true;
}

// SA CCarEnterExit::SetPedInCarDirect (0x650280): put a ped straight into a
// seat with no get-in animation at all. Used by the script and by the
// spawn/population code.
bool
CCarEnterExit::SetPedInCarDirect(CPed *ped, CVehicle *vehicle, int32 seatNumber, bool bAsDriver)
{
	if(vehicle == nil || ped == nil)
		return false;

	if(vehicle->IsBike() || (vehicle->m_pHandlingData && vehicle->m_pHandlingData->m_bTandemSeats)){
		// A bike has one seat per side; the driver takes the front.
		if(bAsDriver){
			vehicle->pDriver = ped;
			ped->m_pMyVehicle = vehicle;
			ped->m_vehDoor = CAR_DOOR_LF;
			vehicle->m_nGettingInFlags &= ~(4 | 8);
			vehicle->m_nGettingOutFlags &= ~(4 | 8);
		}else{
			vehicle->pPassengers[0] = ped;
			ped->m_pMyVehicle = vehicle;
			ped->m_vehDoor = CAR_DOOR_LR;
			vehicle->m_nGettingInFlags &= ~(1 | 2);
			vehicle->m_nGettingOutFlags &= ~(1 | 2);
		}
	}else{
		const int32 doorId = bAsDriver ? 10 : ComputeTargetDoorToEnterAsPassenger(vehicle, seatNumber);
		if(doorId < 0)
			return false;

		if(bAsDriver)
			vehicle->pDriver = ped;
		else
			vehicle->pPassengers[seatNumber] = ped;

		ped->m_pMyVehicle = vehicle;
		ped->m_vehDoor = (uint16)doorId;

		const int32 doorFlag = bAsDriver ? (vehicle->m_nNumMaxPassengers ? 1 : 3)
		                                 : ComputeDoorFlag(vehicle, doorId, false);
		vehicle->m_nGettingInFlags &= ~doorFlag;
		vehicle->m_nGettingOutFlags &= ~doorFlag;
	}

	ped->SetPedState(PED_DRIVING);
	ped->bUsesCollision = false;
	// The seating contract the rest of this codebase reads (WarpPedIntoCar
	// and CVehicle::SetUpDriver do the same): without bInVehicle the
	// world loop never places the ped on his seat, the objective layer
	// does not see the entry as done and hands him the enter task again
	// inside the car, and PlayerInfo keeps treating him as on foot.
	ped->bInVehicle = true;
	ped->bIsInTheAir = false;
	vehicle->RegisterReference((CEntity **) &ped->m_pMyVehicle);
	if (bAsDriver && ped->IsPlayer())
		vehicle->SetStatus(STATUS_PLAYER);
	AddInCarAnim(vehicle, ped, bAsDriver);
	return true;
}
