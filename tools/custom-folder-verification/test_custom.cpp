// Test rig for the custom folder feature: reads a zip archive the way the game
// does (src/extras/custom/CustomZip.cpp), converts what is inside the way the
// game does (src/extras/custom/br/*) and checks the results.
//
//   g++ -O2 -I shim -I ../../reVC/src/extras/custom -I ../../reVC/src/extras/custom/br \
//       -I ../../reVC/src/extras/custom/br/third_party \
//       test_custom.cpp ../../reVC/src/extras/custom/CustomZip.cpp \
//       ../../reVC/src/extras/custom/br/third_party/astc_decomp.cpp -o test_custom
#include "common.h"

#include <string>
#include <vector>
#include <map>
#include <algorithm>

#include "CustomZip.h"
#include "CustomCol.h"
#include "brformats.h"
#include "brtex.h"
#include "brreserved.h"

#define STB_DXT_IMPLEMENTATION
#include "stb_dxt.h"

static int failures = 0;

static void
check(bool ok, const char *what)
{
	printf("%s: %s\n", ok ? "ok  " : "FAIL", what);
	if(!ok)
		failures++;
}

static uint32
Crc32(const uint8 *p, size_t n)
{
	uint32 crc = 0xFFFFFFFF;
	for(size_t i = 0; i < n; i++){
		crc ^= p[i];
		for(int k = 0; k < 8; k++)
			crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
	}
	return ~crc;
}

// ---------------------------------------------------------------------------
// collision: the containers hold San Andreas COL2/COL3 blocks, the game reads
// the first version of the format. Both directions are checked here: a block
// with a version 1 body (CLSF) and one that is already San Andreas shaped
// (CLST) have to come out as the reachable geometry they went in as.
// ---------------------------------------------------------------------------

struct P { const char *tag; std::vector<uint8> data; };

static void
put32(std::vector<uint8> &v, uint32 x)
{
	v.push_back((uint8)x); v.push_back((uint8)(x>>8));
	v.push_back((uint8)(x>>16)); v.push_back((uint8)(x>>24));
}

static void
put16(std::vector<uint8> &v, uint16 x)
{
	v.push_back((uint8)x); v.push_back((uint8)(x>>8));
}

static void
putF(std::vector<uint8> &v, float f)
{
	uint32 x; memcpy(&x, &f, 4); put32(v, x);
}

static void
putName(std::vector<uint8> &v, const char *name, uint16 id)
{
	char buf[22] = { 0 };
	strncpy(buf, name, 21);
	v.insert(v.end(), buf, buf + 22);
	put16(v, id);
}

// version 1 bounds: radius, center, then the box
static void
putBoundsV1(std::vector<uint8> &v)
{
	putF(v, 5.0f);
	putF(v, 1.0f); putF(v, 2.0f); putF(v, 3.0f);
	putF(v, -4.0f); putF(v, -5.0f); putF(v, -6.0f);
	putF(v, 6.0f); putF(v, 7.0f); putF(v, 8.0f);
}

// a .cls block with a version 1 body (CLSF): the converter turns it into COL3
static std::vector<uint8>
MakeClsf(void)
{
	std::vector<uint8> v, body;
	put32(body, 1);						// one sphere
	putF(body, 1.5f);					// radius
	putF(body, 0.5f); putF(body, 0.25f); putF(body, -0.75f);
	body.push_back(5); body.push_back(0); body.push_back(0); body.push_back(0);
	put32(body, 0);						// no lines
	put32(body, 1);						// one box
	putF(body, 0.0f); putF(body, 0.0f); putF(body, 0.0f);
	putF(body, 1.0f); putF(body, 2.0f); putF(body, 3.0f);
	body.push_back(3);
	body.push_back(0); body.push_back(0); body.push_back(0);
	put32(body, 3);						// three vertices (multiples of 1/128)
	putF(body, 0.0f); putF(body, 0.0f); putF(body, 0.0f);
	putF(body, 1.0f); putF(body, 2.0f); putF(body, 3.0f);
	putF(body, -3.25f); putF(body, 0.5f); putF(body, 4.0f);
	put32(body, 1);						// one face
	put32(body, 0); put32(body, 1); put32(body, 2);
	body.push_back(2);
	body.push_back(0); body.push_back(0); body.push_back(0);

	put32(v, 0x46534C43);			// 'CLSF'
	put32(v, (uint32)(body.size() + 24 + 40));	// name, id, bounds and body
	putName(v, "testcol", 0x1234);
	putBoundsV1(v);
	v.insert(v.end(), body.begin(), body.end());
	return v;
}

