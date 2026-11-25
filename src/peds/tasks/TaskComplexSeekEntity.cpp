#include "common.h"

#include "TaskComplexSeekEntity.h"

#include "Entity.h"
#include "Ped.h"
#include "TaskComplexFollowNodeRoute.h"
#include "TaskComplexGoToPointAndStandStill.h"
#include "TaskComplexSequence.h"
#include "TaskComplexTurnToFaceEntityOrCoord.h"
#include "TaskSimpleCarDriveTimed.h"
#include "TaskSimpleStandStill.h"
#include "TaskSimpleTired.h"

// SA CTaskComplexSeekEntity (0x665600 family). Adaptations: only the
// standard seek-pos calculator; TASK_COMPLEX_LEAVE_CAR is not ported
// (starting in a vehicle ends the task); the shared-boat attachment and
// follower chatter in ControlSubTask have no local counterpart and are
// dropped.

CTaskComplexSeekEntity::CTaskComplexSeekEntity(CEntity *entity, int32 seekInterval, int32 scanInterval, float maxEntityDist2D,
	float moveStateRadius, float followNodeThresholdHeightChange, bool bPlayTiredAnim, bool bFaceEntityWhenDone)
: m_entityToSeek(entity), m_seekInterval(seekInterval), m_scanInterval(scanInterval), m_maxEntityDist2D(maxEntityDist2D),
  m_moveStateRadius(moveStateRadius), m_followNodeThresholdHeightChange(followNodeThresholdHeightChange)
{
	m_minEntityDist2D = 0.0f;
	m_moveState = PEDMOVE_RUN;
	m_SeekFlags = 0;
	m_bPlayTiredAnim = bPlayTiredAnim;
	m_bFaceEntityWhenDone = bFaceEntityWhenDone;
	if(m_entityToSeek)
		m_entityToSeek->RegisterReference(&m_entityToSeek);
}

CTaskComplexSeekEntity::CTaskComplexSeekEntity(const CTaskComplexSeekEntity &other)
: CTaskComplexSeekEntity(other.m_entityToSeek, other.m_seekInterval, other.m_scanInterval, other.m_maxEntityDist2D,
	other.m_moveStateRadius, other.m_followNodeThresholdHeightChange, other.m_bPlayTiredAnim, true)
{
	m_moveState = other.m_moveState;
	m_bTrackingEntity = other.m_bTrackingEntity;
	m_minEntityDist2D = other.m_minEntityDist2D;
}

CTaskComplexSeekEntity::~CTaskComplexSeekEntity()
{
	if(m_entityToSeek)
		m_entityToSeek->CleanUpOldReference(&m_entityToSeek);
}

CTask*
CTaskComplexSeekEntity::CreateSubTask(eTaskType taskType, CPed *ped)
{
	switch(taskType){
	case TASK_FINISHED:
		return nil;
	case TASK_COMPLEX_TURN_TO_FACE_ENTITY:
		return new CTaskComplexTurnToFaceEntityOrCoord(m_entityToSeek);
	case TASK_COMPLEX_FOLLOW_NODE_ROUTE:
		return new CTaskComplexFollowNodeRoute(m_moveState, GetSeekPos(ped), m_maxEntityDist2D, m_moveStateRadius,
			m_followNodeThresholdHeightChange, false, -1, true);
	case TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL:
		return new CTaskComplexGoToPointAndStandStill(m_moveState, GetSeekPos(ped), m_maxEntityDist2D, m_moveStateRadius,
			false, false, m_bTrackingEntity);
	case TASK_SIMPLE_CAR_DRIVE_TIMED:
		return new CTaskSimpleCarDriveTimed(ped->m_pMyVehicle, 2000);
	case TASK_COMPLEX_LEAVE_CAR:
		return nil; // not ported yet
	case TASK_COMPLEX_SEQUENCE: {
		CTaskComplexSequence *seq = new CTaskComplexSequence();
		if(m_bFaceEntityWhenDone)
			seq->AddTask(new CTaskComplexTurnToFaceEntityOrCoord(m_entityToSeek));
		seq->AddTask(new CTaskSimpleStandStill(100));
		return seq;
	}
	case TASK_SIMPLE_STAND_STILL:
		return new CTaskSimpleStandStill(2000);
	case TASK_SIMPLE_TIRED:
		return new CTaskSimpleTired(1000);
	default:
		return nil;
	}
}

bool
CTaskComplexSeekEntity::MakeAbortable(CPed *ped, eAbortPriority priority, const CEvent *event)
{
	if(priority == ABORT_PRIORITY_LEISURE){
		m_scanInterval = -1;
		m_seekTimer.SetOutOfTime();
	}

	if(m_pSubTask->MakeAbortable(ped, priority, event)){
		m_scanTimer.SetOutOfTime();
		return true;
	}
	return false;
}

