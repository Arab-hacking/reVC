#include "common.h"

#include "TaskSimpleCarJumpOut.h"

#include <cmath>

#include "AnimBlendAssociation.h"
#include "AnimManager.h"
#include "Automobile.h"
#include "CarEnterExit.h"
#include "DamageManager.h"
#include "Event.h"
#include "General.h"
#include "Ped.h"
#include "PedIK.h"
#include "PedPlacement.h"
#include "TaskSimpleCarSetPedOut.h"
#include "TaskUtilityLineUpPedWithCar.h"
#include "Timer.h"
#include "Vehicle.h"
#include "VehicleAnimGroupData.h"
#include "World.h"

CTaskSimpleCarJumpOut::CTaskSimpleCarJumpOut(CVehicle *veh, uint32 door, CTaskUtilityLineUpPedWithCar *lineUpTask)
	: m_finishedAnim(false), m_anim(nil), m_veh(veh), m_door(door), m_doorFlagsToClear(0), m_numGettingInToClear(0), m_lineUpTask(lineUpTask)
{
	m_initialVehSpeed = m_veh != nil ? m_veh->m_vecMoveSpeed.Magnitude() : 0.0f;
	if (m_veh != nil)
		m_veh->RegisterReference((CEntity **)&m_veh);
}

CTaskSimpleCarJumpOut::CTaskSimpleCarJumpOut(const CTaskSimpleCarJumpOut &other)
	: CTaskSimpleCarJumpOut(other.m_veh, other.m_door, other.m_lineUpTask)
{
	m_doorFlagsToClear = other.m_doorFlagsToClear;
	m_numGettingInToClear = other.m_numGettingInToClear;
}

CTaskSimpleCarJumpOut::~CTaskSimpleCarJumpOut()
{
	if (m_veh != nil)
		m_veh->CleanUpOldReference((CEntity **)&m_veh);
	if (m_anim != nil)
		m_anim->SetFinishCallback(CDefaultAnimCallback::DefaultAnimCB, nil);
}

void
CTaskSimpleCarJumpOut::FinishAnimCarJumpOutCB(CAnimBlendAssociation *anim, void *data)
{
	CTaskSimpleCarJumpOut *self = (CTaskSimpleCarJumpOut *)data;
	self->m_finishedAnim = true;
	self->m_anim = nil;
}

void
CTaskSimpleCarJumpOut::StartAnim(const CPed *ped)
{
	AssocGroupId groupId;
	AnimationId animId;
	ComputeAnimID(groupId, animId);
	m_anim = CAnimManager::BlendAnimation(ped->GetClump(), groupId, animId, 8.0f);
	if (m_anim != nil)
		m_anim->SetFinishCallback(FinishAnimCarJumpOutCB, this);
}

void
CTaskSimpleCarJumpOut::ComputeAnimID(AssocGroupId &outGroup, AnimationId &outAnimId)
{
	outAnimId = (m_door == CAR_DOOR_RF || m_door == CAR_DOOR_RR) ? ANIM_ID_CAR_ROLLOUT_RHS : ANIM_ID_CAR_ROLLOUT_LHS;
	outGroup = m_veh->GetAnimGroup().GetGroup(outAnimId);
}

bool
CTaskSimpleCarJumpOut::MakeAbortable(CPed *ped, eAbortPriority priority, const CEvent *event)
{
	switch (priority) {
	case ABORT_PRIORITY_IMMEDIATE:
		if (m_anim != nil)
			m_anim->m_BlendDelta = -1000.0f;
		return true;
	case ABORT_PRIORITY_URGENT:
		if (event != nil && m_anim != nil && !ped->bInVehicle) {
			const eEventType type = event->GetEventType();
			if (type == EVENT_IN_AIR || type == EVENT_IN_WATER) {
				m_anim->m_BlendDelta = -2.0f;
				m_finishedAnim = true;
				return true;
			}
		}
		return false;
	default:
		return false;
	}
}

