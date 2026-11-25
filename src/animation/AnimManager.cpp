#include "common.h"

#include "AnimManager.h"
#include "AnimAssocDescriptions.h"
#include "AnimBlendHierarchy.h"
#include "AnimBlendClumpData.h"
#include "AnimBlendStaticAssociation.h"
#include "RpAnimBlend.h"
#include "KeyGen.h"
#include "MemoryMgr.h"
#include "FileMgr.h"
#include "FileLoader.h"
#include "ModelInfo.h"
#include "BaseModelInfo.h"
#include "ModelIndices.h"
#include "Streaming.h"
#include "RwHelper.h"
#include "General.h"

// 0x4C4DC0
bool IsClumpSkinned(RpClump *clump)
{
	RpAtomic *a = GetFirstAtomic(clump);
	return a && RpSkinGeometryGetSkin(RpAtomicGetGeometry(a)) != nil;
}

AnimAssocDefinition CAnimManager::ms_aAnimAssocDefinitions[ANIM_TOTAL_GROUPS];
uint32 CAnimManager::ms_numAnimAssocDefinitions;
CAnimBlendAssocGroup *CAnimManager::ms_aAnimAssocGroups;
CAnimBlendHierarchy CAnimManager::ms_aAnimations[NUM_ANIMATIONS];
int32 CAnimManager::ms_numAnimations;
CAnimBlock CAnimManager::ms_aAnimBlocks[NUM_ANIM_BLOCKS];
uint32 CAnimManager::ms_numAnimBlocks;
CLinkList<CAnimBlendHierarchy*> CAnimManager::ms_AnimCache;

// 0x5BF6B0
void CAnimManager::Initialise()
{
	memcpy(ms_aAnimAssocDefinitions, ms_aAnimAssocDefinitionsStatic, sizeof(ms_aAnimAssocDefinitionsStatic));
	ms_numAnimations = 0;
	ms_numAnimBlocks = 0;
	ms_numAnimAssocDefinitions = 118; // ANIM_TOTAL_GROUPS aka NUM_ANIM_ASSOC_GROUPS
	ms_AnimCache.Init(50);
	ReadAnimAssociationDefinitions();
	RegisterAnimBlock("ped");
}

// 0x5BC910
void CAnimManager::ReadAnimAssociationDefinitions()
{
	char groupName[32], blockName[32], type[32];
	bool isAnimSection = false;
	AnimAssocDefinition *def = nil;
	uint32 animCount = 0;

	CFileMgr::SetDir("");
	int f = CFileMgr::OpenFile("DATA\\ANIMGRP.DAT", "rb");
	for (;;) {
		char *l = CFileLoader::LoadLine(f);
		if (!l)
			break;
		if (!*l || *l == '#')
			continue;
		if (isAnimSection) {
			if (sscanf(l, "%s", groupName) == 1) {
				if (!memcmp(groupName, "end", 4)) {
					isAnimSection = false;
				} else {
					AddAnimToAssocDefinition(def, groupName);
				}
			}
		} else {
			assert(sscanf(l, "%s %s %s %d", groupName, blockName, type, &animCount) == 4);
			def = AddAnimAssocDefinition(groupName, blockName, MI_MALE01, animCount, aStdAnimDescs);
			isAnimSection = true;
		}
	}
	CFileMgr::CloseFile(f);
}

// 0x4D4130
void CAnimManager::Shutdown()
{
	for (uint32 i = 0; i < NUM_ANIM_BLOCKS; i++)
		CStreaming::RemoveAnim(i);

	for (int32 i = 0; i < ms_numAnimations; i++)
		ms_aAnimations[i].Shutdown();

	ms_AnimCache.Shutdown();
	delete[] ms_aAnimAssocGroups;
	ms_aAnimAssocGroups = nil;

	// Free the name buffers of the definitions built by ReadAnimAssociationDefinitions
	for (uint32 i = NUM_ANIM_ASSOC_GROUPS; i < ms_numAnimAssocDefinitions; i++) {
		AnimAssocDefinition *def = &ms_aAnimAssocDefinitions[i];
		if (def->AnimNames) {
			delete[] (char *)def->AnimNames[0];
			delete[] def->AnimNames;
			def->AnimNames = nil;
		}
	}
}

CAnimBlock *CAnimManager::GetAnimationBlock(AssocGroupId animGroup)
{
	return GetAssocGroups()[animGroup].m_AnimBlock;
}

// 0x4D3940
CAnimBlock *CAnimManager::GetAnimationBlock(const char *name)
{
	for (uint32 i = 0; i < ms_numAnimBlocks; i++) {
		CAnimBlock &ab = ms_aAnimBlocks[i];
		if (!strcasecmp(name, ab.Name))
			return &ab;
	}
	return nil;
}

