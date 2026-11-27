#include "common.h"

#include "TaskComplexEnterTrainAsPassenger.h"

#include "Ped.h"
#include "TaskComplexGoToPointAndStandStill.h"
#include "TaskSimpleTrainGetIn.h"
#include "Train.h"
#include "Vehicle.h"

CTaskComplexEnterTrainAsPassenger::CTaskComplexEnterTrainAsPassenger(CVehicle *train)
	: m_Train(train), m_DoorPos(0.0f, 0.0f, 0.0f)
{
	if (m_Train != nil)
		m_Train->RegisterReference((CEntity **)&m_Train);
}

CTaskComplexEnterTrainAsPassenger::CTaskComplexEnterTrainAsPassenger(const CTaskComplexEnterTrainAsPassenger &other)
	: CTaskComplexEnterTrainAsPassenger(other.m_Train)
{
	m_DoorPos = other.m_DoorPos;
}

CTaskComplexEnterTrainAsPassenger::~CTaskComplexEnterTrainAsPassenger()
{
	if (m_Train != nil)
		m_Train->CleanUpOldReference((CEntity **)&m_Train);
}

CTask *
CTaskComplexEnterTrainAsPassenger::CreateSubTask(eTaskType taskType, CPed *ped)
{
	switch (taskType) {
	case TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL:
		return new CTaskComplexGoToPointAndStandStill(PEDMOVE_RUN, m_DoorPos, 0.6f, 2.0f);
	case TASK_SIMPLE_TRAIN_GET_IN:
		return new CTaskSimpleTrainGetIn(m_Train);
	default:
		return nil;
	}
}

CTask *
CTaskComplexEnterTrainAsPassenger::CreateNextSubTask(CPed *ped)
{
	eTaskType next;
	switch (m_pSubTask->GetTaskType()) {
	case TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL:
		next = ((CTaskComplexGoToPointAndStandStill *)m_pSubTask)->m_bWasSuccessful
			? TASK_SIMPLE_TRAIN_GET_IN : TASK_FINISHED;
		break;
	default:
		next = TASK_FINISHED;
		break;
	}
	return CreateSubTask(next, ped);
}

CTask *
CTaskComplexEnterTrainAsPassenger::CreateFirstSubTask(CPed *ped)
{
	CTrain *train = (CTrain *)m_Train;
	if (train == nil || ped->bInVehicle)
		return nil;
	if (train->GetStatus() != STATUS_TRAIN_NOT_MOVING || !train->m_aDoors[0].IsFullyOpen())
		return nil;
	ped->GetNearestTrainPedPosition(train, m_DoorPos);
	return CreateSubTask(TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL, ped);
}
