// Exercise the real animation reader with synthetic IFP/ANI streams. No
// proprietary game data, renderer or running game is required.
#include "common.h"
#include "AnimManager.h"
#include "AnimBlendAssocGroup.h"
#include "AnimBlendAssociation.h"
#include "AnimBlendClumpData.h"
#include "AnimBlendSequence.h"
#include "RpAnimBlend.h"
#include "CustomModels.h"

#include <algorithm>
#include <cstdarg>
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

void re3_assert(const char *expr, const char*, unsigned int, const char*)
{
	fprintf(stderr, "Assertion failed: %s\n", expr);
	abort();
}
void re3_log(const char*, const char*, unsigned int, const char*, const char*, ...) {}
void *RwMalloc(size_t n) { return malloc(n); }
void RwFree(void *p) { free(p); }
RwUInt32 RwStreamRead(RwStream *s, void *buf, RwUInt32 len) { return s->read8(buf, len); }
void DebugLogPrintf(const char*, ...) {}

RwInt32 ClumpOffset = 0;
CAnimBlendClumpData::CAnimBlendClumpData(void)
  : numFrames(0), velocity2d(nil), usesSAAnimations(false), frames(nil)
{
	link.Init();
}
CAnimBlendClumpData::~CAnimBlendClumpData(void) {}
bool RpAnimBlendClumpIsInitialized(RpClump *clump)
{
	CAnimBlendClumpData *data = *RPANIMBLENDCLUMPDATA(clump);
	return data && data->numFrames > 0 && data->frames;
}
struct FakeClump
{
	CAnimBlendClumpData *plugin;
	CAnimBlendClumpData data;
	AnimBlendFrameData frame;
	explicit FakeClump(bool sa) : plugin(&data) {
		data.frames = &frame;
		data.numFrames = 1;
		data.usesSAAnimations = sa;
	}
	RpClump *Get(void) { return (RpClump*)&plugin; }
};

static std::vector<uint8> customFile;
static int customStreamsOpened = 0;
static int nativeStreamsOpened = 0;
static std::vector<uint8> NativeANPK();
bool CCustomModels::LoadAnimFileFromCustom(const char*, std::vector<uint8> &out)
{
	out = customFile;
	return !out.empty();
}

// The native-reference groups deliberately do not exist in this test. This
// reproduces VC=none even though the original VC ped dictionary was loaded.
CAnimBlendAssociation *CAnimBlendAssocGroup::GetAnimation(uint32) { return nil; }

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

RwStream *RwStreamOpen(RwStreamType type, RwStreamAccessType access, const void *arg)
{
	Check(access == rwSTREAMREAD, "animation streams are read-only");
	if(type == rwSTREAMFILENAME){
		Check(strcmp((const char*)arg, "ANIM\\PED.IFP") == 0, "native VC IFP requested");
		nativeStreamsOpened++;
		return new TestStream(NativeANPK());
	}
	Check(type == rwSTREAMMEMORY, "custom SA IFP comes from a memory stream");
	const RwMemory *mem = (const RwMemory*)arg;
	customStreamsOpened++;
	const uint8 *bytes = (const uint8*)mem->start;
	return new TestStream(std::vector<uint8>(bytes, bytes + mem->length));
}
RwBool RwStreamClose(RwStream *stream, void*)
{
	delete stream;
	return true;
}

