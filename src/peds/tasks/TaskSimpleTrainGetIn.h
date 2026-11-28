#ifndef __GTA_TASKSIMPLETRAINGETIN_H__
#define __GTA_TASKSIMPLETRAINGETIN_H__

#include "TaskSimple.h"

class CVehicle;

// Step into the train and take a passenger place. Trains have no
// door animation in this game: the ped is lined up at the nearest
// entry and seated, which is all the old state machine ever did.
class CTaskSimpleTrainGetIn : public CTaskSimple
{
public:
	CVehicle *m_pTrain;

public:
	static const eTaskType Type = TASK_SIMPLE_TRAIN_GET_IN;

	CTaskSimpleTrainGetIn(CVehicle *train);
	CTaskSimpleTrainGetIn(const CTaskSimpleTrainGetIn &other);
	~CTaskSimpleTrainGetIn();

	CTask *Clone() const { return new CTaskSimpleTrainGetIn(*this); }
	eTaskType GetTaskType() const { return Type; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil) { return false; }
	bool ProcessPed(CPed *ped);
};

VALIDATE_SIZE(CTaskSimpleTrainGetIn, 0x10);

#endif // __GTA_TASKSIMPLETRAINGETIN_H__
