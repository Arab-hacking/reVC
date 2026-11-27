#ifndef __GTA_TASKSIMPLECARJUMPOUT_H__
#define __GTA_TASKSIMPLECARJUMPOUT_H__

#include "TaskSimple.h"
#include "AnimationEnums.h"

class CAnimBlendAssociation;
class CTaskUtilityLineUpPedWithCar;
class CVehicle;

// The car is moving too fast to get out of properly: dive out and let
// the ground sort it out.
class CTaskSimpleCarJumpOut : public CTaskSimple
{
public:
	bool m_finishedAnim;
	CAnimBlendAssociation *m_anim;
	CVehicle *m_veh;
	float m_initialVehSpeed;
	uint32 m_door;
	uint8 m_doorFlagsToClear;
	uint8 m_numGettingInToClear;
	CTaskUtilityLineUpPedWithCar *m_lineUpTask;

public:
	static const eTaskType Type = TASK_SIMPLE_CAR_JUMP_OUT;

	CTaskSimpleCarJumpOut(CVehicle *veh, uint32 door, CTaskUtilityLineUpPedWithCar *lineUpTask);
	CTaskSimpleCarJumpOut(const CTaskSimpleCarJumpOut &other);
	~CTaskSimpleCarJumpOut();

	static void FinishAnimCarJumpOutCB(CAnimBlendAssociation *anim, void *data);

	void StartAnim(const CPed *ped);
	void ComputeAnimID(AssocGroupId &outGroup, AnimationId &outAnimId);

	CTask *Clone() const { return new CTaskSimpleCarJumpOut(*this); }
	eTaskType GetTaskType() const { return Type; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil);
	bool ProcessPed(CPed *ped);
	bool SetPedPosition(CPed *ped);
};

VALIDATE_SIZE(CTaskSimpleCarJumpOut, 0x24);

#endif // __GTA_TASKSIMPLECARJUMPOUT_H__
