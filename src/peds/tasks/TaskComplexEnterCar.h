#ifndef __GTA_TASKCOMPLEXENTERCAR_H__
#define __GTA_TASKCOMPLEXENTERCAR_H__

#include "TaskComplex.h"
#include "AnimationEnums.h"
#include "PedType.h" // eMoveState
#include "Vehicle.h" // CAR_DOOR_* door nodes

class CTaskUtilityLineUpPedWithCar;
class CVehicle;

// Get into the vehicle: walk to the door, line up with it, open it
// (or haul out whoever is in the way), climb in, shut the door.
// Abstract: use CTaskComplexEnterCarAsDriver or AsPassenger.
class CTaskComplexEnterCar : public CTaskComplex
{
public:
	CVehicle *m_Car;                            //< Vehicle to get into/onto
	bool m_bAsDriver : 1;                       //< Enter as driver
	bool m_bQuitAfterOpeningDoor : 1;           //< Stop after opening the door
	bool m_bQuitAfterDraggingPedOut : 1;        //< Stop after dragging the ped out from the target seat
	bool m_bCarryOnAfterFallingOff : 1;         //< [For bikes, etc] Get back up and continue driving
	int32 m_TargetDoor;                         //< Door to enter on
	int32 m_TargetDoorOppositeToFlag;
	int32 m_TargetSeat;                         //< Seat we want to be at
	int32 m_DraggedPedDownTime;
	eMoveState m_MoveState;                     //< How to approach the vehicle
	uint8 m_NumGettingInSet;
	uint8 m_CamMovementChoice;
	CVector m_TargetDoorPos;                    //< Target door's position
	CTaskUtilityLineUpPedWithCar *m_LineUpUtility;
	bool m_bAborting;                           //< Whenever MakeAbortable was called
	CPed *m_DraggedPed;                         //< The ped we're dragging out (that was at targetSeat)
	uint8 m_DoorFlagsSet;                       //< used for CVehicle::SetGettingInFlags
	float m_CruiseSpeed;                        //< How fast to go after we've entered. Only used when m_bAsDriver is set
	int32 m_EnterCarStartTime;                  //< When we started entering the vehicle

public:
	CTaskComplexEnterCar(CVehicle *targetVehicle, bool bAsDriver, bool bQuitAfterOpeningDoor, bool bQuitAfterDraggingPedOut, bool bCarryOnAfterFallingOff = false);
	CTaskComplexEnterCar(const CTaskComplexEnterCar &other);
	~CTaskComplexEnterCar();

	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil);
	CTask *CreateNextSubTask(CPed *ped);
	CTask *CreateFirstSubTask(CPed *ped);
	CTask *ControlSubTask(CPed *ped);
	CTask *CreateSubTask(eTaskType type, CPed *ped);
	CVector GetTargetPos() const;
	CVehicle *GetCameraAvoidVehicle();

	CVehicle *GetTargetCar() const { return m_Car; }
	void SetMoveState(eMoveState ms) { m_MoveState = ms; }

protected:
	virtual CTask *CreateNextSubTask_AfterSimpleCarAlign(CPed *ped);

	void SetVehicleFlags(CPed *ped);
	bool IsTargetDoorOnTheLeft() const { return m_TargetDoor == CAR_DOOR_LF || m_TargetDoor == CAR_DOOR_LR; }
	int32 ComputeTargetDoorOppositeToFlag() const;

	void PreparePedForVehicleEnter(CPed *ped);
	void PrepareVehicleForPedEnter(CPed *ped);
	void CreateTaskUtilityLineUpPedWithCar(CPed *ped);

	CTask *CreateDragPedOutSubTask(CPed *ped);
};

VALIDATE_SIZE(CTaskComplexEnterCar, 0x50);

#endif // __GTA_TASKCOMPLEXENTERCAR_H__
