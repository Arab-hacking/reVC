#include "common.h"

#include "AnimBlendNode.h"
#include "AnimBlendAssociation.h"

// 0x4CFB70
void CAnimBlendNode::Init()
{
	m_Theta = 0.0f;
	m_InvSinTheta = 0.0f;
	m_KFCurr = -1;
	m_KFPrev = -1;
	m_KFRemainingTime = 0.0f;
	m_Seq = nil;
	m_BlendAssoc = nil;
}

// 0x4D0240
bool CAnimBlendNode::FindKeyFrame(float time)
{
	if (m_Seq->m_FramesNum == 0)
		return false;

	m_KFCurr = 0;
	m_KFPrev = 0;

	if (m_Seq->m_FramesNum == 1) {
		m_KFRemainingTime = 0.0f;
	} else {
		// Find kf that spans over the specified `time`
		for (KeyFrameTrans *kf = m_Seq->GetUKeyFrame(++m_KFCurr); time > kf->DeltaTime; m_KFPrev = m_KFCurr++) {
			time -= m_Seq->GetUKeyFrame(m_KFCurr)->DeltaTime;

			if (m_KFCurr + 1 >= m_Seq->m_FramesNum) {
				// reached end of animation
				if (!m_BlendAssoc->IsLooped()) {
					CalcDeltas();
					m_KFRemainingTime = 0.0f;
					return false;
				}
				m_KFCurr = 0;
			}
		}

		// Now calculate how much we have remaining from this frame
		m_KFRemainingTime = m_Seq->GetUKeyFrame(m_KFCurr)->DeltaTime - time;
	}

	CalcDeltas();
	return true;
}

// 0x4D00E0
void CAnimBlendNode::CalcTheta(float angleCos)
{
	m_Theta = acosf(angleCos < 1.0f ? angleCos : 1.0f);
	m_InvSinTheta = m_Theta == 0.0f
		? 0.0f
		: 1.0f / sinf(m_Theta);
}

// 0x4D0190
void CAnimBlendNode::CalcDeltas()
{
	if (!m_Seq->m_bHasRotation)
		return;

	CalcTheta(
		DotProduct(
			m_Seq->GetUKeyFrame(m_KFCurr)->Rot,
			m_Seq->GetUKeyFrame(m_KFPrev)->Rot
		)
	);
}

// 0x4D0350
void CAnimBlendNode::CalcDeltasCompressed()
{
	if (!m_Seq->m_bHasRotation)
		return;

	KeyFrameTransCompressed *kfA = m_Seq->GetCKeyFrame(m_KFCurr);
	KeyFrameTransCompressed *kfB = m_Seq->GetCKeyFrame(m_KFPrev);

	CQuaternion rotA = kfA->Rot, rotB = kfB->Rot;
	const float angleCos = DotProduct(rotA, rotB);
	if (angleCos < 0.0f) {
		rotB = -rotB;
		kfB->Rot = rotB;
	}
	CalcTheta(angleCos);
}

// 0x4CFB90 - Unused
bool CAnimBlendNode::NextKeyFrameNoCalc()
{
	return false;
}

// 0x4D0650
bool CAnimBlendNode::SetupKeyFrameCompressed()
{
	if (m_Seq->m_FramesNum < 1)
		return false;

	m_KFCurr = 1;
	m_KFPrev = 0;

	if (m_Seq->m_FramesNum == 1) {
		m_KFCurr = 0;
		m_KFRemainingTime = 0.0f;
	} else {
		m_KFRemainingTime = (float)m_Seq->GetCKeyFrame(m_KFCurr)->DeltaTime;
	}

	CalcDeltasCompressed();
	return true;
}

// 0x4D0160 - Unused
bool CAnimBlendNode::UpdateTime()
{
	if (m_BlendAssoc->IsPlaying()) {
		m_KFRemainingTime -= m_BlendAssoc->m_TimeStep;
		if (m_KFRemainingTime <= 0.0f)
			return NextKeyFrameNoCalc();
	}
	return false;
}
