#include "common.h"

#include "General.h"
#include "RwHelper.h"
#include "ModelInfo.h"
#include "ModelIndices.h"
#include "FileMgr.h"
#include "RpAnimBlend.h"
#include "AnimBlendClumpData.h"
#include "AnimBlendAssociation.h"
#include "AnimBlendAssocGroup.h"
#include "AnimManager.h"
#include "AnimFileHeader.h"
#include "Streaming.h"

#ifdef CUSTOM_MODELS
#include "CustomModels.h"
#include "SavehDiag.h"
#include "brformats.h"
#endif

CAnimBlock CAnimManager::ms_aAnimBlocks[NUMANIMBLOCKS];
CAnimBlendHierarchy CAnimManager::ms_aAnimations[NUMANIMATIONS];
int32 CAnimManager::ms_numAnimBlocks;
int32 CAnimManager::ms_numAnimations;
CAnimBlendAssocGroup *CAnimManager::ms_aAnimAssocGroups;
CLinkList<CAnimBlendHierarchy*> CAnimManager::ms_animCache;

// numAnims, clip names, descriptor count, descriptors
#define awc(a) ARRAY_SIZE(a), a
AnimAssocDefinition CAnimManager::ms_aAnimAssocDefinitions[NUM_ANIM_ASSOC_GROUPS] = {
	{ "default", "ped", MI_MALE01, ARRAY_SIZE(aStdAnimations), 191, aStdAnimations, aStdAnimDescs },
	{ "door", "ped", -1, ARRAY_SIZE(aDoorAnimations), 2, aDoorAnimations, aDoorDescs },
	{ "bikes", "bikes", MI_MALE01, ARRAY_SIZE(aBikesAnimations), 12, aBikesAnimations, aBikesDescs },
	{ "bikev", "bikev", MI_MALE01, ARRAY_SIZE(aBikevAnimations), 12, aBikevAnimations, aBikesDescs },
	{ "bikeh", "bikeh", MI_MALE01, ARRAY_SIZE(aBikehAnimations), 12, aBikehAnimations, aBikesDescs },
	{ "biked", "biked", MI_MALE01, ARRAY_SIZE(aBikedAnimations), 12, aBikedAnimations, aBikesDescs },
	{ "wayfarer", "wayfarer", MI_MALE01, ARRAY_SIZE(aWayfarerAnimations), 12, aWayfarerAnimations, aBikesDescs },
	{ "bmx", "bmx", MI_MALE01, ARRAY_SIZE(aBmxAnimations), 15, aBmxAnimations, aBikesDescs },
	{ "mtb", "mtb", MI_MALE01, ARRAY_SIZE(aMtbAnimations), 15, aMtbAnimations, aBikesDescs },
	{ "choppa", "choppa", MI_MALE01, ARRAY_SIZE(aChoppaAnimations), 15, aChoppaAnimations, aBikesDescs },
	{ "quad", "quad", MI_MALE01, ARRAY_SIZE(aQuadAnimations), 12, aQuadAnimations, aQuadDescs },
	{ "python", "python", MI_MALE01, ARRAY_SIZE(aPythonAnimations), 4, aPythonAnimations, aWeaponDescs },
	{ "pythonbad", "python", MI_MALE01, ARRAY_SIZE(aPythonbadAnimations), 4, aPythonbadAnimations, aWeaponDescs1 },
	{ "colt45", "colt45", MI_MALE01, ARRAY_SIZE(aColt45Animations), 4, aColt45Animations, aWeaponDescs },
	{ "colt_cop", "colt45", MI_MALE01, ARRAY_SIZE(aColt_copAnimations), 4, aColt_copAnimations, aWeaponDescs },
	{ "colt45pro", "colt45", MI_MALE01, ARRAY_SIZE(aColt45proAnimations), 4, aColt45proAnimations, aWeaponDescs },
	{ "sawnoff", "colt45", MI_MALE01, ARRAY_SIZE(aSawnoffAnimations), 4, aSawnoffAnimations, aWeaponDescs },
	{ "sawnoffpro", "colt45", MI_MALE01, ARRAY_SIZE(aSawnoffproAnimations), 4, aSawnoffproAnimations, aWeaponDescs },
	{ "silenced", "silenced", MI_MALE01, ARRAY_SIZE(aSilencedAnimations), 4, aSilencedAnimations, aWeaponDescs },
	{ "shotgun", "shotgun", MI_MALE01, ARRAY_SIZE(aShotgunAnimations), 2, aShotgunAnimations, aWeaponDescs },
	{ "shotgunbad", "shotgun", MI_MALE01, ARRAY_SIZE(aShotgunbadAnimations), 2, aShotgunbadAnimations, aWeaponDescs1 },
	{ "buddy", "buddy", MI_MALE01, ARRAY_SIZE(aBuddyAnimations), 4, aBuddyAnimations, aWeaponDescs },
	{ "buddybad", "buddy", MI_MALE01, ARRAY_SIZE(aBuddybadAnimations), 4, aBuddybadAnimations, aWeaponDescs1 },
	{ "uzi", "uzi", MI_MALE01, ARRAY_SIZE(aUziAnimations), 4, aUziAnimations, aWeaponDescs },
	{ "uzibad", "uzi", MI_MALE01, ARRAY_SIZE(aUzibadAnimations), 4, aUzibadAnimations, aWeaponDescs2 },
	{ "rifle", "rifle", MI_MALE01, ARRAY_SIZE(aRifleAnimations), 4, aRifleAnimations, aWeaponDescs },
	{ "riflebad", "rifle", MI_MALE01, ARRAY_SIZE(aRiflebadAnimations), 4, aRiflebadAnimations, aWeaponDescs2 },
	{ "sniper", "rifle", MI_MALE01, ARRAY_SIZE(aRifleAnimations), 4, aRifleAnimations, aWeaponDescs },
	{ "grenade", "grenade", MI_MALE01, ARRAY_SIZE(aGrenadeAnimations), 3, aGrenadeAnimations, aWeaponDescs3 },
	{ "flame", "flame", MI_MALE01, ARRAY_SIZE(aFlameAnimations), 1, aFlameAnimations, aWeaponDescs },
	{ "rocket", "rocket", MI_MALE01, ARRAY_SIZE(aRocketAnimations), 1, aRocketAnimations, aWeaponDescs },
	{ "spraycan", "spraycan", MI_MALE01, ARRAY_SIZE(aSpraycanAnimations), 1, aSpraycanAnimations, aWeaponDescs },
	{ "goggles", "goggles", MI_MALE01, ARRAY_SIZE(aGogglesAnimations), 1, aGogglesAnimations, aWeaponDescs },
	{ "melee_1", "ped", MI_MALE01, ARRAY_SIZE(aMelee_1Animations), 10, aMelee_1Animations, aWeaponDescs4 },
	{ "melee_2", "fight_b", MI_MALE01, ARRAY_SIZE(aMelee_2Animations), 10, aMelee_2Animations, aWeaponDescs4 },
	{ "melee_3", "fight_c", MI_MALE01, ARRAY_SIZE(aMelee_3Animations), 10, aMelee_3Animations, aWeaponDescs5 },
	{ "melee_4", "fight_d", MI_MALE01, ARRAY_SIZE(aMelee_4Animations), 10, aMelee_4Animations, aWeaponDescs4 },
	{ "bbbat_1", "baseball", MI_MALE01, ARRAY_SIZE(aBbbat_1Animations), 10, aBbbat_1Animations, aWeaponDescs4 },
	{ "gclub_1", "baseball", MI_MALE01, ARRAY_SIZE(aBbbat_1Animations), 10, aBbbat_1Animations, aWeaponDescs4 },
	{ "knife_1", "knife", MI_MALE01, ARRAY_SIZE(aKnife_1Animations), 10, aKnife_1Animations, aWeaponDescs4 },
	{ "sword_1", "sword", MI_MALE01, ARRAY_SIZE(aSword_1Animations), 10, aSword_1Animations, aWeaponDescs4 },
	{ "dildo_1", "dildo", MI_MALE01, ARRAY_SIZE(aDildo_1Animations), 10, aDildo_1Animations, aWeaponDescs4 },
	{ "flowers_1", "flowers", MI_MALE01, ARRAY_SIZE(aFlowers_1Animations), 8, aFlowers_1Animations, aWeaponDescs4 },
	{ "csaw_1", "chainsaw", MI_MALE01, ARRAY_SIZE(aCsaw_1Animations), 10, aCsaw_1Animations, aWeaponDescs4 },
	{ "kick_std", "fight_e", MI_MALE01, ARRAY_SIZE(aKick_stdAnimations), 7, aKick_stdAnimations, aWeaponDescs4 },
	{ "pistlwhp", "ped", MI_MALE01, ARRAY_SIZE(aPistlwhpAnimations), 7, aPistlwhpAnimations, aWeaponDescs4 },
	{ "medic", "medic", MI_MALE01, ARRAY_SIZE(aMedicAnimations), 1, aMedicAnimations, aMedicAnimDescs },
	{ "beach", "beach", MI_MALE01, ARRAY_SIZE(aBeachAnimations), 5, aBeachAnimations, aBeachAnimDescs },
	{ "sunbathe", "sunbathe", MI_MALE01, ARRAY_SIZE(aSunbatheAnimations), 16, aSunbatheAnimations, aSunbatheAnimDescs },
	{ "playidles", "playidles", MI_MALE01, ARRAY_SIZE(aPlayIdlesAnimations), 4, aPlayIdlesAnimations, aPlayerIdleAnimDescs },
	{ "riot", "riot", MI_MALE01, ARRAY_SIZE(aRiotAnimations), 7, aRiotAnimations, aRiotAnimDescs },
	{ "strip", "strip", MI_MALE01, ARRAY_SIZE(aStripAnimations), 7, aStripAnimations, aStripAnimDescs },
	{ "gangs", "gangs", MI_MALE01, ARRAY_SIZE(aGangsAnimations), 29, aGangsAnimations, aGangsAnimDescs },
	{ "attractors", "attractors", MI_MALE01, ARRAY_SIZE(aAttractorsAnimations), 3, aAttractorsAnimations, aAttractorsAnimDescs },
	{ "player", "ped", MI_MALE01, ARRAY_SIZE(aPlayerAnimations), 6, aPlayerAnimations, aStdAnimDescs },
	{ "fat", "fat", MI_MALE01, ARRAY_SIZE(aFatAnimations), 6, aFatAnimations, aStdAnimDescs },
	{ "muscular", "muscular", MI_MALE01, ARRAY_SIZE(aMuscularAnimations), 6, aMuscularAnimations, aStdAnimDescs },
	{ "playerrocket", "ped", MI_MALE01, ARRAY_SIZE(aPlayerRocketAnimations), 6, aPlayerRocketAnimations, aStdAnimDescs },
	{ "playerrocketf", "fat", MI_MALE01, ARRAY_SIZE(aPlayerRocketfAnimations), 6, aPlayerRocketfAnimations, aStdAnimDescs },
	{ "playerrocketm", "muscular", MI_MALE01, ARRAY_SIZE(aPlayerRocketmAnimations), 6, aPlayerRocketmAnimations, aStdAnimDescs },
	{ "player2armed", "ped", MI_MALE01, ARRAY_SIZE(aPlayer2ArmedAnimations), 6, aPlayer2ArmedAnimations, aStdAnimDescs },
	{ "player2armedf", "fat", MI_MALE01, ARRAY_SIZE(aPlayer2ArmedfAnimations), 6, aPlayer2ArmedfAnimations, aStdAnimDescs },
	{ "player2armedm", "muscular", MI_MALE01, ARRAY_SIZE(aPlayer2ArmedmAnimations), 6, aPlayer2ArmedmAnimations, aStdAnimDescs },
	{ "playerBBBat", "ped", MI_MALE01, ARRAY_SIZE(aPlayerBBBatAnimations), 6, aPlayerBBBatAnimations, aStdAnimDescs },
	{ "playerBBBatf", "fat", MI_MALE01, ARRAY_SIZE(aPlayerBBBatfAnimations), 6, aPlayerBBBatfAnimations, aStdAnimDescs },
	{ "playerBBBatm", "muscular", MI_MALE01, ARRAY_SIZE(aPlayerBBBatmAnimations), 6, aPlayerBBBatmAnimations, aStdAnimDescs },
	{ "playercsaw", "ped", MI_MALE01, ARRAY_SIZE(aPlayerCsawAnimations), 6, aPlayerCsawAnimations, aStdAnimDescs },
	{ "playercsawf", "fat", MI_MALE01, ARRAY_SIZE(aPlayerCsawfAnimations), 6, aPlayerCsawfAnimations, aStdAnimDescs },
	{ "playercsawm", "muscular", MI_MALE01, ARRAY_SIZE(aPlayerCsawmAnimations), 6, aPlayerCsawmAnimations, aStdAnimDescs },
	{ "playersneak", "ped", MI_MALE01, ARRAY_SIZE(aPlayerSneakAnimations), 6, aPlayerSneakAnimations, aStdAnimDescs },
	{ "playerjetpack", "ped", MI_MALE01, ARRAY_SIZE(aPlayerJetpackAnimations), 6, aPlayerJetpackAnimations, aStdAnimDescs },
	{ "swim", "swim", MI_MALE01, ARRAY_SIZE(aSwimAnimations), 6, aSwimAnimations, aSwimAnimDescs },
	{ "drivebys", "drivebys", MI_MALE01, ARRAY_SIZE(aDrivebysAnimations), 8, aDrivebysAnimations, aDbzAnimDescs },
	{ "bike_dbz", "bike_dbz", MI_MALE01, ARRAY_SIZE(aBike_dbzAnimations), 4, aBike_dbzAnimations, aDbzAnimDescs },
	{ "cop_dbz", "cop_dvbyz", MI_MALE01, ARRAY_SIZE(aCop_dbzAnimations), 4, aCop_dbzAnimations, aDbzAnimDescs },
	{ "quad_dbz", "quad_dbz", MI_MALE01, ARRAY_SIZE(aQuad_dbzAnimations), 4, aQuad_dbzAnimations, aDbzAnimDescs },
	{ "fat_tired", "fat", MI_MALE01, ARRAY_SIZE(aFatTiredAnimations), 1, aFatTiredAnimations, aFatTiredAnimDescs },
	{ "handsignal", "ghands", MI_MALE01, ARRAY_SIZE(aHandsignalAnimations), 5, aHandsignalAnimations, aHandSignalAnimDescs },
	{ "handsignalL", "ghands", MI_MALE01, ARRAY_SIZE(aHandsignalLAnimations), 5, aHandsignalLAnimations, aHandSignalLAnimDescs },
	{ "lhand", "ghands", MI_MALE01, ARRAY_SIZE(aLhandAnimations), 5, aLhandAnimations, aHandAnimDescs },
	{ "rhand", "ghands", MI_MALE01, ARRAY_SIZE(aRhandAnimations), 5, aRhandAnimations, aHandAnimDescs },
	{ "carry", "carry", MI_MALE01, ARRAY_SIZE(aCarryAnimations), 3, aCarryAnimations, aCarryDescs },
	{ "carry05", "carry", MI_MALE01, ARRAY_SIZE(aCarry05Animations), 3, aCarry05Animations, aCarryDescs },
	{ "carry105", "carry", MI_MALE01, ARRAY_SIZE(aCarry105Animations), 3, aCarry105Animations, aCarryDescs },
	{ "int_house", "int_house", MI_MALE01, ARRAY_SIZE(aIntHouseAnimations), 10, aIntHouseAnimations, aIntHouseAnimDescs },
	{ "int_office", "int_office", MI_MALE01, ARRAY_SIZE(aIntOfficeAnimations), 9, aIntOfficeAnimations, aIntOfficeAnimDescs },
	{ "int_shop", "int_shop", MI_MALE01, ARRAY_SIZE(aIntShopAnimations), 3, aIntShopAnimations, aIntShopAnimDescs },
	{ "stealth_kn", "knife", MI_MALE01, ARRAY_SIZE(aStealth_knAnimations), 4, aStealth_knAnimations, aStealthDescs },
	{ "stdcaramims", "ped", MI_MALE01, ARRAY_SIZE(aStdCarAnimations), 39, aStdCarAnimations, aCarAnimDescs2 },
	{ "lowcaramims", "ped", MI_MALE01, ARRAY_SIZE(aLowCarAnimations), 39, aLowCarAnimations, aLowCarAnimDiscs },
	{ "trkcaranims", "truck", MI_MALE01, ARRAY_SIZE(aTrkCarAnimations), 39, aTrkCarAnimations, aTruckAnimDescs },
	{ "stdbikeanims", "ped", MI_MALE01, ARRAY_SIZE(aStdBikeAnimations), 39, aStdBikeAnimations, aStdBikeAnimDescs },
	{ "sportbikeanims", "bikes", MI_MALE01, ARRAY_SIZE(aSportBikeAnimations), 39, aSportBikeAnimations, aStdBikeAnimDescs },
	{ "vespabikeanims", "bikev", MI_MALE01, ARRAY_SIZE(aVespaBikeAnimations), 39, aVespaBikeAnimations, aStdBikeAnimDescs },
	{ "harleybikeanims", "bikeh", MI_MALE01, ARRAY_SIZE(aHarleyBikeAnimations), 39, aHarleyBikeAnimations, aStdBikeAnimDescs },
	{ "dirtbikeanims", "biked", MI_MALE01, ARRAY_SIZE(aDirtBikeAnimations), 39, aDirtBikeAnimations, aStdBikeAnimDescs },
	{ "wayfbikeanims", "wayfarer", MI_MALE01, ARRAY_SIZE(aWayfBikeAnimations), 39, aWayfBikeAnimations, aStdBikeAnimDescs },
	{ "bmxbikeanims", "bmx", MI_MALE01, ARRAY_SIZE(aBmxBikeAnimations), 39, aBmxBikeAnimations, aStdBikeAnimDescs },
	{ "mtbbikeanims", "mtb", MI_MALE01, ARRAY_SIZE(aMtbBikeAnimations), 39, aMtbBikeAnimations, aStdBikeAnimDescs },
	{ "choppabikeanims", "choppa", MI_MALE01, ARRAY_SIZE(aChoppaBikeAnimations), 39, aChoppaBikeAnimations, aStdBikeAnimDescs },
	{ "quadbikeanims", "quad", MI_MALE01, ARRAY_SIZE(aQuadBikeAnimations), 39, aQuadBikeAnimations, aStdBikeAnimDescs },
	{ "vancaranims", "van", MI_MALE01, ARRAY_SIZE(aVanCarAnimations), 39, aVanCarAnimations, aCarAnimDescs1 },
	{ "rustplaneanims", "rustler", MI_MALE01, ARRAY_SIZE(aRustplaneAnimations), 39, aRustplaneAnimations, aCarAnimDescs1 },
	{ "coachcaranims", "coach", MI_MALE01, ARRAY_SIZE(aCoachCarAnimations), 39, aCoachCarAnimations, aCarAnimDescs2 },
	{ "buscaranims", "bus", MI_MALE01, ARRAY_SIZE(aBusCarAnimations), 39, aBusCarAnimations, aCarAnimDescs2 },
	{ "dozercaranims", "dozer", MI_MALE01, ARRAY_SIZE(aDozerCarAnimations), 39, aDozerCarAnimations, aCarAnimDescs2 },
	{ "kartcaranims", "kart", MI_MALE01, ARRAY_SIZE(aKartCarAnimations), 39, aKartCarAnimations, aCarAnimDescs2 },
	{ "convcaranims", "ped", MI_MALE01, ARRAY_SIZE(aConvCarAnimations), 39, aConvCarAnimations, aCarAnimDescs2 },
	{ "mtrkcaranims", "truck", MI_MALE01, ARRAY_SIZE(aTrkCarAnimations), 39, aTrkCarAnimations, aTruckAnimDescs },
	{ "traincarranims", "coach", MI_MALE01, ARRAY_SIZE(aCoachCarAnimations), 39, aCoachCarAnimations, aCarAnimDescs2 },
	{ "stdtallcaramims", "ped", MI_MALE01, ARRAY_SIZE(aStdCarAnimations), 39, aStdCarAnimations, aCarAnimDescs2 },
	{ "hovercaranims", "vortex", MI_MALE01, ARRAY_SIZE(aHoverCarAnimations), 39, aHoverCarAnimations, aCarAnimDescs2 },
	{ "tankcaranims", "tank", MI_MALE01, ARRAY_SIZE(aTankCarAnimations), 39, aTankCarAnimations, aCarAnimDescs2 },
	{ "bfinjcaramims", "BF_injection", MI_MALE01, ARRAY_SIZE(aBfinjCarAnimations), 39, aBfinjCarAnimations, aCarAnimDescs2 },
	{ "learplaneanims", "shamal", MI_MALE01, ARRAY_SIZE(aLearPlaneAnimations), 39, aLearPlaneAnimations, aCarAnimDescs1 },
	{ "harrplaneanims", "rustler", MI_MALE01, ARRAY_SIZE(aHarrPlaneAnimations), 39, aHarrPlaneAnimations, aCarAnimDescs1 },
	{ "stdcarupright", "ped", MI_MALE01, ARRAY_SIZE(aStdCarAnimations), 39, aStdCarAnimations, aCarAnimDescs2 },
	{ "nvadaplaneanims", "nevada", MI_MALE01, ARRAY_SIZE(aNevadaPlaneAnimations), 39, aNevadaPlaneAnimations, aCarAnimDescs1 },

	// SA compiles only the first 118 groups into the game; the ped styles from
	// index 118 on are named by data/animgrp.dat at startup, so their slots
	// start empty and NameReservedAnimGroups gives them a name to be found by.
	// ASSOCGRP_MAN and the 19 sets after it are filled from that file.
};
int32 CAnimManager::ms_numAnimAssocDefinitions = ARRAY_SIZE(ms_aAnimAssocDefinitions);

