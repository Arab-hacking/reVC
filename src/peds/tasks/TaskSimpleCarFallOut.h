#ifndef __GTA_TASKSIMPLECARFALLOUT_H__
#define __GTA_TASKSIMPLECARFALLOUT_H__

#include "TaskSimple.h"
#include "AnimationEnums.h"

class CAnimBlendAssociation;
class CTaskUtilityLineUpPedWithCar;
class CVehicle;

// The car rolls over or the door comes off and the ped simply falls
// out of his seat.
class CTaskSimpleCarFallOut : public CTaskSimple
{
public:
	bool m_HasFinished;
	CAnimBlendAssociation *m_Anim;
	CVehicle *m_Vehicle;
	int32 m_Door;
	CTaskUtilityLineUpPedWithCar *m_LineUpUtility;

public:
	static const eTaskType Type = TASK_SIMPLE_CAR_FALL_OUT;

	CTaskSimpleCarFallOut(CVehicle *target, int32 door, CTaskUtilityLineUpPedWithCar *lineUpUtility);
	CTaskSimpleCarFallOut(const CTaskSimpleCarFallOut &other);
	~CTaskSimpleCarFallOut();

	void ComputeAnimID(AssocGroupId &outGroup, AnimationId &outAnimId);
	void StartAnim(const CPed *ped);

	CTask *Clone() const { return new CTaskSimpleCarFallOut(*this); }
	eTaskType GetTaskType() const { return Type; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil);
	bool ProcessPed(CPed *ped);
	bool SetPedPosition(CPed *ped);

private:
	void DoOpenCarDoor(CPed *ped);
	static void FinishAnimFallOutCB(CAnimBlendAssociation *anim, void *data);
};

VALIDATE_SIZE(CTaskSimpleCarFallOut, 0x1C);

#endif // __GTA_TASKSIMPLECARFALLOUT_H__
