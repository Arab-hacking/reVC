#include "common.h"

#include "main.h"
#include "FileMgr.h"
#include "WeaponInfo.h"
#include "AnimManager.h"
#include "AnimBlendAssociation.h"
#include "Weapon.h"
#include "ModelInfo.h"
#include "ModelIndices.h"

uint16 CWeaponInfo::ms_aReloadSampleTime[WEAPONTYPE_TOTALWEAPONS] =
{
	0,			// UNARMED
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,			// GRENADE
	0,			// DETONATEGRENADE
	0,			// TEARGAS
	0,			// MOLOTOV
	0,			// ROCKET
	250,		// COLT45
	250,		// PYTHON
	150,		// SHOTGUN
	650,		// SPAS12 SHOTGUN
	650,		// STUBBY SHOTGUN
	400,		// TEC9
	400,		// UZIhec
	400,		// SILENCED_INGRAM
	400,		// MP5
	300,		// M16
	300,		// AK47
	423,		// SNIPERRIFLE
	423,		// LASERSCOPE
	400,		// ROCKETLAUNCHER
	0,			// FLAMETHROWER
	0,			// M60
	0,			// MINIGUN
	0,			// DETONATOR
	0,			// HELICANNON
	0			// CAMERA
};

// Yeah...
int32 CWeaponInfo::ms_aMaxAmmoForWeapon[WEAPONTYPE_TOTALWEAPONS] =
{
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1
};

CWeaponInfo aWeaponInfo[WEAPONTYPE_TOTALWEAPONS];
// SA CWeaponInfo::ms_WeaponAimOffsets (0xC8A8A8). Zero-initialised here and
// filled from the '%' lines of weapon.dat by LoadWeaponData, exactly as SA does.
CWeaponInfo::tAnimAimOffsets CWeaponInfo::ms_WeaponAimOffsets[ASSOCGRP_SPRAYCAN + 1 - ASSOCGRP_PYTHON];

char CWeaponInfo::ms_aWeaponNames[WEAPONTYPE_TOTALWEAPONS][32] =
{
	"Unarmed",
	"BrassKnuckle",
	"ScrewDriver",
	"GolfClub",
	"NightStick",
	"Knife",
	"BaseballBat",
	"Hammer",
	"Cleaver",
	"Machete",
	"Katana",
	"Chainsaw",
	"Grenade",
	"DetonateGrenade",
	"TearGas",
	"Molotov",
	"Rocket",
	"Colt45",
	"Python",
	"Shotgun",
	"Spas12Shotgun",
	"StubbyShotgun",
	"Tec9",
	"Uzi",
	"SilencedIngram",
	"Mp5",
	"m4",
	"Ruger",
	"SniperRifle",
	"LaserScope",
	"RocketLauncher",
	"FlameThrower",
	"M60",
	"Minigun",
	"Detonator",
	"HeliCannon",
	"Camera",
};

CWeaponInfo*
CWeaponInfo::GetWeaponInfo(eWeaponType weaponType)
{
	return &aWeaponInfo[weaponType];
}

void
CWeaponInfo::Initialise(void)
{
	debug("Initialising CWeaponInfo...\n");
	for (int i = 0; i < WEAPONTYPE_TOTALWEAPONS; i++) {
		aWeaponInfo[i].m_eWeaponFire = WEAPON_FIRE_INSTANT_HIT;
		aWeaponInfo[i].m_fRange = 0.0f;
		aWeaponInfo[i].m_nFiringRate = 0;
		aWeaponInfo[i].m_nReload = 0;
		aWeaponInfo[i].m_nAmountofAmmunition = 0;
		aWeaponInfo[i].m_nDamage = 0;
		aWeaponInfo[i].m_fSpeed = 0.0f;
		aWeaponInfo[i].m_fRadius = 0.0f;
		aWeaponInfo[i].m_fLifespan = 0.0f;
		aWeaponInfo[i].m_fSpread = 0.0f;
		aWeaponInfo[i].m_vecFireOffset = CVector(0.0f, 0.0f, 0.0f);
		aWeaponInfo[i].m_AnimToPlay = ASSOCGRP_MELEE_1;
		aWeaponInfo[i].m_fAnimLoopStart = 0.0f;
		aWeaponInfo[i].m_fAnimLoopEnd = 0.0f;
		aWeaponInfo[i].m_fAnimFrameFire = 0.0f;
		aWeaponInfo[i].m_fAnim2LoopStart = 0.0f;
		aWeaponInfo[i].m_fAnim2LoopEnd = 0.0f;
		aWeaponInfo[i].m_fAnim2FrameFire = 0.0f;
		aWeaponInfo[i].m_fAnimBreakout = 0.0f;
		aWeaponInfo[i].m_Flags = WEAPONFLAG_USE_GRAVITY | WEAPONFLAG_SLOWS_DOWN | WEAPONFLAG_RAND_SPEED | WEAPONFLAG_EXPANDS | WEAPONFLAG_EXPLODES;
		aWeaponInfo[i].m_nWeaponSlot = WEAPONSLOT_UNARMED;
		aWeaponInfo[i].m_nAimOffsetIndex = 0;
	}

	// SA zeroes the whole aim-offset table in CWeaponInfo::Initialise before
	// weapon.dat is read, so a weapon that never gets a '%' row aims from the
	// ped's origin rather than from garbage.
	for (int i = 0; i < ARRAY_SIZE(ms_WeaponAimOffsets); i++) {
		ms_WeaponAimOffsets[i].AimX = 0.0f;
		ms_WeaponAimOffsets[i].AimZ = 0.0f;
		ms_WeaponAimOffsets[i].DuckX = 0.0f;
		ms_WeaponAimOffsets[i].DuckZ = 0.0f;
		ms_WeaponAimOffsets[i].RLoadA = 0;
		ms_WeaponAimOffsets[i].RLoadB = 0;
		ms_WeaponAimOffsets[i].CrouchRLoadA = 0;
		ms_WeaponAimOffsets[i].CrouchRLoadB = 0;
	}

	debug("Loading weapon data...\n");
	LoadWeaponData();
	debug("CWeaponInfo ready\n");
}

