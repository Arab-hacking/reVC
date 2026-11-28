#ifndef __GTA_TASKCOMPLEXGETONBOATSEAT_H__
#define __GTA_TASKCOMPLEXGETONBOATSEAT_H__

#include "TaskComplex.h"

class CVehicle;

// Settled at the helm position: sit (or stand at the wheel, on
// boats where the driver stands).
class CTaskComplexGetOnBoatSeat : public CTaskComplex
{
public:
	CVehicle *m_Vehicle;

public:
	static const eTaskType Type = TASK_COMPLEX_GET_ON_BOAT_SEAT;

	CTaskComplexGetOnBoatSeat(CVehicle *vehicle);
	CTaskComplexGetOnBoatSeat(const CTaskComplexGetOnBoatSeat &other);
	~CTaskComplexGetOnBoatSeat();

	CTask *Clone() const { return new CTaskComplexGetOnBoatSeat(*this); }
	eTaskType GetTaskType() const { return Type; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil);
	CTask *CreateNextSubTask(CPed *ped) { return nil; }
	CTask *CreateFirstSubTask(CPed *ped);
	CTask *ControlSubTask(CPed *ped) { return m_pSubTask; }
};

VALIDATE_SIZE(CTaskComplexGetOnBoatSeat, 0x10);

#endif // __GTA_TASKCOMPLEXGETONBOATSEAT_H__
