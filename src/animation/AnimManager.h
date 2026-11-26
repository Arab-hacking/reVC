#pragma once

#include "AnimationStyleDescriptor.h"
#include "AnimBlendAssocGroup.h"
#include "AnimBlendAssociation.h"
#include "AnimBlock.h"
#include "templates.h"

struct IFPSectionHeader;

#define MAX_ANIM_BLOCK_NAME 16
#define NUM_ANIM_ASSOC_GROUPS 118
#define NUM_ANIM_BLOCKS 180
#define NUM_ANIMATIONS 2500

class CAnimManager
{
private:
	static const AnimAssocDefinition ms_aAnimAssocDefinitionsStatic[NUM_ANIM_ASSOC_GROUPS];
	static AnimAssocDefinition ms_aAnimAssocDefinitions[ANIM_TOTAL_GROUPS];
	static uint32 ms_numAnimAssocDefinitions;

	static CAnimBlendAssocGroup *ms_aAnimAssocGroups;

	static CAnimBlendHierarchy ms_aAnimations[NUM_ANIMATIONS];
	static int32 ms_numAnimations;

	static CAnimBlock ms_aAnimBlocks[NUM_ANIM_BLOCKS];
	static uint32 ms_numAnimBlocks;

	static CLinkList<CAnimBlendHierarchy*> ms_AnimCache;

public:
	static void Initialise();
	static void Shutdown();

	static void LoadAnimFiles();
	static void LoadAnimFile(RwStream *stream, bool loadCompressed, const char (*uncompressedAnimations)[32] = nil);
	static void ReadAnimAssociationDefinitions();

	static CAnimBlock *GetAnimationBlock(AssocGroupId animGroup);
	static CAnimBlock *GetAnimationBlock(const char *name);

	static int32 GetAnimationBlockIndex(AssocGroupId animGroup);
	static int32 GetAnimationBlockIndex(CAnimBlock *animBlock);
	static int32 GetAnimationBlockIndex(const char *name);

	static AssocGroupId GetFirstAssocGroup(const char *name);
	static CAnimBlendHierarchy *GetAnimation(uint32 hash, const CAnimBlock *animBlock);
	static CAnimBlendHierarchy *GetAnimation(const char *animName, const CAnimBlock *animBlock);
	static CAnimBlendHierarchy &GetAnimation(AnimationId id);
	static const char *GetAnimGroupName(AssocGroupId groupId);
	static const char *GetAnimBlockName(AssocGroupId groupId);
	static AssocGroupId GetAnimationGroupIdByName(const char *name);
	static CAnimBlendStaticAssociation *GetAnimAssociation(AssocGroupId groupId, AnimationId animId);
	static CAnimBlendStaticAssociation *GetAnimAssociation(AssocGroupId groupId, const char *animName);
	static CAnimBlendAssociation *AddAnimationToClump(RpClump *clump, CAnimBlendAssociation *anim); // Internal
	static int32 GetNumRefsToAnimBlock(int32 index);

	static CAnimBlendAssociation *CreateAnimAssociation(AssocGroupId groupId, AnimationId animId);

	static CAnimBlendAssociation *AddAnimation(RpClump *clump, AssocGroupId groupId, AnimationId animId);
	static CAnimBlendAssociation *AddAnimation(RpClump *clump, CAnimBlendHierarchy *hier, int32 clumpAssocFlag);
	static CAnimBlendAssociation *AddAnimationAndSync(RpClump *clump, CAnimBlendAssociation *syncWith, AssocGroupId groupId, AnimationId animId);
	static AnimAssocDefinition *AddAnimAssocDefinition(const char *groupName, const char *blockName, uint32 modelIndex, uint32 animsCount, AnimDescriptor *descriptor);
	static void AddAnimToAssocDefinition(AnimAssocDefinition *definition, const char *animName);
	static void AddAnimBlockRef(int32 index);

	static void CreateAnimAssocGroups();
	static int32 RegisterAnimBlock(const char *name);
	static void RemoveLastAnimFile();
	static void RemoveAnimBlock(int32 index);
	static void RemoveAnimBlockRef(int32 index);
	static void RemoveAnimBlockRefWithoutDelete(int32 index);
	static void RemoveFromUncompressedCache(CAnimBlendHierarchy *hier);

	/*!
	 * @addr 0x4D41C0
	 *
	 * @brief Uncompress animation data (Unless the animation has the keep-compressed flag).
	 * @brief Also marks the anim as recently-used in the LRU cache
	 *
	 * @details This function does a 2 things:
	 * @details - Uncompress anim data  (Unless the animation has the keep-compressed flag)
	 * @details - Since it uses a LRU cache, the hierarchy is put at the front (thus marking it as recently used)
	 *
	 * @details So even if the anim is already un-compressed this function should be called
	 * @details to mark it as recently-used in the LRU cache.
	 *
	 * @param hier The hierarchy to uncompress
	 */
	static void UncompressAnimation(CAnimBlendHierarchy *hier);
	static CAnimBlendAssociation *BlendAnimation(RpClump *clump, CAnimBlendHierarchy *animBlendHier, int32 flags, float clumpAssocBlendData = 8.0f);
	static CAnimBlendAssociation *BlendAnimation(RpClump *clump, AssocGroupId groupId, AnimationId animId, float clumpAssocBlendData = 8.0f);

	static uint32 GetAnimIndex(const CAnimBlendHierarchy *h);
	static bool IsAnimInBlock(const CAnimBlendHierarchy *h, const CAnimBlock *b);

	/// Get random gangtalk anim
	static AnimationId GetRandomGangTalkAnim();

	static CAnimBlock *GetAnimBlocks() { return ms_aAnimBlocks; }
	static uint32 GetNumAnimBlocks() { return ms_numAnimBlocks; }
	static CAnimBlendAssocGroup *GetAssocGroups() { return ms_aAnimAssocGroups; }
	static AnimAssocDefinition *GetAssocGroupDefs() { return ms_aAnimAssocDefinitions; }
	static uint32 GetNumAssocGroupDefs() { return ms_numAnimAssocDefinitions; }

private:
	static void LoadAnimFile_ANPK(RwStream *stream, const IFPSectionHeader &h, bool compress, const char (*uncompressedAnims)[32]);
	static void LoadAnimFile_ANP23(RwStream *stream, bool isANP3, const char *blockName, uint32 numAnims, bool loadCompressed);
	static CAnimBlock *GetOrCreateAnimBlock(const char *name, uint32 numAnims, int32 *blockIndex);
};

// 0x4C4DC0
bool IsClumpSkinned(RpClump *clump);