// Groups whose definition came from data/animgrp.dat own their name array,
// their clip blocks and their block name, so they are the ones to release.
static bool s_dataDefinedGroup[NUM_ANIM_ASSOC_GROUPS];

// Group names in definition order, matching the AssocGroupId values. The
// group table is indexed by the same number, so data/animgrp.dat can fill
// the entries that SA does not compile into the game.
static const char *aAnimGroupNames[NUM_ANIM_ASSOC_GROUPS] = {
	"default",
	"door",
	"bikes",
	"bikev",
	"bikeh",
	"biked",
	"wayfarer",
	"bmx",
	"mtb",
	"choppa",
	"quad",
	"python",
	"pythonbad",
	"colt45",
	"colt_cop",
	"colt45pro",
	"sawnoff",
	"sawnoffpro",
	"silenced",
	"shotgun",
	"shotgunbad",
	"buddy",
	"buddybad",
	"uzi",
	"uzibad",
	"rifle",
	"riflebad",
	"sniper",
	"grenade",
	"flame",
	"rocket",
	"spraycan",
	"goggles",
	"melee_1",
	"melee_2",
	"melee_3",
	"melee_4",
	"bbbat_1",
	"gclub_1",
	"knife_1",
	"sword_1",
	"dildo_1",
	"flowers_1",
	"csaw_1",
	"kick_std",
	"pistlwhp",
	"medic",
	"beach",
	"sunbathe",
	"playidles",
	"riot",
	"strip",
	"gangs",
	"attractors",
	"player",
	"fat",
	"muscular",
	"playerrocket",
	"playerrocketf",
	"playerrocketm",
	"player2armed",
	"player2armedf",
	"player2armedm",
	"playerbbbat",
	"playerbbbatf",
	"playerbbbatm",
	"playercsaw",
	"playercsawf",
	"playercsawm",
	"playersneak",
	"playerjetpack",
	"swim",
	"drivebys",
	"bike_dbz",
	"cop_dbz",
	"quad_dbz",
	"fat_tired",
	"handsignal",
	"handsignall",
	"lhand",
	"rhand",
	"carry",
	"carry05",
	"carry105",
	"int_house",
	"int_office",
	"int_shop",
	"stealth_kn",
	"stdcaramims",
	"lowcaramims",
	"trkcaranims",
	"stdbikeanims",
	"sportbikeanims",
	"vespabikeanims",
	"harleybikeanims",
	"dirtbikeanims",
	"wayfbikeanims",
	"bmxbikeanims",
	"mtbbikeanims",
	"choppabikeanims",
	"quadbikeanims",
	"vancaranims",
	"rustplaneanims",
	"coachcaranims",
	"buscaranims",
	"dozercaranims",
	"kartcaranims",
	"convcaranims",
	"mtrkcaranims",
	"traincarranims",
	"stdtallcaramims",
	"hovercaranims",
	"tankcaranims",
	"bfinjcaramims",
	"learplaneanims",
	"harrplaneanims",
	"stdcarupright",
	"nvadaplaneanims",
	"man",
	"shuffle",
	"oldman",
	"gang1",
	"gang2",
	"oldfatman",
	"fatman",
	"jogger",
	"drunkman",
	"blindman",
	"swat",
	"woman",
	"shopping",
	"busywoman",
	"sexywoman",
	"pro",
	"oldwoman",
	"fatwoman",
	"jogwoman",
	"oldfatwoman",
	"skate",
};


// The compiled table covers 118 groups. The rest of the enum - the civilian
// styles and the driver variants - get their definition from data/animgrp.dat
// at startup, but they still need a name: an IDE names a group by string, and
// it may be read before the file is.
void
CAnimManager::NameReservedAnimGroups(void)
{
	for(int i = 0; i < NUM_ANIM_ASSOC_GROUPS; i++){
		AnimAssocDefinition *def = &ms_aAnimAssocDefinitions[i];
		if(def->name == nil)
			def->name = aAnimGroupNames[i];
	}
}
#undef awc

void
CAnimManager::Initialise(void)
{
	ms_numAnimations = 0;
	ms_numAnimBlocks = 0;
	ms_aAnimAssocGroups = nil;
	ms_animCache.Init(25);
}

void
CAnimManager::Shutdown(void)
{
	int i;

	for(i = 0; i < NUMANIMBLOCKS; i++)
		CStreaming::RemoveAnim(i);

	for(i = 0; i < ms_numAnimations; i++)
		ms_aAnimations[i].Shutdown();

	ms_animCache.Shutdown();

	delete[] ms_aAnimAssocGroups;
	ms_aAnimAssocGroups = nil;

	// The group definitions read from data/animgrp.dat were allocated here and
	// are handed back with everything else, so a restart starts from the
	// compiled table again.
	for(i = 0; i < NUM_ANIM_ASSOC_GROUPS; i++){
		if(!s_dataDefinedGroup[i])
			continue;
		AnimAssocDefinition *def = &ms_aAnimAssocDefinitions[i];
		for(int j = 0; j < def->numAnims; j++)
			RwFree((void*)def->animNames[j]);
		RwFree((void*)def->animNames);
		RwFree(def->animDescs);
		free((void*)def->blockName);
		def->animNames = nil;
		def->animDescs = nil;
		def->blockName = nil;
		def->numAnims = 0;
		def->numDescs = 0;
		s_dataDefinedGroup[i] = false;
	}
}

