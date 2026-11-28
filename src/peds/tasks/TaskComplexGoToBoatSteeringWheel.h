#ifndef __GTA_TASKCOMPLEXGOTOBOATSTEERINGWHEEL_H__
#define __GTA_TASKCOMPLEXGOTOBOATSTEERINGWHEEL_H__

#include "TaskComplex.h"

class CVehicle;

// Walk across the deck to where the wheel (or the driver's spot)
// is, and stand there.
class CTaskComplexGoToBoatSteeringWheel : public CTaskComplex
{
public:
	CVector m_TargetPos;
	CVehicle *m_Vehicle;
	bool m_bAchievedPoint;
	uint32 m_FirstSubTaskStartTime;

public:
	static const eTaskType Type = TASK_COMPLEX_GO_TO_BOAT_STEERING_WHEEL;

	CTaskComplexGoToBoatSteeringWheel(CVehicle *vehicle);
	CTaskComplexGoToBoatSteeringWheel(const CTaskComplexGoToBoatSteeringWheel &other);
	~CTaskComplexGoToBoatSteeringWheel();

	bool HasAchievedPoint() const { return m_bAchievedPoint; }

	CTask *CreateSubTask(eTaskType taskType);
	void ComputeTargetPos();

	CTask *Clone() const { return new CTaskComplexGoToBoatSteeringWheel(*this); }
	eTaskType GetTaskType() const { return Type; }
	CTask *CreateNextSubTask(CPed *ped);
	CTask *CreateFirstSubTask(CPed *ped);
	CTask *ControlSubTask(CPed *ped);
};

VALIDATE_SIZE(CTaskComplexGoToBoatSteeringWheel, 0x24);

#endif // __GTA_TASKCOMPLEXGOTOBOATSTEERINGWHEEL_H__
