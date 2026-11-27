#include "common.h"

#include "main.h"
#include "Particle.h"
#include "RpAnimBlend.h"
#include "Ped.h"
#include "PlayerPed.h"
#include "Wanted.h"
#include "AnimBlendAssociation.h"
#include "AnimBlendStaticAssociation.h"
#include "AnimBlendHierarchy.h"
#include "AnimBlendClumpData.h"
#include "eBoneTag.h"
#include "PedNode.h"
#include "DMAudio.h"
#include "General.h"
#include "cHandlingDataMgr.h"
#include "Replay.h"
#include "Camera.h"
#include "PedPlacement.h"
#include "ZoneCull.h"
#include "Pad.h"
#include "Pickups.h"
#include "Train.h"
#include "PedRoutes.h"
#include "CopPed.h"
#include "Script.h"
#include "CarCtrl.h"
#include "WaterLevel.h"
#include "CarAI.h"
#include "Zones.h"
#include "Cranes.h"
#include "PedAttractor.h"
#include "Bike.h"
#include "Weather.h"
#include "GameLogic.h"
#include "Streaming.h"
#include "CarEnterExit.h"
#include "PedIntelligence.h"
#include "TaskComplexEnterCarAsDriver.h"
#include "TaskComplexEnterTrainAsPassenger.h"
#include "TaskComplexGoToCarDoorAndStandStill.h"
#include "TaskComplexEnterCarAsPassenger.h"
#include "TaskComplexLeaveCar.h"
#include "VehicleAnimGroupData.h"

// The animation a vehicle's occupant plays, resolved the way SA resolves it.
//
// The model names one of the 30 vehicle animation groups; that group holds two
// candidate AssocGroupIds and eighteen flags that decide, per animation, which
// of the two is used. So the answer is not "the model's group" - it is
// CVehicleAnimGroupData::GetGroupForAnim(model's group, this specific clip).
// A model whose group is not in the table falls back to the standard car set.
static AssocGroupId
GetVehicleAnimGroup(CVehicle *veh, AnimationId animId)
{
	if(veh == nil)
		return ANIM_GROUP_STDCARAMIMS;
	// The index into the 30-group table lives in the handling record, not in
	// the model: handling.cfg names the group on the vehicle's own line, so a
	// car and a van built on the same model animate from different banks.
	const int32 groupId = veh->GetAnimGroupId();
	if(groupId >= CVehicleAnimGroupData::NUM_VEH_ANIM_GROUPS)
		return ANIM_GROUP_STDCARAMIMS;
	return CVehicleAnimGroupData::GetGroupForAnim((AssocGroupId)groupId, animId);
}

// A bike carries two animation groups that are not interchangeable. The
// riding clips (ride, lean, drive-by) live in the IFP block group
// CBike::m_bikeAnimType names (ANIM_GROUP_BIKES..ANIM_GROUP_QUAD), while the
// get-in, get-out and jack clips live in the per-style groups
// (ANIM_GROUP_STDBIKEANIMS..ANIM_GROUP_QUADBIKEANIMS). SA picks the entry
// group from the handling record exactly as it does for cars - handling
// groups 3..12 of the 30-group table are the bike styles - so resolve it
// the same way cars do. Only when the handling data names no bike group
// (Vice City handling.cfg has none) fall back to the style group bound to
// the same IFP block the riding clips come from. Asking a riding group for
// a get-in clip can never succeed: those tables hold no such slot, and the
// rider was left standing next to the bike (debug-14).
static AssocGroupId
GetBikeEnterAnimGroup(CVehicle *veh, AnimationId animId)
{
	AssocGroupId group = GetVehicleAnimGroup(veh, animId);
	if(group >= ANIM_GROUP_STDBIKEANIMS && group <= ANIM_GROUP_QUADBIKEANIMS)
		return group;
	switch(((CBike*)veh)->m_bikeAnimType){
	case ANIM_GROUP_BIKEV:    return ANIM_GROUP_VESPABIKEANIMS;
	case ANIM_GROUP_BIKEH:    return ANIM_GROUP_HARLEYBIKEANIMS;
	case ANIM_GROUP_BIKED:    return ANIM_GROUP_DIRTBIKEANIMS;
	case ANIM_GROUP_WAYFARER: return ANIM_GROUP_WAYFBIKEANIMS;
	case ANIM_GROUP_BMX:      return ANIM_GROUP_BMXBIKEANIMS;
	case ANIM_GROUP_MTB:      return ANIM_GROUP_MTBBIKEANIMS;
	case ANIM_GROUP_CHOPPA:   return ANIM_GROUP_CHOPPABIKEANIMS;
	case ANIM_GROUP_QUAD:     return ANIM_GROUP_QUADBIKEANIMS;
	default:                  return ANIM_GROUP_SPORTBIKEANIMS; // ANIM_GROUP_BIKES block
	}
}

CVector vecPedCarDoorAnimOffset;
CVector vecPedCarDoorLoAnimOffset;
CVector vecPedVanRearDoorAnimOffset;
CVector vecPedQuickDraggedOutCarAnimOffset;
CVector vecPedDraggedOutCarAnimOffset;
CVector vecPedTrainDoorAnimOffset;
CVector vecPedStdBikeJumpRhsAnimOffset;
CVector vecPedVespaBikeJumpRhsAnimOffset;
CVector vecPedHarleyBikeJumpRhsAnimOffset;
CVector vecPedDirtBikeJumpRhsAnimOffset;
CVector vecPedBikeKickAnimOffset;

static CVector GetAnimationLastTranslation(AssocGroupId groupId, AnimationId animId);
static CVector GetPedAnimationOffset(CPed *ped, AssocGroupId groupId, AnimationId animId, const CVector &nativeOffset);

void
CPed::SetObjectiveTimer(int time)
{
	if (time == 0) {
		m_objectiveTimer = 0;
	} else if (CTimer::GetTimeInMilliseconds() > m_objectiveTimer) {
		m_objectiveTimer = CTimer::GetTimeInMilliseconds() + time;
	}
}

void
CPed::SetStoredObjective(void)
{
	if (m_objective == m_prevObjective)
		return;

	switch (m_objective) {
		case OBJECTIVE_FLEE_ON_FOOT_TILL_SAFE:
		case OBJECTIVE_KILL_CHAR_ON_FOOT:
		case OBJECTIVE_KILL_CHAR_ANY_MEANS:
		case OBJECTIVE_FLEE_CHAR_ON_FOOT_TILL_SAFE:
		case OBJECTIVE_FLEE_CHAR_ON_FOOT_ALWAYS:
		case OBJECTIVE_GOTO_CHAR_ON_FOOT:
		case OBJECTIVE_GOTO_CHAR_ON_FOOT_WALKING:
		case OBJECTIVE_HASSLE_CHAR:
		case OBJECTIVE_FOLLOW_CHAR_IN_FORMATION:
		case OBJECTIVE_LEAVE_CAR:
		case OBJECTIVE_ENTER_CAR_AS_PASSENGER:
		case OBJECTIVE_ENTER_CAR_AS_DRIVER:
		case OBJECTIVE_GOTO_AREA_ON_FOOT:
		case OBJECTIVE_RUN_TO_AREA:
		case OBJECTIVE_GOTO_SEAT_ON_FOOT:
		case OBJECTIVE_GOTO_ATM_ON_FOOT:
		case OBJECTIVE_GOTO_BUS_STOP_ON_FOOT:
		case OBJECTIVE_GOTO_PIZZA_ON_FOOT:
		case OBJECTIVE_GOTO_SHELTER_ON_FOOT:
		case OBJECTIVE_SPRINT_TO_AREA:
		case OBJECTIVE_GOTO_ICE_CREAM_VAN_ON_FOOT:
			return;
		default:
			m_prevObjective = m_objective;
	}
}

void
CPed::ForceStoredObjective(eObjective objective)
{
	if (objective != OBJECTIVE_ENTER_CAR_AS_DRIVER && objective != OBJECTIVE_ENTER_CAR_AS_PASSENGER) {
		m_prevObjective = m_objective;
		return;
	}

	switch (m_objective) {
		case OBJECTIVE_FLEE_ON_FOOT_TILL_SAFE:
		case OBJECTIVE_KILL_CHAR_ON_FOOT:
		case OBJECTIVE_FLEE_CHAR_ON_FOOT_TILL_SAFE:
		case OBJECTIVE_FLEE_CHAR_ON_FOOT_ALWAYS:
		case OBJECTIVE_GOTO_CHAR_ON_FOOT:
		case OBJECTIVE_GOTO_CHAR_ON_FOOT_WALKING:
		case OBJECTIVE_HASSLE_CHAR:
		case OBJECTIVE_ENTER_CAR_AS_PASSENGER:
		case OBJECTIVE_ENTER_CAR_AS_DRIVER:
		case OBJECTIVE_GOTO_AREA_ON_FOOT:
		case OBJECTIVE_RUN_TO_AREA:
		case OBJECTIVE_GOTO_SEAT_ON_FOOT:
		case OBJECTIVE_GOTO_ATM_ON_FOOT:
		case OBJECTIVE_GOTO_BUS_STOP_ON_FOOT:
		case OBJECTIVE_GOTO_PIZZA_ON_FOOT:
		case OBJECTIVE_GOTO_SHELTER_ON_FOOT:
		case OBJECTIVE_SPRINT_TO_AREA:
		case OBJECTIVE_GOTO_ICE_CREAM_VAN_ON_FOOT:
			return;
		default:
			m_prevObjective = m_objective;
	}
}

bool
CPed::IsTemporaryObjective(eObjective objective)
{
	return objective == OBJECTIVE_LEAVE_CAR || objective == OBJECTIVE_SET_LEADER ||
		objective == OBJECTIVE_LEAVE_CAR_AND_DIE || objective == OBJECTIVE_ENTER_CAR_AS_DRIVER ||
		objective == OBJECTIVE_ENTER_CAR_AS_PASSENGER;
}

void
CPed::SetObjective(eObjective newObj)
{
	if (DyingOrDead() || m_attachedTo)
		return;

	if (newObj == OBJECTIVE_NONE) {
		if ((m_objective == OBJECTIVE_LEAVE_CAR || m_objective == OBJECTIVE_ENTER_CAR_AS_DRIVER || m_objective == OBJECTIVE_ENTER_CAR_AS_PASSENGER
			|| m_objective == OBJECTIVE_LEAVE_CAR_AND_DIE) && !IsPlayer() && !IsPedInControl()) {

			bStartWanderPathOnFoot = true;
		} else {
			m_objective = OBJECTIVE_NONE;
			m_prevObjective = OBJECTIVE_NONE;
		}
	} else if (m_prevObjective != newObj || m_prevObjective == OBJECTIVE_NONE) {
		SetObjectiveTimer(0);

		if (m_objective == newObj)
			return;

		if (IsTemporaryObjective(m_objective)) {
			m_prevObjective = newObj;
		} else {
			if (m_objective != newObj)
				SetStoredObjective();

			m_objective = newObj;
		}
		bObjectiveCompleted = false;

		switch (newObj) {
			case OBJECTIVE_NONE:
				m_prevObjective = OBJECTIVE_NONE;
				break;
			case OBJECTIVE_HAIL_TAXI:
				m_nWaitTimer = 0;
				SetIdle();
				SetMoveState(PEDMOVE_STILL);
				break;
			default:
				break;
		}
	}
}

void
CPed::SetObjective(eObjective newObj, void *entity)
{
	if (DyingOrDead())
		return;

	if (m_prevObjective == newObj && m_prevObjective != OBJECTIVE_NONE)
		return;

	if (entity == this)
		return;

	if (m_attachedTo && newObj != OBJECTIVE_KILL_CHAR_ON_FOOT && newObj != OBJECTIVE_KILL_CHAR_ANY_MEANS && newObj != OBJECTIVE_DESTROY_OBJECT && newObj != OBJECTIVE_DESTROY_CAR)
		return;

	if (m_objective == newObj) {
		switch (newObj) {
			case OBJECTIVE_KILL_CHAR_ON_FOOT:
			case OBJECTIVE_KILL_CHAR_ANY_MEANS:
			case OBJECTIVE_GOTO_CHAR_ON_FOOT:
			case OBJECTIVE_GOTO_CHAR_ON_FOOT_WALKING:
			case OBJECTIVE_HASSLE_CHAR:
			case OBJECTIVE_FOLLOW_CHAR_IN_FORMATION:
			case OBJECTIVE_GOTO_AREA_ANY_MEANS:
			case OBJECTIVE_GUARD_ATTACK:
			case OBJECTIVE_KILL_CHAR_ON_BOAT:
			case OBJECTIVE_SOLICIT_FOOT:
				if (m_pedInObjective == entity)
					return;
				break;
			case OBJECTIVE_LEAVE_CAR:
			case OBJECTIVE_FLEE_CAR:
			case OBJECTIVE_LEAVE_CAR_AND_DIE:
				return;
			case OBJECTIVE_ENTER_CAR_AS_PASSENGER:
			case OBJECTIVE_ENTER_CAR_AS_DRIVER:
			case OBJECTIVE_DESTROY_CAR:
			case OBJECTIVE_SOLICIT_VEHICLE:
			case OBJECTIVE_BUY_ICE_CREAM:
				if (m_carInObjective == entity)
					return;

				if (newObj == OBJECTIVE_BUY_ICE_CREAM && bBoughtIceCream)
					return;

				break;
			case OBJECTIVE_SET_LEADER:
				if (m_leader == entity)
					return;
				break;
			case OBJECTIVE_AIM_GUN_AT:
				if (m_pedInObjective == entity)
					return;
				break;
			default:
				break;
		}
	} else {
		if (newObj != OBJECTIVE_WAIT_IN_CAR_THEN_GET_OUT && (newObj == OBJECTIVE_LEAVE_CAR || newObj == OBJECTIVE_LEAVE_CAR_AND_DIE)
			&& !bInVehicle)
			return;
	}

	bObjectiveCompleted = false;
	ClearPointGunAt();
	m_objectiveTimer = 0;
	if (IsTemporaryObjective(m_objective) && !IsTemporaryObjective(newObj)) {
		m_prevObjective = newObj;
	} else {
		if (m_objective != newObj) {
			if (IsTemporaryObjective(newObj))
				ForceStoredObjective(newObj);
			else
				SetStoredObjective();
		}
		m_objective = newObj;
	}

	switch (newObj) {
		case OBJECTIVE_WAIT_ON_FOOT_FOR_COP:
			m_pedInObjective = (CPed*)entity;
			m_pedInObjective->RegisterReference((CEntity**)&m_pedInObjective);
			SetIdle();
			SetLook(m_pedInObjective);
			break;
		case OBJECTIVE_WAIT_IN_CAR_THEN_GET_OUT:

			// In this special case, entity parameter isn't CEntity, but int.
			SetObjectiveTimer((uintptr)entity);
			break;
		case OBJECTIVE_KILL_CHAR_ON_FOOT:
		case OBJECTIVE_KILL_CHAR_ANY_MEANS:
		case OBJECTIVE_MUG_CHAR:
		case OBJECTIVE_KILL_CHAR_ON_BOAT:
			m_pNextPathNode = nil;
			bUsePedNodeSeek = false;

			if (m_pedInObjective)
				m_pedInObjective->CleanUpOldReference((CEntity**)&m_pedInObjective);
			if (m_pLookTarget)
				m_pLookTarget->CleanUpOldReference(&m_pLookTarget);
			
			m_pLookTarget = (CEntity*)entity;
			m_pedInObjective = (CPed*)entity;
			m_vecSeekPos = CVector(0.0f, 0.0f, 0.0f);
			m_pedInObjective->RegisterReference((CEntity**)&m_pedInObjective);
			// m_pLookTarget = (CEntity*)entity; // duplicate
			m_pLookTarget->RegisterReference((CEntity**)&m_pLookTarget);
			break;
		case OBJECTIVE_FLEE_CHAR_ON_FOOT_TILL_SAFE:
		case OBJECTIVE_FLEE_CHAR_ON_FOOT_ALWAYS:
		case OBJECTIVE_GOTO_CHAR_ON_FOOT:
		case OBJECTIVE_GOTO_CHAR_ON_FOOT_WALKING:
		case OBJECTIVE_HASSLE_CHAR:
		case OBJECTIVE_GUARD_ATTACK:

			if (m_pedInObjective)
				m_pedInObjective->CleanUpOldReference((CEntity**)&m_pedInObjective);
			m_pedInObjective = (CPed*)entity;
			m_vecSeekPos = CVector(0.0f, 0.0f, 0.0f);
			m_pedInObjective->RegisterReference((CEntity**)&m_pedInObjective);
			break;
		case OBJECTIVE_FOLLOW_CHAR_IN_FORMATION:
			m_pedInObjective = (CPed*)entity;
			m_pedInObjective->RegisterReference((CEntity**)&m_pedInObjective);
			m_pedFormation = FORMATION_REAR;
			break;
		case OBJECTIVE_LEAVE_CAR:
		case OBJECTIVE_LEAVE_CAR_AND_DIE:
		case OBJECTIVE_FLEE_CAR:
			m_carInObjective = (CVehicle*)entity;
			m_carInObjective->RegisterReference((CEntity **)&m_carInObjective);
			if (m_carInObjective->bIsBus && m_leaveCarTimer == 0) {
				for (int i = 0; i < m_carInObjective->m_nNumMaxPassengers; i++) {
					if (m_carInObjective->pPassengers[i] == this) {
						m_leaveCarTimer = CTimer::GetTimeInMilliseconds() + 1200 * i;
						break;
					}
				}
			}

			break;
		case OBJECTIVE_DESTROY_OBJECT:
			SetWeaponLockOnTarget((CEntity*)entity);
			break;
		case OBJECTIVE_ENTER_CAR_AS_PASSENGER:
		case OBJECTIVE_ENTER_CAR_AS_DRIVER:
			if (m_nMoveState == PEDMOVE_STILL)
				SetMoveState(PEDMOVE_RUN);

			if (((CVehicle*)entity)->IsBoat() && !IsPlayer() && m_pCurrentPhysSurface != entity) {
				RestorePreviousObjective();
				break;
			}
			// fall through
		case OBJECTIVE_DESTROY_CAR:
		case OBJECTIVE_SOLICIT_VEHICLE:
		case OBJECTIVE_BUY_ICE_CREAM:
			m_carInObjective = (CVehicle*)entity;
			m_carInObjective->RegisterReference((CEntity**)&m_carInObjective);
			m_pSeekTarget = m_carInObjective;
			m_pSeekTarget->RegisterReference((CEntity**)&m_pSeekTarget);
			m_vecSeekPos = CVector(0.0f, 0.0f, 0.0f);
			if (newObj == OBJECTIVE_SOLICIT_VEHICLE) {
				m_objectiveTimer = CTimer::GetTimeInMilliseconds() + 10000;
			} else if (m_objective == OBJECTIVE_ENTER_CAR_AS_PASSENGER && CharCreatedBy == MISSION_CHAR &&
					(m_carInObjective->GetStatus() == STATUS_PLAYER_DISABLED || CPad::GetPad(CWorld::PlayerInFocus)->ArePlayerControlsDisabled())) {
				SetObjectiveTimer(14000);
			} else {
				m_objectiveTimer = 0;
			}
			break;
		case OBJECTIVE_SET_LEADER:
			SetLeader((CEntity*)entity);
			RestorePreviousObjective();
			break;
		case OBJECTIVE_AIM_GUN_AT:
			m_pedInObjective = (CPed*)entity;
			m_pedInObjective->RegisterReference((CEntity**)&m_pedInObjective);
			break;
		case OBJECTIVE_SOLICIT_FOOT:
			m_pedInObjective = (CPed*)entity;
			m_pedInObjective->RegisterReference((CEntity**)&m_pedInObjective);
			break;
		default:
			break;
	}
}

// Only used in 01E1: SET_CHAR_OBJ_FOLLOW_ROUTE opcode
// IDA fails very badly in here, puts a fake loop and ignores SetFollowRoute call...
void
CPed::SetObjective(eObjective newObj, int16 routePoint, int16 routeType)
{
	if (DyingOrDead())
		return;

	if (m_prevObjective == newObj && m_prevObjective != OBJECTIVE_NONE)
		return;

	if (m_objective == newObj && newObj == OBJECTIVE_FOLLOW_ROUTE && m_routeLastPoint == routePoint && m_routeType == routeType)
		return;

	ClearPointGunAt();
	SetObjectiveTimer(0);

	bObjectiveCompleted = false;
	if (IsTemporaryObjective(m_objective)) {
		m_prevObjective = newObj;
	} else {
		if (m_objective != newObj)
			SetStoredObjective();

		m_objective = newObj;
	}

	if (newObj == OBJECTIVE_FOLLOW_ROUTE) {
		SetFollowRoute(routePoint, routeType);
	}
}

void
CPed::SetObjective(eObjective newObj, CVector dest)
{
	if (DyingOrDead())
		return;

	if (m_prevObjective != OBJECTIVE_NONE && m_prevObjective == newObj)
		return;

	if (m_objective == newObj) {
		if (newObj == OBJECTIVE_GOTO_AREA_ANY_MEANS || newObj == OBJECTIVE_GOTO_AREA_ON_FOOT || newObj == OBJECTIVE_RUN_TO_AREA || newObj == OBJECTIVE_SPRINT_TO_AREA) {
			if (m_nextRoutePointPos == dest)
				return;
		} else if (newObj == OBJECTIVE_GUARD_SPOT) {
			if (m_vecSpotToGuard == dest)
				return;
		}
	}

	ClearPointGunAt();
	m_objectiveTimer = 0;
	bObjectiveCompleted = false;
	switch (newObj) {
		case OBJECTIVE_GUARD_SPOT:
			m_vecSpotToGuard = dest;
			m_radiusToGuard = 5.0f;
			SetMoveState(PEDMOVE_STILL);
			break;
		case OBJECTIVE_GUARD_AREA:
		case OBJECTIVE_WAIT_IN_CAR:
		case OBJECTIVE_WAIT_IN_CAR_THEN_GET_OUT:
		case OBJECTIVE_KILL_CHAR_ON_FOOT:
		case OBJECTIVE_KILL_CHAR_ANY_MEANS:
		case OBJECTIVE_FLEE_CHAR_ON_FOOT_TILL_SAFE:
		case OBJECTIVE_FLEE_CHAR_ON_FOOT_ALWAYS:
		case OBJECTIVE_GOTO_CHAR_ON_FOOT:
		case OBJECTIVE_GOTO_CHAR_ON_FOOT_WALKING:
		case OBJECTIVE_HASSLE_CHAR:
		case OBJECTIVE_FOLLOW_CHAR_IN_FORMATION:
		case OBJECTIVE_LEAVE_CAR:
		case OBJECTIVE_ENTER_CAR_AS_PASSENGER:
		case OBJECTIVE_ENTER_CAR_AS_DRIVER:
		case OBJECTIVE_FOLLOW_CAR_IN_CAR:
		case OBJECTIVE_FIRE_AT_OBJECT_FROM_VEHICLE:
		case OBJECTIVE_DESTROY_OBJECT:
		case OBJECTIVE_DESTROY_CAR:
		case OBJECTIVE_GOTO_AREA_IN_CAR:
		case OBJECTIVE_FOLLOW_CAR_ON_FOOT_WITH_OFFSET:
		case OBJECTIVE_GUARD_ATTACK:
		case OBJECTIVE_SET_LEADER:
		case OBJECTIVE_FOLLOW_ROUTE:
		case OBJECTIVE_SOLICIT_VEHICLE:
		case OBJECTIVE_HAIL_TAXI:
		case OBJECTIVE_CATCH_TRAIN:
		case OBJECTIVE_BUY_ICE_CREAM:
		case OBJECTIVE_STEAL_ANY_CAR:
		case OBJECTIVE_STEAL_ANY_MISSION_CAR:
		case OBJECTIVE_MUG_CHAR:
		case OBJECTIVE_LEAVE_CAR_AND_DIE:
		case OBJECTIVE_FLEE_CAR:
		case OBJECTIVE_SUN_BATHE:
		case OBJECTIVE_AIM_GUN_AT:
		case OBJECTIVE_WANDER:
		case OBJECTIVE_WAIT_ON_FOOT_AT_SHELTER:
		case OBJECTIVE_KILL_CHAR_ON_BOAT:
		case OBJECTIVE_SOLICIT_FOOT:
		case OBJECTIVE_WAIT_ON_FOOT_AT_BUS_STOP:
			break;
		case OBJECTIVE_GOTO_AREA_ANY_MEANS:
		case OBJECTIVE_GOTO_AREA_ON_FOOT:
		case OBJECTIVE_GOTO_SEAT_ON_FOOT:
		case OBJECTIVE_GOTO_ATM_ON_FOOT:
		case OBJECTIVE_GOTO_BUS_STOP_ON_FOOT:
		case OBJECTIVE_GOTO_PIZZA_ON_FOOT:
		case OBJECTIVE_GOTO_SHELTER_ON_FOOT:
		case OBJECTIVE_GOTO_ICE_CREAM_VAN_ON_FOOT:
			bIsRunning = false;
			m_pNextPathNode = nil;
			m_nextRoutePointPos = dest;
			m_vecSeekPos = m_nextRoutePointPos;
			m_distanceToCountSeekDone = 0.5f;
			if (newObj == OBJECTIVE_GOTO_ATM_ON_FOOT) {
				m_distanceToCountSeekDone = m_attractor->GetDistanceToCountSeekDone();
				m_acceptableHeadingOffset = m_attractor->GetAcceptableHeading();
			}
			if (newObj == OBJECTIVE_GOTO_SEAT_ON_FOOT) {
				m_distanceToCountSeekDone = m_attractor->GetDistanceToCountSeekDone();
				m_acceptableHeadingOffset = m_attractor->GetAcceptableHeading();
			}
			if (newObj == OBJECTIVE_GOTO_BUS_STOP_ON_FOOT) {
				m_distanceToCountSeekDone = m_attractor->GetDistanceToCountSeekDone();
				m_acceptableHeadingOffset = m_attractor->GetAcceptableHeading();
			}
			if (newObj == OBJECTIVE_GOTO_PIZZA_ON_FOOT) {
				m_distanceToCountSeekDone = m_attractor->GetDistanceToCountSeekDone();
				m_acceptableHeadingOffset = m_attractor->GetAcceptableHeading();
			}
			if (newObj == OBJECTIVE_GOTO_SHELTER_ON_FOOT) {
				bIsRunning = true;
				m_distanceToCountSeekDone = m_attractor->GetDistanceToCountSeekDone();
				m_acceptableHeadingOffset = m_attractor->GetAcceptableHeading();
			}
			if (newObj == OBJECTIVE_GOTO_ICE_CREAM_VAN_ON_FOOT) {
				bIsRunning = true;
				m_distanceToCountSeekDone = m_attractor->GetDistanceToCountSeekDone();
				m_acceptableHeadingOffset = m_attractor->GetAcceptableHeading();
			}
			bUsePedNodeSeek = false;
			if (sq(m_distanceToCountSeekDone) > (m_nextRoutePointPos - GetPosition()).MagnitudeSqr2D()) {
				if (!IsUseAttractorObjective(m_objective))
					return;
				if (Abs(m_fRotationCur - m_attractorHeading) < m_acceptableHeadingOffset)
					return;
			}
			break;
		case OBJECTIVE_RUN_TO_AREA:
		case OBJECTIVE_SPRINT_TO_AREA:
			bIsRunning = true;
			m_pNextPathNode = nil;
			m_nextRoutePointPos = dest;
			m_vecSeekPos = m_nextRoutePointPos;
			m_distanceToCountSeekDone = 0.5f;
			bUsePedNodeSeek = true;
			if (sq(m_distanceToCountSeekDone) > (m_nextRoutePointPos - GetPosition()).MagnitudeSqr2D())
				return;
			break;
		default: break;
	}

	if (IsTemporaryObjective(m_objective)) {
		m_prevObjective = newObj;
	} else {
		if (m_objective != newObj)
			SetStoredObjective();

		m_objective = newObj;
	}
}

