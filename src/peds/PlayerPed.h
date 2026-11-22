#ifndef __GTA_PLAYERPED_H__
#define __GTA_PLAYERPED_H__

#include "Ped.h"
#include "PlayerPedData.h"

class CPad;
class CCopPed;
class CWanted;

class CPlayerPed : public CPed
{
public:
	// SA keeps the player-wide block in CPlayerInfo and the ped only points at
	// it, so that a ped respawn or a replay does not carry a second copy. This
	// fork used to flatten the fields into the ped; the accessors below keep
	// every existing Wanted() / Breath() spelling working and resolve them to
	// the one owner.
	CPlayerPedData *m_pPlayerData;

	int8 m_nSelectedWepSlot;
	bool m_bSpeedTimerFlag;
	uint8 m_nEvadeAmount;
	uint32 m_nSpeedTimer; // m_nStandStillTimer?
	CEntity *m_pEvadingFrom;	// is this CPhysical?
	int32 m_nTargettableObjects[4];
	uint8 m_nDrunkCountdown; //countdown in frames when the drunk effect ends
	bool m_bHasLockOnTarget;
	bool m_bNoPosForMeleeAttack;
	bool unk1;
	CVector m_vecSafePos[6]; // safe places from the player, for example behind a tree
	CPed *m_pPedAtSafePos[6];
	CPed *m_pMeleeList[6]; // reachable peds at each direction(6)
	int16 m_nAttackDirToCheck;
	float m_fWalkAngle; //angle between heading and walking direction
	RpAtomic* m_pMinigunTopAtomic; //atomic for the spinning part of the minigun model
	unsigned int m_nLastBusFareCollected;

	// The player's own block. Bound in the constructor; every accessor below
	// resolves through it.
	void BindPlayerData(CPlayerPedData *data);
	void UnbindPlayerData() { m_pPlayerData = nil; }

	// SA CPlayerPed::GetPlayerData.
	CPlayerPedData *GetPlayerData() const { return m_pPlayerData; }

	// The fields that used to live on the ped and now live in CPlayerPedData.
	// Each one is a member function that returns the field itself, so the old
	// `player->m_fMoveSpeed` spelling becomes `player->MoveSpeed()` without
	// changing any of the arithmetic around them.
	CWanted *&Wanted() const { return m_pPlayerData->m_pWanted; }
	CCopPed *&ArrestingCop() const { return m_pPlayerData->m_pArrestingCop; }
	float &MoveBlendRatio() const { return m_pPlayerData->m_fMoveBlendRatio; }
	float &TimeCanRun() const { return m_pPlayerData->m_fTimeCanRun; }
	float &MoveSpeed() const { return m_pPlayerData->m_fMoveSpeed; }
	float &Breath() const { return m_pPlayerData->m_fBreath; }
	uint32 &PlayerFlags() const { return m_pPlayerData->m_nPlayerFlags; }
	uint32 &AdrenalineEndTime() const { return m_pPlayerData->m_nAdrenalineEndTime; }
	uint8 &Drunkenness() const { return m_pPlayerData->m_nDrunkenness; }
	uint8 &FadeDrunkenness() const { return m_pPlayerData->m_nFadeDrunkenness; }
	uint32 &HitAnimDelayTimer() const { return m_pPlayerData->m_nHitAnimDelayTimer; }
	float &AttackButtonCounter() const { return m_pPlayerData->m_fAttackButtonCounter; }
	float &FPSMoveHeading() const { return m_pPlayerData->m_fFPSMoveHeading; }
	float &GunSpinSpeed() const { return m_pPlayerData->m_fGunSpinSpeed; }
	float &GunSpinAngle() const { return m_pPlayerData->m_fGunSpinAngle; }
	uint16 &PadDownPressedInMilliseconds() const { return m_pPlayerData->m_nPadDownPressedInMilliseconds; }