// a .cls block that is already San Andreas shaped (CLST): the counts are in
// the header, the offsets count from the fourcc plus four, the vertices are
// fixed point and the faces are sixteen bit. Every array starts on a four byte
// border.
static std::vector<uint8>
MakeClst(void)
{
	std::vector<uint8> v, h, data;
	// one sphere: center, radius, surface (20 bytes)
	putF(data, 9.0f); putF(data, 8.0f); putF(data, 7.0f); putF(data, 2.0f);
	data.push_back(6); data.push_back(0); data.push_back(0); data.push_back(0);
	// one box: min, max, surface (28 bytes)
	putF(data, -2.0f); putF(data, -3.0f); putF(data, -4.0f);
	putF(data, 2.0f); putF(data, 3.0f); putF(data, 4.0f);
	data.push_back(200); data.push_back(0); data.push_back(0); data.push_back(0);
	// two vertices, fixed point (6 bytes each): (1, 0, 0) and (0, -2, 0.5)
	put16(data, (uint16)(int16)128); put16(data, 0); put16(data, 0);
	put16(data, 0); put16(data, (uint16)(int16)-256); put16(data, 64);
	// one face: uint16 a, b, c, material, light (8 bytes)
	put16(data, 0); put16(data, 1); put16(data, 1); data.push_back(7); data.push_back(0);

	// counts and offsets, then the three extra fields of the third version
	put16(h, 1); put16(h, 1); put16(h, 1); h.push_back(0); h.push_back(0);
	put32(h, 2);					// flags: not empty
	put32(h, 116);					// spheres   (block offset 120)
	put32(h, 136);					// boxes     (block offset 140)
	put32(h, 0);					// lines
	put32(h, 164);					// vertices  (block offset 168)
	put32(h, 176);					// faces     (block offset 180)
	put32(h, 0);					// planes
	put32(h, 0);					// shadow faces
	put32(h, 0); put32(h, 0);		// shadow vertices and faces

	put32(v, 0x54534C43);			// 'CLST'
	put32(v, (uint32)(24 + 40 + h.size() + data.size()));	// name, id, bounds, header, data
	putName(v, "testcol2", 0);
	// a block of the containers keeps the bounding volumes in the order of the
	// first version: radius, center, box. The converter swaps them for the
	// game and this reader has to put them back.
	putF(v, 5.0f);
	putF(v, 1.0f); putF(v, 2.0f); putF(v, 3.0f);
	putF(v, -4.0f); putF(v, -5.0f); putF(v, -6.0f);
	putF(v, 6.0f); putF(v, 7.0f); putF(v, 8.0f);
	v.insert(v.end(), h.begin(), h.end());
	v.insert(v.end(), data.begin(), data.end());
	return v;
}

// reads the collision file the way the game's own reader does
struct GameCol
{
	float radius, center[3], bmin[3], bmax[3];
	int nS, nB, nV, nF;
	float sphR[4][4]; uint8 sphM[4];
	float boxMin[4][3], boxMax[4][3]; uint8 boxM[4];
	float vtx[8][3];
	int face[8][3]; uint8 faceM[8];
	int materials;
};

static bool
ReadGameCol(const std::vector<uint8> &file, GameCol &c)
{
	memset(&c, 0, sizeof(c));
	const uint8 *p = file.data();
	if(file.size() < 40 || memcmp(p, "COLL", 4) != 0)
		return false;
	p += 8;
	p += 24;						// name and id
	c.radius = *(float*)p;
	memcpy(c.center, p + 4, 12);
	memcpy(c.bmin, p + 16, 12);
	memcpy(c.bmax, p + 28, 12);
	p += 40;

	c.nS = *(int32*)p; p += 4;
	for(int i = 0; i < c.nS && i < 4; i++){
		c.sphR[i][0] = *(float*)p;			// radius first
		memcpy(&c.sphR[i][1], p + 4, 12);
		c.sphM[i] = p[16];
		p += 20;
	}
	{ int lines = *(int32*)p; p += 4; p += lines * 24; }
	c.nB = *(int32*)p; p += 4;
	for(int i = 0; i < c.nB && i < 4; i++){
		memcpy(c.boxMin[i], p, 12);
		memcpy(c.boxMax[i], p + 12, 12);
		c.boxM[i] = p[24];
		p += 28;
	}
	c.nV = *(int32*)p; p += 4;
	for(int i = 0; i < c.nV && i < 8; i++){
		memcpy(c.vtx[i], p, 12);
		p += 12;
	}
	c.nF = *(int32*)p; p += 4;
	for(int i = 0; i < c.nF && i < 8; i++){
		c.face[i][0] = *(int32*)p;
		c.face[i][1] = *(int32*)(p + 4);
		c.face[i][2] = *(int32*)(p + 8);
		c.faceM[i] = p[12];
		p += 16;
	}
	return true;
}