static void U16(std::vector<uint8> &v, uint16 value)
{
	v.push_back((uint8)value);
	v.push_back((uint8)(value >> 8));
}
static void U32(std::vector<uint8> &v, uint32 value)
{
	for(int b = 0; b < 32; b += 8) v.push_back((uint8)(value >> b));
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

static std::vector<uint8>
MobileANI(uint32 groupVal, bool compressed, bool mixed = false)
{
	std::vector<uint8> v;
	v.insert(v.end(), "ANP3", "ANP3" + 4);
	Name(v, "ped", 24);
	U32(v, 1);                 // animation count
	U32(v, groupVal);          // exporter-specific, ignored
	Name(v, "CAR_getin_LHS", 24);
	U32(v, mixed ? 2 : 1);     // number of bone sequences
	U32(v, (compressed ? 32 : 64) + (mixed ? 40 : 0)); // frame data bytes
	U32(v, 1);                 // exporter word, NOT a compression flag
	Name(v, " Root", 24);    // name with leading space
	U32(v, compressed ? 4 : 2); // compressed or float root translation
	U32(v, 2);
	U32(v, 0);                 // SA root bone ID
	for(int i = 0; i < 2; i++){
		if(compressed){
			U16(v, 1024); U16(v, 0); U16(v, 0); U16(v, 3966); // +0.25 x rotation
			U16(v, i * 60); // absolute time in 1/60 s
			U16(v, i * 512); U16(v, 0); U16(v, 0); // +0.5 x translation
		}else{
			Float(v, 0.25f); Float(v, 0); Float(v, 0); Float(v, 0.9682458f);
			Float(v, i * 0.5f); Float(v, 0); Float(v, 0); Float(v, (float)i);
		}
	}
	if(mixed){
		Name(v, " Spine 1", 24);
		U32(v, 1); U32(v, 2); U32(v, 3); // float rotation, SA spine1
		for(int i = 0; i < 2; i++){
			Float(v, 0.5f); Float(v, 0); Float(v, 0); Float(v, 0.8660254f);
			Float(v, (float)i);
		}
	}
	return v;
}

static std::vector<uint8>
NativeANPK(void)
{
	std::vector<uint8> body, info;
	U32(info, 1); Name(info, "ped", 4);
	Chunk(body, "INFO", info);
	std::vector<uint8> clipName;
	Name(clipName, "CAR_getin_LHS", 16);
	Chunk(body, "NAME", clipName);
	std::vector<uint8> seqInfo;
	U32(seqInfo, 1);
	std::vector<uint8> dgan;
	Chunk(dgan, "INFO", seqInfo);
	std::vector<uint8> anim(44, 0);
	memcpy(anim.data(), "Root", 4);
	Patch32(anim, 28, 2); // two keyframes
	std::vector<uint8> cpan;
	Chunk(cpan, "ANIM", anim);
	std::vector<uint8> frames;
	for(int i = 0; i < 2; i++){
		Float(frames, 0.25f); Float(frames, 0); Float(frames, 0); Float(frames, 0.9682458f);
		Float(frames, i * 1.75f); Float(frames, 0); Float(frames, 0); Float(frames, (float)i);
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
	std::vector<uint8> compact = MobileANI(1, true);
	TestStream mobile(compact);
	CAnimManager::LoadAnimFile(&mobile, true, nil, "sa_ped");
	CAnimBlock *sa = CAnimManager::GetAnimationBlock("sa_ped");
	Check(sa && sa->isLoaded && sa->numAnims == 1, "mobile header with groupVal=1 loads exactly one clip");
	CAnimBlendHierarchy *saClip = CAnimManager::GetAnimation("CAR_getin_LHS", sa);
	Check(saClip && saClip->numSequences == 1 && saClip->sequences[0].boneTag == 0,
		"SA bone ID and car clip name preserved");
#ifdef ANIM_COMPRESSION
	Check(saClip->compressed, "SA frameType 4 stays compressed in compressed builds");
	saClip->Uncompress();
#endif
	KeyFrameTrans *lastSA = (KeyFrameTrans*)saClip->sequences[0].GetKeyFrame(1);
	Check(lastSA && lastSA->translation.x == 0.5f && lastSA->rotation.x == 0.25f &&
		lastSA->deltaTime == 1.0f,
		"compressed ANP3 quaternion, time and translation decoded without inversion");

	std::vector<uint8> floatFile = MobileANI(0, false);
	TestStream floating(floatFile);
	CAnimManager::LoadAnimFile(&floating, true, nil, "sa_ped_float");
	CAnimBlock *saFloat = CAnimManager::GetAnimationBlock("sa_ped_float");
	CAnimBlendHierarchy *floatClip = CAnimManager::GetAnimation("CAR_getin_LHS", saFloat);
	Check(floatClip, "ANP3 float clip loaded");
#ifdef ANIM_COMPRESSION
	Check(floatClip->compressed, "float frameType 2 is compressed when requested, even with exporter word 1");
	floatClip->Uncompress();
#endif
	Check(floatClip->sequences[0].GetKeyFrame(1)->rotation.x == 0.25f &&
		((KeyFrameTrans*)floatClip->sequences[0].GetKeyFrame(1))->translation.x == 0.5f,
		"ANP3 float quaternion stays as stored even when exporter word is 1");

	std::vector<uint8> mixFile = MobileANI(1, true, true);
	TestStream mixed(mixFile);
	CAnimManager::LoadAnimFile(&mixed, true, nil, "sa_mixed");
	CAnimBlock *saMixed = CAnimManager::GetAnimationBlock("sa_mixed");
	CAnimBlendHierarchy *mixedClip = CAnimManager::GetAnimation("CAR_getin_LHS", saMixed);
	Check(mixedClip && mixedClip->numSequences == 2, "mixed i16/fp32 ANP3 has two sequences");
#ifdef ANIM_COMPRESSION
	Check(mixedClip->compressed, "mixed ANP3 hierarchy has a consistent compressed state");
	mixedClip->Uncompress();
#endif
	Check(mixedClip->sequences[1].GetKeyFrame(1)->rotation.x == 0.5f &&
		((KeyFrameTrans*)mixedClip->sequences[0].GetKeyFrame(1))->translation.x == 0.5f,
		"mixed ANP3 decodes both compressed and float frameTypes");

	TestStream uncompressed(mixFile);
	CAnimManager::LoadAnimFile(&uncompressed, false, nil, "sa_mixed_plain");
	CAnimBlock *plain = CAnimManager::GetAnimationBlock("sa_mixed_plain");
	CAnimBlendHierarchy *plainClip = CAnimManager::GetAnimation("CAR_getin_LHS", plain);
	Check(plainClip && !plainClip->compressed &&
		((KeyFrameTrans*)plainClip->sequences[0].GetKeyFrame(1))->translation.x == 0.5f &&
		plainClip->sequences[1].GetKeyFrame(1)->rotation.x == 0.5f,
		"requesting uncompressed ANP3 expands i16 frames but leaves float frames intact");

	std::vector<uint8> pcFile = MobileANI(0, false);
	pcFile.insert(pcFile.begin() + 4, 4, 0);  // PC root size before the block name
	pcFile.erase(pcFile.begin() + 36, pcFile.begin() + 40); // no mobile groupVal
	Patch32(pcFile, 4, (uint32)pcFile.size() - 8);
	TestStream pc(pcFile);
	CAnimManager::LoadAnimFile(&pc, false, nil, "sa_pc");
	CAnimBlock *pcBlock = CAnimManager::GetAnimationBlock("sa_pc");
	CAnimBlendHierarchy *pcClip = CAnimManager::GetAnimation("CAR_getin_LHS", pcBlock);
	Check(pcClip && ((KeyFrameTrans*)pcClip->sequences[0].GetKeyFrame(1))->translation.x == 0.5f,
		"PC ANP3 root-size header uses its own count and reads the same frames");

	std::vector<uint8> anp2File = MobileANI(0, true);
	anp2File[3] = '2';
	anp2File.erase(anp2File.begin() + 36 + 28, anp2File.begin() + 36 + 36);
	TestStream anp2(anp2File);
	CAnimManager::LoadAnimFile(&anp2, false, nil, "sa_anp2");
	CAnimBlock *anp2Block = CAnimManager::GetAnimationBlock("sa_anp2");
	CAnimBlendHierarchy *anp2Clip = CAnimManager::GetAnimation("CAR_getin_LHS", anp2Block);
	Check(anp2Clip && !anp2Clip->compressed &&
		((KeyFrameTrans*)anp2Clip->sequences[0].GetKeyFrame(1))->translation.x == 0.5f,
		"ANP2 omits ANP3 per-animation words but keeps compressed frame type 4");

	std::vector<uint8> vcFile = NativeANPK();
	TestStream native(vcFile);
	CAnimManager::LoadAnimFile(&native, true);
	CAnimBlock *vc = CAnimManager::GetAnimationBlock("ped");
	CAnimBlendHierarchy *nativeClip = CAnimManager::GetAnimation("CAR_getin_LHS", vc);
	Check(nativeClip, "native VC clip loaded");
#ifdef ANIM_COMPRESSION
	nativeClip->Uncompress();
#endif
	Check(nativeClip->sequences[0].GetKeyFrame(1)->rotation.x == -0.25f,
		"native ANPK quaternion still inverts its x/y/z components");
	CAnimBlendHierarchy *offsetClip = CAnimManager::GetAnimHierarchyForOffset(ASSOCGRP_STD, ANIM_STD_CAR_GET_IN_LHS);
	Check(offsetClip == nativeClip &&
		((KeyFrameTrans*)offsetClip->sequences[0].GetKeyFrame(1))->translation.x == 1.75f,
		"car-door offset reads the native IFP even with no VC skeleton reference");

	FakeClump vcPed(false), saPed(true);
	Check(CAnimManager::IsAnimationCompatibleWithClump(vcPed.Get(), nativeClip) &&
		!CAnimManager::IsAnimationCompatibleWithClump(vcPed.Get(), saClip) &&
		CAnimManager::IsAnimationCompatibleWithClump(saPed.Get(), saClip) &&
		!CAnimManager::IsAnimationCompatibleWithClump(saPed.Get(), nativeClip),
		"VC and SA skeletons reject animations from the other bank");
	Check(CAnimManager::GetAnimHierarchyForOffset(vcPed.Get(), ASSOCGRP_STD, ANIM_STD_CAR_GET_IN_LHS) == nativeClip &&
		CAnimManager::GetAnimHierarchyForOffset(saPed.Get(), ASSOCGRP_STD, ANIM_STD_CAR_GET_IN_LHS) == saClip,
		"VC and SA car-door offsets come only from the corresponding IFP");
	sa->isLoaded = false;
	Check(CAnimManager::GetAnimHierarchyForOffset(saPed.Get(), ASSOCGRP_STD, ANIM_STD_CAR_GET_IN_LHS) == nil &&
		CAnimManager::GetAnimHierarchyForOffset(vcPed.Get(), ASSOCGRP_STD, ANIM_STD_CAR_GET_IN_LHS) == nativeClip,
		"missing SA bank never borrows native VC animation");
	sa->isLoaded = true;

	customFile = NativeANPK();
	CAnimManager::LoadAnimFile("ANIM\\PED.IFP");
	Check(customStreamsOpened == 0 && nativeStreamsOpened == 1 &&
		CAnimManager::GetAnimation("CAR_getin_LHS", vc) == nativeClip,
		"custom ANPK never replaces the native VC bank: native game file is read");
	customFile = MobileANI(1, true);
	CAnimManager::LoadSAAnimFileFromCustom("ANIM\\BIKES.IFP");
	CAnimBlock *saBikes = CAnimManager::GetAnimationBlock("sa_bikes");
	Check(customStreamsOpened == 1 && saBikes && saBikes->isLoaded &&
		CAnimManager::GetAnimation("CAR_getin_LHS", saBikes) &&
		CAnimManager::GetAnimationBlock("bikes") == nil,
		"custom ANP3 only enters the parallel SA bank");
	puts("Native VC / custom SA bank isolation, real IFP reader and door offsets OK");
	return 0;
}
