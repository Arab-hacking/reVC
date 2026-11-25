#include "common.h"

#include "RpAnimBlend.h"
#include "AnimBlendClumpData.h"
#include "AnimBlendAssociation.h"
#include "AnimBlendHierarchy.h"
#include "AnimBlendNode.h"
#include "AnimManager.h"
#include "KeyGen.h"
#include "RwHelper.h"
#include "NodeName.h"
#include "VisibilityPlugins.h"
#include "PedNode.h"

RwInt32 ClumpOffset;

CAnimBlendClumpData *&RpAnimBlendClumpGetDataRef(RpClump *clump)
{
	return *RWPLUGINOFFSET(CAnimBlendClumpData*, clump, ClumpOffset);
}

// 0x4D5F40
void *ClumpAnimConstruct(void *object, RwInt32 offsetInObject, RwInt32 sizeInObject)
{
	RpClump *clump = (RpClump *)object;
	*RWPLUGINOFFSET(CAnimBlendClumpData*, clump, offsetInObject) = nil;
	return object;
}

// 0x4D6110
void *ClumpAnimDestruct(void *object, RwInt32 offsetInObject, RwInt32 sizeInObject)
{
	RpClump *clump = (RpClump *)object;
	CAnimBlendClumpData *&bd = *RWPLUGINOFFSET(CAnimBlendClumpData*, clump, offsetInObject);
	if (bd) {
		RpAnimBlendClumpRemoveAllAssociations(clump);
		delete bd;
		bd = nil;
	}
	return object;
}

// 0x4D5F90
void *ClumpAnimCopy(void *dstObject, const void *srcObject, RwInt32 offsetInObject, RwInt32 sizeInObject)
{
	return nil;
}

// 0x4D5FA0
void RtAnimBlendKeyFrameApply(void *voidMat, void *voidFrame) // Semantically same as the original `RpHAnimBlendKeyFrameApply`
{
	RwMatrix *mat = (RwMatrix *)voidMat;
	RpHAnimBlendInterpFrame *frame = (RpHAnimBlendInterpFrame *)voidFrame;

	CQuaternion q;
	q.x = frame->q.x;
	q.y = frame->q.y;
	q.z = frame->q.z;
	q.w = frame->q.w;
	q.Get(mat);
	RwV3dAssign(RwMatrixGetPos(mat), &frame->t);
}

// 0x4D60C0
void RpAnimBlendKeyFrameInterpolate(void *voidOut, void *voidIn1, void *voidIn2, float time, void *customData)
{
	RpHAnimBlendInterpFrame *out = (RpHAnimBlendInterpFrame *)voidOut;
	out->q.x = out->q.y = out->q.z = 0.0f;
	out->q.w = 0.0f;
	out->t.x = out->t.y = out->t.z = 0.0f;
}

static void RpHAnimKeyFrameBlend(void *out, void *in1, void *in2, float time)
{
	RpHAnimBlendInterpFrame *o = (RpHAnimBlendInterpFrame *)out;
	RpHAnimBlendKeyFrame *a = (RpHAnimBlendKeyFrame *)in1;
	RpHAnimBlendKeyFrame *b = (RpHAnimBlendKeyFrame *)in2;

	CQuaternion q1(a->q.x, a->q.y, a->q.z, a->q.w);
	CQuaternion q2(b->q.x, b->q.y, b->q.z, b->q.w);
	float theta;
	float dot = DotProduct(q1, q2);
	theta = Acos(dot);
	CQuaternion q;
	if (theta == 0.0f)
		q = q1;
	else
		q.Slerp(q1, q2, theta, Sin(theta), time);

	o->q.x = q.x;
	o->q.y = q.y;
	o->q.z = q.z;
	o->q.w = q.w;
	o->t.x = a->t.x + (b->t.x - a->t.x) * time;
	o->t.y = a->t.y + (b->t.y - a->t.y) * time;
	o->t.z = a->t.z + (b->t.z - a->t.z) * time;
}

static void RpHAnimKeyFrameAdd(void *out, void *in1, void *in2)
{
	RpHAnimBlendKeyFrame *o = (RpHAnimBlendKeyFrame *)out;
	RpHAnimBlendKeyFrame *a = (RpHAnimBlendKeyFrame *)in1;
	RpHAnimBlendKeyFrame *b = (RpHAnimBlendKeyFrame *)in2;

	o->q.x = a->q.x + b->q.x;
	o->q.y = a->q.y + b->q.y;
	o->q.z = a->q.z + b->q.z;
	o->q.w = a->q.w + b->q.w;
	o->t.x = a->t.x + b->t.x;
	o->t.y = a->t.y + b->t.y;
	o->t.z = a->t.z + b->t.z;
}

static void RpHAnimKeyFrameMulRecip(void *frame, void *start)
{
	// Not used by the plugin
}

static void RpHAnimKeyFrameStreamRead(RwStream *stream, RpHAnimAnimation *anim)
{
	RpHAnimBlendKeyFrame *frames = (RpHAnimBlendKeyFrame *)anim->keyframes;
	for (int32 i = 0; i < anim->numFrames; i++) {
		RwStreamRead(stream, &frames[i].time, 4);
		RwStreamRead(stream, &frames[i].q, 4 * 4);
		RwStreamRead(stream, &frames[i].t, 3 * 4);
		int32 prevOff;
		RwStreamRead(stream, &prevOff, 4);
		frames[i].prev = &frames[prevOff / (int32)sizeof(RpHAnimBlendKeyFrame)];
	}
}

static void RpHAnimKeyFrameStreamWrite(RwStream *stream, RpHAnimAnimation *anim)
{
	RpHAnimBlendKeyFrame *frames = (RpHAnimBlendKeyFrame *)anim->keyframes;
	for (int32 i = 0; i < anim->numFrames; i++) {
		RwStreamWrite(stream, &frames[i].time, 4);
		RwStreamWrite(stream, &frames[i].q, 4 * 4);
		RwStreamWrite(stream, &frames[i].t, 3 * 4);
		int32 prevOff = (int32)(frames[i].prev - frames) * (int32)sizeof(RpHAnimBlendKeyFrame);
		RwStreamWrite(stream, &prevOff, 4);
	}
}

