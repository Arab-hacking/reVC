// Collision containers: several collision models live in one file.
//
// Two container shapes carry more than one model:
//
//   .col  - the ZIP/loose form. Records are COLL/COL2/COL3/(BOLL) blocks laid
//           one after another, each with its own 24-byte name.
//   .cls  - the BR form. CLST/CLSF chunks, each with its own 24-byte name at
//           +8 and the "CED2" magic at +28.
//
// Both indexers walked the container and then gave up at the first byte that
// did not start a record. That is wrong for the shapes these files really have:
// an IMG entry is a slice rounded up to 2048-byte sectors, so a .cls with two
// models ends in a zero tail, and converted archives put zero alignment
// between chunks. In those files every model after the first gap was silently
// dropped, and the Android path (`bad > 0`) additionally re-parsed the .cls as
// a legacy file. The indexes now step over zero padding and keep going, while
// still reporting a gap that contains real garbage.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../src/extras/custom/BrCollisionFormat.h"
#include "../src/extras/custom/CustomColFormat.h"

static int failures;

static void
Check(bool cond, const char *what)
{
	if(!cond){
		printf("FAIL: %s\n", what);
		failures++;
	}
}

static void
Put32(std::vector<uint8_t> &v, uint32_t n)
{
	v.push_back((uint8_t)n); v.push_back((uint8_t)(n >> 8));
	v.push_back((uint8_t)(n >> 16)); v.push_back((uint8_t)(n >> 24));
}

static std::vector<uint8_t>
BrChunk(const char *tag, const char *name, size_t extra)
{
	std::vector<uint8_t> c(120 + extra, 0);
	std::memcpy(&c[0], tag, 4);
	uint32_t len = (uint32_t)(c.size() - 8);
	for(int i = 0; i < 4; i++) c[4+i] = (uint8_t)(len >> (i*8));
	std::memcpy(&c[8], name, std::strlen(name));
	std::memcpy(&c[28], "CED2", 4);
	return c;
}

// A valid VC (COLL) block: 32-byte header, bounds, then the five section
// counters (spheres, lines, boxes, vertices, faces) all zero. That is 92 bytes,
// which is the minimum the native-layout validator accepts - a block without
// its section counters is not a collision model at all.
static std::vector<uint8_t>
ColBlock(const char *name)
{
	std::vector<uint8_t> c(72 + 5*4, 0);
	std::memcpy(&c[0], "COLL", 4);
	uint32_t len = (uint32_t)(c.size() - 8);
	for(int i = 0; i < 4; i++) c[4+i] = (uint8_t)(len >> (i*8));
	std::memcpy(&c[8], name, std::strlen(name));
	float radius = 1.0f;
	std::memcpy(&c[32], &radius, 4);
	float v[3] = { -1.0f, -2.0f, -3.0f };
	for(int k = 0; k < 3; k++) std::memcpy(&c[48+k*4], &v[k], 4);
	float w[3] = { 1.0f, 2.0f, 3.0f };
	for(int k = 0; k < 3; k++) std::memcpy(&c[60+k*4], &w[k], 4);
	return c;
}

static void
TestBrContainerHoldsSeveralModels(void)
{
	std::vector<uint8_t> file;
	std::vector<uint8_t> a = BrChunk("CLST", "model_one", 128);
	std::vector<uint8_t> b = BrChunk("CLSF", "model_two", 128);
	file.insert(file.end(), a.begin(), a.end());
	file.insert(file.end(), b.begin(), b.end());

	std::vector<brcollision::Block> blocks;
	size_t bad = brcollision::Index(file.data(), file.size(), blocks);
	Check(blocks.size() == 2, "two chunks in one .cls");
	Check(bad == 0, "no invalid chunk in a clean .cls");
	if(blocks.size() == 2){
		Check(blocks[0].name == "model_one", "first chunk keeps its own name");
		Check(blocks[1].name == "model_two", "second chunk keeps its own name");
		Check(blocks[1].offset == a.size(), "second chunk offset follows the first");
	}
}

