#ifndef __GTA_PLAYERPEDDATA_H__
#define __GTA_PLAYERPEDDATA_H__

#include "PedType.h"

class CWanted;
class CCopPed;
class CEntity;
class CPed;

// SA CPlayerPedData (PlayerPedData.h), the block of state that belongs to a
// player rather than to the ped body: the wanted level, the sprint budget, the
// breath meter, the group, the drunkenness timers.
//
// The order of the fields is SA's, because the save code writes the block
// field by field in this order and the struct is what a save describes. Fields
// SA declares but this fork has no use for yet are still present where SA has
// them, so the layout stays comparable to the reference one field at a time.
//
// Ownership: in SA the data lives in CPlayerInfo (one per player) and the ped
// holds a pointer to it. This fork keeps it in CPlayerInfo the same way; the
// ped pointer is reached through CPlayerPed::GetPlayerData(), which is what
// every call site in the game already uses.
class CPlayerPedData
{
public:
	// SA puts the flag word into a union with four bytes, so that a script or a
	// save can address the flags as one word or as individual bytes. The bit
	// numbering is SA's, bit 0 first.
	union {
		struct {
			uint32 m_bStoppedMoving : 1;
			uint32 m_bAdrenaline : 1;
			uint32 m_bHaveTargetSelected : 1;
			uint32 m_bFreeAiming : 1;
			uint32 m_bCanBeDamaged : 1;
			uint32 m_bAllMeleeAttackPtsBlocked : 1;
			uint32 m_bJustBeenSnacking : 1;
			uint32 m_bRequireHandleBreath : 1;

			uint32 m_bGroupStuffDisabled : 1;
			uint32 m_bGroupAlwaysFollow : 1;
			uint32 m_bGroupNeverFollow : 1;
			uint32 m_bInVehicleDontAllowWeaponChange : 1;
			uint32 m_bRenderWeapon : 1;
		};
		struct {
			uint8 m_bPlayerFlagsByte1;
			uint8 m_bPlayerFlagsByte2;
			uint8 m_bPlayerFlagsByte3;
			uint8 m_bPlayerFlagsByte4;
		};
		uint32 m_nPlayerFlags;
	};

	CWanted *m_pWanted;
	CCopPed *m_pArrestingCop;
	// Transient on-foot input: fight and duck controls are mutually exclusive.
	CVector2D m_vecFightMovement;
	float m_fMoveBlendRatio;
	// SA's sprint budget. It starts at GetFatAndMuscleModifier(STAT_MOD_TIME_CAN_RUN)
	// and drains to -150, which is the point where the player can no longer
	// start a sprint. This replaces the Vice City m_fCurrentStamina/m_fMaxStamina
	// pair: those were an authored 0..1000 meter that no stat fed into.
	float m_fTimeCanRun;
	float m_fMoveSpeed;
	uint8 m_nChosenWeapon;
	uint8 m_nCarDangerCounter;
	uint32 m_nStandStillTimer;
	uint32 m_nHitAnimDelayTimer;
	float m_fAttackButtonCounter;
	CEntity *m_pDangerCar;
	uint32 m_nPlayerGroup;
	uint32 m_nAdrenalineEndTime;
	uint8 m_nDrunkenness;
	uint8 m_nFadeDrunkenness;
	uint8 m_nDrugLevel;
	uint8 m_nScriptLimitToGangSize;
	// The breath meter, in the same units as the lung capacity it was filled
	// from: GetFatAndMuscleModifier(STAT_MOD_AIR_IN_LUNG), which is 1150 for a
	// body with no lung stat and full stamina, and 4000 at the cap.
	float m_fBreath;
	uint32 m_nMeleeWeaponAnimReferenced;
	uint32 m_nMeleeWeaponAnimReferencedExtra;
	float m_fFPSMoveHeading;
	float m_fLookPitch;
	float m_fSkateBoardSpeed;
	float m_fSkateBoardLean;
	RpAtomic *m_pSpecialAtomic;
	float m_fGunSpinSpeed;
	float m_fGunSpinAngle;
	uint32 m_nLastTimeFiring;
	uint32 m_nTargetBone;
	CVector m_vecTargetBoneOffset;
	uint32 m_nBusFaresCollected;
	bool m_bPlayerSprintDisabled;
	bool m_bDontAllowWeaponChange;
	bool m_bForceInteriorLighting;
	uint16 m_nPadDownPressedInMilliseconds;
	uint16 m_nPadUpPressedInMilliseconds;
	uint8 m_nWetness;
	bool m_bPlayersGangActive;
	uint8 m_nWaterCoverPerc;
	float m_fWaterHeight;
	uint32 m_nFireHSMissilePressedTime;
	CEntity *m_LastHSMissileTarget;
	uint32 m_nModelIndexOfLastBuildingShot;
	uint32 m_nLastHSMissileLOSTime : 31;
	uint32 m_bLastHSMissileLOS : 1;
	CPed *m_pCurrentProstitutePed;