void
CAnimManager::UncompressAnimation(CAnimBlendHierarchy *hier)
{
	if(hier->keepCompressed){
		if(hier->totalLength == 0.0f)
			hier->CalcTotalTimeCompressed();
	}else{
		if(!hier->compressed){
			if(hier->linkPtr){
				hier->linkPtr->Remove();
				ms_animCache.head.Insert(hier->linkPtr);
			}
		}else{
			CLink<CAnimBlendHierarchy*> *link = ms_animCache.Insert(hier);
			if(link == nil){
				CAnimBlendHierarchy *lastHier = ms_animCache.tail.prev->item;
				lastHier->RemoveUncompressedData();
				ms_animCache.Remove(ms_animCache.tail.prev);
				lastHier->linkPtr = nil;
				link = ms_animCache.Insert(hier);
			}
			hier->linkPtr = link;
			hier->Uncompress();
		}
	}
}

void
CAnimManager::RemoveFromUncompressedCache(CAnimBlendHierarchy *hier)
{
	if(hier->linkPtr){
		ms_animCache.Remove(hier->linkPtr);
		hier->linkPtr = nil;
	}
}

CAnimBlock*
CAnimManager::GetAnimationBlock(const char *name)
{
	int i;

	for(i = 0; i < ms_numAnimBlocks; i++)
		if(strcasecmp(ms_aAnimBlocks[i].name, name) == 0)
			return &ms_aAnimBlocks[i];
	return nil;
}

int32
CAnimManager::GetAnimationBlockIndex(const char *name)
{
	int i;

	for(i = 0; i < ms_numAnimBlocks; i++)
		if(strcasecmp(ms_aAnimBlocks[i].name, name) == 0)
			return i;
	return -1;
}

int32
CAnimManager::GetAnimationBlockForHierarchy(CAnimBlendHierarchy *hierarchy)
{
	if(hierarchy == nil)
		return -1;
	for(int32 i = 0; i < ms_numAnimBlocks; i++){
		CAnimBlock *block = &ms_aAnimBlocks[i];
		if(block->firstIndex < 0 || block->firstIndex > NUMANIMATIONS || block->numAnims <= 0 ||
		   block->numAnims > NUMANIMATIONS - block->firstIndex)
			continue;
		if(hierarchy >= &ms_aAnimations[block->firstIndex] &&
		   hierarchy < &ms_aAnimations[block->firstIndex + block->numAnims])
			return i;
	}
	return -1;
}

int32
CAnimManager::RegisterAnimBlock(const char *name)
{
	CAnimBlock *animBlock = GetAnimationBlock(name);
	if(animBlock == nil){
		if(ms_numAnimBlocks >= NUMANIMBLOCKS){
			debug("ANIMS: no room to register block %s\n", name);
			return -1;
		}
		animBlock = &ms_aAnimBlocks[ms_numAnimBlocks++];
		strncpy(animBlock->name, name, MAX_ANIMBLOCK_NAME - 1);
		animBlock->name[MAX_ANIMBLOCK_NAME - 1] = '\0';
		animBlock->isLoaded = false;
		animBlock->refCount = 0;
		animBlock->firstIndex = 0;
		animBlock->numAnims = 0;
		animBlock->unloadPending = false;
	}
	return animBlock - ms_aAnimBlocks;
}

int32
CAnimManager::GetNumRefsToAnimBlock(int32 block)
{
	return block >= 0 && block < ms_numAnimBlocks ? ms_aAnimBlocks[block].refCount : 0;
}

void
CAnimManager::AddAnimBlockRef(int32 block)
{
	if(block >= 0 && block < ms_numAnimBlocks)
		ms_aAnimBlocks[block].refCount++;
}

static void
RemovePendingAnimationBlock(int32 block)
{
	CAnimBlock *animBlock = CAnimManager::GetAnimationBlock(block);
	if(strncasecmp(animBlock->name, "sa_", 3) == 0 || !CStreaming::HasAnimLoaded(block))
		CAnimManager::RemoveAnimBlock(block);
	else
		CStreaming::RemoveAnim(block);
}

void
CAnimManager::RemoveAnimBlockRefWithoutDelete(int32 block)
{
	CAnimBlock *animBlock;
	if(block < 0 || block >= ms_numAnimBlocks || ms_aAnimBlocks[block].refCount <= 0)
		return;
	animBlock = &ms_aAnimBlocks[block];
	animBlock->refCount--;
	if(animBlock->refCount == 0 && animBlock->unloadPending)
		RemovePendingAnimationBlock(block);
}

void
CAnimManager::RemoveAnimBlockRef(int32 block)
{
	if(block < 0 || block >= ms_numAnimBlocks || ms_aAnimBlocks[block].refCount <= 0)
		return;
	ms_aAnimBlocks[block].refCount--;
	if(ms_aAnimBlocks[block].refCount == 0){
		if(ms_aAnimBlocks[block].unloadPending)
			RemovePendingAnimationBlock(block);
		else
			CStreaming::RemoveAnim(block);
	}
}

void
CAnimManager::RemoveAnimBlock(int32 block)
{
	int i;
	CAnimBlock *animblock;
	int32 firstAnim, numAnims;
	bool validAnimationRange;

	animblock = &ms_aAnimBlocks[block];
	firstAnim = animblock->firstIndex;
	numAnims = animblock->numAnims;
	validAnimationRange = firstAnim >= 0 && firstAnim <= NUMANIMATIONS &&
		numAnims >= 0 && numAnims <= NUMANIMATIONS - firstAnim;
	// A custom SA bank may be registered next to a native block of the same
	// name. Release it together with that block so custom dictionaries do not
	// accumulate in the fixed animation table.
	if(strncasecmp(animblock->name, "sa_", 3) != 0){
		char saBlockName[MAX_ANIMBLOCK_NAME];
		CAnimBlock *saBlock;
		snprintf(saBlockName, sizeof(saBlockName), "sa_%s", animblock->name);
		saBlock = GetAnimationBlock(saBlockName);
		if(saBlock && saBlock != animblock && saBlock->isLoaded){
			if(saBlock->refCount > 0)
				saBlock->unloadPending = true;
			else
				RemoveAnimBlock(saBlock - ms_aAnimBlocks);
		}
	}
	debug("Removing ANIMS %s\n", animblock->name);
	for(i = 0; i < NUM_ANIM_ASSOC_GROUPS; i++){
		CAnimBlendAssocGroup *group = ms_aAnimAssocGroups ? &ms_aAnimAssocGroups[i] : nil;
		bool usesBlock = group && group->animBlock == animblock;
		if(group && !usesBlock && group->assocList){
			for(int j = 0; j < group->numAssociations; j++){
				CAnimBlendHierarchy *hier = group->assocList[j].hierarchy;
				if(validAnimationRange && hier && hier >= &ms_aAnimations[firstAnim] &&
				   hier < &ms_aAnimations[firstAnim + numAnims]){
					usesBlock = true;
					break;
				}
			}
		}
		if(usesBlock)
			group->DestroyAssociations();
	}
	if(validAnimationRange){
		for(i = 0; i < numAnims; i++)
			ms_aAnimations[firstAnim + i].Shutdown();
	}else{
		debug("ANIMS %s: invalid animation table range, skipping hierarchy cleanup\n", animblock->name);
	}
	animblock->isLoaded = false;
	animblock->refCount = 0;
	animblock->unloadPending = false;
}

CAnimBlendHierarchy*
CAnimManager::GetAnimation(const char *name, CAnimBlock *animBlock)
{
	int i;
	CAnimBlendHierarchy *hier;
	if(name == nil || animBlock == nil || animBlock->firstIndex < 0 ||
	   animBlock->firstIndex >= NUMANIMATIONS || animBlock->numAnims <= 0 ||
	   animBlock->numAnims > NUMANIMATIONS - animBlock->firstIndex)
		return nil;
	hier = &ms_aAnimations[animBlock->firstIndex];

	for(i = 0; i < animBlock->numAnims; i++){
		if(strcasecmp(hier->name, name) == 0)
			return hier;
		hier++;
	}
	return nil;
}

const char*
CAnimManager::GetAnimGroupName(AssocGroupId groupId)
{
	if(groupId < 0 || groupId >= NUM_ANIM_ASSOC_GROUPS)
		return aAnimGroupNames[0];
	// A group that only data/animgrp.dat can define still has a name: the
	// enum already fixes one per id, and callers compare against it before
	// the file is read.
	const char *name = ms_aAnimAssocDefinitions[groupId].name;
	return name ? name : aAnimGroupNames[groupId];
}

const char*
CAnimManager::GetAnimName(AssocGroupId groupId, AnimationId animId)
{
	if(groupId < 0 || groupId >= NUM_ANIM_ASSOC_GROUPS)
		return nil;
	const AnimAssocDefinition *def = &ms_aAnimAssocDefinitions[groupId];
	if(def->numAnims <= 0)
		return nil;
	if(def->numDescs <= 0)
		return nil;
	int index = animId - def->animDescs[0].animId;
	if(index < 0 || index >= def->numDescs || def->animDescs[index].animId != animId)
		return nil;
	return def->animNames[index];
}

static bool
IsSAAnimationBlock(const CAnimBlock *block)
{
	return block && block->isLoaded && strncasecmp(block->name, "sa_", 3) == 0;
}

bool
CAnimManager::IsAnimationCompatibleWithClump(RpClump *clump, CAnimBlendHierarchy *hierarchy)
{
	if(hierarchy == nil)
		return false;
	CAnimBlendClumpData *data = clump ? *RPANIMBLENDCLUMPDATA(clump) : nil;
	if(clump && (data == nil || data->frames == nil || data->numFrames <= 0))
		return false;
	int blockIndex = GetAnimationBlockForHierarchy(hierarchy);
	if(blockIndex < 0)
		return false;
	CAnimBlock *block = GetAnimationBlock(blockIndex);
	return block->isLoaded && IsSAAnimationBlock(block);
}

CAnimBlendAssocGroup*
CAnimManager::GetAnimAssocGroup(RpClump *clump, AssocGroupId groupId)
{
	if(clump == nil || !RpAnimBlendClumpIsInitialized(clump) ||
	   groupId < 0 || groupId >= NUM_ANIM_ASSOC_GROUPS)
		return nil;
	if(ms_aAnimAssocGroups == nil)
		return nil;
	CAnimBlendAssocGroup *group = &ms_aAnimAssocGroups[groupId];
	if(group->assocList == nil || group->numAssociations <= 0 ||
	   group->animBlock == nil || !group->animBlock->isLoaded ||
	   !IsSAAnimationBlock(group->animBlock))
		return nil;
	return group;
}

CAnimBlendAssociation*
CAnimManager::CreateAnimAssociation(AssocGroupId groupId, AnimationId animId)
{
	CAnimBlendAssociation *assoc = GetAnimAssociation(groupId, animId);
	return assoc ? ms_aAnimAssocGroups[groupId].CopyAnimation(animId) : nil;
}

CAnimBlendAssociation*
CAnimManager::CreateAnimAssociation(RpClump *clump, AssocGroupId groupId, AnimationId animId)
{
	if(clump == nil)
		return nil;
	CAnimBlendAssocGroup *group = GetAnimAssocGroup(clump, groupId);
	CAnimBlendAssociation *assoc = group ? group->CopyAnimation(animId, clump) : nil;
	if(assoc == nil){
		// Same rule as BlendAnimation: the id decides the group when the one
		// the caller named does not declare the clip.
		AssocGroupId owner = GetAnimGroupForAnimation(animId);
		if(owner != groupId){
			group = GetAnimAssocGroup(clump, owner);
			assoc = group ? group->CopyAnimation(animId, clump) : nil;
		}
	}
	return assoc;
}

AssocGroupId
CAnimManager::GetAnimGroupForAnimation(AnimationId animId)
{
	// Groups do share clip ids: every car set carries the same seat and door
	// clips, so thirty groups declare CAR_GETIN_LHS_0 and choosing between them
	// by range alone would hand a saloon the animation set of a plane. Prefer
	// the group that is the natural home of the clip - the standard saloon set
	// for the common car clips, the melee sets for fighting - and fall back to
	// the first group that declares the id.
	static const AssocGroupId preferred[] = {
		ASSOCGRP_STDCARAMIMS, ASSOCGRP_MELEE_1, ASSOCGRP_KICK_STD,
		ASSOCGRP_COLT45, ASSOCGRP_UZI, ASSOCGRP_RIFLE, ASSOCGRP_SHOTGUNBAD,
		ASSOCGRP_BIKEV, ASSOCGRP_BIKES, ASSOCGRP_BUDDY, ASSOCGRP_VANCARANIMS,
	};
	for(int p = 0; p < (int)ARRAY_SIZE(preferred); p++){
		const AnimAssocDefinition *def = &ms_aAnimAssocDefinitions[preferred[p]];
		if(def->numDescs <= 0 || def->animDescs == nil)
			continue;
		int index = animId - def->animDescs[0].animId;
		if(index >= 0 && index < def->numDescs && def->animDescs[index].animId == animId)
			return preferred[p];
	}
	for(int i = 0; i < NUM_ANIM_ASSOC_GROUPS; i++){
		const AnimAssocDefinition *def = &ms_aAnimAssocDefinitions[i];
		if(def->numDescs <= 0 || def->animDescs == nil)
			continue;
		int index = animId - def->animDescs[0].animId;
		if(index >= 0 && index < def->numDescs && def->animDescs[index].animId == animId)
			return (AssocGroupId)i;
	}
	return ASSOCGRP_DEFAULT;
}

