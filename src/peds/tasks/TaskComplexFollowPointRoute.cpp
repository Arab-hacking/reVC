#include "common.h"

#include "TaskComplexFollowPointRoute.h"

#include "Ped.h"
#include "PedGeometryAnalyser.h"
#include "PedIntelligence.h"
#include "TaskComplexGoToPointAndStandStill.h"
#include "TaskSimpleGoToPoint.h"
#include "TaskSimpleGoToPointFine.h"
#include "TaskSimpleStandStill.h"
#include "maths.h"

#include <cfloat>

static const int32 STAND_STILL_TIME = 20000;

static CVector2D Normalized2D(const CVector2D &v)
{
	CVector2D result(v);
	result.Normalise();
	return result;
}

static float ClampFloat(float v, float lo, float hi)
{
	return v < lo ? lo : (v > hi ? hi : v);
}

CTaskComplexFollowPointRoute::CTaskComplexFollowPointRoute(eMoveState moveState, const CPointRoute &route, Mode mode,
	float targetRadius, float slowDownDist, bool overshootTarget, bool useBlending, bool standStillAfterMove)
	: m_mode(mode), m_nRouteTraversals(0), m_moveState(moveState), m_targetRadius(targetRadius),
	  m_slowDownDistance(slowDownDist), m_route(new CPointRoute(route)), m_currPointIdx(0),
	  m_speedDecreaseDist(0.0f), m_speedIncreaseDist(0.0f), m_speedDecreaseAmt(0.0f), m_speedIncreaseAmt(0.0f)
{
	m_routeFlags = 0;
	m_bMustOvershootTarget = overshootTarget;
	m_bNewRoute = true;
	m_bStandStillAfterMove = standStillAfterMove;
	m_bUseBlending = useBlending;
	m_bSpeedingUp = useBlending;
}

CTaskComplexFollowPointRoute::CTaskComplexFollowPointRoute(const CTaskComplexFollowPointRoute &other)
	: CTaskComplexFollowPointRoute(other.m_moveState, *other.m_route, other.m_mode, other.m_targetRadius,
		other.m_slowDownDistance, other.m_bMustOvershootTarget, other.m_bUseBlending, other.m_bStandStillAfterMove)
{
}

CTaskComplexFollowPointRoute::~CTaskComplexFollowPointRoute()
{
	delete m_route;
}

eTaskType
CTaskComplexFollowPointRoute::GetSubTaskType()
{
	if (m_route->IsEmpty())
		return TASK_FINISHED;

	if ((int32)m_currPointIdx + 1 < m_route->m_NumEntries) // current is not the last
		return TASK_SIMPLE_GO_TO_POINT;

	if ((int32)m_currPointIdx + 1 == m_route->m_NumEntries) // current will be the last point
		return m_bStandStillAfterMove ? TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL : TASK_SIMPLE_GO_TO_POINT;

	if ((int32)m_currPointIdx == m_route->m_NumEntries) { // current is the last point
		m_nRouteTraversals++;
		switch (m_mode) {
		case Mode::ONE_WAY:
			if (m_bUseBlending)
				m_bWillSlowDown = m_bSlowingDown = m_bSpeedingUp = false;
			return TASK_FINISHED;
		case Mode::RETURN:
			if (m_nRouteTraversals != 1)
				return TASK_FINISHED;
			// fall through
		case Mode::BACK_AND_FORTH:
			m_route->Reverse();
			// fall through
		case Mode::LOOP:
			m_currPointIdx = 0;
			return GetSubTaskType(); // recurse
		}
	}

	return TASK_NONE;
}