int32 CAnimManager::GetAnimationBlockIndex(AssocGroupId animGroup)
{
	return GetAnimationBlock(animGroup) - ms_aAnimBlocks;
}

int32 CAnimManager::GetAnimationBlockIndex(CAnimBlock *animBlock)
{
	return (int32)(animBlock - ms_aAnimBlocks);
}

// 0x4D3990
int32 CAnimManager::GetAnimationBlockIndex(const char *name)
{
	CAnimBlock *b = GetAnimationBlock(name);
	return b ? GetAnimationBlockIndex(b) : -1;
}

// 0x4D39B0
AssocGroupId CAnimManager::GetFirstAssocGroup(const char *name)
{
	for (int32 i = 0; i < ANIM_GROUP_MAN; i++) {
		if (!strcasecmp(ms_aAnimAssocDefinitions[i].BlockName, name))
			return (AssocGroupId)i;
	}
	return ANIM_GROUP_MAN;
}

// 0x4D39F0
CAnimBlendHierarchy *CAnimManager::GetAnimation(uint32 hash, const CAnimBlock *animBlock)
{
	CAnimBlendHierarchy *h = &ms_aAnimations[animBlock->FirstAnimIdx];
	for (uint32 i = animBlock->NumAnims; i-- > 0; h++) {
		if (h->m_hashKey == hash)
			return h;
	}
	return nil;
}

// 0x4D42F0
CAnimBlendHierarchy *CAnimManager::GetAnimation(const char *animName, const CAnimBlock *animBlock)
{
	return GetAnimation(CKeyGen::GetUppercaseKey(animName), animBlock);
}

CAnimBlendHierarchy &CAnimManager::GetAnimation(AnimationId id)
{
	return ms_aAnimations[(size_t)id];
}

// 0x4D3A20
const char *CAnimManager::GetAnimGroupName(AssocGroupId groupId)
{
	return ms_aAnimAssocDefinitions[groupId].GroupName;
}

// 0x4D3A30
const char *CAnimManager::GetAnimBlockName(AssocGroupId groupId)
{
	return ms_aAnimAssocDefinitions[groupId].BlockName;
}

AssocGroupId CAnimManager::GetAnimationGroupIdByName(const char *name)
{
	for (uint32 i = 0; i < ms_numAnimAssocDefinitions; i++) {
		if (!strcasecmp(ms_aAnimAssocDefinitions[i].GroupName, name))
			return (AssocGroupId)i;
	}
	assert(0 && "Couldn't find group with name");
	return ANIM_GROUP_NONE;
}

// 0x4D3A40
CAnimBlendAssociation *CAnimManager::CreateAnimAssociation(AssocGroupId groupId, AnimationId animId)
{
	return GetAssocGroups()[groupId].CopyAnimation(animId);
}

// 0x4D3A60
CAnimBlendStaticAssociation *CAnimManager::GetAnimAssociation(AssocGroupId groupId, AnimationId animId)
{
	return GetAssocGroups()[groupId].GetAnimation(animId);
}

// 0x4D3A80
CAnimBlendStaticAssociation *CAnimManager::GetAnimAssociation(AssocGroupId groupId, const char *animName)
{
	return GetAssocGroups()[groupId].GetAnimation(animName);
}

CAnimBlendAssociation *CAnimManager::AddAnimationToClump(RpClump *clump, CAnimBlendAssociation *anim)
{
	CAnimBlendLink *clumpAnims = &RpAnimBlendClumpGetData(clump)->m_AnimList;

	CAnimBlendAssociation *syncWith = nil;
	if (anim->IsSyncronised()) {
		for (CAnimBlendLink *l = clumpAnims->next; l; l = l->next) {
			CAnimBlendAssociation *a = CAnimBlendAssociation::FromLink(l);
			if (a->IsSyncronised()) {
				syncWith = a;
				break;
			}
		}
	}

	if (syncWith) {
		anim->SyncAnimation(syncWith);
		anim->m_Flags |= ANIMATION_IS_PLAYING;
	} else {
		anim->Start(0.0f);
	}

	clumpAnims->Prepend(&anim->m_Link);

	return anim;
}

// 0x4D3AA0
CAnimBlendAssociation *CAnimManager::AddAnimation(RpClump *clump, AssocGroupId groupId, AnimationId animId)
{
	return AddAnimationToClump(clump, CreateAnimAssociation(groupId, animId));
}

