#include "common.h"

#include "TaskSimpleStandStill.h"
#include "AnimBlendAssociation.h"
#include "AnimManager.h"
#include "Ped.h"
#include "PlayerPed.h"
#include "RpAnimBlend.h"
#include "Timer.h"

// SA CTaskSimpleStandStill::CTaskSimpleStandStill (0x635C60).
CTaskSimpleStandStill::CTaskSimpleStandStill(int32 nTime, bool looped, bool bUseAnimIdleStance, float fBlendData)
{
	m_nTime = nTime;
	m_fBlendData = fBlendData;
	m_bLooped = looped;
	m_bUseAnimIdleStance = bUseAnimIdleStance;
}

// SA CTaskSimpleStandStill::MakeAbortable (0x635CA0): standing still can
// always be interrupted; a leisurely abort also stops the timer so the
// task reports finished on the next tick.
bool
CTaskSimpleStandStill::MakeAbortable(CPed *ped, eAbortPriority priority, const CEvent *event)
{
	(void)ped; (void)event;
	if(priority)
		return true;
	m_timer.m_nStartTime = CTimer::GetTimeInMilliseconds();
	m_timer.m_nInterval = -1;
	m_timer.m_bStarted = true;
	return true;
}

// SA CTaskSimpleStandStill::ProcessPed (0x635D10). (SA also forwards duck
// movement to CTaskSimpleDuck while ducking; that hook lands with the
// duck task - a ducking ped simply keeps its duck pose here.)
bool
CTaskSimpleStandStill::ProcessPed(CPed *ped)
{
	if(!m_timer.m_bStarted && m_timer.Start(m_nTime)){
		if(!ped->bInVehicle){
			ped->SetMoveState(PEDMOVE_STILL);
			ped->m_nSwimmingMoveState = PEDMOVE_STILL;
			if(!ped->bIsDucking)
				CAnimManager::BlendAnimation(ped->GetClump(), ped->m_animGroup, ANIM_ID_IDLE, m_fBlendData);
			if(ped->IsPlayer())
				((CPlayerPed*)ped)->GetPlayerData()->m_fMoveBlendRatio = 0.0f;
		}
	}

	if(!ped->bIsDucking)
		ped->SetMoveState(PEDMOVE_STILL);

	if(m_bUseAnimIdleStance){
		CAnimBlendAssociation *idleAssoc = RpAnimBlendClumpGetAssociation(ped->GetClump(), (uint32)ANIM_ID_IDLE);
		if(idleAssoc && idleAssoc->m_BlendAmount > 0.99f)
			return true;
	}

	if(m_bLooped || !m_timer.Reset())
		return false;

	return m_timer.IsOutOfTime();
}
