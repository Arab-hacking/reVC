// Verify CPedModelInfo::DetectSAAnimationClump distinguishes stock/custom GTA VC
// pedestrian skeletons from GTA SA / BR pedestrian skeletons without requiring
// proprietary game assets.
#include "common.h"

#include "Bones.h"
#include "NodeName.h"
#include "PedModelInfo.h"
#include "RwHelper.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

static void
Check(bool condition, const char *description)
{
	if(!condition){
		fprintf(stderr, "Ped skeleton classification test failed: %s\n", description);
		abort();
	}
}

struct FakeFrame {
	char name[32];
	std::vector<FakeFrame*> children;
};

struct FakeClump {
	bool skinned;
	bool hasHierarchy;
	std::vector<int> boneIds;
	FakeFrame *rootFrame;
};

bool
IsClumpSkinned(RpClump *clump)
{
	return clump != nil && ((FakeClump*)clump)->skinned;
}

RpHAnimHierarchy*
GetAnimHierarchyFromSkinClump(RpClump *clump)
{
	FakeClump *fc = (FakeClump*)clump;
	return (fc && fc->hasHierarchy) ? (RpHAnimHierarchy*)fc : nil;
}

RwInt32
RpHAnimIDGetIndex(RpHAnimHierarchy *hierarchy, RwInt32 id)
{
	FakeClump *fc = (FakeClump*)hierarchy;
	if(!fc)
		return -1;
	for(size_t i = 0; i < fc->boneIds.size(); i++)
		if(fc->boneIds[i] == id)
			return (RwInt32)i;
	return -1;
}

RwFrame*
RpClumpGetFrame(const RpClump *clump)
{
	const FakeClump *fc = (const FakeClump*)clump;
	return fc ? (RwFrame*)fc->rootFrame : nil;
}

char*
GetFrameNodeName(RwFrame *frame)
{
	FakeFrame *ff = (FakeFrame*)frame;
	return ff ? ff->name : nil;
}

RwFrame*
RwFrameForAllChildren(RwFrame *frame, RwFrameCallBack callBack, void *data)
{
	FakeFrame *ff = (FakeFrame*)frame;
	if(!ff)
		return nil;
	for(size_t i = 0; i < ff->children.size(); i++)
		if(callBack((RwFrame*)ff->children[i], data) == nil)
			break;
	return frame;
}

static std::vector<int>
MakeCorePedBones(void)
{
	static const int bones[] = {
		BONE_root, BONE_pelvis, BONE_spine, BONE_spine1, BONE_neck, BONE_head,
		BONE_l_clavicle, BONE_l_upperarm, BONE_l_forearm, BONE_l_hand, BONE_l_finger,
		BONE_r_clavicle, BONE_r_upperarm, BONE_r_forearm, BONE_r_hand, BONE_r_finger,
		BONE_l_thigh, BONE_l_calf, BONE_l_foot, BONE_l_toe,
		BONE_r_thigh, BONE_r_calf, BONE_r_foot, BONE_r_toe
	};
	return std::vector<int>(bones, bones + ARRAY_SIZE(bones));
}

