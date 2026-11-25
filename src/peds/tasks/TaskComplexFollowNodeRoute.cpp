#include "common.h"

#include "TaskComplexFollowNodeRoute.h"

#include "EventHandler.h"
#include "General.h"
#include "PathFind.h"
#include "Ped.h"
#include "PedIntelligence.h"
#include "TaskComplexGoToPointAndStandStill.h"
#include "TaskSimpleGoToPoint.h"
#include "TaskSimpleGoToPointFine.h"
#include "TaskSimpleStandStill.h"
#include "World.h"

// SA CTaskComplexFollowNodeRoute (0x66EA30). Adaptations for this
// codebase's pointer-based path network:
// - The route is computed with CPathFind::DoPathSearch into the same
//   fixed 8 slots SA's routes have, then converted to node addresses.
//   (SA's start-node argument, interior-manager re-routing and node-area
//   availability checks have no counterpart here.)
// - CanGoStraightThere only has the distance test: SA's ped geometry
//   analyser (height-change path clearness) is not ported, and the ped
//   has no bIgnoreHeightDifferenceFollowingNodes flag.
// - TASK_COMPLEX_LEAVE_CAR is not ported yet, so starting in a vehicle
//   ends the task (see the flee tasks for the same treatment).
// - Route points take the VC convention of standing 1 metre above the
//   node (as CTaskComplexWander::ComputeTargetPos does).

// 0x66EA30
CTaskComplexFollowNodeRoute::CTaskComplexFollowNodeRoute(eMoveState moveState, const CVector &targetPt, float targetPtTolerance,
	float slowDownDist, float followNodeThresholdHeightChange, bool bKeepNodesHeadingAwayFromTarget, int32 time, bool bUseBlending)
: m_TargetPt(targetPt), m_MoveState(moveState), m_TargetTolerance(targetPtTolerance), m_SlowDownDist(slowDownDist),
  m_FollowNodeThresholdHeightChange(followNodeThresholdHeightChange), m_TimeToFinishMs(time)
{
	m_NodeRoute = new CNodeRoute();
	m_PtRoute = new CPointRoute();
	m_CurrPtIdx = 0;
	m_FollowFlags = 0;
	m_bKeepNodesHeadingAwayFromTarget = bKeepNodesHeadingAwayFromTarget;
	m_bUseBlending = bUseBlending;
	m_SpeedDecreaseDist = 0.0f;
	m_SpeedIncreaseDist = 0.0f;
	m_SpeedDecreaseAmt = 0.0f;
	m_SpeedIncreaseAmt = 0.0f;
}

// 0x6713E0
CTaskComplexFollowNodeRoute::CTaskComplexFollowNodeRoute(const CTaskComplexFollowNodeRoute &other)
: CTaskComplexFollowNodeRoute(other.m_MoveState, other.m_TargetPt, other.m_TargetTolerance, other.m_SlowDownDist,
	other.m_FollowNodeThresholdHeightChange, other.m_bKeepNodesHeadingAwayFromTarget, other.m_TimeToFinishMs, other.m_bUseBlending)
{
	m_StartNode = other.m_StartNode;
}

// 0x66EB70
CTaskComplexFollowNodeRoute::~CTaskComplexFollowNodeRoute()
{
	delete m_PtRoute;
	delete m_NodeRoute;
}

// 0x669650
eTaskType
CTaskComplexFollowNodeRoute::GetSubTaskType(uint32 progress, bool bLastRoutePointIsTarget, const CPointRoute &route)
{
	if(!bLastRoutePointIsTarget)
		return TASK_SIMPLE_GO_TO_POINT;
	if(progress == (uint32)route.GetSize())
		return TASK_FINISHED;
	return route.GetSize() != (int32)progress + 1
		? TASK_SIMPLE_GO_TO_POINT
		: TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL;
}

// 0x6694E0
void
CTaskComplexFollowNodeRoute::StopTimer(const CEvent *event)
{
	if(!CEventHandler::IsTemporaryEvent(*event))
		m_FinishedTimer.Stop();
}

