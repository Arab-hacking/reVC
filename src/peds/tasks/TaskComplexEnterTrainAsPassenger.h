#ifndef __GTA_TASKCOMPLEXENTERTRAINASPASSENGER_H__
#define __GTA_TASKCOMPLEXENTERTRAINASPASSENGER_H__

#include "TaskComplex.h"
#include "Vector.h"

class CVehicle;

// Walk to the nearest train entry and step in as a passenger. There
// is no SA counterpart - SA has no rideable trains - but the shape is
// the SA one: a go-to task followed by a simple get-in task.
class CTaskComplexEnterTrainAsPassenger : public CTaskComplex
{
public:
	CVehicle *m_Train;
	CVector m_DoorPos;

public:
	static const eTaskType Type = TASK_COMPLEX_ENTER_TRAIN_AS_PASSENGER;

	CTaskComplexEnterTrainAsPassenger(CVehicle *train);
	CTaskComplexEnterTrainAsPassenger(const CTaskComplexEnterTrainAsPassenger &other);
	~CTaskComplexEnterTrainAsPassenger();

	CTask *CreateSubTask(eTaskType taskType, CPed *ped);

	CTask *Clone() const { return new CTaskComplexEnterTrainAsPassenger(*this); }
	eTaskType GetTaskType() const { return Type; }
	CTask *CreateNextSubTask(CPed *ped);
	CTask *CreateFirstSubTask(CPed *ped);
	CTask *ControlSubTask(CPed *ped) { return m_pSubTask; }
};

VALIDATE_SIZE(CTaskComplexEnterTrainAsPassenger, 0x1C);

#endif // __GTA_TASKCOMPLEXENTERTRAINASPASSENGER_H__