void
CPed::SetObjective(eObjective newObj, float heading, const CVector& pos)
{
	switch (newObj) {
	case OBJECTIVE_GOTO_SEAT_ON_FOOT:
	case OBJECTIVE_GOTO_ATM_ON_FOOT:
	case OBJECTIVE_GOTO_BUS_STOP_ON_FOOT:
	case OBJECTIVE_GOTO_PIZZA_ON_FOOT:
	case OBJECTIVE_GOTO_SHELTER_ON_FOOT:
	case OBJECTIVE_GOTO_ICE_CREAM_VAN_ON_FOOT:
		ClearPointGunAt();
		SetObjective(newObj, pos);
		m_attractorHeading = heading;
	}
}

void
CPed::ClearObjective(void)
{
	if (IsPedInControl() || m_nPedState == PED_DRIVING) {
		m_objective = OBJECTIVE_NONE;
		m_pedInObjective = nil;
		m_carInObjective = nil;

		if (m_nPedState == PED_DRIVING && m_pMyVehicle) {
			if (m_pMyVehicle->pDriver != this) {
				if(!IsPlayer())
					bWanderPathAfterExitingCar = true;

				SetObjective(OBJECTIVE_LEAVE_CAR, m_pMyVehicle);
			}
			m_nLastPedState = PED_NONE;
		} else {
			SetIdle();
			SetMoveState(PEDMOVE_STILL);
		}
	} else {
		bClearObjective = true;
	}
}

void
CPed::ClearLeader(void)
{
	if (!m_leader)
		return;

	m_leader = nil;
	if (IsPedInControl()) {
		SetObjective(OBJECTIVE_NONE);
		if (CharCreatedBy == MISSION_CHAR) {
			SetIdle();
		} else {
			SetWanderPath(CGeneral::GetRandomNumberInRange(0,8));
		}
	} else if (m_objective != OBJECTIVE_NONE) {
		bClearObjective = true;
	}
}

void
CPed::UpdateFromLeader(void)
{
	if (CTimer::GetTimeInMilliseconds() <= m_objectiveTimer)
		return;

	if (!m_leader)
		return;

	CVector leaderDist;
	if (m_leader->InVehicle())
		leaderDist = m_leader->m_pMyVehicle->GetPosition() - GetPosition();
	else
		leaderDist = m_leader->GetPosition() - GetPosition();

	if (leaderDist.Magnitude() > 30.0f) {
		if (bWaitForLeaderToComeCloser) {
			if (IsPedInControl()) {
				SetObjective(OBJECTIVE_NONE);
				SetIdle();
				SetMoveState(PEDMOVE_STILL);
			}
			return;
		}
		bWaitForLeaderToComeCloser = true;
	} else
		bWaitForLeaderToComeCloser = false;

	if (IsPedInControl()) {
		if (m_nWaitState == WAITSTATE_PLAYANIM_TAXI)
			WarpPedToNearLeaderOffScreen();

		if (m_leader->m_nPedState == PED_DEAD) {
			SetLeader(nil);
			SetObjective(OBJECTIVE_FLEE_ON_FOOT_TILL_SAFE);
			return;
		}
		if (!m_leader->bInVehicle) {
			if (m_leader->m_objective != OBJECTIVE_ENTER_CAR_AS_DRIVER) {
				if (bInVehicle) {
					if (m_objective != OBJECTIVE_WAIT_IN_CAR_THEN_GET_OUT && m_objective != OBJECTIVE_LEAVE_CAR)
						SetObjective(OBJECTIVE_LEAVE_CAR, m_pMyVehicle);

					return;
				}
				if (m_objective == OBJECTIVE_ENTER_CAR_AS_PASSENGER) {
					RestorePreviousObjective();
					RestorePreviousState();
				}
			}
			if (m_nPedType == PEDTYPE_PROSTITUTE && CharCreatedBy == RANDOM_CHAR) {
				SetLeader(nil);
				return;
			}
		}
		if (!bInVehicle && m_leader->bInVehicle && m_leader->m_nPedState == PED_DRIVING) {
			if (m_objective != OBJECTIVE_ENTER_CAR_AS_PASSENGER && m_objective != OBJECTIVE_ENTER_CAR_AS_DRIVER) {
				if (m_leader->m_pMyVehicle->m_nNumPassengers < m_leader->m_pMyVehicle->m_nNumMaxPassengers)
					SetObjective(OBJECTIVE_ENTER_CAR_AS_PASSENGER, m_leader->m_pMyVehicle);
			}
		} else if (m_leader->m_objective == OBJECTIVE_NONE || (m_leader->IsPlayer() && m_leader->m_objective == OBJECTIVE_WAIT_ON_FOOT)
			|| m_objective == m_leader->m_objective) {

			if (m_leader->m_nPedState == PED_ATTACK && !bDontFight) {
				CEntity *lookTargetOfLeader = m_leader->m_pLookTarget;

				if (lookTargetOfLeader && m_objective != OBJECTIVE_KILL_CHAR_ON_FOOT && lookTargetOfLeader->IsPed() && lookTargetOfLeader != this) {
					SetObjective(OBJECTIVE_KILL_CHAR_ON_FOOT, lookTargetOfLeader);
					SetObjectiveTimer(8000);
					SetLookFlag(m_leader->m_pLookTarget, false);
					SetLookTimer(500);
				}
			} else {
				if (IsPedInControl() && m_nPedState != PED_ATTACK) {
					if (m_leader->m_objective == OBJECTIVE_NONE && m_objective == OBJECTIVE_NONE && m_leader->m_nPedState == PED_CHAT && m_nPedState == PED_CHAT) {
						SetObjective(OBJECTIVE_NONE);
					} else {
						SetObjective(OBJECTIVE_GOTO_CHAR_ON_FOOT, m_leader);
						SetObjectiveTimer(0);
					}
				}
				if (m_nPedState == PED_IDLE && m_leader->IsPlayer() && !bDontFight) {
					if (ScanForThreats() && m_threatEntity) {
						m_pLookTarget = m_threatEntity;
						m_pLookTarget->RegisterReference((CEntity **) &m_pLookTarget);
						TurnBody();
						if (m_attackTimer < CTimer::GetTimeInMilliseconds() && !GetWeapon()->IsTypeMelee()) {
							SetWeaponLockOnTarget(m_threatEntity);
							SetAttack(m_threatEntity);
						}
					}
				}
			}
		} else {
			switch (m_leader->m_objective) {
				case OBJECTIVE_WAIT_ON_FOOT:
				case OBJECTIVE_FLEE_ON_FOOT_TILL_SAFE:
				case OBJECTIVE_WAIT_IN_CAR:
				case OBJECTIVE_FOLLOW_ROUTE:
					SetObjective(m_leader->m_objective);
					m_objectiveTimer = m_leader->m_objectiveTimer;
					break;
				case OBJECTIVE_GUARD_SPOT:
					SetObjective(OBJECTIVE_GUARD_SPOT, m_leader->m_vecSpotToGuard);
					m_objectiveTimer = m_leader->m_objectiveTimer;
					break;
				case OBJECTIVE_KILL_CHAR_ON_FOOT:
				case OBJECTIVE_KILL_CHAR_ANY_MEANS:
				case OBJECTIVE_GOTO_CHAR_ON_FOOT:
				case OBJECTIVE_GOTO_CHAR_ON_FOOT_WALKING:
				case OBJECTIVE_HASSLE_CHAR:
					if (m_leader->m_pedInObjective) {
						SetObjective(m_leader->m_objective, m_leader->m_pedInObjective);
						m_objectiveTimer = m_leader->m_objectiveTimer;
					}
					break;
				case OBJECTIVE_ENTER_CAR_AS_PASSENGER:
				case OBJECTIVE_ENTER_CAR_AS_DRIVER:
					if (m_leader->m_carInObjective) {
						m_leaveCarTimer = CTimer::GetTimeInMilliseconds() + 150;
						SetObjective(OBJECTIVE_ENTER_CAR_AS_PASSENGER, m_leader->m_carInObjective);
						return;
					}
					break;
				case OBJECTIVE_GUARD_ATTACK:
					return;
				case OBJECTIVE_HAIL_TAXI:
					m_leader = nil;
					SetObjective(OBJECTIVE_NONE);
					break;
				default:
					SetObjective(OBJECTIVE_GOTO_CHAR_ON_FOOT, m_leader);
					SetObjectiveTimer(0);
					break;
			}
		}
	} else if (bInVehicle) {
		if ((!m_leader->bInVehicle || m_leader->LeavingCar()) && m_objective != OBJECTIVE_WAIT_IN_CAR_THEN_GET_OUT) {

			switch (m_leader->m_objective) {
				case OBJECTIVE_ENTER_CAR_AS_PASSENGER:
				case OBJECTIVE_ENTER_CAR_AS_DRIVER:
					if (m_pMyVehicle == m_leader->m_pMyVehicle || m_pMyVehicle == m_leader->m_carInObjective)
						break;

					// fall through
				default:
					if (m_pMyVehicle && m_objective != OBJECTIVE_LEAVE_CAR) {
						m_leaveCarTimer = CTimer::GetTimeInMilliseconds() + 250;
						SetObjective(OBJECTIVE_LEAVE_CAR, m_pMyVehicle);
					}

					break;
			}
		}
	}
}

void
CPed::RestorePreviousObjective(void)
{
	if (m_objective == OBJECTIVE_NONE)
		return;

	if (m_objective != OBJECTIVE_LEAVE_CAR && m_objective != OBJECTIVE_ENTER_CAR_AS_PASSENGER && m_objective != OBJECTIVE_ENTER_CAR_AS_DRIVER
		&& !EnteringCar())
		m_pedInObjective = nil;

	if (m_objective == OBJECTIVE_WAIT_IN_CAR_THEN_GET_OUT) {
		m_objective = OBJECTIVE_NONE;
		if (m_pMyVehicle)
			SetObjective(OBJECTIVE_LEAVE_CAR, m_pMyVehicle);

	} else {
		m_objective = m_prevObjective;
		m_prevObjective = OBJECTIVE_NONE;
	}
	bObjectiveCompleted = false;
}

