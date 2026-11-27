#ifndef __GTA_TASKCOMPLEXFOLLOWPOINTROUTE_H__
#define __GTA_TASKCOMPLEXFOLLOWPOINTROUTE_H__

#include "TaskComplex.h"
#include "PedType.h"
#include "PointRoute.h"
#include "Vector.h"

// Follow a route of plain points (as opposed to path-network nodes):
// walk point to point, optionally easing off the speed into corners and
// back up out of them, and finish - or loop - according to the mode.
class CTaskComplexFollowPointRoute : public CTaskComplex
{
public:
	enum class Mode : uint32
	{
		ONE_WAY = 0,        ///< Do the route only once
		RETURN = 1,         ///< Follow route once, then follow it in reverse
		BACK_AND_FORTH = 2, ///< Same as RETURN, but looped forever
		LOOP = 3            ///< Loop the route, always in the same direction
	};

	Mode m_mode;
	int32 m_nRouteTraversals;
	eMoveState m_moveState;
	float m_targetRadius;
	float m_slowDownDistance;
	CPointRoute *m_route;
	uint32 m_currPointIdx;

	union {
		struct {
			uint8 m_bMustOvershootTarget : 1;
			uint8 m_bNewRoute : 1;
			uint8 m_bStandStillAfterMove : 1;
			uint8 m_bUseBlending : 1;
			uint8 m_bWillSlowDown : 1;
			uint8 m_bSlowingDown : 1;
			uint8 m_bSpeedingUp : 1;
		};
		uint8 m_routeFlags;
	};

	// For the two pairs below see CalcBlendRatio.
	float m_speedDecreaseDist;
	float m_speedIncreaseDist;
	float m_speedDecreaseAmt;
	float m_speedIncreaseAmt;

public:
	static const eTaskType Type = TASK_COMPLEX_FOLLOW_POINT_ROUTE;

	CTaskComplexFollowPointRoute(eMoveState moveState, const CPointRoute &route, Mode mode = Mode::ONE_WAY,
		float targetRadius = 0.5f, float slowDownDist = 0.0f, bool overshootTarget = true,
		bool useBlending = false, bool standStillAfterMove = false);
	CTaskComplexFollowPointRoute(const CTaskComplexFollowPointRoute &other);
	~CTaskComplexFollowPointRoute();

	eTaskType GetSubTaskType();
	CTask *CreateTask(eTaskType taskType, CPed *ped);
	CVector GetLastWaypoint(CPed *ped);
	CVector GetNextWaypoint(CPed *ped);
	eTaskType CalcGoToTaskType(CPed *ped, eTaskType subTaskType);
	float CalcBlendRatio(CPed *ped, bool slowing);

	CTask *Clone() const { return new CTaskComplexFollowPointRoute(*this); }
	eTaskType GetTaskType() const { return Type; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil);
	CTask *CreateNextSubTask(CPed *ped);
	CTask *CreateFirstSubTask(CPed *ped);
	CTask *ControlSubTask(CPed *ped);
};

VALIDATE_SIZE(CTaskComplexFollowPointRoute, 0x44);

#endif // __GTA_TASKCOMPLEXFOLLOWPOINTROUTE_H__
