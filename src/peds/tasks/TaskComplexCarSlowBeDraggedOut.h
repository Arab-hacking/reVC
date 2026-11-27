#ifndef __GTA_TASKCOMPLEXCARSLOWBEDRAGGEDOUT_H__
#define __GTA_TASKCOMPLEXCARSLOWBEDRAGGEDOUT_H__

#include "TaskComplex.h"

class CTaskUtilityLineUpPedWithCar;
class CVehicle;

// Be dragged out of the car by the man at the door: fall out (if he
// is doing it to himself, oddly), be pulled out, then be put on the
// ground where he left you.
class CTaskComplexCarSlowBeDraggedOut : public CTaskComplex
{
public:
	CVehicle *m_Vehicle;
	int32 m_Door;
	bool m_IsDraggedBySelf;
	CTaskUtilityLineUpPedWithCar *m_LineUpUtility;

public:
	static const eTaskType Type = TASK_COMPLEX_CAR_SLOW_BE_DRAGGED_OUT;

	CTaskComplexCarSlowBeDraggedOut(CVehicle *vehicle, int32 door, bool isDraggedBySelf);
	CTaskComplexCarSlowBeDraggedOut(const CTaskComplexCarSlowBeDraggedOut &other);
	~CTaskComplexCarSlowBeDraggedOut();

	CTask *CreateSubTask(eTaskType taskType);
	void CreateTaskUtilityLineUpPedWithCar(CPed *ped);
	void PrepareVehicleForPedExit(CPed *ped);
	void PreparePedForVehicleExit(CPed *ped);

	CTask *Clone() const { return new CTaskComplexCarSlowBeDraggedOut(*this); }
	eTaskType GetTaskType() const { return Type; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil);
	CTask *CreateNextSubTask(CPed *ped);
	CTask *CreateFirstSubTask(CPed *ped);
	CTask *ControlSubTask(CPed *ped) { return m_pSubTask; }
};

VALIDATE_SIZE(CTaskComplexCarSlowBeDraggedOut, 0x1C);

#endif // __GTA_TASKCOMPLEXCARSLOWBEDRAGGEDOUT_H__
