#include "common.h"

#include "TaskComplexFallAndGetUp.h"

#include "Ped.h"
#include "TaskSimpleFall.h"
#include "TaskSimpleGetUp.h"

CTaskComplexFallAndGetUp::CTaskComplexFallAndGetUp(AnimationId nFallAnimId, AssocGroupId nFallAnimGroup, int32 nFallDownTime)
	: m_nFallAnimId(nFallAnimId), m_nFallAnimGroup(nFallAnimGroup), m_nFallDownTime(nFallDownTime)
{
}

CTaskComplexFallAndGetUp::CTaskComplexFallAndGetUp(int32 nDir, int32 nFallDownTime)
	: m_nFallDownTime(nFallDownTime)
{
	m_nFallAnimGroup = ANIM_GROUP_DEFAULT;
	switch (nDir) {
	case 0:  m_nFallAnimId = ANIM_ID_KO_SKID_FRONT; break;
	case 1:  m_nFallAnimId = ANIM_ID_KO_SPIN_R; break;
	case 2:  m_nFallAnimId = ANIM_ID_KO_SKID_BACK; break;
	default: m_nFallAnimId = ANIM_ID_KO_SPIN_L; break;
	}
}

CTaskComplexFallAndGetUp::CTaskComplexFallAndGetUp(const CTaskComplexFallAndGetUp &other)
	: CTaskComplexFallAndGetUp(other.m_nFallAnimId, other.m_nFallAnimGroup, other.m_nFallDownTime)
{
}

CTask *
CTaskComplexFallAndGetUp::CreateSubTask(eTaskType taskType)
{
	switch (taskType) {
	case TASK_SIMPLE_FALL:
		return new CTaskSimpleFall(m_nFallAnimId, m_nFallAnimGroup, m_nFallDownTime);
	case TASK_SIMPLE_GET_UP:
		return new CTaskSimpleGetUp();
	default:
		return nil;
	}
}

bool
CTaskComplexFallAndGetUp::IsFalling()
{
	return m_pSubTask->GetTaskType() == TASK_SIMPLE_FALL;
}

void
CTaskComplexFallAndGetUp::SetDownTime(int32 nTime)
{
	m_nFallDownTime = nTime;
	if (IsFalling()) {
		CTaskSimpleFall *fallTask = (CTaskSimpleFall *)m_pSubTask;
		fallTask->m_nTotalDownTime = nTime;
		fallTask->m_nCurrentDownTime = nTime;
	}
}

CTask *
CTaskComplexFallAndGetUp::CreateFirstSubTask(CPed *ped)
{
	return CreateSubTask(TASK_SIMPLE_FALL);
}

CTask *
CTaskComplexFallAndGetUp::CreateNextSubTask(CPed *ped)
{
	switch (m_pSubTask->GetTaskType()) {
	case TASK_SIMPLE_FALL:
		return CreateSubTask(TASK_SIMPLE_GET_UP);
	case TASK_SIMPLE_GET_UP:
		return CreateSubTask(TASK_FINISHED);
	default:
		return nil;
	}
}

bool
CTaskComplexFallAndGetUp::MakeAbortable(CPed *ped, eAbortPriority priority, const CEvent *event)
{
	if (priority == ABORT_PRIORITY_IMMEDIATE)
		return m_pSubTask->MakeAbortable(ped, priority, event);

	// The reference also refuses while the ped is being arrested;
	// that flag does not exist here.
	if (priority == ABORT_PRIORITY_LEISURE) {
		if (IsFalling())
			return false;
		m_nFallDownTime = 0;
	}

	return m_pSubTask->MakeAbortable(ped, priority, event);
}
