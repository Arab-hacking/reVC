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
	return 0;
}
