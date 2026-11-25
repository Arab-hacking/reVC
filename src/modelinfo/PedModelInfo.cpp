#include "common.h"

#include "RwHelper.h"
#include "General.h"
#include "eBoneTag.h"
#include "PedNode.h"
#include "SurfaceTable.h"
#include "Ped.h"
#include "NodeName.h"
#include "VisibilityPlugins.h"
#include "ModelInfo.h"
#include "ModelIndices.h"
#include "custompipes.h"

void
CPedModelInfo::DeleteRwObject(void)
{
	CClumpModelInfo::DeleteRwObject();
	m_bSAAnimationModel = false;
	m_clumpStreamVersion = 0;
	if(m_hitColModel)
		delete m_hitColModel;
	m_hitColModel = nil;
}

static bool
EqualsIgnoreCaseTrim(const char *s, const char *lit)
{
	if(s == nil || lit == nil)
		return false;
	while(*s == ' ' || *s == '\t')
		s++;
	while(*lit){
		char a = *s++;
		char b = *lit++;
		if(a >= 'A' && a <= 'Z') a += 'a' - 'A';
		if(b >= 'A' && b <= 'Z') b += 'a' - 'A';
		if(a != b)
			return false;
	}
	while(*s == ' ' || *s == '\t')
		s++;
	return *s == '\0';
}

static bool
StartsWithIgnoreCaseTrim(const char *s, const char *prefix)
{
	if(s == nil || prefix == nil)
		return false;
	while(*s == ' ' || *s == '\t')
		s++;
	while(*prefix){
		char a = *s++;
		char b = *prefix++;
		if(a >= 'A' && a <= 'Z') a += 'a' - 'A';
		if(b >= 'A' && b <= 'Z') b += 'a' - 'A';
		if(a != b)
			return false;
	}
	return true;
}

static RwFrame*
FindSAFrameMarkerCB(RwFrame *frame, void *data)
{
	const char *name = GetFrameNodeName(frame);
	if(name && (EqualsIgnoreCaseTrim(name, "Normal") ||
	            StartsWithIgnoreCaseTrim(name, "Bip01 Pelvis") ||
	            StartsWithIgnoreCaseTrim(name, "Bip01 Spine"))){
		*(bool*)data = true;
		return nil;
	}
	RwFrameForAllChildren(frame, FindSAFrameMarkerCB, data);
	return *(bool*)data ? nil : frame;
}

bool
CPedModelInfo::DetectSAAnimationClump(RpClump *clump, uint32 clumpStreamVersion)
{
	// Both VC and SA skinned pedestrian DFFs share the 19 core BoneTag IDs
	// (0..5, 21..25, 31..35, 41..43, 51..53), so checking only those IDs
	// misclassifies stock VC peds (cop, player, etc.) as SA skeletons.
	// Require a valid skinned ped hierarchy first, then check SA-specific
	// markers: RW 3.6+ DFF version, SA-only facial/torso bone tags, or SA
	// 3ds Max Bip01/Normal bone frame names.
	static const int requiredBones[] = {
		BONE_ROOT, BONE_PELVIS, BONE_SPINE1, BONE_NECK, BONE_HEAD,
		BONE_L_CLAVICLE, BONE_L_UPPER_ARM, BONE_L_FORE_ARM, BONE_L_HAND,
		BONE_L_FINGER, BONE_R_CLAVICLE, BONE_R_UPPER_ARM, BONE_R_FORE_ARM,
		BONE_R_HAND, BONE_R_FINGER, BONE_L_THIGH, BONE_L_CALF, BONE_L_FOOT,
		BONE_R_THIGH, BONE_R_CALF, BONE_R_FOOT
	};
	// SA-exclusive eBoneTag IDs: BONE_L_BROW(6), BONE_R_BROW(7), BONE_JAW(8),
	// BONE_BELLY(201), BONE_R_BREAST(301), BONE_L_BREAST(302).
	static const int saExclusiveBones[] = { 6, 7, 8, 201, 301, 302 };
	RpHAnimHierarchy *hier;

	if(clump == nil || !IsClumpSkinned(clump))
		return false;
	hier = GetAnimHierarchyFromSkinClump(clump);
	if(hier == nil)
		return false;
	for(uint32 i = 0; i < ARRAY_SIZE(requiredBones); i++)
		if(RpHAnimIDGetIndex(hier, requiredBones[i]) < 0)
			return false;

	if(clumpStreamVersion >= 0x36000)
		return true;

	for(uint32 i = 0; i < ARRAY_SIZE(saExclusiveBones); i++)
		if(RpHAnimIDGetIndex(hier, saExclusiveBones[i]) >= 0)
			return true;

	bool foundSAFrameName = false;
	if(RpClumpGetFrame(clump))
		RwFrameForAllChildren(RpClumpGetFrame(clump), FindSAFrameMarkerCB, &foundSAFrameName);
	return foundSAFrameName;
}

