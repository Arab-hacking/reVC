#include "common.h"

#include "AnimBlendClumpData.h"
#include "MemoryMgr.h"

// 0x4CF0E0
CAnimBlendClumpData::CAnimBlendClumpData()
{
	m_NumFrameData = 0;
	m_PedPosition = nil;
	m_FrameDatas = nil;
}

// 0x4CF100
CAnimBlendClumpData::~CAnimBlendClumpData()
{
	m_AnimList.Remove();
	if (m_FrameDatas)
		RwFreeAlign(m_FrameDatas);
}

// 0x4CF140
void CAnimBlendClumpData::SetNumberOfBones(uint32 numBones)
{
	if (m_FrameDatas)
		RwFreeAlign(m_FrameDatas);
	m_NumFrameData = numBones;
	m_FrameDatas = (AnimBlendFrameData *)RwMallocAlign(sizeof(AnimBlendFrameData) * numBones, 64);
}

// 0x4CF190
void CAnimBlendClumpData::ForAllFrames(void (*callback)(AnimBlendFrameData *, void *), void *data)
{
	for (uint32 i = 0; i < m_NumFrameData; i++)
		callback(&m_FrameDatas[i], data);
}