bool
CTaskSimpleCarJumpOut::ProcessPed(CPed *ped)
{
	if (m_finishedAnim)
		return true;

	if (m_veh == nil)
		return true;

	// While the roll-out is young the ped keeps some of the car's
	// speed, decaying a little every frame.
	{
		bool bDrifting = false;
		if (m_anim == nil)
			bDrifting = true;
		else if (m_veh->IsBike() || m_veh->IsSubQuad())
			bDrifting = m_anim->m_CurrentTime < 0.7333333f;
		else if (m_veh->IsAutomobile())
			bDrifting = m_anim->m_CurrentTime < 0.45f;

		if (bDrifting) {
			m_initialVehSpeed *= std::pow(0.96f, CTimer::GetTimeStep());
			ped->m_vecAnimMoveDelta.y += m_initialVehSpeed * CTimer::GetTimeStep();
		}
	}

	if (!ped->bInVehicle) {
		if (m_veh->IsAutomobile()) {
			ped->m_pedIK.m_flags |= CPedIK::AIMS_WITH_ARM;
			ped->bFallenDown = true;
		}
		return m_anim == nil;
	}

	if (m_anim == nil) {
		StartAnim(ped);
		return false;
	}

	// Once the door-swing window has passed, put him properly out.
	const float threshold = (m_veh->IsBike() || m_veh->IsSubQuad()) ? 0.35f : (m_veh->IsPlane() ? 0.15f : 0.07f);
	if (threshold >= m_anim->m_CurrentTime) {
		if (m_veh->IsAutomobile())
			m_veh->ProcessOpenDoor(m_door, (uint32)m_anim->m_AnimId, m_anim->m_CurrentTime);
		return false;
	}

	{
		CTaskSimpleCarSetPedOut getOutTask(m_veh, (int32)m_door, true);
		getOutTask.m_nDoorFlagsToClear = m_doorFlagsToClear;
		getOutTask.m_nNumGettingInToClear = m_numGettingInToClear;
		getOutTask.ProcessPed(ped);
	}

	CCarEnterExit::RemoveCarSitAnim(ped);

	{
		const float rot = m_veh->GetHeading();
		const float heading = m_veh->IsUpsideDown() ? CGeneral::LimitRadianAngle(rot + (float)M_PI) : rot;
		ped->SetHeading(heading);
	}
	ped->m_vecMoveSpeed = m_veh->m_vecMoveSpeed;
	ped->bKnockedOffBike = true;
	if (m_veh->IsAutomobile()) {
		eDoors door;
		switch (m_door) {
		case CAR_DOOR_RF: door = DOOR_FRONT_RIGHT; break;
		case CAR_DOOR_RR: door = DOOR_REAR_RIGHT; break;
		case CAR_DOOR_LF: door = DOOR_FRONT_LEFT; break;
		default:          door = DOOR_REAR_LEFT; break;
		}
		((CAutomobile *)m_veh)->Damage.SetDoorOpen(door);
	}

	return false;
}

bool
CTaskSimpleCarJumpOut::SetPedPosition(CPed *ped)
{
	if (!ped->bInVehicle)
		return false;

	ped->SetPedPositionInCar();

	if (m_anim != nil && (m_anim->m_AnimId == ANIM_ID_CAR_ROLLOUT_RHS || m_anim->m_AnimId == ANIM_ID_CAR_ROLLOUT_LHS)) {
		const CVector pedPos = ped->GetPosition();

		CVector adjusted = pedPos;
		CWorld::pIgnoreEntity = m_veh;
		const bool found = CPedPlacement::FindZCoorForPed(&adjusted);
		CWorld::pIgnoreEntity = nil;

		if (found && adjusted.z > pedPos.z) {
			const float timeDivider = (m_veh->IsBike() || m_veh->IsSubQuad()) ? 0.27f : 0.07f;
			CVector pos = pedPos;
			pos.z += m_anim->m_CurrentTime / timeDivider * (adjusted.z - pedPos.z);
			ped->GetMatrix().GetPosition() = pos;
		}
	}

	return true;
}