CTask *
CTaskComplexFollowPointRoute::CreateTask(eTaskType taskType, CPed *ped)
{
	switch (taskType) {
	case TASK_SIMPLE_GO_TO_POINT_FINE: {
		CTaskSimpleGoToPointFine *task = new CTaskSimpleGoToPointFine(
			CTaskSimpleGoToPointFine::BaseRatio(m_moveState), (*m_route)[(int32)m_currPointIdx]);
		if (m_bSlowingDown)
			task->gotoFlags.m_targetCircledFlags = 0;
		return task;
	}
	case TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL: {
		float slowDownDist;
		switch (m_mode) {
		case Mode::RETURN:
			slowDownDist = m_nRouteTraversals ? m_slowDownDistance : 0.0f;
			break;
		case Mode::LOOP:
		case Mode::BACK_AND_FORTH:
			slowDownDist = 0.0f;
			break;
		default:
			slowDownDist = m_slowDownDistance;
			break;
		}
		// The walk-round-car task does not exist in this codebase, so the
		// going-for-door flag it would supply is never set here.
		return new CTaskComplexGoToPointAndStandStill(m_moveState, (*m_route)[(int32)m_currPointIdx],
			m_targetRadius, slowDownDist, false, false);
	}
	case TASK_SIMPLE_GO_TO_POINT:
		return new CTaskSimpleGoToPoint(m_moveState, (*m_route)[(int32)m_currPointIdx], 0.5f, false, m_bMustOvershootTarget);
	case TASK_NONE:
		// No leave-group task exists yet; an empty route simply ends the
		// follow with a momentary stand-still.
		return new CTaskSimpleStandStill(1);
	case TASK_SIMPLE_STAND_STILL:
		return new CTaskSimpleStandStill(STAND_STILL_TIME);
	case TASK_FINISHED:
		return nil;
	default:
		return nil;
	}
}

CVector
CTaskComplexFollowPointRoute::GetLastWaypoint(CPed *ped)
{
	if (m_route->m_NumEntries == 1)
		return (*m_route)[0];

	if (m_currPointIdx != 0)
		return (*m_route)[(int32)m_currPointIdx - 1];

	// No point visited yet on this pass: the previous pass ended at the
	// last point (unlikely, but just in case).
	return m_nRouteTraversals ? (*m_route)[m_route->m_NumEntries - 1] : ped->GetPosition();
}

CVector
CTaskComplexFollowPointRoute::GetNextWaypoint(CPed *ped)
{
	if (m_route->m_NumEntries == 1)
		return (*m_route)[0];

	const int32 next = (int32)m_currPointIdx + 1;
	switch (m_mode) {
	case Mode::ONE_WAY:
	case Mode::RETURN:
	case Mode::BACK_AND_FORTH:
		return (*m_route)[next < m_route->m_NumEntries ? next : (int32)m_currPointIdx - 1];
	case Mode::LOOP:
		return (*m_route)[next == m_route->m_NumEntries ? 0 : next];
	default:
		return CVector(0.0f, 0.0f, 0.0f);
	}
}

eTaskType
CTaskComplexFollowPointRoute::CalcGoToTaskType(CPed *ped, eTaskType subTaskType)
{
	m_bUseBlending = false;

	switch (m_moveState) {
	case PEDMOVE_JOG:
	case PEDMOVE_RUN:
	case PEDMOVE_SPRINT: {
		if (m_currPointIdx == 0)
			return subTaskType;

		const int32 currPt = Min((int32)m_currPointIdx, m_route->m_NumEntries - 1);
		const CVector &currPtPos = (*m_route)[currPt];
		const float currToNextDotToPrev = DotProduct2D(
			Normalized2D(CVector2D(currPtPos - GetLastWaypoint(ped))),
			Normalized2D(CVector2D(GetNextWaypoint(ped) - currPtPos)));

		const float MAX_ANGLE_IN_COS = 0.70710678f; // cos(45 deg)
		if (currToNextDotToPrev >= MAX_ANGLE_IN_COS) {
			m_bUseBlending = false;
		} else {
			const float angleProgress = ClampFloat(1.0f - (currToNextDotToPrev + 1.0f) / (MAX_ANGLE_IN_COS + 1.0f), 0.0f, 1.0f);
			if (m_moveState == PEDMOVE_SPRINT) {
				m_speedDecreaseAmt = 3.0f * angleProgress;
				m_speedDecreaseDist = 5.0f;
				m_speedIncreaseDist = 5.0f;
			} else {
				m_speedDecreaseAmt = 1.5f * angleProgress;
				m_speedDecreaseDist = 4.0f;
				m_speedIncreaseDist = 4.0f;
			}
		}
		m_bWillSlowDown = true;
		break;
	}
	default:
		break;
	}

	if (m_bWillSlowDown) {
		switch (subTaskType) {
		case TASK_SIMPLE_GO_TO_POINT:
		case TASK_COMPLEX_GO_TO_POINT_AND_STAND_STILL:
			m_bSlowingDown = false;
			m_bSpeedingUp = true;
			return TASK_SIMPLE_GO_TO_POINT_FINE;
		default:
			break;
		}
	}
	m_bSpeedingUp = m_bSlowingDown = false;
	return subTaskType;
}

