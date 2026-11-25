#pragma once

#include "AnimationEnums.h"

class CAnimBlendSequence;
class CAnimBlendHierarchy;

//! Stores static animation data loaded from an animation clump.
class CAnimBlendStaticAssociation
{
public:
	CAnimBlendStaticAssociation() : m_NumBlendNodes(0), m_AnimId(ANIM_ID_UNDEFINED), m_AnimGroupId(ANIM_GROUP_NONE), m_Flags(0), m_BlendSeqs(nil), m_BlendHier(nil) {}
	CAnimBlendStaticAssociation(RpClump *clump, CAnimBlendHierarchy *hier);
	virtual ~CAnimBlendStaticAssociation();

	void Init(RpClump *clump, CAnimBlendHierarchy *hier);
	void AllocateSequenceArray(int32 count);
	void FreeSequenceArray();

	uint32 GetHashKey() const;

	bool IsValid() const { return !!m_BlendSeqs; }
	CAnimBlendHierarchy *GetAnimHierarchy() const { return m_BlendHier; }

public:
	uint16 m_NumBlendNodes;
	int16 m_AnimId;
	int16 m_AnimGroupId;
	uint16 m_Flags;
	CAnimBlendSequence **m_BlendSeqs;
	CAnimBlendHierarchy *m_BlendHier;
};

VALIDATE_SIZE(CAnimBlendStaticAssociation, 0x14);
