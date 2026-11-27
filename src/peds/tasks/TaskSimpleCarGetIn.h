#ifndef __GTA_TASKSIMPLECARGETIN_H__
#define __GTA_TASKSIMPLECARGETIN_H__

#include "TaskSimple.h"

class CAnimBlendAssociation;
class CTaskUtilityLineUpPedWithCar;
class CVehicle;

// Play the get-in animation for the door: the ped swings in and takes
// his seat; the line-up utility keeps him attached to the door while it
// plays.
class CTaskSimpleCarGetIn : public CTaskSimple
{
public:
	CVehicle *m_veh;
	uint32 m_door;
	CTaskUtilityLineUpPedWithCar *m_lineUpTask;
	CAnimBlendAssociation *m_anim;
	bool m_finished;

public:
	static const eTaskType Type = TASK_SIMPLE_CAR_GET_IN;

	CTaskSimpleCarGetIn(CVehicle *vehicle, uint32 door, CTaskUtilityLineUpPedWithCar *task);
	CTaskSimpleCarGetIn(const CTaskSimpleCarGetIn &other);
	~CTaskSimpleCarGetIn();

	static void FinishAnimCarGetInCB(CAnimBlendAssociation *anim, void *data);

	void StartAnim(const CPed *ped);

	CTask *Clone() const { return new CTaskSimpleCarGetIn(*this); }
	eTaskType GetTaskType() const { return Type; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil);
	bool ProcessPed(CPed *ped);
	bool SetPedPosition(CPed *ped);
};

VALIDATE_SIZE(CTaskSimpleCarGetIn, 0x18);

#endif // __GTA_TASKSIMPLECARGETIN_H__
