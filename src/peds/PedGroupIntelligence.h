#pragma once

// SA CPedGroupIntelligence: the brain of a ped group - the group event
// currently being answered, the highest-priority one seen, and the
// per-member task queues (event-response, secondary, script-command
// and default) that SA's group event handler fills and CPedIntelligence
// drains.
//
// This slice ports the data layout and the queue accessors only. The
// machinery that fills the queues - CEventGroupEvent, the group task
// allocators (CPedGroupDefaultTaskAllocator/CTaskAllocator) and
// CGroupEventHandler - is not ported yet, so Process() currently runs
// nothing and the allocator pointers stay nil (SA starts the default
// allocator at STAND_STILL).

#include "PedGroupMembership.h" // TOTAL_PED_GROUP_MEMBERS
#include "PedTaskPair.h"
#include "DecisionMakerTypes.h" // eDecisionMakerType

class CEvent;
class CPed;
class CPedGroup;
class CTask;
class CTaskAllocator;
class CPedGroupDefaultTaskAllocator;

class CPedGroupIntelligence
{
public:
	CPedGroupIntelligence(CPedGroup *owner);
	~CPedGroupIntelligence();

	void Flush();
	void Process();

	CPedTaskPair *GetPedsTaskPair(CPed *ped, CPedTaskPair *taskPairs) const;
	CTask *GetTask(CPed *ped, CPedTaskPair *taskPairs) const;
	CTask *GetTaskMain(CPed *ped);
	CTask *GetTaskDefault(CPed *ped);
	CTask *GetTaskScriptCommand(CPed *ped);
	CTask *GetTaskSecondary(CPed *ped);
	eSecondaryTask GetTaskSecondarySlot(CPed *ped);

	bool IsCurrentEventValid();
	bool IsGroupResponding();
	void SetGroupDecisionMakerType(eDecisionMakerType type) { m_DecisionMakerType = type; }

	CEvent *GetCurrentEvent() { return m_CurrentEvent; }
	CEvent *GetHighestPriorityEvent() { return m_HighestPriorityEvent; }
	CPedGroup &GetPedGroup() { return *m_pPedGroup; }

private:
	CPedGroup *m_pPedGroup;
	CEvent *m_CurrentEvent; // SA: CEventGroupEvent*
	CEvent *m_HighestPriorityEvent; // SA: CEventGroupEvent*
	CPedTaskPair m_PedTaskPairs[TOTAL_PED_GROUP_MEMBERS];
	CPedTaskPair m_SecondaryPedTaskPairs[TOTAL_PED_GROUP_MEMBERS];
	CPedTaskPair m_ScriptCommandPedTaskPairs[TOTAL_PED_GROUP_MEMBERS];
	CPedTaskPair m_DefaultPedTaskPairs[TOTAL_PED_GROUP_MEMBERS];
	const CPedGroupDefaultTaskAllocator *m_DefaultTaskAllocator;
	CTaskAllocator *m_PrimaryTaskAllocator;
	CTaskAllocator *m_EventResponseTaskAllocator;
	eDecisionMakerType m_DecisionMakerType;
	int32 m_TaskSeqId; // Used in CTaskSequences::ms_taskSequence
};

VALIDATE_SIZE(CPedGroupIntelligence, 0x2A0);
