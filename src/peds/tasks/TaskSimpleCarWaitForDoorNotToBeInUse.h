#ifndef __GTA_TASKCARWAITFORDOORNOTTOBEINUSE_H__
#define __GTA_TASKCARWAITFORDOORNOTTOBEINUSE_H__

#include "TaskSimple.h"

class CVehicle;

// Wait until either of two doors of the vehicle is no longer being used
// by another ped (finished as soon as one of them is free).
class CTaskSimpleCarWaitForDoorNotToBeInUse : public CTaskSimple
{
public:
	CVehicle *m_veh;            //!< The vehicle whose door we have to wait for
	uint32 m_doorsToWaitFor[2]; //!< Wait for any of these doors to be ready (not all)

public:
	static const eTaskType Type = TASK_SIMPLE_CAR_WAIT_FOR_DOOR_NOT_TO_BE_IN_USE;

	CTaskSimpleCarWaitForDoorNotToBeInUse(CVehicle *veh, uint32 doorToWaitFor1, uint32 doorToWaitFor2 = 0);
	CTaskSimpleCarWaitForDoorNotToBeInUse(const CTaskSimpleCarWaitForDoorNotToBeInUse &other);
	~CTaskSimpleCarWaitForDoorNotToBeInUse();

	CTask *Clone() const { return new CTaskSimpleCarWaitForDoorNotToBeInUse(*this); }
	eTaskType GetTaskType() const { return Type; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil);
	bool ProcessPed(CPed *ped);
	bool SetPedPosition(CPed *ped);
};

VALIDATE_SIZE(CTaskSimpleCarWaitForDoorNotToBeInUse, 0x14);

#endif // __GTA_TASKCARWAITFORDOORNOTTOBEINUSE_H__
