#ifndef __GTA_TASKSIMPLECARCLOSEDOORFROMOUTSIDE_H__
#define __GTA_TASKSIMPLECARCLOSEDOORFROMOUTSIDE_H__

#include "TaskSimple.h"

class CAnimBlendAssociation;
class CTaskUtilityLineUpPedWithCar;
class CVehicle;
class CAutoSinglePart;

// Just stepped out: close the door behind you.
class CTaskSimpleCarCloseDoorFromOutside : public CTaskSimple
{
public:
	CVehicle *m_veh;
	uint32 m_door;
	CTaskUtilityLineUpPedWithCar *m_lineUpTask;
	CAnimBlendAssociation *m_anim{};
	bool m_animFinished{ false };
	CAutoSinglePart *m_taskStoreAnim;

public:
	static const eTaskType Type = TASK_SIMPLE_CAR_CLOSE_DOOR_FROM_OUTSIDE;

	CTaskSimpleCarCloseDoorFromOutside(CVehicle *veh, uint32 door, CTaskUtilityLineUpPedWithCar *lineUpTask);
	CTaskSimpleCarCloseDoorFromOutside(const CTaskSimpleCarCloseDoorFromOutside &other);
	~CTaskSimpleCarCloseDoorFromOutside();

	static void FinishAnimCarCloseDoorFromOutsideCB(CAnimBlendAssociation *anim, void *data);

	void StartAnim(CPed *ped);
	void ProcessDoorOpen();

	CTask *Clone() const { return new CTaskSimpleCarCloseDoorFromOutside(*this); }
	eTaskType GetTaskType() const { return Type; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil);
	bool ProcessPed(CPed *ped);
	bool SetPedPosition(CPed *ped);
};

VALIDATE_SIZE(CTaskSimpleCarCloseDoorFromOutside, 0x20);

#endif // __GTA_TASKSIMPLECARCLOSEDOORFROMOUTSIDE_H__
