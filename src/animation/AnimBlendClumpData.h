#pragma once

#include "Vector.h"
#include "AnimBlendFrameData.h"
#include "AnimBlendAssociation.h"

class CAnimBlendClumpData
{
public:
	CAnimBlendLink m_AnimList; //!< List of `CAnimBlendAssociation` - List of anims that are being played on this clump
	union {
		uint32 m_NumFrameData; // For skinned clumps
		uint32 m_NumBones;     // For non-skinned clumps
	};
	CVector *m_PedPosition;
	AnimBlendFrameData *m_FrameDatas; // There's always at least 1 frame present

public:
	CAnimBlendClumpData();
	~CAnimBlendClumpData();

	void ForAllFrames(void (*callback)(AnimBlendFrameData *, void *), void *data);

	CAnimBlendLink &GetAnims() { return m_AnimList; }

	AnimBlendFrameData &GetRootFrameData() const { assert(m_NumFrameData >= 1); return m_FrameDatas[0]; }
	void SetNumberOfBones(uint32 numBones);
	AnimBlendFrameData *GetFrames() { return m_FrameDatas; }
};

VALIDATE_SIZE(CAnimBlendClumpData, 0x14);
