#pragma once

#include "AnimSequenceFrames.h"
#include "eBoneTag.h"

/*!
 * @brief Animation key-frames of a single node (bone)
 */
class CAnimBlendSequence
{
public:
	CAnimBlendSequence(); // 0x4D0C10
	~CAnimBlendSequence();

	void CompressKeyframes(uint8 *frameData);
	size_t GetDataSize(bool compressed) const;
	bool MoveMemory();
	void Print();
	void RemoveQuaternionFlips() const;
	void RemoveUncompressedData(uint8 *frameData);
	void SetBoneTag(eBoneTag32 boneId);
	eBoneTag32 GetBoneTag() const { assert(m_IsUsingBoneTag); return m_BoneTag; }
	void SetName(const char *string);
	uint32 GetNameHashKey() const { assert(!m_IsUsingBoneTag); return m_NameHashKey; }
	void SetNumFrames(uint32 count, bool bHasTranslation, bool compressed, void *frameData);
	bool IsUsingBoneTag() const { return m_IsUsingBoneTag; }
	void Uncompress(uint8 *frameData);
	bool IsCompressed() const { return m_bIsCompressed; }
	bool HasTranslation() const { return m_bHasTranslation; }

	template<bool AsCompressed>
	struct KeyFrameType;

	/*!
	 * @brief Used to implement copy-paste functions
	 * @tparam AsCompressed Whenever `IsCompressed()` is returning true/false at the runtime.
	 * @param n The index of the key-frame
	 * @return The `n`th key-frame
	 */
	template<bool AsCompressed>
	typename KeyFrameType<AsCompressed>::Type *GetKeyFrame(size_t n) const;

	/*!
	 * @brief Get the `n`th key-frame (Use this only for un-compressed anims (if `!IsCompressed()`))
	 * @brief The true return type is `KeyFrameTrans*` (if `HasTranslation()`) or `KeyFrame*`,
	 * @brief The former is just returned for convenience.
	 * @param n The index of the key-frame
	 * @return The `n`th key-frame
	 */
	KeyFrameTrans *GetUKeyFrame(size_t n) const // `U` = Uncompressed
	{
		assert(n < (size_t)m_FramesNum);
		assert(!m_bIsCompressed); // NOTE: If this is ever hit see the note in `UncompressAnimation`
		return (KeyFrameTrans *)(m_bHasTranslation ? // Lie a little by always up-casting to `KeyFrameTrans*` to make our lifes easier
			&((KeyFrameTrans*)m_Frames)[n] :
			&((KeyFrame*)m_Frames)[n]);
	}

	/*!
	 * @brief Get the `n`th key-frame (Use this only for compressed anims (if `IsCompressed()`))
	 * @brief The true return type is `KeyFrameTransCompressed*` (if `HasTranslation()`) or `KeyFrameCompressed*`,
	 * @brief The former is just returned for convenience.
	 * @param n The index of the key-frame
	 * @return The `n`th key-frame
	 */
	KeyFrameTransCompressed *GetCKeyFrame(size_t n) const // `C` = Compressed
	{
		assert(n < (size_t)m_FramesNum);
		assert(m_bIsCompressed); // NOTE: If this is ever hit see the note in `UncompressAnimation`
		return (KeyFrameTransCompressed *)(m_bHasTranslation ? // Lie a little by always up-casting to `KeyFrameTransCompressed*` to make our lifes easier
			&((KeyFrameTransCompressed*)m_Frames)[n] :
			&((KeyFrameCompressed*)m_Frames)[n]);
	}

private:
	/*!
	 * @brief Implementation for compression/decompression of key-frames
	 * @tparam From Key-Frame type to convert from
	 * @tparam To Key-Frame type to convert to
	 * @param frameData Pre-allocated frame data buffer
	 */
	template<typename From, typename To>
	bool ConvertKeyFrames(uint8 *pDataBlock);

private:
	//! The bone/frame this sequence is associated with
	union {
		eBoneTag16 m_BoneTag;   // IF m_IsUsingBoneTag == TRUE
		uint32     m_NameHashKey; // IF m_IsUsingBoneTag == FALSE
	};

public:
	uint16 m_bHasRotation : 1;
	uint16 m_bHasTranslation : 1;      // Root key frames have translation values (quaternion).
	uint16 m_bIsCompressed : 1;        // Compressed key frames.
	uint16 m_bUsingExternalMemory : 1; // When this flag is NOT set, you have to loop through all key frames in m_pFrames and free them separately.
	uint16 m_IsUsingBoneTag : 1;
	uint16 m_FramesNum;
	void *m_Frames;

	CAnimBlendSequence(const CAnimBlendSequence &);
	CAnimBlendSequence &operator=(const CAnimBlendSequence &);
};

template<>
struct CAnimBlendSequence::KeyFrameType<false>
{
	typedef KeyFrameTrans Type;
};

template<>
struct CAnimBlendSequence::KeyFrameType<true>
{
	typedef KeyFrameTransCompressed Type;
};

template<>
inline KeyFrameTrans *CAnimBlendSequence::GetKeyFrame<false>(size_t n) const
{
	assert(IsCompressed() == false);
	return GetUKeyFrame(n);
}

template<>
inline KeyFrameTransCompressed *CAnimBlendSequence::GetKeyFrame<true>(size_t n) const
{
	assert(IsCompressed() == true);
	return GetCKeyFrame(n);
}

VALIDATE_SIZE(CAnimBlendSequence, 0xC);
