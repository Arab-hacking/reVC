#include "common.h"

#include "AnimBlendAssociation.h"
#include "AnimBlendNode.h"
#include "AnimBlendHierarchy.h"
#include "AnimBlendStaticAssociation.h"
#include "AnimBlendClumpData.h"
#include "AnimManager.h"
#include "RpAnimBlend.h"
#include "MemoryMgr.h"

// 0x4CE9B0
CAnimBlendAssociation::CAnimBlendAssociation()
{
	m_AnimGroupId = ANIM_GROUP_NONE;
	m_BlendNodes = nil;
	m_BlendHier = nil;
	m_BlendAmount = 1.0f;
	m_BlendDelta = 0.0f;
	m_CurrentTime = 0.0f;
	m_Speed = 1.0f;
	m_TimeStep = 0.0f;
	m_AnimId = ANIM_ID_UNDEFINED;
	m_Flags = 0;
	m_nCallbackType = ANIM_BLEND_CALLBACK_NONE;
	m_pCallbackFunc = nil;
	m_pCallbackData = nil;
}

// 0x4CEFC0
CAnimBlendAssociation::CAnimBlendAssociation(RpClump *clump, CAnimBlendHierarchy *animHierarchy)
{
	m_AnimGroupId = ANIM_GROUP_NONE;
	m_BlendNodes = nil;
	m_BlendHier = nil;
	m_BlendAmount = 1.0f;
	m_BlendDelta = 0.0f;
	m_CurrentTime = 0.0f;
	m_Speed = 1.0f;
	m_TimeStep = 0.0f;
	m_AnimId = ANIM_ID_UNDEFINED;
	m_Flags = 0;
	m_nCallbackType = ANIM_BLEND_CALLBACK_NONE;
	m_pCallbackFunc = nil;
	m_pCallbackData = nil;
	Init(clump, animHierarchy);
}

// 0x4CF020
CAnimBlendAssociation::CAnimBlendAssociation(CAnimBlendAssociation &assoc)
{
	m_BlendNodes = nil;
	m_BlendAmount = 1.0f;
	m_BlendDelta = 0.0f;
	m_CurrentTime = 0.0f;
	m_Speed = 1.0f;
	m_TimeStep = 0.0f;
	m_nCallbackType = ANIM_BLEND_CALLBACK_NONE;
	m_pCallbackFunc = nil;
	m_pCallbackData = nil;
	Init(assoc);
	if ((m_Flags & ANIMATION_REFERENCE_BLOCK) == 0) {
		CAnimManager::AddAnimBlockRef(m_BlendHier->m_nAnimBlockId);
		m_Flags |= ANIMATION_REFERENCE_BLOCK;
	}
}

// 0x4CF080
CAnimBlendAssociation::CAnimBlendAssociation(CAnimBlendStaticAssociation &assoc)
{
	m_BlendNodes = nil;
	m_BlendAmount = 1.0f;
	m_BlendDelta = 0.0f;
	m_CurrentTime = 0.0f;
	m_Speed = 1.0f;
	m_TimeStep = 0.0f;
	m_nCallbackType = ANIM_BLEND_CALLBACK_NONE;
	m_pCallbackFunc = nil;
	m_pCallbackData = nil;
	Init(assoc);
	if ((m_Flags & ANIMATION_REFERENCE_BLOCK) == 0) {
		CAnimManager::AddAnimBlockRef(m_BlendHier->m_nAnimBlockId);
		m_Flags |= ANIMATION_REFERENCE_BLOCK;
	}
}

// 0x4CECF0
CAnimBlendAssociation::~CAnimBlendAssociation()
{
	if (m_BlendNodes)
		RwFreeAlign(m_BlendNodes);
	m_Link.Remove();
	if (m_Flags & ANIMATION_REFERENCE_BLOCK)
		CAnimManager::RemoveAnimBlockRef(m_BlendHier->m_nAnimBlockId);
}

float CAnimBlendAssociation::GetTimeProgress() const
{
	return m_CurrentTime / m_BlendHier->m_fTotalTime;
}