CAnimBlendAssociation*
CAnimManager::GetAnimAssociation(AssocGroupId groupId, AnimationId animId)
{
	if(ms_aAnimAssocGroups == nil || groupId < 0 || groupId >= NUM_ANIM_ASSOC_GROUPS)
		return nil;
	CAnimBlendAssociation *assoc = ms_aAnimAssocGroups[groupId].GetAnimation(animId);
	return assoc && IsAnimationCompatibleWithClump(nil, assoc->hierarchy) ? assoc : nil;
}

static CAnimBlendHierarchy *FindSAAnimationHierarchy(int groupId, const char *name, CAnimBlock *primaryBlock);

CAnimBlendHierarchy*
CAnimManager::GetAnimHierarchyForOffset(AssocGroupId groupId, AnimationId animId)
{
	return GetAnimHierarchyForOffset(nil, groupId, animId);
}

CAnimBlendHierarchy*
CAnimManager::GetAnimHierarchyForOffset(RpClump *clump, AssocGroupId groupId, AnimationId animId)
{
	if(groupId < 0 || groupId >= NUM_ANIM_ASSOC_GROUPS)
		return nil;
	const AnimAssocDefinition *def = &ms_aAnimAssocDefinitions[groupId];
	if(def->numAnims <= 0)
		return nil;
	if(def->numDescs <= 0)
		return nil;
	int index = animId - def->animDescs[0].animId;
	if(index < 0 || index >= def->numDescs || def->animDescs[index].animId != animId)
		return nil;

	CAnimBlendAssociation *assoc = clump ? GetAnimAssociation(clump, groupId, animId) :
		GetAnimAssociation(groupId, animId);
	if(assoc)
		return assoc->hierarchy;
	// Groups need a reference skeleton, but root-translation lookups do not.
	// Resolve against this clump's own bank even if no reference is resident.
	CAnimBlendClumpData *data = clump ? *RPANIMBLENDCLUMPDATA(clump) : nil;
	if(clump && (data == nil || !RpAnimBlendClumpIsInitialized(clump)))
		return nil;
	(void)data;
	if(def->blockName){
		char name[MAX_ANIMBLOCK_NAME];
		snprintf(name, sizeof(name), "sa_%s", def->blockName);
		CAnimBlock *block = GetAnimationBlock(name);
		if(!IsSAAnimationBlock(block))
			block = GetAnimationBlock("sa_ped");
		CAnimBlendHierarchy *hier = IsSAAnimationBlock(block) ?
			FindSAAnimationHierarchy(groupId, def->animNames[index], block) : nil;
		return IsAnimationCompatibleWithClump(clump, hier) ? hier : nil;
	}
	return nil;
}

static const char *GetSAAnimationAlias(int groupId, const char *name, const char **sourceBlock);

CAnimBlendAssociation*
CAnimManager::GetAnimAssociation(AssocGroupId groupId, const char *name)
{
	if(ms_aAnimAssocGroups == nil || groupId < 0 || groupId >= NUM_ANIM_ASSOC_GROUPS || name == nil)
		return nil;
	CAnimBlendAssociation *assoc = ms_aAnimAssocGroups[groupId].GetAnimation(name);
	if(assoc == nil){
		// Data files name clips the Vice City way - fistfite.dat asks for
		// FIGHTjab and HIT_head. Those names are not in the SA set; the same
		// translation the groups are built with says which clip is meant.
		const char *alias = GetSAAnimationAlias(groupId, name, nil);
		if(alias && strcasecmp(alias, name) != 0)
			assoc = ms_aAnimAssocGroups[groupId].GetAnimation(alias);
	}
	if(assoc == nil){
		// SA puts the reaction and knockdown clips in the style group, not in
		// every group that plays them: fistfite names HIT_front, FIGHT2IDLE and
		// FightShF, which only the standard set declares. Look the name up
		// everywhere before reporting it missing, so a melee table row gets the
		// clip it names instead of no clip at all.
		for(int g = 0; g < NUM_ANIM_ASSOC_GROUPS && assoc == nil; g++){
			if(g == groupId)
				continue;
			CAnimBlendAssociation *found = ms_aAnimAssocGroups[g].GetAnimation(name);
			if(found && IsAnimationCompatibleWithClump(nil, found->hierarchy))
				assoc = found;
		}
	}
	return assoc && IsAnimationCompatibleWithClump(nil, assoc->hierarchy) ? assoc : nil;
}

CAnimBlendAssociation*
CAnimManager::GetAnimAssociation(RpClump *clump, AssocGroupId groupId, AnimationId animId)
{
	CAnimBlendAssocGroup *group = GetAnimAssocGroup(clump, groupId);
	CAnimBlendAssociation *assoc = group ? group->GetAnimation(animId) : nil;
	return assoc && IsAnimationCompatibleWithClump(clump, assoc->hierarchy) ? assoc : nil;
}

CAnimBlendAssociation*
CAnimManager::GetAnimAssociation(RpClump *clump, AssocGroupId groupId, const char *name)
{
	CAnimBlendAssocGroup *group = GetAnimAssocGroup(clump, groupId);
	CAnimBlendAssociation *assoc = group ? group->GetAnimation(name) : nil;
	return assoc && IsAnimationCompatibleWithClump(clump, assoc->hierarchy) ? assoc : nil;
}

CAnimBlendAssociation*
CAnimManager::AddAnimation(RpClump *clump, AssocGroupId groupId, AnimationId animId)
{
	CAnimBlendAssociation *anim = CreateAnimAssociation(clump, groupId, animId);
	if(anim == nil || clump == nil)
		return nil;
	CAnimBlendClumpData *clumpData = *RPANIMBLENDCLUMPDATA(clump);
	if(anim->IsMovement()){
		CAnimBlendAssociation *syncanim = nil;
		CAnimBlendLink *link;
		for(link = clumpData->link.next; link; link = link->next){
			syncanim = CAnimBlendAssociation::FromLink(link);
			if(syncanim->IsMovement())
				break;
		}
		if(link){
			anim->SyncAnimation(syncanim);
			anim->flags |= ASSOC_RUNNING;
		}else
			anim->Start(0.0f);
	}else
		anim->Start(0.0f);

	clumpData->link.Prepend(&anim->link);
	return anim;
}

CAnimBlendAssociation*
CAnimManager::AddAnimationAndSync(RpClump *clump, CAnimBlendAssociation *syncanim, AssocGroupId groupId, AnimationId animId)
{
	CAnimBlendAssociation *anim = CreateAnimAssociation(clump, groupId, animId);
	if(anim == nil || clump == nil)
		return nil;
	CAnimBlendClumpData *clumpData = *RPANIMBLENDCLUMPDATA(clump);
	if (anim->IsMovement() && syncanim){
		anim->SyncAnimation(syncanim);
		anim->flags |= ASSOC_RUNNING;
	}else
		anim->Start(0.0f);

	clumpData->link.Prepend(&anim->link);
	return anim;
}

CAnimBlendAssociation*
CAnimManager::BlendAnimation(RpClump *clump, AssocGroupId groupId, AnimationId animId, float delta)
{
	int removePrevAnim = 0;
	if(clump == nil)
		return nil;
	CAnimBlendClumpData *clumpData = *RPANIMBLENDCLUMPDATA(clump);
	if(clumpData == nil)
		return nil;
	CAnimBlendAssociation *requested = GetAnimAssociation(clump, groupId, animId);
	if(requested == nil){
		// Game logic asks for clips by id and usually names the standard group,
		// but a clip only exists in the group that declares it: car door, melee
		// and bike clips are all outside the standard range. Ask the group that
		// owns the id before giving up, and keep that group for the new
		// association so it is found again next time.
		AssocGroupId owner = GetAnimGroupForAnimation(animId);
		if(owner == groupId)
			return nil;
		requested = GetAnimAssociation(clump, owner, animId);
		if(requested == nil)
			return nil;
		groupId = owner;
	}
	bool isMovement = requested->IsMovement();
	bool isPartial = requested->IsPartial();
	// gta-reversed also keeps facial clips apart from the ones they replace:
	// toBlendIsIndestructible. Without it a switching facial clip cancels an
	// ordinary one that shares its partial-ness.
	bool isFacial = requested->IsFacial();
	CAnimBlendLink *link, *next;
	CAnimBlendAssociation *found = nil, *movementAnim = nil;
	for(link = clumpData->link.next; link; link = next){
		next = link->next;
		CAnimBlendAssociation *anim = CAnimBlendAssociation::FromLink(link);
		if(!IsAnimationCompatibleWithClump(clump, anim->hierarchy)){
#ifdef CUSTOM_MODELS
			CUSTOM_LOG("anim assoc: discarding incompatible %s animation on ped clump\n",
				anim->hierarchy ? anim->hierarchy->name : "(null)");
#endif
			delete anim;
			continue;
		}
		if(isMovement && anim->IsMovement())
			movementAnim = anim;
		if(anim->animId == animId && anim->groupId == groupId &&
		   anim->hierarchy == requested->hierarchy)
			found = anim;
		else if(isPartial == anim->IsPartial() && isFacial == anim->IsFacial()){
			if(anim->blendAmount > 0.0f){
				// gta-reversed: a->m_BlendDelta = std::min(-0.05f, bd) - the
				// fade is never slower than 0.05 per second, otherwise a clip
				// that was almost gone lingered and two poses stayed mixed.
				float blendDelta = -delta*anim->blendAmount;
				if(blendDelta < anim->blendDelta || !isPartial)
					anim->blendDelta = blendDelta < -0.05f ? blendDelta : -0.05f;
			}else{
				anim->blendDelta = -1.0f;
			}
			anim->flags |= ASSOC_DELETEFADEDOUT;
			removePrevAnim = 1;
		}
	}
	if(found){
		found->blendDelta = (1.0f - found->blendAmount)*delta;
		if(!found->IsRunning() && found->currentTime == found->hierarchy->totalLength)
			found->Start(0.0f);
	}else{
		found = AddAnimationAndSync(clump, movementAnim, groupId, animId);
		if(found == nil)
			return nil;
		if(!removePrevAnim && !isPartial){
			found->blendAmount = 1.0f;
			return found;
		}
		found->blendAmount = 0.0f;
		found->blendDelta = delta;
	}
	UncompressAnimation(found->hierarchy);
	return found;
}

static bool
IsAnimAssocGroupReady(CAnimBlendAssocGroup *group)
{
	return group && group->assocList && group->numAssociations > 0 &&
		group->animBlock && group->animBlock->isLoaded;
}

static CPedModelInfo *
FindPedAnimationReference(int preferredModelIndex)
{
	CBaseModelInfo *baseInfo;
	CPedModelInfo *pedInfo;
	int i;

	// Only SA skeletons can drive the animation set now. Prefer the model
	// currently being set up, then any other loaded SA ped.
	if(preferredModelIndex >= 0 && preferredModelIndex < MODELINFOSIZE){
		baseInfo = CModelInfo::GetModelInfo(preferredModelIndex);
		if(baseInfo && baseInfo->GetModelType() == MITYPE_PED && baseInfo->GetRwObject()){
			pedInfo = (CPedModelInfo*)baseInfo;
			if(pedInfo->UsesSAAnimations())
				return pedInfo;
		}
	}

	for(i = MI_PLAYER; i <= MI_LAST_PED; i++){
		baseInfo = CModelInfo::GetModelInfo(i);
		if(baseInfo == nil || baseInfo->GetModelType() != MITYPE_PED || baseInfo->GetRwObject() == nil)
			continue;
		pedInfo = (CPedModelInfo*)baseInfo;
		if(pedInfo->UsesSAAnimations())
			return pedInfo;
	}
	return nil;
}

