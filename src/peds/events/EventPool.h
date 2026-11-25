#pragma once

#include "common.h"

// San Andreas allocates events from a fixed pool (200 slots, created in
// CPools::Initialise; slot size is sizeof(CEventDamage), the largest event
// SA ships) for the same reason as tasks: scanners create events at a high
// rate and pointers must stay valid while queued. Oversized events assert.
#define EVENT_POOL_NUM_SLOTS 200
#define EVENT_POOL_SLOT_SIZE 256

class CEvent;

class CEventPool
{
public:
	CEventPool(int32 numSlots, int32 slotSize);
	~CEventPool();

	void *New(void);
	void Delete(CEvent *event);
	bool IsObjectValid(const CEvent *event) const;

private:
	uint8 *m_Slots;
	uint8 *m_InUse;
	int32 m_NumSlots;
	int32 m_SlotSize;
	int32 m_AllocPtr;
};

// Lazily created on first use, like the task pool.
CEventPool *GetEventPool(void);