// 0x66EBE0
eTaskType
CTaskComplexFollowNodeRoute::CalcGoToTaskType(CPed *ped, eTaskType subTaskType)
{
	if(subTaskType != TASK_SIMPLE_GO_TO_POINT)
		return subTaskType;

	m_SpeedIncreaseAmt = m_SpeedDecreaseAmt;
	m_bWillSlowDown = false;

	if(m_MoveState >= PEDMOVE_JOG){
		if(m_PtRoute->IsEmpty())
			return TASK_SIMPLE_GO_TO_POINT;

		assert(m_CurrPtIdx + 1 <= (uint32)m_PtRoute->GetSize());

		CVector currPt = GetCurrentPt();
		CVector toCurr = currPt - GetLastWaypoint(ped);
		CVector toNext = GetNextWaypoint(ped) - currPt;
		toCurr.Normalise();
		toNext.Normalise();
		float cosAngleBetweenRoutePoints = DotProduct(toCurr, toNext);
		const float COS_45 = 0.70710678f;
		if((m_bWillSlowDown = cosAngleBetweenRoutePoints <= COS_45)){
			bool sprint = m_MoveState == PEDMOVE_SPRINT;
			m_SpeedIncreaseDist = m_SpeedDecreaseDist = sprint ? 5.0f : 4.0f;
			float ratio = 1.0f - (cosAngleBetweenRoutePoints + 1.0f) / (COS_45 + 1.0f);
			m_SpeedDecreaseAmt = Clamp(ratio, 0.0f, 1.0f) * (sprint ? 3.0f : 1.5f);
		}
	}

	if(m_bSlowingDown){
		m_bSlowingDown = false;
		m_bSpeedingUp = true;
		return TASK_SIMPLE_GO_TO_POINT_FINE;
	}
	m_bSpeedingUp = false;
	m_bSlowingDown = false;
	return TASK_SIMPLE_GO_TO_POINT;
}

// 0x66EDC0
float
CTaskComplexFollowNodeRoute::CalcBlendRatio(CPed *ped, bool slowing) const
{
	float spdChangeDistSq = (slowing ? m_SpeedDecreaseDist : m_SpeedIncreaseDist);
	spdChangeDistSq *= spdChangeDistSq;
	CVector distVec = (slowing ? GetCurrentPt() : GetLastWaypoint(ped)) - ped->GetPosition();
	float distToPtSq = distVec.MagnitudeSqr();
	if(distToPtSq >= spdChangeDistSq)
		return -1.0f;
	float spdChangeAmt = slowing ? m_SpeedDecreaseAmt : m_SpeedIncreaseAmt;
	float spdChangeRatio = cosf(Clamp(distToPtSq / spdChangeDistSq, 0.0f, 1.0f) * PI) * 0.5f + 0.5f;
	return Max(
		CTaskSimpleGoToPointFine::BaseRatio(PEDMOVE_WALK) + 0.75f,
		CTaskSimpleGoToPointFine::BaseRatio(m_MoveState) - spdChangeRatio * spdChangeAmt);
}

// 0x66EF20 (distance test only; see file header)
bool
CTaskComplexFollowNodeRoute::CanGoStraightThere(const CPed *ped, const CVector &from, const CVector &to, float maxDist) const
{
	(void)ped;
	CVector dist = from - to;
	return dist.MagnitudeSqr() < maxDist * maxDist;
}

// 0x671750
void
CTaskComplexFollowNodeRoute::SetTarget(CPed *ped, const CVector &targetPt, float targetTolerance, float slowDownDistance, float followNodeThresholdHeightChange, bool bForce)
{
	if(!bForce){
		if(m_TargetPt == targetPt && m_TargetTolerance == targetTolerance && m_SlowDownDist == slowDownDistance && m_FollowNodeThresholdHeightChange == followNodeThresholdHeightChange)
			return; // nothing changed
	}

	m_TargetPt = targetPt;
	m_TargetTolerance = targetTolerance;
	m_SlowDownDist = slowDownDistance;
	m_FollowNodeThresholdHeightChange = followNodeThresholdHeightChange;

	if(m_bUseBlending){
		m_SpeedDecreaseDist = m_SpeedIncreaseDist = 0.0f;
		m_SpeedDecreaseAmt = m_SpeedIncreaseAmt = 0.0f;
		m_bSlowingDown = m_bSpeedingUp = false;
	}

	ComputePathNodes(ped);
	ComputeRoute();

	m_bNewTarget = true;
}