static const char *
GetSAAnimationAlias(int groupId, const char *name, const char **sourceBlock)
{
	if(sourceBlock)
		*sourceBlock = nil;
	if(name == nil)
		return nil;

	// Trucks reuse the van entry animations, so a clip asked for by the van
	// group is the right answer for the truck slots too.
	if(name == nil)
		return nil;
	if(strncasecmp(name, "TRUCK_", 6) == 0){
		static char vanName[32];
		snprintf(vanName, sizeof(vanName), "VAN_%s", name + 6);
		if(sourceBlock) *sourceBlock = nil;
		return vanName;
	}
	if(groupId == ASSOCGRP_FATMAN){
		if(sourceBlock) *sourceBlock = "sa_fat";
		if(strcasecmp(name, "walk_fat") == 0) return "fatwalk";
		if(strcasecmp(name, "run_civi") == 0) return "fatrun";
		if(strcasecmp(name, "sprint_panic") == 0 || strcasecmp(name, "woman_runpanic") == 0) return "fatsprint";
		if(strcasecmp(name, "idle_stance") == 0) return "fatidle";
		if(strcasecmp(name, "walk_start") == 0) return "fatwalkstart";
	}
	if(groupId == ASSOCGRP_OLDFATMAN){
		if(sourceBlock) *sourceBlock = "sa_fat";
		if(strcasecmp(name, "walk_fatold") == 0) return "fatwalk";
		if(strcasecmp(name, "run_fatold") == 0) return "fatrun";
		if(strcasecmp(name, "woman_runpanic") == 0) return "fatsprint";
		if(strcasecmp(name, "idle_stance") == 0) return "fatidle";
	}
	if(groupId == ASSOCGRP_FATWOMAN){
		if(sourceBlock) *sourceBlock = "sa_fat";
		if(strcasecmp(name, "walk_fat") == 0) return "fatwalk";
		if(strcasecmp(name, "woman_run") == 0) return "fatrun";
		if(strcasecmp(name, "woman_runpanic") == 0) return "fatsprint";
		if(strcasecmp(name, "woman_idlestance") == 0) return "fatidle";
	}
	if(groupId == ASSOCGRP_FATMAN){
		if(sourceBlock) *sourceBlock = "sa_fat";
		if(strcasecmp(name, "run_fatold") == 0) return "fatrun";
		if(strcasecmp(name, "woman_runpanic") == 0) return "fatsprint";
	}
	if(groupId == ASSOCGRP_OLDMAN && strcasecmp(name, "walk_old") == 0)
		return "walk_civi";
	if((groupId == ASSOCGRP_GANG1 || groupId == ASSOCGRP_GANG2) &&
	   (strcasecmp(name, "walk_gang1") == 0 || strcasecmp(name, "walk_gang2") == 0))
		return "walk_civi";
	if((groupId == ASSOCGRP_GANG1 || groupId == ASSOCGRP_GANG2) && strcasecmp(name, "run_gang1") == 0)
		return "run_civi";
	if((groupId == ASSOCGRP_JOGGER || groupId == ASSOCGRP_JOGWOMAN) &&
	   (strcasecmp(name, "JOG_maleA") == 0 || strcasecmp(name, "JOG_maleB") == 0))
		return "walk_civi";
	if(groupId == ASSOCGRP_SHUFFLE && strcasecmp(name, "WALK_shuffle") == 0)
		return "walk_civi";
	if((groupId == ASSOCGRP_WOMAN || groupId == ASSOCGRP_SHOPPING ||
	    groupId == ASSOCGRP_BUSYWOMAN || groupId == ASSOCGRP_SEXYWOMAN ||
	    groupId == ASSOCGRP_OLDWOMAN || groupId == ASSOCGRP_JOGWOMAN) &&
	   (strncasecmp(name, "woman_walk", 10) == 0 || strcasecmp(name, "JOG_maleB") == 0))
		return "walk_civi";
	if((groupId == ASSOCGRP_WOMAN || groupId == ASSOCGRP_SHOPPING ||
	    groupId == ASSOCGRP_BUSYWOMAN || groupId == ASSOCGRP_SEXYWOMAN ||
	    groupId == ASSOCGRP_OLDWOMAN || groupId == ASSOCGRP_JOGWOMAN) &&
	   strcasecmp(name, "woman_run") == 0)
		return "run_civi";
	if((groupId == ASSOCGRP_WOMAN || groupId == ASSOCGRP_SHOPPING ||
	    groupId == ASSOCGRP_BUSYWOMAN || groupId == ASSOCGRP_SEXYWOMAN ||
	    groupId == ASSOCGRP_OLDWOMAN || groupId == ASSOCGRP_JOGWOMAN) &&
	   strcasecmp(name, "woman_runpanic") == 0)
		return "sprint_panic";
	if(groupId == ASSOCGRP_FATWOMAN && strcasecmp(name, "woman_run") == 0)
		return "fatrun";
	if((groupId == ASSOCGRP_WOMAN || groupId == ASSOCGRP_SHOPPING ||
	    groupId == ASSOCGRP_BUSYWOMAN || groupId == ASSOCGRP_SEXYWOMAN ||
	    groupId == ASSOCGRP_OLDWOMAN || groupId == ASSOCGRP_JOGWOMAN) &&
	   strcasecmp(name, "woman_idlestance") == 0)
		return "idle_stance";
	if(groupId == ASSOCGRP_PLAYER && strcasecmp(name, "run_1armed") == 0)
		return "run_armed";
	if(groupId == ASSOCGRP_UZI){
		if(sourceBlock) *sourceBlock = "sa_uzi";
		if(strcasecmp(name, "TEC_fire") == 0) return "UZI_fire";
		if(strcasecmp(name, "TEC_crouchfire") == 0) return "UZI_crouchfire";
		if(strcasecmp(name, "TEC_reload") == 0) return "UZI_reload";
		if(strcasecmp(name, "TEC_crouchreload") == 0) return "UZI_crouchreload";
	}
	if(groupId == ASSOCGRP_RIFLE){
		if(sourceBlock) *sourceBlock = "sa_rifle";
		if(strcasecmp(name, "M60_fire") == 0) return "RIFLE_fire";
		if(strcasecmp(name, "M60_reload") == 0) return "RIFLE_load";
	}
	if(groupId == ASSOCGRP_RIFLE){
		if(sourceBlock) *sourceBlock = "sa_rifle";
		if(strcasecmp(name, "WEAPON_sniper") == 0) return "RIFLE_fire";
	}
	if(groupId == ASSOCGRP_COLT45){
		if(sourceBlock) *sourceBlock = "sa_colt45";
		if(strcasecmp(name, "colt45_cop") == 0) return "colt45_fire_2hands";
	}
	if(groupId == ASSOCGRP_SUNBATHE){
		if(sourceBlock) *sourceBlock = "sa_sunbathe";
		if(strcasecmp(name, "bather") == 0) return "Lay_Bac_in";
		if(strcasecmp(name, "batherscape") == 0) return "batherup";
	}
	if(groupId == ASSOCGRP_DEFAULT && strcasecmp(name, "lance") == 0){
		if(sourceBlock) *sourceBlock = "sa_ped";
		return "idle_stance";
	}
	if(groupId == ASSOCGRP_DEFAULT){
		if(strcasecmp(name, "FIGHTsh_F") == 0) return "FightShF";
		if(strcasecmp(name, "FIGHTsh_back") == 0) return "FightShB";
		if(strcasecmp(name, "KO_shot_arml") == 0 || strcasecmp(name, "KO_shot_armR") == 0 ||
		   strcasecmp(name, "KO_shot_legl") == 0 || strcasecmp(name, "KO_shot_legR") == 0)
			return "KO_shot_front";
		if(strcasecmp(name, "HIT_bodyblow") == 0 || strcasecmp(name, "HIT_chest") == 0 ||
		   strcasecmp(name, "HIT_head") == 0)
			return "HIT_front";
		if(strcasecmp(name, "FIGHTIDLE") == 0){
			if(sourceBlock) *sourceBlock = "sa_melee_1";
			return "FIGHTIDLE";
		}
		if(strcasecmp(name, "FIGHTbodyblow") == 0){
			if(sourceBlock) *sourceBlock = "sa_melee_1";
			return "FightA_1";
		}
		if(strcasecmp(name, "FIGHThead") == 0 || strcasecmp(name, "FIGHTpunch") == 0){
			if(sourceBlock) *sourceBlock = "sa_melee_1";
			return "FightA_2";
		}
		if(strcasecmp(name, "FIGHTkick") == 0){
			if(sourceBlock) *sourceBlock = "sa_fight_e";
			return "FIGHTkick";
		}
		if(strcasecmp(name, "FIGHTknee") == 0 || strcasecmp(name, "FIGHTrndhse") == 0 ||
		   strcasecmp(name, "FIGHTbkickL") == 0 || strcasecmp(name, "FIGHTbkickR") == 0){
			if(sourceBlock) *sourceBlock = "sa_melee_1";
			return "FightA_3";
		}
		if(strcasecmp(name, "FIGHTLhook") == 0 || strcasecmp(name, "FIGHTppunch") == 0 ||
		   strcasecmp(name, "FIGHTjab") == 0){
			if(sourceBlock) *sourceBlock = "sa_melee_1";
			return "FightA_1";
		}
		if(strcasecmp(name, "FIGHTlngkck") == 0){
			if(sourceBlock) *sourceBlock = "sa_fight_e";
			return "FIGHTkick_B";
		}
		if(strcasecmp(name, "FIGHTelbowL") == 0 || strcasecmp(name, "FIGHTelbowR") == 0){
			if(sourceBlock) *sourceBlock = "sa_melee_1";
			return "FightA_2";
		}
		if(strcasecmp(name, "punchR") == 0){
			if(sourceBlock) *sourceBlock = "sa_melee_1";
			return "FightA_1";
		}
		if(strcasecmp(name, "KICK_floor") == 0){
			if(sourceBlock) *sourceBlock = "sa_fight_e";
			return "FIGHTkick_B";
		}
		if(strcasecmp(name, "WEAPON_throwu") == 0){
			if(sourceBlock) *sourceBlock = "sa_grenade";
			return "WEAPON_throwu";
		}
		if(strcasecmp(name, "DUCK_down") == 0 || strcasecmp(name, "DUCK_low") == 0)
			return "DUCK_cower";
		if(strcasecmp(name, "RBLOCK_Cshoot") == 0)
			return "WEAPON_crouch";
		if(strncasecmp(name, "CAR_", 4) == 0 || strncasecmp(name, "car_", 4) == 0){
			bool lowCar = strcasecmp(name, "CAR_pulloutL_LHS") == 0 ||
				strcasecmp(name, "CAR_pulloutL_RHS") == 0 ||
				strcasecmp(name, "CAR_getinL_LHS") == 0 ||
				strcasecmp(name, "CAR_getinL_RHS") == 0 ||
				strcasecmp(name, "CAR_closedoorL_LHS") == 0 ||
				strcasecmp(name, "CAR_closedoorL_RHS") == 0 ||
				strcasecmp(name, "CAR_Lshuffle_RHS") == 0 ||
				strcasecmp(name, "CAR_getoutL_LHS") == 0 ||
				strcasecmp(name, "CAR_getoutL_RHS") == 0 ||
				strcasecmp(name, "CAR_LjackedLHS") == 0 ||
				strcasecmp(name, "CAR_LjackedRHS") == 0 ||
				strcasecmp(name, "CAR_rolldoorLO") == 0;
			if(sourceBlock) *sourceBlock = lowCar ? "sa_lowcaramims" : "sa_stdcaramims";
			if(strcasecmp(name, "CAR_jumpin_LHS") == 0) return "CAR_getin_LHS";
			if(strcasecmp(name, "CAR_jumpin_RHS") == 0) return "CAR_getin_RHS";
			if(strcasecmp(name, "CAR_Qjack") == 0 || strcasecmp(name, "CAR_Qjacked") == 0)
				return "CAR_jackedRHS";
		}
	}
	if(groupId == ASSOCGRP_VANCARANIMS){
		if(sourceBlock) *sourceBlock = "sa_vancaranims";
		if(strcasecmp(name, "VAN_openL") == 0) return "VAN_open_back_LHS";
		if(strcasecmp(name, "VAN_getinL") == 0) return "VAN_getin_back_LHS";
		if(strcasecmp(name, "VAN_closeL") == 0) return "VAN_close_back_LHS";
		if(strcasecmp(name, "VAN_getoutL") == 0) return "VAN_getout_back_LHS";
		if(strcasecmp(name, "VAN_open") == 0) return "VAN_open_back_RHS";
		if(strcasecmp(name, "VAN_getin") == 0) return "VAN_getin_back_RHS";
		if(strcasecmp(name, "VAN_close") == 0) return "VAN_close_back_RHS";
		if(strcasecmp(name, "VAN_getout") == 0) return "VAN_getout_back_RHS";
	}
	if(groupId == ASSOCGRP_COACHCARANIMS && sourceBlock)
		*sourceBlock = "sa_coachcaranims";
	if(groupId == ASSOCGRP_STDBIKEANIMS && sourceBlock)
		*sourceBlock = "sa_sportbikeanims";
	if(groupId == ASSOCGRP_VESPABIKEANIMS && sourceBlock)
		*sourceBlock = "sa_vespabikeanims";
	if(groupId == ASSOCGRP_HARLEYBIKEANIMS && sourceBlock)
		*sourceBlock = "sa_harleybikeanims";
	if(groupId == ASSOCGRP_DIRTBIKEANIMS && sourceBlock)
		*sourceBlock = "sa_dirtbikeanims";
	// Every unarmed set is bound to the ped bank in this build; the sa_melee_*
	// and sa_fight_e blocks this table used to point at were never loaded, so
	// the alias resolved to nothing and the clip lookup fell through. Leaving
	// sourceBlock alone keeps the group's own primary block (sa_ped), which is
	// where these clips actually live.
	if(groupId == ASSOCGRP_MELEE_1 || groupId == ASSOCGRP_MELEE_2 ||
	   groupId == ASSOCGRP_MELEE_3 || groupId == ASSOCGRP_MELEE_4){
		if(strcasecmp(name, "punchR") == 0)
			return "FightA_1";
		if(strcasecmp(name, "FIGHTbodyblow") == 0 || strcasecmp(name, "FIGHTppunch") == 0)
			return "FightA_1";
		if(strcasecmp(name, "FIGHTIDLE") == 0)
			return "FIGHTIDLE";
		// The ground kick is FightA_G; there is no FIGHTkick_B clip in the bank.
		if(strcasecmp(name, "KICK_floor") == 0)
			return "FightA_G";
	}
	if(groupId == ASSOCGRP_BBBAT_1 || groupId == ASSOCGRP_GCLUB_1){
		if(sourceBlock)
			*sourceBlock = groupId == ASSOCGRP_GCLUB_1 ? "sa_gclub_1" : "sa_bbbat_1";
		if(strcasecmp(name, "WEAPON_bat_h") == 0 || strcasecmp(name, "WEAPON_golfclub") == 0)
			return "Bat_1";
		if(strcasecmp(name, "WEAPON_bat_v") == 0)
			return "Bat_2";
		if(strcasecmp(name, "BAT_PART") == 0)
			return "BAT_PART";
	}
	if(groupId == ASSOCGRP_KNIFE_1){
		if(sourceBlock) *sourceBlock = "sa_knife_1";
		if(strcasecmp(name, "WEAPON_knife_1") == 0) return "knife_1";
		if(strcasecmp(name, "WEAPON_knife_2") == 0) return "knife_2";
		if(strcasecmp(name, "WEAPON_knife_3") == 0) return "knife_3";
		if(strcasecmp(name, "WEAPON_knifeidle") == 0) return "WEAPON_knifeidle";
	}
	if(groupId == ASSOCGRP_CSAW_1){
		if(sourceBlock) *sourceBlock = "sa_csaw_1";
		if(strcasecmp(name, "WEAPON_csaw") == 0) return "CSaw_1";
		if(strcasecmp(name, "WEAPON_csawlo") == 0) return "CSaw_2";
		if(strcasecmp(name, "csaw_part") == 0) return "CSaw_Part";
	}
	return nil;
}

