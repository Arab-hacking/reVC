#include "common.h"

#include "AnimBlendAssocGroup.h"
#include "AnimBlendAssociation.h"
#include "AnimBlendStaticAssociation.h"
#include "AnimBlendHierarchy.h"
#include "AnimBlock.h"
#include "AnimManager.h"
#include "RwHelper.h"
#include "ModelInfo.h"
#include "BaseModelInfo.h"
#include "RpAnimBlend.h"
#include "KeyGen.h"

// 0x4CE0B0
CAnimBlendAssociation *CAnimBlendAssocGroup::CopyAnimation(const char *name)
{
	return CopyAnimation(GetAnimation(name));
}

// 0x4CE130
CAnimBlendAssociation *CAnimBlendAssocGroup::CopyAnimation(AnimationId id)
{
	return CopyAnimation(GetAnimation(id));
}

CAnimBlendAssociation *CAnimBlendAssocGroup::CopyAnimation(CAnimBlendStaticAssociation *anim)
{
	if (!anim || !anim->m_BlendHier)
		return nil;
	CAnimManager::UncompressAnimation(anim->m_BlendHier);
	return new CAnimBlendAssociation(*anim);
}

void CAnimBlendAssocGroup::CreateAssociation(CAnimBlendStaticAssociation *assoc, CAnimBlendHierarchy *anim, CBaseModelInfo *mi, size_t groupAnimId)
{
	RpClump *clump = (RpClump *)mi->CreateInstance();
	RpAnimBlendClumpInit(clump);
	assoc->Init(clump, anim);
	if (IsClumpSkinned(clump))
		RpClumpForAllAtomics(clump, AtomicRemoveAnimFromSkinCB, nil);
	RpClumpDestroy(clump);

	assoc->m_AnimId = (int16)(m_IdOffset + groupAnimId);
	assoc->m_AnimGroupId = (int16)m_GroupID;
}

// 0x4CE220
void CAnimBlendAssocGroup::CreateAssociations(const char *blockName)
{
	CAnimBlock *ablock = AllocateForBlock(blockName, -1);

	for (size_t i = 0; i < ablock->NumAnims; i++) {
		AnimationId id = (AnimationId)(ablock->FirstAnimIdx + i);
		CAnimBlendHierarchy *anim = &CAnimManager::GetAnimation(id);
		CAnimBlendStaticAssociation *assoc = &GetAssociations()[i];
		CBaseModelInfo *mi = CModelInfo::GetModelInfoFromHashKey(anim->m_hashKey);
		if (!mi) {
			debug("ModelInfo (Key=%u) for animation (Block=%s; ID=%d) not found\n", anim->m_hashKey, blockName, (int)id);
			continue;
		}
		CreateAssociation(assoc, anim, mi, i);
	}
}

// 0x4CE3B0
void CAnimBlendAssocGroup::CreateAssociations(const char *blockName, const char *animNames, const char *modelNames, uint32 buffStride)
{
	if (!modelNames)
		return CreateAssociations(blockName);

	CAnimBlock *ab = AllocateForBlock(blockName, -1);

	// We need to start from 0 in this case - some of the animations may not be found,
	// and if we assume that all went well we end up with gaps in the ms_aAnimations table
	m_NumAnims = 0;

	for (size_t i = 0; i < ab->NumAnims; i++) {
		AnimationId id = (AnimationId)(ab->FirstAnimIdx + i);
		CAnimBlendHierarchy *anim = &CAnimManager::GetAnimation(id);
		CAnimBlendStaticAssociation *assoc = &m_Anims[m_NumAnims]; // Can't use GetAssociations() helper method, as m_NumAnims will always be (no. of elements + 1)

		const char *animName = animNames;
		const char *modelName = modelNames;

		// Find the anim's name and it's model's name, then initialize it
		// Remember that these 2 arrays (animNames, modelNames) are "parallel" (like kv pairs)
		bool found;
		size_t k;
		for (k = 0; (found = k < ab->NumAnims); k++, animName += buffStride, modelName += buffStride) {
			if (CKeyGen::GetUppercaseKey(animName) == anim->GetHashKey())
				break;
		}
		if (!found) {
			debug("Name of Animation (Block=%s; ID=%d) not found\n", blockName, (int)id);
			continue;
		}

		CBaseModelInfo *mi = CModelInfo::GetModelInfo(modelName, nil);
		if (!mi) {
			debug("ModelInfo (%s) for Animation (Name=%s; Block=%s; ID=%d) not found\n", modelName, animName, blockName, (int)id);
			continue;
		}

		CreateAssociation(assoc, anim, mi, i);
		++m_NumAnims;
	}
}

