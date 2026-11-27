#ifndef __GTA_TASKSIMPLECARSHUFFLE_H__
#define __GTA_TASKSIMPLECARSHUFFLE_H__

#include "TaskSimple.h"

class CAnimBlendAssociation;
class CTaskUtilityLineUpPedWithCar;
class CVehicle;

// Slide across the bench from the passenger side into the driver's
// seat.
class CTaskSimpleCarShuffle : public CTaskSimple
{
public:
	CVehicle *m_Car;
	int32 m_TargetDoor;
	CTaskUtilityLineUpPedWithCar *m_LineUpUtility;
	CAnimBlendAssociation *m_Anim;
	bool m_IsFinished;

public:
	static const eTaskType Type = TASK_SIMPLE_CAR_SHUFFLE;

	CTaskSimpleCarShuffle(CVehicle *car, int32 targetDoor, CTaskUtilityLineUpPedWithCar *lineUpTask);
	CTaskSimpleCarShuffle(const CTaskSimpleCarShuffle &other);
	~CTaskSimpleCarShuffle();

	static void FinishAnimCarShuffleCB(CAnimBlendAssociation *anim, void *data);

	void StartAnim(CPed *target);

	CTask *Clone() const { return new CTaskSimpleCarShuffle(*this); }
	eTaskType GetTaskType() const { return Type; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil);
	bool ProcessPed(CPed *ped);
	bool SetPedPosition(CPed *ped);
};

VALIDATE_SIZE(CTaskSimpleCarShuffle, 0x1C);

#endif // __GTA_TASKSIMPLECARSHUFFLE_H__
