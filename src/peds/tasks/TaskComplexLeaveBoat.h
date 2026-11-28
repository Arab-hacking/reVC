#ifndef __GTA_TASKCOMPLEXLEAVEBOAT_H__
#define __GTA_TASKCOMPLEXLEAVEBOAT_H__

#include "TaskComplex.h"

class CVehicle;

// Leaving a boat is not leaving a car: no door, no lineup - just put
// him off the helm.
class CTaskComplexLeaveBoat : public CTaskComplex
{
public:
	CVehicle *m_veh;
	uint32 m_delayTimeMs;

public:
	static const eTaskType Type = TASK_COMPLEX_LEAVE_BOAT;

	CTaskComplexLeaveBoat(CVehicle *vehicle, uint32 delay);
	CTaskComplexLeaveBoat(const CTaskComplexLeaveBoat &other);
	~CTaskComplexLeaveBoat();

	CTask *CreateSubTask(eTaskType taskType);

	CTask *Clone() const { return new CTaskComplexLeaveBoat(*this); }
	eTaskType GetTaskType() const { return Type; }
	CTask *CreateNextSubTask(CPed *ped) { return nil; }
	CTask *CreateFirstSubTask(CPed *ped);
	CTask *ControlSubTask(CPed *ped) { return m_pSubTask; }
};

VALIDATE_SIZE(CTaskComplexLeaveBoat, 0x14);

#endif // __GTA_TASKCOMPLEXLEAVEBOAT_H__
