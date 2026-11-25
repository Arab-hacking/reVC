#ifndef __GTA_TASKSIMPLERUNANIM_H__
#define __GTA_TASKSIMPLERUNANIM_H__

#include "TaskSimpleAnim.h"
#include "AnimationEnums.h"

// SA CTaskSimpleRunAnim (0x20): play one animation from a group until it
// finishes. Derived tasks reuse it with their own task type, which is how
// the one-shot ped animations (cower, fall, die-in-place...) are built.
class CTaskSimpleRunAnim : public CTaskSimpleAnim
{
public:
	AssocGroupId m_nAnimGroup;
	AnimationId m_nAnimId;
	float m_fBlendDelta;
	uint16 m_nTaskType;

public:
	static const eTaskType Type = TASK_SIMPLE_NAMED_ANIM;

	CTaskSimpleRunAnim(AssocGroupId animGroup, AnimationId animId, float fBlendDelta = 4.0f, bool bHoldLastFrame = false);
	CTaskSimpleRunAnim(AssocGroupId animGroup, AnimationId animId, float fBlendDelta, int32 nTaskType, const char *taskName, bool bHoldLastFrame);

	CTask *Clone() const;
	eTaskType GetTaskType() const { return (eTaskType)m_nTaskType; }
	bool ProcessPed(CPed *ped);

	void StartAnim(CPed *ped);
};

VALIDATE_SIZE(CTaskSimpleRunAnim, 0x20);

#endif // __GTA_TASKSIMPLERUNANIM_H__