static uint32 RpHAnimKeyFrameStreamGetSize(RpHAnimAnimation *anim)
{
	return anim->numFrames * (4 + 4 * 4 + 3 * 4 + 4);
}

// 0x4D6150
bool RpAnimBlendPluginAttach()
{
	ClumpOffset = RpClumpRegisterPlugin(
		sizeof(CAnimBlendClumpData*),
		rwID_RPANIMBLENDPLUGIN,
		&ClumpAnimConstruct,
		&ClumpAnimDestruct,
		&ClumpAnimCopy
	);

	if (ClumpOffset == -1)
		return false;

	static rw::AnimInterpolatorInfo rtInfo;
	rtInfo.id = rwID_RPANIMBLENDPLUGIN;
	rtInfo.interpKeyFrameSize = sizeof(RpHAnimBlendInterpFrame);
	rtInfo.animKeyFrameSize = sizeof(RpHAnimBlendKeyFrame);
	rtInfo.applyCB = RtAnimBlendKeyFrameApply;
	rtInfo.blendCB = RpHAnimKeyFrameBlend;
	rtInfo.interpCB = RpAnimBlendKeyFrameInterpolate;
	rtInfo.addCB = RpHAnimKeyFrameAdd;
	rtInfo.mulRecipCB = RpHAnimKeyFrameMulRecip;
	rtInfo.streamRead = (void (*)(rw::Stream*, rw::Animation*))RpHAnimKeyFrameStreamRead;
	rtInfo.streamWrite = (void (*)(rw::Stream*, rw::Animation*))RpHAnimKeyFrameStreamWrite;
	rtInfo.streamGetSize = (uint32 (*)(rw::Animation*))RpHAnimKeyFrameStreamGetSize;
	rtInfo.customDataSize = 0;
	rw::AnimInterpolatorInfo::registerInterp(&rtInfo);

	return true;
}

// 0x4D5F50
void RpAnimBlendAllocateData(RpClump *clump)
{
	RpAnimBlendClumpGetDataRef(clump) = new CAnimBlendClumpData;
}

// 0x4D6510
void RpAnimBlendClumpInitSkinned(RpClump *clump) // Can't hook, `clump` passed in eax
{
	CAnimBlendClumpData *bd = RpAnimBlendClumpGetData(clump);

	const uint32 MAX_NUM_BONES = 64;

	RpAtomic *firstAtomic = GetFirstAtomic(clump);
	RpGeometry *skinGeo = RpAtomicGetGeometry(firstAtomic);
	RpSkin *skin = RpSkinGeometryGetSkin(skinGeo);
	uint32 nBones = RpSkinGetNumBones(skin);
	RpHAnimHierarchy *rpHAHier = GetAnimHierarchyFromSkinClump(clump);

	bd->SetNumberOfBones(nBones);

	// Get bone positions
	CVector bonePositions[64];
	{ // 0x735360 (SkinGetBonePositionsToTable)
		assert(MAX_NUM_BONES >= nBones);

		bonePositions[0] = CVector(0.0f, 0.0f, 0.0f); // root bone

		uint32 nodeStk[64];
		memset(nodeStk, 0, sizeof(nodeStk));
		uint32 *nodeStkPtr = nodeStk;
		uint32 currNodeIdx = 0;

		for (uint32 i = 1; i < nBones; i++) { // Intentionally starting at 1
			// Calculate inverse matrix of this bone
			RwMatrix invBoneMat;
			RwMatrixInvert(&invBoneMat, &RpSkinGetSkinToBoneMatrices(skin)[i]);

			// Calculate position of this bone
			RwV3dTransformPoints(
				&bonePositions[i],
				RwMatrixGetPos(&invBoneMat), 1,
				&RpSkinGetSkinToBoneMatrices(skin)[currNodeIdx]
			);

			// Handle node stack now
			uint32 nodeFlags = rpHAHier->nodeInfo[i].flags;
			if (nodeFlags & rpHANIMPUSHPARENTMATRIX)
				*++nodeStkPtr = currNodeIdx;
			currNodeIdx = (nodeFlags & rpHANIMPOPPARENTMATRIX)
				? *nodeStkPtr--
				: i;
		}
	}

	// Now, fill in the frame blend data from the positions we've just calculated
	for (uint32 i = 0; i < nBones; i++) {
		AnimBlendFrameData *fd = &bd->m_FrameDatas[i];
		fd->KeyFrame = (RpHAnimBlendInterpFrame *)rpHANIMHIERARCHYGETINTERPFRAME(rpHAHier, i);
		fd->BoneTag = rpHAHier->nodeInfo[i].id;
		fd->BonePos = bonePositions[i];
	}

	// Initialize all frames now (FrameInitCBskin)
	for (uint32 i = 0; i < bd->m_NumFrameData; i++)
		bd->m_FrameDatas[i].Flags = 0;
}

static RwFrame *SetFrameDataFrameCB(RwFrame *f, void *data)
{
	AnimBlendFrameData **fd = (AnimBlendFrameData **)data;
	(*fd)->Frame = f;
	(*fd)++;
	RwFrameForAllChildren(f, SetFrameDataFrameCB, fd);
	return f;
}

// 0x4D6640
void RpAnimBlendClumpInitNonSkinned(RpClump *clump)
{
	CAnimBlendClumpData *bd = RpAnimBlendClumpGetData(clump);

	// Number of bones is the count number of child frames (excluding root)
	bd->SetNumberOfBones(RwFrameCount(RpClumpGetFrame(clump)) - 1);

	// Assign frames to our frame data recursively
	AnimBlendFrameData *fd = bd->m_FrameDatas;
	RwFrameForAllChildren(RpClumpGetFrame(clump), SetFrameDataFrameCB, &fd);

	// Initialize all frames now (FrameInitCBnonskin)
	for (uint32 i = 0; i < bd->m_NumFrameData; i++) {
		AnimBlendFrameData *f = &bd->m_FrameDatas[i];
		f->Flags = 0;
		f->FramePos = *RwMatrixGetPos(RwFrameGetMatrix(f->Frame));
		f->BoneTag = (uint32)BONE_UNKNOWN;
	}
}