// 0x669690
CTask*
CTaskComplexFollowNodeRoute::CreateSubTask(eTaskType taskType, CPed *ped) const
{
	switch(taskType){
	case TASK_SIMPLE_GO_TO_POINT_FINE:
		return new CTaskSimpleGoToPointFine(CTaskSimpleGoToPointFine::BaseRatio(m_MoveState), GetCurrentPt());
	case TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL:
		return new CTaskComplexGoToPointAndStandStill(m_MoveState, GetCurrentPt(), m_TargetTolerance, m_SlowDownDist);
	case TASK_COMPLEX_LEAVE_CAR:
		return nil; // not ported yet
	case TASK_SIMPLE_GO_TO_POINT:
		return new CTaskSimpleGoToPoint(m_MoveState, GetCurrentPt());
	case TASK_SIMPLE_STAND_STILL: {
		ped->Teleport(m_TargetPt);
		return new CTaskSimpleStandStill();
	}
	case TASK_FINISHED:
		return nil;
	default:
		return nil;
	}
}

// 0x6698E0
CVector
CTaskComplexFollowNodeRoute::GetLastWaypoint(CPed *ped) const
{
	if(m_PtRoute->GetSize() == 1)
		return (*m_PtRoute)[0];
	return m_CurrPtIdx != 0
		? (*m_PtRoute)[m_CurrPtIdx - 1]
		: ped->GetPosition();
}

// 0x669980
CVector
CTaskComplexFollowNodeRoute::GetNextWaypoint(CPed *ped) const
{
	(void)ped;
	if(m_PtRoute->GetSize() == 1)
		return (*m_PtRoute)[0];
	uint32 nextPt = m_CurrPtIdx + 1;
	return (*m_PtRoute)[nextPt < (uint32)m_PtRoute->GetSize() ? nextPt : m_CurrPtIdx - 1];
}

// 0x66EFA0
void
CTaskComplexFollowNodeRoute::ComputePathNodes(const CPed *ped)
{
	// Make sure TargetPt.z is on the ground
	{
		bool hasTargetPtGroundZ;
		float targetPtGroundZ = CWorld::FindGroundZFor3DCoord(m_TargetPt.x, m_TargetPt.y, m_TargetPt.z + 1.0f, &hasTargetPtGroundZ);
		if(hasTargetPtGroundZ)
			m_TargetPt.z = targetPtGroundZ + 1.0f;
	}

	const CVector pedPos = ped->GetPosition();

	m_NodeRoute->Clear();
	if(!CanGoStraightThere(ped, pedPos, m_TargetPt, ped->GetIntelligence()->m_FollowNodeThresholdDistance)){
		CPathNode *nodes[8];
		int16 numNodes = 0;
		ThePaths.DoPathSearch(PATH_PED, pedPos, -1, m_TargetPt, nodes, &numNodes, 8, nil, nil, 999999.88f, -1);
		for(int32 i = 0; i < numNodes && i < 8; i++)
			m_NodeRoute->Add(AddressFromNode(nodes[i]));
	}
	m_StartNode = CNodeAddress();

	if(Abs(pedPos.z - m_TargetPt.z) >= m_FollowNodeThresholdHeightChange || m_NodeRoute->IsEmpty() || m_bKeepNodesHeadingAwayFromTarget){
		if(!m_NodeRoute->IsEmpty() && !m_bKeepNodesHeadingAwayFromTarget){
			CPathNode *routeFirstNode = NodeFromAddress((*m_NodeRoute)[0]);
			if(routeFirstNode){
				CVector firstDist = m_TargetPt - pedPos;
				CVector nodeDist = routeFirstNode->GetPosition() - pedPos;
				if(firstDist.Magnitude() + 50.0f < nodeDist.Magnitude())
					m_NodeRoute->Clear();
			}
		}
	}else{
		CPathNode *routeFirstNode = NodeFromAddress((*m_NodeRoute)[0]);
		if(routeFirstNode){
			CVector firstDist = m_TargetPt - pedPos;
			CVector nodeDist = routeFirstNode->GetPosition() - pedPos;
			if(firstDist.Magnitude() + 3.0f < nodeDist.Magnitude())
				m_NodeRoute->Clear();
		}
	}

	// Make the route as short as possible by cutting excess nodes (SA's
	// exterior branch; the interior-manager branch has no local analog).
	for(int32 i = 0; i < m_NodeRoute->GetSize(); i++){
		CPathNode *node = NodeFromAddress((*m_NodeRoute)[i]);
		if(node == nil)
			continue;
		CVector nodePos = node->GetPosition();
		nodePos.z += 1.0f;
		if(CanGoStraightThere(ped, nodePos, m_TargetPt, ped->GetIntelligence()->m_FollowNodeThresholdDistance)){
			m_NodeRoute->ResizeTo(i + 1);
			break;
		}
	}

	m_CurrPtIdx = 0;
	if(m_CurrNode.IsValid()){
		for(int32 i = 0; i < m_NodeRoute->GetSize(); i++){
			if((*m_NodeRoute)[i] == m_CurrNode){
				m_CurrPtIdx = i;
				break;
			}
		}
	}
	if(!m_NodeRoute->IsEmpty())
		m_CurrNode = (*m_NodeRoute)[m_CurrPtIdx];
}