// 0x4CE6E0
void CAnimBlendAssocGroup::CreateAssociations(const char *blockName, RpClump *clump, const char **names, uint32 cnt)
{
	AllocateForBlock(blockName, cnt);

	for (size_t i = 0; i < cnt; i++) {
		const char *name = names[i];
		CAnimBlendStaticAssociation *assoc = &GetAssociations()[i];

		if (name[0]) { // Check is empty string
			CAnimBlendHierarchy *hier = CAnimManager::GetAnimation(name, m_AnimBlock);
			if (hier) {
				assoc->Init(clump, hier);
				assoc->m_AnimId = (int16)(m_IdOffset + i);
				assert(assoc->m_AnimId < ANIM_ID_MAX);
			} else {
				// The definition names a clip this block does not
				// have. Keep the slot (ids are positional) but leave
				// it unbound so lookups fail soft instead of crashing.
				debug("Animation %s not found in block %s, association left empty\n", name, blockName);
				assoc->m_AnimId = ANIM_ID_UNDEFINED;
			}
		} else {
			assoc->m_AnimId = ANIM_ID_UNDEFINED;
		}

		assoc->m_AnimGroupId = (int16)m_GroupID;
	}
}

// 0x4CDFF0
void CAnimBlendAssocGroup::DestroyAssociations()
{
	delete[] m_Anims;
	m_Anims = nil;
	m_NumAnims = 0;
}

// 0x4CE040
CAnimBlendStaticAssociation *CAnimBlendAssocGroup::GetAnimation(const char *name)
{
	const uint32 key = CKeyGen::GetUppercaseKey(name);
	for (uint32 i = 0; i < m_NumAnims; i++) {
		CAnimBlendStaticAssociation &a = GetAssociations()[i];
		if (a.GetHashKey() == key)
			return &a;
	}
	return nil;
}

// 0x4CE090
CAnimBlendStaticAssociation *CAnimBlendAssocGroup::GetAnimation(AnimationId id)
{
	// SA could index blindly because its groups were always built and
	// its ids always in range. Here a group may not be built yet (its
	// block never streamed in) or an id may fall outside the group's
	// descriptor range; callers are written to accept nil, so give it
	// to them instead of a wild pointer.
	if (!m_Anims)
		return nil;
	int32 idx = (int32)id - (int32)m_IdOffset;
	if (idx < 0 || idx >= (int32)m_NumAnims)
		return nil;
	return &GetAssociations()[idx];
}

// 0x4CE1B0
AnimationId CAnimBlendAssocGroup::GetAnimationId(const char *name)
{
	if (CAnimBlendStaticAssociation *a = GetAnimation(name))
		return (AnimationId)a->m_AnimId;
	return ANIM_ID_UNDEFINED;
}

// 0x4CDFB0
void CAnimBlendAssocGroup::InitEmptyAssociations(RpClump *clump)
{
	for (size_t i = m_NumAnims; i-- > 0;) {
		CAnimBlendStaticAssociation *anim = &GetAssociations()[i];
		if (!anim->IsValid())
			anim->Init(clump, anim->GetAnimHierarchy());
	}
}

CAnimBlock *CAnimBlendAssocGroup::AllocateForBlock(const char *blockName, int32 numAnims)
{
	if (m_Anims)
		DestroyAssociations();

	CAnimBlock *ablock = CAnimManager::GetAnimationBlock(blockName);

	if (numAnims == -1)
		numAnims = (int32)ablock->NumAnims;

	m_AnimBlock = ablock;
	m_Anims = new CAnimBlendStaticAssociation[numAnims];
	m_NumAnims = numAnims;

	return ablock;
}

// 0x4CE1D0
CAnimBlendAssocGroup::~CAnimBlendAssocGroup()
{
	DestroyAssociations();
}