int
main(void)
{
	// 1. Stock GTA VC ped DFF (e.g. cop.dff / player.dff): RW 3.4.0.3 (0x34003),
	//    has all 24 standard VC BoneTags (including finger bones 25 & 35) and
	//    VC frame names ("Root", "Pelvis", "Spine", "Bip01 L Clavicle", etc.).
	//    Must NOT be classified as an SA skeleton!
	FakeFrame vcClumpRoot = { "cop", {} };
	FakeFrame vcSkeletonRoot = { "Root", {} };
	FakeFrame vcPelvis = { "Pelvis", {} };
	FakeFrame vcSpine = { "Spine", {} };
	FakeFrame vcSpine1 = { "Spine1", {} };
	FakeFrame vcLClavicle = { "Bip01 L Clavicle", {} };
	FakeFrame vcRClavicle = { "Bip01 R Clavicle", {} };
	vcSpine1.children.push_back(&vcLClavicle);
	vcSpine1.children.push_back(&vcRClavicle);
	vcSpine.children.push_back(&vcSpine1);
	vcPelvis.children.push_back(&vcSpine);
	vcSkeletonRoot.children.push_back(&vcPelvis);
	vcClumpRoot.children.push_back(&vcSkeletonRoot);

	FakeClump vcCop = { true, true, MakeCorePedBones(), &vcClumpRoot };
	Check(!CPedModelInfo::DetectSAAnimationClump((RpClump*)&vcCop, 0x34003),
		"stock VC cop.dff (RW 3.4.0.3) must use native VC animations");
	Check(!CPedModelInfo::DetectSAAnimationClump((RpClump*)&vcCop, 0x33002),
		"stock VC ped (RW 3.3.0.2) must use native VC animations");
	Check(!CPedModelInfo::DetectSAAnimationClump((RpClump*)&vcCop, 0),
		"custom VC-rigged ped without stream version must use native VC animations");

	// 2. GTA SA / BR ped DFF with RW 3.6.0.3 header (0x36003).
	FakeClump saRw36Ped = { true, true, MakeCorePedBones(), &vcClumpRoot };
	Check(CPedModelInfo::DetectSAAnimationClump((RpClump*)&saRw36Ped, 0x36003),
		"RW 3.6.0.3 skinned ped DFF must use SA animations");

	// 3. GTA SA ped re-exported with RW 3.4 header but retaining SA-exclusive
	//    bones such as BONE_BELLY (201) or BONE_JAW (8).
	FakeClump saBoneTagPed = { true, true, MakeCorePedBones(), &vcClumpRoot };
	saBoneTagPed.boneIds.push_back(201); // BONE_BELLY
	Check(CPedModelInfo::DetectSAAnimationClump((RpClump*)&saBoneTagPed, 0x34003),
		"ped with SA-exclusive BoneTag 201 must use SA animations");

	// 4. GTA SA ped re-exported with RW 3.4 header and stripped to 19 core bones,
	//    identified by 3ds Max SA Bip01/Normal hierarchy frame names.
	FakeFrame saClumpRoot = { "skin_292", {} };
	FakeFrame saNormalRoot = { "Normal", {} };
	FakeFrame saBipPelvis = { "Bip01 Pelvis", {} };
	FakeFrame saBipSpine = { "Bip01 Spine", {} };
	saBipPelvis.children.push_back(&saBipSpine);
	saNormalRoot.children.push_back(&saBipPelvis);
	saClumpRoot.children.push_back(&saNormalRoot);

	FakeClump saFrameNamedPed = { true, true, MakeCorePedBones(), &saClumpRoot };
	Check(CPedModelInfo::DetectSAAnimationClump((RpClump*)&saFrameNamedPed, 0x34003),
		"ped with SA Normal/Bip01 Pelvis frame hierarchy must use SA animations");

	// 5. Incomplete or unskinned clumps must never be marked as SA peds even if
	//    their RW stream version is 3.6.0.3 (e.g. broken/partial DFF).
	FakeClump unskinnedRw36 = { false, true, MakeCorePedBones(), &saClumpRoot };
	Check(!CPedModelInfo::DetectSAAnimationClump((RpClump*)&unskinnedRw36, 0x36003),
		"unskinned RW 3.6 clump must not use SA ped animations");

	FakeClump missingBonesRw36 = { true, true, { BONE_root, BONE_pelvis }, &saClumpRoot };
	Check(!CPedModelInfo::DetectSAAnimationClump((RpClump*)&missingBonesRw36, 0x36003),
		"incomplete skeleton missing core limb bones must not use SA ped animations");

	printf("Ped skeleton VC vs SA classification tests OK\n");
	return 0;
}