void
CPed::ProcessObjective(void)
{
	if (bClearObjective && (IsPedInControl() || m_nPedState == PED_DRIVING)) {
		ClearObjective();
		bClearObjective = false;
	}
	UpdateFromLeader();

	CVector carOrOurPos;
	CVector targetCarOrHisPos;
	CVector distWithTarget;

	if (m_objective != OBJECTIVE_NONE && (IsPedInControl() || m_nPedState == PED_DRIVING)) {
		if (bInVehicle) {
			if (!m_pMyVehicle) {
				bInVehicle = false;
				return;
			}
			carOrOurPos = m_pMyVehicle->GetPosition();
		} else {
			carOrOurPos = GetPosition();
		}

		if (m_pedInObjective) {
			if (m_pedInObjective->InVehicle() && !m_pedInObjective->BeingDraggedOutOfCar()) {
				targetCarOrHisPos = m_pedInObjective->m_pMyVehicle->GetPosition();
			} else {
				targetCarOrHisPos = m_pedInObjective->GetPosition();
			}
			distWithTarget = targetCarOrHisPos - carOrOurPos;
		} else if (m_carInObjective) {
			targetCarOrHisPos = m_carInObjective->GetPosition();
			distWithTarget = targetCarOrHisPos - carOrOurPos;
		}

		switch (m_objective) {
			case OBJECTIVE_WAIT_ON_FOOT:
				if (GetPedState() == PED_DRIVING)
					m_objective = OBJECTIVE_NONE;
				else {
					SetIdle();
					if (m_attractor) {
						if (m_objectiveTimer && CTimer::GetTimeInMilliseconds() > m_nWaitTimer) {
							GetPedAttractorManager()->BroadcastDeparture(this, m_attractor);
							m_objectiveTimer = 0;
						}
					} else {
						m_objective = OBJECTIVE_NONE;
						SetMoveState(PEDMOVE_STILL);
					}
				}
				break;
			case OBJECTIVE_WAIT_ON_FOOT_FOR_COP:
				if (!m_pedInObjective) {
					m_objective = OBJECTIVE_NONE;
					SetWanderPath(CGeneral::GetRandomNumberInRange(0.f, 8.f));
				} else if (m_pedInObjective->m_nPedType == PEDTYPE_COP && m_pedInObjective->m_nPedState == PED_DEAD) {
					m_objective = OBJECTIVE_NONE;
					SetWanderPath(CGeneral::GetRandomNumberInRange(0.f, 8.f));
					m_pedInObjective = nil;
				}
				break;
			case OBJECTIVE_FLEE_ON_FOOT_TILL_SAFE:
				if (m_leaveCarTimer >= CTimer::GetTimeInMilliseconds())
					break;

				if (InVehicle()) {
					SetObjective(OBJECTIVE_LEAVE_CAR, m_pMyVehicle);
					bFleeAfterExitingCar = true;
				} else if (m_nPedState != PED_FLEE_POSITION) {
					CVector2D fleePos = GetPosition();
					SetFlee(fleePos, 10000);
					bUsePedNodeSeek = true;
					m_pNextPathNode = nil;
				}
				break;
			case OBJECTIVE_GUARD_SPOT:
			{
				distWithTarget = m_vecSpotToGuard - GetPosition();
				if (m_pedInObjective) {
					SetLookFlag(m_pedInObjective, true);
					m_pLookTarget = m_pedInObjective;
					m_pLookTarget->RegisterReference((CEntity **) &m_pLookTarget);
					TurnBody();
				}
				float distWithTargetSc = distWithTarget.Magnitude();
				if (2.0f * m_radiusToGuard >= distWithTargetSc) {
					if (m_pedInObjective && m_pedInObjective->m_nPedState != PED_DEAD) {
						if (distWithTargetSc <= m_radiusToGuard)
							SetIdle();
						else {
							CVector seekPos = m_vecSpotToGuard;
							SetSeek(seekPos, m_radiusToGuard);
						}
					} else if (CTimer::GetTimeInMilliseconds() > m_lookTimer) {
						int threatType = ScanForThreats();
						SetLookTimer(CGeneral::GetRandomNumberInRange(500, 1500));

						// Second condition is pointless and isn't there in Mobile.
						if (threatType == PED_THREAT_GUN || (threatType == PED_THREAT_EXPLOSION && m_threatEntity) || m_threatEntity) {
							if (m_threatEntity->IsPed())
								SetObjective(OBJECTIVE_KILL_CHAR_ON_FOOT, m_threatEntity);
						}
					}
				} else {
					CVector seekPos = m_vecSpotToGuard;
					SetSeek(seekPos, m_radiusToGuard);
				}
				break;
			}
			case OBJECTIVE_WAIT_IN_CAR:
				SetPedState(PED_DRIVING);
				break;
			case OBJECTIVE_WAIT_IN_CAR_THEN_GET_OUT:
				SetPedState(PED_DRIVING);
				break;
			case OBJECTIVE_KILL_CHAR_ANY_MEANS:
			{
				if (m_pedInObjective) {
					if (m_pedInObjective->IsPlayer() && CharCreatedBy != MISSION_CHAR
						&& m_nPedType != PEDTYPE_COP && FindPlayerPed()->Wanted()->m_CurrentCops != 0
						&& !bKindaStayInSamePlace) {

						SetObjective(OBJECTIVE_FLEE_ON_FOOT_TILL_SAFE);
						break;
					}
					if (InVehicle()) {
						if (distWithTarget.Magnitude() >= 20.0f || m_pMyVehicle->m_vecMoveSpeed.MagnitudeSqr() >= sq(0.02f)) {

							if (((m_pMyVehicle->pDriver == this && m_pMyVehicle->AutoPilot.m_nCarMission == MISSION_NONE) || m_pMyVehicle->AutoPilot.m_nCarMission == MISSION_CRUISE)
								&& !m_pMyVehicle->m_nGettingInFlags) {
								m_pMyVehicle->SetStatus(STATUS_PHYSICS);
								m_pMyVehicle->AutoPilot.m_nPrevRouteNode = 0;
								if (m_nPedType == PEDTYPE_COP) {
									m_pMyVehicle->AutoPilot.m_nCruiseSpeed = (FindPlayerPed()->Wanted()->GetWantedLevel() * 0.1f + 0.6f) * (GAME_SPEED_TO_CARAI_SPEED * m_pMyVehicle->m_pHandlingData->m_Transmission.m_fMaxCruiseVelocity);
									m_pMyVehicle->AutoPilot.m_nCarMission = CCarAI::FindPoliceCarMissionForWantedLevel();
								} else {
									m_pMyVehicle->AutoPilot.m_nCruiseSpeed = GAME_SPEED_TO_CARAI_SPEED * m_pMyVehicle->m_pHandlingData->m_Transmission.m_fMaxCruiseVelocity * 0.8f;
									m_pMyVehicle->AutoPilot.m_nCarMission = MISSION_RAMPLAYER_FARAWAY;
								}
								m_pMyVehicle->AutoPilot.m_nDrivingStyle = DRIVINGSTYLE_AVOID_CARS;
							}
						} else {
							bool targetHasVeh = m_pedInObjective->bInVehicle;
							if (!targetHasVeh || targetHasVeh && m_pedInObjective->m_pMyVehicle->CanPedExitCar(false)) {
								m_pMyVehicle->AutoPilot.m_nCruiseSpeed = 0;
								m_pMyVehicle->AutoPilot.m_nCarMission = MISSION_NONE;
								SetObjective(OBJECTIVE_LEAVE_CAR, m_pMyVehicle);
							}
						}
						break;
					}
					if (distWithTarget.Magnitude() > 30.0f && !bKindaStayInSamePlace) {
						if (m_pMyVehicle) {
							m_pMyVehicle->AutoPilot.m_nCruiseSpeed = 0;
							SetObjective(OBJECTIVE_ENTER_CAR_AS_DRIVER, m_pMyVehicle);
						} else {
							float closestVehDist = 60.0f;
							int16 lastVehicle;
							CEntity* vehicles[8];
							CWorld::FindObjectsInRange(GetPosition(), ENTER_CAR_MAX_DIST, true, &lastVehicle, 6, vehicles, false, true, false, false, false);
							CVehicle *foundVeh = nil;
							for(int i = 0; i < lastVehicle; i++) {
								CVehicle *nearVeh = (CVehicle*)vehicles[i];
								/*
								Not used.
								CVector vehSpeed = nearVeh->GetSpeed();
								CVector ourSpeed = GetSpeed();
								*/
								CVector vehDistVec = nearVeh->GetPosition() - GetPosition();
								if (vehDistVec.Magnitude() < closestVehDist && m_pedInObjective->m_pMyVehicle != nearVeh
									&& nearVeh->CanPedOpenLocks(this) && nearVeh->m_fHealth > 250.f) {

									foundVeh = nearVeh;
									closestVehDist = vehDistVec.Magnitude();
								}
							}
							m_pMyVehicle = foundVeh;
							if (m_pMyVehicle) {
								m_pMyVehicle->RegisterReference((CEntity **) &m_pMyVehicle);
								m_pMyVehicle->AutoPilot.m_nCruiseSpeed = 0;
								SetObjective(OBJECTIVE_ENTER_CAR_AS_DRIVER, m_pMyVehicle);
							} else if (!GetIsOnScreen()) {
								CVector ourPos = GetPosition();
								int closestNode = ThePaths.FindNodeClosestToCoors(ourPos, PATH_CAR, 20.0f);
								if (closestNode >= 0) {
									int16 colliding;
									CWorld::FindObjectsKindaColliding(
										ThePaths.m_pathNodes[closestNode].GetPosition(), 10.0f, true, &colliding, 2, nil, false, true, true, false, false);
									if (!colliding) {
										CZoneInfo zoneInfo;
										int chosenCarClass;
										CTheZones::GetZoneInfoForTimeOfDay(&ourPos, &zoneInfo);
										int chosenModel = CCarCtrl::ChooseModel(&zoneInfo, &chosenCarClass);
										CVehicle *newVeh = nil;
										if (chosenModel != -1) {
											if (CModelInfo::IsBikeModel(chosenModel)) {
												newVeh = new CBike(chosenModel, RANDOM_VEHICLE);
											} else {
												newVeh = new CAutomobile(chosenModel, RANDOM_VEHICLE);
											}
										}
										if (newVeh) {
											newVeh->GetMatrix().GetPosition() = ThePaths.m_pathNodes[closestNode].GetPosition();
											newVeh->GetMatrix().GetPosition().z += 4.0f;
											newVeh->SetHeading(DEGTORAD(200.0f));
											newVeh->SetStatus(STATUS_ABANDONED);
											newVeh->m_nDoorLock = CARLOCK_UNLOCKED;
											CWorld::Add(newVeh);
											m_pMyVehicle = newVeh;
											if (m_pMyVehicle) {
												m_pMyVehicle->RegisterReference((CEntity **) &m_pMyVehicle);
												m_pMyVehicle->AutoPilot.m_nCruiseSpeed = 0;
												SetObjective(OBJECTIVE_ENTER_CAR_AS_DRIVER, m_pMyVehicle);
											}
										}
									}
								}
							}
						}
						break;
					}
				} else {
					ClearLookFlag();
					bObjectiveCompleted = true;
				}
			}
			case OBJECTIVE_KILL_CHAR_ON_FOOT:
			{
				if (m_objective == OBJECTIVE_KILL_CHAR_ON_FOOT && InVehicle()) {
					SetObjective(OBJECTIVE_LEAVE_CAR, m_pMyVehicle);
					break;
				}
				if (!m_pedInObjective || m_pedInObjective->DyingOrDead()) {
					bObjectiveCompleted = true;
					ClearLookFlag();
					SetMoveAnim();
					break;
				}
				if (m_pedInObjective) {
					int status;
					if (GetWeapon()->IsTypeMelee())
						status = KillCharOnFootMelee(carOrOurPos, targetCarOrHisPos, distWithTarget);
					else
						status = KillCharOnFootArmed(carOrOurPos, targetCarOrHisPos, distWithTarget);

					if (status == WATCH_UNTIL_HE_DISAPPEARS)
						return;
					if (status == CANT_ATTACK)
						break;
				}
				SetMoveAnim();
				break;
			}
			case OBJECTIVE_FLEE_CHAR_ON_FOOT_TILL_SAFE:
			case OBJECTIVE_FLEE_CHAR_ON_FOOT_ALWAYS:
			{
				if (InVehicle()) {
					if (m_nPedState == PED_DRIVING)
						SetObjective(OBJECTIVE_LEAVE_CAR, m_pMyVehicle);
				} else if (m_nPedState != PED_FLEE_ENTITY) {
					int time;
					if (m_objective == OBJECTIVE_FLEE_CHAR_ON_FOOT_ALWAYS)
						time = 0;
					else
						time = 6000;

					SetFindPathAndFlee(m_pedInObjective, time);
					if (m_pedStats == CPedStats::ms_apPedStats[PEDSTAT_FIREMAN])
						bMakeFleeScream = true;
				}
				break;
			}
			case OBJECTIVE_GOTO_CHAR_ON_FOOT:
			case OBJECTIVE_GOTO_CHAR_ON_FOOT_WALKING:
			case OBJECTIVE_HASSLE_CHAR:
			{
				if (m_pedInObjective) {
					float safeDistance = 2.0f;
					if (m_pedInObjective->bInVehicle)
						safeDistance = 3.0f;
					if (m_objective == OBJECTIVE_HASSLE_CHAR)
						safeDistance = 1.0f;

					float distWithTargetSc = distWithTarget.Magnitude();
					if (m_nPedStateTimer < CTimer::GetTimeInMilliseconds()) {
						if (distWithTargetSc <= safeDistance) {
							bScriptObjectiveCompleted = true;
							if (m_nPedState != PED_ATTACK) {
								SetIdle();
								if (m_pLookTarget)
									m_pLookTarget->CleanUpOldReference(&m_pLookTarget);
								m_pLookTarget = m_pedInObjective;
								m_pLookTarget->RegisterReference((CEntity **) &m_pLookTarget);
								TurnBody();
							}
							if (distWithTargetSc > 2.0f)
								SetMoveState(m_pedInObjective->m_nMoveState);
							else
								SetMoveState(PEDMOVE_STILL);

							if (m_objective == OBJECTIVE_HASSLE_CHAR) {
								Say(SOUND_PED_COP_ASK_FOR_ID);
								m_pedInObjective->Say(SOUND_PED_INNOCENT);
								m_leaveCarTimer = CTimer::GetTimeInMilliseconds() + 3000;
								m_pedInObjective->m_leaveCarTimer = CTimer::GetTimeInMilliseconds() + 3000;
								SetObjective(OBJECTIVE_WANDER);
								m_pedInObjective->SetObjective(OBJECTIVE_WANDER);
								CVector2D dist = GetPosition() - m_pedInObjective->GetPosition();
								m_nPathDir = CGeneral::GetNodeHeadingFromVector(dist.x, dist.y);
								m_pedInObjective->m_nPathDir = CGeneral::GetNodeHeadingFromVector(-dist.x, -dist.y);
							}
						} else {
							SetSeek(m_pedInObjective, safeDistance);
							if (distWithTargetSc >= 5.0f) {
								if (m_leader && m_leader->m_nMoveState == PEDMOVE_SPRINT)
									SetMoveState(PEDMOVE_SPRINT);
								else
									SetMoveState(PEDMOVE_RUN);
							} else {
								if (m_leader && m_leader->m_nMoveState != PEDMOVE_STILL
									&& m_leader->m_nMoveState != PEDMOVE_NONE) {
									if (m_leader->IsPlayer()) {
										if (distWithTargetSc >= 3.0f && FindPlayerPed()->MoveSpeed() >= 1.3f)
											SetMoveState(PEDMOVE_RUN);
										else
											SetMoveState(PEDMOVE_WALK);
									} else {
										SetMoveState(m_leader->m_nMoveState);
									}
								} else if (distWithTargetSc <= 3.0f) {
									SetMoveState(PEDMOVE_WALK);
								} else {
									SetMoveState(PEDMOVE_RUN);
								}
							}
						}
						if (m_objective == OBJECTIVE_GOTO_CHAR_ON_FOOT_WALKING && m_nMoveState > PEDMOVE_STILL)
							SetMoveState(PEDMOVE_WALK);
					}
				} else {
					SetObjective(OBJECTIVE_NONE);
				}
				break;
			}
			case OBJECTIVE_FOLLOW_CHAR_IN_FORMATION:
			{
				if (m_pedInObjective) {
					CVector posToGo = GetFormationPosition();
					distWithTarget = posToGo - carOrOurPos;
					SetSeek(posToGo, 1.0f);
					if (distWithTarget.Magnitude() <= 3.0f) {
						SetSeek(posToGo, 1.0f);
						if (m_pedInObjective && m_pedInObjective->m_nMoveState != PEDMOVE_STILL)
							SetMoveState(m_pedInObjective->m_nMoveState);
					} else {
						SetSeek(posToGo, 1.0f);
						SetMoveState(PEDMOVE_RUN);
					}
				} else {
					SetObjective(OBJECTIVE_NONE);
				}
				break;
			}
			case OBJECTIVE_ENTER_CAR_AS_PASSENGER:
			{
				// Vehicle entry is task-based (the SA way): this
				// objective only hands the ped its enter task.
				if (m_carInObjective) {
					if (bInVehicle && m_pMyVehicle == m_carInObjective) {
						bObjectiveCompleted = true;
						bScriptObjectiveCompleted = true;
						RestorePreviousState();
					} else if (m_carInObjective->IsTrain()) {
						if (GetIntelligence()->FindTaskByType(TASK_COMPLEX_ENTER_TRAIN_AS_PASSENGER) == nil)
							GetIntelligence()->AddTaskPrimaryMaybeInGroup(
								new CTaskComplexEnterTrainAsPassenger(m_carInObjective), true);
					} else if (GetIntelligence()->FindTaskByType(TASK_COMPLEX_ENTER_CAR_AS_PASSENGER) == nil) {
						GetIntelligence()->AddTaskPrimaryMaybeInGroup(
							new CTaskComplexEnterCarAsPassenger(m_carInObjective, 0), true);
					}
				}
				break;
			}
			case OBJECTIVE_ENTER_CAR_AS_DRIVER:
			{
				// Task-based entry, as in the passenger case above.
				if (m_carInObjective) {
					if (bInVehicle && m_pMyVehicle == m_carInObjective) {
						bObjectiveCompleted = true;
						bScriptObjectiveCompleted = true;
						RestorePreviousState();
					} else if (m_carInObjective->IsTrain()) {
						if (GetIntelligence()->FindTaskByType(TASK_COMPLEX_ENTER_TRAIN_AS_PASSENGER) == nil)
							GetIntelligence()->AddTaskPrimaryMaybeInGroup(
								new CTaskComplexEnterTrainAsPassenger(m_carInObjective), true);
					} else if (GetIntelligence()->FindTaskByType(TASK_COMPLEX_ENTER_CAR_AS_DRIVER) == nil) {
						GetIntelligence()->AddTaskPrimaryMaybeInGroup(
								new CTaskComplexEnterCarAsDriver(m_carInObjective), true);
					}
				} else if (bInVehicle && m_pMyVehicle != nil) {
					// In some other vehicle: get out of it first.
					if (GetIntelligence()->FindTaskByType(TASK_COMPLEX_LEAVE_CAR) == nil)
						GetIntelligence()->AddTaskPrimaryMaybeInGroup(
							new CTaskComplexLeaveCar(m_pMyVehicle, 0, 0, true, false), true);
				}
				break;
			}
			case OBJECTIVE_DESTROY_OBJECT:
				if (m_pPointGunAt) {
					if (m_nPedState != PED_ATTACK)
						SetAttack(m_pPointGunAt);
				} else {
					bScriptObjectiveCompleted = true;
					RestorePreviousObjective();
				}
				break;
			case OBJECTIVE_DESTROY_CAR:
			{
				if (!m_carInObjective) {
					ClearLookFlag();
					bObjectiveCompleted = true;
					break;
				}
				float distWithTargetSc = distWithTarget.Magnitude();
				CWeaponInfo *wepInfo = CWeaponInfo::GetWeaponInfo(GetWeapon()->m_eWeaponType);
				float wepRange = wepInfo->m_fRange;
				m_pLookTarget = m_carInObjective;
				m_pLookTarget->RegisterReference((CEntity **) &m_pLookTarget);

				m_pSeekTarget = m_carInObjective;
				m_pSeekTarget->RegisterReference((CEntity**) &m_pSeekTarget);

				TurnBody();
				if (m_carInObjective->m_fHealth <= 0.0f) {
					ClearLookFlag();
					bScriptObjectiveCompleted = true;
					break;
				}

				if (m_attackTimer < CTimer::GetTimeInMilliseconds() && distWithTargetSc < wepRange) {
					// I hope so
					CVector ourHead = GetMatrix() * CVector(0.5f, 0.0f, 0.6f);
					CVector maxShotPos = m_carInObjective->GetPosition() - ourHead;
					maxShotPos *= wepInfo->m_fRange / maxShotPos.Magnitude();
					maxShotPos += ourHead;

					CColPoint foundCol;
					CEntity *foundEnt;

					CWorld::bIncludeDeadPeds = true;
					CWorld::ProcessLineOfSight(ourHead, maxShotPos, foundCol, foundEnt, true, true, true, true, false, true, false);
					CWorld::bIncludeDeadPeds = false;
					if (foundEnt == m_carInObjective) {
						SetAttack(m_carInObjective);
						SetWeaponLockOnTarget(m_carInObjective);
						SetShootTimer(CGeneral::GetRandomNumberInRange(500, 2000));
						if (distWithTargetSc > 10.0f && !bKindaStayInSamePlace) {
							SetAttackTimer(CGeneral::GetRandomNumberInRange(2000, 5000));
						} else {
							SetAttackTimer(CGeneral::GetRandomNumberInRange(50, 300));
							SetMoveState(PEDMOVE_STILL);
						}
					}
				} else if (m_nPedState != PED_ATTACK && !bKindaStayInSamePlace) {
					if (wepRange <= 5.0f) {
						if (Abs(distWithTarget.x) > wepRange || Abs(distWithTarget.y) > wepRange ||
							(distWithTarget.z > -1.0f && distWithTarget.z < 0.3)) {
							SetSeek(m_carInObjective, 3.0f);
							SetMoveState(PEDMOVE_RUN);
						} else {
							SetIdle();
						}
					} else {
						float safeDistance = wepRange * 0.25f;

						SetSeek(m_carInObjective, safeDistance);
						SetMoveState(PEDMOVE_RUN);
					}
				}
				SetLookFlag(m_carInObjective, false);
				TurnBody();
				break;
			}
			case OBJECTIVE_GOTO_AREA_ON_FOOT:
			case OBJECTIVE_RUN_TO_AREA:
			case OBJECTIVE_SPRINT_TO_AREA:
			{
				if (InVehicle()) {
					SetObjective(OBJECTIVE_LEAVE_CAR, m_pMyVehicle);
				} else {
					distWithTarget = m_nextRoutePointPos - GetPosition();
					distWithTarget.z = 0.0f;
					if (sq(m_distanceToCountSeekDone) >= distWithTarget.MagnitudeSqr()) {
						bObjectiveCompleted = true;
						bScriptObjectiveCompleted = true;
						SetMoveState(PEDMOVE_STILL);
					} else if (CTimer::GetTimeInMilliseconds() > m_nPedStateTimer || m_nPedState != PED_SEEK_POSITION) {
						if (bUsePedNodeSeek) {
							CVector bestCoords(0.0f, 0.0f, 0.0f);
							m_vecSeekPos = m_nextRoutePointPos;

							if (!m_pNextPathNode) {
								bool found = FindBestCoordsFromNodes(m_vecSeekPos, &bestCoords);
								if (m_pNextPathNode) {
									// Because it already does that if it finds better coords.
									if (!found) {
										bestCoords = CPathFind::TakeWidthIntoAccountForWandering(m_pNextPathNode, m_randomSeed);
									}
									if ((bestCoords - GetPosition()).Magnitude2D() < m_distanceToCountSeekDone) {
										m_pNextPathNode = nil;
										bUsePedNodeSeek = false;
									}
								}
							}
							if (m_pNextPathNode)
								m_vecSeekPos = CPathFind::TakeWidthIntoAccountForWandering(m_pNextPathNode, m_randomSeed);
						}
						CVector seekPos = m_vecSeekPos;
						SetSeek(m_vecSeekPos, m_distanceToCountSeekDone);
					}
				}

				break;
			}
			case OBJECTIVE_GUARD_ATTACK:
			{
				if (m_pedInObjective) {
					SetLookFlag(m_pedInObjective, true);
					m_pLookTarget = m_pedInObjective;
					m_pLookTarget->RegisterReference((CEntity **) &m_pLookTarget);
					m_lookTimer = m_attackTimer;
					TurnBody();
					float distWithTargetSc = distWithTarget.Magnitude();
					if (distWithTargetSc >= 20.0f) {
						RestorePreviousObjective();
					} else if (m_attackTimer < CTimer::GetTimeInMilliseconds()) {
						if (m_nPedState != PED_SEEK_ENTITY && distWithTargetSc >= 2.0f) {
							SetSeek(m_pedInObjective, 1.0f);
						} else {
							SetAttack(m_pedInObjective);
							SetShootTimer(CGeneral::GetRandomNumberInRange(500.0f, 1500.0f));
						}
						SetAttackTimer(1000);
					}
				} else {
					RestorePreviousObjective();
				}
				break;
			}
			case OBJECTIVE_FOLLOW_ROUTE:
				if (HaveReachedNextPointOnRoute(1.0f)) {
					int nextPoint = GetNextPointOnRoute();
					m_nextRoutePointPos = CRouteNode::GetPointPosition(nextPoint);
				} else {
					CVector seekPos = m_nextRoutePointPos;
					SetSeek(seekPos, 0.8f);
				}
				break;
			case OBJECTIVE_SOLICIT_VEHICLE:
				if (m_carInObjective) {
					if (m_objectiveTimer <= CTimer::GetTimeInMilliseconds()) {
						if (!bInVehicle) {
							SetObjective(OBJECTIVE_NONE);
							SetWanderPath(CGeneral::GetRandomNumber() & 7);
							m_objectiveTimer = CTimer::GetTimeInMilliseconds() + 10000;
							if (IsPedInControl())
								m_pMyVehicle = nil;
						}
					} else {
						if (m_nPedState != PED_FLEE_ENTITY && m_nPedState != PED_SOLICIT &&
						    GetIntelligence()->FindTaskByType(TASK_COMPLEX_GO_TO_CAR_DOOR_AND_STAND_STILL) == nil)
							GetIntelligence()->AddTaskPrimaryMaybeInGroup(
								new CTaskComplexGoToCarDoorAndStandStill(m_carInObjective, PEDMOVE_WALK, true, 0, 0.5f, 4.0f, 50.0f, 30000), true);
					}
				} else {
					RestorePreviousObjective();
					RestorePreviousState();
					if (IsPedInControl())
						m_pMyVehicle = nil;
				}
				break;
			case OBJECTIVE_HAIL_TAXI:
				if (!RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_IDLE_TAXI) && CTimer::GetTimeInMilliseconds() > m_nWaitTimer) {
					Say(SOUND_PED_TAXI_WAIT);
					CAnimManager::BlendAnimation(GetClump(), ANIM_GROUP_DEFAULT, ANIM_ID_IDLE_TAXI, 4.0f);
					m_nWaitTimer = CTimer::GetTimeInMilliseconds() + 2000;
				}
				break;
			case OBJECTIVE_CATCH_TRAIN:
			{
				if (m_carInObjective) {
					SetObjective(OBJECTIVE_ENTER_CAR_AS_PASSENGER, m_carInObjective);
				} else {
					CVehicle* trainToEnter = nil;
					float closestCarDist = CHECK_NEARBY_THINGS_MAX_DIST;
					CVector pos = GetPosition();
					int16 lastVehicle;
					CEntity* vehicles[8];

					CWorld::FindObjectsInRange(pos, 10.0f, true, &lastVehicle, 6, vehicles, false, true, false, false, false);
					for (int i = 0; i < lastVehicle; i++) {
						CVehicle* nearVeh = (CVehicle*)vehicles[i];
						if (nearVeh->IsTrain()) {
							CVector vehDistVec = GetPosition() - nearVeh->GetPosition();
							float vehDist = vehDistVec.Magnitude();
							if (vehDist < closestCarDist && m_pedInObjective->m_pMyVehicle != nearVeh)
							{
								trainToEnter = nearVeh;
								closestCarDist = vehDist;
							}
						}
					}
					if (trainToEnter) {
						m_carInObjective = trainToEnter;
						m_carInObjective->RegisterReference((CEntity **) &m_carInObjective);
					}
				}
				break;
			}
			case OBJECTIVE_BUY_ICE_CREAM:
				if (m_carInObjective) {
					if (m_nPedState != PED_FLEE_ENTITY && m_nPedState != PED_BUY_ICE_CREAM && m_nPedState != PED_CHAT &&
					    GetIntelligence()->FindTaskByType(TASK_COMPLEX_GO_TO_CAR_DOOR_AND_STAND_STILL) == nil)
						GetIntelligence()->AddTaskPrimaryMaybeInGroup(
							new CTaskComplexGoToCarDoorAndStandStill(m_carInObjective, PEDMOVE_WALK, false, 0, 0.5f, 4.0f, 50.0f, 30000), true);
				}
				break;
			case OBJECTIVE_STEAL_ANY_CAR:
			case OBJECTIVE_STEAL_ANY_MISSION_CAR:
			{
				if (bInVehicle) {
					bScriptObjectiveCompleted = true;
					RestorePreviousObjective();
				} else if (m_carJackTimer < CTimer::GetTimeInMilliseconds()) {
					CVehicle *carToSteal = nil;
					float closestCarDist = nEnterCarRangeMultiplier * ENTER_CAR_MAX_DIST;
					CVector pos = GetPosition();
					int16 lastVehicle;
					CEntity *vehicles[8];

					CWorld::FindObjectsInRange(pos, closestCarDist, true, &lastVehicle, 6, vehicles, false, true, false, false, false);
					for(int i = 0; i < lastVehicle; i++) {
						CVehicle *nearVeh = (CVehicle*)vehicles[i];
						if (m_objective == OBJECTIVE_STEAL_ANY_MISSION_CAR || nearVeh->VehicleCreatedBy != MISSION_VEHICLE) {
							if (nearVeh->m_vecMoveSpeed.Magnitude() <= 0.1f) {
								if (nearVeh->CanPedOpenLocks(this)) {
									CVector vehDistVec = GetPosition() - nearVeh->GetPosition();
									float vehDist = vehDistVec.Magnitude();
									if (vehDist < closestCarDist) {
										carToSteal = nearVeh;
										closestCarDist = vehDist;
									}
								}
							}
						}
					}
					if (carToSteal) {
						SetObjective(OBJECTIVE_ENTER_CAR_AS_DRIVER, carToSteal);
						m_carJackTimer = CTimer::GetTimeInMilliseconds() + 5000;
					} else {
						RestorePreviousObjective();
						RestorePreviousState();
					}
				}
				break;
			}
			case OBJECTIVE_MUG_CHAR:
			{
				if (m_pedInObjective) {
					if (m_pedInObjective->IsPlayer() || m_pedInObjective->bInVehicle || m_pedInObjective->m_fHealth <= 0.0f) {
						ClearObjective();
						return;
					}
					if (m_pedInObjective->m_nMoveState > PEDMOVE_WALK) {
						ClearObjective();
						return;
					}
					if (m_pedInObjective->m_objective == OBJECTIVE_KILL_CHAR_ON_FOOT && m_pedInObjective->m_pedInObjective == this) {
						SetObjective(OBJECTIVE_FLEE_CHAR_ON_FOOT_TILL_SAFE, m_pedInObjective);
						SetMoveState(PEDMOVE_SPRINT);
						return;
					}
					if (m_pedInObjective->m_nPedState == PED_FLEE_ENTITY && m_fleeFrom == this
						|| m_pedInObjective->m_objective == OBJECTIVE_FLEE_CHAR_ON_FOOT_TILL_SAFE && m_pedInObjective->m_pedInObjective == this) {
						ClearObjective();
						SetFindPathAndFlee(m_pedInObjective, 15000, true);
						return;
					}
					float distWithTargetScSqr = distWithTarget.MagnitudeSqr();
					if (distWithTargetScSqr <= sq(10.0f)) {
						if (distWithTargetScSqr <= sq(1.4f)) {
							CAnimBlendAssociation *reloadAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_FUCKU);
							m_fRotationDest = CGeneral::GetRadianAngleBetweenPoints(
								m_pedInObjective->GetPosition().x, m_pedInObjective->GetPosition().y,
								GetPosition().x, GetPosition().y);

							if (reloadAssoc || !m_pedInObjective->IsPedShootable()) {
								if (reloadAssoc &&
									(!reloadAssoc->IsPlaying() || reloadAssoc->GetTimeProgress() > 0.8f)) {
									CAnimBlendAssociation *punchAssoc = CAnimManager::BlendAnimation(GetClump(), CWeaponInfo::GetWeaponInfo(GetWeapon()->m_eWeaponType)->m_AnimToPlay, ANIM_ID_FIGHT_1, 8.0f);
									if (punchAssoc)
										punchAssoc->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_FINISH_AUTO_REMOVE;
									CVector2D offset(distWithTarget.x, distWithTarget.y);
									int dir = m_pedInObjective->GetLocalDirection(offset);
									m_pedInObjective->StartFightDefend(dir, HITLEVEL_HIGH, 5);
									m_pedInObjective->ReactToAttack(this);
									m_pedInObjective->Say(SOUND_PED_ROBBED);
									Say(SOUND_PED_MUGGING);
									bRichFromMugging = true;

									// FIX: ClearObjective() clears m_pedInObjective, so get it before call
									CPed *victim = m_pedInObjective;
									ClearObjective();
									if (victim->m_objective != OBJECTIVE_KILL_CHAR_ON_FOOT
										|| victim->m_pedInObjective != this) {
										SetFindPathAndFlee(victim, 15000, true);
										m_nLastPedState = PED_WANDER_PATH;
									} else {
										SetObjective(OBJECTIVE_FLEE_CHAR_ON_FOOT_TILL_SAFE, victim);
										SetMoveState(PEDMOVE_SPRINT);
										m_nLastPedState = PED_WANDER_PATH;
									}
								}
							} else {
								eWeaponType weaponType = GetWeapon()->m_eWeaponType;
								if (weaponType != WEAPONTYPE_UNARMED && weaponType != WEAPONTYPE_BASEBALLBAT)
									SetCurrentWeapon(WEAPONTYPE_UNARMED);

								CAnimBlendAssociation *newReloadAssoc = CAnimManager::BlendAnimation(GetClump(), ANIM_GROUP_DEFAULT, ANIM_ID_FUCKU, 8.0f);
								if (newReloadAssoc)
									newReloadAssoc->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE | ANIMATION_IS_FINISH_AUTO_REMOVE;
							}
						} else {
							SetSeek(m_pedInObjective, 1.0f);
							CAnimBlendAssociation *walkAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_WALK);

							if (walkAssoc)
								walkAssoc->m_Speed = 1.3f;
						}
					} else {
						ClearObjective();
						SetWanderPath(CGeneral::GetRandomNumber() & 7);
					}
				} else {
					m_objective = OBJECTIVE_NONE;
					ClearObjective();
				}
			}
			// fall through
			case OBJECTIVE_WANDER:
				if (CTimer::GetTimeInMilliseconds() > m_leaveCarTimer && !bInVehicle) {
					m_leaveCarTimer = 0;
					m_objective = OBJECTIVE_NONE;
					SetWanderPath(m_nPathDir);
				}
				break;
			case OBJECTIVE_LEAVE_CAR_AND_DIE:
			{
				// The leave-and-die task puts him out as a corpse.
				if (InVehicle() && m_pMyVehicle) {
					if (GetIntelligence()->FindTaskByType(TASK_COMPLEX_LEAVE_CAR) == nil)
						GetIntelligence()->AddTaskPrimaryMaybeInGroup(
							new CTaskComplexLeaveCar(m_pMyVehicle, 0, 0), true);
					RestorePreviousObjective();
				}
				break;
			}
			case OBJECTIVE_GOTO_AREA_ANY_MEANS:
			{
				distWithTarget = m_nextRoutePointPos - GetPosition();
				distWithTarget.z = 0.0f;
				if (InVehicle()) {
					CCarAI::GetCarToGoToCoors(m_pMyVehicle, &m_nextRoutePointPos);
					CCarCtrl::RegisterVehicleOfInterest(m_pMyVehicle);
					if (distWithTarget.MagnitudeSqr() < sq(20.0f)) {
						m_pMyVehicle->AutoPilot.m_nCruiseSpeed = 0;
						ForceStoredObjective(OBJECTIVE_GOTO_AREA_ANY_MEANS);
						SetObjective(OBJECTIVE_LEAVE_CAR, m_pMyVehicle);
					}
					break;
				}
				if (distWithTarget.Magnitude() > 30.0f) {
					if (m_pMyVehicle) {
						m_pMyVehicle->AutoPilot.m_nCruiseSpeed = 0;
					} else {
						float closestVehDist = SQR(60.0f);
						int16 lastVehicle;
						CEntity* vehicles[8];
						// NB: 25.0f in here is prolly a forgotten setting, all other places now use 30.0f (ENTER_CAR_MAX_DIST)
						CWorld::FindObjectsInRange(GetPosition(), 25.0f, true, &lastVehicle, 6, vehicles, false, true, false, false, false);
						CVehicle* foundVeh = nil;
						for (int i = 0; i < lastVehicle; i++) {
							CVehicle* nearVeh = (CVehicle*)vehicles[i];
							/*
							Not used.
							CVector vehSpeed = nearVeh->GetSpeed();
							CVector ourSpeed = GetSpeed();
							*/
							CVector vehDistVec = nearVeh->GetPosition() - GetPosition();
							if (vehDistVec.MagnitudeSqr() < closestVehDist && m_pedInObjective->m_pMyVehicle != nearVeh) {
								foundVeh = nearVeh;
								closestVehDist = vehDistVec.MagnitudeSqr();
							}
						}
						m_pMyVehicle = foundVeh;
						if (m_pMyVehicle) {
							m_pMyVehicle->RegisterReference((CEntity **) &m_pMyVehicle);
							m_pMyVehicle->AutoPilot.m_nCruiseSpeed = 0;
							SetObjective(OBJECTIVE_ENTER_CAR_AS_DRIVER, m_pMyVehicle);
						}
					}
					break;
				}
				// Falls to different objectives in III and VC
#ifdef FIX_BUGS
				break;
#else
				// fall through
#endif
			}
			case OBJECTIVE_GOTO_SEAT_ON_FOOT:
			case OBJECTIVE_GOTO_ATM_ON_FOOT:
			case OBJECTIVE_GOTO_BUS_STOP_ON_FOOT:
			case OBJECTIVE_GOTO_PIZZA_ON_FOOT:
			case OBJECTIVE_GOTO_SHELTER_ON_FOOT:
			case OBJECTIVE_GOTO_ICE_CREAM_VAN_ON_FOOT:
				if (CTimer::GetTimeInMilliseconds() > m_objectiveTimer) {
					m_objectiveTimer = 0;
					if (m_attractor)
						GetPedAttractorManager()->DeRegisterPed(this, m_attractor);
				} else {
					CVector distance = m_nextRoutePointPos - GetPosition();
					distance.z = 0.0f;
					if (m_objective == OBJECTIVE_GOTO_SHELTER_ON_FOOT) {
						if (m_nMoveState == PEDMOVE_RUN && distance.MagnitudeSqr() < SQR(2.0f)) {
							SetMoveState(PEDMOVE_WALK);
							bIsRunning = false;
						}
						if (CWeather::Rain < 0.2f && m_attractor) {
							GetPedAttractorManager()->DeRegisterPed(this, m_attractor);
							return;
						}
					}
					else if (m_objective == OBJECTIVE_GOTO_ICE_CREAM_VAN_ON_FOOT) {
						if (m_nMoveState == PEDMOVE_RUN && distance.MagnitudeSqr() < SQR(4.0f)) {
							SetMoveState(PEDMOVE_WALK);
							bIsRunning = false;
						}
						CVehicle* pIceCreamVan = GetPedAttractorManager()->GetIceCreamVanForEffect(m_attractor->GetEffect());
						if (0.01f * CTimer::GetTimeStep() * 5.0f < pIceCreamVan->m_fDistanceTravelled) {
							GetPedAttractorManager()->DeRegisterPed(this, m_attractor);
							return;
						}
						if (!pIceCreamVan->pDriver ||
							!pIceCreamVan->pDriver->IsPlayer() ||
							pIceCreamVan->pDriver->GetPedState() == PED_ARRESTED ||
							pIceCreamVan->pDriver->GetPedState() == PED_DEAD) {
							GetPedAttractorManager()->DeRegisterPed(this, m_attractor);
							return;
						}
						if (!pIceCreamVan->m_bSirenOrAlarm) {
							GetPedAttractorManager()->DeRegisterPed(this, m_attractor);
							return;
						}
						if (pIceCreamVan->GetStatus() == STATUS_WRECKED) {
							GetPedAttractorManager()->DeRegisterPed(this, m_attractor);
							return;
						}
					}
					if (sq(m_distanceToCountSeekDone) < distance.MagnitudeSqr()) {
						if (CTimer::GetTimeInMilliseconds() > m_nPedStateTimer || GetPedState() != PED_SEEK_POSITION) {
							m_vecSeekPos = m_nextRoutePointPos;
							SetSeek(m_vecSeekPos, m_distanceToCountSeekDone);
						}
					}
					else {
						if (!bReachedAttractorHeadingTarget) {
							float fHeadingDistance = m_fRotationCur - m_attractorHeading;
							float fSinHeading = Sin(fHeadingDistance);
							float fCosHeading = Cos(fHeadingDistance);
							if (fSinHeading > 0.0f) {
								if (fCosHeading > 0.0f)
									m_attractorHeading = m_fRotationCur - Asin(fSinHeading);
								else
									m_attractorHeading = m_fRotationCur - Acos(fCosHeading);
							}
							else {
								if (fCosHeading > 0.0f)
									m_attractorHeading = m_fRotationCur - Asin(fSinHeading);
								else
									m_attractorHeading = m_fRotationCur + Acos(fCosHeading);
							}
							m_fRotationDest = m_attractorHeading;
							m_headingRate = 3.5f;
							bReachedAttractorHeadingTarget = true;
							bTurnedAroundOnAttractor = false;
						}
						if (Abs(m_fRotationCur - m_attractorHeading) >= m_acceptableHeadingOffset &&
							Abs(m_fRotationCur - m_attractorHeading + TWOPI) >= m_acceptableHeadingOffset &&
							Abs(m_fRotationCur - m_attractorHeading - TWOPI) >= m_acceptableHeadingOffset)
							SetMoveState(PEDMOVE_STILL);
						else {
							m_fRotationDest = m_fRotationCur;
							bReachedAttractorHeadingTarget = false;
							bObjectiveCompleted = true;
							bScriptObjectiveCompleted = true;
							RestoreHeadingRate();
							GetPedAttractorManager()->BroadcastArrival(this, m_attractor);
							if (GetPedAttractorManager()->IsAtHeadOfQueue(this, m_attractor)) {
								switch (m_objective) {
								case OBJECTIVE_GOTO_SEAT_ON_FOOT:
									if (!bTurnedAroundOnAttractor) {
										ClearObjective();
										SetWaitState(WAITSTATE_SIT_DOWN, 0);
									}
									else {
										ClearObjective();
										SetWaitState(WAITSTATE_SIT_DOWN_RVRS, 0);
									}
									break;
								case OBJECTIVE_GOTO_ATM_ON_FOOT:
									ClearObjective();
									SetWaitState(WAITSTATE_USE_ATM, 0);
									break;
								case OBJECTIVE_GOTO_BUS_STOP_ON_FOOT:
									ClearObjective();
									SetObjective(OBJECTIVE_WAIT_ON_FOOT_AT_BUS_STOP);
									break;
								case OBJECTIVE_GOTO_PIZZA_ON_FOOT:
									ClearObjective();
									m_prevObjective = OBJECTIVE_NONE;
									SetObjective(OBJECTIVE_WAIT_ON_FOOT);
									m_objectiveTimer = CTimer::GetTimeInMilliseconds() + m_attractor->GetHeadOfQueueWaitTime();
									break;
								case OBJECTIVE_GOTO_SHELTER_ON_FOOT:
									m_prevObjective = OBJECTIVE_NONE;
									SetObjective(OBJECTIVE_WAIT_ON_FOOT_AT_SHELTER);
									break;
								case OBJECTIVE_GOTO_ICE_CREAM_VAN_ON_FOOT:
									m_prevObjective = OBJECTIVE_NONE;
									SetObjective(OBJECTIVE_WAIT_ON_FOOT_AT_ICE_CREAM_VAN);
									break;
								}
							} else {
								m_prevObjective = OBJECTIVE_NONE;
								SetObjective(OBJECTIVE_WAIT_ON_FOOT);
								m_objectiveTimer = 0;
							}
						}
					}
				}
				return;
			case OBJECTIVE_FLEE_CAR:
				if (!bInVehicle && m_nPedState != PED_FLEE_ENTITY && m_pMyVehicle) {
					RestorePreviousObjective();
					SetFlee(m_pMyVehicle, 6000);
					break;
				}
				// fall through
			case OBJECTIVE_LEAVE_CAR:
				// Leaving is task-based too, trains and boats included.
				if (InVehicle() && m_pMyVehicle) {
					if (GetIntelligence()->FindTaskByType(TASK_COMPLEX_LEAVE_CAR) == nil)
						GetIntelligence()->AddTaskPrimaryMaybeInGroup(
							new CTaskComplexLeaveCar(m_pMyVehicle, 0, 0, true, false), true);
					RestorePreviousObjective();
				} else if (!InVehicle()) {
					RestorePreviousObjective();
				}
				if (bHeldHostageInCar) {
					if (CTheScripts::IsPlayerOnAMission()) {
						CVehicle *playerVeh = FindPlayerVehicle();
						if (playerVeh && playerVeh->IsPassenger(this)) {
							if (m_leaveCarTimer != 0)
								m_leaveCarTimer = 0;
						}
					}
				}
				break;
			case OBJECTIVE_AIM_GUN_AT:
				if (m_pedInObjective) {
					if (!bObstacleShowedUpDuringKillObjective)
						SetPointGunAt(m_pedInObjective);

					if (m_nMoveState == PEDMOVE_STILL && IsPedInControl()) {
						SetLookFlag(m_pedInObjective, false);
						TurnBody();
					}
				} else {
					ClearObjective();
				}
				break;
			case OBJECTIVE_WAIT_ON_FOOT_AT_SHELTER:
				SetIdle();
				if (m_attractor && CWeather::Rain < 0.2f)
					GetPedAttractorManager()->DeRegisterPed(this, m_attractor);
				break;
			case OBJECTIVE_KILL_CHAR_ON_BOAT:
				SetPedStats(PEDSTAT_TOUGH_GUY);
				if (bInVehicle) {
					if (m_pedInObjective && m_pedInObjective->m_pCurrentPhysSurface != m_pMyVehicle) {
						bObjectiveCompleted = true;
						ClearObjective();
						CCarCtrl::SwitchVehicleToRealPhysics(m_pMyVehicle);
						m_pMyVehicle->AutoPilot.m_nDrivingStyle = DRIVINGSTYLE_AVOID_CARS;
						m_pMyVehicle->AutoPilot.m_nCruiseSpeed = GAME_SPEED_TO_CARAI_SPEED * m_pMyVehicle->m_pHandlingData->m_Transmission.m_fMaxCruiseVelocity;
						m_pMyVehicle->SetStatus(STATUS_PHYSICS);
					} else {
						SetObjective(OBJECTIVE_LEAVE_CAR, m_pMyVehicle);
					}
				} else if (m_pedInObjective && !m_pedInObjective->DyingOrDead() &&
					(!m_pCurrentPhysSurface || !m_pedInObjective->m_pCurrentPhysSurface ||
						m_pedInObjective->m_pCurrentPhysSurface == m_pCurrentPhysSurface)) {

					CBoat *boatWeAreOn = nil;
					if (m_pCurrentPhysSurface && m_pCurrentPhysSurface->IsVehicle() && ((CVehicle*)m_pCurrentPhysSurface)->IsBoat())
						boatWeAreOn = (CBoat*)m_pCurrentPhysSurface;

					if (boatWeAreOn) {
						SetObjective(OBJECTIVE_RUN_TO_AREA, boatWeAreOn->GetPosition() - (boatWeAreOn->GetForward() * 10.f));
					} else if (m_pedInObjective) {
						SetObjective(OBJECTIVE_FLEE_CHAR_ON_FOOT_ALWAYS, m_pedInObjective);
					}
					SetMoveAnim();
				} else {
					ClearLookFlag();
					SetMoveAnim();
					if (m_pCurrentPhysSurface && m_pCurrentPhysSurface->IsVehicle())
						SetObjective(OBJECTIVE_ENTER_CAR_AS_DRIVER, m_pCurrentPhysSurface);
				}
				break;
			case OBJECTIVE_SOLICIT_FOOT:
				if (m_pedInObjective) {
					if (m_objectiveTimer < CTimer::GetTimeInMilliseconds())
						bScriptObjectiveCompleted = true;
				} else {
					bScriptObjectiveCompleted = true;
				}
				break;
			case OBJECTIVE_WAIT_ON_FOOT_AT_BUS_STOP:
				SetIdle();
				if (m_attractor) {
					float left = GetPosition().x - 10.0f;
					float right = GetPosition().x + 10.0f;
					float top = GetPosition().y - 10.0f;
					float bottom = GetPosition().y + 10.0f;
					int xstart = CWorld::GetSectorIndexX(left);
					int xend = CWorld::GetSectorIndexX(right);
					int ystart = CWorld::GetSectorIndexY(top);
					int yend = CWorld::GetSectorIndexY(bottom);
					assert(xstart <= xend);
					assert(ystart <= yend);

					float minDistance = SQR(10.0f);
					CVehicle* pBus = nil;

					for (int y = ystart; y <= yend; y++) {
						for (int x = xstart; x <= xend; x++) {
							CSector* s = CWorld::GetSector(x, y);
							for (CPtrNode* pNode = s->m_lists[ENTITYLIST_VEHICLES].first; pNode != nil; pNode = pNode->next) {
								CEntity* pEntity = (CEntity*)pNode->item;
								if (!pEntity->IsVehicle())
									continue;
								CVehicle* pVehicle = (CVehicle*)pEntity;
								if (!pVehicle->bIsBus)
									continue;
								if (pVehicle->GetMoveSpeed().MagnitudeSqr() >= SQR(0.005f))
									continue;
								float distanceSq = (GetPosition() - pVehicle->GetPosition()).MagnitudeSqr();
								if (distanceSq < minDistance) {
									minDistance = distanceSq;
									pBus = pVehicle;
								}
							}
						}
					}

					if (pBus) {
						if (pBus->m_nNumPassengers >= pBus->m_nNumMaxPassengers - 1)
							SetObjective(OBJECTIVE_WAIT_ON_FOOT);
						else {
							GetPedAttractorManager()->DeRegisterPed(this, m_attractor);
							SetObjective(OBJECTIVE_ENTER_CAR_AS_PASSENGER, pBus);
							bDontDragMeOutCar = true;
							bRemoveMeWhenIGotIntoCar = true;
							CPlayerPed *player = FindPlayerPed();
							if (pBus->IsPassenger(player) || pBus->IsDriver(player)) {
								bCollectBusFare = true;
							}
						}
					}
				}
				break;
			case OBJECTIVE_WAIT_ON_FOOT_AT_ICE_CREAM_VAN:
			{
				SetIdle();
				CVehicle* pIceCreamVan = GetPedAttractorManager()->GetIceCreamVanForEffect(m_attractor->GetEffect());
				if (m_attractor && m_nWaitState != WAITSTATE_PLAYANIM_CHAT && pIceCreamVan && pIceCreamVan->pDriver && pIceCreamVan->pDriver->IsPlayer()) {
					int time = 5000;
					SetWaitState(WAITSTATE_PLAYANIM_CHAT, &time);
					break;
				}
				if (!m_attractor)
					break;
				CVehicle* pVan = GetPedAttractorManager()->GetIceCreamVanForEffect(m_attractor->GetEffect());
				if (!pVan)
					break;
				if (0.01f * CTimer::GetTimeStep() * 5.0f < pVan->m_fDistanceTravelled) {
					GetPedAttractorManager()->DeRegisterPed(this, m_attractor);
					break;
				}
				if (!pVan->pDriver || !pVan->pDriver->IsPlayer() || pVan->pDriver->GetPedState() == PED_ARRESTED || pVan->pDriver->GetPedState() == PED_DEAD) {
					GetPedAttractorManager()->DeRegisterPed(this, m_attractor);
					break;
				}
				if (!pVan->m_bSirenOrAlarm) {
					GetPedAttractorManager()->DeRegisterPed(this, m_attractor);
					return; // Why?
				}
				if (pVan->GetStatus() == STATUS_WRECKED) {
					GetPedAttractorManager()->DeRegisterPed(this, m_attractor);
					return; // Why?
				}
				break;
			}
		}
		if (bObjectiveCompleted
			|| m_objectiveTimer > 0 && CTimer::GetTimeInMilliseconds() > m_objectiveTimer) {
			RestorePreviousObjective();
			if (m_objectiveTimer > CTimer::GetTimeInMilliseconds() || !m_objectiveTimer)
				m_objectiveTimer = CTimer::GetTimeInMilliseconds() - 1;

			if (CharCreatedBy != RANDOM_CHAR || bInVehicle) {
				if (IsPedInControl())
					RestorePreviousState();
			} else {
				SetWanderPath(CGeneral::GetRandomNumber() & 7);
			}
			ClearAimFlag();
			ClearLookFlag();
		}
	}
}

