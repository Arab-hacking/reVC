#include "common.h"

#include "AnimBlendStaticAssociation.h"
#include "AnimBlendHierarchy.h"

#include "AnimBlendClumpData.h"
#include "RpAnimBlend.h"
#include "MemoryMgr.h"

// 0x4CEF60
CAnimBlendStaticAssociation::CAnimBlendStaticAssociation(RpClump *clump, CAnimBlendHierarchy *h)
{
	m_NumBlendNodes = 0;
	m_AnimId = ANIM_ID_UNDEFINED;
	m_AnimGroupId = ANIM_GROUP_NONE;
	m_Flags = 0;
	m_BlendSeqs = nil;
	m_BlendHier = nil;
	Init(clump, h);
}

// 0x4CEC00
CAnimBlendStaticAssociation::~CAnimBlendStaticAssociation()
{
	FreeSequenceArray();
}

// 0x4CEC20
void CAnimBlendStaticAssociation::Init(RpClump *clump, CAnimBlendHierarchy *h)
{
	m_BlendHier = h;

	if (!clump)
		return;

	CAnimBlendClumpData *clumpAnimData = RpAnimBlendClumpGetData(clump);
	assert(clumpAnimData);

	m_NumBlendNodes = (uint16)clumpAnimData->m_NumFrameData;
	AllocateSequenceArray(m_NumBlendNodes);

	// Initialize sequences from the data in the clump
	for (uint32 s = 0; s < m_BlendHier->m_nSeqCount; s++) {
		CAnimBlendSequence &seq = m_BlendHier->m_pSequences[s];
		if (!seq.m_FramesNum)
			continue;
		AnimBlendFrameData *frameData = seq.IsUsingBoneTag()
			? RpAnimBlendClumpFindBone(clump, seq.GetBoneTag())
			: RpAnimBlendClumpFindFrameFromHashKey(clump, seq.GetNameHashKey());
		if (!frameData)
			continue;
		const int32 nodeIdx = (int32)(frameData - clumpAnimData->m_FrameDatas);
		assert(nodeIdx >= 0 && nodeIdx < m_NumBlendNodes);
		m_BlendSeqs[nodeIdx] = &seq;
	}
}

// 0x4CE960
void CAnimBlendStaticAssociation::AllocateSequenceArray(int32 count)
{
	m_BlendSeqs = (CAnimBlendSequence **)MemoryMgrMalloc(sizeof(CAnimBlendSequence *) * count);
	for (int32 i = 0; i < count; i++)
		m_BlendSeqs[i] = nil;
}

// 0x4CE9A0
void CAnimBlendStaticAssociation::FreeSequenceArray()
{
	if (m_BlendSeqs)
		MemoryMgrFree(m_BlendSeqs);
}

uint32 CAnimBlendStaticAssociation::GetHashKey() const
{
	return m_BlendHier->m_hashKey;
}