static void
TestCollision(void)
{
	P cases[2] = { { "CLSF", MakeClsf() }, { "CLST", MakeClst() } };

	for(int k = 0; k < 2; k++){
		br::ClsStats cls;
		std::vector<uint8> col = br::convertClsToCol(cases[k].data, cls);
		printf("      %s: %u bytes of .cls -> %u bytes of SAN ANDREAS collision\n",
			cases[k].tag, (unsigned)cases[k].data.size(), (unsigned)col.size());

		customcol::Stats st;
		std::vector<uint8> game = customcol::ToGameFormat(col, st);
		printf("      %s: -> %u bytes of the game's format (%d block(s), %d sphere(s), %d box(es), %d vertex(es), %d face(s), %d material(s) fixed)\n",
			cases[k].tag, (unsigned)game.size(), st.blocks, st.spheres, st.boxes, st.vertices, st.faces, st.materials);

		GameCol c;
		bool ok = ReadGameCol(game, c);
		check(ok, k ? "CLST block reads back as the game's format" : "CLSF block reads back as the game's format");
		if(!ok)
			continue;

		if(k == 0){
			// the values MakeClsf put in
			check(c.nS == 1 && c.nB == 1 && c.nV == 3 && c.nF == 1, "CLSF: one sphere, one box, three vertices, one face");
			check(abs(c.sphR[0][0] - 1.5f) < 1e-6f, "CLSF: sphere radius");
			check(abs(c.sphR[0][1] - 0.5f) < 1e-6f && abs(c.sphR[0][2] - 0.25f) < 1e-6f && abs(c.sphR[0][3] + 0.75f) < 1e-6f, "CLSF: sphere center");
			check(c.sphM[0] == 5, "CLSF: sphere material");
			check(abs(c.boxMax[0][0] - 1.0f) < 1e-6f && abs(c.boxMax[0][2] - 3.0f) < 1e-6f && c.boxM[0] == 3, "CLSF: box");
			check(abs(c.vtx[1][0] - 1.0f) < 1e-6f && abs(c.vtx[1][1] - 2.0f) < 1e-6f, "CLSF: vertex (fixed point round trip)");
			check(abs(c.vtx[2][0] + 3.25f) < 1e-6f && abs(c.vtx[2][2] - 4.0f) < 1e-6f, "CLSF: negative vertex");
			check(c.face[0][0] == 0 && c.face[0][1] == 1 && c.face[0][2] == 2 && c.faceM[0] == 2, "CLSF: face");
			check(abs(c.radius - 5.0f) < 1e-6f && abs(c.center[0] - 1.0f) < 1e-6f && abs(c.bmax[2] - 8.0f) < 1e-6f, "CLSF: bounding volumes");
		}else{
			check(c.nS == 1 && c.nB == 1 && c.nV == 2 && c.nF == 1, "CLST: one sphere, one box, two vertices, one face");
			check(abs(c.sphR[0][0] - 2.0f) < 1e-6f && abs(c.sphR[0][2] - 8.0f) < 1e-6f, "CLST: sphere radius and center");
			check(c.sphM[0] == 6, "CLST: sphere material");
			check(abs(c.boxMin[0][2] + 4.0f) < 1e-6f && abs(c.boxMax[0][1] - 3.0f) < 1e-6f, "CLST: box");
			check(c.boxM[0] == 0, "CLST: a material the game does not know became the default one");
			check(abs(c.vtx[0][0] - 1.0f) < 1e-6f, "CLST: vertex (1, 0, 0)");
			check(abs(c.vtx[1][1] + 2.0f) < 1e-6f && abs(c.vtx[1][2] - 0.5f) < 1e-6f, "CLST: vertex (0, -2, 0.5)");
			check(c.face[0][0] == 0 && c.face[0][1] == 1 && c.face[0][2] == 1 && c.faceM[0] == 7, "CLST: face");
			check(abs(c.bmax[0] - 6.0f) < 1e-6f && abs(c.center[1] - 2.0f) < 1e-6f, "CLST: bounding volumes");
		}
	}

	// a block without geometry is dropped, a broken one is counted
	std::vector<uint8> junk;
	junk.push_back(1); junk.push_back(2); junk.push_back(3); junk.push_back(4);
	customcol::Stats st;
	check(customcol::ToGameFormat(junk, st).empty(), "junk is not turned into a collision file");
}

