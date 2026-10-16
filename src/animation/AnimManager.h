#ifndef __GTA_ANIMMANAGER_H__
#define __GTA_ANIMMANAGER_H__

#include "AnimBlendHierarchy.h"
#include "AnimationId.h"

enum AssocGroupId
{
	ASSOCGRP_STD,
	ASSOCGRP_VAN,
	ASSOCGRP_COACH,
	ASSOCGRP_BIKE_STANDARD,
	ASSOCGRP_BIKE_VESPA,
	ASSOCGRP_BIKE_HARLEY,
	ASSOCGRP_BIKE_DIRT,
	ASSOCGRP_UNARMED,
	ASSOCGRP_SCREWDRIVER,
	ASSOCGRP_KNIFE,
	ASSOCGRP_BASEBALLBAT,
	ASSOCGRP_GOLFCLUB,
	ASSOCGRP_CHAINSAW,
	ASSOCGRP_PYTHON,
	ASSOCGRP_COLT,
	ASSOCGRP_SHOTGUN,
	ASSOCGRP_BUDDY,
	ASSOCGRP_TEC,
	ASSOCGRP_UZI,
	ASSOCGRP_RIFLE,
	ASSOCGRP_M60,
	ASSOCGRP_SNIPER,
	ASSOCGRP_THROW,
	ASSOCGRP_FLAMETHROWER,
	ASSOCGRP_MEDIC,
	ASSOCGRP_SUNBATHE,
	ASSOCGRP_PLAYER_IDLE,
	ASSOCGRP_RIOT,
	ASSOCGRP_STRIP,
	ASSOCGRP_LANCE,
	ASSOCGRP_PLAYER,
	ASSOCGRP_PLAYERROCKET,
	ASSOCGRP_PLAYER1ARMED,
	ASSOCGRP_PLAYER2ARMED,
	ASSOCGRP_PLAYERBBBAT,
	ASSOCGRP_PLAYERCHAINSAW,
	ASSOCGRP_SHUFFLE,
	ASSOCGRP_OLD,
	ASSOCGRP_GANG1,
	ASSOCGRP_GANG2,
	ASSOCGRP_FAT,
	ASSOCGRP_OLDFAT,
	ASSOCGRP_JOGGER,
	ASSOCGRP_WOMAN,
	ASSOCGRP_WOMANSHOP,
	ASSOCGRP_BUSYWOMAN,
	ASSOCGRP_SEXYWOMAN,
	ASSOCGRP_FATWOMAN,
	ASSOCGRP_OLDWOMAN,
	ASSOCGRP_JOGWOMAN,
	ASSOCGRP_PANICCHUNKY,
	ASSOCGRP_SKATE,
#ifdef PC_PLAYER_CONTROLS
	ASSOCGRP_PLAYERBACK,
	ASSOCGRP_PLAYERLEFT,
	ASSOCGRP_PLAYERRIGHT,
	ASSOCGRP_ROCKETBACK,
	ASSOCGRP_ROCKETLEFT,
	ASSOCGRP_ROCKETRIGHT,
	ASSOCGRP_CHAINSAWBACK,
	ASSOCGRP_CHAINSAWLEFT,
	ASSOCGRP_CHAINSAWRIGHT,
#endif

	NUM_ANIM_ASSOC_GROUPS
};

class CAnimBlendAssociation;
class CAnimBlendAssocGroup;

#define MAX_ANIMBLOCK_NAME 20

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

struct AnimAssocDesc
{
	int32 animId;
	int32 flags;
};

struct AnimAssocDefinition
{
	char const *name;
	char const *blockName;
	int32 modelIndex;
	int32 numAnims;
	char const **animNames;
	AnimAssocDesc *animDescs;
};

class CAnimManager
{
	static const AnimAssocDefinition ms_aAnimAssocDefinitions[NUM_ANIM_ASSOC_GROUPS];
	static CAnimBlock ms_aAnimBlocks[NUMANIMBLOCKS];
	static CAnimBlendHierarchy ms_aAnimations[NUMANIMATIONS];
	static int32 ms_numAnimBlocks;
	static int32 ms_numAnimations;
	static CAnimBlendAssocGroup *ms_aAnimAssocGroups;
	static CAnimBlendAssocGroup *ms_aSAAnimAssocGroups;	// per-clump SA associations; VC groups remain native
	static RpClump *ms_pVCPedAnimationReference;	// owned native VC skeleton for custom-model startup
	static CLinkList<CAnimBlendHierarchy*> ms_animCache;
	static CAnimBlendAssocGroup *GetAnimAssocGroup(RpClump *clump, AssocGroupId groupId);
public:

	static void Initialise(void);
	static void Shutdown(void);
	// Takes ownership of a temporary native VC ped clump used only as an
	// association-building reference when all loaded peds are SA models.
	static void SetVCPedAnimationReference(RpClump *clump);
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
	// A hierarchy may only be used with a clump from the same animation bank.
	// A null clump denotes an explicitly native-VC-only lookup.
	static bool IsAnimationCompatibleWithClump(RpClump *clump, CAnimBlendHierarchy *hierarchy);
	// Vehicle approach offsets must come from the ped's own bank. The native
	// lookup also works without a VC reference clump.
	static CAnimBlendHierarchy *GetAnimHierarchyForOffset(AssocGroupId groupId, AnimationId animId);
	static CAnimBlendHierarchy *GetAnimHierarchyForOffset(RpClump *clump, AssocGroupId groupId, AnimationId animId);
	static const char *GetAnimGroupName(AssocGroupId groupId);
	static CAnimBlendAssociation *CreateAnimAssociation(AssocGroupId groupId, AnimationId animId);
	static CAnimBlendAssociation *CreateAnimAssociation(RpClump *clump, AssocGroupId groupId, AnimationId animId);
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
	static void RemoveLastAnimFile(void);
	static CAnimBlendAssocGroup* GetAnimAssocGroups(void) { return ms_aAnimAssocGroups; }
};

#endif // __GTA_ANIMMANAGER_H__