static void
TestBrContainerSurvivesImgRounding(void)
{
	// What an IMG actually hands over: the entry rounded up to 2048-byte
	// sectors, so a two-model .cls ends in zero padding.
	std::vector<uint8_t> file;
	std::vector<uint8_t> a = BrChunk("CLST", "model_one", 200);
	std::vector<uint8_t> b = BrChunk("CLSF", "model_two", 200);
	file.insert(file.end(), a.begin(), a.end());
	file.insert(file.end(), b.begin(), b.end());
	while(file.size() % 2048) file.push_back(0);

	std::vector<brcollision::Block> blocks;
	size_t bad = brcollision::Index(file.data(), file.size(), blocks);
	Check(blocks.size() == 2, "sector padding does not hide the second model");
	Check(bad == 0, "sector padding is not an invalid chunk");
}

static void
TestBrContainerSurvivesAlignmentBetweenChunks(void)
{
	std::vector<uint8_t> file;
	std::vector<uint8_t> a = BrChunk("CLST", "first", 64);
	std::vector<uint8_t> b = BrChunk("CLST", "second", 64);
	file.insert(file.end(), a.begin(), a.end());
	while(file.size() % 16) file.push_back(0);
	file.insert(file.end(), b.begin(), b.end());

	std::vector<brcollision::Block> blocks;
	size_t bad = brcollision::Index(file.data(), file.size(), blocks);
	Check(blocks.size() == 2, "alignment between chunks is skipped");
	Check(bad == 0, "alignment between chunks is not an error");
}

static void
TestBrContainerStillRejectsGarbage(void)
{
	std::vector<uint8_t> file;
	std::vector<uint8_t> a = BrChunk("CLST", "aa", 64);
	std::vector<uint8_t> b = BrChunk("CLST", "bb", 64);
	file.insert(file.end(), a.begin(), a.end());
	const uint8_t junk[4] = { 0x11, 0x22, 0x33, 0x44 };
	file.insert(file.end(), junk, junk + 4);
	file.insert(file.end(), b.begin(), b.end());

	std::vector<brcollision::Block> blocks;
	size_t bad = brcollision::Index(file.data(), file.size(), blocks);
	Check(bad > 0, "a gap of real garbage is still reported");
	Check(blocks.size() == 1, "the valid chunk before the garbage is kept");
}

static void
TestColContainerHoldsSeveralModels(void)
{
	std::vector<uint8_t> file;
	std::vector<uint8_t> a = ColBlock("aa");
	std::vector<uint8_t> b = ColBlock("bb");
	file.insert(file.end(), a.begin(), a.end());
	file.insert(file.end(), b.begin(), b.end());

	std::vector<customcolformat::Block> blocks;
	size_t bad = customcolformat::Index(file.data(), file.size(), blocks);
	Check(blocks.size() == 2, "two records in one .col");
	Check(bad == 0, "no bad record in a clean .col");
	if(blocks.size() == 2){
		Check(blocks[0].name == "aa", "first .col record name");
		Check(blocks[1].name == "bb", "second .col record name");
	}
}

static void
TestColContainerSurvivesSectorPadding(void)
{
	std::vector<uint8_t> file;
	std::vector<uint8_t> a = ColBlock("aa");
	std::vector<uint8_t> b = ColBlock("bb");
	file.insert(file.end(), a.begin(), a.end());
	while(file.size() % 16) file.push_back(0);
	file.insert(file.end(), b.begin(), b.end());
	file.resize(2048, 0);            // sector-rounded, like an IMG entry

	std::vector<customcolformat::Block> blocks;
	size_t bad = customcolformat::Index(file.data(), file.size(), blocks);
	Check(blocks.size() == 2, "padding does not hide the second .col record");
	Check(bad == 0, "sector padding is not a bad .col record");
}

static void
TestColContainerStillRejectsGarbage(void)
{
	std::vector<uint8_t> file;
	std::vector<uint8_t> a = ColBlock("aa");
	std::vector<uint8_t> b = ColBlock("bb");
	file.insert(file.end(), a.begin(), a.end());
	const uint8_t junk[4] = { 0x11, 0x22, 0x33, 0x44 };
	file.insert(file.end(), junk, junk + 4);
	file.insert(file.end(), b.begin(), b.end());

	std::vector<customcolformat::Block> blocks;
	size_t bad = customcolformat::Index(file.data(), file.size(), blocks);
	Check(bad > 0, "garbage between .col records is still reported");
}