	// The three flags below live in CPlayerPedData's bitfield, which is where SA
	// keeps them: one bit each in PlayerFlags(). A reference cannot name a
	// bitfield member, so the old spellings go through a one-bit proxy that
	// reads and writes the bit in place. That keeps `player->CanBeDamaged()` a
	// valid lvalue exactly as `player->m_bCanBeDamaged` used to be.
	struct FlagsProxy {
		uint32 *word;
		uint32 mask;
		FlagsProxy(uint32 *w, uint32 m) : word(w), mask(m) { }
		operator bool() const { return (*word & mask) != 0; }
		bool operator!() const { return (*word & mask) == 0; }
		FlagsProxy &operator=(bool value)
		{
			if(value)
				*word |= mask;
			else
				*word &= ~mask;
			return *this;
		}
	};
	FlagsProxy AdrenalineActive() const { return FlagsProxy(&m_pPlayerData->m_nPlayerFlags, 0x2); }
	FlagsProxy HaveTargetSelected() const { return FlagsProxy(&m_pPlayerData->m_nPlayerFlags, 0x4); }
	FlagsProxy CanBeDamaged() const { return FlagsProxy(&m_pPlayerData->m_nPlayerFlags, 0x10); }
#ifdef FREE_CAM
	bool m_bFreeAimActive;
	CVector m_cachedCamSource;
	CVector m_cachedCamFront;
	CVector m_cachedCamUp;
#endif

	static bool bDontAllowWeaponChange;
#ifndef MASTER
	static bool bDebugPlayerInfo;
#endif

	CPlayerPed();
	~CPlayerPed();
	void SetMoveAnim() { };

	void ReApplyMoveAnims(void);
	void ClearWeaponTarget(void);
	void SetWantedLevel(int32 level);
	void SetWantedLevelNoDrop(int32 level);
	void KeepAreaAroundPlayerClear(void);
	void AnnoyPlayerPed(bool);
	void MakeChangesForNewWeapon(int32);
	void MakeChangesForNewWeapon(eWeaponType);
	void SetInitialState(void);
	void ProcessControl(void);
	void ClearAdrenaline(void);
	// SA CPlayerPed::ResetSprintEnergy / HandleSprintEnergy (0x60A530/0x60A550).
	// The budget is m_fTimeCanRun; the old name is kept as a thin caller.
	void ResetSprintEnergy(void);
	bool HandleSprintEnergy(bool sprint, float adrenalineConsumedPerTimeStep);
	void UseSprintEnergy(void);
	class CPlayerInfo *GetPlayerInfoForThisPlayerPed();
	void SetRealMoveAnim(void);
	void RestoreSprintEnergy(float);
	float DoWeaponSmoothSpray(void);
	void DoStuffToGoOnFire(void);
	bool DoesTargetHaveToBeBroken(CVector, CWeapon*);
	void RunningLand(CPad*);
	bool IsThisPedAnAimingPriority(CPed*);
	void PlayerControlSniper(CPad*);
	void PlayerControlM16(CPad*);
	void PlayerControlFighter(CPad*);
	void ProcessWeaponSwitch(CPad*);
	void MakeObjectTargettable(int32);
	void PlayerControl1stPersonRunAround(CPad *padUsed);
	void EvaluateNeighbouringTarget(CEntity*, CEntity**, float*, float, float, bool, bool);
	void EvaluateTarget(CEntity*, CEntity**, float*, float, float, bool);
	bool FindNextWeaponLockOnTarget(CEntity*, bool);
	bool FindWeaponLockOnTarget(void);
	void ProcessAnimGroups(void);
	void ProcessPlayerWeapon(CPad*);
	void PlayerControlZelda(CPad*);
	bool DoesPlayerWantNewWeapon(eWeaponType, bool);
	void PlayIdleAnimations(CPad*);
	void RemovePedFromMeleeList(CPed*);
	void GetMeleeAttackCoords(CVector&, int8, float);
	int32 FindMeleeAttackPoint(CPed*, CVector&, uint32&);
	bool CanIKReachThisTarget(CVector, CWeapon*, bool);
	void RotatePlayerToTrackTarget(void);
	bool MovementDisabledBecauseOfTargeting(void);
	void FindNewAttackPoints(void);
	void SetNearbyPedsToInteractWithPlayer(void);
	void UpdateMeleeAttackers(void);

	static void SetupPlayerPed(int32);
	static void DeactivatePlayerPed(int32);
	static void ReactivatePlayerPed(int32);

#ifdef COMPATIBLE_SAVES
	virtual void Save(uint8*& buf);
	virtual void Load(uint8*& buf);
#endif

	static const uint32 nSaveStructSize;
};

//VALIDATE_SIZE(CPlayerPed, 0x5F0);

#endif // __GTA_PLAYERPED_H__