// 0x4D4330
CAnimBlendAssociation *CAnimManager::AddAnimation(RpClump *clump, CAnimBlendHierarchy *hier, int32 clumpAssocFlag)
{
	CAnimBlendAssociation *anim = new CAnimBlendAssociation(clump, hier);
	anim->m_Flags |= clumpAssocFlag;
	anim->ReferenceAnimBlock();
	UncompressAnimation(hier);
	return AddAnimationToClump(clump, anim);
}

// 0x4D3B30
CAnimBlendAssociation *CAnimManager::AddAnimationAndSync(RpClump *clump, CAnimBlendAssociation *syncWith, AssocGroupId groupId, AnimationId animId)
{
	CAnimBlendAssociation *a = CreateAnimAssociation(groupId, animId);
	if (a->IsSyncronised() && syncWith) {
		a->SyncAnimation(syncWith);
		a->m_Flags |= ANIMATION_IS_PLAYING;
	} else {
		a->Start(0.0f);
	}

	RpAnimBlendClumpGetData(clump)->m_AnimList.Prepend(&a->m_Link);

	return a;
}

// 0x4D3BA0
AnimAssocDefinition *CAnimManager::AddAnimAssocDefinition(const char *groupName, const char *blockName, uint32 modelIndex, uint32 animsCount, AnimDescriptor *descriptor)
{
	AnimAssocDefinition *def = &ms_aAnimAssocDefinitions[ms_numAnimAssocDefinitions++];

	strcpy(def->GroupName, groupName);
	strcpy(def->BlockName, blockName);

	def->ModelIndex = (int32)modelIndex;
	def->NumAnims = (int32)animsCount;
	def->AnimDescr = descriptor;

	def->AnimNames = new const char *[animsCount];
	const size_t bufsz = AnimAssocDefinition::ANIM_NAME_BUF_SZ * animsCount;
	char *buf = new char[bufsz];
	memset(buf, 0, bufsz);
	for (uint32 i = animsCount; i-- > 0;)
		def->AnimNames[i] = buf + i * AnimAssocDefinition::ANIM_NAME_BUF_SZ;

	return def;
}

// 0x4D3C80
void CAnimManager::AddAnimToAssocDefinition(AnimAssocDefinition *def, const char *animName)
{
	int32 i = 0;
	for (; def->AnimNames[i][0]; i++)
		assert(i < def->NumAnims);
	// `const_cast` is fine here, because it's heap allocated
	strncpy((char *)def->AnimNames[i], animName, AnimAssocDefinition::ANIM_NAME_BUF_SZ);
}

// 0x4D3CC0
void CAnimManager::CreateAnimAssocGroups()
{
	for (uint32 i = 0; i < ms_numAnimAssocDefinitions; i++) {
		CAnimBlendAssocGroup &group = GetAssocGroups()[i];
		AnimAssocDefinition *def = &ms_aAnimAssocDefinitions[i];
		CAnimBlock *block = GetAnimationBlock(def->BlockName);
		if (block == nil || !block->IsLoaded || group.m_Anims)
			continue;

		RpClump *clump = nil;
		if (def->ModelIndex != -1) {
			clump = (RpClump *)CModelInfo::GetModelInfo(def->ModelIndex)->CreateInstance();
			RpAnimBlendClumpInit(clump);
		}

		group.m_GroupID = (AssocGroupId)i;
		group.m_IdOffset = def->AnimDescr->AnimId;
		group.CreateAssociations(def->BlockName, clump, def->AnimNames, def->NumAnims);
		for (uint32 j = 0; j < group.m_NumAnims; j++)
			group.GetAnimation(def->AnimDescr[j].AnimId)->m_Flags |= def->AnimDescr[j].Flags;

		if (clump) {
			if (IsClumpSkinned(clump))
				RpClumpForAllAtomics(clump, AtomicRemoveAnimFromSkinCB, nil);
			RpClumpDestroy(clump);
		}
	}
}

// 0x4D3E50
int32 CAnimManager::RegisterAnimBlock(const char *name)
{
	CAnimBlock *ab = GetAnimationBlock(name);
	if (ab == nil) { // Initialize a new anim block
		ab = &ms_aAnimBlocks[ms_numAnimBlocks++];
		strncpy(ab->Name, name, MAX_ANIM_BLOCK_NAME);
		ab->NumAnims = 0;
		ab->GroupId = GetFirstAssocGroup(name);
		assert(ab->RefCnt == 0);
	}

	return GetAnimationBlockIndex(ab);
}

// 0x4D3ED0
void CAnimManager::RemoveLastAnimFile()
{
	CAnimBlock *ab = &ms_aAnimBlocks[--ms_numAnimBlocks];
	ms_numAnimations = ab->FirstAnimIdx;
	for (uint32 i = 0; i < ab->NumAnims; i++) // Remove related animations too
		ms_aAnimations[ab->FirstAnimIdx + i].Shutdown();
	ab->IsLoaded = false;
}

