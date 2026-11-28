#ifndef __GTA_TASKSIMPLECARFORCEPEDOUT_H__
#define __GTA_TASKSIMPLECARFORCEPEDOUT_H__

#include "TaskSimple.h"

class CVehicle;

// Put the ped straight on the ground at the door, no animation:
// used when there is simply no room for a proper get-out.
class CTaskSimpleCarForcePedOut : public CTaskSimple
{
public:
	CVehicle *m_Veh;
	int32 m_Door;

public:
	static const eTaskType Type = TASK_SIMPLE_CAR_FORCE_PED_OUT;

	CTaskSimpleCarForcePedOut(CVehicle *veh, int32 door);
	CTaskSimpleCarForcePedOut(const CTaskSimpleCarForcePedOut &other);
	~CTaskSimpleCarForcePedOut();

	CTask *Clone() const { return new CTaskSimpleCarForcePedOut(*this); }
	eTaskType GetTaskType() const { return Type; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil);
	bool ProcessPed(CPed *ped);
};

VALIDATE_SIZE(CTaskSimpleCarForcePedOut, 0x10);

#endif // __GTA_TASKSIMPLECARFORCEPEDOUT_H__
