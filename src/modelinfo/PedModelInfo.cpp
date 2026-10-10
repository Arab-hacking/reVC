#include "common.h"

#include "RwHelper.h"
#include "General.h"
#include "Bones.h"
#include "SurfaceTable.h"
#include "Ped.h"
#include "NodeName.h"
#include "VisibilityPlugins.h"
#include "ModelInfo.h"
#include "custompipes.h"

void
CPedModelInfo::DeleteRwObject(void)
{
	CClumpModelInfo::DeleteRwObject();
	m_bSAAnimationModel = false;
	if(m_hitColModel)
		delete m_hitColModel;
	m_hitColModel = nil;
}

static bool
IsSAAnimationClump(RpClump *clump)
{
	// SA pedestrian DFFs use the tagged, skinned hierarchy (including finger
	// bones). VC's native ped hierarchy is not this skeleton, so do not infer
	// the animation set from the model name or its anim group.
	static const int requiredBones[] = {
		BONE_root, BONE_pelvis, BONE_spine1, BONE_neck, BONE_head,
		BONE_l_clavicle, BONE_l_upperarm, BONE_l_forearm, BONE_l_hand,
		BONE_l_finger, BONE_r_clavicle, BONE_r_upperarm, BONE_r_forearm,
		BONE_r_hand, BONE_r_finger, BONE_l_thigh, BONE_l_calf, BONE_l_foot,
		BONE_r_thigh, BONE_r_calf, BONE_r_foot
	};
	RpHAnimHierarchy *hier;

	if(clump == nil || !IsClumpSkinned(clump))
		return false;
	hier = GetAnimHierarchyFromSkinClump(clump);
	if(hier == nil)
		return false;
	for(uint32 i = 0; i < ARRAY_SIZE(requiredBones); i++)
		if(RpHAnimIDGetIndex(hier, requiredBones[i]) < 0)
			return false;
	return true;
}

// leftover...
RwObjectNameIdAssocation CPedModelInfo::m_pPedIds[PED_NODE_MAX] = {
	{ "Smid",	PED_MID, 0, },	// that is strange...
	{ "Shead",	PED_HEAD, 0, },
	{ "Supperarml",	PED_UPPERARML, 0, },
	{ "Supperarmr",	PED_UPPERARMR, 0, },
	{ "SLhand",	PED_HANDL, 0, },
	{ "SRhand",	PED_HANDR, 0, },
	{ "Supperlegl",	PED_UPPERLEGL, 0, },
	{ "Supperlegr",	PED_UPPERLEGR, 0, },
	{ "Sfootl",	PED_FOOTL, 0, },
	{ "Sfootr",	PED_FOOTR, 0, },
	{ "Slowerlegr",	PED_LOWERLEGR, 0, },
	{ nil,	0, 0, },
};

void
CPedModelInfo::SetClump(RpClump *clump)
{
#ifdef EXTENDED_PIPELINES
	CustomPipes::AttachRimPipe(clump);
#endif
	CClumpModelInfo::SetClump(clump);
	m_bSAAnimationModel = IsSAAnimationClump(clump);
	SetFrameIds(m_pPedIds);	// not needed in VC actually
	if(m_hitColModel == nil)
		CreateHitColModelSkinned(clump);
	RpClumpForAllAtomics(m_clump, SetAtomicRendererCB, (void*)CVisibilityPlugins::RenderPedCB);
	if(strcmp(GetModelName(), "player") == 0)
		RpClumpForAllAtomics(m_clump, SetAtomicRendererCB, (void*)CVisibilityPlugins::RenderPlayerCB);
}

struct ColNodeInfo
{
	Const char *name;
	int pedNode;
	int pieceType;
	float x, z;
	float radius;
};

#define NUMPEDINFONODES 10
ColNodeInfo m_pColNodeInfos[NUMPEDINFONODES] = {
	{ nil, PED_HEAD,	PEDPIECE_HEAD,	0.0f,   0.05f, 0.15f },
	{ nil, PED_MID,		PEDPIECE_TORSO,	0.0f,   0.15f, 0.2f },
	{ nil, PED_MID,		PEDPIECE_TORSO,	0.0f,  -0.05f, 0.25f },
	{ nil, PED_MID,		PEDPIECE_MID,	0.0f,  -0.25f, 0.25f },
	{ nil, PED_UPPERARML,	PEDPIECE_LEFTARM,	0.03f, -0.05f,  0.16f },
	{ nil, PED_UPPERARMR,	PEDPIECE_RIGHTARM,	-0.03f, -0.05f,  0.16f },
	{ nil, PED_LOWERLEGL,	PEDPIECE_LEFTLEG,	0.0f,   0.15f, 0.2f },
	{ nil, PED_LOWERLEGR,	PEDPIECE_RIGHTLEG,	0.0f,   0.15f, 0.2f },
	{ nil, PED_FOOTL,	PEDPIECE_LEFTLEG,	0.0f,   0.15f, 0.15f },
	{ nil, PED_FOOTR,	PEDPIECE_RIGHTLEG,	0.0f,   0.15f, 0.15f },
};

