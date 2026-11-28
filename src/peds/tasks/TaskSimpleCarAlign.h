#ifndef __GTA_TASKCARSIMPLEALIGN_H__
#define __GTA_TASKCARSIMPLEALIGN_H__

#include "TaskSimple.h"
#include "Vector.h"

class CAnimBlendAssociation;
class CTaskUtilityLineUpPedWithCar;
class CVehicle;

// Play the align animation that turns the ped side-on to the car and
// settles him at the door, ready to open it. While the animation runs
// the line-up utility keeps him pinned to the door.
class CTaskSimpleCarAlign : public CTaskSimple
{
public:
	bool m_animHasFinished;
	CAnimBlendAssociation *m_anim;
	CVehicle *m_veh;
	CVector m_doorPos;
	uint32 m_door;
	CTaskUtilityLineUpPedWithCar *m_lineUpTask;

public:
	static const eTaskType Type = TASK_SIMPLE_CAR_ALIGN;

	CTaskSimpleCarAlign(CVehicle *veh, const CVector &doorPos, uint32 targetDoor, CTaskUtilityLineUpPedWithCar *taskLineUp);
	CTaskSimpleCarAlign(const CTaskSimpleCarAlign &other);
	~CTaskSimpleCarAlign();

	static void FinishAnimCarAlignCB(CAnimBlendAssociation *anim, void *data);

	void FixHeading(CPed *ped);
	void StartAnim(CPed *ped);

	CTask *Clone() const { return new CTaskSimpleCarAlign(*this); }
	eTaskType GetTaskType() const { return Type; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil);
	bool ProcessPed(CPed *ped);
	bool SetPedPosition(CPed *ped);
};

VALIDATE_SIZE(CTaskSimpleCarAlign, 0x28);

#endif // __GTA_TASKCARSIMPLEALIGN_H__