// 0x6699E0
void
CTaskComplexFollowNodeRoute::ComputeRoute()
{
	m_PtRoute->Clear();
	for(int32 i = 0; i < m_NodeRoute->GetSize(); i++){
		CPathNode *node = NodeFromAddress((*m_NodeRoute)[i]);
		if(node == nil)
			continue;
		CVector pos = node->GetPosition();
		pos.z += 1.0f;
		m_PtRoute->Add(pos);
	}
	m_LastRoutePointIsTarget = !m_PtRoute->IsFull();
	if(m_LastRoutePointIsTarget)
		m_PtRoute->Add(m_TargetPt);
}

// 0x669520. SA additionally widens the go-to target radius when a stopped
// vehicle blocks the way; that needs the vehicle-collision event classes
// and the ped geometry analyser, both unported.
bool
CTaskComplexFollowNodeRoute::MakeAbortable(CPed *ped, eAbortPriority priority, const CEvent *event)
{
	return m_pSubTask->MakeAbortable(ped, priority, event);
}

// 0x6718D0
CTask*
CTaskComplexFollowNodeRoute::CreateNextSubTask(CPed *ped)
{
	eTaskType subTaskType = m_pSubTask->GetTaskType();

	if(m_FinishedTimer.IsOutOfTime()){
		if(subTaskType == TASK_SIMPLE_STAND_STILL){
			ped->Teleport(m_TargetPt);
			return CreateSubTask(TASK_FINISHED, ped);
		}
	}

	if(subTaskType == TASK_COMPLEX_LEAVE_CAR)
		return ped->bInVehicle ? CreateSubTask(TASK_FINISHED, ped) : CreateFirstSubTask(ped);

	if(m_LastRoutePointIsTarget){
		switch(subTaskType){
		case TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL:
			m_CurrPtIdx++;
			break;
		case TASK_SIMPLE_STAND_STILL:
			return CreateSubTask(TASK_FINISHED, ped);
		case TASK_SIMPLE_GO_TO_POINT:
			m_CurrPtIdx++;
			if(m_CurrPtIdx < (uint32)m_NodeRoute->GetSize())
				m_CurrNode = (*m_NodeRoute)[m_CurrPtIdx];
			break;
		case TASK_SIMPLE_GO_TO_POINT_FINE:
		default:
			return nil;
		}
	}else{
		if(m_CurrPtIdx + 1 == (uint32)m_PtRoute->GetSize()){
			// Reached the last point in the route, calculate next few
			SetTarget(ped, m_TargetPt, m_TargetTolerance, m_SlowDownDist, m_FollowNodeThresholdHeightChange, true);
			return CreateFirstSubTask(ped);
		}
		assert(!m_PtRoute->IsEmpty());
		assert(m_CurrPtIdx < (uint32)m_PtRoute->GetSize());

		m_CurrPtIdx++;
		m_CurrNode = (*m_NodeRoute)[m_CurrPtIdx];
	}

	eTaskType taskType = GetSubTaskType(m_CurrPtIdx, m_LastRoutePointIsTarget, *m_PtRoute);
	return CreateSubTask(m_bUseBlending ? taskType : CalcGoToTaskType(ped, taskType), ped);
}

