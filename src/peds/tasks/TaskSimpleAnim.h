#ifndef __GTA_TASKSIMPLEANIM_H__
#define __GTA_TASKSIMPLEANIM_H__

#include "TaskSimple.h"

class CAnimBlendAssociation;

// SA CTaskSimpleAnim (0x10): base for tasks that are "play this animation".
// It owns the finish bookkeeping: the animation's finish callback marks the
// task finished, and aborting blends the animation out (or drops it at
// once for urgent aborts).
class CTaskSimpleAnim : public CTaskSimple
{
public:
	CAnimBlendAssociation *m_pAnim;
	union {
		struct {
			bool m_bIsFinished : 1;
			bool m_bDontInterrupt : 1;
			bool m_bHoldLastFrame : 1;
			// Used by CTaskSimpleRunAnim only
			bool m_bDontBlendOut : 1;
			// Used by CTaskSimpleRunNamedAnim only
			bool m_bRunInSequence : 1;
			bool m_bOffsetAtEnd : 1;
			bool m_bOffsetAvailable : 1;
		};
		uint8 m_nFlags;
	};

public:
	explicit CTaskSimpleAnim(bool bHoldLastFrame);
	~CTaskSimpleAnim();

	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil);
	static void FinishRunAnimCB(CAnimBlendAssociation *blendAssoc, void *data);
};

VALIDATE_SIZE(CTaskSimpleAnim, 0x10);

#endif // __GTA_TASKSIMPLEANIM_H__
