// Verify the SA animation association binds hierarchy sequences to clump
// bones by BoneTag (or by bone-name hash), so a ped whose skeleton has
// extra, missing or reordered bones still gets the right per-bone
// animation. The real association/node/clump-data code is linked against
// synthetic clumps; no proprietary game assets are needed.
#include "common.h"

#include "AnimBlendAssociation.h"
#include "AnimBlendNode.h"
#include "AnimBlendClumpData.h"
#include "AnimBlendHierarchy.h"
#include "AnimManager.h"
#include "MemoryMgr.h"
#include "NodeName.h"
#include "eBoneTag.h"
#include "RpAnimBlend.h"
#include "RwHelper.h"

#include <cstdio>
#include <cstdlib>

static void
Check(bool condition, const char *description)
{
	if(!condition){
		fprintf(stderr, "Animation node test failed: %s\n", description);
		abort();
	}
}

// Stand-ins for the engine services used by the code under test. The real
// RpAnimBlend.cpp is linked, so the clump plugin offset is the real global;
// pointing it at the first member of FakeClump makes a plain struct look
// like a clump with RpAnimBlend data attached.
void
re3_assert(const char *expr, const char*, unsigned int, const char*)
{
	fprintf(stderr, "Assertion failed: %s\n", expr);
	abort();
}

void*
RwMallocAlign(uint32 size, uint32 alignment)
{
	void *ptr = nil;
	Check(posix_memalign(&ptr, alignment, size) == 0, "aligned allocation");
	return ptr;
}

void
RwFreeAlign(void *ptr)
{
	free(ptr);
}

// The test hierarchies never own external frame memory, so this is only
// referenced, never called with a live block.
void
MemoryMgrFree(void *ptr)
{
	free(ptr);
}

static int32 numAnimBlockRefs;

void CAnimManager::UncompressAnimation(CAnimBlendHierarchy*) {}
void CAnimManager::RemoveFromUncompressedCache(CAnimBlendHierarchy*) {}
void CAnimManager::AddAnimBlockRef(int32) { numAnimBlockRefs++; }
void CAnimManager::RemoveAnimBlockRef(int32) { numAnimBlockRefs--; }

// The fake clumps in these tests are always skinned ped skeletons, which is
// the branch of RpAnimBlendClumpFindFrameFromHashKey under test.
bool
IsClumpSkinned(RpClump*)
{
	return true;
}

// Only reached for non-skinned clumps, which these tests never build.
char*
GetFrameNodeName(RwFrame*)
{
	static char empty[1] = { 0 };
	return empty;
}

struct FakeClump
{
	CAnimBlendClumpData *plugin;
	CAnimBlendClumpData data;

	FakeClump(int count, const int *boneTags)
	  : plugin(&data)
	{
		data.SetNumberOfBones(count);
		for(int i = 0; i < count; i++){
			data.m_FrameDatas[i].Flags = 0;
			data.m_FrameDatas[i].BoneTag = (uint32)boneTags[i];
		}
	}

	RpClump *Get(void) { return (RpClump*)&plugin; }
};

static CAnimBlendHierarchy*
MakeHierarchy(int numSequences, float totalTime)
{
	// ~CAnimBlendHierarchy() delete[]s m_pSequences, so both the hierarchy
	// and its sequences are heap allocated and released with delete.
	CAnimBlendHierarchy *hier = new CAnimBlendHierarchy;
	hier->m_pSequences = new CAnimBlendSequence[numSequences];
	hier->m_nSeqCount = (uint16)numSequences;
	hier->m_fTotalTime = totalTime;
	return hier;
}

