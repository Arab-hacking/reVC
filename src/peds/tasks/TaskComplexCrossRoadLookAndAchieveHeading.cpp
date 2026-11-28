#include "TaskComplexCrossRoadLookAndAchieveHeading.h"

#include "Ped.h"
#include "TaskSimpleAchieveHeading.h"
#include "TaskSimpleRunAnim.h"

// SA CTaskComplexCrossRoadLookAndAchieveHeading (0x631AE0).

CTaskComplexCrossRoadLookAndAchieveHeading::CTaskComplexCrossRoadLookAndAchieveHeading(int32 durationInMs, float fTargetHeading)
: m_nDuration(durationInMs), m_fTargetHeading(fTargetHeading)
{
}

// 0x631C40
CTask*
CTaskComplexCrossRoadLookAndAchieveHeading::CreateNextSubTask(CPed *ped)
{
	switch(m_pSubTask->GetTaskType()){
	case TASK_SIMPLE_ANIM:
		return CreateSubTask(TASK_FINISHED, ped);
	case TASK_SIMPLE_ACHIEVE_HEADING:
		return CreateSubTask(TASK_SIMPLE_ANIM, ped);
	default:
		return nil;
	}
}

// 0x631B60
CTask*
CTaskComplexCrossRoadLookAndAchieveHeading::CreateSubTask(eTaskType taskType, CPed *ped)
{
	switch(taskType){
	case TASK_SIMPLE_ANIM:
		return new CTaskSimpleRunAnim(ped->m_animGroup, ANIM_ID_ROADCROSS, 4.0f, false);
	case TASK_SIMPLE_ACHIEVE_HEADING:
		return new CTaskSimpleAchieveHeading(m_fTargetHeading, 0.5f, 0.2f);
	case TASK_FINISHED:
		return nil;
	default:
		return nil;
	}
}

// 0x631B20
bool
CTaskComplexCrossRoadLookAndAchieveHeading::MakeAbortable(CPed *ped, eAbortPriority priority, const CEvent *event)
{
	if(priority == ABORT_PRIORITY_URGENT || priority == ABORT_PRIORITY_IMMEDIATE)
		return m_pSubTask->MakeAbortable(ped, priority, event);
	return false;
}
