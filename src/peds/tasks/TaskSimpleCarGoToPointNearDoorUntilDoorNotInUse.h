#ifndef __GTA_TASKCARGOTOPOINTNEARDOORUNTILDOORNOTINUSE_H__
#define __GTA_TASKCARGOTOPOINTNEARDOORUNTILDOORNOTINUSE_H__

#include "TaskSimple.h"
#include "PedType.h"
#include "Vector.h"

class CTaskSimpleGoToPoint;
class CTaskSimpleCarWaitForDoorNotToBeInUse;
class CVehicle;

// Walk to a point just past the door of the vehicle (the door-open
// position, doubled up from the car's centre, so the ped ends up clear
// of the door's swing), and wait there until the door is free to use.
class CTaskSimpleCarGoToPointNearDoorUntilDoorNotInUse : public CTaskSimple
{
public:
	CTaskSimpleGoToPoint *m_TaskGoToPoint;
	CTaskSimpleCarWaitForDoorNotToBeInUse *m_TaskWaitForDoor;

public:
	static const eTaskType Type = TASK_SIMPLE_GO_TO_POINT_NEAR_CAR_DOOR_UNTIL_DOOR_NOT_IN_USE;

	CTaskSimpleCarGoToPointNearDoorUntilDoorNotInUse(CVehicle *vehicle, int32 targetDoor, const CVector &targetPt, eMoveState moveState);
	CTaskSimpleCarGoToPointNearDoorUntilDoorNotInUse(const CTaskSimpleCarGoToPointNearDoorUntilDoorNotInUse &other);
	~CTaskSimpleCarGoToPointNearDoorUntilDoorNotInUse();

	CTask *Clone() const { return new CTaskSimpleCarGoToPointNearDoorUntilDoorNotInUse(*this); }
	eTaskType GetTaskType() const { return Type; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil) { return true; }
	bool ProcessPed(CPed *ped);

protected:
	CVector ComputeTarget(CPed *ped) const;
};

VALIDATE_SIZE(CTaskSimpleCarGoToPointNearDoorUntilDoorNotInUse, 0x10);

#endif // __GTA_TASKCARGOTOPOINTNEARDOORUNTILDOORNOTINUSE_H__
