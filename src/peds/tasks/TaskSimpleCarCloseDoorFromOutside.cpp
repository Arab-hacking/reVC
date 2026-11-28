#include "common.h"

#include "TaskSimpleCarCloseDoorFromOutside.h"

#include "AnimBlendAssociation.h"
#include "AnimManager.h"
#include "Automobile.h"
#include "DamageManager.h"
#include "Pad.h"
#include "Ped.h"
#include "PedPlacement.h"
#include "PlayerInfo.h"
#include "TaskUtilityLineUpPedWithCar.h"
#include "Vehicle.h"
#include "VehicleAnimGroupData.h"

CTaskSimpleCarCloseDoorFromOutside::CTaskSimpleCarCloseDoorFromOutside(CVehicle *veh, uint32 door, CTaskUtilityLineUpPedWithCar *lineUpTask)
	: m_veh(veh), m_door(door), m_lineUpTask(lineUpTask), m_taskStoreAnim(nil)
{
	if (m_veh != nil)
		m_veh->RegisterReference((CEntity **)&m_veh);
}

CTaskSimpleCarCloseDoorFromOutside::CTaskSimpleCarCloseDoorFromOutside(const CTaskSimpleCarCloseDoorFromOutside &other)
	: CTaskSimpleCarCloseDoorFromOutside(other.m_veh, other.m_door, other.m_lineUpTask)
{
}

CTaskSimpleCarCloseDoorFromOutside::~CTaskSimpleCarCloseDoorFromOutside()
{
	if (m_veh != nil)
		m_veh->CleanUpOldReference((CEntity **)&m_veh);
	if (m_anim != nil)
		m_anim->SetFinishCallback(CDefaultAnimCallback::DefaultAnimCB, nil);
}

void
CTaskSimpleCarCloseDoorFromOutside::FinishAnimCarCloseDoorFromOutsideCB(CAnimBlendAssociation *anim, void *data)
{
	CTaskSimpleCarCloseDoorFromOutside *self = (CTaskSimpleCarCloseDoorFromOutside *)data;
	self->m_animFinished = true;
	self->m_anim = nil;
}

// The door node this task talks about, as a damage-manager door.
static eDoors DoorNodeToDoor(uint32 doorNode)
{
	switch (doorNode) {
	case CAR_DOOR_RF: return DOOR_FRONT_RIGHT;
	case CAR_DOOR_RR: return DOOR_REAR_RIGHT;
	case CAR_DOOR_LF: return DOOR_FRONT_LEFT;
	default:          return DOOR_REAR_LEFT;
	}
}

// The close animation for the door, and its group.
static AnimationId CloseAnimForDoor(uint32 doorNode)
{
	switch (doorNode) {
	case CAR_DOOR_RF: return ANIM_ID_CAR_CLOSE_RHS_0;
	case CAR_DOOR_RR: return ANIM_ID_CAR_CLOSE_RHS_1;
	case CAR_DOOR_LF: return ANIM_ID_CAR_CLOSE_LHS_0;
	default:          return ANIM_ID_CAR_CLOSE_LHS_1;
	}
}

void
CTaskSimpleCarCloseDoorFromOutside::StartAnim(CPed *ped)
{
	const CVector pedPos = ped->GetPosition();
	CVector adjustedPos = pedPos;
	if (CPedPlacement::FindZCoorForPed(&adjustedPos) && pedPos.z - 1.0f >= adjustedPos.z) {
		// Well above the ground: he isn't standing by the car at all.
		m_animFinished = true;
		return;
	}

	m_veh->m_nFakePhysics = 0;
	m_veh->m_vecMoveSpeed += CVector(0.001f, 0.001f, 0.001f);
	m_veh->m_vecTurnSpeed += CVector(0.001f, 0.001f, 0.001f);

	bool skipClose = false;
	if (ped->IsPlayer()) {
		CPad *pad = CPad::GetPad(0);
		if (pad->DisablePlayerControls == 0 && m_veh->m_nDoorLock != CARLOCK_FORCE_SHUT_DOORS) {
			// The player is already trying to move away: let him.
			if (pad->GetTarget() || pad->GetLeftStickX() != 0 || pad->GetLeftStickY() != 0 ||
			    pad->NewState.DPadUp || pad->NewState.DPadDown || pad->NewState.DPadLeft || pad->NewState.DPadRight)
				skipClose = true;
		}
		if (!skipClose) {
			float engineDamage = 0.0f;
			if (m_veh->IsAutomobile())
				engineDamage = ((CAutomobile *)m_veh)->Damage.GetEngineStatus();
			if (m_veh->m_pCarFire != nil || engineDamage >= 225.0f)
				skipClose = true;
		}
	}

	if (skipClose) {
		// Leave the door as it stands and be done.
		if (m_veh->IsAutomobile())
			((CAutomobile *)m_veh)->Damage.SetDoorOpen(DoorNodeToDoor(m_door));
		m_animFinished = true;
		return;
	}

	const AnimationId animId = CloseAnimForDoor(m_door);
	m_anim = CAnimManager::BlendAnimation(ped->GetClump(), m_veh->GetAnimGroup().GetGroup(animId), animId, 1000.0f);
	if (m_anim != nil)
		m_anim->SetFinishCallback(FinishAnimCarCloseDoorFromOutsideCB, this);
}

void
CTaskSimpleCarCloseDoorFromOutside::ProcessDoorOpen()
{
	m_veh->ProcessOpenDoor(m_door, (uint32)CloseAnimForDoor(m_door), 1.0f);
}

bool
CTaskSimpleCarCloseDoorFromOutside::MakeAbortable(CPed *ped, eAbortPriority priority, const CEvent *event)
{
	if (priority == ABORT_PRIORITY_IMMEDIATE) {
		if (m_anim != nil)
			m_anim->m_BlendDelta = -1000.0f;
		if (m_veh != nil)
			ProcessDoorOpen();
		return true;
	}
	return false;
}

bool
CTaskSimpleCarCloseDoorFromOutside::ProcessPed(CPed *ped)
{
	if (m_animFinished)
		return true;

	if (m_veh == nil || m_veh->GetAnimGroup().m_specialFlags.bDontCloseDoorAfterGettingOut)
		return true;

	if (m_anim == nil) {
		StartAnim(ped);
		if (m_anim == nil)
			return true;
	}

	m_veh->ProcessOpenDoor(m_door, (uint32)m_anim->m_AnimId, m_anim->m_CurrentTime);
	return false;
}

bool
CTaskSimpleCarCloseDoorFromOutside::SetPedPosition(CPed *ped)
{
	m_lineUpTask->ProcessPed(ped, m_veh, m_anim);
	return true;
}