CTask*
CTaskComplexSeekEntity::CreateNextSubTask(CPed *ped)
{
	switch(m_pSubTask->GetTaskType()){
	case TASK_COMPLEX_SEQUENCE:
	case TASK_COMPLEX_TURN_TO_FACE_ENTITY:
		return CreateSubTask(TASK_FINISHED, ped);
	case TASK_COMPLEX_FOLLOW_NODE_ROUTE:
	case TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL: {
		CVector dist = ped->GetPosition() - GetSeekPos(ped);
		CVector2D dist2d(dist.x, dist.y);
		float dist2DSq = dist2d.MagnitudeSqr();
		if(dist2DSq > m_maxEntityDist2D * m_maxEntityDist2D)
			return CreateSubTaskWhenPedIsTooFarFromEntity(ped, dist2DSq);
		m_bAchievedSeekEntity = true;
		return CreateSubTask(m_bFaceEntityWhenDone ? TASK_COMPLEX_TURN_TO_FACE_ENTITY : TASK_FINISHED, ped);
	}
	case TASK_SIMPLE_CAR_DRIVE_TIMED:
		return CreateFirstSubTask(ped);
	case TASK_COMPLEX_LEAVE_CAR:
		return CreateSubTask(ped->bInVehicle ? TASK_FINISHED : TASK_SIMPLE_CAR_DRIVE_TIMED, ped);
	case TASK_SIMPLE_STAND_STILL:
		return CreateSubTask(TASK_SIMPLE_STAND_STILL, ped);
	case TASK_SIMPLE_TIRED: {
		CTaskSimpleStandStill task(-1);
		task.ProcessPed(ped);
		return CreateSubTask(TASK_FINISHED, ped);
	}
	default:
		return nil;
	}
}

CTask*
CTaskComplexSeekEntity::CreateFirstSubTask(CPed *ped)
{
	m_scanTimer.Start(m_scanInterval);
	m_seekTimer.Start(m_seekInterval);

	if(!m_entityToSeek)
		return CreateSubTask(TASK_SIMPLE_TIRED, ped);

	if(ped->bInVehicle)
		return CreateSubTask(TASK_COMPLEX_LEAVE_CAR, ped);

	CVector seekPos = GetSeekPos(ped);
	CVector dist = ped->GetPosition() - seekPos;
	CVector2D dist2d(dist.x, dist.y);
	float dist2DSq = dist2d.MagnitudeSqr();
	if(dist2DSq > m_maxEntityDist2D * m_maxEntityDist2D)
		return CreateSubTaskWhenPedIsTooFarFromEntity(ped, dist2DSq);

	m_bAchievedSeekEntity = true;
	ped->SetMoveState(PEDMOVE_STILL);
	return CreateSubTask(TASK_COMPLEX_SEQUENCE, ped);
}

CTask*
CTaskComplexSeekEntity::ControlSubTask(CPed *ped)
{
	eTaskType subTaskType = m_pSubTask->GetTaskType();

	switch(subTaskType){
	case TASK_COMPLEX_FOLLOW_NODE_ROUTE:
	case TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL:
		break;
	default:
		return nil;
	}

	if(!m_entityToSeek)
		return m_pSubTask->MakeAbortable(ped) ? nil : m_pSubTask;

	if(m_seekTimer.IsOutOfTime()){
		if(!m_pSubTask->MakeAbortable(ped))
			return m_pSubTask;
		if(!m_bPlayTiredAnim)
			return nil;
		return new CTaskSimpleTired(1000);
	}

	if(!m_scanTimer.IsOutOfTime())
		return m_pSubTask;

	m_scanTimer.Start(m_scanInterval);

	CVector seekPos = GetSeekPos(ped);
	CVector pedDist = ped->GetPosition() - seekPos;
	CVector2D pedDist2d(pedDist.x, pedDist.y);
	float pedToSeekPosDist2dSq = pedDist2d.MagnitudeSqr();

	if(subTaskType == TASK_COMPLEX_FOLLOW_NODE_ROUTE){
		if(m_minEntityDist2D == 0.0f || pedToSeekPosDist2dSq >= (m_minEntityDist2D - 1.0f) * (m_minEntityDist2D - 1.0f)){
			((CTaskComplexFollowNodeRoute*)m_pSubTask)->SetTarget(ped, seekPos, m_maxEntityDist2D, m_moveStateRadius, m_followNodeThresholdHeightChange, false);
		}else if(m_pSubTask->MakeAbortable(ped)){
			return CreateSubTask(TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL, ped);
		}
	}else{
		CTaskComplexGoToPointAndStandStill *subTaskStandStill = (CTaskComplexGoToPointAndStandStill*)m_pSubTask;
		if(subTaskStandStill->m_bWasSuccessful){
			return m_pSubTask;
		}else if(m_minEntityDist2D == 0.0f || pedToSeekPosDist2dSq <= (m_minEntityDist2D + 1.0f) * (m_minEntityDist2D + 1.0f)){
			subTaskStandStill->GoToPoint(seekPos, m_maxEntityDist2D, m_moveStateRadius, false);
		}else if(m_pSubTask->MakeAbortable(ped)){
			return CreateSubTask(TASK_COMPLEX_FOLLOW_NODE_ROUTE, ped);
		}
	}
	return m_pSubTask;
}

CVector
CTaskComplexSeekEntity::GetSeekPos(CPed *ped)
{
	if(m_entityToSeek == nil)
		return ped->GetPosition();
	CVector seekPos;
	m_entitySeekPosCalculator.ComputeEntitySeekPos(*ped, *m_entityToSeek, seekPos);
	return seekPos;
}

CTask*
CTaskComplexSeekEntity::CreateSubTaskWhenPedIsTooFarFromEntity(CPed *ped, float pedToSeekPosDist2DSq)
{
	return CreateSubTask(
		m_minEntityDist2D == 0.0f || pedToSeekPosDist2DSq > m_minEntityDist2D * m_minEntityDist2D
			? TASK_COMPLEX_FOLLOW_NODE_ROUTE : TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL,
		ped);
}
