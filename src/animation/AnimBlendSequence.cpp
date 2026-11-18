#include "common.h"

#include "AnimBlendSequence.h"
#include "MemoryHeap.h"

CAnimBlendSequence::CAnimBlendSequence(void)
{
	type = 0;
	name[0] = '\0';
	numFrames = 0;
	boneTag = -1;
	keyFrames = nil;
	keyFramesCompressed = nil;
}

CAnimBlendSequence::~CAnimBlendSequence(void)
{
	if(keyFrames)
		RwFree(keyFrames);
	if(keyFramesCompressed)
		RwFree(keyFramesCompressed);
}

void
CAnimBlendSequence::SetName(const char *name)
{
	strncpy(this->name, name, sizeof(this->name) - 1);
	this->name[sizeof(this->name) - 1] = '\0';
}

void
CAnimBlendSequence::SetNumFrames(int numFrames, bool translation, bool compressed)
{
	type = translation ? (KF_ROT | KF_TRANS) : KF_ROT;
	if(compressed)
		keyFramesCompressed = RwMalloc(KeyFrameCompressedSize(translation) * numFrames);
	else
		keyFrames = RwMalloc(KeyFrameSize(translation) * numFrames);
	this->numFrames = numFrames;
}

void
CAnimBlendSequence::SetKeyFrameTime(int n, float time)
{
	if(n < 0 || n >= numFrames)
		return;
	if(keyFrames)
		GetKeyFrame(n)->deltaTime = time;
	else if(keyFramesCompressed)
		GetKeyFrameCompressed(n)->SetTime(time);
}

// Makes every key use the same hemisphere as the previous one. Without this the
// shortest-path interpolation between neighbours would flip.
void
CAnimBlendSequence::RemoveQuaternionFlips(void)
{
	if(numFrames < 2)
		return;

	// A sequence holds either the uncompressed or the compressed array, never
	// both, so each branch has to read its own storage.
	if(keyFrames){
		CQuaternion last = GetKeyFrame(0)->rotation;
		for(int i = 1; i < numFrames; i++){
			KeyFrame *frame = GetKeyFrame(i);
			float dot = DotProduct(last, frame->rotation);
			if(dot < 0.0f){
				frame->rotation.x = -frame->rotation.x;
				frame->rotation.y = -frame->rotation.y;
				frame->rotation.z = -frame->rotation.z;
				frame->rotation.w = -frame->rotation.w;
			}
			last = frame->rotation;
		}
		return;
	}

	if(keyFramesCompressed){
		CQuaternion last;
		GetKeyFrameCompressed(0)->GetRotation(&last);
		for(int i = 1; i < numFrames; i++){
			CQuaternion rot;
			KeyFrameCompressed *frame = GetKeyFrameCompressed(i);
			frame->GetRotation(&rot);
			float dot = DotProduct(last, rot);
			if(dot < 0.0f){
				frame->rot[0] = (int16)-frame->rot[0];
				frame->rot[1] = (int16)-frame->rot[1];
				frame->rot[2] = (int16)-frame->rot[2];
				frame->rot[3] = (int16)-frame->rot[3];
				rot.x = -rot.x; rot.y = -rot.y; rot.z = -rot.z; rot.w = -rot.w;
			}
			last = rot;
		}
	}
}

// Expands the compressed array into the uncompressed one, keeping the
// compressed data so a later CompressKeyframes can drop it again.
void
CAnimBlendSequence::Uncompress(void)
{
	if(keyFramesCompressed == nil || keyFrames != nil)
		return;

	keyFrames = RwMalloc(KeyFrameSize(HasTranslation()) * numFrames);
	for(int i = 0; i < numFrames; i++){
		CQuaternion rot;
		GetKeyFrameCompressed(i)->GetRotation(&rot);
		KeyFrame *dst = GetKeyFrame(i);
		dst->rotation = rot;
		dst->deltaTime = GetKeyFrameCompressed(i)->GetDeltaTime();
		if(HasTranslation()){
			CVector trans;
			((KeyFrameTransCompressed*)GetKeyFrameCompressed(i))->GetTranslation(&trans);
			((KeyFrameTrans*)dst)->translation = trans;
		}
	}
}

// Copies the live frame array into a buffer of exactly its own size, dropping
// the slack left by the pool allocation. Returns false when nothing was moved.
bool
CAnimBlendSequence::MoveMemory(void)
{
	void *shrunk;
	int32 size;

	if(numFrames <= 0)
		return false;
	if(keyFrames != nil){
		size = GetKeyFrameElementSize();
		shrunk = RwMalloc(size * numFrames);
		memcpy(shrunk, keyFrames, size * numFrames);
		RwFree(keyFrames);
		keyFrames = shrunk;
		return true;
	}
	if(keyFramesCompressed != nil){
		size = GetKeyFrameCompressedElementSize();
		shrunk = RwMalloc(size * numFrames);
		memcpy(shrunk, keyFramesCompressed, size * numFrames);
		RwFree(keyFramesCompressed);
		keyFramesCompressed = shrunk;
		return true;
	}
	return false;
}

// Builds the compressed array from the uncompressed one. The compressed layout
// is what vehicle and bike banks ship, so the engine has to be able to produce
// it when a bank arrives uncompressed and the caller asks to keep it packed.
void
CAnimBlendSequence::CompressKeyframes(void)
{
	if(keyFrames == nil || keyFramesCompressed != nil || numFrames <= 0)
		return;

	const bool trans = HasTranslation();
	keyFramesCompressed = RwMalloc(KeyFrameCompressedSize(trans) * numFrames);
	for(int i = 0; i < numFrames; i++){
		KeyFrame *src = GetKeyFrame(i);
		KeyFrameCompressed *dst = GetKeyFrameCompressed(i);
		dst->SetRotation(src->rotation);
		dst->SetTime(src->deltaTime);
		if(trans){
			KeyFrameTrans *srcT = (KeyFrameTrans*)src;
			((KeyFrameTransCompressed*)dst)->SetTranslation(srcT->translation);
		}
	}
}

void
CAnimBlendSequence::RemoveUncompressedData(void)
{
	if(keyFrames){
		RwFree(keyFrames);
		keyFrames = nil;
	}
}
