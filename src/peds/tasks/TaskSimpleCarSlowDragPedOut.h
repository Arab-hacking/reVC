#ifndef __GTA_TASKSIMPLECARSLOWDRAGPEDOUT_H__
#define __GTA_TASKSIMPLECARSLOWDRAGPEDOUT_H__

#include "TaskSimple.h"
#include "AnimationEnums.h"

class CAnimBlendAssociation;
class CTaskUtilityLineUpPedWithCar;
class CVehicle;

// Walk up to the car, open the door and haul whoever is sitting
// behind it out onto the ground. The victim's own task is the
// matching be-dragged-out complex; this one plays the pull.
class CTaskSimpleCarSlowDragPedOut : public CTaskSimple
{
public:
	bool m_bAnimFinished;
	CAnimBlendAssociation *m_AnimAssoc;
	CVehicle *m_Vehicle;
	int32 m_TargetDoor;
	CTaskUtilityLineUpPedWithCar *m_LineUpPedWithCarTask;
	bool m_bWasPedStatic;

public:
	static const eTaskType Type = TASK_SIMPLE_CAR_SLOW_DRAG_PED_OUT;

	CTaskSimpleCarSlowDragPedOut(CVehicle *vehicle, int32 targetDoor, CTaskUtilityLineUpPedWithCar *lineUpPedWithCarTask, bool isPedStatic);
	CTaskSimpleCarSlowDragPedOut(const CTaskSimpleCarSlowDragPedOut &other);
	~CTaskSimpleCarSlowDragPedOut();

	static void FinishAnimCarSlowDragPedOutCB(CAnimBlendAssociation *anim, void *task);

	void ComputeAnimID(AssocGroupId &outGroup, AnimationId &outAnimId);
	void StartAnim(CPed *ped);
	CPed *GetJackedPed() const;

	CTask *Clone() const { return new CTaskSimpleCarSlowDragPedOut(*this); }
	eTaskType GetTaskType() const { return Type; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil);
	bool ProcessPed(CPed *ped);
	bool SetPedPosition(CPed *ped);
};

VALIDATE_SIZE(CTaskSimpleCarSlowDragPedOut, 0x20);

#endif // __GTA_TASKSIMPLECARSLOWDRAGPEDOUT_H__