// A .col record whose declared size is shorter than its own 24-byte name makes
// the game's loader copy wrap around; a record larger than the buffer reads
// past the end. The indexer must reject both rather than pass them on.
static void
TestShortAndOversizedColRecordsAreRejected(void)
{
	{
		std::vector<uint8_t> file = ColBlock("aa");
		uint32_t tiny = 8;                  // < 64 minimum in the indexer
		for(int i = 0; i < 4; i++) file[4+i] = (uint8_t)(tiny >> (i*8));
		std::vector<customcolformat::Block> blocks;
		size_t bad = customcolformat::Index(file.data(), file.size(), blocks);
		Check(blocks.empty() && bad > 0, "a record shorter than its header is rejected");
	}
	{
		std::vector<uint8_t> file = ColBlock("aa");
		uint32_t huge = 0x7FFFFFF0u;
		for(int i = 0; i < 4; i++) file[4+i] = (uint8_t)(huge >> (i*8));
		std::vector<customcolformat::Block> blocks;
		size_t bad = customcolformat::Index(file.data(), file.size(), blocks);
		Check(blocks.empty() && bad > 0, "a record longer than the buffer is rejected");
	}
}

// The indexer reports the block span including its 8-byte tag+size header, and
// a VC block must still validate end to end after all of the above.
static void
TestIndexedBlockValidatesAsVc(void)
{
	std::vector<uint8_t> file = ColBlock("aa");
	std::vector<customcolformat::Block> blocks;
	customcolformat::Index(file.data(), file.size(), blocks);
	Check(blocks.size() == 1, "one record indexed");
	if(blocks.size() == 1){
		Check(blocks[0].size == file.size(), "block size covers tag, length and body");
		Check(blocks[0].vc, "COLL is recognised as the native layout");
		Check(customcolformat::ValidVC(file.data(), file.size()),
		      "the block passes the native-layout validator");
	}
}

// A CED2 chunk whose three counts are all zero converts to a COLL block that is
// structurally valid and completely empty: 0 spheres, 0 boxes, 0 faces. The game
// accepts that model and the map ends up with no collision there, while every
// tally on the way up says the block was installed. It has to be rejected.
static void
TestEmptyBrChunkIsRejected(void)
{
	std::vector<uint8_t> chunk = BrChunk("CLSF", "empty_road", 0);
	Check(chunk.size() == 120, "chunk is a bare header");
	float radius = 5.0f;
	std::memcpy(&chunk[44], &radius, 4);
	float bmin[3] = { -10.0f, -10.0f, -10.0f };
	float bmax[3] = { 10.0f, 10.0f, 10.0f };
	for(int k = 0; k < 3; k++) std::memcpy(&chunk[48+k*4], &bmin[k], 4);
	for(int k = 0; k < 3; k++) std::memcpy(&chunk[60+k*4], &bmax[k], 4);
	// counts at +72 are already zero in BrChunk

	std::vector<uint8_t> game;
	Check(!brcollision::Convert(chunk.data(), chunk.size(), game),
	      "a chunk with no spheres, boxes or faces is not converted");
	Check(game.empty(), "nothing is written for an empty chunk");
}

int
main(void)
{
	TestEmptyBrChunkIsRejected();
	TestBrContainerHoldsSeveralModels();
	TestBrContainerSurvivesImgRounding();
	TestBrContainerSurvivesAlignmentBetweenChunks();
	TestBrContainerStillRejectsGarbage();
	TestColContainerHoldsSeveralModels();
	TestColContainerSurvivesSectorPadding();
	TestColContainerStillRejectsGarbage();
	TestShortAndOversizedColRecordsAreRejected();
	TestIndexedBlockValidatesAsVc();

	if(failures){
		printf("%d collision container check(s) failed\n", failures);
		return 1;
	}
	printf("collision containers ok\n");
	return 0;
}