float
CTaskComplexFollowPointRoute::CalcBlendRatio(CPed *ped, bool slowing)
{
	struct Calc {
		static float Run(CTaskComplexFollowPointRoute *self, CPed *ped, float requiredDist, const CVector &relativeToPoint, float amount)
		{
			const float currDistSq = (ped->GetPosition() - relativeToPoint).MagnitudeSqr();
			if (currDistSq >= requiredDist * requiredDist)
				return -1.0f;
			const float distProg = ClampFloat(currDistSq / (requiredDist * requiredDist), 0.0f, 1.0f);
			const float mult = Cos(distProg * PI) / 2.0f + 0.5f;
			return Max(CTaskSimpleGoToPointFine::BaseRatio(self->m_moveState) - amount * mult,
				CTaskSimpleGoToPointFine::BaseRatio(PEDMOVE_WALK));
		}
	};
	if (slowing)
		return Calc::Run(this, ped, m_speedDecreaseDist, (*m_route)[(int32)m_currPointIdx], m_speedDecreaseAmt);
	return Calc::Run(this, ped, m_speedIncreaseDist, GetLastWaypoint(ped), m_speedIncreaseAmt);
}

bool
CTaskComplexFollowPointRoute::MakeAbortable(CPed *ped, eAbortPriority priority, const CEvent *event)
{
	// The reference also widens the current go-to radius when a slow
	// vehicle blocks the point; that needs the vehicle-carrying event
	// classes, which this codebase's event system does not have yet.

	if (priority == ABORT_PRIORITY_LEISURE)
		m_route->Clear();

	return m_pSubTask->MakeAbortable(ped, priority, event);
}

CTask *
CTaskComplexFollowPointRoute::CreateNextSubTask(CPed *ped)
{
	switch (m_pSubTask->GetTaskType()) {
	case TASK_FINISHED:
		return nil;
	case TASK_NONE:
	case TASK_SIMPLE_STAND_STILL:
	case TASK_SIMPLE_GO_TO_POINT:
		if ((int32)m_currPointIdx + 1 == m_route->m_NumEntries && !m_bStandStillAfterMove)
			return CreateTask(TASK_FINISHED, ped);
		break;
	default:
		break;
	}

	// Otherwise advance to the next point.
	m_currPointIdx++;

	const eTaskType nextSubTask = GetSubTaskType();
	return CreateTask(m_bUseBlending ? CalcGoToTaskType(ped, nextSubTask) : nextSubTask, ped);
}