// 0x4D3F40
void CAnimManager::RemoveAnimBlock(int32 index)
{
	CAnimBlock *ab = &ms_aAnimBlocks[index];

	for (uint32 i = 0; i < ms_numAnimAssocDefinitions; i++) {
		CAnimBlendAssocGroup &g = GetAssocGroups()[i];
		if (g.m_AnimBlock == ab)
			g.DestroyAssociations();
	}

	for (uint32 i = 0; i < ab->NumAnims; i++) // Remove related animations too
		ms_aAnimations[ab->FirstAnimIdx + i].Shutdown();

	ab->IsLoaded = false;
	ab->RefCnt = 0;
}

// 0x4D3FB0
void CAnimManager::AddAnimBlockRef(int32 index)
{
	ms_aAnimBlocks[index].RefCnt++;
}

// 0x4D3FD0
void CAnimManager::RemoveAnimBlockRef(int32 index)
{
	ms_aAnimBlocks[index].RefCnt--;
}

// 0x4D3FF0
void CAnimManager::RemoveAnimBlockRefWithoutDelete(int32 index)
{
	ms_aAnimBlocks[index].RefCnt--;
}

// 0x4D4010
int32 CAnimManager::GetNumRefsToAnimBlock(int32 index)
{
	return ms_aAnimBlocks[index].RefCnt;
}

// 0x4D41C0
void CAnimManager::UncompressAnimation(CAnimBlendHierarchy *h)
{
	if (h->IsRunningCompressed()) { // Keep as compressed?
		if (h->GetTotalTime() == 0.0f)
			h->CalcTotalTimeCompressed();
	} else if (!h->IsUncompressed()) { // Need to uncompress?
		assert(!h->m_Link); // Sanity check
		CLink<CAnimBlendHierarchy*> *l = ms_AnimCache.Insert(h);
		if (!l) { // No more free links? (This is totally normal as animations aren't compressed back unless the cache is full)
			// Remove least recently used item
			CLink<CAnimBlendHierarchy*> *llr = ms_AnimCache.tail.prev;
			llr->item->RemoveUncompressedData();

			// TODO: If the anim is still in use this will corrupt the animation data, and (hopefully) hit the assert in `GetKeyFrame`!
			//       There's currently no way for us to tell if the animation is in use, so there's no better solution for now.
			RemoveFromUncompressedCache(llr->item);

			// Now try again, this time it should succeed
			l = ms_AnimCache.Insert(h);
			assert(l);
		}
		h->m_Link = l;
		h->Uncompress();
	} else if (h->m_Link) { // Already uncompressed, mark as recently-used in cache
		ms_AnimCache.Remove(h->m_Link); // Remove from current position
		h->m_Link = ms_AnimCache.Insert(h); // Now re-insert at head
	}
}

// 0x4D42A0
void CAnimManager::RemoveFromUncompressedCache(CAnimBlendHierarchy *h)
{
	if (CLink<CAnimBlendHierarchy*> *l = h->m_Link) {
		assert(l->item == h);
		ms_AnimCache.Remove(l);
		h->m_Link = nil;
	}
}

// 0x4D4410
CAnimBlendAssociation *CAnimManager::BlendAnimation(RpClump *clump, CAnimBlendHierarchy *toBlendHier, int32 toBlendFlags, float blendDelta)
{
	CAnimBlendClumpData *clumpAnimData = RpAnimBlendClumpGetData(clump); // Get running anim data

	CAnimBlendAssociation *running = nil; // Running instance of this anim
	bool bFadeThisOut = false;
	for (CAnimBlendLink *l = clumpAnimData->m_AnimList.next; l; l = l->next) {
		CAnimBlendAssociation *a = CAnimBlendAssociation::FromLink(l);
		assert(a->m_BlendHier);
		if (a->m_BlendHier && a->m_BlendHier == toBlendHier) { // Found an instance of this anim running
			running = a;
		} else if (((toBlendFlags & ANIMATION_IS_PARTIAL) != 0) == a->IsPartial()) {
			if (a->m_BlendAmount <= 0.0f) {
				a->m_BlendDelta = -1.0f;
			} else {
				float bd = a->GetBlendAmount() * -blendDelta;
				if ((toBlendFlags & ANIMATION_IS_PARTIAL) == 0 || bd <= a->GetBlendDelta() || (a->m_BlendHier->m_nAnimBlockId && a->m_BlendHier->m_nAnimBlockId == toBlendHier->m_nAnimBlockId)) {
					a->m_BlendDelta = bd < -0.05f ? bd : -0.05f;
				}
			}

			a->SetFlag(ANIMATION_IS_BLEND_AUTO_REMOVE);
			bFadeThisOut = true;
		}
	}

	// If already running just re-adjust blend delta (and re-start it if it has finished)
	if (running) {
		running->SetBlendDelta((1.0f - running->m_BlendAmount) * blendDelta);
		if (running->HasFinished())
			running->Start();
		UncompressAnimation(running->m_BlendHier); // Make sure anim doesn't get removed
		return running;
	}

	// Otherwise create new instance
	CAnimBlendAssociation *a = new CAnimBlendAssociation(clump, toBlendHier);
	a->m_Flags = toBlendFlags;
	a->ReferenceAnimBlock();
	UncompressAnimation(a->m_BlendHier);
	clumpAnimData->m_AnimList.Prepend(&a->m_Link);
	a->Start();
	if (bFadeThisOut || (toBlendFlags & ANIMATION_IS_PARTIAL)) {
		a->SetBlend(0.0f, blendDelta);
		UncompressAnimation(toBlendHier); // No need to call this again here, but doesn't hurt....
	} else {
		a->m_BlendAmount = 1.0f;
	}
	return a;
}