void
CWeaponInfo::LoadWeaponData(void)
{
	float spread, speed, lifeSpan, radius;
	float range, fireOffsetX, fireOffsetY, fireOffsetZ;
	float anim2LoopStart, anim2LoopEnd, delayBetweenAnim2AndFire, animBreakout;
	float delayBetweenAnimAndFire, animLoopStart, animLoopEnd;
	int flags, ammoAmount, damage, reload, weaponType;
	int firingRate, modelId, modelId2, weaponSlot;
	char line[256], weaponName[32], fireType[32];
	char animToPlay[32];

	size_t bp, buflen;
	int lp, linelen;
	int missingWeaponModels = 0;
		
	CFileMgr::SetDir("DATA");
	buflen = CFileMgr::LoadFile("WEAPON.DAT", work_buff, sizeof(work_buff), "r");

	for (bp = 0; bp < buflen; ) {
		// read file line by line
		for (linelen = 0; work_buff[bp] != '\n' && bp < buflen; bp++) {
			line[linelen++] = work_buff[bp];
		}
		bp++;
		line[linelen] = '\0';

		// skip white space
		for (lp = 0; line[lp] <= ' ' && line[lp] != '\0'; lp++);

		if (line[lp] == '\0' || line[lp] == '#')
			continue;

		spread = 0.0f;
		flags = 0;
		speed = 0.0f;
		ammoAmount = 0;
		lifeSpan = 0.0f;
		radius = 0.0f;
		range = 0.0f;
		damage = 0;
		reload = 0;
		firingRate = 0;
		fireOffsetX = 0.0f;
		weaponName[0] = '\0';
		fireType[0] = '\0';
		fireOffsetY = 0.0f;
		fireOffsetZ = 0.0f;
		sscanf(
			&line[lp],
			"%s %s %f %d %d %d %d %f %f %f %f %f %f %f %s %f %f %f %f %f %f %f %d %d %x %d",
			weaponName,
			fireType,
			&range,
			&firingRate,
			&reload,
			&ammoAmount,
			&damage,
			&speed,
			&radius,
			&lifeSpan,
			&spread,
			&fireOffsetX,
			&fireOffsetY,
			&fireOffsetZ,
			animToPlay,
			&animLoopStart,
			&animLoopEnd,
			&delayBetweenAnimAndFire,
			&anim2LoopStart,
			&anim2LoopEnd,
			&delayBetweenAnim2AndFire,
			&animBreakout,
			&modelId,
			&modelId2,
			&flags,
			&weaponSlot);

		if (strncmp(weaponName, "ENDWEAPONDATA", 13) == 0) {
			if (missingWeaponModels)
				REVC_LOG_INFO("LoadWeaponData: %d weapon model id(s) have no model info", missingWeaponModels);
			return;
		}

		weaponType = FindWeaponType(weaponName);

		CVector vecFireOffset(fireOffsetX, fireOffsetY, fireOffsetZ);

		aWeaponInfo[weaponType].m_eWeaponFire = FindWeaponFireType(fireType);
		aWeaponInfo[weaponType].m_fRange = range;
		aWeaponInfo[weaponType].m_nFiringRate = firingRate;
		aWeaponInfo[weaponType].m_nReload = reload;
		aWeaponInfo[weaponType].m_nAmountofAmmunition = ammoAmount;
		aWeaponInfo[weaponType].m_nDamage = damage;
		aWeaponInfo[weaponType].m_fSpeed = speed;
		aWeaponInfo[weaponType].m_fRadius = radius;
		aWeaponInfo[weaponType].m_fLifespan = lifeSpan;
		aWeaponInfo[weaponType].m_fSpread = spread;
		aWeaponInfo[weaponType].m_vecFireOffset = vecFireOffset;
		aWeaponInfo[weaponType].m_fAnimLoopStart = animLoopStart / 30.0f;
		aWeaponInfo[weaponType].m_fAnimLoopEnd = animLoopEnd / 30.0f;
		aWeaponInfo[weaponType].m_fAnim2LoopStart = anim2LoopStart / 30.0f;
		aWeaponInfo[weaponType].m_fAnim2LoopEnd = anim2LoopEnd / 30.0f;
		aWeaponInfo[weaponType].m_fAnimFrameFire = delayBetweenAnimAndFire / 30.0f;
		aWeaponInfo[weaponType].m_fAnim2FrameFire = delayBetweenAnim2AndFire / 30.0f;
		aWeaponInfo[weaponType].m_fAnimBreakout = animBreakout / 30.0f;
		aWeaponInfo[weaponType].m_nModelId = modelId;
		aWeaponInfo[weaponType].m_nModel2Id = modelId2;
		aWeaponInfo[weaponType].m_Flags = flags;
		aWeaponInfo[weaponType].m_nWeaponSlot = weaponSlot;

		if (animLoopEnd < 98.0f && weaponType != WEAPONTYPE_FLAMETHROWER && !CWeapon::IsShotgun(weaponType))
			aWeaponInfo[weaponType].m_nFiringRate = ((aWeaponInfo[weaponType].m_fAnimLoopEnd - aWeaponInfo[weaponType].m_fAnimLoopStart) * 900.0f);

		if (weaponType == WEAPONTYPE_DETONATOR || weaponType == WEAPONTYPE_HELICANNON)
			modelId = -1;
		else if (weaponType == WEAPONTYPE_DETONATOR_GRENADE)
			modelId = MI_BOMB;

		// WEAPON.DAT is the VC table: its model ids are the native WEAPONS.IDE
		// ids, and that IDE is not read any more. GetModelInfo then returns nil
		// and SetWeaponInfo writes m_atomics[2] at offset 0x40 through it, which
		// is the observed 0xC0000005 write to 0x40. Only stamp the ids that
		// actually resolved to a weapon model.
		if (modelId != -1) {
			CBaseModelInfo *wmi = CModelInfo::GetModelInfo(modelId);
			if (wmi && wmi->GetModelType() == MITYPE_WEAPON)
				((CWeaponModelInfo*)wmi)->SetWeaponInfo(weaponType);
			else
				missingWeaponModels++;
		}

		for (int i = 0; i < NUM_ANIM_ASSOC_GROUPS; i++) {
			const char *groupName = CAnimManager::GetAnimGroupName((AssocGroupId)i);
			if (groupName && !strcmp(animToPlay, groupName)) {
				aWeaponInfo[weaponType].m_AnimToPlay = (AssocGroupId)i;
				break;
			}
		}
	}
}

