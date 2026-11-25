#pragma once

#include "AnimBlendSequence.h"
#include "templates.h"

/*!
 * @brief The animation object.
 *
 * @details It contains `CAnimBlendSequence`'s each of which is the animation for one bone (node).
 * @details The data from here is copied to `CAnimBlendAssociation` when an animation is requested for a clump.
 * @details It is never destroyed and stays in memory unless `CStreaming` forces the IFP to unload to free up memory.
 */
class CAnimBlendHierarchy
{
public:
	uint32 m_hashKey;
	CAnimBlendSequence *m_pSequences; //!< Per-node animations - NOTE: Order of these depends on the order of nodes in Clump this was built from
	uint16 m_nSeqCount;
	bool m_bIsCompressed;
	bool m_bKeepCompressed;
	int32 m_nAnimBlockId;
	float m_fTotalTime;
	CLink<CAnimBlendHierarchy*> *m_Link; //!< Link to the next animation in the block (?)

public:
	CAnimBlendHierarchy();
	~CAnimBlendHierarchy();

	void Shutdown();

	uint8 *AllocSequenceBlock(bool compressed) const;

	//! @addr 0x4CF2F0
	void CalcTotalTime() { ICalcTotalTime<false>(); }

	//! @addr 0x4CF3E0
	void CalcTotalTimeCompressed() { ICalcTotalTime<true>(); }

	void RemoveAnimSequences();
	void RemoveQuaternionFlips() const;
	void RemoveUncompressedData();

	void SetName(const char *string);
	void Uncompress();

	CAnimBlendSequence *FindSequence(const char *name) const;
	void *GetSequenceBlock() const;
	void CompressKeyframes() const;

	void MoveMemory();
	void Print();

	CAnimBlendSequence *GetSequences() const { return m_pSequences; }
	uint32 GetNumSequences() const { return m_nSeqCount; }
	uint32 GetHashKey() const { return m_hashKey; }
	float GetTotalTime() const { return m_fTotalTime; }
	bool IsRunningCompressed() const { return m_bKeepCompressed; }
	bool IsUncompressed() const { return !m_bIsCompressed; }
	void SetNumSequences(size_t n);

	uint32 GetIndex() const;

private:
	template<bool Compressed>
	void ICalcTotalTime()
	{
		m_fTotalTime = 0.0f;
		for (uint32 i = 0; i < m_nSeqCount; i++) {
			CAnimBlendSequence &seq = m_pSequences[i];
			if (seq.m_FramesNum == 0) // FIX_BUGS by Mitchell Tobass
				continue;
			float lastTime = (float)seq.GetKeyFrame<Compressed>(seq.m_FramesNum - 1)->DeltaTime;
			if (lastTime > m_fTotalTime)
				m_fTotalTime = lastTime;
			for (int32 j = seq.m_FramesNum; j-- > 1;) {
				typename CAnimBlendSequence::KeyFrameType<Compressed>::Type *kfA = seq.GetKeyFrame<Compressed>(j - 1);
				typename CAnimBlendSequence::KeyFrameType<Compressed>::Type *kfB = seq.GetKeyFrame<Compressed>(j);
				kfB->DeltaTime = kfB->DeltaTime - kfA->DeltaTime;
			}
		}
	}
};

VALIDATE_SIZE(CAnimBlendHierarchy, 0x18);
