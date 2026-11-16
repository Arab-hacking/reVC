// Test for the model payload validator (br::validateClumpForGame): the game
// must reject the client's empty model stubs (a clump without atomics, or with
// a zero-size geometry - that is how the client stores unused weapons and
// vehicles, e.g. rocketla/seaspar) and truncated models BEFORE the game parses
// them, while every real model still passes.
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

typedef uint8_t uint8;
typedef uint32_t uint32;
typedef uint64_t uint64;
#define nil NULL

#include "brformats.h"

// convenience overloads for the test
namespace br {
static inline bool validateClumpForGame(const std::vector<uint8> &v, std::string &why)
{ return validateClumpForGame(v.data(), (uint32)v.size(), why); }
}

static int failures = 0;

static void
check(const char *what, bool ok, bool want, const std::string &why)
{
	fflush(stdout);
	printf("%-38s %s%s\n", what, ok == want ? "ok  " : "FAIL",
		ok ? "" : (std::string("  -> ") + why).c_str());
	fflush(stdout);
	if(ok != want)
		failures++;
}

typedef std::vector<uint8> Buf;

static void
w32(Buf &v, uint32 x)
{
	v.push_back(x); v.push_back(x >> 8); v.push_back(x >> 16); v.push_back(x >> 24);
}
static void
w16(Buf &v, uint32 x)
{
	v.push_back(x); v.push_back(x >> 8);
}
static void
put(Buf &dst, const Buf &src)
{
	dst.insert(dst.end(), src.begin(), src.end());
}
static Buf
chunk(uint32 id, const Buf &payload)
{
	Buf v;
	w32(v, id);
	w32(v, (uint32)payload.size());
	w32(v, 0x1803FFFF);	// libid, the normalizer's version
	put(v, payload);
	return v;
}
static Buf
operator+(const Buf &a, const Buf &b)
{
	Buf v = a;
	put(v, b);
	return v;
}

// a geometry struct in the SA layout: header 16, uvs, tris, one morph target
static Buf
geomStruct(uint32 flags, uint32 tris, uint32 verts, bool fill)
{
	Buf s;
	w32(s, flags); w32(s, tris); w32(s, verts); w32(s, 1);	// one morph target
	uint32 numTex = (flags >> 16) & 0xFF;
	if(numTex == 0) numTex = (flags & 0x04 ? 1 : 0) + (flags & 0x80 ? 2 : 0);
	if(!(flags & 0x01000000)){
		for(uint32 i = 0; i < numTex * verts * 8; i++) s.push_back(fill ? 0x7F : 0);
		for(uint32 i = 0; i < tris * 8; i++) s.push_back(fill ? 0x11 : 0);
	}
	// morph target: bounding sphere + hasVerts/hasNormals + the vertex data
	for(int i = 0; i < 5; i++) w32(s, 0);
	w32(s, 1);	// hasVerts
	if(fill) for(uint32 i = 0; i < verts * 12; i++) s.push_back(0x22);
	return s;
}

static Buf
matList(void)
{
	Buf s;
	w32(s, 1);	// one material
	w16(s, 0);	// its index
	Buf m;
	w32(m, 0xFF808080);	// color
	for(int i = 0; i < 7; i++) w32(m, 0x3F800000);	// surface props etc.
	return chunk(0x08, s) + chunk(0x07, m);	// rwID_MATLIST + rwID_MATERIAL
}

// one atomic: struct + geometry(struct + matlist)
static Buf
atomic(uint32 tris, uint32 verts, bool fill)
{
	Buf as;
	w32(as, 0x5); w32(as, 0); w32(as, 0); w32(as, 0);	// flags, unused, geom ptr, frame ptr
	Buf g = chunk(0x01, geomStruct(0x06, tris, verts, fill)) + matList();
	return chunk(0x14, chunk(0x01, as) + chunk(0x0F, g));
}

static Buf
clump(const Buf &atomics, uint32 numAtomics)
{
	Buf s;
	w32(s, numAtomics);
	Buf f;
	w32(f, 1);	// one frame
	for(int i = 0; i < 14; i++) w32(f, 0);	// the 56-byte frame
	return chunk(0x10, chunk(0x01, s) + chunk(0x0E, f) + atomics);
}

int
main(int argc, char *argv[])
{
	std::string why;

	// 1. a real model passes
	check("valid clump", br::validateClumpForGame(clump(atomic(1, 3, true), 1), why), true, why);

	// 2. the client's stub: a clump with no atomics at all (like the 192-byte rocketla)
	check("stub: no atomics", br::validateClumpForGame(clump(Buf(), 0), why), false, why);

	// 3. the stub with an atomic but an empty geometry (0 verts, 0 tris)
	check("stub: empty geometry", br::validateClumpForGame(clump(atomic(0, 0, false), 1), why), false, why);

	// 4. truncated: declares 100 verts / 1000 tris, the data is not there
	check("truncated geometry", br::validateClumpForGame(clump(atomic(1000, 100, false), 1), why), false, why);

	// 5. native geometry: the data lives in the plugin, the counters are legal
	{
		Buf as;
		w32(as, 0x5); w32(as, 0); w32(as, 0); w32(as, 0);
		Buf g = chunk(0x01, geomStruct(0x01000006, 2, 4, false)) + matList();
		check("native geometry",
			br::validateClumpForGame(clump(chunk(0x14, chunk(0x01, as) + chunk(0x0F, g)), 1), why), true, why);
	}

	// 6. not a clump at all: not ours to judge, accepted
	check("not a clump (skipped)", br::validateClumpForGame(chunk(0x16, Buf()), why), true, why);

	// 7. garbage: accepted (the game's loaders decide)
	{
		Buf g;
		for(int i = 0; i < 40; i++) g.push_back(i * 7);
		check("garbage (skipped)", br::validateClumpForGame(g, why), true, why);
	}

	// 8. a real client model: glendale.mod, understood in place, must pass
	if(argc > 1){
		FILE *f = fopen(argv[1], "rb");
		if(f == nil){
			printf("cannot open %s\n", argv[1]);
			return 2;
		}
		fseek(f, 0, SEEK_END);
		long n = ftell(f);
		fseek(f, 0, SEEK_SET);
		Buf mod(n);
		if(fread(mod.data(), 1, n, f) != (size_t)n){ fclose(f); return 2; }
		fclose(f);
		br::RwFixStats st;
		std::string err;
		if(!br::understandModInplace(mod, st, err))
			printf("glendale.mod: understand failed: %s\n", err.c_str());
		check("real BR model (glendale.mod)", br::validateClumpForGame(mod, why), true, why);
	}

	printf("== %s (%d failure(s)) ==\n", failures ? "FAILED" : "all tests passed", failures);
	return failures ? 1 : 0;
}
