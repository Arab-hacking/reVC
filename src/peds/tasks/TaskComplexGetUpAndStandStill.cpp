#include "common.h"

#include "TaskComplexGetUpAndStandStill.h"

#include "TaskSimpleGetUp.h"
#include "TaskSimpleStandStill.h"

CTask *
CTaskComplexGetUpAndStandStill::CreateSubTask(eTaskType taskType)
{
	switch (taskType) {
	case TASK_SIMPLE_GET_UP:
		return new CTaskSimpleGetUp();
	case TASK_SIMPLE_STAND_STILL:
		return new CTaskSimpleStandStill(0, false, false, 8.0f);
	default:
		return nil;
	}
}

CTask *
CTaskComplexGetUpAndStandStill::CreateFirstSubTask(CPed *ped)
{
	return CreateSubTask(TASK_SIMPLE_GET_UP);
}

CTask *
CTaskComplexGetUpAndStandStill::CreateNextSubTask(CPed *ped)
{
	if (m_pSubTask->GetTaskType() == TASK_SIMPLE_STAND_STILL)
		return CreateSubTask(TASK_FINISHED);

	if (m_pSubTask->GetTaskType() == TASK_SIMPLE_GET_UP) {
		CTaskSimpleGetUp *subTask = (CTaskSimpleGetUp *)m_pSubTask;
		return CreateSubTask(subTask->m_bHasPedGotUp ? TASK_SIMPLE_STAND_STILL : TASK_FINISHED);
	}

	return nil;
}
