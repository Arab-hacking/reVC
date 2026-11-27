#ifndef __GTA_TASKSIMPLECARCLOSEDOORFROMINSIDE_H__
#define __GTA_TASKSIMPLECARCLOSEDOORFROMINSIDE_H__

#include "TaskSimple.h"
#include "AnimationEnums.h"

class CAnimBlendAssociation;
class CTaskUtilityLineUpPedWithCar;
class CVehicle;

// Once seated, reach back and pull the door shut.
class CTaskSimpleCarCloseDoorFromInside : public CTaskSimple
{
public:
	CVehicle *m_veh;
	uint32 m_door;
	CTaskUtilityLineUpPedWithCar *m_lineUpTask;
	CAnimBlendAssociation *m_anim;
	bool m_animHasFinished;

public:
	static const eTaskType Type = TASK_SIMPLE_CAR_CLOSE_DOOR_FROM_INSIDE;

	CTaskSimpleCarCloseDoorFromInside(CVehicle *veh, uint32 door, CTaskUtilityLineUpPedWithCar *lineUpTask);
	CTaskSimpleCarCloseDoorFromInside(const CTaskSimpleCarCloseDoorFromInside &other);
	~CTaskSimpleCarCloseDoorFromInside();

	static void FinishAnimCarCloseDoorFromInsideCB(CAnimBlendAssociation *anim, void *data);

	void ComputeAnimID(AssocGroupId &outGroup, AnimationId &outAnimId);
	void StartAnim(CPed *ped);
	void ProcessDoorOpen(CPed *ped);

	CTask *Clone() const { return new CTaskSimpleCarCloseDoorFromInside(*this); }
	eTaskType GetTaskType() const { return Type; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil);
	bool ProcessPed(CPed *ped);
	bool SetPedPosition(CPed *ped);
};

VALIDATE_SIZE(CTaskSimpleCarCloseDoorFromInside, 0x18);

#endif // __GTA_TASKSIMPLECARCLOSEDOORFROMINSIDE_H__