// 0x4D6720
void RpAnimBlendClumpInit(RpClump *clump)
{
	RpAnimBlendAllocateData(clump);
	if (IsClumpSkinned(clump))
		RpAnimBlendClumpInitSkinned(clump);
	else
		RpAnimBlendClumpInitNonSkinned(clump);
	RpAnimBlendClumpGetData(clump)->GetRootFrameData().HasVelocity = true;
}

// 0x4D6790
CAnimBlendAssociation *RpAnimBlendClumpAddAssociation(RpClump *clump, CAnimBlendAssociation *association, uint32 playFlags, float startTime, float blendAmount)
{
	// Unused function
	assert(0);
	return nil;
}

CAnimBlendLink &RpAnimBlendClumpGetAssociations(RpClump *clump)
{
	return RpAnimBlendClumpGetData(clump)->m_AnimList;
}

// 0x4D58A0
eBoneTag32 ConvertPedNode2BoneTag(ePedNode pedNode)
{
	switch (pedNode) {
	case PED_NODE_UPPER_TORSO:     return BONE_SPINE1;
	case PED_NODE_HEAD:            return BONE_HEAD;
	case PED_NODE_LEFT_ARM:        return BONE_L_UPPER_ARM;
	case PED_NODE_RIGHT_ARM:       return BONE_R_UPPER_ARM;
	case PED_NODE_LEFT_HAND:       return BONE_L_HAND;
	case PED_NODE_RIGHT_HAND:      return BONE_R_HAND;
	case PED_NODE_LEFT_LEG:        return BONE_L_THIGH;
	case PED_NODE_RIGHT_LEG:       return BONE_R_THIGH;
	case PED_NODE_LEFT_FOOT:       return BONE_L_FOOT;
	case PED_NODE_RIGHT_FOOT:      return BONE_R_FOOT;
	case PED_NODE_RIGHT_LOWER_LEG: return BONE_R_CALF;
	case PED_NODE_LEFT_LOWER_LEG:  return BONE_L_CALF;
	case PED_NODE_LEFT_LOWER_ARM:  return BONE_L_FORE_ARM;
	case PED_NODE_RIGHT_LOWER_ARM: return BONE_R_FORE_ARM;
	case PED_NODE_LEFT_CLAVICLE:   return BONE_L_CLAVICLE;
	case PED_NODE_RIGHT_CLAVICLE:  return BONE_R_CLAVICLE;
	case PED_NODE_NECK:            return BONE_NECK;
	case PED_NODE_JAW:             return BONE_JAW;
	default:                       return BONE_UNKNOWN;
	}
}

// 0x4D56F0
const char *ConvertBoneTag2BoneName(eBoneTag32 boneTag)
{
	switch (boneTag) {
	case BONE_R_BREAST:    return "R Breast";
	case BONE_L_BREAST:    return "L Breast";
	case BONE_BELLY:       return "Belly";
	case BONE_ROOT:        return "Root";
	case BONE_PELVIS:      return "Pelvis";
	case BONE_SPINE:       return "Spine";
	case BONE_SPINE1:      return "Spine1";
	case BONE_NECK:        return "Neck";
	case BONE_HEAD:        return "Head";
	case BONE_L_BROW:      return "L Brow";
	case BONE_R_BROW:      return "R Brow";
	case BONE_JAW:         return "Jaw";
	case BONE_R_CLAVICLE:  return "Bip01 R Clavicle";
	case BONE_R_UPPER_ARM: return "R UpperArm";
	case BONE_R_FORE_ARM:  return "R Forearm";
	case BONE_R_HAND:      return "R Hand";
	case BONE_R_FINGER:    return "R Fingers";
	case BONE_R_FINGER_01: return "R Finger01";
	case BONE_L_CLAVICLE:  return "Bip01 L Clavicle";
	case BONE_L_UPPER_ARM: return "L UpperArm";
	case BONE_L_FORE_ARM:  return "L Forearm";
	case BONE_L_HAND:      return "L Hand";
	case BONE_L_FINGER:    return "L Fingers";
	case BONE_L_FINGER_01: return "L Finger01";
	case BONE_L_THIGH:     return "L Thigh";
	case BONE_L_CALF:      return "L Calf";
	case BONE_L_FOOT:      return "L Foot";
	case BONE_L_TOE_0:     return "L Toe";
	case BONE_R_THIGH:     return "R Thigh";
	case BONE_R_CALF:      return "R Calf";
	case BONE_R_FOOT:      return "R Foot";
	case BONE_R_TOE_0:     return "R Toe";
	default:               return nil;
	}
}

// 0x4D6BE0
CAnimBlendAssociation *RpAnimBlendClumpExtractAssociations(RpClump *clump)
{
	CAnimBlendClumpData *bd = RpAnimBlendClumpGetData(clump);
	CAnimBlendLink *head = bd->m_AnimList.next;
	bd->m_AnimList.next = nil;
	return head ? CAnimBlendAssociation::FromLink(head) : nil;
}

// 0x4D6C30
void RpAnimBlendClumpGiveAssociations(RpClump *clump, CAnimBlendAssociation *associations)
{
	CAnimBlendClumpData *bd = RpAnimBlendClumpGetData(clump);

	// Delete all animations of this clump
	RpAnimBlendClumpRemoveAllAssociations(clump);

	// Use new list of associations
	bd->m_AnimList.next = &associations->m_Link;
	associations->m_Link.prev = &bd->m_AnimList;
}

// 0x4D64A0
void RpAnimBlendClumpFillFrameArray(RpClump *clump, AnimBlendFrameData **outFrameData)
{
	CAnimBlendClumpData *bd = RpAnimBlendClumpGetData(clump);

	if (IsClumpSkinned(clump)) { // 0x4D6450 (FillFrameArrayIndiciesSkinned)
		RpHAnimHierarchy *ah = GetAnimHierarchyFromClump(clump);
		for (int32 i = PED_NODE_UPPER_TORSO; i < TOTAL_PED_NODES; i++)
			outFrameData[i] = &bd->m_FrameDatas[RpHAnimIDGetIndex(ah, ConvertPedNode2BoneTag((ePedNode)i))];
	} else { // 0x4D6430 (FillFrameArrayIndiciesNonSkinned)
		for (uint32 i = 0; i < bd->m_NumFrameData; i++) {
			AnimBlendFrameData *fd = &bd->m_FrameDatas[i];
			outFrameData[CVisibilityPlugins::GetFrameHierarchyId(fd->Frame)] = fd;
		}
	}
}

