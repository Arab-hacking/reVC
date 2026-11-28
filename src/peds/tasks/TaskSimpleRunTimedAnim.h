#ifndef __GTA_TASKSIMPLERUNTIMEDANIM_H__
#define __GTA_TASKSIMPLERUNTIMEDANIM_H__

#include "TaskSimpleAnim.h"
#include "TaskTimer.h"
#include "AnimationEnums.h"

// SA CTaskSimpleRunTimedAnim: play an animation for a fixed time rather
// than until it ends - hands up, cower-in-place variants and the like.
class CTaskSimpleRunTimedAnim : public CTaskSimpleAnim
{
public:
	AssocGroupId m_animGrpId;
	AnimationId m_animId;
	float m_blendDelta;
	float m_unused1;
	uint32 m_durationMs;
	CTaskTimer m_timer;
	eTaskType m_taskId;

public:
	CTaskSimpleRunTimedAnim(AssocGroupId groupId, AnimationId animId, float blendDelta, float unused1, uint32 durationMs, bool holdLastFrame);
	CTaskSimpleRunTimedAnim(AssocGroupId groupId, AnimationId animId, float blendDelta, float unused1, uint32 durationMs, eTaskType taskType, const char *taskName, bool holdLastFrame);
	CTaskSimpleRunTimedAnim(const CTaskSimpleRunTimedAnim &other);
	~CTaskSimpleRunTimedAnim() {}

	void StartAnim(CPed *ped);

	CTask *Clone() const { return new CTaskSimpleRunTimedAnim(*this); }
	eTaskType GetTaskType() const { return m_taskId; }
	bool ProcessPed(CPed *ped);
};

#endif // __GTA_TASKSIMPLERUNTIMEDANIM_H__