void
CPed::SetFollowRoute(int16 currentPoint, int16 routeType)
{
	m_routeLastPoint = currentPoint;
	m_routeType = routeType;
	m_routePointsBeingPassed = 1;
	m_routePointsPassed = 0;
	m_objective = OBJECTIVE_FOLLOW_ROUTE;
	m_routeStartPoint = CRouteNode::GetRouteStart(currentPoint);
	m_nextRoutePointPos = CRouteNode::GetPointPosition(GetNextPointOnRoute());
}

int
CPed::GetNextPointOnRoute(void)
{
	int16 nextPoint = m_routePointsBeingPassed + m_routePointsPassed + m_routeStartPoint;

	// Route is complete
	if (nextPoint < 0 || nextPoint > NUMPEDROUTES || m_routeLastPoint != CRouteNode::GetRouteThisPointIsOn(nextPoint)) {

		switch (m_routeType) {
			case PEDROUTE_STOP_WHEN_DONE:
				nextPoint = -1;
				break;
			case PEDROUTE_GO_BACKWARD_WHEN_DONE:
				m_routePointsBeingPassed = -m_routePointsBeingPassed;
				nextPoint = m_routePointsBeingPassed + m_routePointsPassed + m_routeStartPoint;
				break;
			case PEDROUTE_GO_TO_START_WHEN_DONE:
				m_routePointsPassed = -1;
				nextPoint = m_routePointsBeingPassed + m_routePointsPassed + m_routeStartPoint;
				break;
			default:
				break;
		}
	}
	return nextPoint;
}

bool
CPed::HaveReachedNextPointOnRoute(float distToCountReached)
{
	if ((m_nextRoutePointPos - GetPosition()).Magnitude2D() < distToCountReached) {
		m_routePointsPassed += m_routePointsBeingPassed;
		return true;
	}
	return false;
}

bool
CPed::CanSeeEntity(CEntity *entity, float threshold)
{
	float neededAngle = CGeneral::GetRadianAngleBetweenPoints(
		entity->GetPosition().x,
		entity->GetPosition().y,
		GetPosition().x,
		GetPosition().y);

	if (neededAngle < 0.0f)
		neededAngle += TWOPI;
	else if (neededAngle > TWOPI)
		neededAngle -= TWOPI;

	float ourAngle = m_fRotationCur;
	if (ourAngle < 0.0f)
		ourAngle += TWOPI;
	else if (ourAngle > TWOPI)
		ourAngle -= TWOPI;

	float neededTurn = Abs(neededAngle - ourAngle);

	return neededTurn < threshold || TWOPI - threshold < neededTurn;
}

// Only used while deciding which gun ped should switch to, if no ammo left.
bool
CPed::SelectGunIfArmed(void)
{
	for (int i = 0; i < TOTAL_WEAPON_SLOTS; i++) {
		if (GetWeapon(i).m_nAmmoTotal > 0) {
			eWeaponType weaponType = GetWeapon(i).m_eWeaponType;

			if (weaponType == WEAPONTYPE_COLT45 || weaponType == WEAPONTYPE_UZI || weaponType == WEAPONTYPE_MP5 || weaponType == WEAPONTYPE_M4 ||
				weaponType == WEAPONTYPE_COLT45 || weaponType == WEAPONTYPE_PYTHON || weaponType == WEAPONTYPE_SHOTGUN ||
				weaponType == WEAPONTYPE_SPAS12_SHOTGUN || weaponType == WEAPONTYPE_STUBBY_SHOTGUN ||
				weaponType == WEAPONTYPE_ROCKETLAUNCHER || weaponType == WEAPONTYPE_SNIPERRIFLE || weaponType == WEAPONTYPE_FLAMETHROWER) {
				SetCurrentWeapon(i);
				return true;
			}
		}
	}
	SetCurrentWeapon(WEAPONTYPE_UNARMED);
	return false;
}
void
CPed::ReactToPointGun(CEntity *entWithGun)
{
	CPed *pedWithGun = (CPed*)entWithGun;
	int waitTime;

	if (IsPlayer() || !IsPedInControl() || (CharCreatedBy == MISSION_CHAR && !bCrouchWhenScared))
		return;

	if (m_leader == pedWithGun)
		return;

	if (m_nWaitState == WAITSTATE_PLAYANIM_HANDSUP || m_nWaitState == WAITSTATE_PLAYANIM_HANDSCOWER ||
		(GetPosition() - pedWithGun->GetPosition()).MagnitudeSqr2D() > 225.0f || (m_nPedType == PEDTYPE_GANG7 && pedWithGun == FindPlayerPed()))
		return;

	if (m_leader) {
		if (FindPlayerPed() == m_leader)
			return;

		ClearLeader();
	}
	if (m_pedStats->m_flags & STAT_GUN_PANIC
		&& (m_nPedState != PED_ATTACK || GetWeapon()->IsTypeMelee())
		&& m_nPedState != PED_FLEE_ENTITY && m_nPedState != PED_AIMGUN) {

		waitTime = CGeneral::GetRandomNumberInRange(3000, 6000);
		SetWaitState(WAITSTATE_PLAYANIM_HANDSCOWER, &waitTime);
		Say(SOUND_PED_HANDS_COWER);
		m_pLookTarget = pedWithGun;
		m_pLookTarget->RegisterReference((CEntity **) &m_pLookTarget);
		SetMoveState(PEDMOVE_NONE);

	} else if (m_nPedType != pedWithGun->m_nPedType) {
		if (IsGangMember() || m_nPedType == PEDTYPE_MEDIC || m_nPedType == PEDTYPE_FIREMAN) {
			RegisterThreatWithGangPeds(pedWithGun);
		}

		if (m_nPedType == PEDTYPE_COP) {
			if (pedWithGun->IsPlayer()) {
				((CPlayerPed*)pedWithGun)->Wanted()->SetWantedLevelNoDrop(2);
				if (bCrouchWhenShooting || bKindaStayInSamePlace) {
					SetDuck(CGeneral::GetRandomNumberInRange(1000, 3000));
					return;
				}
			}
		}

		if (m_nPedType != PEDTYPE_COP
			&& (m_nPedState != PED_ATTACK || GetWeapon()->IsTypeMelee())
			&& (m_nPedState != PED_FLEE_ENTITY || pedWithGun->IsPlayer() && m_fleeFrom != pedWithGun)
			&& m_nPedState != PED_AIMGUN && m_objective != OBJECTIVE_KILL_CHAR_ON_FOOT) {

			waitTime = CGeneral::GetRandomNumberInRange(3000, 6000);
			SetWaitState(WAITSTATE_PLAYANIM_HANDSUP, &waitTime);
			Say(SOUND_PED_HANDS_UP);
			m_pLookTarget = pedWithGun;
			m_pLookTarget->RegisterReference((CEntity **) &m_pLookTarget);
			SetMoveState(PEDMOVE_NONE);
			if (m_nPedState == PED_FLEE_ENTITY) {
				m_fleeFrom = pedWithGun;
				m_fleeFrom->RegisterReference((CEntity **) &m_fleeFrom);
			}

			if (FindPlayerPed() == pedWithGun && bRichFromMugging) {
				int money = CGeneral::GetRandomNumberInRange(100, 300);
				int pickupCount = money / 40 + 1;
				int moneyPerPickup = money / pickupCount;

				for (int i = 0; i < pickupCount; i++) {
					float pickupX = 1.5f * Sin((CGeneral::GetRandomNumber() % 256)/256.0f * TWOPI) + GetPosition().x;
					float pickupY = 1.5f * Cos((CGeneral::GetRandomNumber() % 256)/256.0f * TWOPI) + GetPosition().y;
					bool found = false;
					float groundZ = CWorld::FindGroundZFor3DCoord(pickupX, pickupY, GetPosition().z, &found) + 0.5f;
					if (found) {
						CPickups::GenerateNewOne(CVector(pickupX, pickupY, groundZ), MI_MONEY, PICKUP_MONEY, moneyPerPickup + (CGeneral::GetRandomNumber() & 7));
					}
				}
				bRichFromMugging = false;
			}
		}
	}
}

void
CPed::ReactToAttack(CEntity *attacker)
{
	if (IsPlayer() && attacker->IsPed()) {
		InformMyGangOfAttack(attacker);
		SetLookFlag(attacker, true);
		SetLookTimer(700);
		return;
	}
	
	if (m_nPedType == PEDTYPE_GANG7 && attacker->IsPed() && ((CPed*)attacker)->IsPlayer()) {
		if (m_nPedState != PED_FALL) {
			SetFall(15000, (AnimationId)(ANIM_ID_KO_SHOT_FRONT_0 + CGeneral::GetRandomNumberInRange(0, 5)), 0);
		}

	} else if (m_nPedState == PED_DRIVING && InVehicle()
		&& (m_pMyVehicle->pDriver == this || m_pMyVehicle->pDriver && m_pMyVehicle->pDriver->m_nPedState == PED_DRIVING && m_pMyVehicle->pDriver->m_objective != OBJECTIVE_LEAVE_CAR_AND_DIE)) {

		if (m_pMyVehicle->VehicleCreatedBy == RANDOM_VEHICLE
			&& (m_pMyVehicle->GetStatus() == STATUS_SIMPLE || m_pMyVehicle->GetStatus() == STATUS_PHYSICS)
			&& m_pMyVehicle->AutoPilot.m_nCarMission == MISSION_CRUISE) {

			CCarCtrl::SwitchVehicleToRealPhysics(m_pMyVehicle);
			m_pMyVehicle->AutoPilot.m_nDrivingStyle = DRIVINGSTYLE_AVOID_CARS;
			m_pMyVehicle->AutoPilot.m_nCruiseSpeed = GAME_SPEED_TO_CARAI_SPEED * m_pMyVehicle->m_pHandlingData->m_Transmission.m_fMaxCruiseVelocity;
			m_pMyVehicle->SetStatus(STATUS_PHYSICS);
		}

	} else if ((IsPedInControl() || m_nPedState == PED_DRIVING) && (CharCreatedBy != MISSION_CHAR || bRespondsToThreats)) {
		if (m_leader != attacker && (!m_leader || FindPlayerPed() != m_leader) && attacker->IsPed()) {

			CPed *attackerPed = (CPed*)attacker; 
			if (bNotAllowedToDuck) {
				if (!attackerPed->GetWeapon()->IsTypeMelee()) {
					m_duckAndCoverTimer = CTimer::GetTimeInMilliseconds();
					return;
				}
			} else if (bCrouchWhenShooting || bKindaStayInSamePlace) {
				SetDuck(CGeneral::GetRandomNumberInRange(1000,3000));
				return;
			}

			if (m_nWaitState == WAITSTATE_STRIPPER) {
				ClearWaitState();
				SetObjective(OBJECTIVE_KILL_CHAR_ON_FOOT, attacker);
				SetObjectiveTimer(20000);

			} else {
				if (m_pedStats->m_nFear <= 100 - attackerPed->m_pedStats->m_nTemper) {
					if (m_pedStats != attackerPed->m_pedStats) {
						if (IsGangMember() || m_nPedType == PEDTYPE_MEDIC || m_nPedType == PEDTYPE_FIREMAN) {
							RegisterThreatWithGangPeds(attackerPed);
						}
						if (!attackerPed->GetWeapon()->IsTypeMelee() && GetWeapon()->IsTypeMelee()) {
							SetObjective(OBJECTIVE_FLEE_CHAR_ON_FOOT_TILL_SAFE, attacker);
							SetMoveState(PEDMOVE_RUN);
						} else {
							SetObjective(OBJECTIVE_KILL_CHAR_ON_FOOT, attacker);
							SetObjectiveTimer(20000);
						}
					}
				} else {
					SetObjective(OBJECTIVE_FLEE_CHAR_ON_FOOT_TILL_SAFE, attackerPed);
					SetMoveState(PEDMOVE_RUN);
					if (attackerPed->GetWeapon()->IsTypeMelee())
						Say(SOUND_PED_FLEE_RUN);
				}
			}
		}
	}
}