// 0x4D6400
AnimBlendFrameData *RpAnimBlendClumpFindBone(RpClump *clump, eBoneTag32 boneTag)
{
	CAnimBlendClumpData *bd = RpAnimBlendClumpGetData(clump);
	AnimBlendFrameData *ret = nil;
	for (uint32 i = 0; i < bd->m_NumFrameData; i++) {
		AnimBlendFrameData *f = &bd->m_FrameDatas[i];
		if (f->BoneTag == boneTag)
			ret = f;
	}
	return ret;
}

// 0x4D62A0
AnimBlendFrameData *RpAnimBlendClumpFindFrame(RpClump *clump, const char *needle)
{
	CAnimBlendClumpData *bd = RpAnimBlendClumpGetData(clump);
	AnimBlendFrameData *ret = nil;
	if (IsClumpSkinned(clump)) { // 0x4D6240
		for (uint32 i = 0; i < bd->m_NumFrameData; i++) {
			AnimBlendFrameData *f = &bd->m_FrameDatas[i];
			const char *boneName = ConvertBoneTag2BoneName((eBoneTag)f->BoneTag);
			if (boneName && !strcasecmp(boneName, needle))
				ret = f;
		}
	} else { // 0x4D6240
		for (uint32 i = 0; i < bd->m_NumFrameData; i++) {
			AnimBlendFrameData *f = &bd->m_FrameDatas[i];
			if (!strcasecmp(GetFrameNodeName(f->Frame), needle))
				ret = f;
		}
	}
	return ret;
}

// 0x4D6370
AnimBlendFrameData *RpAnimBlendClumpFindFrameFromHashKey(RpClump *clump, uint32 boneNameKey)
{
	CAnimBlendClumpData *bd = RpAnimBlendClumpGetData(clump);
	AnimBlendFrameData *ret = nil;
	if (IsClumpSkinned(clump)) { // 0x4D6310
		for (uint32 i = 0; i < bd->m_NumFrameData; i++) {
			AnimBlendFrameData *f = &bd->m_FrameDatas[i];
			const char *boneName = ConvertBoneTag2BoneName((eBoneTag)f->BoneTag);
			if (boneName && boneNameKey == CKeyGen::GetUppercaseKey(boneName))
				ret = f;
		}
	} else { // 0x4D6340
		for (uint32 i = 0; i < bd->m_NumFrameData; i++) {
			AnimBlendFrameData *f = &bd->m_FrameDatas[i];
			if (boneNameKey == CKeyGen::GetUppercaseKey(GetFrameNodeName(f->Frame)))
				ret = f;
		}
	}
	return ret;
}

// Iterate over all associations of a clump.
// The callback shouldn't modify the clump's association list.
template<typename Fn>
static void RpAnimBlendClumpForEachAssociation(RpClump *clump, Fn fn)
{
	CAnimBlendClumpData *bd = RpAnimBlendClumpGetData(clump);
	for (CAnimBlendLink *l = bd->m_AnimList.next; l;) {
		CAnimBlendAssociation *a = CAnimBlendAssociation::FromLink(l);
		l = a->m_Link.next;
		fn(a);
	}
}

template<typename Fn>
static CAnimBlendAssociation *RpAnimBlendClumpFindAssociationIf_N(RpClump *clump, Fn pred, uint32 n)
{
	CAnimBlendClumpData *bd = RpAnimBlendClumpGetData(clump);
	for (CAnimBlendLink *l = bd->m_AnimList.next; l;) {
		CAnimBlendAssociation *a = CAnimBlendAssociation::FromLink(l);
		if (pred(*a)) {
			if (n-- == 0)
				return a;
		}
		l = a->m_Link.next;
	}
	return nil;
}

// 0x4D68E0
CAnimBlendAssociation *RpAnimBlendClumpGetAssociation(RpClump *clump, bool, CAnimBlendHierarchy *h)
{
	CAnimBlendClumpData *bd = RpAnimBlendClumpGetData(clump);
	for (CAnimBlendLink *l = bd->m_AnimList.next; l;) {
		CAnimBlendAssociation *a = CAnimBlendAssociation::FromLink(l);
		if (a->m_BlendHier == h)
			return a;
		l = a->m_Link.next;
	}
	return nil;
}

// 0x4D6870
CAnimBlendAssociation *RpAnimBlendClumpGetAssociation(RpClump *clump, const char *name)
{
	uint32 nameKey = CKeyGen::GetUppercaseKey(name);
	CAnimBlendClumpData *bd = RpAnimBlendClumpGetData(clump);
	for (CAnimBlendLink *l = bd->m_AnimList.next; l;) {
		CAnimBlendAssociation *a = CAnimBlendAssociation::FromLink(l);
		if (a->m_BlendHier && a->m_BlendHier->m_hashKey == nameKey)
			return a;
		l = a->m_Link.next;
	}
	return nil;
}

// 0x4D68B0
CAnimBlendAssociation *RpAnimBlendClumpGetAssociation(RpClump *clump, uint32 animId)
{
	CAnimBlendClumpData *bd = RpAnimBlendClumpGetData(clump);
	for (CAnimBlendLink *l = bd->m_AnimList.next; l;) {
		CAnimBlendAssociation *a = CAnimBlendAssociation::FromLink(l);
		if (a->m_AnimId == (int16)animId)
			return a;
		l = a->m_Link.next;
	}
	return nil;
}

// 0x4D15E0
CAnimBlendAssociation *RpAnimBlendClumpGetFirstAssociation(RpClump *clump)
{
	CAnimBlendClumpData *bd = RpAnimBlendClumpGetData(clump);
	return RpAnimBlendClumpIsInitialized(clump) && bd->m_AnimList.next
		? CAnimBlendAssociation::FromLink(bd->m_AnimList.next)
		: nil;
}

// 0x4D6A70
CAnimBlendAssociation *RpAnimBlendClumpGetFirstAssociation(RpClump *clump, uint32 flags)
{
	CAnimBlendClumpData *bd = RpAnimBlendClumpGetData(clump);
	for (CAnimBlendLink *l = bd->m_AnimList.next; l;) {
		CAnimBlendAssociation *a = CAnimBlendAssociation::FromLink(l);
		if (a->m_Flags & flags)
			return a;
		l = a->m_Link.next;
	}
	return nil;
}

