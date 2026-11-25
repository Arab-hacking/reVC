// Exercise the real SA animation reader (ANP3 and ANPK) with synthetic IFP
// streams. No proprietary game data, renderer or running game is required.
// The ANIM_COMPRESSION build feeds compressed keyframes and exercises
// CAnimBlendHierarchy::Uncompress; the plain build uses float keyframes.
#include "common.h"

#include "AnimManager.h"
#include "AnimBlendHierarchy.h"
#include "AnimBlendSequence.h"
#include "AnimSequenceFrames.h"
#include "MemoryMgr.h"
#include "eBoneTag.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static void
Check(bool ok, const char *what)
{
	if(!ok){
		fprintf(stderr, "Animation reader test failed: %s\n", what);
		abort();
	}
}

void
re3_assert(const char *expr, const char*, unsigned int, const char*)
{
	fprintf(stderr, "Assertion failed: %s\n", expr);
	abort();
}

// The reader allocates the ANP3 frame block and uncompressed frame data
// through the game memory manager; plain malloc is faithful enough here.
void*
MemoryMgrMalloc(size_t size)
{
	return malloc(size);
}

void
MemoryMgrFree(void *ptr)
{
	free(ptr);
}

class TestStream : public RwStream
{
public:
	std::vector<uint8> bytes;
	size_t pos;
	explicit TestStream(const std::vector<uint8> &data) : bytes(data), pos(0) {}
	uint32 write8(const void*, uint32) override { abort(); }
	uint32 read8(void *out, uint32 n) override {
		size_t count = std::min((size_t)n, bytes.size() - pos);
		memcpy(out, bytes.data() + pos, count);
		pos += count;
		return (uint32)count;
	}
	void seek(int32 off, int32 whence = 1) override {
		if(whence == 0) pos = off;
		else if(whence == 1) pos += off;
		else pos = bytes.size() - off;
		Check(pos <= bytes.size(), "seek within test stream");
	}
	uint32 tell(void) override { return (uint32)pos; }
	bool eof(void) override { return pos == bytes.size(); }
};

RwUInt32
RwStreamRead(RwStream *stream, void *buf, RwUInt32 len)
{
	return stream->read8(buf, len);
}

RwStream*
RwStreamSkip(RwStream *stream, RwUInt32 offset)
{
	stream->seek((int32)offset, 1);
	return stream;
}

static void U32(std::vector<uint8> &v, uint32 value)
{
	for(int b = 0; b < 32; b += 8) v.push_back((uint8)(value >> b));
}
static void I16(std::vector<uint8> &v, int16 value)
{
	v.push_back((uint8)value);
	v.push_back((uint8)((uint16)value >> 8));
}
static void Float(std::vector<uint8> &v, float value)
{
	uint32 bits;
	memcpy(&bits, &value, 4);
	U32(v, bits);
}
static void Patch32(std::vector<uint8> &v, size_t at, uint32 value)
{
	for(int b = 0; b < 32; b += 8) v[at + b / 8] = (uint8)(value >> b);
}
static void Name(std::vector<uint8> &v, const char *name, size_t width)
{
	size_t at = v.size();
	v.resize(at + width, 0);
	Check(strlen(name) < width, "fixture name fits");
	memcpy(v.data() + at, name, strlen(name));
}
static void Chunk(std::vector<uint8> &v, const char *tag, const std::vector<uint8> &body)
{
	v.insert(v.end(), tag, tag + 4);
	U32(v, (uint32)body.size());
	v.insert(v.end(), body.begin(), body.end());
	while(v.size() & 3) v.push_back(0);
}

// ANP3 with one clip of two sequences: a root sequence with translation
// (BoneTag 0) and a rotation-only spine sequence (BoneTag 3). Keyframe
// times are absolute, as in real IFP files; the reader converts them to
// per-frame deltas when it computes the total time.
static std::vector<uint8>
SAAnim_ANP3(bool compressed)
{
	std::vector<uint8> v;
	v.insert(v.end(), "ANP3", "ANP3" + 4);
	U32(v, 0); // section size is not used by the ANP2/3 reader
	Name(v, "ped", 24);
	U32(v, 1); // animation count
	Name(v, "run_test", 24);
	U32(v, 2); // sequence count
	U32(v, compressed ? 52 : 104); // frame data bytes
	U32(v, compressed ? 1 : 0); // flags: bit 0 = compressed frames
	Name(v, " Root", 24); // exporter name with leading space
	U32(v, compressed ? 4 : 2); // KeyFrameTransCompressed / KeyFrameTrans
	U32(v, 2);
	U32(v, 0); // BONE_ROOT
	for(int i = 0; i < 2; i++){
		if(compressed){
			I16(v, 1024); I16(v, 0); I16(v, 0); I16(v, 3966); // +0.25 x rotation
			I16(v, i * 30); // absolute time in 1/60 s
			I16(v, i * 1024); I16(v, 0); I16(v, 0); // +1.0 x translation
		}else{
			Float(v, 0.25f); Float(v, 0); Float(v, 0); Float(v, 0.9682458f);
			Float(v, i * 0.5f); // absolute time, stored before the translation
			Float(v, (float)i); Float(v, 0); Float(v, 0); // translation
		}
	}
	Name(v, " Spine 1", 24);
	U32(v, compressed ? 3 : 1); // KeyFrameCompressed / KeyFrame
	U32(v, 2);
	U32(v, 3); // BONE_SPINE1
	for(int i = 0; i < 2; i++){
		if(compressed){
			I16(v, 2048); I16(v, 0); I16(v, 0); I16(v, 3547); // +0.5 x rotation
			I16(v, i * 30);
		}else{
			Float(v, 0.5f); Float(v, 0); Float(v, 0); Float(v, 0.8660254f);
			Float(v, i * 0.5f);
		}
	}
	return v;
}

