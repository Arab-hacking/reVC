// Standalone regression coverage for custom-folder VC and SA animation files.
// The sniffer must reject malformed IFPs before the game's non-defensive
// RenderWare animation reader sees them.
#include "../tools/custom-folder-verification/shim/common.h"
#include "../src/extras/custom/br/brformats.h"

#include <string>
#include <vector>

static void
Check(bool ok, const char *message)
{
	if(!ok){
		fprintf(stderr, "IFP test failed: %s\n", message);
		exit(1);
	}
}

static void
Put32(std::vector<uint8> &data, uint32 n)
{
	for(int i = 0; i < 4; i++)
		data.push_back((uint8)(n >> (8 * i)));
}

static void
Set32(std::vector<uint8> &data, size_t pos, uint32 n)
{
	for(int i = 0; i < 4; i++)
		data[pos + i] = (uint8)(n >> (8 * i));
}

static void
Name(std::vector<uint8> &data, const char *name, size_t length)
{
	size_t old = data.size();
	data.resize(old + length, 0);
	Check(strlen(name) < length, "test name fits the field");
	memcpy(&data[old], name, strlen(name));
}

static void
PutChunk(std::vector<uint8> &data, const char *tag, const std::vector<uint8> &body)
{
	data.insert(data.end(), tag, tag + 4);
	Put32(data, (uint32)body.size());
	data.insert(data.end(), body.begin(), body.end());
	while(data.size() % 4)
		data.push_back(0);
}

static std::vector<uint8>
VCIfp(const char *block, bool twoAnimations)
{
	std::vector<uint8> contents, info;
	Put32(info, twoAnimations ? 2 : 1);
	Name(info, block, strlen(block) + 1);
	PutChunk(contents, "INFO", info);
	const char *names[] = { "walk_civi", "idle_stance" };
	for(int a = 0; a < (twoAnimations ? 2 : 1); a++){
		std::vector<uint8> name, dgan, seqInfo, cpan, anim, frames;
		Name(name, names[a], strlen(names[a]) + 1);
		PutChunk(contents, "NAME", name);
		Put32(seqInfo, 1);
		PutChunk(dgan, "INFO", seqInfo);
		Name(anim, "root", 24);
		anim.resize(44, 0);
		Set32(anim, 28, 1);
		PutChunk(cpan, "ANIM", anim);
		frames.resize(0x14, 0);
		PutChunk(cpan, "KR00", frames);
		PutChunk(dgan, "CPAN", cpan);
		PutChunk(contents, "DGAN", dgan);
	}
	std::vector<uint8> file;
	file.insert(file.end(), "ANPK", "ANPK" + 4);
	Put32(file, (uint32)contents.size());
	file.insert(file.end(), contents.begin(), contents.end());
	return file;
}

static std::vector<uint8>
SAIfp(void)
{
	std::vector<uint8> body, file;
	Name(body, "ped", 24);
	Put32(body, 1);
	Name(body, "walk_civi", 24);
	Put32(body, 1);
	Put32(body, 0);	// ANP3 framesAllocSize
	Put32(body, 0);	// flags
	Name(body, "root", 24);
	Put32(body, 1);	// rotation-only frames
	Put32(body, 1);	// frame count
	Put32(body, 0);	// bone tag
	body.resize(body.size() + 0x14, 0);
	file.insert(file.end(), "ANP3", "ANP3" + 4);
	Put32(file, (uint32)body.size());
	file.insert(file.end(), body.begin(), body.end());
	return file;
}