// 0x4D6910
CAnimBlendAssociation *RpAnimBlendClumpGetMainAssociation(RpClump *clump, CAnimBlendAssociation **pp2ndAnim, float *pBlendVal2nd)
{
	CAnimBlendAssociation *aA = nil, *aB = nil;
	float bA = 0.0f, bB = 0.0f;

	CAnimBlendClumpData *bd = RpAnimBlendClumpGetData(clump);
	for (CAnimBlendLink *l = bd->m_AnimList.next; l;) {
		CAnimBlendAssociation *a = CAnimBlendAssociation::FromLink(l);
		l = a->m_Link.next;
		if (a->IsPartial())
			continue;
		float blend = a->m_BlendAmount;
		if (blend > bA) { // Found a new main?
			aB = aA; bB = bA;
			aA = a;  bA = blend;
		} else if (blend > bB) { // Found a new secondary?
			aB = a;
			bB = blend;
		}
	}

	if (pp2ndAnim)
		*pp2ndAnim = aB;
	if (pBlendVal2nd)
		*pBlendVal2nd = bB;

	return aA;
}

// 0x4D6A30
CAnimBlendAssociation *RpAnimBlendClumpGetMainAssociation_N(RpClump *clump, uint32 n)
{
	CAnimBlendClumpData *bd = RpAnimBlendClumpGetData(clump);
	for (CAnimBlendLink *l = bd->m_AnimList.next; l;) {
		CAnimBlendAssociation *a = CAnimBlendAssociation::FromLink(l);
		if (!a->IsPartial()) {
			if (n-- == 0)
				return a;
		}
		l = a->m_Link.next;
	}
	return nil;
}

// 0x4D69A0
CAnimBlendAssociation *RpAnimBlendClumpGetMainPartialAssociation(RpClump *clump)
{
	CAnimBlendAssociation *mA = nil;
	float mB = 0.0f;
	CAnimBlendClumpData *bd = RpAnimBlendClumpGetData(clump);
	for (CAnimBlendLink *l = bd->m_AnimList.next; l;) {
		CAnimBlendAssociation *a = CAnimBlendAssociation::FromLink(l);
		l = a->m_Link.next;
		if (!a->IsPartial())
			continue;
		if (a->m_BlendAmount > mB) {
			mA = a;
			mB = a->m_BlendAmount;
		}
	}
	return mA;
}

// 0x4D69F0
CAnimBlendAssociation *RpAnimBlendClumpGetMainPartialAssociation_N(RpClump *clump, int32 n)
{
	CAnimBlendClumpData *bd = RpAnimBlendClumpGetData(clump);
	for (CAnimBlendLink *l = bd->m_AnimList.next; l;) {
		CAnimBlendAssociation *a = CAnimBlendAssociation::FromLink(l);
		if (a->IsPartial()) {
			if (n-- == 0)
				return a;
		}
		l = a->m_Link.next;
	}
	return nil;
}

static uint32 CountAssociations(RpClump *clump, bool wantPartial, bool countAll)
{
	uint32 count = 0;
	CAnimBlendClumpData *bd = RpAnimBlendClumpGetData(clump);
	for (CAnimBlendLink *l = bd->m_AnimList.next; l;) {
		CAnimBlendAssociation *a = CAnimBlendAssociation::FromLink(l);
		if (countAll || a->IsPartial() == wantPartial)
			count++;
		l = a->m_Link.next;
	}
	return count;
}

// 0x4D6B60
uint32 RpAnimBlendClumpGetNumAssociations(RpClump *clump)
{
	return CountAssociations(clump, false, true);
}

// 0x4D6BB0
uint32 RpAnimBlendClumpGetNumNonPartialAssociations(RpClump *clump)
{
	return CountAssociations(clump, false, false);
}

// 0x4D6B80
uint32 RpAnimBlendClumpGetNumPartialAssociations(RpClump *clump)
{
	return CountAssociations(clump, true, false);
}

// 0x4D6760
bool RpAnimBlendClumpIsInitialized(RpClump *clump)
{
	CAnimBlendClumpData *bd = RpAnimBlendClumpGetData(clump);
	return bd && bd->m_NumFrameData;
}

// 0x4D6B00
void RpAnimBlendClumpPauseAllAnimations(RpClump *clump)
{
	CAnimBlendClumpData *bd = RpAnimBlendClumpGetData(clump);
	for (CAnimBlendLink *l = bd->m_AnimList.next; l;) {
		CAnimBlendAssociation *a = CAnimBlendAssociation::FromLink(l);
		a->m_Flags &= ~ANIMATION_IS_PLAYING;
		l = a->m_Link.next;
	}
}

// 0x4D6B30
void RpAnimBlendClumpUnPauseAllAnimations(RpClump *clump)
{
	CAnimBlendClumpData *bd = RpAnimBlendClumpGetData(clump);
	for (CAnimBlendLink *l = bd->m_AnimList.next; l;) {
		CAnimBlendAssociation *a = CAnimBlendAssociation::FromLink(l);
		a->m_Flags |= ANIMATION_IS_PLAYING;
		l = a->m_Link.next;
	}
}

// 0x4D6C00
void RpAnimBlendClumpRemoveAllAssociations(RpClump *clump)
{
	CAnimBlendClumpData *bd = RpAnimBlendClumpGetData(clump);
	for (CAnimBlendLink *l = bd->m_AnimList.next; l;) {
		CAnimBlendAssociation *a = CAnimBlendAssociation::FromLink(l);
		l = a->m_Link.next; // Mustn't use a range loop here, because it doesn't pre-cache `next`
		delete a;
	}
}

// 0x4D6820
void RpAnimBlendClumpRemoveAssociations(RpClump *clump, uint32 flags)
{
	CAnimBlendClumpData *bd = RpAnimBlendClumpGetData(clump);
	for (CAnimBlendLink *l = bd->m_AnimList.next; l;) {
		CAnimBlendAssociation *a = CAnimBlendAssociation::FromLink(l);
		l = a->m_Link.next;
		if (!flags || (a->m_Flags & flags))
			delete a;
	}
}