// 0x4CED50
void CAnimBlendAssociation::Init(RpClump *clump, CAnimBlendHierarchy *animHierarchy)
{
	CAnimBlendClumpData *animClumpData = RpAnimBlendClumpGetData(clump);
	m_NumBlendNodes = (uint16)animClumpData->m_NumFrameData;
	AllocateAnimBlendNodeArray(m_NumBlendNodes);
	for (int32 i = 0; i < m_NumBlendNodes; i++)
		m_BlendNodes[i].m_BlendAssoc = this;

	m_BlendHier = animHierarchy;
	for (uint32 s = 0; s < m_BlendHier->m_nSeqCount; s++) {
		CAnimBlendSequence &seq = m_BlendHier->m_pSequences[s];
		if (!seq.m_FramesNum)
			continue;
		AnimBlendFrameData *frame = seq.IsUsingBoneTag()
			? RpAnimBlendClumpFindBone(clump, seq.GetBoneTag())
			: RpAnimBlendClumpFindFrameFromHashKey(clump, seq.GetNameHashKey());
		if (!frame)
			continue;
		m_BlendNodes[frame - animClumpData->m_FrameDatas].m_Seq = &seq;
	}
}

// 0x4CEE40
void CAnimBlendAssociation::Init(CAnimBlendAssociation &assoc)
{
	m_BlendHier = assoc.m_BlendHier;
	m_NumBlendNodes = assoc.m_NumBlendNodes;
	m_Flags = assoc.m_Flags;
	m_AnimId = assoc.m_AnimId;
	m_AnimGroupId = assoc.m_AnimGroupId;
	AllocateAnimBlendNodeArray(m_NumBlendNodes);
	for (int32 i = 0; i < m_NumBlendNodes; i++) {
		m_BlendNodes[i] = assoc.m_BlendNodes[i];
		m_BlendNodes[i].m_BlendAssoc = this;
	}
}

// 0x4CEEC0
void CAnimBlendAssociation::Init(CAnimBlendStaticAssociation &assoc)
{
	m_BlendHier = assoc.m_BlendHier;
	m_NumBlendNodes = assoc.m_NumBlendNodes;
	m_Flags = assoc.m_Flags;
	m_AnimId = assoc.m_AnimId;
	m_AnimGroupId = assoc.m_AnimGroupId;
	AllocateAnimBlendNodeArray(m_NumBlendNodes);
	for (int32 i = 0; i < m_NumBlendNodes; i++) {
		m_BlendNodes[i].m_Seq = assoc.m_BlendSeqs[i];
		m_BlendNodes[i].m_BlendAssoc = this;
	}
}

// 0x4CEB70
void CAnimBlendAssociation::Start(float currentTime)
{
	m_Flags |= ANIMATION_IS_PLAYING;
	SetCurrentTime(currentTime);
}

uint32 CAnimBlendAssociation::GetHashKey() const
{
	return m_BlendHier->m_hashKey;
}

// 0x4CE9F0
void CAnimBlendAssociation::AllocateAnimBlendNodeArray(int32 count)
{
	m_BlendNodes = (CAnimBlendNode *)RwMallocAlign(count * sizeof(CAnimBlendNode), 64);
	for (int32 i = 0; i < count; i++)
		m_BlendNodes[i].Init();
}

// 0x4CEA40
void CAnimBlendAssociation::FreeAnimBlendNodeArray()
{
	if (m_BlendNodes)
		RwFreeAlign(m_BlendNodes);
}

// 0x4CEBA0
void CAnimBlendAssociation::SetBlend(float blendAmount, float blendDelta)
{
	m_BlendAmount = blendAmount;
	m_BlendDelta = blendDelta;
}

// 0x4CEB80
void CAnimBlendAssociation::SetBlendTo(float blendAmount, float blendDelta)
{
	m_BlendDelta = (blendAmount - m_BlendAmount) * blendDelta;
}

// 0x4CEA80
void CAnimBlendAssociation::SetCurrentTime(float currentTime)
{
	for (m_CurrentTime = currentTime; m_CurrentTime >= m_BlendHier->m_fTotalTime; m_CurrentTime -= m_BlendHier->m_fTotalTime) {
		if (!IsLooped()) {
			m_CurrentTime = m_BlendHier->m_fTotalTime;
			break;
		}
	}

	CAnimManager::UncompressAnimation(m_BlendHier);

	if (m_BlendHier->m_bKeepCompressed) {
		for (int32 i = 0; i < m_NumBlendNodes; i++) {
			if (m_BlendNodes[i].m_Seq)
				m_BlendNodes[i].SetupKeyFrameCompressed();
		}
	} else {
		for (int32 i = 0; i < m_NumBlendNodes; i++) {
			if (m_BlendNodes[i].m_Seq)
				m_BlendNodes[i].FindKeyFrame(m_CurrentTime);
		}
	}
}