static const char *
GetSAStyleFallbackName(int groupId, const char *name)
{
	if(name == nil)
		return nil;
	if(groupId == ASSOCGRP_SKATE){
		if(strcasecmp(name, "skate_run") == 0) return "run_civi";
		if(strcasecmp(name, "skate_sprint") == 0) return "sprint_panic";
		if(strcasecmp(name, "skate_idle") == 0) return "idle_stance";
	}
#ifdef PC_PLAYER_CONTROLS
	if(groupId == ASSOCGRP_PLAYER){
		if(strncasecmp(name, "walk_start", 10) == 0) return "walk_start";
		if(strncasecmp(name, "walk_", 5) == 0) return "walk_player";
		if(strncasecmp(name, "run_", 4) == 0) return "run_player";
	}
	if(groupId == ASSOCGRP_PLAYERROCKET){
		if(strncasecmp(name, "walkst_rocket", 13) == 0) return "walk_start_rocket";
		if(strncasecmp(name, "walk_rocket", 11) == 0) return "walk_rocket";
		if(strncasecmp(name, "run_rocket", 10) == 0) return "run_rocket";
	}
	if(groupId == ASSOCGRP_PLAYERCSAW){
		if(strncasecmp(name, "walkst_csaw", 11) == 0) return "walk_start_csaw";
		if(strncasecmp(name, "walk_csaw", 9) == 0) return "walk_csaw";
		if(strncasecmp(name, "run_csaw", 8) == 0) return "run_csaw";
	}
#endif
	if(groupId != ASSOCGRP_SHUFFLE && groupId != ASSOCGRP_OLDMAN &&
	   groupId != ASSOCGRP_GANG1 && groupId != ASSOCGRP_GANG2 &&
	   groupId != ASSOCGRP_FATMAN && groupId != ASSOCGRP_OLDFATMAN &&
	   groupId != ASSOCGRP_JOGGER && groupId != ASSOCGRP_WOMAN &&
	   groupId != ASSOCGRP_SHOPPING && groupId != ASSOCGRP_BUSYWOMAN &&
	   groupId != ASSOCGRP_SEXYWOMAN && groupId != ASSOCGRP_FATWOMAN &&
	   groupId != ASSOCGRP_OLDWOMAN && groupId != ASSOCGRP_JOGWOMAN &&
	   groupId != ASSOCGRP_DRUNKMAN && groupId != ASSOCGRP_BLINDMAN &&
	   groupId != ASSOCGRP_SWAT && groupId != ASSOCGRP_PRO &&
	   groupId != ASSOCGRP_OLDFATWOMAN)
		return nil;
	if(strncasecmp(name, "walk_start", 10) == 0)
		return "walk_start";
	if(strcasecmp(name, "woman_runpanic") == 0 || strncasecmp(name, "sprint", 6) == 0)
		return "sprint_panic";
	if(strncasecmp(name, "walk", 4) == 0 || strncasecmp(name, "JOG_", 4) == 0)
		return "walk_civi";
	if(strncasecmp(name, "run", 3) == 0 || strncasecmp(name, "woman_run", 9) == 0)
		return "run_civi";
	if(strncasecmp(name, "idle", 4) == 0 || strcasecmp(name, "woman_idlestance") == 0)
		return "idle_stance";
	return nil;
}

static CAnimBlendHierarchy *
FindSAAnimationHierarchy(int groupId, const char *name, CAnimBlock *primaryBlock)
{
	const char *sourceBlockName = nil;
	const char *alias = GetSAAnimationAlias(groupId, name, &sourceBlockName);
	CAnimBlock *sourceBlock = sourceBlockName ? CAnimManager::GetAnimationBlock(sourceBlockName) : primaryBlock;
	CAnimBlendHierarchy *hier;

	if(alias && sourceBlock && sourceBlock->isLoaded){
		hier = CAnimManager::GetAnimation(alias, sourceBlock);
		if(hier)
			return hier;
	}
	if(alias && primaryBlock && primaryBlock != sourceBlock && primaryBlock->isLoaded){
		hier = CAnimManager::GetAnimation(alias, primaryBlock);
		if(hier)
			return hier;
	}
	if(primaryBlock && primaryBlock->isLoaded){
		hier = CAnimManager::GetAnimation(name, primaryBlock);
		if(hier)
			return hier;
	}
	if(sourceBlock && sourceBlock != primaryBlock && sourceBlock->isLoaded){
		hier = CAnimManager::GetAnimation(name, sourceBlock);
		if(hier)
			return hier;
	}
	const char *styleFallback = GetSAStyleFallbackName(groupId, name);
	if(styleFallback && primaryBlock && primaryBlock->isLoaded){
		hier = CAnimManager::GetAnimation(styleFallback, primaryBlock);
		if(hier)
			return hier;
	}
	return nil;
}

static bool
HasNewSAAnimation(CAnimBlendAssocGroup *saGroup, int groupId, const AnimAssocDefinition *def, CAnimBlock *primaryBlock)
{
	if(saGroup == nil || saGroup->assocList == nil || def == nil)
		return false;
	int numAssociations = saGroup->numAssociations < def->numAnims ?
		saGroup->numAssociations : def->numAnims;
	for(int i = 0; i < numAssociations; i++)
		if(saGroup->assocList[i].hierarchy == nil &&
		   FindSAAnimationHierarchy(groupId, def->animNames[i], primaryBlock))
			return true;
	return false;
}

#ifdef CUSTOM_MODELS
static void
LoadAnimBankFromCustom(const char *bankName, void *context)
{
	char filename[64];
	snprintf(filename, sizeof(filename), "ANIM\\%s.IFP", bankName);
	CAnimManager::LoadSAAnimFileFromCustom(filename);
}
#endif

// SA keeps the group definitions it does not compile into the game - the
// civilian styles such as woman, shopping and the gangs - in data/animgrp.dat.
// Each block names the group and lists one clip per line until "end"; the
// descriptor flags come from the shared table SA uses for every group.
// The descriptor tables carry the San Andreas flag bits; the association
// keeps the internal ones. Translating here is what lets the tables be used
// verbatim, instead of every reader reinterpreting the bit layout.
static int16
SAFlagsToAssocFlags(int32 saFlags)
{
	int16 flags = 0;
	if(saFlags & ANIMATION_IS_LOOPED)            flags |= ASSOC_REPEAT;
	if(saFlags & ANIMATION_IS_BLEND_AUTO_REMOVE) flags |= ASSOC_DELETEFADEDOUT;
	if(saFlags & ANIMATION_IS_FINISH_AUTO_REMOVE)flags |= ASSOC_FADEOUTWHENDONE;
	if(saFlags & ANIMATION_IS_PARTIAL)           flags |= ASSOC_PARTIAL;
	if(saFlags & ANIMATION_CAN_EXTRACT_VELOCITY) flags |= ASSOC_MOVEMENT | ASSOC_HAS_TRANSLATION;
	if(saFlags & ANIMATION_CAN_EXTRACT_X_VELOCITY)flags |= ASSOC_HAS_X_TRANSLATION;
	if(saFlags & ANIMATION_WALK)                 flags |= ASSOC_WALK;
	if(saFlags & ANIMATION_DONT_ADD_TO_PARTIAL_BLEND) flags |= ASSOC_IDLE;
	if(saFlags & ANIMATION_IS_FRONT)             flags |= ASSOC_FRONTAL;
	return flags;
}

void
CAnimManager::ReadAnimAssociationDefinitions(void)
{
	NameReservedAnimGroups();
#ifdef CUSTOM_MODELS
	std::vector<uint8> bytes;
	if(!CCustomModels::ReadDataFile("data/animgrp.dat", bytes) || bytes.empty())
		return;

	std::string text((const char*)bytes.data(), bytes.size());
	int cur = -1;
	size_t pos = 0;
	while(pos < text.size()){
		size_t eol = text.find('\n', pos);
		if(eol == std::string::npos) eol = text.size();
		std::string raw = text.substr(pos, eol - pos);
		pos = eol + 1;

		size_t first = raw.find_first_not_of(" \t\r");
		if(first == std::string::npos || raw[first] == '#')
			continue;
		std::string line = raw.substr(first);

		if(cur >= 0){
			// A clip line ends the block when it reads "end".
			AnimAssocDefinition *def = &ms_aAnimAssocDefinitions[cur];
			int slot = 0;
			while(slot < def->numAnims && def->animNames[slot] &&
			      def->animNames[slot][0])
				slot++;
			if(strncasecmp(line.c_str(), "end", 3) == 0 || slot >= def->numAnims){
				cur = -1;
				if(strncasecmp(line.c_str(), "end", 3) == 0)
					continue;
			}else{
				snprintf((char*)def->animNames[slot], 24, "%s", line.c_str());
				continue;
			}
		}

		char group[32], block[32], type[32];
		int count = 0;
		if(sscanf(line.c_str(), "%31s %31s %31s %d", group, block, type, &count) != 4)
			continue;
		if(count <= 0)
			continue;
		int slot = -1;
		for(int i = 0; i < NUM_ANIM_ASSOC_GROUPS; i++){
			if(strcasecmp(aAnimGroupNames[i], group) == 0){
				slot = i;
				break;
			}
		}
		if(slot < 0){
			REVC_LOG_WARNING("animgrp %s: no slot for this group\n", group);
			continue;
		}
		AnimAssocDefinition *def = &ms_aAnimAssocDefinitions[slot];
		if(def->numAnims > 0)
			continue; // already defined in code
		// A group cannot name more clips than the shared table describes, and
		// the count has to be settled before anything is allocated from it.
		if(count > (int)ARRAY_SIZE(aStdAnimDescs))
			count = ARRAY_SIZE(aStdAnimDescs);
		def->blockName = strdup(block);
		def->modelIndex = MI_MALE01;
		def->numAnims = count;
		def->numDescs = count;
		def->animNames = (char const**)RwCalloc(count, sizeof(char*));
		def->animDescs = (AnimAssocDesc*)RwCalloc(count, sizeof(AnimAssocDesc));
		if(def->blockName == nil || def->animNames == nil || def->animDescs == nil){
			REVC_LOG_ERROR("animgrp %s: out of memory for %d slots\n", group, count);
			def->numAnims = 0;
			continue;
		}
		// The block lists its clips in id order and the flags are the standard
		// locomotion set, which is what SA passes to every group it reads.
		for(int i = 0; i < count; i++){
			def->animNames[i] = (char const*)RwCalloc(1, 24);
			def->animDescs[i] = aStdAnimDescs[i];
		}
		s_dataDefinedGroup[slot] = true;
		cur = slot;
		CUSTOM_LOG("animgrp %s: %d slots declared in block %s\n", group, count, block);
	}
#endif
}

void
CAnimManager::LoadAnimFiles(void)
{
#ifdef CUSTOM_MODELS
	// The group definitions that live in data/animgrp.dat have to be in place
	// before the banks are named against them.
	ReadAnimAssociationDefinitions();
	// SA splits fighting, weapon, style, car and bike clips across many IFP
	// banks and an SA installation ships all of them in custom/zip/mesh/*.bpc.
	// Load what the archives actually contain: a fixed file list silently
	// drops every bank it does not name.
	CCustomModels::ForEachAnimBank(LoadAnimBankFromCustom, nil);
#endif
	ms_aAnimAssocGroups = new CAnimBlendAssocGroup[NUM_ANIM_ASSOC_GROUPS];
	CreateAnimAssocGroups();
}