// A ped whose skeleton has no SA bones cannot be driven by the SA animation
// set, so look for a loaded SA ped that keeps the same animation group. This
// keeps clothing and stats sensible instead of leaving the ped frozen.
// The set of animation groups that belong to female pedestrians. A VC woman
// replaced by the first SA ped the loop found became a man, which is why the
// female models came back with the wrong walk and the wrong build.
static bool
IsFemaleAnimGroup(uint32 group)
{
	switch(group){
	case ANIM_GROUP_WOMAN:
	case ANIM_GROUP_SHOPPING:
	case ANIM_GROUP_BUSYWOMAN:
	case ANIM_GROUP_SEXYWOMAN:
	case ANIM_GROUP_OLDWOMAN:
	case ANIM_GROUP_FATWOMAN:
	case ANIM_GROUP_JOGWOMAN:
		return true;
	default:
		return false;
	}
}

// A heavy ped - the fatman/fatwoman styles - keeps its bulk only if the
// replacement is sized the same way.
static bool
IsHeavyAnimGroup(uint32 group)
{
	return group == ANIM_GROUP_FATMAN || group == ANIM_GROUP_FATWOMAN ||
		group == ANIM_GROUP_OLDFATMAN || group == ANIM_GROUP_OLDMAN;
}

int32
CPedModelInfo::FindSAReplacementModel(int32 modelId)
{
	CBaseModelInfo *baseInfo;
	CPedModelInfo *info;
	uint32 wantGroup = 0;
	int32 fallback = -1;
	int32 i;

	if(modelId >= 0 && modelId < MODELINFOSIZE){
		baseInfo = CModelInfo::GetModelInfo(modelId);
		if(baseInfo && baseInfo->GetModelType() == MITYPE_PED){
			wantGroup = ((CPedModelInfo*)baseInfo)->m_animGroup;
			// A slot is expected to keep the body it was given the first time.
			int32 chosen = ((CPedModelInfo*)baseInfo)->m_saReplacementModel;
			if(chosen >= 0 && chosen < MODELINFOSIZE)
				return chosen;
		}
	}
	const bool wantFemale = IsFemaleAnimGroup(wantGroup);
	const bool wantHeavy = IsHeavyAnimGroup(wantGroup);

	// Two passes: a replacement of the same sex and build is worth far more
	// than "the first SA clump that happened to be resident", which is how
	// every VC pedestrian used to become male01.
	// The scan cannot stop at MI_LAST_PED. The native ped block (0..129) was
	// filled by DEFAULT.IDE, which is no longer read; the archive's own actors
	// live above FIRST_BR_SLOT, so a scan of the native range alone would never
	// find the SA bodies this build actually has.
	for(int32 pass = 0; pass < 2; pass++){
		for(i = 0; i < CPedModelInfo::ms_lastPedSlot; i++){
			baseInfo = CModelInfo::GetModelInfo(i);
			if(baseInfo == nil || baseInfo->GetModelType() != MITYPE_PED || i == modelId)
				continue;
			info = (CPedModelInfo*)baseInfo;
			// The skeleton is only classified once the clump is resident, so an
			// unloaded model cannot be judged and must not be offered as a
			// replacement. Callers load the chosen model and re-check it.
			if(info->GetRwObject() == nil || !info->UsesSAAnimations())
				continue;
			if(pass == 0 && (IsFemaleAnimGroup(info->m_animGroup) != wantFemale ||
			                 IsHeavyAnimGroup(info->m_animGroup) != wantHeavy))
				continue;
			if(info->m_animGroup == wantGroup)
				return i;
			if(fallback < 0)
				fallback = i;
		}
	}
	return fallback;
}

// leftover...
int32 CPedModelInfo::ms_lastPedSlot = MI_LAST_PED + 1;

RwObjectNameIdAssocation CPedModelInfo::m_pPedIds[NUM_PED_NAME_ID_ASSOC] = {
	{ "Smid",		PED_NODE_UPPER_TORSO, 0, },
	{ "Shead",		PED_NODE_HEAD, 0, },
	{ "Supperarml",	PED_NODE_LEFT_ARM, 0, },
	{ "Supperarmr",	PED_NODE_RIGHT_ARM, 0, },
	{ "SLhand",		PED_NODE_LEFT_HAND, 0, },
	{ "SRhand",		PED_NODE_RIGHT_HAND, 0, },
	{ "Supperlegl",	PED_NODE_LEFT_LEG, 0, },
	{ "Supperlegr",	PED_NODE_RIGHT_LEG, 0, },
	{ "Sfootl",		PED_NODE_LEFT_FOOT, 0, },
	{ "Sfootr",		PED_NODE_RIGHT_FOOT, 0, },
	{ "Slowerlegl",	PED_NODE_LEFT_LOWER_LEG, 0, },
	{ "Slowerlegr",	PED_NODE_RIGHT_LOWER_LEG, 0, },
	{ nil,			0, 0, },
};

