#ifndef __GTA_TASKCOMPLEXGOTOCARDOORANDSTANDSTILL_H__
#define __GTA_TASKCOMPLEXGOTOCARDOORANDSTANDSTILL_H__

#include "TaskComplex.h"
#include "TaskTimer.h"
#include "PedType.h"
#include "Vector.h"

class CPointRoute;
class CVehicle;

// Get the ped to the door of the vehicle he is going to enter and stand
// still there: straight at the door when the way is clear, around the
// car's bounding box when it is not, and via a point just past the door
// when the door itself is still being used by somebody else.
class CTaskComplexGoToCarDoorAndStandStill : public CTaskComplex
{
public:
	CVehicle *m_Vehicle;
	eMoveState m_MoveState;
	bool m_bIsDriver;
	float m_TargetRadius;
	float m_SlowDownDist;
	float m_MaxSeekDist;
	int32 m_MaxSeekTime;
	CTaskTimer m_SeekTimer;
	int32 m_TargetDoor;
	CVector m_TargetPt;
	int32 m_TargetSeat;
	bool m_bTryingToEnterInWater;
	bool m_bAchievedTargetDoor;
	CPointRoute *m_RouteToDoor;

public:
	static const eTaskType Type = TASK_COMPLEX_GO_TO_CAR_DOOR_AND_STAND_STILL;

	CTaskComplexGoToCarDoorAndStandStill(CVehicle *vehicle, eMoveState moveState, bool bIsDriver, int32 targetSeat,
		float targetRadius, float slowDownDist, float maxSeekDist, int32 maxSeekTime);
	CTaskComplexGoToCarDoorAndStandStill(const CTaskComplexGoToCarDoorAndStandStill &other);
	~CTaskComplexGoToCarDoorAndStandStill();

	CTask *Clone() const { return new CTaskComplexGoToCarDoorAndStandStill(*this); }
	eTaskType GetTaskType() const { return Type; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil);
	CTask *CreateNextSubTask(CPed *ped);
	CTask *CreateFirstSubTask(CPed *ped);
	CTask *ControlSubTask(CPed *ped);

	CVector GetTargetPt() const { return m_TargetPt; }
	void SetTargetPt(CVector pt) { m_TargetPt = pt; }

	void SetTargetDoor(int32 door) { m_TargetDoor = door; }
	int32 GetTargetDoor() const { return m_TargetDoor; }

	bool HasAchievedDoor() const { return m_bAchievedTargetDoor; }

	void SetTryingToEnterInWater(bool v) { m_bTryingToEnterInWater = v; }

protected:
	CTask *CreateSubTask(eTaskType taskType, CPed *ped);
	bool IsVehicleInRange(const CPed &ped);
	void ComputeRouteToDoor(const CPed &ped);
	bool CalculateTargetDoor(CPed *ped, bool &bCanWaitForDoorToBeFree);
};

VALIDATE_SIZE(CTaskComplexGoToCarDoorAndStandStill, 0x50);

#endif // __GTA_TASKCOMPLEXGOTOCARDOORANDSTANDSTILL_H__
