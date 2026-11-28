#ifndef __GTA_EVENTSCRIPTCOMMAND_H__
#define __GTA_EVENTSCRIPTCOMMAND_H__

#include "Event.h"

class CTask;

// SA CEventScriptCommand: how the remaining script/task layer talks to a
// ped - "run this task, at this primary slot". Script commands outrank
// almost everything a ped decides for itself, which is what their priority
// numbers below encode.
class CEventScriptCommand : public CEvent
{
public:
	int32 m_primaryTaskIndex;
	CTask *m_task;
	bool m_affectsDeadPeds;

	static const eEventType Type = EVENT_SCRIPT_COMMAND;

	CEventScriptCommand(int32 primaryTaskIndex, CTask *task, bool affectsDeadPeds = false);
	~CEventScriptCommand();

	eEventType GetEventType() const { return Type; }
	int32 GetEventPriority() const;
	int32 GetLifeTime() { return 0; }
	CEvent *Clone() const;
	bool AffectsPed(CPed *ped);
	bool TakesPriorityOver(const CEvent &refEvent);
	bool IsValid(CPed *ped);
	virtual CTask *CloneScriptTask() const;
};

#endif // __GTA_EVENTSCRIPTCOMMAND_H__
