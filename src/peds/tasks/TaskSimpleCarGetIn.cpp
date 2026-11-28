#include "common.h"

#include "TaskSimpleCarGetIn.h"

#include "AnimBlendAssociation.h"
#include "AnimManager.h"
#include "Bike.h"
#include "Ped.h"
#include "TaskUtilityLineUpPedWithCar.h"
#include "Timer.h"
#include "Vehicle.h"
#include "VehicleAnimGroupData.h"

CTaskSimpleCarGetIn::CTaskSimpleCarGetIn(CVehicle *vehicle, uint32 door, CTaskUtilityLineUpPedWithCar *task)
	: m_veh(vehicle), m_door(door), m_lineUpTask(task), m_anim(nil), m_finished(false)
{
	if (m_veh != nil)
		m_veh->RegisterReference((CEntity **)&m_veh);
}

CTaskSimpleCarGetIn::CTaskSimpleCarGetIn(const CTaskSimpleCarGetIn &other)
	: CTaskSimpleCarGetIn(other.m_veh, other.m_door, other.m_lineUpTask)
{
}

CTaskSimpleCarGetIn::~CTaskSimpleCarGetIn()
{
	if (m_veh != nil)
		m_veh->CleanUpOldReference((CEntity **)&m_veh);
}

void
CTaskSimpleCarGetIn::FinishAnimCarGetInCB(CAnimBlendAssociation *anim, void *data)
{
	CTaskSimpleCarGetIn *self = (CTaskSimpleCarGetIn *)data;
	self->m_finished = true;
	self->m_anim = nil;
}

void
CTaskSimpleCarGetIn::StartAnim(CPed *ped)
{
	AnimationId animId;
	switch (m_door) {
	case CAR_DOOR_RF:
		animId = ANIM_ID_CAR_GETIN_RHS_0;
		break;
	case CAR_DOOR_RR:
		animId = ANIM_ID_CAR_GETIN_RHS_1;
		break;
	case CAR_DOOR_LF:
		animId = ANIM_ID_CAR_GETIN_LHS_0;
		break;
	case CAR_DOOR_LR:
		animId = ANIM_ID_CAR_GETIN_LHS_1;
		break;
	default: // CAR_WINDSCREEN: over the front of a bike
		animId = ANIM_ID_CAR_GETIN_BIKE_FRONT;
		break;
	}

	AssocGroupId grpId = m_veh->GetAnimGroup().GetGroup(animId);
	if (grpId == ANIM_GROUP_CONVCARANIMS && m_door == CAR_DOOR_LF) {
		// A convertible with its driver door open (or gone) uses the
		// standard get-in instead of the climb-over one.
		if (m_veh->IsDoorMissing(CAR_DOOR_LF) || !m_veh->IsDoorClosed(CAR_DOOR_LF))
			grpId = ANIM_GROUP_STDCARAMIMS;
	}

	m_anim = CAnimManager::BlendAnimation(ped->GetClump(), grpId, animId, 4.0f);
	if (m_veh->IsBike())
		REVC_LOG_INFO("Bike anim: get-in vehGroup=%d door=%d anim=%d group=%d clip=%s", (int32)m_veh->GetAnimGroupId(), (int32)m_door, animId, grpId, m_anim ? "found" : "MISSING");
	if (m_anim != nil)
		m_anim->SetFinishCallback(FinishAnimCarGetInCB, this);
	else
		// No clip exists for this vehicle/group (a missing bike
		// animation leaves the classic mount unported): sat down
		// anyway rather than hanging the entry forever.
		m_finished = true;
}

bool
CTaskSimpleCarGetIn::MakeAbortable(CPed *ped, eAbortPriority priority, const CEvent *event)
{
	if (priority == ABORT_PRIORITY_IMMEDIATE) {
		if (m_anim != nil)
			m_anim->m_BlendDelta = -1000.0f;
		return true;
	}
	return false;
}

bool
CTaskSimpleCarGetIn::ProcessPed(CPed *ped)
{
	if (m_veh == nil)
		return true;

	if (m_finished) {
		ped->RemoveWeaponWhenEnteringVehicle();
		return true;
	}

	if (m_anim == nil)
		StartAnim(ped);

	// Getting onto a BMX pushes it forward a little as the ped mounts.
	if (m_veh != nil && m_veh->m_nVehicleType == VEHICLE_TYPE_BMX && m_anim != nil
		&& (m_anim->m_Flags & ANIMATION_IS_PLAYING)) {
		switch ((AnimationId)m_anim->m_AnimId) {
		case ANIM_ID_CAR_GETIN_LHS_0:
		case ANIM_ID_CAR_GETIN_RHS_0:
		case ANIM_ID_CAR_GETIN_LHS_1:
		case ANIM_ID_CAR_GETIN_RHS_1:
			if (30.0f / 30.0f < m_anim->m_CurrentTime)
				m_veh->ApplyMoveForce(m_veh->GetMatrix().GetForward() * CTimer::GetTimeStep() * m_veh->m_fMass * 0.001f);
			break;
		default:
			break;
		}
	}

	return false;
}

bool
CTaskSimpleCarGetIn::SetPedPosition(CPed *ped)
{
	m_lineUpTask->ProcessPed(ped, m_veh, m_anim);
	return true;
}
