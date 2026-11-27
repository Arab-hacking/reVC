#ifndef __GTA_TASKSIMPLECARSLOWBEDRAGGEDOUT_H__
#define __GTA_TASKSIMPLECARSLOWBEDRAGGEDOUT_H__

#include "TaskSimple.h"
#include "AnimationEnums.h"

class CAnimBlendAssociation;
class CTaskUtilityLineUpPedWithCar;
class CVehicle;

// The victim's side of the slow drag-out: played while seated, it
// blends out of the seat and ends up lying where the jacker left him.
class CTaskSimpleCarSlowBeDraggedOut : public CTaskSimple
{
public:
	bool m_HasAnimFinished;
	CAnimBlendAssociation *m_Anim;
	CVehicle *m_Vehicle;
	int32 m_Door;
	CTaskUtilityLineUpPedWithCar *m_LineUpUtility;
	bool m_IsDraggedBySelf;
	bool m_FrontAnim;

public:
	static const eTaskType Type = TASK_SIMPLE_CAR_SLOW_BE_DRAGGED_OUT;

	CTaskSimpleCarSlowBeDraggedOut(CVehicle *target, int32 door, CTaskUtilityLineUpPedWithCar *lineUpUtility, bool isDraggedBySelf);
	CTaskSimpleCarSlowBeDraggedOut(const CTaskSimpleCarSlowBeDraggedOut &other);
	~CTaskSimpleCarSlowBeDraggedOut();

	void ComputeAnimID(AssocGroupId &outGroup, AnimationId &outAnimId);
	void StartAnim(CPed *ped);

	CTask *Clone() const { return new CTaskSimpleCarSlowBeDraggedOut(*this); }
	eTaskType GetTaskType() const { return Type; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil);
	bool ProcessPed(CPed *ped);
	bool SetPedPosition(CPed *ped);

private:
	void DoOpenCarDoor(CPed *ped);
	static void FinishAnimCarSlowBeDraggedOutCB(CAnimBlendAssociation *anim, void *data);
};

VALIDATE_SIZE(CTaskSimpleCarSlowBeDraggedOut, 0x20);

#endif // __GTA_TASKSIMPLECARSLOWBEDRAGGEDOUT_H__