	// SA CPlayerPedData::CPlayerPedData: every field at its idle value. The
	// breath meter is filled by CPlayerPed::ResetPlayerBreath, not here, because
	// it depends on stats that are not loaded at construction time.
	CPlayerPedData()
	{
		m_nPlayerFlags = 0;
		m_pWanted = nil;
		m_pArrestingCop = nil;
		m_vecFightMovement = CVector2D(0.0f, 0.0f);
		m_fMoveBlendRatio = 0.0f;
		m_fTimeCanRun = 0.0f;
		m_fMoveSpeed = 0.0f;
		m_nChosenWeapon = 0;
		m_nCarDangerCounter = 0;
		m_nStandStillTimer = 0;
		m_nHitAnimDelayTimer = 0;
		m_fAttackButtonCounter = 0.0f;
		m_pDangerCar = nil;
		m_nPlayerGroup = 0;
		m_nAdrenalineEndTime = 0;
		m_nDrunkenness = 0;
		m_nFadeDrunkenness = 0;
		m_nDrugLevel = 0;
		m_nScriptLimitToGangSize = 0;
		m_fBreath = 0.0f;
		m_nMeleeWeaponAnimReferenced = 0;
		m_nMeleeWeaponAnimReferencedExtra = 0;
		m_fFPSMoveHeading = 0.0f;
		m_fLookPitch = 0.0f;
		m_fSkateBoardSpeed = 0.0f;
		m_fSkateBoardLean = 0.0f;
		m_pSpecialAtomic = nil;
		m_fGunSpinSpeed = 0.0f;
		m_fGunSpinAngle = 0.0f;
		m_nLastTimeFiring = 0;
		m_nTargetBone = 0;
		m_vecTargetBoneOffset = CVector(0.0f, 0.0f, 0.0f);
		m_nBusFaresCollected = 0;
		m_bPlayerSprintDisabled = false;
		m_bDontAllowWeaponChange = false;
		m_bForceInteriorLighting = false;
		m_nPadDownPressedInMilliseconds = 0;
		m_nPadUpPressedInMilliseconds = 0;
		m_nWetness = 0;
		m_bPlayersGangActive = false;
		m_nWaterCoverPerc = 0;
		m_fWaterHeight = 0.0f;
		m_nFireHSMissilePressedTime = 0;
		m_LastHSMissileTarget = nil;
		m_nModelIndexOfLastBuildingShot = 0;
		m_nLastHSMissileLOSTime = 0;
		m_bLastHSMissileLOS = 0;
		m_pCurrentProstitutePed = nil;
	}

	// Writes and reads every field a save has to carry. The wanted level, the
	// clothes and the arresting cop are not here: they are serialized by the
	// blocks that own them (CWanted, CPedClothesDesc, the cop pool).
	// Size of the block, in bytes. Fixed: the save is a byte stream, so a field
	// added here has to be taken out of the CPlayerPed::Save padding budget
	// (164 bytes were reserved for exactly this) and the number below moves
	// with it. The writer asserts the two agree.
	static constexpr uint32 SAVE_SIZE = 137;
	void Save(uint8 *buf) const;
	void Load(const uint8 *buf);
};

#endif // __GTA_PLAYERPEDDATA_H__
