#ifndef __GTA_TASKSIMPLEDIE_H__
#define __GTA_TASKSIMPLEDIE_H__

#include "TaskSimple.h"
#include "AnimBlendAssociation.h" // for eAnimationFlags
#include "AnimationEnums.h"

class CAnimBlendHierarchy;
class CAnimBlendAssociation;

// Die, with the animation chosen by whoever killed him (or the
// default knock-out fall when nobody said).
class CTaskSimpleDie : public CTaskSimple
{
public:
	AssocGroupId m_animGroupId;
	AnimationId m_animId;
	CAnimBlendHierarchy *m_animHierarchy;
	eAnimationFlags m_animFlags;
	float m_blendDelta;
	float m_animSpeed;
	union {
		struct {
			bool m_dieAnimFinished : 1;
			bool m_alreadyDead : 1;
		};
		uint8 m_nFlags;
	};
	CAnimBlendAssociation *m_animAssociation;

public:
	static const eTaskType Type = TASK_SIMPLE_DIE;

	CTaskSimpleDie(AssocGroupId animGroupId, AnimationId animId, float blendDelta, float animSpeed);
	CTaskSimpleDie(const char *animName, const char *animBlock, eAnimationFlags animFlags, float blendDelta, float animSpeed);
	CTaskSimpleDie(CAnimBlendHierarchy *animHierarchy, eAnimationFlags animFlags, float blendDelta, float animSpeed);
	CTaskSimpleDie(const CTaskSimpleDie &other);
	~CTaskSimpleDie();

	static void FinishAnimDieCB(CAnimBlendAssociation *association, void *data);

	void StartAnim(CPed *ped);

	CTask *Clone() const;
	eTaskType GetTaskType() const { return Type; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil);
	bool ProcessPed(CPed *ped);
};

VALIDATE_SIZE(CTaskSimpleDie, 0x28);

#endif // __GTA_TASKSIMPLEDIE_H__