// The root translation of a vehicle clip is how far the ped travels
// over the course of it; door positions are built from these.
static CVector
GetAnimationLastTranslation(AssocGroupId groupId, AnimationId animId)
{
	CAnimBlendStaticAssociation *sa = CAnimManager::GetAnimAssociation(groupId, animId);
	CAnimBlendHierarchy *hier = sa ? sa->m_BlendHier : nil;
	if (hier == nil || hier->m_pSequences == nil || hier->m_nSeqCount <= 0)
		return CVector(0.0f, 0.0f, 0.0f);
	CAnimManager::UncompressAnimation(hier);
	// SA IFPs can put non-root sequences before the root. Never use an
	// arm's translation to align a ped with a vehicle door.
	CAnimBlendSequence *root = nil;
	for (int i = 0; i < hier->m_nSeqCount; i++) {
		CAnimBlendSequence *seq = &hier->m_pSequences[i];
		if (seq->IsUsingBoneTag() && seq->GetBoneTag() == BONE_ROOT) {
			root = seq;
			break;
		}
	}
	if (root == nil)
		root = hier->m_pSequences; // native VC's original first-sequence convention
	if (root->m_FramesNum <= 0 || !root->HasTranslation())
		return CVector(0.0f, 0.0f, 0.0f);
	return root->GetUKeyFrame(root->m_FramesNum - 1)->Trans;
}

static CVector
GetPedAnimationOffset(CPed *ped, AssocGroupId groupId, AnimationId animId, const CVector &nativeOffset)
{
	(void)ped;
	CVector fromAnim = GetAnimationLastTranslation(groupId, animId);
	return fromAnim.MagnitudeSqr() > 0.0f ? fromAnim : nativeOffset;
}

void
CPed::QuitEnteringCar(void)
{
	// Entry is task-based now: quitting means telling the enter task
	// to stop. Its destructor hands the door flags and the getting-in
	// count back, so there is nothing else to unwind here.
	if (m_pVehicleAnim)
		m_pVehicleAnim->m_BlendDelta = -1000.0f;
	m_pVehicleAnim = nil;

	if (GetIntelligence() != nil) {
		static const eTaskType enterTasks[] = {
			TASK_COMPLEX_ENTER_CAR_AS_DRIVER, TASK_COMPLEX_ENTER_CAR_AS_PASSENGER,
			TASK_COMPLEX_ENTER_BOAT_AS_DRIVER, TASK_COMPLEX_ENTER_TRAIN_AS_PASSENGER
		};
		for (int i = 0; i < (int)ARRAY_SIZE(enterTasks); i++) {
			if (CTask *task = GetIntelligence()->FindTaskByType(enterTasks[i]))
				task->MakeAbortable(this, ABORT_PRIORITY_URGENT, nil);
		}
	}

	// The objective layer must not hand the entry straight back.
	if (m_objective == OBJECTIVE_ENTER_CAR_AS_DRIVER || m_objective == OBJECTIVE_ENTER_CAR_AS_PASSENGER)
		RestorePreviousObjective();

	bUsesCollision = true;
	if (!DyingOrDead())
		SetIdle();
}

bool
CPed::EnteringCar(void)
{
	if (GetIntelligence() == nil)
		return false;
	return GetIntelligence()->FindTaskByType(TASK_COMPLEX_ENTER_CAR_AS_DRIVER) != nil ||
		GetIntelligence()->FindTaskByType(TASK_COMPLEX_ENTER_CAR_AS_PASSENGER) != nil ||
		GetIntelligence()->FindTaskByType(TASK_COMPLEX_ENTER_BOAT_AS_DRIVER) != nil ||
		GetIntelligence()->FindTaskByType(TASK_COMPLEX_ENTER_TRAIN_AS_PASSENGER) != nil;
}

bool
CPed::LeavingCar(void)
{
	if (GetIntelligence() == nil)
		return false;
	return GetIntelligence()->FindTaskByType(TASK_COMPLEX_LEAVE_CAR) != nil ||
		GetIntelligence()->FindTaskByType(TASK_COMPLEX_LEAVE_BOAT) != nil;
}

bool
CPed::BeingDraggedOutOfCar(void)
{
	if (GetIntelligence() == nil)
		return false;
	return GetIntelligence()->FindTaskByType(TASK_COMPLEX_CAR_SLOW_BE_DRAGGED_OUT) != nil ||
		GetIntelligence()->FindTaskByType(TASK_COMPLEX_CAR_SLOW_BE_DRAGGED_OUT_AND_STAND_UP) != nil ||
		GetIntelligence()->FindTaskByType(TASK_SIMPLE_CAR_SLOW_BE_DRAGGED_OUT) != nil;
}

void
CPed::SetFormation(eFormation type)
{
	// FIX: Formations in GetFormationPosition were in range 1-8, whereas in here it's 0-7.
	//      To not change the behaviour, range in here tweaked by 1 with the use of enum.

	switch (m_pedFormation) {
		case FORMATION_REAR:
		case FORMATION_REAR_LEFT:
		case FORMATION_REAR_RIGHT:
		case FORMATION_FRONT_LEFT:
		case FORMATION_FRONT_RIGHT:
		case FORMATION_LEFT:
		case FORMATION_RIGHT:
		case FORMATION_FRONT:
			break;
		default:
			Error("Unknown formation type, PedAI.cpp");
			break;
	}
	m_pedFormation = type;
}

CVector
CPed::GetFormationPosition(void)
{
	if (!m_pedInObjective)
		return GetPosition();

	if (m_pedInObjective->m_nPedState == PED_DEAD) {
		if (!m_pedInObjective->m_pedInObjective) {
			m_pedInObjective = nil;
			return GetPosition();
		}
		m_pedInObjective = m_pedInObjective->m_pedInObjective;
	}

	CVector formationOffset;
	float offset = CGeneral::GetRandomNumberInRange(1.f, 1.25f) * 1.75f;
	switch (m_pedFormation) {
		case FORMATION_REAR:
			formationOffset = CVector(0.0f, -offset, 0.0f);
			break;
		case FORMATION_REAR_LEFT:
			formationOffset = CVector(-offset, -offset, 0.0f);
			break;
		case FORMATION_REAR_RIGHT:
			formationOffset = CVector(offset, -offset, 0.0f);
			break;
		case FORMATION_FRONT_LEFT:
			formationOffset = CVector(-offset, offset, 0.0f);
			break;
		case FORMATION_FRONT_RIGHT:
			formationOffset = CVector(offset, offset, 0.0f);
			break;
		case FORMATION_LEFT:
			formationOffset = CVector(-offset, 0.0f, 0.0f);
			break;
		case FORMATION_RIGHT:
			formationOffset = CVector(offset, 0.0f, 0.0f);
			break;
		case FORMATION_FRONT:
			formationOffset = CVector(0.0f, offset, 0.0f);
			break;
		default:
			formationOffset = CVector(0.0f, 0.0f, 0.0f);
			break;
	}
	return m_pedInObjective->GetMatrix() * formationOffset;
}

void
CPed::LineUpPedWithCar(PedLineUpPhase phase)
{
	bool vehIsUpsideDown = false;
	bool stillGettingInOut = false;
	int vehAnim = ANIM_ID_UNDEFINED;
	float seatPosMult = 0.0f;
	float currentZ;
	float adjustedTimeStep;
	CVector autoZPos;

	if (CReplay::IsPlayingBack())
		return;

	if (!bChangedSeat && phase != LINE_UP_TO_CAR_2) {
		if (m_pMyVehicle->IsBike()) {
			if (RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_BIKE_RIDE) ||
				RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_BIKE_PASSENGER)) {
				SetPedPositionInCar();
				return;
			}
		} else {
			if (RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_CAR_SIT)) {
				SetPedPositionInCar();
				return;
			}
			if (RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_CAR_LSIT)) {
				SetPedPositionInCar();
				return;
			}
			if (RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_CAR_SIT_WEAK)) {
				SetPedPositionInCar();
				return;
			}
			if (RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_CAR_SIT_PRO)) {
				SetPedPositionInCar();
				return;
			}
		}
		bChangedSeat = true;
	}
	if (phase == LINE_UP_TO_CAR_FALL) {
		SetPedPositionInCar();
		autoZPos = GetPosition();
		CPedPlacement::FindZCoorForPed(&autoZPos);
		if (m_pVehicleAnim && (m_pVehicleAnim->m_AnimId == ANIM_ID_DEFAULT_CAR_ROLLOUT_LHS || m_pVehicleAnim->m_AnimId == ANIM_ID_DEFAULT_CAR_ROLLOUT_LHS)
			&& autoZPos.z > GetPosition().z) {
			m_matrix.GetPosition().z = autoZPos.z;
		}
		if (m_pVehicleAnim && m_pVehicleAnim->m_AnimId == ANIM_ID_BIKE_HIT) {
			if (autoZPos.z > GetPosition().z)
				m_matrix.GetPosition().z += m_pVehicleAnim->GetTimeProgress() * (autoZPos.z - GetPosition().z);

		} else if (m_pVehicleAnim) {
			if (m_pVehicleAnim->m_AnimId == ANIM_ID_UNKNOWN_26) {
				if (autoZPos.z > GetPosition().z) {
					m_matrix.GetPosition().z += (m_pVehicleAnim->m_CurrentTime * (20.f / 7.f)) * (autoZPos.z - GetPosition().z);
				}
			}
		}
		return;
	}
	if (phase == LINE_UP_TO_CAR_START) {
		m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f);
	}
	CVehicle *veh = m_pMyVehicle;

	// Not quite right, IsUpsideDown func. checks for <= -0.9f.
	if (veh->GetUp().z <= -0.8f)
		vehIsUpsideDown = true;

	if (m_vehDoor == CAR_DOOR_RF || m_vehDoor == CAR_DOOR_RR) {
		if (vehIsUpsideDown) {
			m_fRotationDest = -PI + veh->GetForward().Heading();
		} else if (veh->bIsBus) {
			m_fRotationDest = 0.5f * PI + veh->GetForward().Heading();
		} else {
			m_fRotationDest = veh->GetForward().Heading();
		}
	} else if (m_vehDoor == CAR_DOOR_LF || m_vehDoor == CAR_DOOR_LR) {
		if (vehIsUpsideDown) {
			m_fRotationDest = veh->GetForward().Heading();
		} else if (veh->bIsBus) {
			m_fRotationDest = -0.5f * PI + veh->GetForward().Heading();
		} else {
			m_fRotationDest = veh->GetForward().Heading();
		}
	} else {
		// I don't know will this part ever run(maybe boats?), but the game also handles that. I don't know is it intentional.

		if (vehIsUpsideDown) {
			m_fRotationDest = veh->GetForward().Heading();
		} else if (veh->bIsBus) {
			m_fRotationDest = 0.5f * PI + veh->GetForward().Heading();
		} else if (m_vehDoor == CAR_WINDSCREEN) {
			m_fRotationDest = veh->GetForward().Heading() + PI;
		} else {
			m_fRotationDest = veh->GetForward().Heading();
		}
	}

	bool multExtractedFromAnim = false;
	bool multExtractedFromAnimBus = false;
	float zBlend;
	if (m_pVehicleAnim) {
		vehAnim = m_pVehicleAnim->m_AnimId;

		switch (vehAnim) {
			case ANIM_ID_CAR_JACKEDRHS:
			case ANIM_ID_CAR_JACKEDLHS:
			case ANIM_ID_CAR_GETIN_LHS_1:
			case ANIM_ID_CAR_GETIN_RHS_1:
				multExtractedFromAnim = true;
				zBlend = Max(m_pVehicleAnim->GetTimeProgress() - 0.3f, 0.0f) / (1.0f - 0.3f);
				// fall through

			case ANIM_ID_CAR_CLOSEDOOR_LHS_0:
			case ANIM_ID_CAR_GETOUT_LHS_0:
			case ANIM_ID_CAR_GETOUT_RHS_0:

				if (!multExtractedFromAnim) {
					multExtractedFromAnim = true;
					zBlend = Max(m_pVehicleAnim->GetTimeProgress() - 0.5f, 0.0f) / (1.0f - 0.5f);
				}
				// fall through

			case ANIM_ID_DEFAULT_CAR_CRAWLOUTRHS_0:
			case ANIM_ID_CAR_GETOUT_LHS_1:
			case ANIM_ID_CAR_GETOUT_RHS_1:
				seatPosMult = m_pVehicleAnim->GetTimeProgress();
				break;
			case ANIM_ID_CAR_GETIN_RHS_0:
			case ANIM_ID_CAR_GETIN_LHS_0:
				if (veh && veh->IsAutomobile() && veh->bIsBus) {
					multExtractedFromAnimBus = true;
					zBlend = Min(m_pVehicleAnim->GetTimeProgress(), 0.5f) / 0.5f;
				}
				// fall through

			case ANIM_ID_CAR_OPEN_LHS:
			case ANIM_ID_DRIVE_BOAT:
				seatPosMult = (m_pVehicleAnim->m_BlendHier->m_fTotalTime - m_pVehicleAnim->m_CurrentTime) / m_pVehicleAnim->m_BlendHier->m_fTotalTime;
				break;
			case ANIM_ID_CAR_CLOSEDOOR_RHS_0:
			case ANIM_ID_CAR_SHUFFLE_RHS_0:
				seatPosMult = 0.0f;
				break;
			{
				float animLength = m_pVehicleAnim->m_BlendHier->m_fTotalTime;
				seatPosMult = Max(0.0f, 0.5f * animLength - m_pVehicleAnim->m_CurrentTime) / animLength;
				break;
			}
			case ANIM_ID_CAR_CLOSE_LHS_0:
			case ANIM_ID_CAR_CLOSE_RHS_0:
				seatPosMult = 1.0f;
				break;
			default:
				if (veh->IsBike()) {
					seatPosMult = 0.0f;
				} else {
					if (bInVehicle)
						seatPosMult = 0.0f;
					else
						seatPosMult = 1.0f;
				}
				break;
		}
	} else {
		if (veh->IsBike()) {
			seatPosMult = 0.0f;
		} else {
			if (bInVehicle)
				seatPosMult = 0.0f;
			else
				seatPosMult = 1.0f;
		}
	}

	CVector neededPos;

	if (phase == LINE_UP_TO_CAR_2) {
		neededPos = GetPosition();
	} else {
		neededPos = GetPositionToOpenCarDoor(veh, m_vehDoor, seatPosMult, this);
	}

	autoZPos = neededPos;

	if (veh->bIsInWater) {
		if (veh->m_nVehicleType == VEHICLE_TYPE_BOAT && veh->IsUpsideDown())
			autoZPos.z += 1.0f;
	} else {
		CPedPlacement::FindZCoorForPed(&autoZPos);
	}

	if (phase == LINE_UP_TO_CAR_END || phase == LINE_UP_TO_CAR_2) {
		neededPos.z = GetPosition().z;

		// Getting out
		if (!veh->bIsBus || (veh->bIsBus && vehIsUpsideDown)) {
			float nextZSpeed = m_vecMoveSpeed.z - GRAVITY * CTimer::GetTimeStep();

			// If we're not in ground at next step, apply animation
			if (neededPos.z + nextZSpeed >= autoZPos.z) {
				m_vecMoveSpeed.z = nextZSpeed;
				ApplyMoveSpeed();
				// Removing below line breaks the animation
				neededPos.z = GetPosition().z;
			} else {
				neededPos.z = autoZPos.z;
				m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f);
			}
		}
	}

	if (autoZPos.z > neededPos.z) {
		if(m_pVehicleAnim) vehAnim = m_pVehicleAnim->m_AnimId;
		if (veh->IsBike() && m_pVehicleAnim && vehAnim != ANIM_ID_CAR_GETIN_BIKE_FRONT) {
			float zBlend;
			if (vehAnim != ANIM_ID_CAR_GETOUT_LHS_0 && vehAnim != ANIM_ID_CAR_GETOUT_RHS_0) {
				if (vehAnim != ANIM_ID_CAR_GETIN_LHS_0 && vehAnim != ANIM_ID_CAR_GETIN_RHS_0) {
					zBlend = 0.0f;
				} else {
					float animLength = m_pVehicleAnim->m_BlendHier->m_fTotalTime;
					zBlend = Min(1.0f, 2.0f * m_pVehicleAnim->m_CurrentTime / animLength);
				}
			} else {
				zBlend = 1.0f - seatPosMult;
			}
			float curZ = veh->GetPosition().z + FEET_OFFSET;
			neededPos.z = ((curZ - autoZPos.z) - veh->GetHeightAboveRoad()) * zBlend + autoZPos.z;
		} else if (multExtractedFromAnim) {
			neededPos.z += (autoZPos.z - neededPos.z) * zBlend;
		} else {
			currentZ = GetPosition().z;
			if (m_pVehicleAnim && vehAnim != ANIM_ID_CAR_GETIN_LHS_1 && vehAnim != ANIM_ID_CAR_CLOSEDOOR_LHS_1 && vehAnim != ANIM_ID_CAR_CLOSEDOOR_RHS_1 && vehAnim != ANIM_ID_CAR_GETIN_RHS_1) {
				neededPos.z = autoZPos.z;
				m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f);
			} else if (neededPos.z < currentZ && m_pVehicleAnim && vehAnim != ANIM_ID_CAR_CLOSEDOOR_LHS_1 && vehAnim != ANIM_ID_CAR_CLOSEDOOR_RHS_1) {
				adjustedTimeStep = Max(m_pVehicleAnim->m_TimeStep, 0.1f);

				// Smoothly change ped position
				neededPos.z = currentZ - (currentZ - neededPos.z) / ((m_pVehicleAnim->m_BlendHier->m_fTotalTime - m_pVehicleAnim->m_CurrentTime) / adjustedTimeStep);
			}
		}
	} else {
		// We may need to raise up the ped
		if (phase == LINE_UP_TO_CAR_START) {
			currentZ = GetPosition().z;

			if (neededPos.z > currentZ) {
				if (multExtractedFromAnimBus) {
					neededPos.z = (neededPos.z - currentZ) * zBlend + currentZ;
				} else {
					if (m_pVehicleAnim &&
						(vehAnim == ANIM_ID_CAR_GETIN_RHS_0 || vehAnim == ANIM_ID_CAR_GETIN_RHS_0 || vehAnim == ANIM_ID_CAR_GETIN_LHS_0 || vehAnim == ANIM_ID_CAR_GETIN_LHS_0
							|| vehAnim == ANIM_ID_CAR_OPEN_LHS || vehAnim == ANIM_ID_CAR_GETIN_LHS_1 || vehAnim == ANIM_ID_CAR_GETIN_RHS_1)) {
						adjustedTimeStep = Max(m_pVehicleAnim->m_TimeStep, 0.1f);

						// Smoothly change ped position
						neededPos.z = (neededPos.z - currentZ) / ((m_pVehicleAnim->m_BlendHier->m_fTotalTime - m_pVehicleAnim->m_CurrentTime) / adjustedTimeStep) + currentZ;
					} else if (EnteringCar() || m_nPedState == PED_DRIVING && veh->IsBike()) {
						neededPos.z = Max(currentZ, autoZPos.z);
					}
				}
			}
		}
	}

	if (CTimer::GetTimeInMilliseconds() < m_nPedStateTimer)
		stillGettingInOut = veh->m_nVehicleType != VEHICLE_TYPE_BOAT || bOnBoat;

	if (!stillGettingInOut) {
		m_fRotationCur = m_fRotationDest;
	} else {
		float limitedDest = CGeneral::LimitRadianAngle(m_fRotationDest);
		float timeUntilStateChange = (m_nPedStateTimer - CTimer::GetTimeInMilliseconds())/600.0f;

		if (timeUntilStateChange <= 0.0f) {
			m_vecOffsetSeek.x = 0.0f;
			m_vecOffsetSeek.y = 0.0f;
		}
		m_vecOffsetSeek.z = 0.0f;

		neededPos -= timeUntilStateChange * m_vecOffsetSeek;

		if (PI + m_fRotationCur < limitedDest) {
			limitedDest -= 2 * PI;
		} else if (m_fRotationCur - PI > limitedDest) {
			limitedDest += 2 * PI;
		}
		m_fRotationCur -= (m_fRotationCur - limitedDest) * (1.0f - timeUntilStateChange);
	}

	if (seatPosMult > 0.2f || vehIsUpsideDown || veh->IsBike()) {
		SetPosition(neededPos);
		SetHeading(m_fRotationCur);
	} else {
		CMatrix vehDoorMat(veh->GetMatrix());
		vehDoorMat.GetPosition() += Multiply3x3(vehDoorMat, GetLocalPositionToOpenCarDoor(veh, m_vehDoor, 0.0f, this));

		if (m_vehDoor == CAR_WINDSCREEN || veh->bIsBus) {
			CMatrix correctionMat;
			if (veh->bIsBus && (m_vehDoor == CAR_DOOR_LF || m_vehDoor == CAR_DOOR_LR))
				correctionMat.SetRotateZ(-HALFPI);
			else if (veh->bIsBus && (m_vehDoor == CAR_DOOR_RF || m_vehDoor == CAR_DOOR_RR))
				correctionMat.SetRotateZ(HALFPI);
			else
				correctionMat.SetRotateZ(PI);

			vehDoorMat = vehDoorMat * correctionMat;
		}
		GetMatrix() = vehDoorMat;
	}

}

void
CPed::PedAnimDoorCloseRollingCB(CAnimBlendAssociation *animAssoc, void *arg)
{
	CPed *ped = (CPed *)arg;

	CAutomobile *veh = (CAutomobile *)(ped->m_pMyVehicle);

	if (animAssoc)
		animAssoc->m_BlendDelta = -1000.0f;

	veh->ProcessOpenDoor(CAR_DOOR_LF, ANIM_ID_CAR_ROLLDOOR, 1.0f);

	veh->m_nGettingOutFlags &= ~CAR_DOOR_FLAG_LF;

	if (veh->Damage.GetDoorStatus(DOOR_FRONT_LEFT) == DOOR_STATUS_SWINGING)
		veh->Damage.SetDoorStatus(DOOR_FRONT_LEFT, DOOR_STATUS_OK);
}

