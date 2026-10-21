// Reproduce the SA-ped crash with the real animation association/node code,
// synthetic clumps and no proprietary game assets. A reference skeleton with
// two bones must not be blindly copied to a ped with three or reordered bones.
#include "common.h"

#include "AnimBlendAssociation.h"
#include "AnimBlendClumpData.h"
#include "AnimManager.h"
#include "Bones.h"
#include "RpAnimBlend.h"

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

// Stand-ins for the engine services used by the association code under test.
RwInt32 ClumpOffset = 0;

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

int32 CAnimManager::GetAnimationBlockForHierarchy(CAnimBlendHierarchy*) { return -1; }
void CAnimManager::UncompressAnimation(CAnimBlendHierarchy*) { abort(); }
void CAnimManager::AddAnimBlockRef(int32) { abort(); }
void CAnimManager::RemoveAnimBlockRefWithoutDelete(int32) { abort(); }

CAnimBlendClumpData::CAnimBlendClumpData(void)
  : numFrames(0), velocity2d(nil), usesSAAnimations(false), frames(nil)
{
	link.Init();
}
CAnimBlendClumpData::~CAnimBlendClumpData(void) {}

CAnimBlendHierarchy::CAnimBlendHierarchy(void)
  : sequences(nil), numSequences(0), compressed(false), keepCompressed(false), totalLength(1.0f), linkPtr(nil)
{
	name[0] = 0;
}

CAnimBlendSequence::CAnimBlendSequence(void)
  : type(0), numFrames(0), boneTag(-1), keyFrames(nil), keyFramesCompressed(nil)
{
	name[0] = 0;
}
CAnimBlendSequence::~CAnimBlendSequence(void) {}

AnimBlendFrameData*
RpAnimBlendClumpFindBone(RpClump *clump, uint32 boneTag)
{
	CAnimBlendClumpData *data = *RPANIMBLENDCLUMPDATA(clump);
	for(int i = 0; i < data->numFrames; i++)
		if(data->frames[i].nodeID == (int32)boneTag)
			return &data->frames[i];
	return nil;
}

AnimBlendFrameData*
RpAnimBlendClumpFindFrame(RpClump *clump, const char *name)
{
	CAnimBlendClumpData *data = *RPANIMBLENDCLUMPDATA(clump);
	for(int i = 0; i < data->numFrames; i++){
		const char *boneName = ConvertBoneTag2BoneName(data->frames[i].nodeID);
		if(MatchAnimBoneNames(boneName, name))
			return &data->frames[i];
	}
	return nil;
}

// RPANIMBLENDCLUMPDATA only reads the plugin pointer at ClumpOffset; no
// actual RenderWare object needs to be created for these tests.
struct FakeClump
{
	CAnimBlendClumpData *plugin;
	CAnimBlendClumpData data;
	AnimBlendFrameData frames[3];

	FakeClump(int count, int firstBone, int secondBone, int thirdBone = -1)
	  : plugin(&data)
	{
		data.frames = frames;
		data.numFrames = count;
		frames[0].nodeID = firstBone;
		frames[1].nodeID = secondBone;
		frames[2].nodeID = thirdBone;
	}

	RpClump *Get(void) { return (RpClump*)&plugin; }
};

int
main(void)
{
	Check(offsetof(CAnimBlendAssociation, flags) == 0x3E,
		"faulting address 0x3E is the association flags field");

	FakeClump reference(2, 11, 22);
	FakeClump larger(3, 22, 33, 11);
	FakeClump reversed(2, 22, 11);
	FakeClump smaller(1, 22, 11);
	CAnimBlendHierarchy hierarchy;
	CAnimBlendSequence sequences[2];
	sequences[0].boneTag = 11;
	sequences[0].numFrames = 1;
	sequences[1].boneTag = 22;
	sequences[1].numFrames = 1;
	hierarchy.sequences = sequences;
	hierarchy.numSequences = 2;

	CAnimBlendAssociation prototype;
	prototype.Init(reference.Get(), &hierarchy);
	prototype.animId = 9;
	prototype.groupId = 4;
	prototype.flags = ASSOC_PARTIAL;
	Check(prototype.numNodes == 2 && prototype.nodes[0].sequence == &sequences[0] &&
		prototype.nodes[1].sequence == &sequences[1], "reference mapping");

	{
		CAnimBlendAssociation target(prototype, larger.Get());
		Check(target.numNodes == 3, "target has more frames than reference");
		Check(target.nodes[0].sequence == &sequences[1] &&
			target.nodes[1].sequence == nil &&
			target.nodes[2].sequence == &sequences[0], "reordered and extra bones");
		for(int i = 0; i < target.numNodes; i++)
			Check(target.nodes[i].association == &target, "each frame node has its owner");
		Check(target.animId == 9 && target.groupId == 4 && target.flags == ASSOC_PARTIAL,
			"association metadata retained");
	}
	{
		CAnimBlendAssociation target(prototype, reversed.Get());
		Check(target.numNodes == 2 && target.nodes[0].sequence == &sequences[1] &&
			target.nodes[1].sequence == &sequences[0], "reorder even when counts match");
		target.BindNodesToClump(larger.Get());
		Check(target.numNodes == 3 && target.nodes[2].association == &target,
			"legacy association can be rebound safely");
	}
	{
		CAnimBlendAssociation target(prototype, smaller.Get());
		Check(target.numNodes == 1 && target.nodes[0].sequence == &sequences[1],
			"missing bone is skipped");
	}
	Check(MatchAnimBoneNames("Spine1", " Spine 1") &&
		MatchAnimBoneNames("Bip01 L Clavicle", " L Clavicle") &&
		MatchAnimBoneNames(ConvertBoneTag2BoneName(BONE_l_toe), " L Toe0") &&
		MatchAnimBoneNames(ConvertBoneTag2BoneName(BONE_r_finger01), " R Finger01") &&
		!MatchAnimBoneNames("Spine1", "Spine2") &&
		!MatchAnimBoneNames("R Hand", "L Hand"),
		"SA bone-name fallback ignores exporter spacing without swapping limbs");
	FakeClump saPed(3, BONE_root, BONE_spine1, BONE_l_clavicle);
	CAnimBlendHierarchy hiAlign;
	CAnimBlendSequence alignSequences[3];
	strcpy(alignSequences[0].name, " Root");
	strcpy(alignSequences[1].name, " Spine 1");
	strcpy(alignSequences[2].name, " L Clavicle");
	for(int i = 0; i < 3; i++){
		alignSequences[i].boneTag = -1;
		alignSequences[i].numFrames = 1;
	}
	hiAlign.sequences = alignSequences;
	hiAlign.numSequences = 3;
	CAnimBlendAssociation align;
	align.Init(saPed.Get(), &hiAlign);
	Check(align.nodes[0].sequence == &alignSequences[0] &&
		align.nodes[1].sequence == &alignSequences[1] &&
		align.nodes[2].sequence == &alignSequences[2],
		"CAR_alignHI with every bone ID -1 still animates an SA ped");
	CAnimBlendHierarchy emptyHierarchy;
	emptyHierarchy.totalLength = 0.0f;
	CAnimBlendAssociation empty;
	empty.hierarchy = &emptyHierarchy;
	empty.flags = ASSOC_REPEAT;
	empty.SetCurrentTime(1.0f);
	empty.SyncAnimation(&empty);
	Check(empty.currentTime == 0.0f, "zero-duration repeat and sync cannot loop or divide by zero");
	puts("Animation skeleton rebinding and empty clip guards OK");
	return 0;
}
