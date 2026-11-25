#include "common.h"

#include "PedDamageResponseCalculator.h"

#include "Entity.h"
#include "Ped.h"
#include "PlayerInfo.h"
#include "PlayerPed.h"
#include "Timer.h"
#include "World.h"

// SA CPedDamageResponseCalculator (0x4AD3F0). Adaptations for this
// codebase: the cheat branches are absent; the same-group/friends
// friendly-fire reduction in AccountForPedDamageStats needs CPedGroups
// and CPedIntelligence::AreFriends, neither ported yet, so only the
// stats scaling is applied; the kill rule delegates to
// CPed::WillForceDeathFrom, the ped-side twin of SA's rule that already
// lived in InflictDamage; pain speech (CTX_GLOBAL_PAIN_*) is skipped -
// the SA speech contexts are not ported yet.

float CPedDamageResponseCalculator::ms_damageFactor = 1000.0f;

CPedDamageResponseCalculator::CPedDamageResponseCalculator(const CEntity *entity, float fDamage, eWeaponType weaponType, ePedPieceTypes bodyPart, bool bSpeak)
: m_pDamager(entity), m_fDamageFactor(fDamage), m_weaponType(weaponType), m_bodyPart(bodyPart), m_bSpeak(bSpeak)
{
}

// 0x4AD430 (stats scaling only; see file header)
void
CPedDamageResponseCalculator::AccountForPedDamageStats(CPed *ped, CPedDamageResponse &response)
{
	(void)response;

	m_fDamageFactor *= ped->IsPlayer()
		? 0.33f
		: ped->m_pedStats->m_fDefendWeakness;
}

// 0x4AD550
void
CPedDamageResponseCalculator::AccountForPedArmour(CPed *ped, CPedDamageResponse &response)
{
	if(ped->m_fArmour == 0.0f)
		return;

	if(m_weaponType == WEAPONTYPE_DROWNING || m_weaponType == WEAPONTYPE_FALL)
		return;

	if(FindPlayerPed() == ped)
		CWorld::Players[CWorld::PlayerInFocus].m_nTimeLastArmourLoss = CTimer::GetTimeInMilliseconds();

	if(m_fDamageFactor > ped->m_fArmour){
		m_fDamageFactor = m_fDamageFactor - ped->m_fArmour;
		response.m_fDamageArmor = ped->m_fArmour;
		ped->m_fArmour = 0.0f;
	}else{
		response.m_fDamageArmor = m_fDamageFactor;
		ped->m_fArmour = ped->m_fArmour - m_fDamageFactor;
		m_fDamageFactor = 0.0f;
	}
}

// 0x4AD610: SA's rule lives in CPed::WillForceDeathFrom here.
bool
CPedDamageResponseCalculator::ComputeWillForceDeath(CPed *ped, CPedDamageResponse &response)
{
	(void)response;
	return ped->WillForceDeathFrom(m_weaponType, m_bodyPart, (CEntity*)m_pDamager, m_fDamageFactor);
}

// 0x4B3210 (minus cheats and pain speech)
void
CPedDamageResponseCalculator::ComputeWillKillPed(CPed *ped, CPedDamageResponse &response, bool bSpeak)
{
	(void)bSpeak;

	response.m_bForceDeath = ComputeWillForceDeath(ped, response);

	if(!response.m_bForceDeath && m_weaponType == WEAPONTYPE_FALL){
		float prevHP = ped->m_fHealth;
		ped->m_fHealth = Max(prevHP - m_fDamageFactor, 5.0f);
		response.m_bHealthZero = false;
		response.m_fDamageHealth = prevHP - ped->m_fHealth;
		return;
	}

	if(response.m_bForceDeath || ped->m_fHealth - m_fDamageFactor < 1.0f){
		response.m_fDamageHealth = ped->m_fHealth;
		response.m_bHealthZero = true;
		ped->m_fHealth = 0.0f;
		return;
	}

	response.m_bHealthZero = false;
	response.m_fDamageHealth = m_fDamageFactor;
	ped->m_fHealth = ped->m_fHealth - m_fDamageFactor;
}

// 0x4B5C2A
bool
CPedDamageResponseCalculator::IsBleedingWeapon(CPed *ped) const
{
	if(ped->IsPlayer())
		return false;

	if(m_weaponType == WEAPONTYPE_KNIFE || m_weaponType == WEAPONTYPE_KATANA || m_weaponType == WEAPONTYPE_CHAINSAW)
		return true;

	return false;
}

// 0x4B5AC0
void
CPedDamageResponseCalculator::ComputeDamageResponse(CPed *ped, CPedDamageResponse &response, bool bSpeak)
{
	if(response.m_bDamageCalculated)
		return;

	response = CPedDamageResponse();
	response.m_bDamageCalculated = true;
	response.m_bCheckIfAffectsPed = true;

	AccountForPedDamageStats(ped, response);
	AccountForPedArmour(ped, response);
	ComputeWillKillPed(ped, response, bSpeak);

	if(response.GetTotalDamage() <= 0.0f)
		return;

	if(IsBleedingWeapon(ped))
		ped->m_bleedCounter = 200;

	if(ped == FindPlayerPed())
		CWorld::Players[CWorld::PlayerInFocus].m_nTimeLastHealthLoss = CTimer::GetTimeInMilliseconds();

	ped->m_lastWepDam = m_weaponType;
	if(m_pDamager)
		ped->m_lastDamEntity = (CEntity*)m_pDamager;
}
