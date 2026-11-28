#pragma once

// SA CPedDamageResponseCalculator: turns "hit by X for N with weapon W
// in body part P" into a CPedDamageResponse - applying the ped's damage
// stats, its armour, the outright-kill rules and finally the health.
// CPed::InflictDamage in this codebase already applies damage its own
// merged way; the calculator exists for the SA call sites that compute
// a response without applying it (vehicle/script/task damage), and its
// kill rule is the ped's own WillForceDeathFrom.

#include "PedDamageResponse.h"
#include "WeaponType.h"
#include "PedNode.h"

class CEntity;
class CPed;

class CPedDamageResponseCalculator
{
public:
	const CEntity *m_pDamager;
	float m_fDamageFactor;
	eWeaponType m_weaponType;
	ePedPieceTypes m_bodyPart;
	bool m_bSpeak;

	static float ms_damageFactor;

public:
	CPedDamageResponseCalculator(const CEntity *entity, float fDamage, eWeaponType weaponType, ePedPieceTypes bodyPart, bool bSpeak);

	void AccountForPedDamageStats(CPed *ped, CPedDamageResponse &response);
	void AccountForPedArmour(CPed *ped, CPedDamageResponse &response);
	bool ComputeWillForceDeath(CPed *ped, CPedDamageResponse &response);
	void ComputeWillKillPed(CPed *ped, CPedDamageResponse &response, bool bSpeak);
	bool IsBleedingWeapon(CPed *ped) const;
	void ComputeDamageResponse(CPed *ped, CPedDamageResponse &response, bool bSpeak);
};