// The game reads a .mod natively now: one buffer, decrypt + repair in place,
// no separate .dff. The wrapper brconv still uses has to produce the same
// bytes, and the guards/animation headers the loader relies on are checked.
static void
TestNativeMod(const std::vector<uint8> &mod)
{
	if(!mod.empty()){
		br::RwFixStats st1, st2;
		std::string e1, e2;
		std::vector<uint8> viaWrapper = br::convertModToDff(mod, st1, e1);
		std::vector<uint8> inPlace = mod;
		bool ok = br::understandModInplace(inPlace, st2, e2);
		check(ok && !viaWrapper.empty(), "the .mod is understood in place");
		check(ok && viaWrapper.size() == inPlace.size() &&
		      memcmp(viaWrapper.data(), inPlace.data(), inPlace.size()) == 0,
		      "in-place read and the wrapper give the same bytes");
		check(st1.versionsChanged == st2.versionsChanged &&
		      st1.texNamesFixed == st2.texNamesFixed &&
		      st1.schemaFixes == st2.schemaFixes &&
		      st1.binmeshDropped == st2.binmeshDropped,
		      "in-place read reports the same repairs");
	}else{
		check(false, "the .mod is understood in place");
	}

	// the player.mod stub is never taken from an archive
	check(brres::isReserved("player"), "player is a reserved name");
	check(!brres::isReserved("glendale"), "glendale is not reserved");

	// BR .ani: the header fields go back to where SA (and this game) read
	// them - size at +4, block name at +8, numAnims at +32
	std::vector<uint8> ani(0x28 + 64, 0);
	const char magic[4] = { 'A', 'N', 'P', '3' };
	memcpy(ani.data(), magic, 4);
	// BR names the block with at least four printable characters (the header
	// detector requires it); "ped" would be rejected, real files do not use it
	memcpy(&ani[4], "walk", 4);                                 // block name start
	uint32 numAnims = 1;
	memcpy(&ani[28], &numAnims, 4);                              // carried with the name
	uint32 brSize = 40;
	memcpy(&ani[0x20], &brSize, 4);                              // size at +0x20
	uint32 garbage = 0xDEADBEEF;
	memcpy(&ani[0x24], &garbage, 4);
	check(br::isBrAni(ani.data(), ani.size()), "a BR .ani is recognised");
	check(br::convertAniToIfp(ani), "the .ani header is reordered");
	check(memcmp(&ani[4], &brSize, 4) == 0, ".ani: size moved to +4");
	check(memcmp(&ani[8], "walk", 4) == 0, ".ani: block name at +8");
	uint32 got = 0;
	memcpy(&got, &ani[32], 4);
	check(got == 1, ".ani: numAnims at +32");
	{
		std::vector<uint8> zeros(64, 0);
		check(memcmp(&ani[0x28], zeros.data(), 64) == 0, ".ani: body untouched");
	}
	check(!br::isBrAni(ani.data(), ani.size()), "the reordered .ani is not a BR header anymore");
}