void
CPedModelInfo::SetClump(RpClump *clump)
{
#ifdef EXTENDED_PIPELINES
	CustomPipes::AttachRimPipe(clump);
#endif
	CClumpModelInfo::SetClump(clump);
	m_bSAAnimationModel = DetectSAAnimationClump(clump, m_clumpStreamVersion);
	SetFrameIds(m_pPedIds);	// not needed in VC actually
	if(m_hitColModel == nil)
		CreateHitColModelSkinned(clump);
	RpClumpForAllAtomics(m_clump, SetAtomicRendererCB, (void*)CVisibilityPlugins::RenderPedCB);
	if(strcmp(GetModelName(), "player") == 0)
		RpClumpForAllAtomics(m_clump, SetAtomicRendererCB, (void*)CVisibilityPlugins::RenderPlayerCB);
}

// One hit-collision sphere per entry, in the order San Andreas lays them out
// (CPedModelInfo::m_pColNodeInfos, 0x8A6308). The bone ids are real eBoneTag
// values, not ped nodes: SA stores the bone directly, and the offsets are in
// that bone's space. The piece is what the sphere reports to damage code.
tPedColNodeInfo CPedModelInfo::m_pColNodeInfos[NUM_PED_COLMODEL_NODES] = {
	{ {}, BONE_HEAD,			PED_PIECE_HEAD,			CVector( 0.05f, 0.0f, 0.0f), 0.15f },
	{ {}, BONE_SPINE1,			PED_PIECE_TORSO,		CVector( 0.20f, 0.0f, 0.0f), 0.20f },
	{ {}, BONE_SPINE1,			PED_PIECE_TORSO,		CVector( 0.0f,  0.0f, 0.0f), 0.20f },
	{ {}, BONE_SPINE,			PED_PIECE_ASS,			CVector(-0.10f, 0.0f, 0.0f), 0.20f },
	{ {}, BONE_L_UPPER_ARM,		PED_PIECE_LEFT_ARM,		CVector( 0.06f, 0.0f, 0.0f), 0.14f },
	{ {}, BONE_R_UPPER_ARM,		PED_PIECE_RIGHT_ARM,	CVector( 0.06f, 0.0f, 0.0f), 0.14f },
	{ {}, BONE_L_FORE_ARM,		PED_PIECE_LEFT_ARM,		CVector( 0.05f, 0.0f, 0.0f), 0.14f },
	{ {}, BONE_R_FORE_ARM,		PED_PIECE_RIGHT_ARM,	CVector( 0.05f, 0.0f, 0.0f), 0.14f },
	{ {}, BONE_L_CALF,			PED_PIECE_LEFT_LEG,		CVector(-0.10f, 0.0f, 0.0f), 0.18f },
	{ {}, BONE_R_CALF,			PED_PIECE_RIGHT_LEG,	CVector(-0.10f, 0.0f, 0.0f), 0.18f },
	{ {}, BONE_L_FOOT,			PED_PIECE_LEFT_LEG,		CVector(-0.18f, 0.0f, 0.0f), 0.16f },
	{ {}, BONE_R_FOOT,			PED_PIECE_RIGHT_LEG,	CVector(-0.18f, 0.0f, 0.0f), 0.16f },
};

void
CPedModelInfo::CreateHitColModelSkinned(RpClump *clump)
{
	RpHAnimHierarchy *hier = GetAnimHierarchyFromSkinClump(clump);
	CColModel *colmodel = new CColModel;
	CColSphere *spheres = (CColSphere*)RwMalloc(NUM_PED_COLMODEL_NODES*sizeof(CColSphere));
	RwMatrix *invmat = RwMatrixCreate();
	RwMatrix *mat = RwMatrixCreate();
	RwMatrixInvert(invmat, RwFrameGetMatrix(RpClumpGetFrame(clump)));

	for(int i = 0; i < NUM_PED_COLMODEL_NODES; i++){
		const tPedColNodeInfo &node = m_pColNodeInfos[i];
		int idx = RpHAnimIDGetIndex(hier, node.m_nBoneID);
		CVector center = node.m_vecCenter;

		// A model without the bone gets the sphere where the table puts it.
		if(idx >= 0 && idx < hier->numNodes){
			*mat = *invmat;
			RwMatrixTransform(mat, &RpHAnimHierarchyGetMatrixArray(hier)[idx], rwCOMBINEPRECONCAT);
			RwV3d pos = { center.x, center.y, center.z };
			RwV3dTransformPoints(&pos, &pos, 1, mat);
			center = CVector(pos);
		}
		spheres[i].center = center;
		spheres[i].radius = node.m_fRadius;
		spheres[i].surface = SURFACE_PED;
		spheres[i].material = node.m_nFlags;
	}
	RwMatrixDestroy(invmat);
	RwMatrixDestroy(mat);
	colmodel->spheres = spheres;
	colmodel->numSpheres = NUM_PED_COLMODEL_NODES;
	colmodel->boundingSphere.Set(1.5f, CVector(0.0f, 0.0f, 0.0f));
	colmodel->boundingBox.Set(CVector(-0.5f, -0.5f, -1.2f), CVector(0.5f, 0.5f, 1.2f));
	colmodel->level = LEVEL_GENERIC;
	m_hitColModel = colmodel;
}