// 0x4D67E0
void RpAnimBlendClumpSetBlendDeltas(RpClump *clump, uint32 flags, float delta)
{
	CAnimBlendClumpData *bd = RpAnimBlendClumpGetData(clump);
	for (CAnimBlendLink *l = bd->m_AnimList.next; l;) {
		CAnimBlendAssociation *a = CAnimBlendAssociation::FromLink(l);
		l = a->m_Link.next;
		if (!flags || (a->m_Flags & flags))
			a->SetBlendDelta(delta);
	}
}

struct AnimBlendUpdateData { // OG name
	bool32 IncludePartial;               //!< Has non-moving anim (Eg.: Anim with MOVEMENT flag NOT set)
	CAnimBlendNode *BlendNodeArrays[12]; //!< Null terminated array of arrays.
	                                     //!< Each entry is incremented on each call of the appropriate `FrameUpdateCallBack`
	                                     //!< So that on every call they point to the blend node of the given frame
	                                     //!< (This works because the sequences are sorted the same way as the frames appear in the clump)
};

static CAnimBlendClumpData *gpAnimBlendClump; // 0xB4EA0C

static float CalculateTotalBlendOfPartial(AnimBlendUpdateData *c, AnimBlendFrameData *fd, bool checkDontAddToBlend)
{
	float sum = 0.0f;
	if (c->IncludePartial) {
		for (CAnimBlendNode **it = c->BlendNodeArrays; *it; it++) {
			CAnimBlendNode *node = *it;
			if (!node->IsValid())
				continue;
			CAnimBlendAssociation *assoc = node->m_BlendAssoc;
			if (assoc->IsPartial() && (!checkDontAddToBlend || !(assoc->m_Flags & ANIMATION_DONT_ADD_TO_PARTIAL_BLEND)))
				sum += assoc->m_BlendAmount;
		}
	}
	return sum;
}

// Whether this <IsCompressed, IsSkinned, ExtractVelocity, Extract3DVelocity> combination
// needs the quaternion sign flip fix when accumulating rotations.
template<bool IsCompressed, bool IsSkinned, bool ExtractVelocity, bool Extract3DVelocity>
struct NeedsRemoveQuatFlips
{
	enum { value = false };
};
template<> struct NeedsRemoveQuatFlips<true, true, false, false>  { enum { value = true }; }; // FrameUpdateCallBackCompressedSkinned
template<> struct NeedsRemoveQuatFlips<false, true, false, false> { enum { value = true }; }; // FrameUpdateCallBackSkinned
template<> struct NeedsRemoveQuatFlips<false, true, true, false>  { enum { value = true }; }; // FrameUpdateCallBackSkinnedWithVelocityExtraction
template<> struct NeedsRemoveQuatFlips<false, false, true, false> { enum { value = true }; }; // FrameUpdateCallBackNonSkinnedWithVelocityExtraction

static CVector lerpVec(const CVector &a, const CVector &b, float t)
{
	return CVector(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t);
}

/*!
 * @brief Copy-paste eliminated per-tick frame update
 * @tparam IsCompressed If the anim data is compressed
 * @tparam IsSkinned If the frame is skinned or not
 * @tparam ExtractVelocity Whenever to use the translation to update the ped's position
 * @tparam Extract3DVelocity Whenever to use Z translation too for updating the ped's position
 * @param fd Frame Data
 * @param c Context
 */
