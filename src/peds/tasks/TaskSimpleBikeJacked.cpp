#include "common.h"

#include "TaskSimpleBikeJacked.h"

#include "AnimBlendAssociation.h"
#include "AnimManager.h"
#include "EventKnockOffBike.h"
#include "General.h"
#include "Ped.h"
#include "PedIntelligence.h"
#include "RpAnimBlend.h"
#include "TaskUtilityLineUpPedWithCar.h"
#include "Vehicle.h"
#include "VehicleAnimGroupData.h"
#include "World.h"

CTaskSimpleBikeJacked::CTaskSimpleBikeJacked(CVehicle *vehicle, uint32 door, uint32 time, CPed *carJacker, bool isVictimDriver)
	: m_animWasPlayed(false), m_firstAnim(nil), m_secondAnimId(ANIM_ID_NO_ANIMATION_SET),
	  m_vehicle(vehicle), m_door(door), m_time(time), m_jacker(carJacker), m_isVictimDriver(isVictimDriver),
	  m_taskUtilityLineUpPedWithCar(nil)
{
	if (m_vehicle != nil)
		m_vehicle->RegisterReference((CEntity **)&m_vehicle);
	if (m_jacker != nil)
		m_jacker->RegisterReference((CEntity **)&m_jacker);
}

CTaskSimpleBikeJacked::CTaskSimpleBikeJacked(const CTaskSimpleBikeJacked &other)
	: CTaskSimpleBikeJacked(other.m_vehicle, other.m_door, other.m_time, other.m_jacker, other.m_isVictimDriver)
{
}

CTaskSimpleBikeJacked::~CTaskSimpleBikeJacked()
{
	if (m_vehicle != nil)
		m_vehicle->CleanUpOldReference((CEntity **)&m_vehicle);
	if (m_jacker != nil)
		m_jacker->CleanUpOldReference((CEntity **)&m_jacker);
	delete m_taskUtilityLineUpPedWithCar;
	if (m_firstAnim != nil)
		m_firstAnim->SetFinishCallback(CDefaultAnimCallback::DefaultAnimCB, nil);
}

bool
CTaskSimpleBikeJacked::MakeAbortable(CPed *ped, eAbortPriority priority, const CEvent *event)
{
	if (priority == ABORT_PRIORITY_IMMEDIATE) {
		if (m_firstAnim != nil)
			m_firstAnim->m_BlendDelta = -1000.0f;
		return true;
	}
	return false;
}

void
CTaskSimpleBikeJacked::FinishAnimBikeHitCB(CAnimBlendAssociation *anim, void *data)
{
	CTaskSimpleBikeJacked *self = (CTaskSimpleBikeJacked *)data;
	self->m_firstAnim = nil;
	self->m_animWasPlayed = true;
	self->m_secondAnimId = (self->m_door == CAR_DOOR_RF || self->m_door == CAR_DOOR_LF)
		? ANIM_ID_BIKE_FALL_OFF : ANIM_ID_BIKE_FALLR;
}

bool
CTaskSimpleBikeJacked::ProcessPed(CPed *ped)
{
	if (m_vehicle == nil)
		return true;

	if (!ped->bInVehicle && m_firstAnim == nil)
		return true;

	if (!m_animWasPlayed) {
		if (m_vehicle->IsDriver(ped))
			ped->SetRadioStation();

		if (m_firstAnim == nil) {
			bool bCanPlay = true;
			if (m_jacker != nil) {
				// Wait for the jacker's pull-out/get-in to get going
				// before the hit lands.
				if (CAnimBlendAssociation *anim = RpAnimBlendClumpGetAssociation(ped->GetClump(), ANIM_ID_CAR_PULLOUT_RHS))
					bCanPlay = anim->m_CurrentTime > 0.3f;
				else if (CAnimBlendAssociation *anim2 = RpAnimBlendClumpGetAssociation(ped->GetClump(), ANIM_ID_CAR_PULLOUT_LHS))
					bCanPlay = anim2->m_CurrentTime > 0.3f;
				else if (CAnimBlendAssociation *anim3 = RpAnimBlendClumpGetAssociation(ped->GetClump(), ANIM_ID_CAR_GETIN_BIKE_FRONT))
					bCanPlay = anim3->m_CurrentTime > 0.3f;
				// The reference has the two peds shout here too.
			}
			if (bCanPlay) {
				// Ride anims play out of the bike's own group.
				const AssocGroupId rideGroup = m_vehicle->GetAnimGroup().GetGroup(ANIM_ID_BIKE_HIT);
				m_firstAnim = CAnimManager::BlendAnimation(ped->GetClump(), rideGroup, ANIM_ID_BIKE_HIT);
				if (m_firstAnim != nil)
					m_firstAnim->SetFinishCallback(FinishAnimBikeHitCB, this);
			}
		}

		if (m_taskUtilityLineUpPedWithCar == nil)
			m_taskUtilityLineUpPedWithCar = new CTaskUtilityLineUpPedWithCar(CVector(), 0, 0, (int32)m_door);

		return false;
	}

	if (m_secondAnimId == ANIM_ID_NO_ANIMATION_SET)
		return true;

	CAnimManager::BlendAnimation(ped->GetClump(), ANIM_GROUP_DEFAULT, m_secondAnimId);

	// Tell the ped he's off the bike (and the bike's his attacker's).
	CEventKnockOffBike *event = new CEventKnockOffBike(m_vehicle, m_vehicle->m_vecMoveSpeed,
		CVector(0.0f, 0.0f, 0.0f), 0.0f, 0.0f, KNOCK_OFF_TYPE_NONE_1, 0, (int32)m_time,
		m_jacker, m_isVictimDriver, true);
	ped->GetIntelligence()->GetEventGroup().Add(event, false);

	m_secondAnimId = ANIM_ID_NO_ANIMATION_SET;

	return false;
}

bool
CTaskSimpleBikeJacked::SetPedPosition(CPed *ped)
{
	if (ped->bInVehicle)
		ped->SetPedPositionInCar();
	return true;
}
