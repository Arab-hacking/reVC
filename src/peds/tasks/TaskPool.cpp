#include "common.h"

#include "MemoryMgr.h"
#include "TaskPool.h"
#include "Task.h"

CTaskPool::CTaskPool(int32 numSlots, int32 slotSize)
{
	m_NumSlots = numSlots;
	m_SlotSize = slotSize;
	m_AllocPtr = 0;
	m_Slots = (uint8*)MemoryMgrMalloc(numSlots * slotSize);
	m_InUse = (uint8*)MemoryMgrMalloc(numSlots);
	memset(m_InUse, 0, numSlots);
}

CTaskPool::~CTaskPool()
{
	MemoryMgrFree(m_Slots);
	MemoryMgrFree(m_InUse);
}

void*
CTaskPool::New(void)
{
	for(int32 i = 0; i < m_NumSlots; i++){
		int32 slot = m_AllocPtr++ % m_NumSlots;
		if(!m_InUse[slot]){
			m_InUse[slot] = 1;
			return m_Slots + slot * m_SlotSize;
		}
	}
	assert(0 && "Task pool is full");
	return nil;
}

void
CTaskPool::Delete(CTask *task)
{
	if(task == nil)
		return;
	assert(IsObjectValid(task));
	int32 slot = (int32)(((uint8*)task - m_Slots) / m_SlotSize);
	m_InUse[slot] = 0;
}

bool
CTaskPool::IsObjectValid(const CTask *task) const
{
	if(task == nil)
		return false;
	const uint8 *p = (const uint8*)task;
	if(p < m_Slots || p >= m_Slots + m_NumSlots * m_SlotSize)
		return false;
	if((p - m_Slots) % m_SlotSize != 0)
		return false;
	return m_InUse[(p - m_Slots) / m_SlotSize] != 0;
}

CTaskPool*
GetTaskPool(void)
{
	static CTaskPool pool(TASK_POOL_NUM_SLOTS, TASK_POOL_SLOT_SIZE);
	return &pool;
}