// 0x671800
CTask*
CTaskComplexFollowNodeRoute::CreateFirstSubTask(CPed *ped)
{
	if(m_TimeToFinishMs >= 0)
		m_FinishedTimer.Start(m_TimeToFinishMs);

	if(m_bUseBlending){
		m_SpeedDecreaseDist = m_SpeedIncreaseDist = 0.0f;
		m_SpeedDecreaseAmt = m_SpeedIncreaseAmt = 0.0f;
		m_bSlowingDown = m_bSpeedingUp = false;
	}

	if(ped->bInVehicle)
		return CreateSubTask(TASK_COMPLEX_LEAVE_CAR, ped);

	SetTarget(ped, m_TargetPt, m_TargetTolerance, m_SlowDownDist, m_FollowNodeThresholdHeightChange, true);
	m_bNewTarget = false;

	eTaskType taskType = GetSubTaskType(m_CurrPtIdx, m_LastRoutePointIsTarget, *m_PtRoute);
	return CreateSubTask(m_bUseBlending ? taskType : CalcGoToTaskType(ped, taskType), ped);
}

// 0x671AB0
CTask*
CTaskComplexFollowNodeRoute::ControlSubTask(CPed *ped)
{
	eTaskType subTaskType = m_pSubTask->GetTaskType();

	if(m_bNewTarget && subTaskType != TASK_COMPLEX_LEAVE_CAR)
		return CreateFirstSubTask(ped);

	if(m_FinishedTimer.IsOutOfTime()){
		if(subTaskType != TASK_SIMPLE_STAND_STILL){
			return m_pSubTask->MakeAbortable(ped)
				? CreateSubTask(TASK_SIMPLE_STAND_STILL, ped)
				: m_pSubTask;
		}
	}

	if(m_bUseBlending){
		// If we're already going fine, adjust move ratio
		if(subTaskType == TASK_SIMPLE_GO_TO_POINT_FINE){
			CTaskSimpleGoToPointFine *goToPointFine = (CTaskSimpleGoToPointFine*)m_pSubTask;

			float blendRatio = CalcBlendRatio(ped, true);
			goToPointFine->SetMoveRatio(blendRatio >= 0.0f ? blendRatio : CTaskSimpleGoToPointFine::BaseRatio(m_MoveState));

			if(m_bSpeedingUp && blendRatio < 0.0f){
				m_bSpeedingUp = false;
				return CreateSubTask(TASK_SIMPLE_GO_TO_POINT, ped);
			}
		}

		// If we're supposed to slow down, but aren't yet
		if(m_bWillSlowDown && !m_bSlowingDown && subTaskType != TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL){
			float blendRatio = CalcBlendRatio(ped, true);
			if(blendRatio >= 0.0f){
				m_bSpeedingUp = false;
				m_bWillSlowDown = false;
				m_bSlowingDown = true;

				CTask *goToPtFine = subTaskType != TASK_SIMPLE_GO_TO_POINT_FINE
					? CreateSubTask(TASK_SIMPLE_GO_TO_POINT_FINE, ped)
					: m_pSubTask;
				((CTaskSimpleGoToPointFine*)goToPtFine)->SetMoveRatio(blendRatio);

				return goToPtFine;
			}
		}
	}

	return m_pSubTask;
}