void
CPedModelInfo::CreateHitColModelSkinned(RpClump *clump)
{
	RpHAnimHierarchy *hier = GetAnimHierarchyFromSkinClump(clump);
	CColModel *colmodel = new CColModel;
	CColSphere *spheres = (CColSphere*)RwMalloc(NUMPEDINFONODES*sizeof(CColSphere));
	RwFrame *root = RpClumpGetFrame(m_clump);
	RwMatrix *invmat = RwMatrixCreate();
	RwMatrix *mat = RwMatrixCreate();
	RwMatrixInvert(invmat, RwFrameGetMatrix(RpClumpGetFrame(clump)));

	for(int i = 0; i < NUMPEDINFONODES; i++){
		*mat = *invmat;

		// From LCS. Otherwise gives FPE
#ifdef FIX_BUGS
		spheres[i].center = CVector(0.0f, 0.0f, 0.0f);
#else
		int id = ConvertPedNode2BoneTag(m_pColNodeInfos[i].pedNode);	// this is wrong, wtf R* ???
		int idx = RpHAnimIDGetIndex(hier, id);

		// This doesn't really work as the positions are not initialized yet
		RwMatrixTransform(mat, &RpHAnimHierarchyGetMatrixArray(hier)[idx], rwCOMBINEPRECONCAT);
		RwV3d pos = { 0.0f, 0.0f, 0.0f };
		RwV3dTransformPoints(&pos, &pos, 1, mat);

		spheres[i].center = pos + CVector(m_pColNodeInfos[i].x, 0.0f, m_pColNodeInfos[i].z);
#endif
		spheres[i].radius = m_pColNodeInfos[i].radius;
		spheres[i].surface = SURFACE_PED;
		spheres[i].piece = m_pColNodeInfos[i].pieceType;
	}
	RwMatrixDestroy(invmat);
	RwMatrixDestroy(mat);
	colmodel->spheres = spheres;
	colmodel->numSpheres = NUMPEDINFONODES;
	colmodel->boundingSphere.Set(2.0f, CVector(0.0f, 0.0f, 0.0f));
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
	invmat = RwMatrixCreate();
	mat = RwMatrixCreate();
	RwMatrixInvert(invmat, RwFrameGetMatrix(RpClumpGetFrame(clump)));

	for(int i = 0; i < NUMPEDINFONODES; i++){
		*mat = *invmat;
		int id = ConvertPedNode2BoneTag(m_pColNodeInfos[i].pedNode);
		int idx = RpHAnimIDGetIndex(hier, id);

		RwMatrixTransform(mat, &RpHAnimHierarchyGetMatrixArray(hier)[idx], rwCOMBINEPRECONCAT);
		RwV3d pos = { 0.0f, 0.0f, 0.0f };
		RwV3dTransformPoints(&pos, &pos, 1, mat);

		spheres[i].center = pos + CVector(m_pColNodeInfos[i].x, 0.0f, m_pColNodeInfos[i].z);
	}
	RwMatrixDestroy(invmat);
	RwMatrixDestroy(mat);
	return m_hitColModel;
}

CColModel*
CPedModelInfo::AnimatePedColModelSkinnedWorld(RpClump *clump)
{
	if(m_hitColModel == nil)
		CreateHitColModelSkinned(clump);
	CColSphere *spheres = m_hitColModel->spheres;
	RpHAnimHierarchy *hier = GetAnimHierarchyFromSkinClump(clump);
	RwMatrix *mat;

	for(int i = 0; i < NUMPEDINFONODES; i++){
		int id = ConvertPedNode2BoneTag(m_pColNodeInfos[i].pedNode);
		int idx = RpHAnimIDGetIndex(hier, id);

		mat = &RpHAnimHierarchyGetMatrixArray(hier)[idx];
		RwV3d pos = { 0.0f, 0.0f, 0.0f };
		RwV3dTransformPoints(&pos, &pos, 1, mat);

		spheres[i].center = pos + CVector(m_pColNodeInfos[i].x, 0.0f, m_pColNodeInfos[i].z);
	}
	return m_hitColModel;
}