// A real BR .ani is a size-less ANP3 IFP. The ignored group value may be
// fileSize-8, zero or one; none of these is a second animation count. The
// exporter can also leave arbitrary bytes after the declared animations.
static std::vector<uint8>
MobileIfp(const char *magic, uint32 groupValue, uint32 frameType, bool junkTail)
{
	std::vector<uint8> file;
	file.insert(file.end(), magic, magic + 4);
	Name(file, "ped", 24);
	Put32(file, 1);
	Put32(file, groupValue);
	Name(file, "CAR_getin_LHS", 24);
	Put32(file, 1);
	if(magic[3] == '3'){
		Put32(file, frameType == 4 ? 16 : frameType == 3 ? 10 : frameType == 2 ? 32 : 20);
		Put32(file, 1); // exporter/version word, NOT a compression flag
	}
	Name(file, " Root", 24);
	Put32(file, frameType);
	Put32(file, 1);
	Put32(file, 0); // root bone ID
	size_t frameSize = frameType == 4 ? 16 : frameType == 3 ? 10 : frameType == 2 ? 32 : 20;
	file.resize(file.size() + frameSize, 0);
	if(junkTail)
		file.insert(file.end(), 68, 0x5a);
	return file;
}

static size_t
Find(const std::vector<uint8> &file, const char *tag)
{
	for(size_t i = 0; i + 4 <= file.size(); ++i)
		if(memcmp(&file[i], tag, 4) == 0)
			return i;
	Check(false, "fixture contains requested chunk");
	return 0;
}

