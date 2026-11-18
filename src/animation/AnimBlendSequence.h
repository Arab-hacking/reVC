#ifndef __GTA_ANIMBLENDSEQUENCE_H__
#define __GTA_ANIMBLENDSEQUENCE_H__

#include "AnimSequenceFrames.h"

#ifdef MoveMemory
#undef MoveMemory	// windows shit
#endif


// The sequence of key frames of one animated node
class CAnimBlendSequence
{
public:
	enum {
		KF_ROT = 1,
		KF_TRANS = 2
	};
	int32 type;
	char name[24];
	int32 numFrames;
	int16 boneTag;
	void *keyFrames;
	void *keyFramesCompressed;

	CAnimBlendSequence(void);
	virtual ~CAnimBlendSequence(void);
	void SetName(const char *name);
	void SetNumFrames(int numFrames, bool translation, bool compressed);
	void RemoveQuaternionFlips(void);
	KeyFrame *GetKeyFrame(int n) {
		return type & KF_TRANS ?
			&((KeyFrameTrans*)keyFrames)[n] :
			&((KeyFrame*)keyFrames)[n];
	}
	KeyFrameCompressed *GetKeyFrameCompressed(int n) {
		return type & KF_TRANS ?
			&((KeyFrameTransCompressed*)keyFramesCompressed)[n] :
			&((KeyFrameCompressed*)keyFramesCompressed)[n];
	}
	bool HasTranslation(void) const { return !!(type & KF_TRANS); }
	bool IsCompressed(void) const { return keyFramesCompressed != nil; }
	// Size of one frame record in the active array, zero when it is unused.
	int32 GetKeyFrameElementSize(void) const { return HasTranslation() ? sizeof(KeyFrameTrans) : sizeof(KeyFrame); }
	int32 GetKeyFrameCompressedElementSize(void) const { return HasTranslation() ? sizeof(KeyFrameTransCompressed) : sizeof(KeyFrameCompressed); }
	float GetKeyFrameTime(int n) const {
		if(n < 0 || n >= numFrames)
			return 0.0f;
		if(keyFrames)
			return ((KeyFrame*)keyFrames)[n].deltaTime;
		if(keyFramesCompressed)
			return ((KeyFrameCompressed*)keyFramesCompressed)[n].GetDeltaTime();
		return 0.0f;
	}
	void SetKeyFrameTime(int n, float time);
	void Uncompress(void);
	void CompressKeyframes(void);
	void RemoveUncompressedData(void);
	bool MoveMemory(void);

	void SetBoneTag(int tag) { boneTag = tag; }
};
VALIDATE_SIZE(CAnimBlendSequence, 0x30);

#endif // __GTA_ANIMBLENDSEQUENCE_H__
