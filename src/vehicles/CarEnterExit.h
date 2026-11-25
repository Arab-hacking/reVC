#ifndef __GTA_CARENTEREXIT_H__
#define __GTA_CARENTEREXIT_H__

class CPed;
class CVehicle;

// SA CCarEnterExit. The door arithmetic the ped code asks about when it gets
// into or out of a vehicle: which door to walk to, whether there is room to
// open it, and which flags a door maps to.
//
// In Vice City this lived scattered over CPed (SetEnterCar, EnterCar,
// ExitCar, GetPositionToOpenCarDoor, GetNearestDoor...) and CVehicle. San
// Andreas pulled every one of those decisions into a single stateless class so
// that the tasks, the AI and the script opcodes could all ask the same
// questions and get the same answers. That is what this file keeps: the ped
// code owns its state machine, the door arithmetic lives here.
//
// Door ids are the eCarNodes door values, exactly as in SA:
//   CAR_DOOR_LF 8, CAR_DOOR_RF 9, CAR_DOOR_RR 10, CAR_DOOR_LR 11,
//   18 is the bike/quad "left side" pseudo door used for fast jacking.
class CCarEnterExit
{
public:
	// ---- door queries ------------------------------------------------------
	static bool CarHasOpenableDoor(const CVehicle *vehicle, int32 doorId, const CPed *ped);
	static bool IsCarDoorReady(const CVehicle *vehicle, int32 doorId);

	// ---- flag and index arithmetic ----------------------------------------
	static int32 ComputeDoorFlag(const CVehicle *vehicle, int32 doorId, bool bCheckVehicleType);
	static CPed *ComputeSlowJackedPed(const CVehicle *vehicle, int32 doorId);

	// ---- door discovery ----------------------------------------------------
	static bool GetNearestCarDoor(const CPed *ped, const CVehicle *vehicle, CVector &outPos, int32 &doorId);
	static bool GetNearestCarPassengerDoor(const CPed *ped, const CVehicle *vehicle, CVector *outVec, int32 *doorId,
		bool checkIfOccupiedTandemSeat, bool checkIfDoorIsEnterable, bool checkIfRoomToGetIn);
	static CVector GetPositionToOpenCarDoor(const CVehicle *vehicle, int32 doorId);
	static CVector GetPositionToOpenCarDoor(const CVehicle *vehicle, int32 doorId, const CPed *ped);

	// ---- go / no-go --------------------------------------------------------
	static bool IsPathToDoorBlockedByVehicleCollisionModel(const CPed *ped, const CVehicle *vehicle, const CVector &pos);
	static bool IsRoomForPedToLeaveCar(const CVehicle *vehicle, int32 doorId);
};

#endif // __GTA_CARENTEREXIT_H__
