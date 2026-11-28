#pragma once

#include "Quaternion.h"
#include "AnimBlendSequence.h"
#include "AnimBlendAssociation.h"

/*!
 * @brief Represents the animation of a single node (bone)
 *
 * @details An animation can move one or more nodes (bones).
 * @details Each node has a sequence (that contain key-frames)
 */
class CAnimBlendNode
{
public:
	void Init();

	void CalcTheta(float angle);

	//! @addr 0x4D0190
	void CalcDeltas();

	//! @addr 0x4D0350
	void CalcDeltasCompressed();

	//! @addr 0x4CFC50
	void GetCurrentTranslation(CVector &trans, float weight) { I_GetCurrentTranslation<false>(trans, weight); }

	//! @addr 0x4CFE60
	void GetCurrentTranslationCompressed(CVector &trans, float weight) { I_GetCurrentTranslation<true>(trans, weight); }

	//! @addr 0x4CFD90
	void GetEndTranslation(CVector &trans, float weight) { I_GetEndTranslation<false>(trans, weight); }

	//! @addr 0x4D0000
	void GetEndTranslationCompressed(CVector &trans, float weight) { I_GetEndTranslation<true>(trans, weight); }

	//! @addr 0x4D04A0
	bool NextKeyFrame() { return I_NextKeyFrame<false>(); }

	//! @addr 0x4D0570
	bool NextKeyFrameCompressed() { return I_NextKeyFrame<true>(); }

	//! @addr 0x4CFB90
	bool NextKeyFrameNoCalc();

	//! @addr 0x4D06C0
	bool Update(CVector &trans, CQuaternion &rot, float weight) { return I_Update<false>(trans, rot, weight); }

	//! @addr 0x4D08D0
	bool UpdateCompressed(CVector &trans, CQuaternion &rot, float weight) { return I_Update<true>(trans, rot, weight); }

	bool UpdateTime(); // Unused
	bool SetupKeyFrameCompressed();
	bool FindKeyFrame(float time);
	CAnimBlendAssociation *GetAnimAssoc() const { return m_BlendAssoc; }
	bool IsValid() const { return !!m_Seq; }
	CAnimBlendSequence *GetRootKF() const { return &m_Seq[0]; }
	CAnimBlendSequence *GetSeq() const { return m_Seq; }

	//! @brief Inverse progress of current frame (Eg.: How much is left of this frame)
	template<bool IsCompressed>
	float GetTimeRemainingProgress() const
	{
		typename CAnimBlendSequence::KeyFrameType<IsCompressed>::Type *kf = m_Seq->GetKeyFrame<IsCompressed>(m_KFCurr);
		return (float)kf->DeltaTime == 0.0f
			? 0.0f
			: ((float)kf->DeltaTime - m_KFRemainingTime) / (float)kf->DeltaTime;
	}

	template<bool IsCompressed>
	void I_GetCurrentTranslation(CVector &trans, float weight)
	{
		trans = CVector(0.0f, 0.0f, 0.0f);

		if (!m_Seq->m_bHasTranslation) // Moved up here
			return;

		const float blend = m_BlendAssoc->GetBlendAmount(weight);
		if (blend <= 0.0f)
			return;

		typename CAnimBlendSequence::KeyFrameType<IsCompressed>::Type *kfA = m_Seq->GetKeyFrame<IsCompressed>(m_KFCurr);
		typename CAnimBlendSequence::KeyFrameType<IsCompressed>::Type *kfB = m_Seq->GetKeyFrame<IsCompressed>(m_KFPrev);
		CVector transA = kfA->Trans;
		CVector transB = kfB->Trans;
		trans = (transB + (transA - transB) * GetTimeRemainingProgress<IsCompressed>()) * blend;
	}