CVector
CPed::GetPositionToOpenCarDoor(CVehicle *veh, uint32 component, CPed *ped)
{
	CVector doorPos;
	CVector vehDoorOffset;
	CVehicleModelInfo* vehModel = veh->GetModelInfo();

	if (veh->IsBike()) {
		CBike* bike = (CBike*)veh;
		CVector kickOffset = GetPedAnimationOffset(ped, ANIM_GROUP_HARLEYBIKEANIMS, ANIM_ID_CAR_GETIN_BIKE_FRONT, vecPedBikeKickAnimOffset);

		if (component == CAR_WINDSCREEN) {
			doorPos = vehModel->GetFrontSeatPosn();
			return bike->GetMatrix() * (doorPos +
				CVector(-kickOffset.x, kickOffset.y, -kickOffset.z));
		} else {
			switch (GetBikeEnterAnimGroup(veh, ANIM_ID_CAR_GETIN_LHS_0)) {
			case ANIM_GROUP_VESPABIKEANIMS:
				vehDoorOffset = GetPedAnimationOffset(ped, ANIM_GROUP_VESPABIKEANIMS, ANIM_ID_CAR_GETIN_LHS_0, vecPedVespaBikeJumpRhsAnimOffset);
				break;
			case ANIM_GROUP_HARLEYBIKEANIMS:
				vehDoorOffset = GetPedAnimationOffset(ped, ANIM_GROUP_HARLEYBIKEANIMS, ANIM_ID_CAR_GETIN_LHS_0, vecPedHarleyBikeJumpRhsAnimOffset);
				break;
			case ANIM_GROUP_DIRTBIKEANIMS:
				vehDoorOffset = GetPedAnimationOffset(ped, ANIM_GROUP_DIRTBIKEANIMS, ANIM_ID_CAR_GETIN_LHS_0, vecPedDirtBikeJumpRhsAnimOffset);
				break;
			default:
				vehDoorOffset = GetPedAnimationOffset(ped, ANIM_GROUP_STDBIKEANIMS, ANIM_ID_CAR_GETIN_LHS_0, vecPedStdBikeJumpRhsAnimOffset);
				break;
			}
		}

		doorPos = vehModel->GetFrontSeatPosn();
		if (component == CAR_DOOR_LR || component == CAR_DOOR_RR) {
			doorPos = vehModel->m_positions[CAR_POS_BACKSEAT];
		}

		if (component == CAR_DOOR_LR || component == CAR_DOOR_LF) {
			vehDoorOffset.x *= -1.f;
		}

		CVector correctedPos;
		bike->GetCorrectedWorldDoorPosition(correctedPos, vehDoorOffset, doorPos);
		return correctedPos;
	} else {
		float seatOffset;
		if (veh->bIsVan && (component == CAR_DOOR_LR || component == CAR_DOOR_RR)) {
			seatOffset = 0.0f;
			vehDoorOffset = GetPedAnimationOffset(ped, ANIM_GROUP_VANCARANIMS, ANIM_ID_CAR_GETIN_LHS_1, vecPedVanRearDoorAnimOffset);
		} else {
			seatOffset = veh->m_pHandlingData->m_fSeatOffsetDistance;
			if (veh->bLowVehicle) {
				vehDoorOffset = GetPedAnimationOffset(ped, ANIM_GROUP_LOWCARAMIMS, ANIM_ID_CAR_GETIN_LHS_0, vecPedCarDoorLoAnimOffset);
			} else {
				vehDoorOffset = GetPedAnimationOffset(ped, ANIM_GROUP_STDCARAMIMS, ANIM_ID_CAR_GETIN_LHS_0, vecPedCarDoorAnimOffset);
			}
		}

		switch (component) {
		case CAR_DOOR_RF:
			doorPos = vehModel->GetFrontSeatPosn();
			doorPos.x += seatOffset;
			vehDoorOffset.x = -vehDoorOffset.x;
			break;

		case CAR_DOOR_RR:
			doorPos = vehModel->m_positions[CAR_POS_BACKSEAT];
			doorPos.x += seatOffset;
			vehDoorOffset.x = -vehDoorOffset.x;
			break;

		case CAR_DOOR_LF:
			doorPos = vehModel->GetFrontSeatPosn();
			doorPos.x += seatOffset;
			doorPos.x = -doorPos.x;
			break;

		case CAR_DOOR_LR:
			doorPos = vehModel->m_positions[CAR_POS_BACKSEAT];
			doorPos.x += seatOffset;
			doorPos.x = -doorPos.x;
			break;

		default:
			doorPos = vehModel->GetFrontSeatPosn();
			vehDoorOffset = CVector(0.0f, 0.0f, 0.0f);
			break;
		}

		CVector diffVec = doorPos - vehDoorOffset;
		return Multiply3x3(veh->GetMatrix(), diffVec) + veh->GetPosition();

		//unused
		//doorPos = Multiply3x3(veh->GetMatrix(), doorPos) + veh->GetMatrix();
	}
}




// Unused






uint8
CPed::GetNearestTrainDoor(CVehicle *train, CVector &doorPos)
{
	GetNearestTrainPedPosition(train, doorPos);
/*
	// Not used.
	CVehicleModelInfo* trainModel = (CVehicleModelInfo*)CModelInfo::GetModelInfo(train->m_modelIndex);
	CMatrix trainMat = CMatrix(train->GetMatrix());

	doorPos = trainModel->m_positions[m_vehDoor];
	doorPos.x -= 1.5f;
	doorPos = Multiply3x3(trainMat, doorPos);
	doorPos += train->GetPosition();
*/
	return 1;
}

uint8
CPed::GetNearestTrainPedPosition(CVehicle *train, CVector &enterPos)
{
	CVector enterStepOffset;
	CVehicleModelInfo *trainModel = (CVehicleModelInfo *)CModelInfo::GetModelInfo(train->GetModelIndex());
	CMatrix trainMat = CMatrix(train->GetMatrix());
	CVector leftEntryPos, rightEntryPos, midEntryPos;
	float distLeftEntry, distRightEntry, distMidEntry;

	// enterStepOffset = vecPedCarDoorAnimOffset;
	enterStepOffset = CVector(1.5f, 0.0f, 0.0f);

	if (train->pPassengers[TRAIN_POS_LEFT_ENTRY]) {
		distLeftEntry = 999.0f;
	} else {
		leftEntryPos = trainModel->m_positions[TRAIN_POS_LEFT_ENTRY] - enterStepOffset;
		leftEntryPos = Multiply3x3(trainMat, leftEntryPos);
		leftEntryPos += train->GetPosition();
		distLeftEntry = (leftEntryPos - GetPosition()).Magnitude();
	}

	if (train->pPassengers[TRAIN_POS_MID_ENTRY]) {
		distMidEntry = 999.0f;
	} else {
		midEntryPos = trainModel->m_positions[TRAIN_POS_MID_ENTRY] - enterStepOffset;
		midEntryPos = Multiply3x3(trainMat, midEntryPos);
		midEntryPos += train->GetPosition();
		distMidEntry = (midEntryPos - GetPosition()).Magnitude();
	}

	if (train->pPassengers[TRAIN_POS_RIGHT_ENTRY]) {
		distRightEntry = 999.0f;
	} else {
		rightEntryPos = trainModel->m_positions[TRAIN_POS_RIGHT_ENTRY] - enterStepOffset;
		rightEntryPos = Multiply3x3(trainMat, rightEntryPos);
		rightEntryPos += train->GetPosition();
		distRightEntry = (rightEntryPos - GetPosition()).Magnitude();
	}

	if (distMidEntry < distLeftEntry) {
		if (distMidEntry < distRightEntry) {
			enterPos = midEntryPos;
			m_vehDoor = TRAIN_POS_MID_ENTRY;
		} else {
			enterPos = rightEntryPos;
			m_vehDoor = TRAIN_POS_RIGHT_ENTRY;
		}
	} else if (distRightEntry < distLeftEntry) {
		enterPos = rightEntryPos;
		m_vehDoor = TRAIN_POS_RIGHT_ENTRY;
	} else {
		enterPos = leftEntryPos;
		m_vehDoor = TRAIN_POS_LEFT_ENTRY;
	}

	return 1;
}




void
CPed::SetPedPositionInTrain(void)
{
	LineUpPedWithTrain();
}

void
CPed::LineUpPedWithTrain(void)
{
	CVector lineUpPos;
	CVehicleModelInfo *trainModel = (CVehicleModelInfo *)CModelInfo::GetModelInfo(m_pMyVehicle->GetModelIndex());
	CVector enterOffset(1.5f, 0.0f, -0.2f);

	m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f);
	m_fRotationCur = m_pMyVehicle->GetForward().Heading() - HALFPI;
	m_fRotationDest = m_fRotationCur;

	if (!bInVehicle) {
		GetNearestTrainDoor(m_pMyVehicle, lineUpPos);
		lineUpPos.z += 0.2f;
	} else {
		if (m_pMyVehicle->pPassengers[TRAIN_POS_LEFT_ENTRY] == this) {

			lineUpPos = trainModel->m_positions[TRAIN_POS_LEFT_ENTRY] - enterOffset;

		} else if (m_pMyVehicle->pPassengers[TRAIN_POS_MID_ENTRY] == this) {

			lineUpPos = trainModel->m_positions[TRAIN_POS_MID_ENTRY] - enterOffset;

		} else if (m_pMyVehicle->pPassengers[TRAIN_POS_RIGHT_ENTRY] == this) {

			lineUpPos = trainModel->m_positions[TRAIN_POS_RIGHT_ENTRY] - enterOffset;
		}
		lineUpPos = Multiply3x3(m_pMyVehicle->GetMatrix(), lineUpPos);
		lineUpPos += m_pMyVehicle->GetPosition();
	}

	if (m_pVehicleAnim) {
		float percentageLeft = (m_pVehicleAnim->m_BlendHier->m_fTotalTime - m_pVehicleAnim->m_CurrentTime) / m_pVehicleAnim->m_BlendHier->m_fTotalTime;
		lineUpPos += (GetPosition() - lineUpPos) * percentageLeft;
	}

	SetPosition(lineUpPos);
	SetHeading(m_fRotationCur);
}




void
CPed::RegisterThreatWithGangPeds(CEntity *attacker)
{
	CPed *attackerPed = nil;
	if ((CharCreatedBy == MISSION_CHAR && bIsPlayerFriend && (attacker == FindPlayerPed() || attacker == FindPlayerVehicle()))
		|| (attacker && m_leader == attacker)
		|| (m_nPedType == PEDTYPE_GANG7 && attacker == FindPlayerPed()))
		return;

	if (attacker) {
		if (m_objective != OBJECTIVE_KILL_CHAR_ON_FOOT && m_objective != OBJECTIVE_KILL_CHAR_ANY_MEANS) {
			if (attacker->IsPed()) {
				attackerPed = (CPed*)attacker;
			} else if (attacker->IsVehicle()) {
				attackerPed = ((CVehicle*)attacker)->pDriver;
				if (!attackerPed)
					return;
			} else
				return;

			if (attackerPed && (attackerPed->IsPlayer() || attackerPed->IsGangMember())) {
				for (int i = 0; i < m_numNearPeds; ++i) {
					CPed *nearPed = m_nearPeds[i];
					if (nearPed->IsPointerValid()) {
						if (nearPed->CharCreatedBy == RANDOM_CHAR && nearPed != this && nearPed->m_nPedType == m_nPedType)
							nearPed->m_fearFlags |= CPedType::GetFlag(attackerPed->m_nPedType);
					}
				}
			}
		}
	}

	if (attackerPed && attackerPed->IsPlayer() && (attackerPed->EnteringCar() || attackerPed->bInVehicle)) {
		if (!attackerPed->m_pMyVehicle || attackerPed->m_pMyVehicle->GetModelIndex() != MI_TOPFUN) {
			int16 lastVehicle;
			CEntity *vehicles[8];
			CWorld::FindObjectsInRange(GetPosition(), 30.0f, true, &lastVehicle, 6, vehicles, false, true, false, false, false);

			if (lastVehicle > 8)
				lastVehicle = 8;

			for (int j = 0; j < lastVehicle; ++j) {
				CVehicle *nearVeh = (CVehicle*) vehicles[j];

				if (nearVeh->VehicleCreatedBy != MISSION_VEHICLE) {
					CPed *nearVehDriver = nearVeh->pDriver;

					if (nearVehDriver && nearVehDriver != this && nearVehDriver->m_nPedType == m_nPedType && nearVehDriver->CharCreatedBy == RANDOM_CHAR) {

						if (nearVeh->IsVehicleNormal() && nearVeh->IsAutomobile()) {
							nearVeh->AutoPilot.m_nCruiseSpeed = GAME_SPEED_TO_CARAI_SPEED * nearVeh->m_pHandlingData->m_Transmission.m_fMaxCruiseVelocity * 0.8f;
							nearVeh->AutoPilot.m_nCarMission = MISSION_RAMPLAYER_FARAWAY;
							nearVeh->SetStatus(STATUS_PHYSICS);
							nearVeh->AutoPilot.m_nTempAction = TEMPACT_NONE;
							nearVeh->AutoPilot.m_nDrivingStyle = DRIVINGSTYLE_AVOID_CARS;
						}
					}
				}
			}
		}
	}
}

// Some helper function which doesn't exist in og game.
inline void
SelectClosestNodeForSeek(CPed *ped, CPathNode *node, CVector2D closeDist, CVector2D farDist, CPathNode *closeNode, CPathNode *closeNode2, int runCount = 3)
{
	for (int i = 0; i < node->numLinks; i++) {

		CPathNode *testNode = &ThePaths.m_pathNodes[ThePaths.ConnectedNode(i + node->firstLink)];

		if (testNode && testNode != closeNode && testNode != closeNode2) {
			CVector2D posDiff(ped->m_vecSeekPos - testNode->GetPosition());
			float dist = posDiff.MagnitudeSqr();

			if (farDist.MagnitudeSqr() > dist) {

				if (closeDist.MagnitudeSqr() > dist) {
					ped->m_pNextPathNode = (closeNode2 ? closeNode2 : testNode);
					farDist = posDiff;
				} else {
					ped->m_pNextPathNode = closeNode;
					closeDist = posDiff;
				}
			}

			if (--runCount > 0)
				SelectClosestNodeForSeek(ped, testNode, closeDist, farDist, closeNode, (closeNode2 ? closeNode2 : testNode), runCount);
		}
	}
}

bool
CPed::FindBestCoordsFromNodes(CVector unused, CVector *bestCoords)
{
	if (m_pNextPathNode || !bUsePedNodeSeek)
		return false;

	const CVector &ourPos = GetPosition();

	int closestNodeId = ThePaths.FindNodeClosestToCoors(GetPosition(), 1, 999999.9f);

	CVector seekObjPos = m_vecSeekPos;
	seekObjPos.z += 1.0f;

	if (CWorld::GetIsLineOfSightClear(ourPos, seekObjPos, true, false, false, true, false, false, false))
		return false;

	m_pNextPathNode = nil;

	CVector2D seekPosDist (m_vecSeekPos - ourPos);

	CPathNode *closestNode = &ThePaths.m_pathNodes[closestNodeId];
	CVector2D closeDist(m_vecSeekPos - closestNode->GetPosition());

	SelectClosestNodeForSeek(this, closestNode, closeDist, seekPosDist, closestNode, nil);

	if (m_pNextPathNode) {

		// Function above decided that directly going to next node makes more sense then seeking the object.
		CVector correctedCoords = CPathFind::TakeWidthIntoAccountForWandering(m_pNextPathNode, m_randomSeed);

		if ((correctedCoords - ourPos).MagnitudeSqr2D() < seekPosDist.MagnitudeSqr()) {
			*bestCoords = CPathFind::TakeWidthIntoAccountForWandering(m_pNextPathNode, m_randomSeed);
			return true;
		}
		m_pNextPathNode = nil;
	}

	return false;
}

bool
CPed::DuckAndCover(void)
{
	if (!m_pedInObjective || CTimer::GetTimeInMilliseconds() <= m_duckAndCoverTimer)
		return false;

	if (bKindaStayInSamePlace){

		if (CTimer::GetTimeInMilliseconds() <= m_leaveCarTimer) {
			if (!m_pLookTarget || m_pLookTarget != m_pedInObjective) {
				m_pLookTarget = m_pedInObjective;
				m_pLookTarget->RegisterReference((CEntity **) &m_pLookTarget);
			}
			if (!bIsAimingGun)
				SetAimFlag(m_pedInObjective);

		} else {
			bKindaStayInSamePlace = false;
			if (bIsDucking)
				ClearDuck(true);

			bCrouchWhenShooting = false;
			bDuckAndCover = false;
			m_headingRate = 10.0f;
			m_duckAndCoverTimer = CTimer::GetTimeInMilliseconds() + CGeneral::GetRandomNumberInRange(20000,30000);
			if (m_pSeekTarget && m_pSeekTarget->IsVehicle())
				((CVehicle*)m_pSeekTarget)->m_numPedsUseItAsCover--;
		}
		return false;
	}
	
	int16 lastVehicle = 0;
	CEntity* vehicles[8];

	bool justDucked = false;
	CVehicle *foundVeh = nil;
	float maxDist = 225.0f;
	if (bIsDucking)
		ClearDuck(true);

	bCrouchWhenShooting = false;
	bool duckingWithoutVeh = false;
	if (CTimer::GetTimeInMilliseconds() > m_leaveCarTimer) {

		for(int i = 0; i < 6; i++) {
			CPlayerPed *player = (CPlayerPed*)m_pedInObjective;

			if (player->m_pPedAtSafePos[i] == this) {
				duckingWithoutVeh = true;
				CVector &safePos = player->m_vecSafePos[i];
				bool notRunningToSafePos = false;

				if (m_vecSeekPos.x != safePos.x && m_vecSeekPos.y != safePos.y && m_vecSeekPos.z != safePos.z)
					notRunningToSafePos = true;

				if (!notRunningToSafePos) {
					CVector target = player->m_vecSafePos[i];
					SetSeek(target, 1.0f);
					duckingWithoutVeh = true;
					m_attackTimer = CTimer::GetTimeInMilliseconds() + 6000;
					bDuckAndCover = true;
				}
				break;
			}
		}
		if (!duckingWithoutVeh) {
			for (int i = 0; i < 6; i++) {
				CPlayerPed* player = (CPlayerPed*)m_pedInObjective;
				if (!player->m_pPedAtSafePos[i] && player->m_vecSafePos[i].x != 0.0f) {
					player->m_pPedAtSafePos[i] = this;
					CVector target = player->m_vecSafePos[i];
					SetSeek(target, 1.0f);
					m_headingRate = 15.0f;
					ClearPointGunAt();
					duckingWithoutVeh = 1;
					bIsRunning = true;
					bDuckAndCover = true;
					justDucked = true;
					m_leaveCarTimer = CTimer::GetTimeInMilliseconds() + 500;
					break;
				}
			}
		}
		if (!duckingWithoutVeh) {
			CVector pos = GetPosition();
			CWorld::FindObjectsInRange(pos, CHECK_NEARBY_THINGS_MAX_DIST, true, &lastVehicle, 6, vehicles, false, true, false, false, false);
		}

		for (int i = 0; i < lastVehicle; i++) {
			CVehicle *veh = (CVehicle*) vehicles[i];
			if (veh->IsAutomobile() && veh->m_vecMoveSpeed.Magnitude() <= 0.02f
				&& !veh->bIsBus && !veh->bIsVan && !veh->bIsBig
				&& veh->m_numPedsUseItAsCover < 3) {

				float dist = (GetPosition() - veh->GetPosition()).MagnitudeSqr();
				if (dist < maxDist) {
					maxDist = dist;
					foundVeh = veh;
				}
			}
		}
		if (foundVeh) {
			// Unused.
			// CVector lfWheelPos, rfWheelPos;
			// foundVeh->GetComponentWorldPosition(CAR_WHEEL_RF, rfWheelPos);
			// foundVeh->GetComponentWorldPosition(CAR_WHEEL_LF, lfWheelPos);
			CVector rightSide, leftSide;

			// 3 persons can use the car as cover. Found the correct position for us.
			if (foundVeh->m_numPedsUseItAsCover == 2) {
				rightSide = CVector(1.5f, -0.5f, 0.0f);
				leftSide = CVector(-1.5f, -0.5f, 0.0f);
			} else if (foundVeh->m_numPedsUseItAsCover == 1) {
				rightSide = CVector(1.5f, 0.5f, 0.0f);
				leftSide = CVector(-1.5f, 0.5f, 0.0f);
			} else if (foundVeh->m_numPedsUseItAsCover == 0) {
				rightSide = CVector(1.5f, 0.0f, 0.0f);
				leftSide = CVector(-1.5f, 0.0f, 0.0f);
			}

			CMatrix vehMatrix(foundVeh->GetMatrix());
			CVector duckAtRightSide = Multiply3x3(vehMatrix, rightSide) + foundVeh->GetPosition();

			CVector duckAtLeftSide = Multiply3x3(vehMatrix, leftSide) + foundVeh->GetPosition();

			CVector distWithPedRightSide = m_pedInObjective->GetPosition() - duckAtRightSide;
			CVector distWithPedLeftSide = m_pedInObjective->GetPosition() - duckAtLeftSide;

			CVector duckPos;
			if (distWithPedRightSide.MagnitudeSqr() <= distWithPedLeftSide.MagnitudeSqr())
				duckPos = duckAtLeftSide;
			else
				duckPos = duckAtRightSide;

			if (CWorld::TestSphereAgainstWorld(duckPos, 0.5f, nil, true, true, true, false, false, false)) {
				SetSeek(duckPos, 1.0f);
				m_headingRate = 15.0f;
				bIsRunning = true;
				bDuckAndCover = true;
				justDucked = true;
				m_leaveCarTimer = CTimer::GetTimeInMilliseconds() + 500;
				if (foundVeh->bIsLawEnforcer) {
					m_carInObjective = foundVeh;
					m_carInObjective->RegisterReference((CEntity**)&m_carInObjective);
				}
				m_pSeekTarget = foundVeh;
				m_pSeekTarget->RegisterReference((CEntity**)&m_pSeekTarget);
				ClearPointGunAt();
			} else {
				m_duckAndCoverTimer = CTimer::GetTimeInMilliseconds() + CGeneral::GetRandomNumberInRange(10000, 15000);
				bDuckAndCover = false;
			}
		} else if (!duckingWithoutVeh) {
			bDuckAndCover = false;
		}
	}

	if (!justDucked && !bDuckAndCover)
		return false;
	
	if (!Seek()) {
		if (m_nMoveState == PEDMOVE_STILL) {
			bDuckAndCover = false;
			return false;
		} else
			return true;
	}

	bKindaStayInSamePlace = true;
	bDuckAndCover = false;
	m_vecSeekPos = CVector(0.0f, 0.0f, 0.0f);
	if (m_pSeekTarget && m_pSeekTarget->IsVehicle())
		((CVehicle*)m_pSeekTarget)->m_numPedsUseItAsCover++;
	
	SetIdle();
	SetMoveState(PEDMOVE_STILL);
	SetMoveAnim();
	if (!m_pLookTarget || m_pLookTarget != m_pedInObjective) {
		m_pLookTarget = m_pedInObjective;
		m_pLookTarget->RegisterReference((CEntity **) &m_pLookTarget);
	}

	m_leaveCarTimer = CTimer::GetTimeInMilliseconds() + CGeneral::GetRandomNumberInRange(3000, 6000);
	bCrouchWhenShooting = true;
	SetDuck(CGeneral::GetRandomNumberInRange(2000, 5000), true);
	return false;
}

CVector
CPed::GetPositionToOpenCarDoor(CVehicle *veh, uint32 component, float offset, CPed *ped)
{	
	CVector doorPos;
	CMatrix vehMat(veh->GetMatrix());

	if (veh->IsBike()) {
		CVehicleModelInfo* vehModel = (CVehicleModelInfo*)CModelInfo::GetModelInfo(veh->GetModelIndex());
		CVector vehDoorOffset;
		CBike* bike = (CBike*)veh;
		CVector kickOffset = GetPedAnimationOffset(ped, ANIM_GROUP_HARLEYBIKEANIMS, ANIM_ID_CAR_GETIN_BIKE_FRONT, vecPedBikeKickAnimOffset);
		doorPos = vehModel->GetFrontSeatPosn();

		if (component == CAR_WINDSCREEN) {
#ifdef FIX_BUGS
			return bike->GetMatrix() * (doorPos +
				CVector(-kickOffset.x, kickOffset.y, -kickOffset.z));
#else
			return bike->GetMatrix() * (doorPos + kickOffset);
#endif
		} else {
			switch (GetBikeEnterAnimGroup(veh, ANIM_ID_CAR_GETIN_LHS_0)) {
			case ANIM_GROUP_VESPABIKEANIMS:
				vehDoorOffset = GetPedAnimationOffset(ped, ANIM_GROUP_VESPABIKEANIMS, ANIM_ID_CAR_GETIN_LHS_0, vecPedVespaBikeJumpRhsAnimOffset);
				break;
			case ANIM_GROUP_HARLEYBIKEANIMS:
				vehDoorOffset = GetPedAnimationOffset(ped, ANIM_GROUP_HARLEYBIKEANIMS, ANIM_ID_CAR_GETIN_LHS_0, vecPedHarleyBikeJumpRhsAnimOffset);
				break;
			case ANIM_GROUP_DIRTBIKEANIMS:
				vehDoorOffset = GetPedAnimationOffset(ped, ANIM_GROUP_DIRTBIKEANIMS, ANIM_ID_CAR_GETIN_LHS_0, vecPedDirtBikeJumpRhsAnimOffset);
				break;
			default:
				vehDoorOffset = GetPedAnimationOffset(ped, ANIM_GROUP_STDBIKEANIMS, ANIM_ID_CAR_GETIN_LHS_0, vecPedStdBikeJumpRhsAnimOffset);
				break;
			}
			vehDoorOffset.x += offset * bike->m_pHandlingData->m_fSeatOffsetDistance;
			if (component == CAR_DOOR_LR || component == CAR_DOOR_RR) {
				doorPos = vehModel->m_positions[CAR_POS_BACKSEAT];
			}

			if (component == CAR_DOOR_LR || component == CAR_DOOR_LF)
				vehDoorOffset.x *= -1.f;

			CVector correctedPos;
			bike->GetCorrectedWorldDoorPosition(correctedPos, vehDoorOffset, doorPos);
			return correctedPos;
		}
	}
	doorPos = Multiply3x3(vehMat, GetLocalPositionToOpenCarDoor(veh, component, offset, ped));

	return veh->GetPosition() + doorPos;
}

