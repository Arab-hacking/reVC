#include "common.h"

#include "AnimBlendSequence.h"
#include "KeyGen.h"
#include "MemoryMgr.h"
#include "MemoryHeap.h"

// 0x4D0C10
CAnimBlendSequence::CAnimBlendSequence()
{
	m_bHasRotation = false;
	m_bHasTranslation = false;
	m_bIsCompressed = false;
	m_bUsingExternalMemory = false;
	m_IsUsingBoneTag = false;
	m_FramesNum = 0;
	m_Frames = nil;
	m_NameHashKey = 0;
}

// 0x4D0C30
CAnimBlendSequence::~CAnimBlendSequence()
{
	// If we allocated memory, clean it
	if (!m_bUsingExternalMemory && m_Frames)
		MemoryMgrFree(m_Frames);
}

static void CopyKeyFrameTrans(KeyFrameTransCompressed *dst, const KeyFrameTrans *src) { dst->Trans = src->Trans; }
static void CopyKeyFrameTrans(KeyFrameTrans *dst, const KeyFrameTransCompressed *src) { dst->Trans = src->Trans; }
static void CopyKeyFrameTrans(KeyFrame *, const KeyFrameCompressed *) {}
static void CopyKeyFrameTrans(KeyFrameCompressed *, const KeyFrame *) {}

template<typename From, typename To>
bool CAnimBlendSequence::ConvertKeyFrames(uint8 *pDataBlock)
{
	if (m_FramesNum == 0)
		return false;

	void *const outFrames = pDataBlock
		? (void *)pDataBlock
		: MemoryMgrMalloc(GetDataSize(!m_bIsCompressed));

	From *inKF = (From *)m_Frames;
	To *outKF = (To *)outFrames;
	for (int32 i = m_FramesNum; i-- > 0; inKF++, outKF++) {
		outKF->Rot = inKF->Rot;
		outKF->DeltaTime = (float)inKF->DeltaTime;
		CopyKeyFrameTrans(outKF, inKF);
	}

	if (!m_bUsingExternalMemory)
		MemoryMgrFree(m_Frames);

	m_Frames = outFrames;
	m_bUsingExternalMemory = pDataBlock != nil;

	return true;
}

// 0x4D0F40
void CAnimBlendSequence::CompressKeyframes(uint8 *pDataBlock)
{
	assert(!m_bIsCompressed);

	bool converted = m_bHasTranslation
		? ConvertKeyFrames<KeyFrameTrans, KeyFrameTransCompressed>(pDataBlock)
		: ConvertKeyFrames<KeyFrame, KeyFrameCompressed>(pDataBlock);
	if (converted)
		m_bIsCompressed = true;
}

// 0x4D0D40
void CAnimBlendSequence::Uncompress(uint8 *pDataBlock)
{
	assert(m_bIsCompressed);

	bool converted = m_bHasTranslation
		? ConvertKeyFrames<KeyFrameTransCompressed, KeyFrameTrans>(pDataBlock)
		: ConvertKeyFrames<KeyFrameCompressed, KeyFrame>(pDataBlock);
	if (converted)
		m_bIsCompressed = false;
}

// 0x4D12A0
void CAnimBlendSequence::RemoveUncompressedData(uint8 *frameData)
{
	if (m_FramesNum)
		CompressKeyframes(frameData);
}

// 0x4D0C90
size_t CAnimBlendSequence::GetDataSize(bool compressed) const
{
	const size_t kfSize = compressed
		? m_bHasTranslation ? sizeof(KeyFrameTransCompressed) : sizeof(KeyFrameCompressed)
		: m_bHasTranslation ? sizeof(KeyFrameTrans) : sizeof(KeyFrame);
	return kfSize * m_FramesNum;
}

// 0x4D1190
void CAnimBlendSequence::RemoveQuaternionFlips() const
{
	if (m_FramesNum < 2)
		return;

	KeyFrame *f;
	CQuaternion last = GetUKeyFrame(0)->Rot;
	for (uint32 i = 1; i < (uint32)m_FramesNum; i++) {
		f = GetUKeyFrame(i);
		if (DotProduct(last, f->Rot) < 0.0f)
			f->Rot = -f->Rot;
		last = f->Rot;
	}
}

// 0x4D0C50
void CAnimBlendSequence::SetName(const char *name)
{
	m_IsUsingBoneTag = false;
	m_NameHashKey = CKeyGen::GetUppercaseKey(name);
}

// 0x4D0C70
void CAnimBlendSequence::SetBoneTag(eBoneTag32 boneId)
{
	if (boneId != BONE_UNKNOWN) {
		m_IsUsingBoneTag = true;
		m_BoneTag = (eBoneTag16)boneId;
	}
}

// 0x4D0CD0
void CAnimBlendSequence::SetNumFrames(uint32 count, bool bHasTranslation, bool compressed, void *frameData)
{
	m_bHasTranslation = bHasTranslation;
	m_FramesNum = (uint16)count;
	m_Frames = frameData ? frameData : MemoryMgrMalloc(GetDataSize(compressed));
	m_bUsingExternalMemory = frameData != nil;
	m_bHasRotation = true;
	m_bIsCompressed = compressed;
}

// 0x4D1150
bool CAnimBlendSequence::MoveMemory()
{
	if (m_bUsingExternalMemory || !m_Frames)
		return false;

	void *prevPtr = m_Frames;
	m_Frames = gMainHeap.MoveMemory(m_Frames);
	return prevPtr != m_Frames; // Return true if the memory was moved
}

// 0x4D1180
void CAnimBlendSequence::Print()
{
}
