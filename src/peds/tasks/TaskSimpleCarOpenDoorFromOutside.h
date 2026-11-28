#ifndef __GTA_TASKSIMPLECAROPENDOORFROMOUTSIDE_H__
#define __GTA_TASKSIMPLECAROPENDOORFROMOUTSIDE_H__

#include "TaskSimple.h"
#include "AnimationEnums.h"

class CAnimBlendAssociation;
class CTaskUtilityLineUpPedWithCar;
class CVehicle;

// The ped pulls the door open from outside: play the open animation and
// swing the door with it once the animation reaches the door part. If
// he is opening the player's car to drag the player out, the player is
// kept from exiting until this is over.
class CTaskSimpleCarOpenDoorFromOutside : public CTaskSimple
{
public:
	CVehicle *m_veh;
	uint32 m_door;
	CTaskUtilityLineUpPedWithCar *m_lineUpTask;
	CAnimBlendAssociation *m_anim;
	bool m_finished;
	bool m_disallowPlayerDriverToExitCar;
	float m_doorOpenAngleRatio;

public:
	static const eTaskType Type = TASK_SIMPLE_CAR_OPEN_DOOR_FROM_OUTSIDE;

	CTaskSimpleCarOpenDoorFromOutside(CVehicle *veh, uint32 door, CTaskUtilityLineUpPedWithCar *lineUpTask, bool disallowPlayerDriverToExitCar);
	CTaskSimpleCarOpenDoorFromOutside(const CTaskSimpleCarOpenDoorFromOutside &other);
	~CTaskSimpleCarOpenDoorFromOutside();

	static void FinishAnimCarOpenDoorFromOutsideCB(CAnimBlendAssociation *anim, void *data);

	void ComputeAnimID(AssocGroupId &outGroup, AnimationId &outAnimId);
	void StartAnim(CPed *ped);

	CTask *Clone() const { return new CTaskSimpleCarOpenDoorFromOutside(*this); }
	eTaskType GetTaskType() const { return Type; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil);
	bool ProcessPed(CPed *ped);
	bool SetPedPosition(CPed *ped);
};

VALIDATE_SIZE(CTaskSimpleCarOpenDoorFromOutside, 0x20);

#endif // __GTA_TASKSIMPLECAROPENDOORFROMOUTSIDE_H__