void
CAnimManager::CreateAnimAssocGroups(int preferredModelIndex)
{
	if(ms_aAnimAssocGroups == nil)
		return;
	if(preferredModelIndex >= 0 && preferredModelIndex < MODELINFOSIZE){
		CBaseModelInfo *baseInfo = CModelInfo::GetModelInfo(preferredModelIndex);
		if(baseInfo && baseInfo->GetModelType() == MITYPE_PED){
			CPedModelInfo *pedInfo = (CPedModelInfo*)baseInfo;
			int groupId = (int)pedInfo->m_animGroup;
			// A partial SA group may gain aliases when another SA bank loads,
			// so keep rebuilding until it resolves completely.
			if(groupId >= 0 && groupId < NUM_ANIM_ASSOC_GROUPS &&
			   IsAnimAssocGroupReady(&ms_aAnimAssocGroups[groupId]))
				return;
		}
	}

	int i, j;
	bool needClump = false;
	for(i = 0; i < NUM_ANIM_ASSOC_GROUPS; i++){
		const AnimAssocDefinition *def = &ms_aAnimAssocDefinitions[i];
		char blockName[MAX_ANIMBLOCK_NAME];
		CAnimBlock *block;
		// Groups the compiled table and data/animgrp.dat both leave empty have
		// no block to bind to.
		if(def->blockName == nil)
			continue;
		snprintf(blockName, sizeof(blockName), "sa_%s", def->blockName);
		block = GetAnimationBlock(blockName);
		if(block == nil || !block->isLoaded)
			block = GetAnimationBlock("sa_ped");
		if(block && block->isLoaded &&
		   (!IsAnimAssocGroupReady(&ms_aAnimAssocGroups[i]) ||
		    ms_aAnimAssocGroups[i].animBlock != block ||
		    HasNewSAAnimation(&ms_aAnimAssocGroups[i], i, def, block)))
			needClump = true;
	}
	CPedModelInfo *reference = needClump ? FindPedAnimationReference(preferredModelIndex) : nil;
	RpClump *clump = reference ? (RpClump*)reference->CreateInstance() : nil;
#ifdef CUSTOM_MODELS
	CUSTOM_LOG("animation groups: reference %s\n",
		reference ? reference->GetModelName() : "none");
#endif
	if(clump)
		RpAnimBlendClumpInit(clump);

	for(i = 0; i < NUM_ANIM_ASSOC_GROUPS; i++){
		const AnimAssocDefinition *def = &ms_aAnimAssocDefinitions[i];
		CAnimBlendAssocGroup *group = &ms_aAnimAssocGroups[i];
		char blockName[MAX_ANIMBLOCK_NAME];
		CAnimBlock *block;
		CAnimBlendHierarchy *hierarchies[256];

		if(clump == nil)
			break;
		if(def->blockName == nil)
			continue;
		snprintf(blockName, sizeof(blockName), "sa_%s", def->blockName);
		block = GetAnimationBlock(blockName);
		if(block == nil || !block->isLoaded)
			block = GetAnimationBlock("sa_ped");
		if(block == nil || !block->isLoaded || def->numAnims > ARRAY_SIZE(hierarchies))
			continue;
		if(IsAnimAssocGroupReady(group) && group->animBlock == block &&
		   !HasNewSAAnimation(group, i, def, block))
			continue;

		for(j = 0; j < def->numAnims; j++)
			hierarchies[j] = FindSAAnimationHierarchy(i, def->animNames[j], block);
		group->groupId = i;
		group->firstAnimId = def->numDescs > 0 ? def->animDescs[0].animId : ANIM_ID_UNDEFINED;
#ifdef CUSTOM_MODELS
		CUSTOM_LOG("animation groups: building %s from %s (%d slots)\n",
			def->name, block->name, def->numAnims);
#endif
		group->CreateAssociations(block->name, clump, hierarchies, def->numAnims);
		// A group may name more clips than the descriptor table describes, so
		// the flags stop where the descriptors do.
		for(j = 0; j < group->numAssociations && j < def->numDescs; j++){
			CAnimBlendAssociation *assoc = group->GetAnimation(def->animDescs[j].animId);
			if(assoc && assoc->hierarchy)
				assoc->flags |= SAFlagsToAssocFlags(def->animDescs[j].flags);
		}
#ifdef CUSTOM_MODELS
		CUSTOM_LOG("animation groups: flags applied to %s\n", def->name);
#endif
	}

	if(clump){
#ifdef CUSTOM_MODELS
		CUSTOM_LOG("animation groups: cleaning reference clump\n");
#endif
		if(IsClumpSkinned(clump))
			RpClumpForAllAtomics(clump, AtomicRemoveAnimFromSkinCB, nil);
		RpClumpDestroy(clump);
#ifdef CUSTOM_MODELS
		CUSTOM_LOG("animation groups: reference clump destroyed\n");
#endif
	}
#ifdef CUSTOM_MODELS
	CUSTOM_LOG("animation groups: completed\n");
#endif
}

void
CAnimManager::MakeSAAnimBlockName(const char *filename, char *blockName, size_t blockNameSize)
{
	const char *base = filename;
	const char *p;
	const char *dot;
	char stem[MAX_ANIMBLOCK_NAME];
	size_t length, i;

	for(p = filename; *p; p++)
		if(*p == '/' || *p == '\\')
			base = p + 1;
	dot = strrchr(base, '.');
	length = dot ? (size_t)(dot - base) : strlen(base);
	// Leave room for the "sa_" prefix and the terminator: truncating the stem
	// here would hand two different banks the same block name.
	if(length > MAX_ANIMBLOCK_NAME - 4)
		length = MAX_ANIMBLOCK_NAME - 4;
	for(i = 0; i < length; i++)
		stem[i] = (char)tolower((unsigned char)base[i]);
	stem[length] = '\0';
	snprintf(blockName, blockNameSize, "sa_%s", stem);
	if(blockNameSize > 0)
		blockName[blockNameSize - 1] = '\0';
}

static bool
IsCompactAnimFile(const uint8 *data, size_t size)
{
	return size >= 4 && (memcmp(data, "ANP2", 4) == 0 || memcmp(data, "ANP3", 4) == 0);
}

#ifdef CUSTOM_MODELS
// Called for both files read directly by path and streamed IFP blocks. The
// ped animation set is SA only, so a custom ANP2/ANP3 bank is the only source.
void
CAnimManager::LoadSAAnimFileFromCustom(const char *filename)
{
	std::vector<uint8> customBuf;
	if(!CCustomModels::LoadAnimFileFromCustom(filename, customBuf))
		return;
#ifdef ANDROID
	if(!IsCompactAnimFile(customBuf.data(), customBuf.size()) &&
	   !(customBuf.size() >= 4 && memcmp(customBuf.data(), "ANPK", 4) == 0)){
		CUSTOM_LOG("animation %s: unrecognized custom anim header; bank skipped\n", filename);
		return;
	}
#else
	if(!IsCompactAnimFile(customBuf.data(), customBuf.size())){
		CUSTOM_LOG("animation %s: unrecognized SA animation header; bank skipped\n", filename);
		return;
	}
#endif

	int numNames = 0;
	bool validSniff = br::SniffAnimNames(customBuf.data(), customBuf.size(), nil,
		NUMANIMATIONS, numNames) && numNames > 0;
#ifdef ANDROID
	if(!validSniff && br::convertAniToIfp(customBuf)){
		numNames = 0;
		validSniff = br::SniffAnimNames(customBuf.data(), customBuf.size(), nil,
			NUMANIMATIONS, numNames) && numNames > 0;
	}
#endif
	if(!validSniff){
		CUSTOM_LOG("animation %s: invalid SA IFP structure; bank skipped\n", filename);
		return;
	}
	char blockName[MAX_ANIMBLOCK_NAME];
	CAnimManager::MakeSAAnimBlockName(filename, blockName, sizeof(blockName));
	CAnimBlock *loadedBlock = GetAnimationBlock(blockName);
	if(loadedBlock && loadedBlock->isLoaded){
		loadedBlock->unloadPending = false;
		return;
	}
	if((loadedBlock == nil && ms_numAnimBlocks >= NUMANIMBLOCKS) ||
	   ((!loadedBlock || loadedBlock->numAnims != numNames) &&
	    numNames > NUMANIMATIONS - ms_numAnimations)){
		CUSTOM_LOG("animation %s: no room for the SA dictionary (%d clips)\n",
			filename, numNames);
		return;
	}
	RwMemory mem;
	mem.start = customBuf.data();
	mem.length = (uint32)customBuf.size();
	RwStream *stream = RwStreamOpen(rwSTREAMMEMORY, rwSTREAMREAD, &mem);
	if(stream){
		LoadAnimFile(stream, true, nil, blockName);
		RwStreamClose(stream, nil);
		loadedBlock = GetAnimationBlock(blockName);
	}
	CUSTOM_LOG("animation %s: %s in bank %s\n",
		filename, loadedBlock && loadedBlock->isLoaded ? "loaded" : "FAILED to load",
		blockName);
}
#endif

void
CAnimManager::LoadAnimFile(const char *filename)
{
	// No VC animations are loaded any more; the file is only read through the
	// custom SA bank loader.
#ifdef CUSTOM_MODELS
	LoadSAAnimFileFromCustom(filename);
#else
	REVC_LOG_ERROR("animation %s: the ped animation set requires custom SA banks", filename);
#endif
}

void
CAnimManager::LoadAnimFile(RwStream *stream, bool compress, char (*uncompressedAnims)[32], const char *blockNameOverride)
{
	#define ROUNDSIZE(x) if((x) & 3) (x) += 4 - ((x)&3)
	struct IfpHeader {
		char ident[4];
		uint32 size;
	};
	IfpHeader anpk, info, name, dgan, cpan, anim;
	char buf[256];
	int j, k, l;
	float *fbuf = (float*)buf;

	// block name
	RwStreamRead(stream, &anpk, sizeof(IfpHeader));
	// San Andreas and Black Russia packages: compact header without the
	// ANPK chunk layers. The first word is either the PC root length or the
	// first four characters of the mobile block name, not a reliable size.
	if(memcmp(anpk.ident, "ANP3", 4) == 0 || memcmp(anpk.ident, "ANP2", 4) == 0){
		uint8 header[32];
		memcpy(header, &anpk.size, 4);
		if(RwStreamRead(stream, header + 4, 28) != 28){
			REVC_LOG_WARNING("ANP%c: truncated animation block header", anpk.ident[3]);
			return;
		}
		animfile::ANP23Header parsed;
		// Custom files have already been checked against their actual length by
		// SniffAnimNames. Other stream types need only the header's count/name;
		// don't mistake an ignored mobile groupVal of 0 or 1 for that count.
		if(!animfile::ParseANP23Header(header, sizeof(header), 0, parsed)){
			REVC_LOG_WARNING("ANP%c: invalid animation block header", anpk.ident[3]);
			return;
		}
		LoadAnimFile_ANP23(stream, anpk.ident, compress, parsed.blockName, parsed.numAnims, blockNameOverride);
		return;
	}
	ROUNDSIZE(anpk.size);
	RwStreamRead(stream, &info, sizeof(IfpHeader));
	ROUNDSIZE(info.size);
	RwStreamRead(stream, buf, info.size);
	const char *targetBlockName = (blockNameOverride && blockNameOverride[0] != '\0') ? blockNameOverride : (buf + 4);
	CAnimBlock *animBlock = GetAnimationBlock(targetBlockName);
	if(animBlock){
		// A replacement may have a different number of clips from the old
		// dictionary. Reuse its slots only when they have the same capacity.
		if(animBlock->numAnims != *(int*)buf){
			animBlock->numAnims = *(int*)buf;
			animBlock->firstIndex = ms_numAnimations;
		}
	}else{
		// The block table is fixed size and ReadANPK may name a new block for
		// every stream it sees.
		if(ms_numAnimBlocks >= NUMANIMBLOCKS){
			REVC_LOG_WARNING("ANIMS %s: no free block slot, block skipped", targetBlockName);
			return;
		}
		animBlock = &ms_aAnimBlocks[ms_numAnimBlocks++];
		strncpy(animBlock->name, targetBlockName, MAX_ANIMBLOCK_NAME);
		animBlock->name[MAX_ANIMBLOCK_NAME - 1] = '\0';
		animBlock->refCount = 0;
		animBlock->numAnims = *(int*)buf;
		animBlock->firstIndex = ms_numAnimations;
	}
	animBlock->unloadPending = false;

	debug("Loading ANIMS %s\n", animBlock->name);
	animBlock->isLoaded = true;

	// The clip count comes from the file and is used to index the animation
	// store, which is a fixed array; a block that does not fit is skipped here
	// rather than tripping the assert inside the loop.
	if(animBlock->numAnims <= 0 || animBlock->numAnims > NUMANIMATIONS ||
	   animBlock->firstIndex < 0 ||
	   animBlock->numAnims > NUMANIMATIONS - animBlock->firstIndex){
		REVC_LOG_WARNING("ANIMS %s: %d clips from index %d do not fit the animation store, block skipped",
			animBlock->name, animBlock->numAnims, animBlock->firstIndex);
		animBlock->numAnims = 0;
		animBlock->isLoaded = false;
		return;
	}

	int animIndex = animBlock->firstIndex;
	for(j = 0; j < animBlock->numAnims; j++){
		assert(animIndex < ARRAY_SIZE(ms_aAnimations));
		CAnimBlendHierarchy *hier = &ms_aAnimations[animIndex++];

		// animation name
		RwStreamRead(stream, &name, sizeof(IfpHeader));
		ROUNDSIZE(name.size);
		RwStreamRead(stream, buf, name.size);
		hier->SetName(buf);

#ifdef ANIM_COMPRESSION
		bool compressHier = compress;
#else
		bool compressHier = false;
#endif
		if (uncompressedAnims) {
			for (int i = 0; uncompressedAnims[i][0]; i++) {
				if (!CGeneral::faststricmp(uncompressedAnims[i], hier->name)){
					debug("Loading %s uncompressed\n", hier->name);
					compressHier = false;
				}
			}
		}

		hier->compressed = compressHier;
		hier->keepCompressed = false;

		// DG info has number of nodes/sequences
		RwStreamRead(stream, (char*)&dgan, sizeof(IfpHeader));
		ROUNDSIZE(dgan.size);
		RwStreamRead(stream, (char*)&info, sizeof(IfpHeader));
		ROUNDSIZE(info.size);
		RwStreamRead(stream, buf, info.size);
		hier->numSequences = *(int*)buf;
		hier->sequences = new CAnimBlendSequence[hier->numSequences];

		CAnimBlendSequence *seq = hier->sequences;
		for(k = 0; k < hier->numSequences; k++, seq++){
			// Each node has a name and key frames
			RwStreamRead(stream, &cpan, sizeof(IfpHeader));
			ROUNDSIZE(dgan.size);
			RwStreamRead(stream, &anim, sizeof(IfpHeader));
			ROUNDSIZE(anim.size);
			RwStreamRead(stream, buf, anim.size);
			int numFrames = *(int*)(buf+28);
			seq->SetName(buf);
			if(anim.size == 44)
				seq->SetBoneTag(*(int*)(buf+40));
			if(numFrames == 0)
				continue;

			bool hasScale = false;
			bool hasTranslation = false;
			RwStreamRead(stream, &info, sizeof(info));
			if(strncmp(info.ident, "KRTS", 4) == 0){
				hasScale = true;
				seq->SetNumFrames(numFrames, true, compressHier);
			}else if(strncmp(info.ident, "KRT0", 4) == 0){
				hasTranslation = true;
				seq->SetNumFrames(numFrames, true, compressHier);
			}else if(strncmp(info.ident, "KR00", 4) == 0){
				seq->SetNumFrames(numFrames, false, compressHier);
			}
			if(strstr(seq->name, "L Toe"))
				debug("anim %s has toe keyframes\n", hier->name); // BUG: seq->name

			for(l = 0; l < numFrames; l++){
				if(hasScale){
					RwStreamRead(stream, buf, 0x2C);
					CQuaternion rot(fbuf[0], fbuf[1], fbuf[2], fbuf[3]);
					rot.Invert();
					CVector trans(fbuf[4], fbuf[5], fbuf[6]);

					if(compressHier){
						KeyFrameTransCompressed *kf = (KeyFrameTransCompressed*)seq->GetKeyFrameCompressed(l);
						kf->SetRotation(rot);
						kf->SetTranslation(trans);
						// scaling ignored
						kf->SetTime(fbuf[10]);	// absolute time here
					}else{
						KeyFrameTrans *kf = (KeyFrameTrans*)seq->GetKeyFrame(l);
						kf->rotation = rot;
						kf->translation = trans;
						// scaling ignored
						kf->deltaTime = fbuf[10];	// absolute time here
					}
				}else if(hasTranslation){
					RwStreamRead(stream, buf, 0x20);
					CQuaternion rot(fbuf[0], fbuf[1], fbuf[2], fbuf[3]);
					rot.Invert();
					CVector trans(fbuf[4], fbuf[5], fbuf[6]);

					if(compressHier){
						KeyFrameTransCompressed *kf = (KeyFrameTransCompressed*)seq->GetKeyFrameCompressed(l);
						kf->SetRotation(rot);
						kf->SetTranslation(trans);
						kf->SetTime(fbuf[7]);	// absolute time here
					}else{
						KeyFrameTrans *kf = (KeyFrameTrans*)seq->GetKeyFrame(l);
						kf->rotation = rot;
						kf->translation = trans;
						kf->deltaTime = fbuf[7];	// absolute time here
					}
				}else{
					RwStreamRead(stream, buf, 0x14);
					CQuaternion rot(fbuf[0], fbuf[1], fbuf[2], fbuf[3]);
					rot.Invert();

					if(compressHier){
						KeyFrameCompressed *kf = (KeyFrameCompressed*)seq->GetKeyFrameCompressed(l);
						kf->SetRotation(rot);
						kf->SetTime(fbuf[4]);	// absolute time here
					}else{
						KeyFrame *kf = (KeyFrame*)seq->GetKeyFrame(l);
						kf->rotation = rot;
						kf->deltaTime = fbuf[4];	// absolute time here
					}
				}
			}
		}

		if(!compressHier){
			hier->RemoveQuaternionFlips();
			hier->CalcTotalTime();
		}
	}
	if(animIndex > ms_numAnimations)
		ms_numAnimations = animIndex;
}

