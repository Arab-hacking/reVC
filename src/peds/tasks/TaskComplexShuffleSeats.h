#ifndef __GTA_TASKCOMPLEXSHUFFLESEATS_H__
#define __GTA_TASKCOMPLEXSHUFFLESEATS_H__

#include "TaskComplex.h"
#include "eTaskType.h"

class CTaskUtilityLineUpPedWithCar;
class CVehicle;
class CPed;
class CEvent;

// Move over one seat inside the car: slide across, then settle into
// the seat on the other side (or out, if interrupted).
class CTaskComplexShuffleSeats : public CTaskComplex
{
public:
	CVehicle *m_Veh;
	int32 m_OriginDoor;
	int32 m_TargetDoor;
	CVector m_TargetDoorPos;
	CTaskUtilityLineUpPedWithCar *m_TaskUtilityLineUpPedWithCar;
	bool m_bAbort;

public:
	static const eTaskType Type = TASK_COMPLEX_SHUFFLE_SEATS;

	CTaskComplexShuffleSeats(CVehicle *veh);
	CTaskComplexShuffleSeats(const CTaskComplexShuffleSeats &other);
	~CTaskComplexShuffleSeats();

	CTask *CreateSubTask(eTaskType taskType, CPed *ped);
	void CreateTaskUtilityLineUpPedWithCar(CPed *ped);

	CTask *Clone() const { return new CTaskComplexShuffleSeats(*this); }
	eTaskType GetTaskType() const { return Type; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil);
	CTask *CreateNextSubTask(CPed *ped);
	CTask *CreateFirstSubTask(CPed *ped);
	CTask *ControlSubTask(CPed *ped);
};

VALIDATE_SIZE(CTaskComplexShuffleSeats, 0x30);

#endif // __GTA_TASKCOMPLEXSHUFFLESEATS_H__