CVector
CPed::GetLocalPositionToOpenCarDoor(CVehicle *veh, uint32 component, float seatPosMult, CPed *ped)
{
	CVehicleModelInfo *vehModel; 
	CVector vehDoorPos;
	CVector vehDoorOffset;
	float seatOffset;

	vehModel = (CVehicleModelInfo *)CModelInfo::GetModelInfo(veh->GetModelIndex());

	if (veh->IsBike()) {
		CBike *bike = (CBike*)veh;
		CVector kickOffset = GetPedAnimationOffset(ped, ANIM_GROUP_HARLEYBIKEANIMS, ANIM_ID_CAR_GETIN_BIKE_FRONT, vecPedBikeKickAnimOffset);
		vehDoorPos = vehModel->GetFrontSeatPosn();

		if (component == CAR_WINDSCREEN) {
			return bike->GetMatrix() * (vehDoorPos + kickOffset);
		} else {
			switch (GetBikeEnterAnimGroup(veh, ANIM_ID_CAR_GETIN_LHS_0)) {
				case ANIM_GROUP_VESPABIKEANIMS:
					vehDoorOffset = GetPedAnimationOffset(ped, ANIM_GROUP_VESPABIKEANIMS, ANIM_ID_CAR_GETIN_LHS_0, vecPedVespaBikeJumpRhsAnimOffset);
					break;
				case ANIM_GROUP_HARLEYBIKEANIMS:
					vehDoorOffset = GetPedAnimationOffset(ped, ANIM_GROUP_HARLEYBIKEANIMS, ANIM_ID_CAR_GETIN_LHS_0, vecPedHarleyBikeJumpRhsAnimOffset);
					break;
				case ANIM_GROUP_DIRTBIKEANIMS:
					vehDoorOffset = GetPedAnimationOffset(ped, ANIM_GROUP_DIRTBIKEANIMS, ANIM_ID_CAR_GETIN_LHS_0, vecPedDirtBikeJumpRhsAnimOffset);
					break;
				default:
					vehDoorOffset = GetPedAnimationOffset(ped, ANIM_GROUP_STDBIKEANIMS, ANIM_ID_CAR_GETIN_LHS_0, vecPedStdBikeJumpRhsAnimOffset);
					break;
			}
			float xOffsetFromAnim = vehDoorOffset.x + seatPosMult * bike->m_pHandlingData->m_fSeatOffsetDistance;
			if (component == CAR_DOOR_LR || component == CAR_DOOR_RR) {
				vehDoorPos = vehModel->m_positions[CAR_POS_BACKSEAT];
			}

			if (component == CAR_DOOR_LR || component == CAR_DOOR_LF)
				xOffsetFromAnim *= -1.f;

			return bike->GetMatrix() * (vehDoorPos + CVector(xOffsetFromAnim, vehDoorOffset.y, vehDoorOffset.z));
		}
	} else {
		if (veh->bIsVan && (component == CAR_DOOR_LR || component == CAR_DOOR_RR)) {
			seatOffset = 0.0f;
			vehDoorOffset = GetPedAnimationOffset(ped, ANIM_GROUP_VANCARANIMS, ANIM_ID_CAR_GETIN_LHS_1, vecPedVanRearDoorAnimOffset);
		} else {
			seatOffset = veh->m_pHandlingData->m_fSeatOffsetDistance * seatPosMult;
			if (veh->bLowVehicle) {
				vehDoorOffset = GetPedAnimationOffset(ped, ANIM_GROUP_LOWCARAMIMS, ANIM_ID_CAR_GETIN_LHS_0, vecPedCarDoorLoAnimOffset);
			} else {
				vehDoorOffset = GetPedAnimationOffset(ped, ANIM_GROUP_STDCARAMIMS, ANIM_ID_CAR_GETIN_LHS_0, vecPedCarDoorAnimOffset);
			}
		}

		switch (component) {
			case CAR_DOOR_RF:
				vehDoorPos = vehModel->GetFrontSeatPosn();
				vehDoorPos.x += seatOffset;
				vehDoorOffset.x = -vehDoorOffset.x;
				break;

			case CAR_DOOR_RR:
				vehDoorPos = vehModel->m_positions[CAR_POS_BACKSEAT];
				vehDoorPos.x += seatOffset;
				vehDoorOffset.x = -vehDoorOffset.x;
				break;

			case CAR_DOOR_LF:
				vehDoorPos = vehModel->GetFrontSeatPosn();
				vehDoorPos.x = -(vehDoorPos.x + seatOffset);
				break;

			case CAR_DOOR_LR:
				vehDoorPos = vehModel->m_positions[CAR_POS_BACKSEAT];
				vehDoorPos.x = -(vehDoorPos.x + seatOffset);
				break;

			default:
				vehDoorPos = vehModel->GetFrontSeatPosn();
				vehDoorOffset = CVector(0.0f, 0.0f, 0.0f);
		}
		return vehDoorPos - vehDoorOffset;
	}
}

// TODO: what is this parameter for?
void
CPed::SetDuck(uint32 time, bool sth)
{
	if (bIsDucking || CTimer::GetTimeInMilliseconds() <= m_duckTimer && !sth) {
		if (sth && CTimer::GetTimeInMilliseconds() + time > m_duckTimer)
			m_duckTimer = CTimer::GetTimeInMilliseconds() + time;
		return;
	}

	CAnimBlendAssociation *duckAssoc;
	if (bCrouchWhenShooting) {
		duckAssoc = CAnimManager::BlendAnimation(GetClump(), ANIM_GROUP_DEFAULT, ANIM_ID_WEAPON_CROUCH, 4.0f);
		if (!duckAssoc) return; // stay upright if this SA bank has no crouch clip
		duckAssoc->m_Flags &= ~ANIMATION_IS_FINISH_AUTO_REMOVE;
		bIsDucking = true;
		if (IsPlayer() && ((CPlayerPed*)this)->GetPlayerData())
			((CPlayerPed*)this)->GetPlayerData()->m_vecFightMovement = CVector2D(0.0f, 0.0f);
		m_duckTimer = CTimer::GetTimeInMilliseconds() + time;
	} else {
		duckAssoc = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_DUCK_COWER);
		if (!duckAssoc || duckAssoc->m_BlendDelta < 0.0f) {
			duckAssoc = CAnimManager::BlendAnimation(GetClump(), ANIM_GROUP_DEFAULT, ANIM_ID_DUCK_COWER, 4.0f);
			if (!duckAssoc) return;
			bIsDucking = true;
			m_duckTimer = CTimer::GetTimeInMilliseconds() + time;
		}
	}
}

void
CPed::Duck(void)
{
	if (CTimer::GetTimeInMilliseconds() > m_duckTimer)
		ClearDuck();
	else if (bIsDucking && bCrouchWhenShooting) {
		CWeaponInfo *weapon = CWeaponInfo::GetWeaponInfo(GetWeapon()->m_eWeaponType);
		CAnimBlendAssociation *crouchAnim = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_WEAPON_CROUCH);
		if (!crouchAnim) {
			if(GetCrouchFireAnim(weapon))
				crouchAnim = RpAnimBlendClumpGetAssociation(GetClump(), GetCrouchFireAnim(weapon));
		}
		if (!crouchAnim) {
			if(GetCrouchReloadAnim(weapon))
				crouchAnim = RpAnimBlendClumpGetAssociation(GetClump(), GetCrouchReloadAnim(weapon));
		}
		if (!crouchAnim && IsPlayer() && IsPedInControl())
			crouchAnim = CAnimManager::BlendAnimation(GetClump(), ANIM_GROUP_DEFAULT, ANIM_ID_WEAPON_CROUCH, 4.0f);
		if (!crouchAnim) {
			ClearDuck(false);
#if defined FIX_BUGS || defined FREE_CAM
			if (IsPlayer())
				bCrouchWhenShooting = false;
#endif
		}
	}
}

void
CPed::ClearDuck(bool clearTimer)
{
	// Both poses can be present during a transition. Fade each one rather than
	// leaving the weapon crouch active when the cower association also exists.
	const AnimationId duckAnims[] = {
		ANIM_ID_DUCK_COWER, ANIM_ID_WEAPON_CROUCH, ANIM_ID_GUNCROUCHFWD,
		ANIM_ID_GUNCROUCHBWD, ANIM_ID_CROUCH_ROLL_L, ANIM_ID_CROUCH_ROLL_R
	};
	for (int i = 0; i < ARRAY_SIZE(duckAnims); ++i) {
		CAnimBlendAssociation *animAssoc = RpAnimBlendClumpGetAssociation(GetClump(), duckAnims[i]);
		if (animAssoc) {
			if (duckAnims[i] == ANIM_ID_CROUCH_ROLL_L || duckAnims[i] == ANIM_ID_CROUCH_ROLL_R)
				animAssoc->m_nCallbackType = ANIM_BLEND_CALLBACK_NONE;
			animAssoc->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE;
			animAssoc->m_BlendDelta = -4.0f;
		}
	}
	if (IsPlayer() && ((CPlayerPed*)this)->GetPlayerData())
		((CPlayerPed*)this)->GetPlayerData()->m_vecFightMovement = CVector2D(0.0f, 0.0f);
	bIsDucking = false;
	if (IsPlayer() && m_nPedState == PED_AIMGUN)
		CAnimManager::BlendAnimation(GetClump(), ANIM_GROUP_DEFAULT, ANIM_ID_GUN_STAND, 4.0f);

	if (clearTimer) {
		m_duckTimer = 0;
	}
}

void
CPed::InformMyGangOfAttack(CEntity *attacker)
{
	CPed *attackerPed;

	if (m_objective == OBJECTIVE_KILL_CHAR_ON_FOOT || m_objective == OBJECTIVE_KILL_CHAR_ANY_MEANS)
		return;

	if (attacker->IsPed()) {
		attackerPed = (CPed*)attacker;
	} else {
		if (!attacker->IsVehicle())
			return;

		attackerPed = ((CVehicle*)attacker)->pDriver;
		if (!attackerPed)
			return;
	}

	if (attackerPed->m_nPedType == PEDTYPE_COP)
		return;

	for (int i = 0; i < m_numNearPeds; i++)	{
		CPed *nearPed = m_nearPeds[i];
		if (nearPed && nearPed != this) {
			CPed *leader = nearPed->m_leader;
			if (leader && leader == this && nearPed->m_pedStats->m_nFear < nearPed->m_pedStats->m_nTemper) {
				nearPed->SetObjective(OBJECTIVE_KILL_CHAR_ON_FOOT, attackerPed);
				nearPed->SetObjectiveTimer(30000);
			}
		}
	}
}




bool
CPed::IsRoomToBeCarJacked(void)
{
	if (!m_pMyVehicle)
		return false;

	CVector offset;
	if (m_pMyVehicle->IsBike()) {
		offset = vecPedStdBikeJumpRhsAnimOffset;
	} else if (m_pMyVehicle->bLowVehicle || m_nPedType == PEDTYPE_COP) {
		offset = vecPedDraggedOutCarAnimOffset;
	} else {
		offset = vecPedQuickDraggedOutCarAnimOffset;
	}

	offset.z = 0.0f;
	if (m_pMyVehicle->IsRoomForPedToLeaveCar(CAR_DOOR_LF, &offset)) {
		return true;
	}

	return false;
}

void
CPed::AddInCarAnims(CVehicle* car, bool isDriver)
{
	if (car->IsBoat()) {
		if (car->m_pHandlingData->m_nModelFlags & VEHICLE_HANDLING_MODEL_SIT_IN_BOAT) {
			m_pVehicleAnim = CAnimManager::BlendAnimation(GetClump(), ANIM_GROUP_DEFAULT, ANIM_ID_CAR_SIT, 100.0f);
		} else {
			m_pVehicleAnim = CAnimManager::BlendAnimation(GetClump(), ANIM_GROUP_DEFAULT, ANIM_ID_DRIVE_BOAT, 100.0f);
		}
	} else if (car->IsBike()) {
		if (isDriver) {
			m_pVehicleAnim = CAnimManager::BlendAnimation(GetClump(), ((CBike*)car)->m_bikeAnimType, ANIM_ID_BIKE_RIDE, 100.0f);
		} else {
			m_pVehicleAnim = CAnimManager::BlendAnimation(GetClump(), ((CBike*)car)->m_bikeAnimType, ANIM_ID_BIKE_PASSENGER, 100.0f);
		}
	} else {
		// SA CCarEnterExit::AddInCarAnim (0x64F720): the seat pose always
		// comes from the default ped group, never from the vehicle's own
		// animation group. The vehicle groups only hold the in/out clips
		// (their ids start at ANIM_ID_CAR_ALIGN_LHS); CAR_sit and its
		// variants live in the default group, so resolving them through
		// GetVehicleAnimGroup produced nil and the ped stayed standing in
		// the car. A low car has its own lower pose for both seats.
		AnimationId seatAnim;
		if (isDriver)
			seatAnim = car->bLowVehicle ? ANIM_ID_CAR_LSIT : ANIM_ID_CAR_SIT;
		else
			seatAnim = car->bLowVehicle ? ANIM_ID_CAR_SITPLO : ANIM_ID_CAR_SITP;
		m_pVehicleAnim = CAnimManager::BlendAnimation(GetClump(), ANIM_GROUP_DEFAULT, seatAnim, 100.0f);
		if (IsPlayer())
			REVC_LOG_INFO("Vehicle entry: seat anim %d -> %s", seatAnim,
				m_pVehicleAnim ? "playing" : "MISSING");
	}

	StopNonPartialAnims();
}

void
CPed::RemoveDrivebyAnims()
{
	CAnimBlendAssociation *animAssoc;

	AnimationId LeftAnim = ANIM_ID_DRIVEBY_L;
	AnimationId RightAnim = ANIM_ID_DRIVEBY_R;

	if (m_pMyVehicle->m_pHandlingData->m_nModelFlags & VEHICLE_HANDLING_MODEL_IS_BIKE) {
		LeftAnim = ANIM_ID_BIKE_DRIVEBYRHS;
		RightAnim = ANIM_ID_BIKE_DRIVEBYLHS;
	} else if (m_pMyVehicle->bLowVehicle) {
		LeftAnim = ANIM_ID_DRIVEBYL_L;
		RightAnim = ANIM_ID_DRIVEBYL_R;
	}

	animAssoc = RpAnimBlendClumpGetAssociation((RpClump*)m_rwObject, ANIM_ID_BIKE_DRIVEBYRHS);
	if (animAssoc)
		animAssoc->m_BlendDelta = -1000.0f;
	animAssoc = RpAnimBlendClumpGetAssociation((RpClump*)m_rwObject, ANIM_ID_BIKE_DRIVEBYLHS);
	if (animAssoc)
		animAssoc->m_BlendDelta = -1000.0f;
	animAssoc = RpAnimBlendClumpGetAssociation((RpClump*)m_rwObject, ANIM_ID_BIKE_DRIVEBYFT);
	if (animAssoc)
		animAssoc->m_BlendDelta = -1000.0f;
}

void
CPed::RemoveInCarAnims(void)
{
	// SA CCarEnterExit::RemoveCarSitAnim (0x64F680) removes the in-car set
	// by ANIMATION_SECONDARY_TASK_ANIM: every steering, look-back and
	// drive-by clip carries it in SA's data. This used to iterate
	// ANIMATION_IS_SYNCRONISED, which in SA's data is the walk-cycle flag
	// and marks no in-car clip at all - the steering clips survived the
	// exit at blend 0, and the player-control gate that refuses input
	// while a secondary anim exists then blocked all movement on foot.
	CAnimBlendAssociation* assoc;

	for (assoc = RpAnimBlendClumpGetFirstAssociation(GetClump(), ANIMATION_SECONDARY_TASK_ANIM); assoc;
		assoc = RpAnimBlendGetNextAssociation(assoc, ANIMATION_SECONDARY_TASK_ANIM)) {
		assoc->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE;
		assoc->m_BlendDelta = -1000.0f;
	}
}

bool
CPed::PositionPedOutOfCollision(void)
{
	CVehicle *veh = m_pMyVehicle;
	if (!veh)
		return false;

	if (bDonePositionOutOfCollision)
		return true;

	bool foundAPos = false;
	CColModel *vehCol = veh->GetColModel();
	CVector vehPos = veh->GetPosition();
	CVector ourPos = GetPosition();
	CVector newPos = ourPos;
	CWorld::pIgnoreEntity = veh;
	bUsesCollision = false;
	bJustCheckCollision = true;
	m_vecMoveSpeed = CVector(0.f, 0.f, 0.f);
	if (veh->IsOnItsSide()) {
		// Top of the veh.
		newPos = vehPos;
		newPos.z = FEET_OFFSET + vehCol->boundingBox.max.x + vehPos.z;
		GetMatrix().SetTranslate(newPos);
		if (!CheckCollision()) {
			if (CWorld::GetIsLineOfSightClear(vehPos, newPos, true, false, false, true, false, false, false))
				foundAPos = true;
		}

	} else if (m_vehDoor != 0) {
		// Try the normal way
		CVector pos = GetPositionToOpenCarDoor(m_pMyVehicle, m_vehDoor, this);
		newPos = pos;
		GetMatrix().SetTranslate(newPos);
		if (!CheckCollision()) {
			if (CWorld::GetIsLineOfSightClear(vehPos, newPos, true, false, false, true, false, false, false))
				foundAPos = true;
		}
	}

	float vehRelativeExitX = vehCol->boundingBox.min.x - 0.355f;
	if (m_vehDoor == CAR_DOOR_RF || m_vehDoor == CAR_DOOR_RR)
		vehRelativeExitX = 0.355f + vehCol->boundingBox.max.x;

	if (!foundAPos) {
		// Check sides of veh., respective to seat column-veh. center difference(why?)
		float exitOffset = vehRelativeExitX - DotProduct(ourPos - vehPos, veh->GetRight());
		newPos = exitOffset * veh->GetRight() + ourPos;
		GetMatrix().SetTranslate(newPos);
		if (!CheckCollision()) {
			if (CWorld::GetIsLineOfSightClear(vehPos, newPos, true, false, false, true, false, false, false))
				foundAPos = true;
		}
	}
	if (!foundAPos) {
		// Iterate through sections of veh. length + static offset on X
		float minY = vehCol->boundingBox.min.y;
		float ySection = (vehCol->boundingBox.max.y - minY) / 3.f;
		for (int i = 0; i < 4; i++) {
			float fwdMult = i * ySection + minY;
			newPos = vehRelativeExitX * veh->GetRight() + fwdMult * veh->GetForward() + vehPos;
			GetMatrix().SetTranslate(newPos);
			if (!CheckCollision()) {
				if (CWorld::GetIsLineOfSightClear(vehPos, newPos, true, false, false, true, false, false, false)) {
					foundAPos = true;
					break;
				}
			}
		}
	}
	if (!foundAPos) {
		// Back of veh.
		newPos = (vehCol->boundingBox.min.y - 0.355f) * veh->GetForward() + vehPos;
		GetMatrix().SetTranslate(newPos);
		if (!CheckCollision()) {
			if (CWorld::GetIsLineOfSightClear(vehPos, newPos, true, false, false, true, false, false, false))
				foundAPos = true;
		}
	}
	if (!foundAPos) {
		// Front of veh.
		newPos = (0.355f + vehCol->boundingBox.max.y) * veh->GetForward() + vehPos;
		GetMatrix().SetTranslate(newPos);
		if (!CheckCollision()) {
			if (CWorld::GetIsLineOfSightClear(vehPos, newPos, true, false, false, true, false, false, false))
				foundAPos = true;
		}
	}
	if (!foundAPos) {
		// Opposite X side + back
		newPos = vehCol->boundingBox.min.y * veh->GetForward() + vehPos - vehRelativeExitX * veh->GetRight();
		GetMatrix().SetTranslate(newPos);
		if (!CheckCollision()) {
			if (CWorld::GetIsLineOfSightClear(vehPos, newPos, true, false, false, true, false, false, false))
				foundAPos = true;
		}
	}
	if (!foundAPos) {
		// Opposite X side + front
		newPos = vehCol->boundingBox.max.y * veh->GetForward() + vehPos - vehRelativeExitX * veh->GetRight();
		GetMatrix().SetTranslate(newPos);
		if (!CheckCollision()) {
			if (CWorld::GetIsLineOfSightClear(vehPos, newPos, true, false, false, true, false, false, false))
				foundAPos = true;
		}
	}
	if (!foundAPos) {
		// Top of veh.
		if (veh->IsAutomobile()) {
			newPos = vehCol->boundingBox.max.z * veh->GetUp() + vehPos;
			newPos.z += FEET_OFFSET;
			GetMatrix().SetTranslate(newPos);
			if (!CheckCollision()) {
				if (CWorld::GetIsLineOfSightClear(vehPos, newPos, true, false, false, true, false, false, false))
					foundAPos = true;
			}
		}
	}
	m_vecMoveSpeed = CVector(0.f, 0.f, 0.f);
	m_vecTurnSpeed = CVector(0.f, 0.f, 0.f);
	veh->m_vecMoveSpeed = CVector(0.f, 0.f, 0.f);
	veh->m_vecTurnSpeed = CVector(0.f, 0.f, 0.f);
	CWorld::pIgnoreEntity = nil;
	bUsesCollision = true;
	bJustCheckCollision = false;
	bDonePositionOutOfCollision = true;
	if (foundAPos)
		return true;
	int foundNode = ThePaths.FindNodeClosestToCoors(vehPos, PATH_PED, 999999.9f, true, false, false, false);
	if (foundNode < 0)
		return false;
	newPos = ThePaths.m_pathNodes[foundNode].GetPosition();
	CPedPlacement::FindZCoorForPed(&newPos);
	GetMatrix().SetTranslate(newPos);
	SetHeading(m_pMyVehicle->GetForward().Heading());
	return true;
}

// "Any" means he shouldn't have to be in vehicle.
bool
CPed::PositionAnyPedOutOfCollision(void)
{
	CVehicle *veh;
	CVector posNearVeh;
	CVector posSomewhereClose;
	bool putNearVeh = false;
	bool putSomewhereClose = false;
	int smallestDistNearVeh = 999;
	int smallestDistSomewhereClose = 999;

	CVector potentialPos;
	potentialPos.y = GetPosition().y - 3.5f;
	potentialPos.z = GetPosition().z;

	for (int yTry = 0; yTry < 15; yTry++) {
		potentialPos.x = GetPosition().x - 3.5f;

		for (int xTry = 0; xTry < 15; xTry++) {
			CPedPlacement::FindZCoorForPed(&potentialPos);
			if (!CWorld::TestSphereAgainstWorld(potentialPos, 0.6f, this, true, false, false, true, false, false)) {
				float potentialChangeSqr = (potentialPos - GetPosition()).MagnitudeSqr();
				veh = (CVehicle*)CWorld::TestSphereAgainstWorld(potentialPos, 0.6f, this, false, true, false, false, false, false);
				if (veh) {
					if (potentialChangeSqr < smallestDistNearVeh) {
						posNearVeh = potentialPos;
						putNearVeh = true;
						smallestDistNearVeh = potentialChangeSqr;
					}
				} else if (potentialChangeSqr < smallestDistSomewhereClose) {
					smallestDistSomewhereClose = potentialChangeSqr;
					posSomewhereClose = potentialPos;
					putSomewhereClose = true;
				}
			}
			potentialPos.x += 0.5f;
		}
		potentialPos.y += 0.5f;
	}

	if (!putSomewhereClose && !putNearVeh)
		return false;

	// We refrain from using posNearVeh, probably because of it may be top of the vehicle.
	if (putSomewhereClose) {
		SetPosition(posSomewhereClose);
	} else {
		CVector vehSize = veh->GetModelInfo()->GetColModel()->boundingBox.max;
		posNearVeh.z += vehSize.z;
		SetPosition(posNearVeh);
	}
	return true;
}

bool
CPed::WarpPedToNearLeaderOffScreen(void)
{
	bool teleported = false;
	if (GetIsOnScreen() || m_leaveCarTimer > CTimer::GetTimeInMilliseconds())
		return false;

	CVector warpToPos = m_leader->GetPosition();
	CVector distVec = warpToPos - GetPosition();
	float halfOfDist = distVec.Magnitude() * 0.5f;
	CVector halfNormalizedDist = distVec / halfOfDist;

	CVector appropriatePos = GetPosition();
	int tryCount = Min(10, (int)halfOfDist);
	for (int i = 0; i < tryCount; ++i) {
		appropriatePos += halfNormalizedDist;
		CVector zCorrectedPos = appropriatePos;
		CPedPlacement::FindZCoorForPed(&zCorrectedPos);

		if (Abs(zCorrectedPos.z - warpToPos.z) < 3.0f || Abs(zCorrectedPos.z - appropriatePos.z) < 3.0f) {
			appropriatePos.z = zCorrectedPos.z;
			if (!TheCamera.IsSphereVisible(appropriatePos, 0.6f)
				&& CWorld::GetIsLineOfSightClear(appropriatePos, warpToPos, true, true, false, true, false, false, false)
				&& !CWorld::TestSphereAgainstWorld(appropriatePos, 0.6f, this, true, true, false, true, false, false)) {
				teleported = true;
				Teleport(appropriatePos);
			}
		}
	}
	m_leaveCarTimer = CTimer::GetTimeInMilliseconds() + 3000;
	return teleported;
}

bool
CPed::WarpPedToNearEntityOffScreen(CEntity *warpTo)
{
	bool teleported = false;
	if (GetIsOnScreen() || m_leaveCarTimer > CTimer::GetTimeInMilliseconds())
		return false;

	CVector warpToPos = warpTo->GetPosition();
	CVector distVec = warpToPos - GetPosition();
	float halfOfDist = distVec.Magnitude() * 0.5f;
	CVector halfNormalizedDist = distVec / halfOfDist;

	CVector appropriatePos = GetPosition();
	int tryCount = Min(10, (int)halfOfDist);
	for (int i = 0; i < tryCount; ++i) {
		appropriatePos += halfNormalizedDist;
		CVector zCorrectedPos = appropriatePos;
		CPedPlacement::FindZCoorForPed(&zCorrectedPos);

		if (Abs(zCorrectedPos.z - warpToPos.z) < 3.0f || Abs(zCorrectedPos.z - appropriatePos.z) < 3.0f) {
			appropriatePos.z = zCorrectedPos.z;
			if (!TheCamera.IsSphereVisible(appropriatePos, 0.6f)
				&& CWorld::GetIsLineOfSightClear(appropriatePos, warpToPos, true, true, false, true, false, false, false)
				&& !CWorld::TestSphereAgainstWorld(appropriatePos, 0.6f, this, true, true, false, true, false, false)) {
				teleported = true;
				Teleport(appropriatePos);
			}
		}
	}
	m_leaveCarTimer = CTimer::GetTimeInMilliseconds() + 3000;
	return teleported;
}

