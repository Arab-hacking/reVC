#ifndef __GTA_ANIMMANAGER_H__
#define __GTA_ANIMMANAGER_H__

#include "AnimBlendHierarchy.h"
#include "AnimationId.h"
#include "AnimAssocTables.h"

enum AssocGroupId
{
	ASSOCGRP_DEFAULT = 0,
	ASSOCGRP_DOOR = 1,
	ASSOCGRP_BIKES = 2,
	ASSOCGRP_BIKEV = 3,
	ASSOCGRP_BIKEH = 4,
	ASSOCGRP_BIKED = 5,
	ASSOCGRP_WAYFARER = 6,
	ASSOCGRP_BMX = 7,
	ASSOCGRP_MTB = 8,
	ASSOCGRP_CHOPPA = 9,
	ASSOCGRP_QUAD = 10,
	ASSOCGRP_PYTHON = 11,
	ASSOCGRP_PYTHONBAD = 12,
	ASSOCGRP_COLT45 = 13,
	ASSOCGRP_COLT_COP = 14,
	ASSOCGRP_COLT45PRO = 15,
	ASSOCGRP_SAWNOFF = 16,
	ASSOCGRP_SAWNOFFPRO = 17,
	ASSOCGRP_SILENCED = 18,
	ASSOCGRP_SHOTGUN = 19,
	ASSOCGRP_SHOTGUNBAD = 20,
	ASSOCGRP_BUDDY = 21,
	ASSOCGRP_BUDDYBAD = 22,
	ASSOCGRP_UZI = 23,
	ASSOCGRP_UZIBAD = 24,
	ASSOCGRP_RIFLE = 25,
	ASSOCGRP_RIFLEBAD = 26,
	ASSOCGRP_SNIPER = 27,
	ASSOCGRP_GRENADE = 28,
	ASSOCGRP_FLAME = 29,
	ASSOCGRP_ROCKET = 30,
	ASSOCGRP_SPRAYCAN = 31,
	ASSOCGRP_GOGGLES = 32,
	ASSOCGRP_MELEE_1 = 33,
	ASSOCGRP_MELEE_2 = 34,
	ASSOCGRP_MELEE_3 = 35,
	ASSOCGRP_MELEE_4 = 36,
	ASSOCGRP_BBBAT_1 = 37,
	ASSOCGRP_GCLUB_1 = 38,
	ASSOCGRP_KNIFE_1 = 39,
	ASSOCGRP_SWORD_1 = 40,
	ASSOCGRP_DILDO_1 = 41,
	ASSOCGRP_FLOWERS_1 = 42,
	ASSOCGRP_CSAW_1 = 43,
	ASSOCGRP_KICK_STD = 44,
	ASSOCGRP_PISTLWHP = 45,
	ASSOCGRP_MEDIC = 46,
	ASSOCGRP_BEACH = 47,
	ASSOCGRP_SUNBATHE = 48,
	ASSOCGRP_PLAYIDLES = 49,
	ASSOCGRP_RIOT = 50,
	ASSOCGRP_STRIP = 51,
	ASSOCGRP_GANGS = 52,
	ASSOCGRP_ATTRACTORS = 53,
	ASSOCGRP_PLAYER = 54,
	ASSOCGRP_FATMAN = 55,
	ASSOCGRP_MUSCULAR = 56,
	ASSOCGRP_PLAYERROCKET = 57,
	ASSOCGRP_PLAYERROCKETF = 58,
	ASSOCGRP_PLAYERROCKETM = 59,
	ASSOCGRP_PLAYER2ARMED = 60,
	ASSOCGRP_PLAYER2ARMEDF = 61,
	ASSOCGRP_PLAYER2ARMEDM = 62,
	ASSOCGRP_PLAYERBBBAT = 63,
	ASSOCGRP_PLAYERBBBATF = 64,
	ASSOCGRP_PLAYERBBBATM = 65,
	ASSOCGRP_PLAYERCSAW = 66,
	ASSOCGRP_PLAYERCSAWF = 67,
	ASSOCGRP_PLAYERCSAWM = 68,
	ASSOCGRP_PLAYERSNEAK = 69,
	ASSOCGRP_PLAYERJETPACK = 70,
	ASSOCGRP_SWIM = 71,
	ASSOCGRP_DRIVEBYS = 72,
	ASSOCGRP_BIKE_DBZ = 73,
	ASSOCGRP_COP_DBZ = 74,
	ASSOCGRP_QUAD_DBZ = 75,
	ASSOCGRP_FAT_TIRED = 76,
	ASSOCGRP_HANDSIGNAL = 77,
	ASSOCGRP_HANDSIGNALL = 78,
	ASSOCGRP_LHAND = 79,
	ASSOCGRP_RHAND = 80,
	ASSOCGRP_CARRY = 81,
	ASSOCGRP_CARRY05 = 82,
	ASSOCGRP_CARRY105 = 83,
	ASSOCGRP_INT_HOUSE = 84,
	ASSOCGRP_INT_OFFICE = 85,
	ASSOCGRP_INT_SHOP = 86,
	ASSOCGRP_STEALTH_KN = 87,
	ASSOCGRP_LOWCARAMIMS = 89,
	ASSOCGRP_STDCARANIMS = 88,
	ASSOCGRP_LOWCARANIMS = 89,
	ASSOCGRP_TRKCARANIMS = 90,
	ASSOCGRP_STDBIKEANIMS = 91,
	ASSOCGRP_SPORTBIKEANIMS = 92,
	ASSOCGRP_VESPABIKEANIMS = 93,
	ASSOCGRP_HARLEYBIKEANIMS = 94,
	ASSOCGRP_DIRTBIKEANIMS = 95,
	ASSOCGRP_WAYFBIKEANIMS = 96,
	ASSOCGRP_BMXBIKEANIMS = 97,
	ASSOCGRP_MTBBIKEANIMS = 98,
	ASSOCGRP_CHOPPABIKEANIMS = 99,
	ASSOCGRP_QUADBIKEANIMS = 100,
	ASSOCGRP_VANCARANIMS = 101,
	ASSOCGRP_RUSTPLANEANIMS = 102,
	ASSOCGRP_COACHCARANIMS = 103,
	ASSOCGRP_BUSCARANIMS = 104,
	ASSOCGRP_DOZERCARANIMS = 105,
	ASSOCGRP_KARTCARANIMS = 106,
	ASSOCGRP_CONVCARANIMS = 107,
	ASSOCGRP_MTRKCARANIMS = 108,
	ASSOCGRP_TRAINCARRANIMS = 109,
	ASSOCGRP_STDTALLCARAMIMS = 110,
	ASSOCGRP_HOVERCARANIMS = 111,
	ASSOCGRP_TANKCARANIMS = 112,
	ASSOCGRP_BFINJCARAMIMS = 113,
	ASSOCGRP_LEARPLANEANIMS = 114,
	ASSOCGRP_HARRPLANEANIMS = 115,
	ASSOCGRP_STDCARUPRIGHT = 116,
	ASSOCGRP_NVADAPLANEANIMS = 117,
	ASSOCGRP_SHUFFLE = 118,
	ASSOCGRP_OLDMAN = 119,
	ASSOCGRP_GANG1 = 120,
	ASSOCGRP_GANG2 = 121,
	ASSOCGRP_OLDFATMAN = 122,
	ASSOCGRP_JOGGER = 123,
	ASSOCGRP_DRUNKMAN = 124,
	ASSOCGRP_BLINDMAN = 125,
	ASSOCGRP_SWAT = 126,
	ASSOCGRP_WOMAN = 127,
	ASSOCGRP_SHOPPING = 128,
	ASSOCGRP_BUSYWOMAN = 129,
	ASSOCGRP_SEXYWOMAN = 130,
	ASSOCGRP_PRO = 131,
	ASSOCGRP_OLDWOMAN = 132,
	ASSOCGRP_FATWOMAN = 133,
	ASSOCGRP_JOGWOMAN = 134,
	ASSOCGRP_OLDFATWOMAN = 135,
	ASSOCGRP_SKATE = 136,
	ASSOCGRP_PANICCHUNKY = 137,
	ASSOCGRP_UNARMED = 138,