CColModel*
CPedModelInfo::AnimatePedColModelSkinned(RpClump *clump)
{
	if(m_hitColModel == nil){
		CreateHitColModelSkinned(clump);
		return m_hitColModel;
	}
	RwMatrix *invmat, *mat;
	CColSphere *spheres = m_hitColModel->spheres;
	RpHAnimHierarchy *hier = GetAnimHierarchyFromSkinClump(clump);
	if(hier == nil)
		return m_hitColModel;
	invmat = RwMatrixCreate();
	mat = RwMatrixCreate();
	RwMatrixInvert(invmat, RwFrameGetMatrix(RpClumpGetFrame(clump)));

	for(int i = 0; i < NUM_PED_COLMODEL_NODES; i++){
		const tPedColNodeInfo &node = m_pColNodeInfos[i];
		int idx = RpHAnimIDGetIndex(hier, node.m_nBoneID);
		if(idx < 0 || idx >= hier->numNodes){
			spheres[i].center = node.m_vecCenter;
			continue;
		}
		*mat = *invmat;
		RwMatrixTransform(mat, &RpHAnimHierarchyGetMatrixArray(hier)[idx], rwCOMBINEPRECONCAT);
		RwV3d pos = { node.m_vecCenter.x, node.m_vecCenter.y, node.m_vecCenter.z };
		RwV3dTransformPoints(&pos, &pos, 1, mat);
		spheres[i].center = CVector(pos);
	}
	RwMatrixDestroy(invmat);
	RwMatrixDestroy(mat);

	// The bound volume follows the chest bone, so a ped lying down does not
	// keep an upright box around itself.
	CVector boundCenter(0.0f, 0.0f, 0.0f);
	int spineIdx = RpHAnimIDGetIndex(hier, BONE_SPINE1);
	if(spineIdx >= 0 && spineIdx < hier->numNodes){
		*mat = *invmat;
		RwMatrixTransform(mat, &RpHAnimHierarchyGetMatrixArray(hier)[spineIdx], rwCOMBINEPRECONCAT);
		RwV3d pos = { 0.0f, 0.0f, 0.0f };
		RwV3dTransformPoints(&pos, &pos, 1, mat);
		boundCenter = CVector(pos);
	}
	m_hitColModel->boundingSphere.Set(1.5f, boundCenter);
	m_hitColModel->boundingBox.Set(boundCenter - CVector(1.2f, 1.2f, 1.2f),
	                               boundCenter + CVector(1.2f, 1.2f, 1.2f));
	return m_hitColModel;
}

CColModel*
CPedModelInfo::AnimatePedColModelSkinnedWorld(RpClump *clump)
{
	if(m_hitColModel == nil)
		CreateHitColModelSkinned(clump);
	CColSphere *spheres = m_hitColModel->spheres;
	RpHAnimHierarchy *hier = GetAnimHierarchyFromSkinClump(clump);
	if(hier == nil)
		return m_hitColModel;
	RwMatrix *mat;

	for(int i = 0; i < NUM_PED_COLMODEL_NODES; i++){
		const tPedColNodeInfo &node = m_pColNodeInfos[i];
		int idx = RpHAnimIDGetIndex(hier, node.m_nBoneID);
		CVector center = node.m_vecCenter;
		if(idx < 0 || idx >= hier->numNodes){
			RwMatrix *rootLtm = RwFrameGetLTM(RpClumpGetFrame(clump));
			spheres[i].center = CVector(rootLtm->pos) + center;
			continue;
		}
		mat = &RpHAnimHierarchyGetMatrixArray(hier)[idx];
		RwV3d pos = { center.x, center.y, center.z };
		RwV3dTransformPoints(&pos, &pos, 1, mat);
		spheres[i].center = CVector(pos);
	}
	return m_hitColModel;
}
