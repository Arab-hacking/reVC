#pragma once

// SA CTaskComplexFollowNodeRoute: get to a point along the ped path
// network - compute a node route, then walk it node by node with the
// plain/fine go-to tasks, optionally easing speed through corners
// ("blending"). The route containers are SA's fixed 8-slot routes.

#include "NodeAddress.h"
#include "NodeRoute.h"
#include "PedType.h"
#include "PointRoute.h"
#include "TaskComplex.h"
#include "TaskTimer.h"
#include "Vector.h"

class CTaskComplexFollowNodeRoute : public CTaskComplex
{
public:
	CVector m_TargetPt;
	eMoveState m_MoveState;
	float m_TargetTolerance;
	float m_SlowDownDist;
	float m_FollowNodeThresholdHeightChange;
	CNodeAddress m_StartNode;
	CNodeRoute *m_NodeRoute;
	CPointRoute *m_PtRoute;
	CNodeAddress m_CurrNode;
	uint32 m_CurrPtIdx;
	int32 m_TimeToFinishMs;
	CTaskTimer m_FinishedTimer;
	union {
		uint8 m_FollowFlags;
		struct {
			uint8 m_bKeepNodesHeadingAwayFromTarget : 1;
			uint8 m_LastRoutePointIsTarget : 1;
			uint8 m_bNewTarget : 1;
			uint8 m_bUseBlending : 1;
			uint8 m_bWillSlowDown : 1;
			uint8 m_bSlowingDown : 1;
			uint8 m_bSpeedingUp : 1;
		};
	};
	float m_SpeedDecreaseDist;
	float m_SpeedIncreaseDist;
	float m_SpeedDecreaseAmt;
	float m_SpeedIncreaseAmt;

public:
	CTaskComplexFollowNodeRoute(eMoveState moveState, const CVector &targetPt, float targetPtTolerance = 0.5f,
		float slowDownDist = 3.0f, float followNodeThresholdHeightChange = 2.0f,
		bool bKeepNodesHeadingAwayFromTarget = false, int32 time = -1, bool bUseBlending = true);
	CTaskComplexFollowNodeRoute(const CTaskComplexFollowNodeRoute &other);
	~CTaskComplexFollowNodeRoute();

	static eTaskType GetSubTaskType(uint32 progress, bool bLastRoutePointIsTarget, const CPointRoute &route);

	CTask *Clone() const { return new CTaskComplexFollowNodeRoute(*this); }
	eTaskType GetTaskType() const { return TASK_COMPLEX_FOLLOW_NODE_ROUTE; }
	void StopTimer(const CEvent *event);
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil);
	CTask *CreateNextSubTask(CPed *ped);
	CTask *CreateFirstSubTask(CPed *ped);
	CTask *ControlSubTask(CPed *ped);

	void SetTarget(CPed *ped, const CVector &target, float targetPtTolerance, float slowDownDistance, float followNodeThresholdHeightChange, bool bForce);
	void SetStartNode(CNodeAddress naddr) { m_StartNode = naddr; }

protected:
	CTask *CreateSubTask(eTaskType taskType, CPed *ped) const;

	CVector GetLastWaypoint(CPed *ped) const;
	CVector GetNextWaypoint(CPed *ped) const;
	eTaskType CalcGoToTaskType(CPed *ped, eTaskType taskType);
	float CalcBlendRatio(CPed *ped, bool bUsePointRoute) const;
	bool CanGoStraightThere(const CPed *ped, const CVector &from, const CVector &to, float maxDist) const;
	void ComputePathNodes(const CPed *ped);
	void ComputeRoute();
	CVector GetCurrentPt() const { return (*m_PtRoute)[m_CurrPtIdx]; }
};

VALIDATE_SIZE(CTaskComplexFollowNodeRoute, 0x60);
