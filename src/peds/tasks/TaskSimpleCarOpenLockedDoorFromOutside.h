#ifndef __GTA_TASKSIMPLECAROPENLOCKEDDOORFROMOUTSIDE_H__
#define __GTA_TASKSIMPLECAROPENLOCKEDDOORFROMOUTSIDE_H__

#include "TaskSimple.h"

class CAnimBlendAssociation;
class CTaskUtilityLineUpPedWithCar;
class CVehicle;

// The ped tries the handle of a locked door and rattles it: the entry
// stops here (the complex task decides what that means).
class CTaskSimpleCarOpenLockedDoorFromOutside : public CTaskSimple
{
public:
	CVehicle *m_veh;
	uint32 m_door;
	CTaskUtilityLineUpPedWithCar *m_lineUpUtil;
	CAnimBlendAssociation *m_anim;
	bool m_hasAnimFinished;

public:
	static const eTaskType Type = TASK_SIMPLE_CAR_OPEN_LOCKED_DOOR_FROM_OUTSIDE;

	CTaskSimpleCarOpenLockedDoorFromOutside(CVehicle *veh, uint32 door, CTaskUtilityLineUpPedWithCar *lineUpUtil);
	CTaskSimpleCarOpenLockedDoorFromOutside(const CTaskSimpleCarOpenLockedDoorFromOutside &other);
	~CTaskSimpleCarOpenLockedDoorFromOutside();

	static void FinishAnimCarOpenLockedDoorFromOutsideCB(CAnimBlendAssociation *anim, void *data);

	void StartAnim(CPed *ped);

	CTask *Clone() const { return new CTaskSimpleCarOpenLockedDoorFromOutside(*this); }
	eTaskType GetTaskType() const { return Type; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil);
	bool ProcessPed(CPed *ped);
	bool SetPedPosition(CPed *ped);
};

VALIDATE_SIZE(CTaskSimpleCarOpenLockedDoorFromOutside, 0x18);

#endif // __GTA_TASKSIMPLECAROPENLOCKEDDOORFROMOUTSIDE_H__
