#include "common.h"

#include "AnimBlendHierarchy.h"
#include "AnimBlendClumpData.h"
#include "RpAnimBlend.h"
#include "AnimManager.h"
#include "AnimBlendAssociation.h"
#include "MemoryMgr.h"

CAnimBlendAssociation::CAnimBlendAssociation(void)
{
	numNodes = 0;
	groupId = -1;
	nodes = nil;
	hierarchy = nil;
	animBlockRef = -1;
	blendAmount = 1.0f;
	blendDelta = 0.0f;
	currentTime = 0.0f;
	speed = 1.0f;
	timeStep = 0.0f;
	animId = -1;
	flags = 0;
	callbackType = CB_NONE;
	link.Init();
}

CAnimBlendAssociation::CAnimBlendAssociation(CAnimBlendAssociation &other)
{
	nodes = nil;
	animBlockRef = -1;
	blendAmount = 1.0f;
	blendDelta = 0.0f;
	currentTime = 0.0f;
	speed = 1.0f;
	timeStep = 0.0f;
	callbackType = CB_NONE;
	link.Init();
	Init(other);
}

CAnimBlendAssociation::CAnimBlendAssociation(CAnimBlendAssociation &other, RpClump *clump)
{
	nodes = nil;
	animBlockRef = -1;
	blendAmount = 1.0f;
	blendDelta = 0.0f;
	currentTime = 0.0f;
	speed = 1.0f;
	timeStep = 0.0f;
	callbackType = CB_NONE;
	link.Init();
	Init(other, clump);
}

CAnimBlendAssociation::~CAnimBlendAssociation(void)
{
	FreeAnimBlendNodeArray();
	link.Remove();
	if(animBlockRef >= 0){
		int32 block = animBlockRef;
		animBlockRef = -1;
		CAnimManager::RemoveAnimBlockRefWithoutDelete(block);
	}
}


void
CAnimBlendAssociation::AllocateAnimBlendNodeArray(int n)
{
	int i;

	nodes = (CAnimBlendNode*)RwMallocAlign(n*sizeof(CAnimBlendNode), 64);
	for(i = 0; i < n; i++)
		nodes[i].Init();
}

void
CAnimBlendAssociation::FreeAnimBlendNodeArray(void)
{
	if(nodes)
		RwFreeAlign(nodes);
}

void
CAnimBlendAssociation::BindNodesToClump(RpClump *clump)
{
	CAnimBlendClumpData *clumpData = *RPANIMBLENDCLUMPDATA(clump);
	assert(clumpData && clumpData->frames && clumpData->numFrames > 0);
	assert(hierarchy);

	// A template can have fewer bones than a custom ped. FrameUpdate advances
	// a pointer once per frame, so copying the template's nodes here would read
	// past the end of the allocation. Bone order can differ even when the frame
	// counts match; map each sequence to this clump's frames instead.
	FreeAnimBlendNodeArray();
	nodes = nil;
	numNodes = clumpData->numFrames;
	AllocateAnimBlendNodeArray(numNodes);
	for(int i = 0; i < numNodes; i++)
		nodes[i].association = this;

	for(int i = 0; i < hierarchy->numSequences; i++){
		CAnimBlendSequence *seq = &hierarchy->sequences[i];
		AnimBlendFrameData *frame = seq->boneTag == -1 ?
			RpAnimBlendClumpFindFrame(clump, seq->name) :
			RpAnimBlendClumpFindBone(clump, seq->boneTag);
		if(frame && seq->numFrames > 0)
			nodes[frame - clumpData->frames].sequence = seq;
	}
}

void
CAnimBlendAssociation::Init(RpClump *clump, CAnimBlendHierarchy *hier)
{
	animBlockRef = -1;

	// A custom animation set can be missing individual animations.
	if(hier == nil)
		return;

	hierarchy = hier;
	BindNodesToClump(clump);
}

void
CAnimBlendAssociation::Init(CAnimBlendAssociation &assoc, RpClump *clump)
{
	hierarchy = assoc.hierarchy;
	animBlockRef = hierarchy ? CAnimManager::GetAnimationBlockForHierarchy(hierarchy) : -1;
	if(animBlockRef >= 0)
		CAnimManager::AddAnimBlockRef(animBlockRef);
	flags = assoc.flags;
	animId = assoc.animId;
	groupId = assoc.groupId;
	if(clump){
		BindNodesToClump(clump);
	}else{
		// Cutscene/global associations retain the reference skeleton's order.
		numNodes = assoc.numNodes;
		AllocateAnimBlendNodeArray(numNodes);
		for(int i = 0; i < numNodes; i++){
			nodes[i] = assoc.nodes[i];
			nodes[i].association = this;
		}
	}
}