	NUM_ANIM_ASSOC_GROUPS
};

class CAnimBlendAssociation;
class CAnimBlendAssocGroup;

// SA names its dictionaries after the IFP that supplied them, and the banks
// run well past the VC 20-byte limit: sa_sportbikeanims is 18 on its own.
#define MAX_ANIMBLOCK_NAME 40

// A block of hierarchies
struct CAnimBlock
{
	char name[MAX_ANIMBLOCK_NAME];
	bool isLoaded;
	int16 refCount;
	int32 firstIndex;	// first animtion in ms_aAnimations
	int32 numAnims;
	bool unloadPending;	// parallel SA bank waits for active associations to release it
};

struct AnimAssocDefinition
{
	char const *name;
	char const *blockName;
	int32 modelIndex;
	int32 numAnims;
	// A few San Andreas groups list more clip names than descriptors: the
	// extra slots take the flags of their neighbour but have no id of their
	// own, so the descriptor count is tracked separately.
	int32 numDescs;
	char const **animNames;
	AnimAssocDesc *animDescs;
};

class CAnimManager
{
	// The compiled table covers the groups SA ships in code. Further groups
	// come from data/animgrp.dat at startup, so the array is filled in place.
	static AnimAssocDefinition ms_aAnimAssocDefinitions[NUM_ANIM_ASSOC_GROUPS];
	static int32 ms_numAnimAssocDefinitions;
	static CAnimBlock ms_aAnimBlocks[NUMANIMBLOCKS];
	static CAnimBlendHierarchy ms_aAnimations[NUMANIMATIONS];
	static int32 ms_numAnimBlocks;
	static int32 ms_numAnimations;
	static CAnimBlendAssocGroup *ms_aAnimAssocGroups;
	static CLinkList<CAnimBlendHierarchy*> ms_animCache;
	static CAnimBlendAssocGroup *GetAnimAssocGroup(RpClump *clump, AssocGroupId groupId);
public:

	static void Initialise(void);
	static void Shutdown(void);
	static void UncompressAnimation(CAnimBlendHierarchy *anim);
	static void RemoveFromUncompressedCache(CAnimBlendHierarchy *hier);
	static CAnimBlock *GetAnimationBlock(int32 block) { return &ms_aAnimBlocks[block]; }
	static CAnimBlock *GetAnimationBlock(const char *name);
	static int32 GetAnimationBlockIndex(const char *name);
	static int32 GetAnimationBlockForHierarchy(CAnimBlendHierarchy *hierarchy);
	static int32 RegisterAnimBlock(const char *name);
	static int32 GetNumRefsToAnimBlock(int32 block);
	static void AddAnimBlockRef(int32 block);
	static void RemoveAnimBlockRefWithoutDelete(int32 block);
	static void RemoveAnimBlockRef(int32 block);
	static void RemoveAnimBlock(int32 block);
	static CAnimBlendHierarchy *GetAnimation(const char *name, CAnimBlock *animBlock);
	static CAnimBlendHierarchy *GetAnimation(int32 n) { return n >= 0 && n < NUMANIMATIONS ? &ms_aAnimations[n] : nil; }
	// Only hierarchies from an SA dictionary can drive a ped. A null clump
	// skips the skeleton checks and only validates the bank.
	static bool IsAnimationCompatibleWithClump(RpClump *clump, CAnimBlendHierarchy *hierarchy);
	// Maps a file name or dictionary name to its "sa_" block name.
	static void MakeSAAnimBlockName(const char *filename, char *blockName, size_t blockNameSize);
	// Vehicle approach offsets must come from the ped's own SA bank. The
	// lookup also works without a reference clump.
	static CAnimBlendHierarchy *GetAnimHierarchyForOffset(AssocGroupId groupId, AnimationId animId);
	static CAnimBlendHierarchy *GetAnimHierarchyForOffset(RpClump *clump, AssocGroupId groupId, AnimationId animId);
	static const char *GetAnimGroupName(AssocGroupId groupId);
	// Expected clip name for a group slot, even when its IFP is absent.
	static const char *GetAnimName(AssocGroupId groupId, AnimationId animId);
	static CAnimBlendAssociation *CreateAnimAssociation(AssocGroupId groupId, AnimationId animId);
	static CAnimBlendAssociation *CreateAnimAssociation(RpClump *clump, AssocGroupId groupId, AnimationId animId);
	// The group that owns a clip. A clip id is only meaningful inside its own
	// group, so callers that hold an id and no group ask here.
	static AssocGroupId GetAnimGroupForAnimation(AnimationId animId);
	static CAnimBlendAssociation *GetAnimAssociation(AssocGroupId groupId, AnimationId animId);
	static CAnimBlendAssociation *GetAnimAssociation(AssocGroupId groupId, const char *name);
	static CAnimBlendAssociation *GetAnimAssociation(RpClump *clump, AssocGroupId groupId, AnimationId animId);
	static CAnimBlendAssociation *GetAnimAssociation(RpClump *clump, AssocGroupId groupId, const char *name);
	static CAnimBlendAssociation *AddAnimation(RpClump *clump, AssocGroupId groupId, AnimationId animId);
	static CAnimBlendAssociation *AddAnimationAndSync(RpClump *clump, CAnimBlendAssociation *syncanim, AssocGroupId groupId, AnimationId animId);
	static CAnimBlendAssociation *BlendAnimation(RpClump *clump, AssocGroupId groupId, AnimationId animId, float delta);
	static void LoadAnimFiles(void);
	static void LoadAnimFile(const char *filename);
	static void LoadAnimFile(RwStream *stream, bool compress, char (*uncompressedAnims)[32] = nil, const char *blockNameOverride = nil);
#ifdef CUSTOM_MODELS
	// Only compact SA/BR ANP2/ANP3 files from custom enter the parallel sa_*
	// bank. ANPK (including a custom VC replacement) never replaces native VC.
	static void LoadSAAnimFileFromCustom(const char *filename);
#endif
	// SA/BR animation packages with the compact header (ANP2/ANP3), as found
	// in San Andreas ifp files and Black Russia .ani containers
	static void LoadAnimFile_ANP23(RwStream *stream, const char *ident, bool compress,
		const char *blockName, uint32 numAnims, const char *blockNameOverride = nil);
	static void CreateAnimAssocGroups(int preferredModelIndex = -1);
public:
	// Called before the level files, which name ped groups by string.
	static void ReadAnimAssociationDefinitions(void);
private:
	static void NameReservedAnimGroups(void);
public:
	static void RemoveLastAnimFile(void);
	static CAnimBlendAssocGroup* GetAnimAssocGroups(void) { return ms_aAnimAssocGroups; }
};

#endif // __GTA_ANIMMANAGER_H__
