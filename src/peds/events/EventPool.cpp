#include "common.h"

#include "MemoryMgr.h"
#include "EventPool.h"
#include "Event.h"

CEventPool::CEventPool(int32 numSlots, int32 slotSize)
{
	m_NumSlots = numSlots;
	m_SlotSize = slotSize;
	m_AllocPtr = 0;
	m_Slots = (uint8*)MemoryMgrMalloc(numSlots * slotSize);
	m_InUse = (uint8*)MemoryMgrMalloc(numSlots);
	memset(m_InUse, 0, numSlots);
}

CEventPool::~CEventPool()
{
	MemoryMgrFree(m_Slots);
	MemoryMgrFree(m_InUse);
}

void*
CEventPool::New(void)
{
	for(int32 i = 0; i < m_NumSlots; i++){
		int32 slot = m_AllocPtr++ % m_NumSlots;
		if(!m_InUse[slot]){
			m_InUse[slot] = 1;
			return m_Slots + slot * m_SlotSize;
		}
	}
	assert(0 && "Event pool is full");
	return nil;
}

void
CEventPool::Delete(CEvent *event)
{
	if(event == nil)
		return;
	assert(IsObjectValid(event));
	int32 slot = (int32)(((uint8*)event - m_Slots) / m_SlotSize);
	m_InUse[slot] = 0;
}

bool
CEventPool::IsObjectValid(const CEvent *event) const
{
	if(event == nil)
		return false;
	const uint8 *p = (const uint8*)event;
	if(p < m_Slots || p >= m_Slots + m_NumSlots * m_SlotSize)
		return false;
	if((p - m_Slots) % m_SlotSize != 0)
		return false;
	return m_InUse[(p - m_Slots) / m_SlotSize] != 0;
}

CEventPool*
GetEventPool(void)
{
	static CEventPool pool(EVENT_POOL_NUM_SLOTS, EVENT_POOL_SLOT_SIZE);
	return &pool;
}
