#include "common.h"

#include "AnimBlendHierarchy.h"
#include "AnimManager.h"
#include "KeyGen.h"
#include "MemoryMgr.h"

// 0x4CF270
CAnimBlendHierarchy::CAnimBlendHierarchy()
{
	m_pSequences = nil;
	m_nSeqCount = 0;
	m_bIsCompressed = false;
	m_bKeepCompressed = false;
	m_nAnimBlockId = -1;
	m_fTotalTime = 0.0f;
	m_Link = nil;
	m_hashKey = 0;
}

// 0x4CF970
CAnimBlendHierarchy::~CAnimBlendHierarchy()
{
	RemoveAnimSequences();
}

// 0x4CF980
void CAnimBlendHierarchy::Shutdown()
{
	RemoveAnimSequences();
	m_bIsCompressed = false;
}

// 0x4CF8E0
void CAnimBlendHierarchy::RemoveAnimSequences()
{
	void *oldFrameData = nil;
	if (m_pSequences && m_pSequences->m_bUsingExternalMemory)
		oldFrameData = m_pSequences->m_Frames;
	CAnimManager::RemoveFromUncompressedCache(this);
	delete[] m_pSequences;
	m_pSequences = nil;
	m_nSeqCount = 0;
	m_fTotalTime = 0.0f;
	if (oldFrameData)
		MemoryMgrFree(oldFrameData);
}

// 0x4CF510
uint8 *CAnimBlendHierarchy::AllocSequenceBlock(bool compressed) const
{
	size_t size = 0;
	for (uint32 i = 0; i < m_nSeqCount; i++)
		size += m_pSequences[i].GetDataSize(compressed);
	return (uint8 *)MemoryMgrMalloc(size);
}

// 0x4CF290
CAnimBlendSequence *CAnimBlendHierarchy::FindSequence(const char *name) const
{
	if (!m_nSeqCount)
		return nil;

	const uint32 hash = CKeyGen::GetUppercaseKey(name);
	for (uint32 i = 0; i < m_nSeqCount; i++) {
		if (m_pSequences[i].GetNameHashKey() == hash)
			return &m_pSequences[i];
	}

	return nil;
}

// 0x4CF2D0
void CAnimBlendHierarchy::SetName(const char *name)
{
	m_hashKey = CKeyGen::GetUppercaseKey(name);
}

// 0x4CF4E0
void CAnimBlendHierarchy::RemoveQuaternionFlips() const
{
	for (uint32 i = 0; i < m_nSeqCount; i++)
		m_pSequences[i].RemoveQuaternionFlips();
}

// 0x4CF560
void *CAnimBlendHierarchy::GetSequenceBlock() const
{
	return m_pSequences->m_bUsingExternalMemory
		? m_pSequences->m_Frames
		: nil;
}

// 0x4CF5F0
void CAnimBlendHierarchy::Uncompress()
{
	assert(m_nSeqCount > 0);
	assert(m_bIsCompressed);

	void *cSeqData = GetSequenceBlock();

	// Now uncompress sequence data
	uint8 *uSeqData = AllocSequenceBlock(false);
	for (uint32 i = 0; i < m_nSeqCount; i++) {
		m_pSequences[i].Uncompress(uSeqData);
		uSeqData += m_pSequences[i].GetDataSize(false);
	}

	// Now we no longer need the old compressed data
	if (cSeqData)
		MemoryMgrFree(cSeqData);

	m_bIsCompressed = false;
	if (m_fTotalTime == 0.0f) {
		RemoveQuaternionFlips();
		CalcTotalTime();
	}
}

// 0x4CF6C0
void CAnimBlendHierarchy::CompressKeyframes() const
{
	assert(!m_bIsCompressed);

	void *uSeqData = GetSequenceBlock();

	// Compress current data
	uint8 *cSeqData = AllocSequenceBlock(true);
	for (uint32 i = 0; i < m_nSeqCount; i++) {
		m_pSequences[i].CompressKeyframes(cSeqData);
		cSeqData += m_pSequences[i].GetDataSize(true);
	}

	// Now we no longer need the old uncompressed data
	if (uSeqData)
		MemoryMgrFree(uSeqData);
}

// 0x4CF760
void CAnimBlendHierarchy::RemoveUncompressedData()
{
	assert(m_nSeqCount > 0);
	assert(!m_bIsCompressed);

	CompressKeyframes();

	m_bIsCompressed = true;
}

// 0x4CF800
void CAnimBlendHierarchy::MoveMemory()
{
	for (uint32 i = 0; i < m_nSeqCount; i++)
		m_pSequences[i].MoveMemory();
}

// 0x4CF8A0
void CAnimBlendHierarchy::Print()
{
	CAnimManager::UncompressAnimation(this);
	for (uint32 i = 0; i < m_nSeqCount; i++)
		m_pSequences[i].Print();
}

void CAnimBlendHierarchy::SetNumSequences(size_t n)
{
	m_nSeqCount = (uint16)n;
	m_pSequences = new CAnimBlendSequence[n]; // Yes, they used `new`
}

uint32 CAnimBlendHierarchy::GetIndex() const
{
	return CAnimManager::GetAnimIndex(this);
}