template<bool IsCompressed, bool IsSkinned, bool ExtractVelocity, bool Extract3DVelocity>
static void FrameUpdateCallBackT(AnimBlendFrameData *fd, AnimBlendUpdateData *c)
{
	const float partialScale = 1.0f - CalculateTotalBlendOfPartial(c, fd, true);

	const bool IsFixBugs = true;

	// (if `ExtractVelocity`): Extract current velocity
	CVector currV(0.0f, 0.0f, 0.0f); // Current velocity (before update), Z only used if `ExtractZVelocity`
	if (ExtractVelocity) {
		for (CAnimBlendNode **it = c->BlendNodeArrays; *it; it++) {
			CAnimBlendNode *node = *it;
			if (!node->IsValid() || !node->GetRootKF()->HasTranslation())
				continue;
			CAnimBlendAssociation *assoc = node->m_BlendAssoc;
			if (!(assoc->m_Flags & ANIMATION_CAN_EXTRACT_VELOCITY))
				continue;
			if (IsFixBugs || !Extract3DVelocity) {
				if (assoc->m_Flags & ANIMATION_IGNORE_ROOT_TRANSLATION) // BUGFIX: Check this flag for 3D too (Like they did for 2D)
					continue;
			}
			CVector t;
			node->I_GetCurrentTranslation<IsCompressed>(t, partialScale);
			// GetVelocityFromTranslation
			currV.x += (!IsFixBugs && Extract3DVelocity || (assoc->m_Flags & ANIMATION_CAN_EXTRACT_X_VELOCITY)) // BUGFIX: Check this flag for 3D too (Like they did for 2D)
				? t.x : 0.0f;
			currV.y += t.y;
			currV.z += Extract3DVelocity ? t.z : 0.0f;
		}
	}

	CQuaternion nextQ(0.0f, 0.0f, 0.0f, 0.0f); // Rotation after update
	CVector nextT(0.0f, 0.0f, 0.0f);            // Translation after update

	// if `ExtractVelocity`:
	CVector nextV(0.0f, 0.0f, 0.0f), loopedNextV(0.0f, 0.0f, 0.0f); // Velocity after update, Z only used if `ExtractZVelocity`
	bool hasLoopedVelocity = false;
	// else:
	float nextBlendT = 0.0f; // Key-frame blend

	for (CAnimBlendNode **it = c->BlendNodeArrays; *it; ++*it, it++) { // NOTE: Increments NodeArray pointer too!
		CAnimBlendNode *node = *it;
		if (!node->IsValid())
			continue;
		CAnimBlendAssociation *assoc = node->m_BlendAssoc;

		const float blendScale = partialScale;
		CVector t;
		CQuaternion q;
		bool looped = node->I_Update<IsCompressed>(t, q, blendScale);

		// NOTE: Not sure if doing this at all is necessary, but let's keep it the way it was.
		if (NeedsRemoveQuatFlips<IsCompressed, IsSkinned, ExtractVelocity, Extract3DVelocity>::value)
			nextQ = nextQ + (DotProduct(q, nextQ) >= 0.0f ? q : -q);
		else
			nextQ = nextQ + q;

		if (!node->GetRootKF()->HasTranslation())
			continue;

		if (IsFixBugs || (ExtractVelocity && !Extract3DVelocity)) {
			if (assoc->m_Flags & ANIMATION_IGNORE_ROOT_TRANSLATION) // BUGFIX: Check this flag for 3D too (Like they did for 2D)
				continue;
		}

		nextT += t;
		if (!ExtractVelocity)
			nextBlendT += assoc->m_BlendAmount;

		if (!ExtractVelocity)
			continue;

		if (!(assoc->m_Flags & ANIMATION_CAN_EXTRACT_VELOCITY))
			continue;

		CVector cT;
		node->I_GetCurrentTranslation<IsCompressed>(cT, partialScale);
		// GetVelocityFromTranslation
		nextV.x += (!IsFixBugs && Extract3DVelocity || (assoc->m_Flags & ANIMATION_CAN_EXTRACT_X_VELOCITY)) ? cT.x : 0.0f;
		nextV.y += cT.y;
		nextV.z += Extract3DVelocity ? cT.z : 0.0f;

		if ((hasLoopedVelocity = hasLoopedVelocity || looped)) {
			CVector eT;
			node->I_GetEndTranslation<IsCompressed>(eT, blendScale);
			// GetVelocityFromTranslation
			loopedNextV.x += (!IsFixBugs && Extract3DVelocity || (assoc->m_Flags & ANIMATION_CAN_EXTRACT_X_VELOCITY)) ? eT.x : 0.0f;
			loopedNextV.y += eT.y;
			loopedNextV.z += Extract3DVelocity ? eT.z : 0.0f;
		}
	}

	RwMatrix *fmat = IsSkinned ? nil : RwFrameGetMatrix(fd->Frame);

	if (!fd->KeyFramesIgnoreNodeOrientation) {
		nextQ.Normalise();
		if (IsSkinned) {
			fd->KeyFrame->q.x = nextQ.x;
			fd->KeyFrame->q.y = nextQ.y;
			fd->KeyFrame->q.z = nextQ.z;
			fd->KeyFrame->q.w = nextQ.w;
		} else {
			RwMatrixSetIdentity(fmat);
			nextQ.Get(fmat); // Set delta rotation as the rotation of the frame
		}
	}

	if (!fd->KeyFramesIgnoreNodeTranslation) {
		// Apply velocity
		if (ExtractVelocity) {
			CVector *wsPos = gpAnimBlendClump->m_PedPosition;
			wsPos->x = nextV.x - currV.x;
			wsPos->y = nextV.y - currV.y;
			if (Extract3DVelocity)
				wsPos->z = nextV.z - currV.z;
			if (hasLoopedVelocity) {
				wsPos->x += loopedNextV.x;
				wsPos->y += loopedNextV.y;
				if (Extract3DVelocity)
					wsPos->z += loopedNextV.z;
			}
		}

		// Apply translation
		const CVector &nodePos = IsSkinned ? fd->BonePos : fd->FramePos;
		if (ExtractVelocity) {
			if (IsSkinned) {
				fd->KeyFrame->t.x = nextT.x - nextV.x + nodePos.x;
				fd->KeyFrame->t.y = nextT.y - nextV.y + nodePos.y;
				if (Extract3DVelocity) {
					fd->KeyFrame->t.z = nextT.z - nextV.z + nodePos.z;
				} else if (!IsCompressed) { // FrameUpdateCallBackSkinnedWithVelocityExtraction
					fd->KeyFrame->t.z = nextT.z;
					if (fd->KeyFrame->t.z >= -0.8f) {
						fd->KeyFrame->t.z += fd->KeyFrame->t.z >= -0.4f
							? fd->BonePos.z
							: (fd->KeyFrame->t.z * 2.5f + 2.0f) * fd->BonePos.z;
					}
				} // otherwise `z` remains untouched
			} else {
				RwV3d *t = RwMatrixGetPos(fmat);
				t->x = nextT.x - nextV.x + nodePos.x;
				t->y = nextT.y - nextV.y + nodePos.y;
				if (Extract3DVelocity)
					t->z = nextT.z - nextV.z + nodePos.z;
			}
		} else {
			CVector blended = lerpVec(nodePos, nextT, nextBlendT);
			if (IsSkinned)
				fd->KeyFrame->t = blended;
			else
				RwV3dAssign(RwMatrixGetPos(fmat), &blended);
		}
	}

	if (!IsSkinned)
		RwMatrixUpdate(fmat);
}

/*!
 * @brief Per-tick frame update wrapper
 * @tparam IsCompressed If the anim data is compressed
 * @tparam IsSkinned If the frame is skinned or not
 * @param fd Frame Data
 * @param data Context (Real type: `AnimBlendUpdateData*`)
 */
template<bool IsCompressed, bool IsSkinned>
static void FrameUpdateCallBackW(AnimBlendFrameData *fd, void *data)
{
	AnimBlendUpdateData *c = (AnimBlendUpdateData *)data;
	if (fd->HasVelocity && gpAnimBlendClump->m_PedPosition) {
		if (fd->HasZVelocity) // NOTE: Originally compressed anims immediately did 3D velocity, so this flag may not be set on compressed anims?
			FrameUpdateCallBackT<IsCompressed, IsSkinned, true, true>(fd, c);
		else
			FrameUpdateCallBackT<IsCompressed, IsSkinned, true, false>(fd, c);
	} else {
		FrameUpdateCallBackT<IsCompressed, IsSkinned, false, false>(fd, c);
	}
}

// 0x4D2E10
static void FrameUpdateCallBackOffscreen(AnimBlendFrameData *fd, void *data)
{
	AnimBlendUpdateData *c = (AnimBlendUpdateData *)data;
	if (fd->HasVelocity && gpAnimBlendClump->m_PedPosition)
		FrameUpdateCallBackT<false, true, true, false>(fd, c);
}