// 0x4CEBC0
void CAnimBlendAssociation::SetDeleteCallback(void (*callback)(CAnimBlendAssociation *, void *), void *data)
{
	m_nCallbackType = ANIM_BLEND_CALLBACK_DELETE;
	m_pCallbackFunc = callback;
	m_pCallbackData = data;
}

// 0x4CEBE0
void CAnimBlendAssociation::SetFinishCallback(void (*callback)(CAnimBlendAssociation *, void *), void *data)
{
	m_nCallbackType = ANIM_BLEND_CALLBACK_FINISH;
	m_pCallbackFunc = callback;
	m_pCallbackData = data;
}

// 0x4CEB40
void CAnimBlendAssociation::SyncAnimation(CAnimBlendAssociation *syncWith)
{
	SetCurrentTime(syncWith->GetTimeProgress() * m_BlendHier->GetTotalTime());
}

// 0x4D1490
bool CAnimBlendAssociation::UpdateBlend(float timeStep)
{
	m_BlendAmount += m_BlendDelta * timeStep;

	if (m_BlendAmount <= 0.0f && m_BlendDelta < 0.0f) { // We're faded out and are not fading in
		m_BlendAmount = 0.0f;
		m_BlendDelta = m_BlendDelta > 0.0f ? m_BlendDelta : 0.0f;

		if (m_Flags & ANIMATION_IS_BLEND_AUTO_REMOVE) {
			if (m_nCallbackType == ANIM_BLEND_CALLBACK_DELETE || m_nCallbackType == ANIM_BLEND_CALLBACK_FINISH) {
				m_pCallbackFunc(this, m_pCallbackData);
				SetFinishCallback(CDefaultAnimCallback::DefaultAnimCB, nil);
			}
			delete this;
			return false;
		}
	}

	if (m_BlendAmount > 1.0f) { // Maximally faded in, clamp values
		m_BlendAmount = 1.0f;
		m_BlendDelta = m_BlendDelta < 0.0f ? m_BlendDelta : 0.0f;
	}

	return true;
}

// 0x4D13D0
bool CAnimBlendAssociation::UpdateTime(float timeStep, float timeMult)
{
	(void)timeStep;
	(void)timeMult;

	if (!IsPlaying())
		return true;

	// Finished yet?
	if (m_CurrentTime >= (double)m_BlendHier->GetTotalTime()) {
		SetFlag(ANIMATION_IS_PLAYING, false);
		return true;
	}

	// Okay, so advance...
	m_CurrentTime += m_TimeStep;

	// Is it finished now?
	if (m_CurrentTime < m_BlendHier->GetTotalTime())
		return true;

	// Should it be repeating? If so, jump to beginning
	if (IsLooped()) {
		m_CurrentTime -= m_BlendHier->GetTotalTime();
		return true;
	}

	// Anim has finished
	m_CurrentTime = m_BlendHier->GetTotalTime();

	// Maybe auto-remove
	if (m_Flags & ANIMATION_IS_FINISH_AUTO_REMOVE) {
		SetFlag(ANIMATION_IS_BLEND_AUTO_REMOVE, true);
		m_BlendDelta = -4.0f;
	}

	// Call finish callback (if any)
	if (m_nCallbackType == ANIM_BLEND_CALLBACK_FINISH) {
		m_nCallbackType = ANIM_BLEND_CALLBACK_NONE;
		m_pCallbackFunc(this, m_pCallbackData);
		SetFinishCallback(CDefaultAnimCallback::DefaultAnimCB, nil);
	}

	// And we're done
	return true;
}

// 0x4D13A0
void CAnimBlendAssociation::UpdateTimeStep(float timeStep, float totalTimeRecp)
{
	if (IsPlaying()) {
		m_TimeStep = IsSyncronised()
			? m_BlendHier->m_fTotalTime * totalTimeRecp
			: m_Speed;
		m_TimeStep *= timeStep;
	}
}

bool CAnimBlendAssociation::HasFinished() const
{
	return m_CurrentTime == m_BlendHier->m_fTotalTime;
}

// 0x4CEA50
void CAnimBlendAssociation::ReferenceAnimBlock()
{
	if (m_Flags & ANIMATION_REFERENCE_BLOCK)
		return;
	CAnimManager::AddAnimBlockRef(m_BlendHier->m_nAnimBlockId);
	SetFlag(ANIMATION_REFERENCE_BLOCK, true);
}

// 0x4CEB60
CAnimBlendNode *CAnimBlendAssociation::GetNode(int32 nodeIndex)
{
	return &m_BlendNodes[nodeIndex];
}
