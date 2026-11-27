#ifndef __GTA_TASKSIMPLECARGETOUT_H__
#define __GTA_TASKSIMPLECARGETOUT_H__

#include "TaskSimple.h"
#include "AnimationEnums.h"

class CAnimBlendAssociation;
class CTaskUtilityLineUpPedWithCar;
class CVehicle;

// Climb out of the car through the door, swinging it on the way.
class CTaskSimpleCarGetOut : public CTaskSimple
{
public:
	bool m_finished;
	bool m_vehHasDoorToOpen;
	CAnimBlendAssociation *m_anim;
	bool m_isUpsideDown;
	CVehicle *m_veh;
	uint32 m_door;
	CTaskUtilityLineUpPedWithCar *m_taskLineUp;

public:
	static const eTaskType Type = TASK_SIMPLE_CAR_GET_OUT;

	CTaskSimpleCarGetOut(CVehicle *veh, uint32 door, CTaskUtilityLineUpPedWithCar *taskLineUp);
	CTaskSimpleCarGetOut(const CTaskSimpleCarGetOut &other);
	~CTaskSimpleCarGetOut();

	static void FinishAnimCarGetOutCB(CAnimBlendAssociation *anim, void *data);

	void ComputeAnimID(AssocGroupId &outGroup, AnimationId &outAnimId);
	void StartAnim(const CPed *ped);

	CTask *Clone() const { return new CTaskSimpleCarGetOut(*this); }
	eTaskType GetTaskType() const { return Type; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil);
	bool ProcessPed(CPed *ped);
	bool SetPedPosition(CPed *ped);
};

VALIDATE_SIZE(CTaskSimpleCarGetOut, 0x20);

#endif // __GTA_TASKSIMPLECARGETOUT_H__
