#include "common.h"

#include "PlayerPedData.h"

// CPlayerPedData's save block.
//
// Every field is written in declaration order through one macro pair, so the
// reader and the writer cannot drift apart: the static_assert at the end turns
// a size mismatch into a compile error rather than into a save that loads
// garbage.
//
// The two pointers (m_pDangerCar, m_LastHSMissileTarget) are not written: an
// entity address means nothing across a load, and both are re-acquired by the
// code that uses them. They keep their place in the block as four zero bytes so
// that the layout stays fixed.
//
// The bitfield pair at the end (m_nLastHSMissileLOSTime / m_bLastHSMissileLOS)
// cannot be addressed, so it is read and written through a temporary word.
void
CPlayerPedData::Save(uint8 *buf) const
{
	uint8 *start = buf;

#define Write(data) do { memcpy(buf, &(data), sizeof(data)); buf += sizeof(data); } while(0)
#define WriteZero(n) do { memset(buf, 0, (n)); buf += (n); } while(0)

	uint32 flags = m_nPlayerFlags;
	uint32 hsTime = m_nLastHSMissileLOSTime;
	uint32 hsLos = m_bLastHSMissileLOS;

	Write(flags);
	Write(m_fMoveBlendRatio);
	Write(m_fTimeCanRun);
	Write(m_fMoveSpeed);
	Write(m_nChosenWeapon);
	Write(m_nCarDangerCounter);
	Write(m_nStandStillTimer);
	Write(m_nHitAnimDelayTimer);
	Write(m_fAttackButtonCounter);
	Write(m_nPlayerGroup);
	Write(m_nAdrenalineEndTime);
	Write(m_nDrunkenness);
	Write(m_nFadeDrunkenness);
	Write(m_nDrugLevel);
	Write(m_nScriptLimitToGangSize);
	Write(m_fBreath);
	Write(m_nMeleeWeaponAnimReferenced);
	Write(m_fFPSMoveHeading);
	Write(m_fLookPitch);
	Write(m_fSkateBoardSpeed);
	Write(m_fSkateBoardLean);
	Write(m_fGunSpinSpeed);
	Write(m_fGunSpinAngle);
	Write(m_nLastTimeFiring);
	Write(m_nTargetBone);
	Write(m_vecTargetBoneOffset.x);
	Write(m_vecTargetBoneOffset.y);
	Write(m_vecTargetBoneOffset.z);
	Write(m_nBusFaresCollected);
	Write(m_bPlayerSprintDisabled);
	Write(m_bDontAllowWeaponChange);
	Write(m_bForceInteriorLighting);
	Write(m_nPadDownPressedInMilliseconds);
	Write(m_nPadUpPressedInMilliseconds);
	Write(m_nWetness);
	Write(m_bPlayersGangActive);
	Write(m_nWaterCoverPerc);
	Write(m_fWaterHeight);
	Write(m_nFireHSMissilePressedTime);
	WriteZero(sizeof(uint32)); // m_LastHSMissileTarget
	Write(m_nModelIndexOfLastBuildingShot);
	Write(hsTime);
	Write(hsLos);
	WriteZero(sizeof(uint32)); // m_pCurrentProstitutePed

	assert(buf - start == SAVE_SIZE);

#undef Write
#undef WriteZero
}

void
CPlayerPedData::Load(const uint8 *buf)
{
	const uint8 *start = buf;

#define Read(data) do { memcpy(&(data), buf, sizeof(data)); buf += sizeof(data); } while(0)
#define Skip(n) do { buf += (n); } while(0)

	uint32 flags = 0;
	uint32 hsTime = 0;
	uint32 hsLos = 0;

	Read(flags);
	Read(m_fMoveBlendRatio);
	Read(m_fTimeCanRun);
	Read(m_fMoveSpeed);
	Read(m_nChosenWeapon);
	Read(m_nCarDangerCounter);
	Read(m_nStandStillTimer);
	Read(m_nHitAnimDelayTimer);
	Read(m_fAttackButtonCounter);
	Read(m_nPlayerGroup);
	Read(m_nAdrenalineEndTime);
	Read(m_nDrunkenness);
	Read(m_nFadeDrunkenness);
	Read(m_nDrugLevel);
	Read(m_nScriptLimitToGangSize);
	Read(m_fBreath);
	Read(m_nMeleeWeaponAnimReferenced);
	Read(m_fFPSMoveHeading);
	Read(m_fLookPitch);
	Read(m_fSkateBoardSpeed);
	Read(m_fSkateBoardLean);
	Read(m_fGunSpinSpeed);
	Read(m_fGunSpinAngle);
	Read(m_nLastTimeFiring);
	Read(m_nTargetBone);
	Read(m_vecTargetBoneOffset.x);
	Read(m_vecTargetBoneOffset.y);
	Read(m_vecTargetBoneOffset.z);
	Read(m_nBusFaresCollected);
	Read(m_bPlayerSprintDisabled);
	Read(m_bDontAllowWeaponChange);
	Read(m_bForceInteriorLighting);
	Read(m_nPadDownPressedInMilliseconds);
	Read(m_nPadUpPressedInMilliseconds);
	Read(m_nWetness);
	Read(m_bPlayersGangActive);
	Read(m_nWaterCoverPerc);
	Read(m_fWaterHeight);
	Read(m_nFireHSMissilePressedTime);
	Skip(sizeof(uint32)); // m_LastHSMissileTarget, re-acquired by the seeker code
	Read(m_nModelIndexOfLastBuildingShot);
	Read(hsTime);
	Read(hsLos);
	Skip(sizeof(uint32)); // m_pCurrentProstitutePed

	m_nPlayerFlags = flags;
	m_nLastHSMissileLOSTime = hsTime;
	m_bLastHSMissileLOS = hsLos;
	m_pDangerCar = nil;
	m_LastHSMissileTarget = nil;
	m_pCurrentProstitutePed = nil;
	m_pWanted = nil;
	m_pArrestingCop = nil;
	m_pSpecialAtomic = nil;

	assert(buf - start == SAVE_SIZE);

#undef Read
#undef Skip
}