// 0x4D4610
CAnimBlendAssociation *CAnimManager::BlendAnimation(RpClump *clump, AssocGroupId groupId, AnimationId animId, float blendDelta)
{
	CAnimBlendClumpData *clumpAnimData = RpAnimBlendClumpGetData(clump); // Get running anim data

	CAnimBlendStaticAssociation *toBlendAnim = GetAssocGroups()[groupId].GetAnimation(animId);
	const bool toBlendIsMoving = (toBlendAnim->m_Flags & ANIMATION_IS_SYNCRONISED) != 0;
	const bool toBlendIsPartial = (toBlendAnim->m_Flags & ANIMATION_IS_PARTIAL) != 0;
	const bool toBlendIsIndestructible = (toBlendAnim->m_Flags & ANIMATION_FACIAL) != 0;

	CAnimBlendAssociation *running = nil, *movingAnim = nil; // Running instance of this anim, and the last moving anim in the chain
	bool bFadeThisOut = false;
	for (CAnimBlendLink *l = clumpAnimData->m_AnimList.next; l; l = l->next) {
		CAnimBlendAssociation *a = CAnimBlendAssociation::FromLink(l);

		if (toBlendIsMoving && a->IsSyncronised())
			movingAnim = a;

		if (a->m_AnimId == animId && a->m_AnimGroupId == groupId) {
			running = a;
		} else if (toBlendIsPartial == a->IsPartial() && toBlendIsIndestructible == a->IsFacial()) {
			if (a->m_BlendAmount <= 0.0f) {
				a->m_BlendDelta = -1.0f;
			} else {
				float bd = a->GetBlendAmount() * -blendDelta;
				if (bd <= a->GetBlendDelta() || !toBlendIsPartial)
					a->m_BlendDelta = bd < -0.05f ? bd : -0.05f;
			}
			a->SetFlag(ANIMATION_IS_BLEND_AUTO_REMOVE);
			bFadeThisOut = true;
		}
	}

	// If already running just re-adjust blend delta (and start it if it has finished)
	if (running) {
		running->SetBlendDelta((1.0f - running->GetBlendAmount()) * blendDelta);
		if (running->HasFinished())
			running->Start();
		UncompressAnimation(running->m_BlendHier);
		return running;
	}

	// Otherwise create new anim, and possibly sync it with the last moving anim of the clump
	CAnimBlendAssociation *anim = AddAnimationAndSync(clump, movingAnim, groupId, animId);
	if (bFadeThisOut || toBlendIsPartial) {
		anim->SetBlend(0.0f, blendDelta);
		UncompressAnimation(anim->m_BlendHier);
	} else {
		anim->m_BlendAmount = 1.0f;
	}

	return anim;
}

uint32 CAnimManager::GetAnimIndex(const CAnimBlendHierarchy *h)
{
	const int32 idx = (int32)(h - ms_aAnimations);
	assert(idx >= 0 && idx <= ms_numAnimations);
	return (uint32)idx;
}

bool CAnimManager::IsAnimInBlock(const CAnimBlendHierarchy *h, const CAnimBlock *b)
{
	const int32 animIdx = (int32)h->GetIndex();
	return animIdx >= b->FirstAnimIdx && animIdx <= b->FirstAnimIdx + (int32)b->NumAnims;
}