int
main(int argc, char **argv)
{
	if(argc < 2){
		printf("usage: %s archive.zip [reference.dff]\n", argv[0]);
		return 1;
	}

	// ---- the archive ------------------------------------------------------
	CustomZip zip;
	check(zip.Open(argv[1]), "archive opens");

	int modEntry = -1, bodyEntry = -1, wheelEntry = -1;
	for(int i = 0; i < zip.GetNumEntries(); i++){
		const CustomZipEntry *e = zip.GetEntry(i);
		std::string name = e->name;
		if(name.size() >= 4 && name.compare(name.size()-4, 4, ".mod") == 0) modEntry = i;
		if(name.find("body.btx") != std::string::npos) bodyEntry = i;
		if(name.find("wheel.btx") != std::string::npos) wheelEntry = i;
	}
	check(modEntry >= 0, "a .mod entry is found");
	check(bodyEntry >= 0 && wheelEntry >= 0, "the .btx entries are found");

	// ---- stored entry (a whole model through the reader) ------------------
	std::vector<uint8> mod;
	if(modEntry >= 0){
		const CustomZipEntry *e = zip.GetEntry(modEntry);
		mod.resize(e->uncompressedSize);
		check(zip.Extract(modEntry, mod.data(), (uint32)mod.size()), "the .mod entry extracts");
		check(br::isMod(mod.data(), mod.size()), "the .mod has the container header");
	}

	// ---- deflated entry (the inflater) -----------------------------------
	for(int which = 0; which < 2; which++){
		int idx = which ? wheelEntry : bodyEntry;
		if(idx < 0) continue;
		const CustomZipEntry *e = zip.GetEntry(idx);
		printf("      entry %s: %u bytes compressed -> %u bytes (%s)\n", e->name,
			e->compressedSize, e->uncompressedSize, e->method == 8 ? "deflate" : "stored");

		std::vector<uint8> raw(e->uncompressedSize);
		check(zip.Extract(idx, raw.data(), (uint32)raw.size()), "the .btx entry extracts");

		std::string err;
		brtex::Texture tex;
		check(brtex::parseBtx(raw.data(), raw.size(), tex, &err), "the .btx parses");
		if(!err.empty())
			printf("      parse note: %s\n", err.c_str());
		check(!tex.mips.empty(), "the .btx has mip levels");
		if(!tex.mips.empty()){
			printf("      %ux%u, %d mip(s), alpha %d\n", tex.mips[0].w, tex.mips[0].h,
				(int)tex.mips.size(), tex.hasAlpha);
			// the first pixel of the body texture is r=200, g=120, b=40
			const uint8 *px = tex.mips[0].rgba.data();
			printf("      first pixel %d %d %d %d\n", px[0], px[1], px[2], px[3]);
			check(px[3] == 255, "the texture is opaque");
		}
		check(Crc32(raw.data(), raw.size()) != 0, "the data has content");

		// the game builds power of two rasters out of these
		brtex::Texture pot = tex;
		bool resized = brtex::makePot(pot);
		check(!pot.mips.empty(), "the texture survives the power of two pass");
		printf("      %ux%u -> %ux%u%s\n", tex.mips.empty()?0:tex.mips[0].w, tex.mips.empty()?0:tex.mips[0].h,
			pot.mips.empty()?0:pot.mips[0].w, pot.mips.empty()?0:pot.mips[0].h, resized ? " (resized)" : "");
	}

	// ---- the whole conversion chain, as the game runs it ------------------
	if(!mod.empty()){
		br::RwFixStats stats;
		std::string err;
		std::vector<uint8> dff = br::convertModToDff(mod, stats, err);
		check(!dff.empty(), "the .mod converts to a renderware model");
		printf("      %u bytes -> %u bytes, source version 0x%X, %d chunk version fix(es)%s%s\n",
			(unsigned)mod.size(), (unsigned)dff.size(), stats.srcVersion, stats.versionsChanged,
			stats.terrain ? ", with terrain layers" : "", err.empty() ? "" : ", note: ");
		if(!err.empty())
			printf("%s\n", err.c_str());

		std::vector<std::string> names;
		brtex::collectTextureNames(dff.data(), dff.size(), names);
		printf("      %d texture name(s) referenced by the model:", (int)names.size());
		for(size_t i = 0; i < names.size() && i < 6; i++)
			printf(" %s", names[i].c_str());
		printf("%s\n", names.size() > 6 ? " ..." : "");
		check(!names.empty(), "the model references textures");

		if(argc > 2){
			FILE *f = fopen(argv[2], "rb");
			if(f){
				fseek(f, 0, SEEK_END);
				long n = ftell(f);
				fseek(f, 0, SEEK_SET);
				std::vector<uint8> ref(n);
				size_t got = fread(ref.data(), 1, n, f);
				fclose(f);
				bool same = got == (size_t)n && ref.size() == dff.size() &&
				            memcmp(ref.data(), dff.data(), dff.size()) == 0;
				check(same, "the result is byte for byte the reference conversion");
			}
		}
	}

	// ---- collision --------------------------------------------------------
	TestCollision();

	// ---- native .mod reading, guards, .ani headers -----------------------
	TestNativeMod(mod);

	printf("\n%s (%d failure(s))\n", failures ? "FAILED" : "all tests passed", failures);
	return failures != 0;
}
