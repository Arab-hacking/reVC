#include "common.h"

#include "PedGroupIntelligence.h"

#include "Entity.h"
#include "Event.h"
#include "Ped.h"
#include "PedGroup.h"

// SA CPedGroupIntelligence (data and queue accessors; see the header
// for what is deferred).

// 0x5F7250
CPedGroupIntelligence::CPedGroupIntelligence(CPedGroup *owner)
: m_pPedGroup(owner)
{
	m_CurrentEvent = nil;
	m_HighestPriorityEvent = nil;
	// SA points this at the STAND_STILL default task allocator; the
	// group task allocators are not ported yet.
	m_DefaultTaskAllocator = nil;
	m_PrimaryTaskAllocator = nil;
	m_EventResponseTaskAllocator = nil;
	m_DecisionMakerType = DECISION_MAKER_UNKNOWN;
	m_TaskSeqId = -1;
}

// Seemingly always inlined as Flush()
CPedGroupIntelligence::~CPedGroupIntelligence()
{
	Flush();
}

// 0x5F7350 (NOTE: in SA this is seemingly the destructor body)
void
CPedGroupIntelligence::Flush()
{
	for(int32 i = 0; i < TOTAL_PED_GROUP_MEMBERS; i++){
		m_PedTaskPairs[i].Flush();
		m_SecondaryPedTaskPairs[i].Flush();
		m_ScriptCommandPedTaskPairs[i].Flush();
		m_DefaultPedTaskPairs[i].Flush();
	}

	if(m_HighestPriorityEvent != m_CurrentEvent){
		delete m_HighestPriorityEvent;
		m_HighestPriorityEvent = nil;
	}
	delete m_CurrentEvent;
	m_CurrentEvent = nil;

	delete m_PrimaryTaskAllocator;
	m_PrimaryTaskAllocator = nil;
	delete m_EventResponseTaskAllocator;
	m_EventResponseTaskAllocator = nil;

	m_DecisionMakerType = DECISION_MAKER_UNKNOWN;
	m_TaskSeqId = TASK_INVALID;
}

// SA 0x5FC4A0: the group event machinery (CEventGroupEvent, the group
// task allocators, CGroupEventHandler) is not ported yet, so there is
// nothing to process - the task queues stay empty and members simply
// keep whatever their own intelligence is doing.
void
CPedGroupIntelligence::Process()
{
}

// notsa
CPedTaskPair*
CPedGroupIntelligence::GetPedsTaskPair(CPed *ped, CPedTaskPair *taskPairs) const
{
	for(int32 i = 0; i < TOTAL_PED_GROUP_MEMBERS; i++)
		if(taskPairs[i].Ped == ped)
			return &taskPairs[i];
	return nil;
}

// 0x5F7660-ish
CTask*
CPedGroupIntelligence::GetTask(CPed *ped, CPedTaskPair *taskPairs) const
{
	if(CPedTaskPair *tp = GetPedsTaskPair(ped, taskPairs))
		return tp->Task;
	return nil;
}

// 0x5F85A0
CTask*
CPedGroupIntelligence::GetTaskMain(CPed *ped)
{
	CTask *task;
	if((task = GetTaskScriptCommand(ped)) != nil)
		return task;
	if((task = GetTask(ped, m_PedTaskPairs)) != nil)
		return task;
	return GetTaskDefault(ped);
}

CTask*
CPedGroupIntelligence::GetTaskDefault(CPed *ped)
{
	return GetTask(ped, m_DefaultPedTaskPairs);
}

// 0x5F8690
CTask*
CPedGroupIntelligence::GetTaskScriptCommand(CPed *ped)
{
	return GetTask(ped, m_ScriptCommandPedTaskPairs);
}

// 0x5F8620
CTask*
CPedGroupIntelligence::GetTaskSecondary(CPed *ped)
{
	return GetTask(ped, m_SecondaryPedTaskPairs);
}

// 0x5F8650
eSecondaryTask
CPedGroupIntelligence::GetTaskSecondarySlot(CPed *ped)
{
	if(CPedTaskPair *tp = GetPedsTaskPair(ped, m_SecondaryPedTaskPairs))
		return tp->Slot;
	return TASK_SECONDARY_INVALID;
}

// 0x5F77A0 (the SA CEventGroupEvent wrapper is flattened to its event)
bool
CPedGroupIntelligence::IsCurrentEventValid()
{
	if(m_CurrentEvent){
		if(m_CurrentEvent->GetEventType() == EVENT_PLAYER_COMMAND_TO_GROUP){
			if(CEntity *src = m_CurrentEvent->GetSourceEntity()){
				if(src->IsPed()){
					if(m_pPedGroup->GetMembership().IsMember((CPed*)src))
						return false;
				}
			}
		}
	}
	return true;
}

// 0x5F7760
bool
CPedGroupIntelligence::IsGroupResponding()
{
	for(int32 i = 0; i < TOTAL_PED_GROUP_MEMBERS; i++){
		if(m_PedTaskPairs[i].Ped && m_PedTaskPairs[i].Task)
			return true;
		if(m_SecondaryPedTaskPairs[i].Ped && m_SecondaryPedTaskPairs[i].Task)
			return true;
	}
	return false;
}