void
CAnimBlendAssociation::SetBlend(float amount, float delta)
{
	blendAmount = amount;
	blendDelta = delta;
}

void
CAnimBlendAssociation::SetFinishCallback(void (*cb)(CAnimBlendAssociation*, void*), void *arg)
{
	callbackType = CB_FINISH;
	callback = cb;
	callbackArg = arg;
}

void
CAnimBlendAssociation::SetDeleteCallback(void (*cb)(CAnimBlendAssociation*, void*), void *arg)
{
	callbackType = CB_DELETE;
	callback = cb;
	callbackArg = arg;
}

void
CAnimBlendAssociation::SetCurrentTime(float time)
{
	int i;

	// A truncated custom clip can have no timed keyframes. Repeating clips
	// must not loop forever subtracting a zero duration.
	if(hierarchy == nil || hierarchy->totalLength <= 0.0f){
		currentTime = 0.0f;
		return;
	}
	for(currentTime = time; currentTime >= hierarchy->totalLength; currentTime -= hierarchy->totalLength)
		if (!IsRepeating()) {
			currentTime = hierarchy->totalLength;
			break;
		}

	CAnimManager::UncompressAnimation(hierarchy);
#ifdef ANIM_COMPRESSION
	// strangely PC has this but android doesn't
	if(hierarchy->keepCompressed){
		for(i = 0; i < numNodes; i++)
			if(nodes[i].sequence)
				nodes[i].SetupKeyFrameCompressed();
	}else
#endif
	{
		for(i = 0; i < numNodes; i++)
			if(nodes[i].sequence)
				nodes[i].FindKeyFrame(currentTime);
	}
}

void
CAnimBlendAssociation::SyncAnimation(CAnimBlendAssociation *other)
{
	if(other == nil || other->hierarchy == nil || other->hierarchy->totalLength <= 0.0f){
		SetCurrentTime(0.0f);
		return;
	}
	SetCurrentTime(other->currentTime/other->hierarchy->totalLength * hierarchy->totalLength);
}

void
CAnimBlendAssociation::Start(float time)
{
	flags |= ASSOC_RUNNING;
	SetCurrentTime(time);
}

void
CAnimBlendAssociation::UpdateTimeStep(float timeDelta, float relSpeed)
{
	if(IsRunning())
		timeStep = (flags & ASSOC_MOVEMENT ? relSpeed*hierarchy->totalLength : speed) * timeDelta;
}

bool
CAnimBlendAssociation::UpdateTime(float timeDelta, float relSpeed)
{
	if(!IsRunning())
		return true;
	if(currentTime >= hierarchy->totalLength){
		flags &= ~ASSOC_RUNNING;
		return true;
	}

	currentTime += timeStep;

	if(currentTime >= hierarchy->totalLength){
		// Ran past end

		if(IsRepeating())
			currentTime -= hierarchy->totalLength;
		else{
			currentTime = hierarchy->totalLength;
			if(flags & ASSOC_FADEOUTWHENDONE){
				flags |= ASSOC_DELETEFADEDOUT;
				blendDelta = -4.0f;
			}
			if(callbackType == CB_FINISH){
				callbackType = CB_NONE;
				callback(this, callbackArg);
			}
		}
	}
	return true;
}

// return whether we still exist after this function
bool
CAnimBlendAssociation::UpdateBlend(float timeDelta)
{
	blendAmount += blendDelta * timeDelta;

	if(blendAmount <= 0.0f && blendDelta < 0.0f){
		// We're faded out and are not fading in
		blendAmount = 0.0f;
		blendDelta = Max(0.0f, blendDelta);
		if(flags & ASSOC_DELETEFADEDOUT){
			if(callbackType == CB_FINISH || callbackType == CB_DELETE)
				callback(this, callbackArg);
			delete this;
			return false;
		}
	}

	if(blendAmount > 1.0f){
		// Maximally faded in, clamp values
		blendAmount = 1.0f;
		blendDelta = Min(0.0f, blendDelta);
	}

	return true;
}