CTask *
CTaskComplexFollowPointRoute::CreateFirstSubTask(CPed *ped)
{
	m_bNewRoute = false;

	if (m_bUseBlending)
		m_speedDecreaseDist = m_speedIncreaseDist = m_speedDecreaseAmt = m_speedIncreaseAmt = 0.0f;

	if (m_route->IsEmpty())
		return CreateTask(TASK_NONE, ped);

	if (ped->bInVehicle) {
		// The reference first leaves the car here; the leave-car task is
		// written with the rest of the vehicle exit work, so until then
		// this route cannot start from inside a vehicle.
		return CreateTask(TASK_NONE, ped);
	}

	// Find the next point to follow.
	if (m_mode != Mode::ONE_WAY) {
		int32 closestNextPtIdx = -1;

		const CVector &pedPos = ped->GetPosition();
		const int32 npts = m_route->m_NumEntries;
		{
			float closestNextPtDistSq = FLT_MAX;
			for (int32 i = 0; i < npts; i++) {
				const int32 nextPtIdx = (i + 1) % npts;

				const CVector &thisPtPos = (*m_route)[i];
				const CVector thisToNextPt = (*m_route)[nextPtIdx] - thisPtPos;
				CVector thisToNextPtDir(thisToNextPt);
				thisToNextPtDir.Normalise();
				const float dot = DotProduct(pedPos - thisPtPos, thisToNextPtDir);

				if (dot <= 0.0f)
					continue; // ped is behind this point relative to the next

				if (thisToNextPt.MagnitudeSqr() <= dot * dot)
					continue;

				const CVector proj = thisPtPos + thisToNextPtDir * dot;
				const float dist = (pedPos - proj).MagnitudeSqr();
				if (dist < closestNextPtDistSq * closestNextPtDistSq) {
					closestNextPtDistSq = dist;
					closestNextPtIdx = nextPtIdx;
				}
			}
		}

		// If the loop above found nothing, take the nearest point.
		if (closestNextPtIdx == -1) {
			float closestDistSq = FLT_MAX;
			for (int32 i = 0; i < npts; i++) {
				const float dist = (pedPos - (*m_route)[i]).MagnitudeSqr();
				if (dist < closestDistSq * closestDistSq) {
					closestDistSq = dist;
					closestNextPtIdx = i;
				}
			}
		}

		m_currPointIdx = (uint32)closestNextPtIdx;
	}

	const eTaskType nextSubTask = GetSubTaskType();
	return CreateTask(m_bUseBlending ? CalcGoToTaskType(ped, nextSubTask) : nextSubTask, ped);
}

CTask *
CTaskComplexFollowPointRoute::ControlSubTask(CPed *ped)
{
	const eTaskType subTaskType = m_pSubTask->GetTaskType();

	if (ped->GetIntelligence()->m_AnotherStaticCounter > 30 || m_route->IsEmpty())
		return nil;

	if (m_bUseBlending && subTaskType == TASK_NONE) {
		m_bWillSlowDown = m_bSlowingDown = m_bSpeedingUp = false;
		return m_pSubTask;
	}

	if (m_bUseBlending) {
		CTaskSimpleGoToPointFine *gotoPointFine = (CTaskSimpleGoToPointFine *)m_pSubTask;

		if (subTaskType == TASK_SIMPLE_GO_TO_POINT_FINE) {
			if (m_bSlowingDown) {
				const float ratio = CalcBlendRatio(ped, true);
				gotoPointFine->SetMoveRatio(ratio >= 0.0f ? ratio : CTaskSimpleGoToPointFine::BaseRatio(m_moveState));
			} else if (m_bSpeedingUp) {
				const float moveRatio = CalcBlendRatio(ped, true);
				if (moveRatio < 0.0f) {
					m_bSpeedingUp = false;
					gotoPointFine->SetMoveRatio(CTaskSimpleGoToPointFine::BaseRatio(m_moveState));
					return CreateTask(TASK_SIMPLE_GO_TO_POINT, ped);
				}
			}
		}

		if (m_bWillSlowDown && !m_bSlowingDown) {
			const float moveRatio = CalcBlendRatio(ped, true);
			if (moveRatio >= 0.0f) {
				m_bWillSlowDown = m_bSlowingDown = false;
				if (subTaskType == TASK_SIMPLE_GO_TO_POINT_FINE) {
					gotoPointFine->SetMoveRatio(moveRatio);
				} else {
					CTaskSimpleGoToPointFine *task = (CTaskSimpleGoToPointFine *)CreateTask(TASK_SIMPLE_GO_TO_POINT_FINE, ped);
					task->SetMoveRatio(moveRatio);
					return task;
				}
			}
		}
	}
	return m_pSubTask;
}