// Classic chunked ANPK: rotations in KRT0 sections are conjugated by the
// reader, and the 44-byte ANIM chunk carries the sequence BoneTag.
static std::vector<uint8>
SAAnim_ANPK(void)
{
	std::vector<uint8> body, info;
	U32(info, 1); Name(info, "cars", 16);
	Chunk(body, "INFO", info);
	std::vector<uint8> clipName;
	Name(clipName, "CAR_LB_PRO", 16);
	Chunk(body, "NAME", clipName);
	std::vector<uint8> seqInfo;
	U32(seqInfo, 1);
	std::vector<uint8> dgan;
	Chunk(dgan, "INFO", seqInfo);
	std::vector<uint8> anim(44, 0);
	memcpy(anim.data(), "Root", 4);
	Patch32(anim, 28, 2); // two keyframes
	Patch32(anim, 40, 0); // BoneTag = BONE_ROOT
	std::vector<uint8> cpan;
	Chunk(cpan, "ANIM", anim);
	std::vector<uint8> frames;
	for(int i = 0; i < 2; i++){
		Float(frames, 0.25f); Float(frames, 0); Float(frames, 0); Float(frames, 0.9682458f);
		Float(frames, i * 1.75f); Float(frames, 0); Float(frames, 0); // translation
		Float(frames, (float)i); // absolute time
	}
	Chunk(cpan, "KRT0", frames);
	Chunk(dgan, "CPAN", cpan);
	Chunk(body, "DGAN", dgan);
	std::vector<uint8> v;
	v.insert(v.end(), "ANPK", "ANPK" + 4);
	U32(v, (uint32)body.size());
	v.insert(v.end(), body.begin(), body.end());
	return v;
}

int
main(void)
{
#ifdef ANIM_COMPRESSION
	const bool compressed = true;
#else
	const bool compressed = false;
#endif
	std::vector<uint8> anp3 = SAAnim_ANP3(compressed);
	TestStream stream3(anp3);
	CAnimManager::LoadAnimFile(&stream3, true);
	CAnimBlock *block = CAnimManager::GetAnimationBlock("ped");
	Check(block && block->IsLoaded && block->NumAnims == 1, "ANP3 block registered");
	CAnimBlendHierarchy *clip = CAnimManager::GetAnimation("run_test", block);
	Check(clip != nil, "ANP3 clip found by name");
	Check(clip->m_nSeqCount == 2, "ANP3 clip has two sequences");
	CAnimBlendSequence &root = clip->m_pSequences[0];
	CAnimBlendSequence &spine = clip->m_pSequences[1];
	Check(root.IsUsingBoneTag() && root.GetBoneTag() == BONE_ROOT,
		"root sequence carries BONE_ROOT");
	Check(spine.IsUsingBoneTag() && spine.GetBoneTag() == BONE_SPINE1,
		"spine sequence carries BONE_SPINE1");
	Check(root.m_FramesNum == 2 && spine.m_FramesNum == 2, "frame counts");
	if(compressed){
		Check(clip->m_bIsCompressed && root.IsCompressed(),
			"compressed frames stay compressed");
		KeyFrameTransCompressed *kf = root.GetCKeyFrame(1);
		Check((float)kf->Rot.x == 0.25f && (float)kf->Trans.x == 1.0f &&
			(float)kf->DeltaTime == 0.5f, "compressed keyframe values");
		clip->Uncompress();
		Check(!clip->m_bIsCompressed && !root.IsCompressed(),
			"uncompress clears the compressed flags");
	}
	KeyFrameTrans *lastRoot = root.GetUKeyFrame(1);
	Check(lastRoot->Rot.x == 0.25f && lastRoot->Trans.x == 1.0f &&
		lastRoot->DeltaTime == 0.5f, "root keyframe values");
	KeyFrame *lastSpine = (KeyFrame*)spine.GetUKeyFrame(1);
	Check(lastSpine->Rot.x == 0.5f && lastSpine->DeltaTime == 0.5f,
		"spine keyframe values");
	Check(clip->GetTotalTime() == 0.5f, "ANP3 clip total time");

	std::vector<uint8> anpk = SAAnim_ANPK();
	TestStream streamK(anpk);
	CAnimManager::LoadAnimFile(&streamK, false);
	CAnimBlock *cars = CAnimManager::GetAnimationBlock("cars");
	Check(cars && cars->IsLoaded && cars->NumAnims == 1, "ANPK block registered");
	CAnimBlendHierarchy *clipK = CAnimManager::GetAnimation("CAR_LB_PRO", cars);
	Check(clipK != nil && clipK->m_nSeqCount == 1, "ANPK clip found by name");
	CAnimBlendSequence &kseq = clipK->m_pSequences[0];
	Check(kseq.IsUsingBoneTag() && kseq.GetBoneTag() == BONE_ROOT,
		"ANPK 44-byte ANIM chunk provides the BoneTag");
	KeyFrameTrans *klast = kseq.GetUKeyFrame(1);
	Check(klast->Rot.x == -0.25f && klast->Rot.w == 0.9682458f,
		"ANPK rotation is conjugated");
	Check(klast->Trans.x == 1.75f && klast->DeltaTime == 1.0f,
		"ANPK translation and time");
	Check(clipK->GetTotalTime() == 1.0f, "ANPK clip total time");

	puts("SA animation file reader tests OK");
	return 0;
}