eWeaponType
CWeaponInfo::FindWeaponType(char *name)
{
	for (int i = 0; i < WEAPONTYPE_TOTALWEAPONS; i++) {
		if (strcmp(ms_aWeaponNames[i], name) == 0) {
			return static_cast<eWeaponType>(i);
		}
	}
	return WEAPONTYPE_UNARMED;
}

eWeaponFire
CWeaponInfo::FindWeaponFireType(char *name)
{
	if (strcmp(name, "MELEE") == 0) return WEAPON_FIRE_MELEE;
	if (strcmp(name, "INSTANT_HIT") == 0) return WEAPON_FIRE_INSTANT_HIT;
	if (strcmp(name, "PROJECTILE") == 0) return WEAPON_FIRE_PROJECTILE;
	if (strcmp(name, "AREA_EFFECT") == 0) return WEAPON_FIRE_AREA_EFFECT;
	if (strcmp(name, "CAMERA") == 0) return WEAPON_FIRE_CAMERA;
	Error("Unknown weapon fire type, WeaponInfo.cpp");
	return WEAPON_FIRE_INSTANT_HIT;
}

void
CWeaponInfo::Shutdown(void)
{
	debug("Shutting down CWeaponInfo...\n");
	debug("CWeaponInfo shut down\n");
}
