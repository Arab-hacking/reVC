#ifndef __GTA_TASKSIMPLESCRATCHHEAD_H__
#define __GTA_TASKSIMPLESCRATCHHEAD_H__

#include "TaskSimpleRunAnim.h"

// SA CTaskSimpleScratchHead: the "thinking" idle animation. Wander uses
// it whenever a ped has run out of path - it stands, scratches its head,
// and picks a new direction.
class CTaskSimpleScratchHead : public CTaskSimpleRunAnim
{
public:
	CTaskSimpleScratchHead() : CTaskSimpleRunAnim(ANIM_GROUP_DEFAULT, ANIM_ID_XPRESSSCRATCH, 4.0f, TASK_SIMPLE_SCRATCH_HEAD, "ScratchHead", false) {}
	CTaskSimpleScratchHead(const CTaskSimpleScratchHead &other) : CTaskSimpleScratchHead() {}
	~CTaskSimpleScratchHead() {}

	CTask *Clone() const { return new CTaskSimpleScratchHead(); }
	virtual bool IsInterruptable(const CPed *ped) { return false; }
};

#endif // __GTA_TASKSIMPLESCRATCHHEAD_H__
