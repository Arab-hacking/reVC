#ifndef __GTA_ANIMSEQUENCEFRAMES_H__
#define __GTA_ANIMSEQUENCEFRAMES_H__

#include "Quaternion.h"

// Scale factors used by the file loaders. They live here because both the
// loader and the compressed-key accessors must agree on them.
#define ANIM_QUAT_SCALE		(1.0f/4096.0f)
#define ANIM_TRANS_SCALE	(1.0f/1024.0f)
#define ANIM_TIME_SCALE		(1.0f/60.0f)

// Key frame layouts taken from the ANP2/ANP3 specification: the on-disk record
// sizes and the in-memory layouts are the same, and the type of a sequence
// decides which of the two in-memory shapes it uses.
//
//   uncompressed, rotation only          20 bytes  (0x14)
//   uncompressed, rotation + translation 32 bytes  (0x20)
//   compressed,   rotation only          10 bytes  (0x0a)
//   compressed,   rotation + translation 16 bytes  (0x10)
//
// The loaders convert compressed records into the uncompressed shape, so both
// arrays exist and are independent.

// Times in the file are absolute; after loading they are replaced by the
// duration of the interval that ends at that key, hence "delta".
struct KeyFrame
{
	CQuaternion rotation;
	float deltaTime;	// relative to the previous key frame
};

struct KeyFrameTrans : KeyFrame
{
	CVector translation;
};

// Quaternion components are int16 scaled by 2^-12, time is a uint16 in frames
// at 60 Hz, translation is int16 scaled by 2^-10.
struct KeyFrameCompressed
{
	int16 rot[4];
	int16 deltaTime;	// uint16 frames at 60 Hz, split so the layout matches

	float GetDeltaTime(void) const { return (uint16)deltaTime * ANIM_TIME_SCALE; }
	void SetTime(float t) { deltaTime = (int16)(uint16)(t * 60.0f + 0.5f); }

	void GetRotation(CQuaternion *quat) const {
		const float scale = 1.0f/4096.0f;
		quat->x = rot[0]*scale;
		quat->y = rot[1]*scale;
		quat->z = rot[2]*scale;
		quat->w = rot[3]*scale;
	}
	void SetRotation(const CQuaternion &quat) {
		rot[0] = (int16)(quat.x * 4096.0f);
		rot[1] = (int16)(quat.y * 4096.0f);
		rot[2] = (int16)(quat.z * 4096.0f);
		rot[3] = (int16)(quat.w * 4096.0f);
	}
};

struct KeyFrameTransCompressed : KeyFrameCompressed
{
	int16 trans[3];

	void GetTranslation(CVector *vec) const {
		const float scale = 1.0f/1024.0f;
		vec->x = trans[0]*scale;
		vec->y = trans[1]*scale;
		vec->z = trans[2]*scale;
	}
	void SetTranslation(const CVector &vec) {
		trans[0] = (int16)(vec.x * 1024.0f);
		trans[1] = (int16)(vec.y * 1024.0f);
		trans[2] = (int16)(vec.z * 1024.0f);
	}
};

VALIDATE_SIZE(KeyFrame, 0x14);
VALIDATE_SIZE(KeyFrameTrans, 0x20);
VALIDATE_SIZE(KeyFrameCompressed, 0x0a);
VALIDATE_SIZE(KeyFrameTransCompressed, 0x10);

// Frame counts a sequence can hold, used by every accessor.
inline int32
KeyFrameSize(bool translation)
{
	return translation ? (int32)sizeof(KeyFrameTrans) : (int32)sizeof(KeyFrame);
}

inline int32
KeyFrameCompressedSize(bool translation)
{
	return translation ? (int32)sizeof(KeyFrameTransCompressed) : (int32)sizeof(KeyFrameCompressed);
}

#endif // __GTA_ANIMSEQUENCEFRAMES_H__
