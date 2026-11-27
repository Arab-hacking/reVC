#ifndef __GTA_TASKCOMPLEXLEAVECAR_H__
#define __GTA_TASKCOMPLEXLEAVECAR_H__

#include "TaskComplex.h"
#include "AnimationEnums.h"

class CTaskUtilityLineUpPedWithCar;
class CVehicle;

// Get this ped out of the vehicle: wait for the car to slow down if
// it has to, open the door, climb out, close it, and be on foot.
// Dying peds are put out as corpses; boats have their own way out.
class CTaskComplexLeaveCar : public CTaskComplex
{
public:
	CVehicle *m_pTargetVehicle;
	int32 m_nTargetDoor;
	int32 m_nDelayTime;
	bool m_bSensibleLeaveCar;
	bool m_bForceGetOut;
	bool m_bDie;
	CTaskUtilityLineUpPedWithCar *m_pTaskUtilityLineUpPedWithCar;
	uint8 m_nDoorFlagsSet;
	uint8 m_nNumGettingInSet;
	AnimationId m_nDieAnimID;
	float m_fDieAnimBlendDelta;
	float m_fDieAnimSpeed;
	bool m_bIsInAir;

public:
	static const eTaskType Type = TASK_COMPLEX_LEAVE_CAR;

	CTaskComplexLeaveCar(CVehicle *targetVehicle, int32 nTargetDoor, int32 nDelayTime, bool bSensibleLeaveCar, bool bForceGetOut);
	CTaskComplexLeaveCar(CVehicle *targetVehicle, int32 nTargetDoor, int32 nDelayTime);
	CTaskComplexLeaveCar(const CTaskComplexLeaveCar &other);
	~CTaskComplexLeaveCar();

	CTask *CreateSubTask(eTaskType taskType, CPed *ped);
	void ComputeTargetDoor(CPed *ped);

	CTask *Clone() const { return new CTaskComplexLeaveCar(*this); }
	eTaskType GetTaskType() const { return Type; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil);
	CTask *CreateNextSubTask(CPed *ped);
	CTask *CreateFirstSubTask(CPed *ped);
	CTask *ControlSubTask(CPed *ped);

private:
	void RegisterPedLeaving(CPed *ped);
	void CreateLineUpUtility(CPed *ped);
};

VALIDATE_SIZE(CTaskComplexLeaveCar, 0x34);

#endif // __GTA_TASKCOMPLEXLEAVECAR_H__
