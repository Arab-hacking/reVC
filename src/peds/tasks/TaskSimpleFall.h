#ifndef __GTA_TASKSIMPLEFALL_H__
#define __GTA_TASKSIMPLEFALL_H__

#include "TaskSimple.h"
#include "AnimationEnums.h"

class CAnimBlendAssociation;

// Fall over and stay down for a while.
class CTaskSimpleFall : public CTaskSimple
{
public:
	CAnimBlendAssociation *m_pAnim;
	AnimationId m_nAnimId;
	AssocGroupId m_nAnimGroup;
	int32 m_nTotalDownTime;
	int32 m_nCurrentDownTime;
	bool m_bIsFinished;

	// How long the player may be kept down before his input gets
	// him up again; the reference keeps this in a global the fall
	// tasks share, initialised to the usual fall-down time.
	static const int32 m_nMaxPlayerDownTime = 2000;

public:
	static const eTaskType Type = TASK_SIMPLE_FALL;

	CTaskSimpleFall(AnimationId nAnimId, AssocGroupId nAnimGroup, int32 nDownTime);
	CTaskSimpleFall(const CTaskSimpleFall &other);
	~CTaskSimpleFall();

	static void FinishFallAnimCB(CAnimBlendAssociation *blendAssoc, void *data);

	bool StartAnim(CPed *ped);
	void ProcessFall(CPed *ped);

	CTask *Clone() const { return new CTaskSimpleFall(*this); }
	eTaskType GetTaskType() const { return Type; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil);
	bool ProcessPed(CPed *ped);
};

VALIDATE_SIZE(CTaskSimpleFall, 0x20);

#endif // __GTA_TASKSIMPLEFALL_H__