int
main(void)
{
	ClumpOffset = 0;

	// Hierarchy keyed by BoneTag: sequence 0 moves bone 11, sequence 1
	// moves bone 22.
	CAnimBlendHierarchy *hier = MakeHierarchy(2, 1.0f);
	hier->m_pSequences[0].SetBoneTag(11);
	hier->m_pSequences[0].m_FramesNum = 1;
	hier->m_pSequences[1].SetBoneTag(22);
	hier->m_pSequences[1].m_FramesNum = 1;

	static const int refBones[] = { 11, 22 };
	FakeClump reference(2, refBones);
	{
		CAnimBlendAssociation prototype(reference.Get(), hier);
		prototype.m_AnimId = 9;
		prototype.m_AnimGroupId = 4;
		prototype.SetFlag(ANIMATION_IS_PARTIAL);
		Check(prototype.m_NumBlendNodes == 2 &&
			prototype.m_BlendNodes[0].m_Seq == &hier->m_pSequences[0] &&
			prototype.m_BlendNodes[1].m_Seq == &hier->m_pSequences[1],
			"reference mapping");

		// The same hierarchy played on a skeleton with an extra bone and
		// the shared bones in the opposite order must bind by BoneTag,
		// not by node index.
		static const int bigBones[] = { 22, 33, 11 };
		FakeClump larger(3, bigBones);
		CAnimBlendAssociation target(larger.Get(), hier);
		Check(target.m_NumBlendNodes == 3, "target has more frames than reference");
		Check(target.m_BlendNodes[0].m_Seq == &hier->m_pSequences[1] &&
			target.m_BlendNodes[1].m_Seq == nil &&
			target.m_BlendNodes[2].m_Seq == &hier->m_pSequences[0],
			"reordered and extra bones");
		for(int i = 0; i < target.m_NumBlendNodes; i++)
			Check(target.m_BlendNodes[i].m_BlendAssoc == &target,
				"each frame node has its owner");

		// A copy keeps the per-node sequences, re-points the nodes at the
		// copy and holds its own anim block reference.
		{
			CAnimBlendAssociation copied(prototype);
			Check(copied.m_NumBlendNodes == 2 &&
				copied.m_BlendNodes[0].m_Seq == &hier->m_pSequences[0] &&
				copied.m_BlendNodes[1].m_Seq == &hier->m_pSequences[1],
				"copied mapping retained");
			for(int i = 0; i < copied.m_NumBlendNodes; i++)
				Check(copied.m_BlendNodes[i].m_BlendAssoc == &copied,
					"copied nodes point at the copy");
			Check(copied.m_AnimId == 9 && copied.m_AnimGroupId == 4 &&
				(copied.m_Flags & ANIMATION_IS_PARTIAL) != 0,
				"association metadata retained");
			Check(numAnimBlockRefs == 1, "copy holds an anim block reference");
		}
		Check(numAnimBlockRefs == 0, "anim block reference released with the copy");
	}

	// Bones a skeleton does not have are simply left unanimated.
	static const int smallBones[] = { 22 };
	FakeClump smaller(1, smallBones);
	{
		CAnimBlendAssociation target(smaller.Get(), hier);
		Check(target.m_NumBlendNodes == 1 &&
			target.m_BlendNodes[0].m_Seq == &hier->m_pSequences[1],
			"missing bone is skipped");
	}

	// Sequences loaded from an IFP carry a bone-name hash instead of a
	// BoneTag; on a skinned clump they bind through the name SA gives the
	// BoneTag, regardless of the case used in the file.
	CAnimBlendHierarchy *nameHier = MakeHierarchy(1, 1.0f);
	nameHier->m_pSequences[0].SetName("spine1");
	nameHier->m_pSequences[0].m_FramesNum = 1;
	static const int nameBones[] = { BONE_ROOT, BONE_SPINE1, BONE_HEAD };
	FakeClump saPed(3, nameBones);
	{
		CAnimBlendAssociation assoc(saPed.Get(), nameHier);
		Check(assoc.m_NumBlendNodes == 3 &&
			assoc.m_BlendNodes[0].m_Seq == nil &&
			assoc.m_BlendNodes[1].m_Seq == &nameHier->m_pSequences[0] &&
			assoc.m_BlendNodes[2].m_Seq == nil,
			"name-hashed sequence binds through its bone name");
	}

	// A hierarchy with no duration must not spin the wrap loop in
	// SetCurrentTime forever when the animation is not looped, and syncing
	// into it must stay at zero. (The looped zero-duration case cannot
	// occur in SA: such a hierarchy has no keyframes to play.)
	CAnimBlendHierarchy *emptyHier = MakeHierarchy(0, 0.0f);
	{
		CAnimBlendAssociation empty;
		empty.m_BlendHier = emptyHier;
		empty.m_NumBlendNodes = 0;
		empty.SetCurrentTime(1.0f);
		Check(empty.m_CurrentTime == 0.0f, "zero-duration time is clamped");

		CAnimBlendAssociation syncSource;
		syncSource.m_BlendHier = hier;
		syncSource.m_NumBlendNodes = 0;
		syncSource.m_CurrentTime = 0.5f;
		empty.SyncAnimation(&syncSource);
		Check(empty.m_CurrentTime == 0.0f, "sync into zero-duration hierarchy");
	}

	delete hier;
	delete nameHier;
	delete emptyHier;
	puts("Animation skeleton binding and empty clip guards OK");
	return 0;
}
