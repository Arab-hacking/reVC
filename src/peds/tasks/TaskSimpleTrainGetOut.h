#ifndef __GTA_TASKSIMPLETRAINGETOUT_H__
#define __GTA_TASKSIMPLETRAINGETOUT_H__

#include "TaskSimple.h"

class CVehicle;

// Step out of the train once it has stopped and its doors are open.
class CTaskSimpleTrainGetOut : public CTaskSimple
{
public:
	CVehicle *m_pTrain;

public:
	static const eTaskType Type = TASK_SIMPLE_TRAIN_GET_OUT;

	CTaskSimpleTrainGetOut(CVehicle *train);
	CTaskSimpleTrainGetOut(const CTaskSimpleTrainGetOut &other);
	~CTaskSimpleTrainGetOut();

	CTask *Clone() const { return new CTaskSimpleTrainGetOut(*this); }
	eTaskType GetTaskType() const { return Type; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil) { return false; }
	bool ProcessPed(CPed *ped);
};

VALIDATE_SIZE(CTaskSimpleTrainGetOut, 0x10);

#endif // __GTA_TASKSIMPLETRAINGETOUT_H__