// ANP2/ANP3: one blockName[24] + numAnims, then animations without the ANPK
// chunk headers. Key frames are the same byte layouts this game stores
// (KeyFrame 0x14, KeyFrameTrans 0x20, KeyFrameCompressed 0xA,
// KeyFrameTransCompressed 0x10 - identical in San Andreas, verified against
// gta-reversed AnimSequenceFrames.h). Unlike ANPK, ANP23's float quaternion
// needs no inversion. Compressed frames (frameType 3/4) are read into the
// compressed arrays and expanded when the build does not retain compression.
void
CAnimManager::LoadAnimFile_ANP23(RwStream *stream, const char *ident, bool compress,
	const char *declaredName, uint32 numAnims, const char *blockNameOverride)
{
	char buf[256];
	float *fbuf = (float*)buf;
	int j, k, l;
	bool isANP3 = ident[3] == '3';
	char blockName[24];
	strncpy(blockName, blockNameOverride ? blockNameOverride : declaredName, sizeof(blockName) - 1);
	blockName[sizeof(blockName) - 1] = '\0';

	if(numAnims == 0 || numAnims > (uint32)NUMANIMATIONS){
		debug("ANP%c: invalid animation count for block %s (%u)\n", ident[3], blockName, numAnims);
		return;
	}
	CAnimBlock *animBlock = GetAnimationBlock(blockName);
	if(animBlock && (animBlock->numAnims < 0 ||
	   (animBlock->numAnims > 0 &&
	    (animBlock->firstIndex < 0 || animBlock->firstIndex > NUMANIMATIONS ||
	     animBlock->numAnims > NUMANIMATIONS - animBlock->firstIndex ||
	     animBlock->numAnims != (int32)numAnims)))){
		debug("ANP%c: block %s has an invalid animation range or count (%d at %d, file has %u)\n",
			ident[3], blockName, animBlock->numAnims, animBlock->firstIndex, numAnims);
		return;
	}
	if((animBlock == nil || animBlock->numAnims == 0) &&
	   ms_numAnimations + (int32)numAnims > NUMANIMATIONS){
		debug("ANP%c: no room for block %s (%u anims)\n", ident[3], blockName, numAnims);
		return;
	}
	if(animBlock){
		if(animBlock->numAnims == 0){
			animBlock->numAnims = numAnims;
			animBlock->firstIndex = ms_numAnimations;
		}
	}else{
		if(ms_numAnimBlocks >= NUMANIMBLOCKS){
			debug("ANP%c: no room for block %s (%u anims)\n", ident[3], blockName, numAnims);
			return;
		}
		animBlock = &ms_aAnimBlocks[ms_numAnimBlocks++];
		strncpy(animBlock->name, blockName, MAX_ANIMBLOCK_NAME - 1);
		animBlock->name[MAX_ANIMBLOCK_NAME - 1] = '\0';
		animBlock->isLoaded = false;
		animBlock->refCount = 0;
		animBlock->unloadPending = false;
		animBlock->numAnims = numAnims;
		animBlock->firstIndex = ms_numAnimations;
	}
	debug("Loading ANIMS %s (ANP%c)\n", animBlock->name, ident[3]);
	animBlock->isLoaded = true;
	animBlock->unloadPending = false;

	int animIndex = animBlock->firstIndex;
	for(j = 0; j < (int)numAnims; j++){
		if(animIndex >= NUMANIMATIONS){
			debug("ANP%c: animation table full\n", ident[3]);
			return;
		}
		CAnimBlendHierarchy *hier = &ms_aAnimations[animIndex++];

		char aname[24];
		RwStreamRead(stream, aname, sizeof(aname));
		aname[23] = '\0';
		hier->SetName(aname);
#ifdef CUSTOM_MODELS
		CUSTOM_LOG("ANP%c %s: clip %d is %s\n", ident[3], animBlock->name, j, aname);
#endif

		uint32 numSeq;
		RwStreamRead(stream, &numSeq, sizeof(numSeq));

		if(isANP3){
			// Size of frame data and an exporter/version word (often 1). Neither
			// indicates whether frames are compressed: frameType on each bone does.
			uint32 framesSize, exporterWord;
			RwStreamRead(stream, &framesSize, sizeof(framesSize));
			RwStreamRead(stream, &exporterWord, sizeof(exporterWord));
			(void)framesSize;
			(void)exporterWord;
		}

		bool compressHier = false;
#ifdef ANIM_COMPRESSION
		compressHier = compress;
#endif
		bool hasCompressedFrames = false;
		hier->compressed = false;
		hier->keepCompressed = false;

		if(numSeq > 0x1000){	// garbage header, do not allocate gigabytes
			debug("ANP%c: %s has an impossible sequence count\n", ident[3], hier->name);
			return;
		}
		if(numSeq == 0){
			hier->numSequences = 0;
			hier->sequences = nil;
			hier->totalLength = 0.0f;
			continue;
		}
		hier->numSequences = numSeq;
		hier->sequences = new CAnimBlendSequence[numSeq];
		CAnimBlendSequence *seq = hier->sequences;
		for(k = 0; k < (int)numSeq; k++, seq++){
			char seqName[24];
			RwStreamRead(stream, seqName, sizeof(seqName));
			seqName[23] = '\0';
			uint32 frameType, numFrames;
			int32 boneTag;
			RwStreamRead(stream, &frameType, sizeof(frameType));
			RwStreamRead(stream, &numFrames, sizeof(numFrames));
			RwStreamRead(stream, &boneTag, sizeof(boneTag));
			seq->SetName(seqName);
			seq->SetBoneTag(boneTag);
			if(numFrames == 0)
				continue;
			if(numFrames > 0xFFFF){
				debug("ANP%c: %s/%s has an impossible frame count (%d)\n",
					ident[3], hier->name, seq->name, numFrames);
				return;
			}
			if(frameType < 1 || frameType > 4){
				debug("ANP%c: unknown frame type %d in %s\n", ident[3], frameType, hier->name);
				return;	// the stream is positioned wrong from here on
			}
			bool hasTrans = frameType == 2 || frameType == 4;
			if(frameType == 3 || frameType == 4){
				// pre-compressed frames: the file layout is the struct layout
				seq->SetNumFrames(numFrames, hasTrans, true);
				uint8 *dst = (uint8*)seq->GetKeyFrameCompressed(0);
				size_t stride = hasTrans ? 16 : 10;
				RwStreamRead(stream, dst, (uint32)(stride * numFrames));
				hasCompressedFrames = true;
#ifndef ANIM_COMPRESSION
				// The game normally builds without animation-cache compression.
				// Expand SA's frames now so normal update code works in every build.
				seq->Uncompress();
#endif
				continue;
			}
			// ANP2/ANP3 store the quaternion as used by the skeleton. Only
			// ANPK inverts the x/y/z components on read.
			seq->SetNumFrames(numFrames, hasTrans, compressHier);
			if(compressHier)
				hasCompressedFrames = true;
			for(l = 0; l < (int)numFrames; l++){
				if(hasTrans){
					RwStreamRead(stream, buf, 0x20);
					CQuaternion rot(fbuf[0], fbuf[1], fbuf[2], fbuf[3]);
					CVector trans(fbuf[4], fbuf[5], fbuf[6]);
					if(compressHier){
						KeyFrameTransCompressed *kf = (KeyFrameTransCompressed*)seq->GetKeyFrameCompressed(l);
						kf->SetRotation(rot);
						kf->SetTranslation(trans);
						kf->SetTime(fbuf[7]);
					}else{
						KeyFrameTrans *kf = (KeyFrameTrans*)seq->GetKeyFrame(l);
						kf->rotation = rot;
						kf->translation = trans;
						kf->deltaTime = fbuf[7];
					}
				}else{
					RwStreamRead(stream, buf, 0x14);
					CQuaternion rot(fbuf[0], fbuf[1], fbuf[2], fbuf[3]);
					if(compressHier){
						KeyFrameCompressed *kf = (KeyFrameCompressed*)seq->GetKeyFrameCompressed(l);
						kf->SetRotation(rot);
						kf->SetTime(fbuf[4]);
					}else{
						KeyFrame *kf = (KeyFrame*)seq->GetKeyFrame(l);
						kf->rotation = rot;
						kf->deltaTime = fbuf[4];
					}
				}
			}
		}
#ifdef ANIM_COMPRESSION
		// If the caller requested uncompressed animations, expand every
		// frameType 3/4 sequence as well. The ANP3 exporter word is not a
		// compression flag, and a dictionary may mix float and i16 frames.
		if(!compressHier && hasCompressedFrames){
			for(int s = 0; s < hier->numSequences; s++)
				hier->sequences[s].Uncompress();
			hasCompressedFrames = false;
		}
		hier->compressed = hasCompressedFrames;
#else
		hier->compressed = false;
#endif
		// Quaternion signs must be aligned before the times are turned into
		// interval deltas, otherwise the interpolator takes the long way round.
		// Both steps run for whichever array is live: a compressed dictionary
		// used to skip them entirely and ended up with a zero length.
		hier->RemoveQuaternionFlips();
		if(hier->compressed)
			hier->CalcTotalTimeCompressed();
		else
			hier->CalcTotalTime();
	}
	if(animIndex > ms_numAnimations)
		ms_numAnimations = animIndex;
}

void
CAnimManager::RemoveLastAnimFile(void)
{
	int i;
	ms_numAnimBlocks--;
	ms_numAnimations = ms_aAnimBlocks[ms_numAnimBlocks].firstIndex;
	for(i = 0; i < ms_aAnimBlocks[ms_numAnimBlocks].numAnims; i++)
		ms_aAnimations[ms_aAnimBlocks[ms_numAnimBlocks].firstIndex + i].Shutdown();
	ms_aAnimBlocks[ms_numAnimBlocks].isLoaded = false;
}
