#ifndef __GTA_TASKCOMPLEXENTERBOATASDRIVER_H__
#define __GTA_TASKCOMPLEXENTERBOATASDRIVER_H__

#include "TaskComplex.h"

class CVehicle;

// Already aboard: walk to the wheel, take the seat, and the boat is
// yours. (Getting aboard from the water or the dock is somebody
// else's business.)
class CTaskComplexEnterBoatAsDriver : public CTaskComplex
{
public:
	CVehicle *m_EnterInto;

public:
	static const eTaskType Type = TASK_COMPLEX_ENTER_BOAT_AS_DRIVER;

	CTaskComplexEnterBoatAsDriver(CVehicle *target);
	CTaskComplexEnterBoatAsDriver(const CTaskComplexEnterBoatAsDriver &other);
	~CTaskComplexEnterBoatAsDriver();

	CTask *CreateSubTask(eTaskType taskType);

	CTask *Clone() const { return new CTaskComplexEnterBoatAsDriver(*this); }
	eTaskType GetTaskType() const { return Type; }
	CTask *CreateNextSubTask(CPed *ped);
	CTask *CreateFirstSubTask(CPed *ped);
	CTask *ControlSubTask(CPed *ped) { return m_pSubTask; }
};

VALIDATE_SIZE(CTaskComplexEnterBoatAsDriver, 0x10);

#endif // __GTA_TASKCOMPLEXENTERBOATASDRIVER_H__
