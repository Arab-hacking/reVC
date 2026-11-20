#ifndef __GTA_PEDMODELINFO_H__
#define __GTA_PEDMODELINFO_H__

#include "ClumpModelInfo.h"
#include "ColModel.h"
#include "PedType.h"

enum PedNode {
	PED_TORSO = 0,	// has no bone!
	PED_MID,
	PED_HEAD,
	PED_UPPERARML,
	PED_UPPERARMR,
	PED_HANDL,
	PED_HANDR,
	PED_UPPERLEGL,
	PED_UPPERLEGR,
	PED_FOOTL,
	PED_FOOTR,
	PED_LOWERLEGR,
	PED_LOWERLEGL,

	PED_FOREARML,
	PED_FOREARMR,
	PED_CLAVICLEL,
	PED_CLAVICLER,
	PED_NECK,

	PED_NODE_MAX
};

class CPedModelInfo : public CClumpModelInfo
{
public:
	uint32 m_animGroup;
	ePedType m_pedType;
	ePedStats m_pedStatType;
	uint32 m_carsCanDrive;
	CColModel *m_hitColModel;
	int8 radio1, radio2;
	bool m_bSAAnimationModel;
	uint32 m_clumpStreamVersion;
	// Set once a VC-skeleton ped has been stood in for by an SA model, so every
	// later instance of this slot gets the same body instead of resolving the
	// replacement again, and a ped that is hidden mid-game does not come back
	// as a different person.
	int32 m_saReplacementModel;

	static RwObjectNameIdAssocation m_pPedIds[PED_NODE_MAX];

	// One past the highest slot that currently holds a ped model. The archive's
	// actors land above the native 0..129 block, so a replacement scan bounded
	// by MI_LAST_PED would never see them; bounding it by the whole 32768-entry
	// model-info array instead would be far too slow. This is maintained by
	// CPedModelInfo::NoteSlot and reset on a restart.
	static int32 ms_lastPedSlot;
	static void NoteSlot(int32 slot) { if(slot >= ms_lastPedSlot) ms_lastPedSlot = slot + 1; }

	CPedModelInfo(void) : CClumpModelInfo(MITYPE_PED) { m_hitColModel = nil; m_bSAAnimationModel = false; m_clumpStreamVersion = 0; m_saReplacementModel = -1; }
	~CPedModelInfo(void) { delete m_hitColModel; }
	void DeleteRwObject(void);
	void SetClump(RpClump *);
	void SetClumpStreamVersion(uint32 version) { m_clumpStreamVersion = version; }
	bool UsesSAAnimations(void) const { return m_bSAAnimationModel; }
	// The ped animation set is SA only, so a VC skeleton cannot be animated.
	// Replacement is decided by the caller keeping the same animation group.
	static int32 FindSAReplacementModel(int32 modelId);
	static bool DetectSAAnimationClump(RpClump *clump, uint32 clumpStreamVersion = 0);

	void CreateHitColModelSkinned(RpClump *clump);
	CColModel *GetHitColModel(void) { return m_hitColModel; }
	CColModel *AnimatePedColModelSkinned(RpClump *clump);
	CColModel *AnimatePedColModelSkinnedWorld(RpClump *clump);
};

#endif // __GTA_PEDMODELINFO_H__