// 0x4D5620
void CAnimManager::LoadAnimFiles()
{
	RwStream *stream = RwStreamOpen(rwSTREAMFILENAME, rwSTREAMREAD, (void *)"ANIM\\PED.IFP");
	assert(stream);
	LoadAnimFile(stream, true);
	RwStreamClose(stream, nil);

	ms_aAnimAssocGroups = new CAnimBlendAssocGroup[ms_numAnimAssocDefinitions];
	CreateAnimAssocGroups();
}

struct IFPSectionHeader
{
	union {
		uint32 ID;
		char IDFourCC[4];
	};
	uint32 Size;
};

static uint32 MakeFourCC(const char fourcc[4])
{
	return (uint32)fourcc[0] | ((uint32)fourcc[1] << 8) | ((uint32)fourcc[2] << 16) | ((uint32)fourcc[3] << 24);
}

CAnimBlock *CAnimManager::GetOrCreateAnimBlock(const char *name, uint32 numAnims, int32 *blockIndex)
{
	CAnimBlock *ab;
	if ((ab = GetAnimationBlock(name))) { // Block already exists, initialize it if necessary
		if (!ab->NumAnims) {
			ab->NumAnims = numAnims;
			ab->FirstAnimIdx = ms_numAnimations;
		}
	} else { // Create block
		ab = &ms_aAnimBlocks[ms_numAnimBlocks++];

		strncpy(ab->Name, name, MAX_ANIM_BLOCK_NAME);
		ab->NumAnims = numAnims;
		ab->FirstAnimIdx = ms_numAnimations;
		ab->GroupId = GetFirstAssocGroup(ab->Name);
	}
	ab->IsLoaded = true;
	*blockIndex = GetAnimationBlockIndex(ab);
	return ab;
}

// 0x4D47F0
void CAnimManager::LoadAnimFile(RwStream *stream, bool loadCompressed, const char (*uncompressedAnimations)[32])
{
	IFPSectionHeader h;
	RwStreamRead(stream, &h, sizeof(h));
	switch (h.ID) {
	case 0x33504E41: // ANP3
	case 0x32504E41: // ANP2
		return LoadAnimFile_ANP23(stream, h, loadCompressed, h.ID == 0x33504E41);
	case 0x4B504E41: // ANPK
		return LoadAnimFile_ANPK(stream, h, loadCompressed, uncompressedAnimations);
	default:
		assert(0 && "Unknown IFP format");
	}
}

static uint32 RoundTo4(uint32 num)
{
	return num % 4
		? num + 4 - (num % 4)
		: num;
}