int32
CPed::KillCharOnFootArmed(CVector &ourPos, CVector &targetPos, CVector &distWithTarget)
{
	bool killPlayerInNoPoliceZone = false;
	if (m_pedInObjective->IsPlayer() && CCullZones::NoPolice())
		killPlayerInNoPoliceZone = true;

	if (!bNotAllowedToDuck || killPlayerInNoPoliceZone) {
		if (m_nPedType == PEDTYPE_COP && !m_pedInObjective->GetWeapon()->IsTypeMelee())
			bNotAllowedToDuck = true;
	} else {
		if (!m_pedInObjective->bInVehicle) {
			if (m_pedInObjective->GetWeapon()->IsTypeMelee()) {
				bNotAllowedToDuck = false;
				bCrouchWhenShooting = false;
			} else if (DuckAndCover()) {
				return CANT_ATTACK;
			}
		} else {
			bNotAllowedToDuck = false;
			bCrouchWhenShooting = false;
		}
	}
	if (m_leaveCarTimer > CTimer::GetTimeInMilliseconds() && !bKindaStayInSamePlace) {
		SetMoveState(PEDMOVE_STILL);
		return CANT_ATTACK;
	}
	if (m_pedInObjective->IsPlayer()) {
		CPlayerPed *player = FindPlayerPed();
		if (m_nPedType == PEDTYPE_COP && player->Wanted()->m_bIgnoredByCops
			|| player->Wanted()->m_bIgnoredByEveryone
			|| m_pedInObjective->bIsInWater
			|| m_pedInObjective->m_nPedState == PED_ARRESTED) {

			if (m_nPedState != PED_ARREST_PLAYER)
				SetIdle();

			return CANT_ATTACK;
		}
	}
	if (m_pedInObjective->IsPlayer() && m_nPedType != PEDTYPE_COP
		&& CharCreatedBy != MISSION_CHAR && FindPlayerPed()->Wanted()->m_CurrentCops != 0) {
		SetObjective(OBJECTIVE_FLEE_ON_FOOT_TILL_SAFE);

		return CANT_ATTACK;
	}
	if (m_pedInObjective->m_fHealth <= 0.0f) {
		bObjectiveCompleted = true;
		bScriptObjectiveCompleted = true;
		return CANT_ATTACK;
	}
	CWeaponInfo *wepInfo = CWeaponInfo::GetWeaponInfo(GetWeapon()->m_eWeaponType);
	float wepRange = wepInfo->m_fRange;
	float wepRangeAdjusted = wepRange / 3.f;

	float distWithTargetSc = distWithTarget.Magnitude();
	if (m_pedInObjective->bInVehicle && !m_pedInObjective->BeingDraggedOutOfCar()) {
		CVehicle *vehOfTarget = m_pedInObjective->m_pMyVehicle;
		if (vehOfTarget->bIsInWater || vehOfTarget->GetStatus() == STATUS_PLAYER_DISABLED
			|| m_pedInObjective->IsPlayer() && CPad::GetPad(0)->ArePlayerControlsDisabled()) {
			SetIdle();
			return WATCH_UNTIL_HE_DISAPPEARS;
		}
		SetLookFlag(vehOfTarget, false);
		if (m_nMoveState == PEDMOVE_STILL && IsPedInControl())
			TurnBody();

		if (!EnteringCar()) {
			if (m_pedInObjective->m_nPedState != PED_ARRESTED) {
				if (m_attackTimer < CTimer::GetTimeInMilliseconds() && distWithTargetSc < wepRange && distWithTargetSc > 3.0f) {

					SetAttack(vehOfTarget);
					SetWeaponLockOnTarget(vehOfTarget);
					SetShootTimer(CGeneral::GetRandomNumberInRange(500, 2000));
					
					CVector2D dirVehGoing = vehOfTarget->m_vecMoveSpeed;
					if (dirVehGoing.Magnitude() > 0.2f) {
						CVector2D vehDist = GetPosition() - vehOfTarget->GetPosition();
						vehDist.Normalise();
						dirVehGoing.Normalise();
						if (DotProduct2D(vehDist, dirVehGoing) > 0.8f) {
							SetAttackTimer(CGeneral::GetRandomNumberInRange(200, 500));
							SetMoveState(PEDMOVE_STILL);
						}
						return ATTACK_IN_PROGRESS;
					}
					if (distWithTargetSc <= m_distanceToCountSeekDone) {
						SetAttackTimer(CGeneral::GetRandomNumberInRange(200, 500));
						SetMoveState(PEDMOVE_STILL);
					} else {
						SetAttackTimer(CGeneral::GetRandomNumberInRange(2000, 5000));
					}
					return ATTACK_IN_PROGRESS;
				} else if (m_nPedState != PED_ATTACK && !bKindaStayInSamePlace && !killPlayerInNoPoliceZone) {
					if (vehOfTarget) {
						if (m_nPedType == PEDTYPE_COP || vehOfTarget->bIsBus) {
							if (GetIntelligence()->FindTaskByType(TASK_COMPLEX_GO_TO_CAR_DOOR_AND_STAND_STILL) == nil)
								GetIntelligence()->AddTaskPrimaryMaybeInGroup(
									new CTaskComplexGoToCarDoorAndStandStill(vehOfTarget, PEDMOVE_RUN, true, 0, 0.5f, 4.0f, 50.0f, 30000), true);
						} else {
							m_vehDoor = 0;
							if (m_pedInObjective == vehOfTarget->pDriver || vehOfTarget->bIsBus) {
								m_vehDoor = CAR_DOOR_LF;
							} else if (m_pedInObjective == vehOfTarget->pPassengers[0]) {
								m_vehDoor = CAR_DOOR_RF;
							} else if (m_pedInObjective == vehOfTarget->pPassengers[1]) {
								m_vehDoor = CAR_DOOR_LR;
							} else if (m_pedInObjective == vehOfTarget->pPassengers[2]) {
								m_vehDoor = CAR_DOOR_RR;
							}
							if (GetIntelligence()->FindTaskByType(TASK_COMPLEX_GO_TO_CAR_DOOR_AND_STAND_STILL) == nil)
								GetIntelligence()->AddTaskPrimaryMaybeInGroup(
									new CTaskComplexGoToCarDoorAndStandStill(vehOfTarget, PEDMOVE_RUN, true, m_vehDoor, 0.5f, 4.0f, 50.0f, 30000), true);
						}
					}
				}
			}
		}
		return ATTACK_IN_PROGRESS;
	}
	if (m_nMoveState == PEDMOVE_STILL && IsPedInControl()) {
		SetLookFlag(m_pedInObjective, false);
		TurnBody();
	}
	if (m_nPedType == PEDTYPE_COP && m_pedInObjective->IsPlayer()) {
		float maxArrestDist = 1.5f;
		if (((CCopPed*)this)->m_bDragsPlayerFromCar) {
			if (m_nPedState == PED_FALL) {
				maxArrestDist = 3.5f;
			} else if (!BeingDraggedOutOfCar()) {
				((CCopPed*)this)->m_bDragsPlayerFromCar = 0;
			}
		}

		if (distWithTargetSc < maxArrestDist) {
			if (m_pedInObjective->m_getUpTimer > CTimer::GetTimeInMilliseconds()
				|| m_pedInObjective->BeingDraggedOutOfCar()) {

				((CCopPed*)this)->SetArrestPlayer(m_pedInObjective);
				return WATCH_UNTIL_HE_DISAPPEARS;
			}
		}
	}
	/*
	if (distWithTargetSc > 0.1f) {
		junk code
	} */

	if (m_shotTime != 0 && m_ceaseAttackTimer != 0) {
		if (CTimer::GetTimeInMilliseconds() > m_ceaseAttackTimer + m_shotTime) {
			ClearLookFlag();
			bObjectiveCompleted = true;
			m_shotTime = 0;
			return CANT_ATTACK;
		}
	}

	if (!bKindaStayInSamePlace && !bStopAndShoot && m_nPedState != PED_ATTACK && !bDuckAndCover && !killPlayerInNoPoliceZone) {
		if (distWithTargetSc > wepRange
			|| m_pedInObjective->m_getUpTimer > CTimer::GetTimeInMilliseconds()
			|| m_pedInObjective->m_nPedState == PED_ARRESTED
			|| m_pedInObjective->EnteringCar() && distWithTargetSc < 3.0f) {

			if (m_pedInObjective->EnteringCar())
				wepRangeAdjusted = 2.0f;

			if (bUsePedNodeSeek) {
				CVector bestCoords(0.0f, 0.0f, 0.0f);
				m_vecSeekPos = m_pedInObjective->GetPosition();

				if (!m_pNextPathNode)
					FindBestCoordsFromNodes(m_vecSeekPos, &bestCoords);

				if (m_pNextPathNode)
					m_vecSeekPos = CPathFind::TakeWidthIntoAccountForWandering(m_pNextPathNode, m_randomSeed);

				SetSeek(m_vecSeekPos, m_distanceToCountSeekDone);
			} else {
				SetSeek(m_pedInObjective, wepRangeAdjusted);
			}
			if (m_pedInObjective->m_pCurrentPhysSurface && distWithTargetSc < 5.0f) {
				bStopAndShoot = true;
				b158_8 = true;
				SetMoveState(PEDMOVE_STILL);
			} else if (b158_8) {
				bStopAndShoot = false;
				b158_8 = false;
			}
			return ATTACK_IN_PROGRESS;
		}
	}
	if (m_attackTimer < CTimer::GetTimeInMilliseconds()
		&& distWithTargetSc < wepRange && m_pedInObjective->m_nPedState != PED_GETUP && !m_pedInObjective->BeingDraggedOutOfCar()) {

		if (bIsDucking && !bCrouchWhenShooting) {
			CAnimBlendAssociation* duckAnim = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_DUCK_COWER);
			if (!duckAnim)
				duckAnim = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_DUCK_COWER);
			if (!duckAnim)
				duckAnim = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_WEAPON_CROUCH);

			if (duckAnim) {
				duckAnim->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE;
				duckAnim->m_BlendDelta = -4.0f;
			}
			bIsDucking = false;
			return CANT_ATTACK;
		}
		bObstacleShowedUpDuringKillObjective = false;
		SetAttack(m_pedInObjective);
		SetWeaponLockOnTarget(m_pedInObjective);
		SetShootTimer(CGeneral::GetRandomNumberInRange(600.0f, 1500.0f));

		int time;
		if (distWithTargetSc <= wepRangeAdjusted)
			time = CGeneral::GetRandomNumberInRange(100.0f, 500.0f);
		else
			time = CGeneral::GetRandomNumberInRange(1500.0f, 3000.0f);

		SetAttackTimer(time);
	} else {
		if (!m_pedInObjective->m_pCurrentPhysSurface && b158_8) {
			b158_8 = false;
			bStopAndShoot = false;
		}

		if (m_nPedState != PED_ATTACK && m_nPedState != PED_FIGHT) {
			if (bNotAllowedToDuck && bKindaStayInSamePlace && !bIsDucking && bCrouchWhenShooting) {
				SetDuck(CGeneral::GetRandomNumberInRange(1000, 5000), false);
				return CANT_ATTACK;
			}
			if (bObstacleShowedUpDuringKillObjective) {
				if (m_nPedType == PEDTYPE_COP) {
					if (GetWeapon()->m_eWeaponType > WEAPONTYPE_COLT45
						|| m_fleeFrom && m_fleeFrom->IsObject()) {
						wepRangeAdjusted = 6.0f;
					} else if (m_fleeFrom && m_fleeFrom->IsVehicle()) {
						wepRangeAdjusted = 4.0f;
					} else {
						wepRangeAdjusted = 2.0f;
					}
				} else {
					wepRangeAdjusted = 2.0f;
				}
			}
			if (distWithTargetSc <= wepRangeAdjusted) {
				SetMoveState(PEDMOVE_STILL);
				bIsPointingGunAt = true;
				if (m_nPedState != PED_AIMGUN && !bDuckAndCover) {
					m_attackTimer = CTimer::GetTimeInMilliseconds();
					SetIdle();
				}
			} else {
				if (m_nPedState != PED_SEEK_ENTITY && m_nPedState != PED_SEEK_POSITION
					&& !bStopAndShoot && !killPlayerInNoPoliceZone && !bKindaStayInSamePlace) {
					Say(SOUND_PED_ATTACK);
					SetSeek(m_pedInObjective, wepRangeAdjusted);
					bIsRunning = true;
					if (m_nPedType == PEDTYPE_COP && FindPlayerPed()->Wanted()->m_CurrentCops > 1) {
						if (CGeneral::GetRandomNumber() & 1)
							Say(SOUND_PED_GUNAIMEDAT3);
						else
							Say(SOUND_PED_GUNAIMEDAT2);
					}
				}
			}
		}
	}

	if (CTimer::GetTimeInMilliseconds() > m_nPedStateTimer)
		m_fRotationDest = CGeneral::GetRadianAngleBetweenPoints(m_pedInObjective->GetPosition().x, m_pedInObjective->GetPosition().y, GetPosition().x, GetPosition().y);
				
	return ATTACK_IN_PROGRESS;
}

int32
CPed::KillCharOnFootMelee(CVector &ourPos, CVector &targetPos, CVector &distWithTarget)
{
	bool killPlayerInNoPoliceZone = false;
	float distWithTargetSc = distWithTarget.Magnitude();
	CPlayerPed *victimPlayer = nil;
	if (m_pedInObjective->IsPlayer())
		victimPlayer = (CPlayerPed*)m_pedInObjective;

	if (victimPlayer) {
		if (CCullZones::NoPolice()
			|| m_pedInObjective->m_pCurrentPhysSurface
			&& m_pedInObjective->m_pCurrentPhysSurface != m_pCurrentPhysSurface
			&& distWithTargetSc < 5.f) {

			if (victimPlayer && victimPlayer->m_bSpeedTimerFlag && (IsGangMember() || m_nPedType == PEDTYPE_COP)
				&& CharCreatedBy != MISSION_CHAR) {
				GiveWeapon(WEAPONTYPE_COLT45, 1000, 1);
				SetCurrentWeapon(WEAPONTYPE_COLT45);
				SetMoveState(PEDMOVE_STILL);
				bStopAndShoot = true;
				b158_8 = true;
				return CANT_ATTACK;
			}
			killPlayerInNoPoliceZone = true;
		}
	}
	bNotAllowedToDuck = false;
	bStopAndShoot = false;
	b158_8 = false;
	if (m_leaveCarTimer > CTimer::GetTimeInMilliseconds() && !bKindaStayInSamePlace) {
		SetMoveState(PEDMOVE_STILL);
		return CANT_ATTACK;
	}
	if (victimPlayer) {
		CPlayerPed *player = FindPlayerPed();
		if (m_nPedType == PEDTYPE_COP && player->Wanted()->m_bIgnoredByCops
			|| player->Wanted()->m_bIgnoredByEveryone
			|| m_pedInObjective->bIsInWater
			|| m_pedInObjective->m_nPedState == PED_ARRESTED) {

			if (m_nPedState != PED_ARREST_PLAYER)
				SetIdle();

			return CANT_ATTACK;
		}
	}
	if (victimPlayer && m_nPedType != PEDTYPE_COP && CharCreatedBy != MISSION_CHAR
		&& FindPlayerPed()->Wanted()->m_CurrentCops != 0) {
		SetObjective(OBJECTIVE_FLEE_ON_FOOT_TILL_SAFE);

		return CANT_ATTACK;
	}
	if (m_pedInObjective->m_fHealth <= 0.0f) {
		bObjectiveCompleted = true;
		bScriptObjectiveCompleted = true;
		return ATTACK_IN_PROGRESS;
	}
	bool canReachVictim = false;
	uint32 endOfAttack = 0;
	CWeaponInfo *wepInfo = CWeaponInfo::GetWeaponInfo(GetWeapon()->m_eWeaponType);

	// Already calculated at the start
	// float distWithTargetSc = distWithTarget.Magnitude();
	float maxDistToKeep = 0.3f;
	float wepRange = wepInfo->m_fRange / 2.f;

	if (GetWeapon()->m_eWeaponType == WEAPONTYPE_UNARMED && !IsPlayer() && !(m_pedStats->m_flags & STAT_CAN_KICK))
		wepRange -= 0.3f;

	if (distWithTargetSc <= 5.f && victimPlayer && !victimPlayer->m_bNoPosForMeleeAttack) {
	
		if (m_pedInObjective->EnteringCar() && wepRange > 2.f) {
			m_vecSeekPos = m_pedInObjective->GetPosition();
			wepRange = 1.0f;
			maxDistToKeep = 0.5f;
		} else {
			int8 attackDir = victimPlayer->FindMeleeAttackPoint(this, distWithTarget, endOfAttack);
			if (attackDir == -1) {
				m_vecSeekPos = victimPlayer->GetPosition();
				maxDistToKeep = 4.0f;
			} else {
				victimPlayer->GetMeleeAttackCoords(m_vecSeekPos, attackDir, wepRange);
				distWithTargetSc = (m_vecSeekPos - GetPosition()).Magnitude();
				canReachVictim = true;
			}
		}
	} else {
		m_vecSeekPos = m_pedInObjective->GetPosition();
		maxDistToKeep = Max(0.8f, 0.9f * wepRange);
		wepRange *= 1.1f;
		if (victimPlayer && victimPlayer->m_bNoPosForMeleeAttack)
			victimPlayer = nil;
	}

	if (m_pedInObjective->bInVehicle && !m_pedInObjective->BeingDraggedOutOfCar()) {
		CVehicle *vehOfTarget = m_pedInObjective->m_pMyVehicle;

		if (vehOfTarget){
			if (vehOfTarget->bIsInWater || vehOfTarget->GetStatus() == STATUS_PLAYER_DISABLED
				|| m_pedInObjective->IsPlayer() && CPad::GetPad(0)->ArePlayerControlsDisabled()) {
				SetIdle();
				return WATCH_UNTIL_HE_DISAPPEARS;
			}
			SetLookFlag(vehOfTarget, false);

			if (!EnteringCar()) {
				if (m_pedInObjective->m_nPedState != PED_ARRESTED) {
					if (m_nPedState != PED_ATTACK && !bKindaStayInSamePlace && !killPlayerInNoPoliceZone) {

						if (m_nPedType == PEDTYPE_COP || vehOfTarget->bIsBus) {
							if (GetIntelligence()->FindTaskByType(TASK_COMPLEX_GO_TO_CAR_DOOR_AND_STAND_STILL) == nil)
								GetIntelligence()->AddTaskPrimaryMaybeInGroup(
									new CTaskComplexGoToCarDoorAndStandStill(vehOfTarget, PEDMOVE_RUN, true, 0, 0.5f, 4.0f, 50.0f, 30000), true);
						} else {
							m_vehDoor = 0;
							if (m_pedInObjective == vehOfTarget->pDriver || vehOfTarget->bIsBus) {
								m_vehDoor = CAR_DOOR_LF;
							} else if (m_pedInObjective == vehOfTarget->pPassengers[0]) {
								m_vehDoor = CAR_DOOR_RF;
							} else if (m_pedInObjective == vehOfTarget->pPassengers[1]) {
								m_vehDoor = CAR_DOOR_LR;
							} else if (m_pedInObjective == vehOfTarget->pPassengers[2]) {
								m_vehDoor = CAR_DOOR_RR;
							}
							if (GetIntelligence()->FindTaskByType(TASK_COMPLEX_GO_TO_CAR_DOOR_AND_STAND_STILL) == nil)
								GetIntelligence()->AddTaskPrimaryMaybeInGroup(
									new CTaskComplexGoToCarDoorAndStandStill(vehOfTarget, PEDMOVE_RUN, true, m_vehDoor, 0.5f, 4.0f, 50.0f, 30000), true);
						}
					}
				}
			}
		}
		return ATTACK_IN_PROGRESS;
	}
	if (m_nMoveState == PEDMOVE_STILL && IsPedInControl()) {
		SetLookFlag(m_pedInObjective, false);
		if(m_nPedState == PED_IDLE || m_nPedState == PED_ATTACK || m_nPedState == PED_FIGHT)
			TurnBody();
	}
	if (m_nPedType == PEDTYPE_COP && m_pedInObjective->IsPlayer()) {
		float maxArrestDist = 1.5f;
		if (((CCopPed*)this)->m_bDragsPlayerFromCar) {
			if (m_nPedState == PED_FALL) {
				maxArrestDist = 3.5f;
			} else if (!BeingDraggedOutOfCar()) {
				((CCopPed*)this)->m_bDragsPlayerFromCar = 0;
			}
		}

		if (distWithTargetSc < maxArrestDist) {
			if (m_pedInObjective->m_getUpTimer > CTimer::GetTimeInMilliseconds()
				|| m_pedInObjective->BeingDraggedOutOfCar()) {

				((CCopPed*)this)->SetArrestPlayer(m_pedInObjective);
				return WATCH_UNTIL_HE_DISAPPEARS;
			}
		}
	}

	if (distWithTargetSc > maxDistToKeep && !bKindaStayInSamePlace && m_nPedState != PED_ATTACK &&
		(m_nPedState != PED_FIGHT || m_curFightMove == FIGHTMOVE_IDLE) && !killPlayerInNoPoliceZone) {

		bool goForward = false;

		if (m_nPedState == PED_FIGHT) {
			if (canReachVictim) {
				CVector attackAndVictimDist = m_vecSeekPos - m_pedInObjective->GetPosition();
				CVector victimFarness = attackAndVictimDist / wepRange;
				CVector distVec = GetPosition() - m_pedInObjective->GetPosition();
				float distSqr = distVec.MagnitudeSqr();
				if (sq(wepRange) > distSqr && distSqr > 0.05f) {
					distVec.Normalise();
					if (DotProduct2D(victimFarness, distVec) > Cos(DEGTORAD(30.f)))
						goForward = true;
				}
			}
		}
		if (goForward) {
			CAnimBlendAssociation *shuffle = CAnimManager::BlendAnimation(GetClump(), ANIM_GROUP_DEFAULT, ANIM_ID_FIGHTSH_BWD, 16.f);
			if (shuffle) {
				m_curFightMove = FIGHTMOVE_SHUFFLE_F;
				shuffle->SetFinishCallback(FinishFightMoveCB, this);
				m_fightState = FIGHTSTATE_NO_MOVE;
				m_fightButtonPressure = 0;
				m_takeAStepAfterAttack = false;
			} else {
				// Missing SA shuffle: seek directly rather than waiting for a
				// finish callback that will never fire.
				m_curFightMove = FIGHTMOVE_IDLE;
				SetSeek(m_vecSeekPos, maxDistToKeep);
			}

		} else if (bUsePedNodeSeek && !canReachVictim) {
			CVector bestCoords(0.0f, 0.0f, 0.0f);

			if (!m_pNextPathNode)
				FindBestCoordsFromNodes(m_vecSeekPos, &bestCoords);

			if (m_pNextPathNode)
				m_vecSeekPos = CPathFind::TakeWidthIntoAccountForWandering(m_pNextPathNode, m_randomSeed);

			SetSeek(m_vecSeekPos, m_distanceToCountSeekDone);
		} else {
			if (canReachVictim)
				SetSeek(m_vecSeekPos, maxDistToKeep);
			else
				SetSeek(m_pedInObjective, maxDistToKeep);
		}
		return ATTACK_IN_PROGRESS;
	}

	if (m_attackTimer < CTimer::GetTimeInMilliseconds()
		&& distWithTargetSc < wepRange && m_pedInObjective->m_nPedState != PED_GETUP && !m_pedInObjective->BeingDraggedOutOfCar()) {

		if (bIsDucking) {
			CAnimBlendAssociation* duckAnim = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_DUCK_COWER);
			if (!duckAnim)
				duckAnim = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_DUCK_COWER);
			if (!duckAnim)
				duckAnim = RpAnimBlendClumpGetAssociation(GetClump(), ANIM_ID_WEAPON_CROUCH);

			if (duckAnim) {
				duckAnim->m_Flags |= ANIMATION_IS_BLEND_AUTO_REMOVE;
				duckAnim->m_BlendDelta = -4.0f;
			}
			bIsDucking = false;
			return CANT_ATTACK;
		}

		if (canReachVictim || !victimPlayer) {
			SetMoveState(PEDMOVE_STILL);
			SetAttack(m_pedInObjective);
			m_fRotationDest = CGeneral::GetRadianAngleBetweenPoints(
				m_pedInObjective->GetPosition().x, m_pedInObjective->GetPosition().y,
				GetPosition().x, GetPosition().y);
			SetShootTimer(CGeneral::GetRandomNumberInRange(0.f, 500.f));

			int time;
			if (endOfAttack <= CTimer::GetTimeInMilliseconds())
				time = CGeneral::GetRandomNumberInRange(100.0f, 1500.0f);
			else
				time = endOfAttack - CTimer::GetTimeInMilliseconds() + CGeneral::GetRandomNumberInRange(400.0f, 1500.0f);

			SetAttackTimer(time);
			bObstacleShowedUpDuringKillObjective = false;
		}
		return ATTACK_IN_PROGRESS;
	} else {
		if (!m_pedInObjective->m_pCurrentPhysSurface && m_pCurrentPhysSurface && b158_8) {
			b158_8 = false;
			bStopAndShoot = false;
		}

		if (m_nPedState != PED_ATTACK && m_nPedState != PED_FIGHT) {
			SetMoveState(PEDMOVE_STILL);
			if (GetWeapon()->m_eWeaponType == WEAPONTYPE_UNARMED)
				StartFightAttack(0);
		}
		return ATTACK_IN_PROGRESS;
	}
}

bool
CPed::CanBeDamagedByThisGangMember(CPed* who)
{
	return m_gangFlags & (1 << (who->m_nPedType - PEDTYPE_GANG1));
}
