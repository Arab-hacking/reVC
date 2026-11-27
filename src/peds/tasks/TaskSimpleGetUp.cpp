#include "common.h"

#include "TaskSimpleGetUp.h"

#include "AnimBlendAssociation.h"
#include "AnimBlendHierarchy.h"
#include "AnimManager.h"
#include "EventDamage.h"
#include "Pad.h"
#include "Ped.h"
#include "PedDamageResponseCalculator.h"
#include "PedIntelligence.h"
#include "PedIK.h"
#include "PedPlacement.h"
#include "RpAnimBlend.h"
#include "Timer.h"
#include "Vehicle.h"
#include "WeaponInfo.h"

CTaskSimpleGetUp::CTaskSimpleGetUp()
	: m_bHasPedGotUp(false), m_bIsFinished(false), m_Anim(nil)
{
}

CTaskSimpleGetUp::CTaskSimpleGetUp(const CTaskSimpleGetUp &other)
	: CTaskSimpleGetUp()
{
}

CTaskSimpleGetUp::~CTaskSimpleGetUp()
{
	if (m_Anim != nil)
		m_Anim->SetFinishCallback(CDefaultAnimCallback::DefaultAnimCB, nil);
}

void
CTaskSimpleGetUp::FinishGetUpAnimCB(CAnimBlendAssociation *blendAssoc, void *data)
{
	CTaskSimpleGetUp *task = (CTaskSimpleGetUp *)data;
	task->m_bHasPedGotUp = true;
	task->m_bIsFinished = true;
	task->m_Anim->m_BlendDelta = -1000.0f;
	task->m_Anim = nil;
}

bool
CTaskSimpleGetUp::ProcessPed(CPed *ped)
{
	ped->m_pedIK.m_flags |= CPedIK::SLOPE_PITCH;

	if (m_bIsFinished)
		return true;

	if (m_Anim == nil)
		StartAnim(ped);

	if (m_Anim == nil || m_Anim->m_CurrentTime < m_Anim->m_BlendHier->m_fTotalTime * 0.75f)
		ped->bFallenDown = true;

	return false;
}

bool
CTaskSimpleGetUp::MakeAbortable(CPed *ped, eAbortPriority priority, const CEvent *event)
{
	if (priority == ABORT_PRIORITY_URGENT) {
		bool bTooMuchTimePassed = false;
		bool bFatalDamage = false;

		if (event != nil) {
			if (event->GetEventType() == EVENT_DAMAGE) {
				const CEventDamage *eventDamage = (const CEventDamage *)event;
				if (eventDamage->m_damageResponse.m_bHealthZero && eventDamage->m_bAddToEventGroup)
					bFatalDamage = true;
				else if (CTimer::GetTimeInMilliseconds() - eventDamage->m_nStartTime > (uint32)(CTimer::GetTimeStepInMilliseconds() * 3.0f))
					bTooMuchTimePassed = true;
			} else if (event->GetEventPriority() < 61)
				return false;
		}

		if (m_Anim != nil && !bTooMuchTimePassed) {
			m_Anim->SetFinishCallback(CDefaultAnimCallback::DefaultAnimCB, nil);
			m_Anim = nil;
			m_bHasPedGotUp = true;
			m_bIsFinished = true;
		} else if (!m_bHasPedGotUp && !bFatalDamage)
			return false;

		return true;
	}

	if (priority == ABORT_PRIORITY_IMMEDIATE) {
		if (m_Anim != nil)
			m_Anim->m_BlendDelta = -1000.0f;
		return true;
	}

	return false;
}

bool
CTaskSimpleGetUp::StartAnim(CPed *ped)
{
	CEntity *blocking = CPedPlacement::IsPositionClearOfCars(&ped->GetPosition());
	CVehicle *vehicle = (blocking != nil && blocking->IsVehicle()) ? (CVehicle *)blocking : nil;

	if (vehicle == nil || vehicle->IsBike() || vehicle->IsSubQuad()) {
		// Nothing (heavy) on top of him: climb up. The reference also
		// re-tests a car the ped is ignoring collision with before
		// allowing this; that field does not exist here.
		const AnimationId animId = RpAnimBlendClumpGetFirstAssociation(ped->GetClump(), ANIMATION_IS_FRONT) != nil
			? ANIM_ID_GETUP_FRONT : ANIM_ID_GETUP_0;
		m_Anim = CAnimManager::BlendAnimation(ped->GetClump(), ANIM_GROUP_DEFAULT, animId, 1000.0f);
		if (m_Anim != nil)
			m_Anim->SetFinishCallback(FinishGetUpAnimCB, this);
		ped->SetPedState(PED_IDLE);
		return true;
	}

	// Crushed under a car: take run-over damage instead of getting up.
	// The reference also marks the ped stuck under the car; there is
	// no such flag here.
	float fDamage;
	if (ped->IsPlayer()) {
		if (CTimer::GetTimeStep() <= 0.0f)
			return false;
		fDamage = CTimer::GetTimeStep();
	} else {
		if (CPad::GetPad(0)->DisablePlayerControls == 0)
			return false;
		fDamage = 1000.0f;
	}

	CPedDamageResponseCalculator damageCalculator(vehicle, fDamage, WEAPONTYPE_RUNOVERBYCAR, PED_PIECE_TORSO, false);
	CEventDamage eventDamage(vehicle, CTimer::GetTimeInMilliseconds(), WEAPONTYPE_RUNOVERBYCAR, PED_PIECE_TORSO, 0, false, ped->bInVehicle);
	if (eventDamage.AffectsPed(ped))
		damageCalculator.ComputeDamageResponse(ped, eventDamage.m_damageResponse, true);
	else
		eventDamage.m_damageResponse.m_bDamageCalculated = true;
	ped->GetIntelligence()->GetEventGroup().Add(&eventDamage, false);

	return false;
}