// Code from 0x4D4863
// NOTE:
// This code was written based off the docs on gtamods
void CAnimManager::LoadAnimFile_ANPK(RwStream *s, const IFPSectionHeader &h, bool bLoadCompressed, const char (*pLoadUncompressed)[32])
{
	(void)h;

	struct InfoHeader {
		uint32 Num;
		char Name[MAX_ANIM_BLOCK_NAME];
	};

	// Read ANPK section
	IFPSectionHeader hANPK;
	RwStreamRead(s, &hANPK, sizeof(hANPK));
	InfoHeader infoAnim;
	RwStreamRead(s, &infoAnim, RoundTo4(hANPK.Size));

	int32 ablockIdx;
	CAnimBlock *ablock = GetOrCreateAnimBlock(infoAnim.Name, infoAnim.Num, &ablockIdx);

	// Read TAnimation's for this block
	for (uint32 animN = 0; animN < infoAnim.Num; animN++) {
		CAnimBlendHierarchy *hier = &ms_aAnimations[ablock->FirstAnimIdx + animN];

		// Read `TAnimation::AnimName`
		{
			char name[64];
			IFPSectionHeader hAnimName;
			RwStreamRead(s, &hAnimName, sizeof(hAnimName));
			assert(hAnimName.ID == MakeFourCC("NAME"));
			RwStreamRead(s, name, RoundTo4(hAnimName.Size));
			hier->SetName(name);
		}

		// Check if this animation is compressed
		bool isCompressed = bLoadCompressed;
		if (pLoadUncompressed) {
			for (size_t k = 0; pLoadUncompressed[k][0]; k++) {
				if (CKeyGen::GetUppercaseKey(pLoadUncompressed[k]) == hier->GetHashKey()) {
					isCompressed = false;
					break;
				}
			}
		}
		hier->m_nAnimBlockId = ablockIdx;
		hier->m_bIsCompressed = isCompressed;
		hier->m_bKeepCompressed = false;

		// Read `TAnimation::AnimData` (DGAN header)
		IFPSectionHeader hAnimData;
		RwStreamRead(s, &hAnimData, sizeof(hAnimData));
		assert(hAnimData.ID == MakeFourCC("DGAN"));

		// Read `DGAN::AnimInfo` (INFO header)
		IFPSectionHeader hAnimInfo;
		RwStreamRead(s, &hAnimInfo, sizeof(hAnimInfo));
		assert(hAnimInfo.ID == MakeFourCC("INFO"));
		struct AnimInfoHeader {
			uint32 Num;
			char Pad[60];
		};
		AnimInfoHeader animInfo;
		RwStreamRead(s, &animInfo, RoundTo4(hAnimInfo.Size));

		hier->m_nSeqCount = (uint16)animInfo.Num;
		hier->m_pSequences = new CAnimBlendSequence[hier->m_nSeqCount]; // Yes, they used `new`

		// Read `DGAN::AnimInfo` (CPAN) entries (The sequences of the animation)
		for (uint32 seqN = 0; seqN < animInfo.Num; seqN++) {
			CAnimBlendSequence *seq = &hier->m_pSequences[seqN];

			// CPAN header
			IFPSectionHeader hObjectInfo;
			RwStreamRead(s, &hObjectInfo, sizeof(hObjectInfo));
			assert(hObjectInfo.ID == MakeFourCC("CPAN"));

			struct Anim {
				char ObjName[28];    // 00 - Name of this sequence
				uint32 NumFrames;    // 28 - Number of (key)frames
				uint32 Next;         // 32 - Next sibling
				uint32 Prev;         // 36 - Previous sibling
				eBoneTag32 BoneTag;  // 40 - Only present if `Header.Size == 44`
			};
			IFPSectionHeader hObjInfo;
			RwStreamRead(s, &hObjInfo, sizeof(hObjInfo));
			assert(hObjInfo.ID == MakeFourCC("ANIM"));
			Anim objInfo;
			RwStreamRead(s, &objInfo, RoundTo4(hObjInfo.Size));

			// Set sequence name
			seq->SetName(objInfo.ObjName);

			// Set bone tag if available
			if (hObjInfo.Size == sizeof(Anim))
				seq->SetBoneTag(objInfo.BoneTag);

			if (!objInfo.NumFrames)
				continue;

			// Frame data header ("KFRM" -> Key Frame)
			IFPSectionHeader hKFRM;
			RwStreamRead(s, &hKFRM, sizeof(hKFRM));
			assert(hKFRM.ID == MakeFourCC("KRTS") || hKFRM.ID == MakeFourCC("KRT0") || hKFRM.ID == MakeFourCC("KR00"));

			// Frame properties
			const bool hasRotation = hKFRM.IDFourCC[1] == 'R';
			const bool hasTranslation = hKFRM.IDFourCC[2] == 'T';
			const bool hasScale = hKFRM.IDFourCC[3] == 'S';

			assert(hasRotation); // Rotation must always be present

			// Allocate frame data
			seq->SetNumFrames(objInfo.NumFrames, hasTranslation, isCompressed, nil);

			// Read frame data from the stream
			for (uint32 kfN = 0; kfN < objInfo.NumFrames; kfN++) {
				if (isCompressed) {
					KeyFrameTransCompressed *kf = seq->GetCKeyFrame(kfN);
					CQuaternion rot;
					RwStreamRead(s, &rot, sizeof(rot));
					kf->Rot = rot.Conjugated();
					if (hasTranslation) {
						CVector trans;
						RwStreamRead(s, &trans, sizeof(trans));
						kf->Trans = trans;
						if (hasScale)
							RwStreamSkip(s, sizeof(CVector)); // Scale ignored
					}
					float deltaTime;
					RwStreamRead(s, &deltaTime, sizeof(deltaTime));
					kf->DeltaTime = deltaTime;
				} else {
					KeyFrameTrans *kf = seq->GetUKeyFrame(kfN);
					CQuaternion rot;
					RwStreamRead(s, &rot, sizeof(rot));
					kf->Rot = rot.Conjugated();
					if (hasTranslation) {
						CVector trans;
						RwStreamRead(s, &trans, sizeof(trans));
						kf->Trans = trans;
						if (hasScale)
							RwStreamSkip(s, sizeof(CVector)); // Scale ignored
					}
					float deltaTime;
					RwStreamRead(s, &deltaTime, sizeof(deltaTime));
					kf->DeltaTime = deltaTime;
				}
			}
		}

		if (!hier->m_bIsCompressed) {
			hier->RemoveQuaternionFlips();
			hier->CalcTotalTime();
		}
	}
	ms_numAnimations = ablock->FirstAnimIdx + (int32)infoAnim.Num > ms_numAnimations
		? ablock->FirstAnimIdx + (int32)infoAnim.Num
		: ms_numAnimations;
}

