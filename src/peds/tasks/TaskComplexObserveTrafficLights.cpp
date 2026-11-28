#include "TaskComplexObserveTrafficLights.h"

#include "General.h"
#include "Ped.h"
#include "TaskSimpleRunAnim.h"
#include "TaskSimpleStandStill.h"
#include "TrafficLights.h"

// SA CTaskComplexObserveTrafficLights (0x6317E0).

CTask*
CTaskComplexObserveTrafficLights::CreateNextSubTask(CPed *ped)
{
	if(m_pSubTask == nil || m_pSubTask->GetTaskType() == TASK_SIMPLE_STAND_STILL)
		return new CTaskSimpleRunAnim(ped->m_animGroup, ANIM_ID_ROADCROSS, 4.0f, false);
	if(m_pSubTask->GetTaskType() == TASK_SIMPLE_ANIM)
		return new CTaskSimpleStandStill(CGeneral::GetRandomNumberInRange(3000, 5000));
	return nil;
}

// 0x6318E0
CTask*
CTaskComplexObserveTrafficLights::ControlSubTask(CPed *ped)
{
	if(CTrafficLights::LightForPeds() || !m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_LEISURE, nil))
		return m_pSubTask;
	return nil;
}
