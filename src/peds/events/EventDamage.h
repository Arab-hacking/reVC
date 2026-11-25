#pragma once

// SA CEventDamage: an editable-response event carrying the damage
// bookkeeping (CPedDamageResponse) plus the weapon/body-part/direction
// of the hit. In SA the damage itself is applied by the calculator in
// ComputeDamageResponseIfAffectsPed; in this codebase CPed::InflictDamage
// already applies weapon damage (death anims, body parts, kill
// registration) at the existing call sites, so here the calculator runs
// only where a caller explicitly asks for an SA-style response, and
// CWeapon::GenerateDamageEvent pre-marks the response with the raw
// incoming damage. The response task is decided by
// CEventHandler::ComputeDamageResponse. SA's stealth-kill and ped-group
// branches of AffectsPed are omitted (no stealth task, no CPedGroups in
// this codebase yet), as are ComputeDeathAnim/ComputeDamageAnim -
// InflictDamage owns the damage/death animations locally.

#include "EventEditableResponse.h"
#include "AnimationEnums.h"
#include "PedDamageResponse.h"
#include "PedNode.h"
#include "WeaponType.h"

class CPedDamageResponseCalculator;
class CEntity;

class CEventDamage : public CEventEditableResponse
{
public:
	CEntity *m_pSourceEntity;
	uint32 m_nStartTime;
	eWeaponType m_weaponType;
	ePedPieceTypes m_pedPieceType;
	uint8 m_ucDirection; // see eFallDir: 0 fwd, 1 right, 2 back, 3 left
	uint8 m_bJumpedOutOfMovingCar : 1;
	uint8 m_bFallDown : 1; // victim falls on the ground when damaged
	uint8 m_bAnimAdded : 1;
	uint8 m_bWitnessedInVehicle : 1;
	uint8 m_bStealthMode : 1;
	AssocGroupId m_nAnimGroup;
	AnimationId m_nAnimID;
	float m_fAnimBlend;
	float m_fAnimSpeed;
	CPedDamageResponse m_damageResponse;

public:
	CEventDamage(const CEventDamage &event);
	CEventDamage(CEntity *source, uint32 startTime, eWeaponType weaponType, ePedPieceTypes pieceHit, uint8 direction, bool bJumpedOutOfMovingCar, bool bPedInVehicle);
	~CEventDamage();

	eEventType GetEventType() const override { return EVENT_DAMAGE; }
	int32 GetEventPriority() const override { return 65; }
	int32 GetLifeTime() override { return 0; }
	bool AffectsPed(CPed *ped) override;
	bool IsCriminalEvent() override;
	void ReportCriminalEvent(CPed *ped) override;
	CEntity *GetSourceEntity() const override { return m_pSourceEntity; }
	bool TakesPriorityOver(const CEvent &refEvent) override;
	float GetLocalSoundLevel() override { return 55.0f; }
	bool DoInformVehicleOccupants(CPed *ped) override;
	bool CanBeInterruptedBySameEvent() override { return true; }
	CEventEditableResponse *CloneEditable() const override;

	void From(const CEventDamage &event);
	void ProcessDamage(CPed *ped);
	void ComputeBodyPartToRemove(int32 &pedNode);

	// Either computes the damage, or sets it as computed (without
	// computing it) - very common logic in the SA code.
	void ComputeDamageResponseIfAffectsPed(CPed *ped, CPedDamageResponseCalculator &calculator, bool bSpeak);

	bool IsSameEventForAI(const CEventDamage &o) const { return o.m_pSourceEntity == m_pSourceEntity && o.m_weaponType == m_weaponType; }
	bool HasKilledPed() const { return m_damageResponse.m_bHealthZero && m_bAddToEventGroup; }
	bool WasHeadShot() const;

	bool GetAnimAdded() const { return m_bAnimAdded; }
	AnimationId GetAnimId() const { return m_nAnimID; }
	AssocGroupId GetAnimGroup() const { return m_nAnimGroup; }
	const CPedDamageResponse &GetDamageResponse() const { return m_damageResponse; }
};

VALIDATE_SIZE(CEventDamage, 0x44);