// 0x4D1570
static void RpAnimBlendNodeUpdateKeyFrames(AnimBlendUpdateData *c, AnimBlendFrameData *frames, uint32 nFrames)
{
	for (CAnimBlendNode **it = c->BlendNodeArrays; *it; it++) {
		CAnimBlendNode *nodeArray = *it; // #nodes == nFrames
		CAnimBlendAssociation *assoc = nodeArray[0].m_BlendAssoc;
		for (uint32 frameN = 0; frameN < nFrames; frameN++) {
			AnimBlendFrameData *fd = &frames[frameN];
			if (fd->HasVelocity && gpAnimBlendClump->m_PedPosition)
				continue;
			CAnimBlendNode *node = &nodeArray[frameN];
			if (!node->IsValid())
				continue;
			node->FindKeyFrame(assoc->m_CurrentTime);
		}
	}
}

// 0x4D34F0
void RpAnimBlendClumpUpdateAnimations(RpClump *clump, float timeStep, bool isOnScreen)
{
	CAnimBlendClumpData *bd = RpAnimBlendClumpGetData(clump);

	if (bd->m_AnimList.IsEmpty())
		return;

	gpAnimBlendClump = bd;

	AnimBlendUpdateData ctx;
	ctx.IncludePartial = false;
	memset(ctx.BlendNodeArrays, 0, sizeof(ctx.BlendNodeArrays));
	uint32 nodesCnt = 0;

	float totalTime = 0.0f, totalBlendAmnt = 0.0f;

	// 0x4D351F - Update blends, collect node arrays
	for (CAnimBlendLink *l = bd->m_AnimList.next; l;) {
		CAnimBlendAssociation *a = CAnimBlendAssociation::FromLink(l);
		l = a->m_Link.next;
		if (!a->UpdateBlend(timeStep))
			continue;
		CAnimBlendHierarchy *ah = a->m_BlendHier;
		if (ah->m_pSequences == nil || ah->m_nSeqCount == 0)
			continue;
		CAnimManager::UncompressAnimation(ah);
		if (nodesCnt + 1 <= (uint32)(sizeof(ctx.BlendNodeArrays) / sizeof(ctx.BlendNodeArrays[0])) - 1) // - 1 for null terminator
			ctx.BlendNodeArrays[nodesCnt++] = a->m_BlendNodes;
		if (a->IsSyncronised()) {
			totalTime += ah->m_fTotalTime / a->m_Speed * a->m_BlendAmount;
			totalBlendAmnt += a->m_BlendAmount;
		} else {
			ctx.IncludePartial = true;
		}
	}
	ctx.BlendNodeArrays[nodesCnt] = nil; // Null terminator

	// 0x4D35A2 - Update animation's timesteps
	float animTimeMult = totalTime == 0.0f
		? 1.0f
		: 1.0f / totalTime * totalBlendAmnt;
	for (CAnimBlendLink *l = bd->m_AnimList.next; l;) {
		CAnimBlendAssociation *a = CAnimBlendAssociation::FromLink(l);
		l = a->m_Link.next;
		a->UpdateTimeStep(timeStep, animTimeMult);
	}

	// 0x4D360E - Update all animations' frames
	AnimBlendFrameData *rootFD = &bd->GetRootFrameData();
	rootFD->IsUpdatingFrame = true;

	if (rootFD->IsCompressed) {
		bd->ForAllFrames(
			IsClumpSkinned(clump)
				? &FrameUpdateCallBackW<true, true>   // FrameUpdateCallBackCompressedSkinned
				: &FrameUpdateCallBackW<true, false>, // FrameUpdateCallBackCompressedNonSkinned
			&ctx
		);
	} else if (isOnScreen) {
		if (rootFD->NeedsKeyFrameUpdate)
			RpAnimBlendNodeUpdateKeyFrames(&ctx, bd->m_FrameDatas, bd->m_NumFrameData);

		bd->ForAllFrames(
			IsClumpSkinned(clump)
				? FrameUpdateCallBackW<false, true>   // FrameUpdateCallBackSkinned
				: FrameUpdateCallBackW<false, false>, // FrameUpdateCallBackNonSkinned
			&ctx
		);

		rootFD->NeedsKeyFrameUpdate = false;
	} else {
		bd->ForAllFrames(FrameUpdateCallBackOffscreen, &ctx);
		rootFD->NeedsKeyFrameUpdate = true;
	}

	// 0x4D3715 - Update all animations' times
	for (CAnimBlendLink *l = bd->m_AnimList.next; l;) {
		CAnimBlendAssociation *a = CAnimBlendAssociation::FromLink(l);
		l = a->m_Link.next;
		a->UpdateTime(timeStep, animTimeMult);
	}

	// 0x4D3764
	RwFrameUpdateObjects(RpClumpGetFrame(clump));
}

// 0x4D60E0
RpHAnimAnimation *RpAnimBlendCreateAnimationForHierarchy(RpHAnimHierarchy *hierarchy)
{
	if (!hierarchy)
		return nil;
	// The dummy animation only exists so the hierarchy's interpolator has two
	// key frames per node to point at; the game writes into the interpolation
	// frames and never plays this animation back.
	return RpHAnimAnimationCreate(
		rwID_RPANIMBLENDPLUGIN,
		2 * hierarchy->numNodes,
		0,
		0.0f
	);
}

// 0x4D5EF0
const char *RpAnimBlendFrameGetName(RwFrame *frame)
{
	return GetFrameNodeName(frame);
}

// 0x4D5F00
void RpAnimBlendFrameSetName(RwFrame *frame, const char *name)
{
	SetFrameNodeName(frame, name);
}

// 0x4D6AB0
CAnimBlendAssociation *RpAnimBlendGetNextAssociation(CAnimBlendAssociation *association)
{
	CAnimBlendLink *next = association->m_Link.next;
	return next
		? CAnimBlendAssociation::FromLink(next)
		: nil;
}

// 0x4D6AD0
CAnimBlendAssociation *RpAnimBlendGetNextAssociation(CAnimBlendAssociation *association, uint32 flags)
{
	assert(flags);
	for (CAnimBlendLink *l = association->m_Link.next; l;) {
		CAnimBlendAssociation *a = CAnimBlendAssociation::FromLink(l);
		l = a->m_Link.next;
		if (a->m_Flags & flags)
			return a;
	}
	return nil;
}
