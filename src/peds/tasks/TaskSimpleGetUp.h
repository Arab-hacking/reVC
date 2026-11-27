#ifndef __GTA_TASKSIMPLEGETUP_H__
#define __GTA_TASKSIMPLEGETUP_H__

#include "TaskSimple.h"

class CAnimBlendAssociation;

// Knocked flat: get back on your feet (unless something heavy is
// sitting on you, which hurts instead).
class CTaskSimpleGetUp : public CTaskSimple
{
public:
	bool m_bHasPedGotUp;
	bool m_bIsFinished;
	CAnimBlendAssociation *m_Anim;

public:
	static const eTaskType Type = TASK_SIMPLE_GET_UP;

	CTaskSimpleGetUp();
	CTaskSimpleGetUp(const CTaskSimpleGetUp &other);
	~CTaskSimpleGetUp();

	static void FinishGetUpAnimCB(CAnimBlendAssociation *blendAssoc, void *data);

	bool StartAnim(CPed *ped);

	CTask *Clone() const { return new CTaskSimpleGetUp(*this); }
	eTaskType GetTaskType() const { return Type; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil);
	bool ProcessPed(CPed *ped);
};

VALIDATE_SIZE(CTaskSimpleGetUp, 0x10);

#endif // __GTA_TASKSIMPLEGETUP_H__
