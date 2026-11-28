#pragma once

#include "common.h"

// San Andreas allocates every task from one fixed pool (500 slots, created
// in CPools::Initialise) instead of the heap: task pointers stay stable for
// the whole frame and CTask::IsTaskPtr can validate a pointer by checking
// it against the pool. Slot size is the size of the largest task SA ships;
// operator new asserts against it so an oversized task fails loudly instead
// of corrupting its neighbour.
#define TASK_POOL_NUM_SLOTS 500
#define TASK_POOL_SLOT_SIZE 512

class CTask;

class CTaskPool
{
public:
	CTaskPool(int32 numSlots, int32 slotSize);
	~CTaskPool();

	void *New(void);
	void Delete(CTask *task);
	bool IsObjectValid(const CTask *task) const;

private:
	uint8 *m_Slots;
	uint8 *m_InUse;
	int32 m_NumSlots;
	int32 m_SlotSize;
	int32 m_AllocPtr;
};

// Lazily created on first use: tasks can be allocated before CPools would
// have initialised the SA pool (test fixtures do this, for one).
CTaskPool *GetTaskPool(void);