	template<bool IsCompressed>
	void I_GetEndTranslation(CVector &trans, float weight)
	{
		trans = CVector(0.0f, 0.0f, 0.0f);

		if (!m_Seq->m_bHasTranslation) // Moved up here
			return;

		const float blend = m_BlendAssoc->GetBlendAmount(weight);
		if (blend <= 0.0f)
			return;

		typename CAnimBlendSequence::KeyFrameType<IsCompressed>::Type *kf = m_Seq->GetKeyFrame<IsCompressed>(m_Seq->m_FramesNum - 1);
		trans = kf->Trans;
		trans *= blend;
	}

	template<bool IsCompressed>
	bool I_NextKeyFrame()
	{
		if (m_Seq->m_FramesNum <= 1)
			return false;

		// Store old KF
		m_KFPrev = m_KFCurr;

		// Find next frame, this should usually be the next one
		// Unless `m_BlendAssoc->m_TimeStep > NextKF->DeltaTime` (In which case `m_KFRemainingTime` becomes negative)
		bool looped = false;
		while (m_KFRemainingTime <= 0.0f) {
			m_KFCurr++;

			if (m_KFCurr >= m_Seq->m_FramesNum) {
				// reached end of animation
				if (!m_BlendAssoc->IsLooped()) {
					m_KFCurr--;
					m_KFRemainingTime = 0.0f;
					return false;
				}
				looped = true;
				m_KFCurr = 0;
			}

			m_KFRemainingTime += (float)m_Seq->GetKeyFrame<IsCompressed>(m_KFCurr)->DeltaTime;
		}

		m_KFPrev = m_KFCurr == 0
			? m_Seq->m_FramesNum - 1 // Previous frame was the last in the sequence
			: m_KFCurr - 1;

		if (IsCompressed)
			CalcDeltasCompressed();
		else
			CalcDeltas();

		return looped;
	}

	template<bool IsCompressed>
	bool I_Update(CVector &trans, CQuaternion &rot, float weight)
	{
		bool looped = false;

		trans = CVector(0.0f, 0.0f, 0.0f);
		rot = CQuaternion(0.0f, 0.0f, 0.0f, 0.0f);

		if (m_BlendAssoc->IsPlaying()) {
			m_KFRemainingTime -= m_BlendAssoc->m_TimeStep;
			if (m_KFRemainingTime <= 0.0f)
				looped = I_NextKeyFrame<IsCompressed>();
		}

		const float blend = m_BlendAssoc->GetBlendAmount(weight);
		if (blend > 0.0f) {
			typename CAnimBlendSequence::KeyFrameType<IsCompressed>::Type *kfA = m_Seq->GetKeyFrame<IsCompressed>(m_KFCurr);
			typename CAnimBlendSequence::KeyFrameType<IsCompressed>::Type *kfB = m_Seq->GetKeyFrame<IsCompressed>(m_KFPrev);

			// Calculate interpolation t
			const float t = GetTimeRemainingProgress<IsCompressed>();

			// Sanity check
			assert(m_Seq->m_bHasTranslation || m_Seq->m_bHasRotation);

			// Update translation
			if (m_Seq->m_bHasTranslation) {
				CVector transA = kfA->Trans;
				CVector transB = kfB->Trans;
				trans = (transB + (transA - transB) * t) * blend;
			}

			// Update rotation
			if (m_Seq->m_bHasRotation) {
				CQuaternion rotA = kfA->Rot;
				CQuaternion rotB = kfB->Rot;
				rot.Slerp(rotB, rotA, m_Theta, m_InvSinTheta, t);
				rot *= blend;
			}
		}

		return looped;
	}

public:
	float m_Theta;                  //!< Angle between current and previous key-frames
	float m_InvSinTheta;            //!< 1 / sin(m_Theta), used in slerp calculation
	int16 m_KFCurr;                 //!< Current key frame (A)
	int16 m_KFPrev;                 //!< Previous key frame (B)
	float m_KFRemainingTime;        //!< Time until frames have to advance
	CAnimBlendSequence *m_Seq;      //!< Node animation key-frames
	CAnimBlendAssociation *m_BlendAssoc; //!< Parent anim to which this node belongs to
};

VALIDATE_SIZE(CAnimBlendNode, 0x18);
