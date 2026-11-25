#include "common.h"

#include "EventScriptCommand.h"
#include "Ped.h"
#include "Task.h"

// SA CEventScriptCommand::CEventScriptCommand (0x4AC290).
CEventScriptCommand::CEventScriptCommand(int32 primaryTaskIndex, CTask *task, bool affectsDeadPeds)
{
	m_primaryTaskIndex = primaryTaskIndex;
	m_task = task;
	m_affectsDeadPeds = affectsDeadPeds;
}

// SA CEventScriptCommand::~CEventScriptCommand (0x4AC2E0): the event owns
// the task it carries.
CEventScriptCommand::~CEventScriptCommand()
{
	delete m_task;
}

CEvent*
CEventScriptCommand::Clone() const
{
	return new CEventScriptCommand(m_primaryTaskIndex, m_task ? m_task->Clone() : nil, m_affectsDeadPeds);
}

// SA CEventScriptCommand::GetEventPriority (0x4AC350). Script commands sit
// above decision-maker responses; commands that must reach a dead ped, kill
// it, or keep it swimming outrank the rest. (SA also boosts looping named
// anims; that case lands with CTaskSimpleRunAnim.)
int32
CEventScriptCommand::GetEventPriority() const
{
	if(m_affectsDeadPeds)
		return 75;
	if(m_task == nil)
		return 53;
	eTaskType taskId = m_task->GetTaskType();
	if(taskId == TASK_SIMPLE_DIE || taskId == TASK_SIMPLE_SWIM || taskId == TASK_COMPLEX_USE_MOBILE_PHONE)
		return 71;
	if(taskId != TASK_COMPLEX_DIE)
		return 53;
	return 71;
}

// SA CEventScriptCommand::AffectsPed (0x4AC3A0).
bool
CEventScriptCommand::AffectsPed(CPed *ped)
{
	return !ped->Dead() || m_affectsDeadPeds;
}

// SA CEventScriptCommand::TakesPriorityOver (0x4AC3C0).
bool
CEventScriptCommand::TakesPriorityOver(const CEvent &refEvent)
{
	eEventType refEventType = refEvent.GetEventType();
	if(m_affectsDeadPeds && (refEventType == EVENT_DEATH || refEventType == EVENT_DAMAGE))
		return true;
	return GetEventPriority() >= refEvent.GetEventPriority();
}

// SA CEventScriptCommand::IsValid (0x4AC410).
bool
CEventScriptCommand::IsValid(CPed *ped)
{
	if(ped)
		return !ped->Dead();
	if(m_bValid || m_nTimeActive <= GetLifeTime())
		return true;
	return false;
}

// SA CEventScriptCommand::CloneScriptTask (0x4AC460).
CTask*
CEventScriptCommand::CloneScriptTask() const
{
	if(m_task)
		return m_task->Clone();
	return nil;
}
