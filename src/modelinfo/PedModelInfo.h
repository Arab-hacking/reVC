#ifndef __GTA_PEDMODELINFO_H__
#define __GTA_PEDMODELINFO_H__

#include "ClumpModelInfo.h"
#include "ColModel.h"
#include "AnimManager.h"
#include "PedType.h"
#include "eBoneTag.h"
#include "PedNode.h"

class CPedModelInfo : public CClumpModelInfo
{
public:
	AssocGroupId m_animGroup;
	ePedType m_pedType;
	ePedStats m_pedStatType;
	uint16 m_carsCanDrive;
	uint16 m_nPedFlags;
	CColModel *m_hitColModel;
	int8 radio1, radio2;
	uint8 m_nRace;
	int16 m_nPedAudioType;
	int16 m_nVoiceMin;
	int16 m_nVoiceMax;
	int16 m_nVoiceId;
	bool m_bSAAnimationModel;
	uint32 m_clumpStreamVersion;
	// Set once a VC-skeleton ped has been stood in for by an SA model, so every
	// later instance of this slot gets the same body instead of resolving the
	// replacement again, and a ped that is hidden mid-game does not come back
	// as a different person.
	int32 m_saReplacementModel;

	static const int32 NUM_PED_NAME_ID_ASSOC = 13;
	static const int32 NUM_PED_COLMODEL_NODES = 12;

	static RwObjectNameIdAssocation m_pPedIds[NUM_PED_NAME_ID_ASSOC];
	static tPedColNodeInfo m_pColNodeInfos[NUM_PED_COLMODEL_NODES];

	// One past the highest slot that currently holds a ped model. The archive's
	// actors land above the native 0..129 block, so a replacement scan bounded
	// by MI_LAST_PED would never see them; bounding it by the whole 32768-entry
	// model-info array instead would be far too slow. This is maintained by
	// CPedModelInfo::NoteSlot and reset on a restart.
	static int32 ms_lastPedSlot;
	static void NoteSlot(int32 slot) { if(slot >= ms_lastPedSlot) ms_lastPedSlot = slot + 1; }

	CPedModelInfo(void) : CClumpModelInfo(MITYPE_PED)
	{
		m_animGroup = ANIM_GROUP_DEFAULT;
		m_pedType = PEDTYPE_CIVMALE;
		m_pedStatType = PEDSTAT_STD_MISSION;
		m_carsCanDrive = 0;
		m_nPedFlags = 0;
		m_hitColModel = nil;
		radio1 = -1;
		radio2 = -1;
		m_nRace = RACE_DEFAULT;
		m_nPedAudioType = -1;
		m_nVoiceMin = -1;
		m_nVoiceMax = -1;
		m_nVoiceId = -1;
		m_bSAAnimationModel = false;
		m_clumpStreamVersion = 0;
		m_saReplacementModel = -1;
	}
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