int
main(void)
{
	char names[16][24] = {};
	char block[24] = {};
	int count = 0;
	std::vector<uint8> vc = VCIfp("ped", true);
	Check(br::SniffAnimNames(vc.data(), vc.size(), names, 16, count, block), "VC ANPK accepted");
	Check(count == 2 && std::string(block) == "ped" &&
		std::string(names[0]) == "walk_civi" && std::string(names[1]) == "idle_stance",
		"VC block and clip names extracted");
	Check(br::VCAnimNameEqual(block, "PED") && br::VCAnimHasPedCore(names, count),
		"matching VC ped block with movement clips can replace the game file");
	std::vector<uint8> wrongBlock = VCIfp("cars", true);
	Check(br::SniffAnimNames(wrongBlock.data(), wrongBlock.size(), names, 16, count, block) &&
		!br::VCAnimNameEqual(block, "ped"), "wrong VC block name requires fallback");
	std::vector<uint8> incompletePed = VCIfp("ped", false);
	Check(br::SniffAnimNames(incompletePed.data(), incompletePed.size(), names, 16, count, block) &&
		!br::VCAnimHasPedCore(names, count), "missing ped idle animation requires fallback");
	Check(!br::SniffAnimNames(vc.data(), vc.size(), names, 1, count),
		"VC dictionary with more clips than available slots is rejected");

	std::vector<uint8> sa = SAIfp();
	Check(br::SniffAnimNames(sa.data(), sa.size(), names, 16, count, block) &&
		count == 1 && std::string(names[0]) == "walk_civi" && block[0] == '\0',
		"SA ANP3 still accepted as a parallel animation bank");
	animfile::ANP23Header header;
	Check(animfile::ParseANP23Header(sa.data() + 4, sa.size() - 4, sa.size(), header) &&
		!header.mobile && header.numAnims == 1 && std::string(header.blockName) == "ped",
		"PC ANP3 root size, block name and count read in the right order");

	for(uint32 groupValue = 0; groupValue <= 1; ++groupValue){
		std::vector<uint8> ani = MobileIfp("ANP3", groupValue, 4, true);
		ani[36 + strlen("CAR_getin_LHS") + 1] = 0x81; // exporter junk after NUL
		Check(animfile::ParseANP23Header(ani.data() + 4, ani.size() - 4, ani.size(), header) &&
			header.mobile && header.numAnims == 1 && std::string(header.blockName) == "ped",
			"size-less ANP3 count is not confused with groupVal 0 or 1");
		Check(br::SniffAnimNames(ani.data(), ani.size(), names, 16, count) &&
			count == 1 && std::string(names[0]) == "CAR_getin_LHS",
			"mobile .ani compressed translation frames and stale exporter tail accepted");
	}
	std::vector<uint8> ani = MobileIfp("ANP3", 0, 3, false);
	Set32(ani, 32, (uint32)ani.size() - 8);
	Check(br::SniffAnimNames(ani.data(), ani.size(), names, 16, count) && count == 1,
		"real .ani groupVal=fileSize-8 accepted");
	Check(!br::isBrAni(ani.data(), ani.size()) ||
		animfile::ParseANP23Header(ani.data() + 4, ani.size() - 4, ani.size(), header),
		"a modern mobile .ani must not have its header reordered");
	std::vector<uint8> mobileAnp2 = MobileIfp("ANP2", 0, 2, false);
	Check(br::SniffAnimNames(mobileAnp2.data(), mobileAnp2.size(), names, 16, count) && count == 1,
		"size-less ANP2 float translation frames accepted");
	std::vector<uint8> mobileFloat = MobileIfp("ANP3", 1, 1, false);
	Check(br::SniffAnimNames(mobileFloat.data(), mobileFloat.size(), names, 16, count) && count == 1,
		"ANP3 float rotation frames accepted independently of exporter word");

	std::vector<uint8> legacy = MobileIfp("ANP3", 0, 4, false);
	memcpy(legacy.data() + 4, "walk", 4); // the old detector needs four printable bytes
	legacy.insert(legacy.begin() + 36, 4, 0x5a); // legacy shim before the clip
	Check(br::isBrAni(legacy.data(), legacy.size()) &&
		!br::SniffAnimNames(legacy.data(), legacy.size(), nil, 16, count),
		"legacy extra word is not mistaken for a valid mobile dictionary");
	Check(br::convertAniToIfp(legacy) &&
		br::SniffAnimNames(legacy.data(), legacy.size(), names, 16, count) &&
		count == 1 && std::string(names[0]) == "CAR_getin_LHS",
		"legacy extra word removed before handing animation data to the reader");
	std::vector<uint8> modernWalk = MobileIfp("ANP3", 1, 4, false);
	memcpy(modernWalk.data() + 4, "walk", 4);
	Check(br::isBrAni(modernWalk.data(), modernWalk.size()) &&
		br::SniffAnimNames(modernWalk.data(), modernWalk.size(), nil, 16, count),
		"real mobile dictionary remains valid despite matching legacy prefix heuristic");

	std::vector<uint8> corrupt = vc;
	corrupt.pop_back();
	Check(!br::SniffAnimNames(corrupt.data(), corrupt.size(), names, 16, count),
		"truncated VC file rejected");
	corrupt = vc;
	Set32(corrupt, Find(corrupt, "ANIM") + 4, 0xFFFFFFFFu);
	Check(!br::SniffAnimNames(corrupt.data(), corrupt.size(), names, 16, count),
		"oversized animation chunk rejected (prevents stack overwrite)");
	corrupt = vc;
	Set32(corrupt, Find(corrupt, "KR00") + 4, 0xFFFFFFFFu);
	Check(!br::SniffAnimNames(corrupt.data(), corrupt.size(), names, 16, count),
		"frame count/size mismatch rejected");
	corrupt = vc;
	corrupt[Find(corrupt, "NAME")] = 'X';
	Check(!br::SniffAnimNames(corrupt.data(), corrupt.size(), names, 16, count),
		"missing VC chunk rejected");
	corrupt = sa;
	Set32(corrupt, Find(corrupt, "root") + 24, 9);
	Check(!br::SniffAnimNames(corrupt.data(), corrupt.size(), names, 16, count),
		"invalid SA frame type rejected");
	corrupt = MobileIfp("ANP3", 1, 4, false);
	Set32(corrupt, 28, 2);
	Check(!br::SniffAnimNames(corrupt.data(), corrupt.size(), names, 16, count),
		"missing second mobile clip is rejected, not filled from groupVal");
	corrupt = MobileIfp("ANP3", 0, 4, false);
	corrupt.pop_back();
	Check(!br::SniffAnimNames(corrupt.data(), corrupt.size(), names, 16, count),
		"truncated compressed translation frames rejected");
	return 0;
}
