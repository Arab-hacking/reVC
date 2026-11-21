#ifndef __GTA_CARENTEREXIT_H__
#define __GTA_CARENTEREXIT_H__

class CPed;
class CVehicle;

// SA CCarEnterExit. Everything that decides whether, where and how a ped gets
// into or out of a vehicle.
//
// In Vice City this lived scattered over CPed (SetEnterCar, EnterCar,
// ExitCar, GetPositionToOpenCarDoor, GetNearestDoor...) and CVehicle. San
// Andreas pulled every one of those decisions into a single stateless class so
// that the tasks, the AI and the script opcodes could all ask the same
// questions and get the same answers. That is what this file restores: the ped
// code keeps its state machine, but the door arithmetic moves here.
//
// Door ids are the eCarNodes door values, exactly as in SA:
//   CAR_DOOR_LF 8, CAR_DOOR_RF 9, CAR_DOOR_RR 10, CAR_DOOR_LR 11,
//   18 is the bike/quad "left side" pseudo door used for fast jacking.
class CCarEnterExit
{
public:
	// SA CCarEnterExit::ms_fMaxSpeed_CanDragPedOut = 0.1, and the faster
	// player variant = 0.2.
	static float ms_fMaxSpeed_CanDragPedOut;
	static float ms_fMaxSpeed_PlayerCanDragPedOut;

	// SA CCarEnterExit::ms_bPedOffsetsCalculated. The offsets below are read
	// out of the get-in / get-out animations once and then reused.
	static bool ms_bPedOffsetsCalculated;

	static CVector ms_vecPedGetUpAnimOffset;
	static CVector ms_vecPedBedLAnimOffset;
	static CVector ms_vecPedBedRAnimOffset;
	static CVector ms_vecPedDeskAnimOffset;
	static CVector ms_vecPedChairAnimOffset;
	static CVector ms_vecPedQuickDraggedOutCarAnimOffset;

public:
	static void Initialise(void);

	// ---- animation helpers -------------------------------------------------
	static void AddInCarAnim(const CVehicle *vehicle, CPed *ped, bool bAsDriver);
	static void RemoveCarSitAnim(const CPed *ped);
	static void RemoveGetInAnims(const CPed *ped);
	static void SetAnimOffsetForEnterOrExitVehicle(void);

	// ---- door queries ------------------------------------------------------
	static bool CarHasDoorToClose(const CVehicle *vehicle, int32 doorId);
	static bool CarHasDoorToOpen(const CVehicle *vehicle, int32 doorId);
	static bool CarHasOpenableDoor(const CVehicle *vehicle, int32 doorId, const CPed *ped);
	static bool CarHasPartiallyOpenDoor(const CVehicle *vehicle, int32 doorId);
	static bool IsCarDoorInUse(const CVehicle *vehicle, int32 firstDoorId, int32 secondDoorId);
	static bool IsCarDoorReady(const CVehicle *vehicle, int32 doorId);
	static bool IsCarQuickJackPossible(CVehicle *vehicle, int32 doorId, const CPed *ped);
	static bool IsCarSlowJackRequired(const CVehicle *vehicle, int32 doorId);

	// ---- flag and index arithmetic ----------------------------------------
	static int32 ComputeDoorFlag(const CVehicle *vehicle, int32 doorId, bool bCheckVehicleType);
	static int32 ComputeOppositeDoorFlag(const CVehicle *vehicle, int32 doorId, bool bCheckVehicleType);
	static int32 ComputePassengerIndexFromCarDoor(const CVehicle *vehicle, int32 doorId);
	static CPed *ComputeSlowJackedPed(const CVehicle *vehicle, int32 doorId);
	static int32 ComputeTargetDoorToEnterAsPassenger(const CVehicle *vehicle, int32 passengerNum);
	static int32 ComputeTargetDoorToExit(const CVehicle *vehicle, const CPed *ped);

	// ---- door discovery ----------------------------------------------------
	static bool GetNearestCarDoor(const CPed *ped, const CVehicle *vehicle, CVector &outPos, int32 &doorId);
	static bool GetNearestCarPassengerDoor(const CPed *ped, const CVehicle *vehicle, CVector *outVec, int32 *doorId,
		bool checkIfOccupiedTandemSeat, bool checkIfDoorIsEnterable, bool checkIfRoomToGetIn);
	static CVector GetPositionToOpenCarDoor(const CVehicle *vehicle, int32 doorId);
	static CVector GetPositionToOpenCarDoor(const CVehicle *vehicle, int32 doorId, const CPed *ped);

	// ---- go / no-go --------------------------------------------------------
	static bool IsClearToDriveAway(const CVehicle *vehicle);
	static bool IsPathToDoorBlockedByVehicleCollisionModel(const CPed *ped, const CVehicle *vehicle, const CVector &pos);
	static bool IsPedHealthy(CPed *ped);
	static bool IsRoomForPedToLeaveCar(const CVehicle *vehicle, int32 doorId, const CVector *offset = nil);
	static bool IsVehicleHealthy(const CVehicle *vehicle);
	static bool IsVehicleStealable(const CVehicle *vehicle, const CPed *ped);

	// ---- state changes -----------------------------------------------------
	static void MakeUndraggedDriverPedLeaveCar(const CVehicle *vehicle, const CPed *ped);
	static void MakeUndraggedPassengerPedsLeaveCar(const CVehicle *targetVehicle, const CPed *draggedPed, const CPed *ped);
	static void QuitEnteringCar(CPed *ped, CVehicle *vehicle, int32 doorId, bool bCarWasBeingJacked);
	static bool SetPedInCarDirect(CPed *ped, CVehicle *vehicle, int32 seatNumber, bool bAsDriver);
};

#endif // __GTA_CARENTEREXIT_H__