// NOTE:
// This code was written based off the docs on gtamods, but it does seem to work
void CAnimManager::LoadAnimFile_ANP23(RwStream *s, const IFPSectionHeader &h, bool bLoadCompressed, bool isANP3)
{
	(void)h;
	char blockName[24];
	RwStreamRead(s, blockName, sizeof(blockName));
	uint32 numAnims;
	RwStreamRead(s, &numAnims, sizeof(numAnims));

	int32 ablockId;
	CAnimBlock *ablock = GetOrCreateAnimBlock(blockName, numAnims, &ablockId);

	for (uint32 animN = 0; animN < numAnims; animN++) {
		CAnimBlendHierarchy *hier = &ms_aAnimations[ablock->FirstAnimIdx + animN];

		// Animation name
		char aname[24];
		RwStreamRead(s, aname, sizeof(aname));
		hier->SetName(aname);

		// Number of sequences
		uint32 numSeq;
		RwStreamRead(s, &numSeq, sizeof(numSeq));

		// In ANP3 a big chunk of memory is allocated for all frames
		// instead of allocating lots of small chunks
		char *frames = nil;
		if (isANP3) {
			uint32 size, flags;
			RwStreamRead(s, &size, sizeof(size));
			RwStreamRead(s, &flags, sizeof(flags));

			hier->m_bIsCompressed = (flags & 1) != 0;

			frames = (char *)MemoryMgrMalloc(size);
		} else {
			hier->m_bIsCompressed = bLoadCompressed;
		}

		hier->m_nAnimBlockId = ablockId;
		hier->m_bKeepCompressed = false;

		// Allocate sequences now
		hier->SetNumSequences(numSeq);

		// Read sequences
		for (uint32 seqN = 0; seqN < numSeq; seqN++) {
			CAnimBlendSequence *seq = &hier->m_pSequences[seqN];

			char seqName[24];
			RwStreamRead(s, seqName, sizeof(seqName));
			uint32 frameType, numFrames;
			eBoneTag32 boneTag;
			RwStreamRead(s, &frameType, sizeof(frameType));
			RwStreamRead(s, &numFrames, sizeof(numFrames));
			RwStreamRead(s, &boneTag, sizeof(boneTag));

			// Only 1 of these will be valid in the end
			// If BoneTag != -1 then it overwrites the name.
			seq->SetName(seqName);
			seq->SetBoneTag(boneTag);

			// Read frames
			switch (frameType) {
			case 1:
				seq->SetNumFrames(numFrames, false, false, frames);
				RwStreamRead(s, seq->m_Frames, sizeof(KeyFrame) * numFrames);
				if (isANP3)
					frames += sizeof(KeyFrame) * numFrames;
				break;
			case 2:
				seq->SetNumFrames(numFrames, true, false, frames);
				RwStreamRead(s, seq->m_Frames, sizeof(KeyFrameTrans) * numFrames);
				if (isANP3)
					frames += sizeof(KeyFrameTrans) * numFrames;
				break;
			case 3:
				seq->SetNumFrames(numFrames, false, true, frames);
				RwStreamRead(s, seq->m_Frames, sizeof(KeyFrameCompressed) * numFrames);
				if (isANP3)
					frames += sizeof(KeyFrameCompressed) * numFrames;
				break;
			case 4:
				seq->SetNumFrames(numFrames, true, true, frames);
				RwStreamRead(s, seq->m_Frames, sizeof(KeyFrameTransCompressed) * numFrames);
				if (isANP3)
					frames += sizeof(KeyFrameTransCompressed) * numFrames;
				break;
			default:
				assert(0 && "Invalid FrameType");
			}

			if (isANP3)
				seq->m_bUsingExternalMemory = true;
		}

		if (!hier->m_bIsCompressed) {
			hier->RemoveQuaternionFlips();
			hier->CalcTotalTime();
		}
	}

	ms_numAnimations = ablock->FirstAnimIdx + (int32)numAnims > ms_numAnimations
		? ablock->FirstAnimIdx + (int32)numAnims
		: ms_numAnimations;
}

AnimationId CAnimManager::GetRandomGangTalkAnim()
{
	static const AnimationId gangTalkAnims[] = {
		ANIM_ID_PRTIAL_GNGTLKA,
		ANIM_ID_PRTIAL_GNGTLKB,
		ANIM_ID_PRTIAL_GNGTLKC,
		ANIM_ID_PRTIAL_GNGTLKD,

		ANIM_ID_PRTIAL_GNGTLKE,
		ANIM_ID_PRTIAL_GNGTLKF,
		ANIM_ID_PRTIAL_GNGTLKG,
		ANIM_ID_PRTIAL_GNGTLKH,
	};
	return gangTalkAnims[CGeneral::GetRandomNumberInRange(0, 8)];
}
