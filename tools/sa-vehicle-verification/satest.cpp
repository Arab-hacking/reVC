/*
 * satest.cpp - headless verification harness for GTA:SA (RW 3.6.0.3) assets loaded
 * through librw's real stream readers (the same code path reVC uses).
 *
 * Build (librw built with LIBRW_PLATFORM=NULL):
 *   g++ -std=c++11 -I<librw> -I<librw>/src satest.cpp <build>/src/librw.a -lm -o satest
 *
 * Mirrors reVC's plugin registration (NodeName on Frame) so frame node names are
 * parsed exactly like in the game.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rw.h>

using namespace rw;

/* ---- reVC's NodeName plugin (src/rw/NodeName.cpp), reimplemented for the harness ---- */
#define VENDOR_ROCKSTAR 0x0253F2
#define MAKECHUNKID(vendor, id) (((vendor & 0xFFFFFF) << 8) | (id & 0xFF))
enum { ID_NODENAME = MAKECHUNKID(VENDOR_ROCKSTAR, 0xFE) };

static int32 gNodeNameOffset = -1;

static void *NodeNameCtor(void *object, int32 offset, int32) {
	if (gNodeNameOffset > 0) *(char*)((char*)object + offset) = '\0';
	return object;
}
static void *NodeNameDtor(void *object, int32, int32) { return object; }
static void *NodeNameCopy(void *dst, void *src, int32 offset, int32) {
	strncpy((char*)((char*)dst + offset), (char*)((char*)src + offset), 23);
	return nil;
}
static Stream *NodeNameRead(Stream *s, int32 length, void *object, int32 offset, int32) {
	if (length > 23) length = 23;
	s->read8((char*)object + offset, length);
	((char*)object)[offset + length] = '\0';
	return s;
}
static Stream *NodeNameWrite(Stream *s, int32 length, void *object, int32 offset, int32) {
	s->write8((char*)object + offset, length);
	return s;
}
static int32 NodeNameSize(void *object, int32 offset, int32) {
	return (int32)strlen((char*)object + offset);
}
static const char *FrameName(Frame *f) {
	if (gNodeNameOffset < 0 || f == nil) return "<nil>";
	return (char*)f + gNodeNameOffset;
}

static int32 gFrameIdOffset = -1;
static void *FrameIdCtor(void *o, int32, int32) { *(int32*)((char*)o + gFrameIdOffset) = 0; return o; }
static int32 FrameId(Frame *f) { return gFrameIdOffset < 0 ? -1 : *(int32*)((char*)f + gFrameIdOffset); }

static int32 TotalAtomics = 0;
static int32 gErrCount = 0;

static Atomic *PrintAtomic(Atomic *a, void *) {
	Geometry *g = a->geometry;
	Frame *f = a->getFrame();
	const char *name = FrameName(f);
	TotalAtomics++;
	if (g == nil) {
		printf("  !! atomic on frame '%s' has NULL geometry\n", name);
		gErrCount++;
		return a;
	}
	printf("  frame=%-22s tris=%-6d verts=%-6d flags=0x%06X texCoordSets=%d morph=%d mats=%d\n",
	       name, g->numTriangles, g->numVertices, g->flags, (g->flags >> 16) & 0xFF,
	       g->numMorphTargets, g->matList.numMaterials);
	for (int32 i = 0; i < g->matList.numMaterials; i++) {
		Material *m = g->matList.materials[i];
		printf("       mat color=(%d,%d,%d,%d) tex=%s\n",
		       m->color.red, m->color.green, m->color.blue, m->color.alpha,
		       m->texture ? m->texture->name : "-");
		if(MatFX::getEffects(m) != MatFX::NOTHING)
			printf("       matfx effect=%d\n", MatFX::getEffects(m));
	}
	return a;
}

static Frame *PrintFrame(Frame *f, void *data) {
	int depth = *(int*)data;
	for (int i = 0; i < depth; i++) printf("   ");
	printf(" frame id=%-3d %s\n", FrameId(f), FrameName(f));
	int d = depth + 1;
	if (f->child) PrintFrame(f->child, &d);
	if (f->next) PrintFrame(f->next, &depth);
	return f;
}

static void TestTexture(const Texture *t) {
	Raster *r = t->raster;
	if (r == nil) {
		printf("  !! tex %-22s has NULL raster\n", t->name);
		gErrCount++;
		return;
	}
	printf("  tex %-22s raster ok platform=%d fmt=0x%04X %dx%d depth=%d levels=%d type=%d\n",
	       t->name, r->platform, r->format, r->width, r->height, r->depth, r->getNumLevels(), r->type);
}

int main(int argc, char *argv[]) {
	if (argc < 2) { fprintf(stderr, "usage: %s file.dff [file.txd]\n", argv[0]); return 1; }

	if (!Engine::init()) { fprintf(stderr, "Engine::init failed\n"); return 1; }
	if (!Engine::open(nil)) { fprintf(stderr, "Engine::open failed\n"); return 1; }
	if (!Engine::start()) { fprintf(stderr, "Engine::start failed\n"); return 1; }
	printf("librw version 0x%X, platform %d\n", version, platform);

	/* same plugin registration as reVC */
	gNodeNameOffset = Frame::registerPlugin(24, ID_NODENAME, NodeNameCtor, NodeNameDtor, NodeNameCopy);
	Frame::registerPluginStream(ID_NODENAME, NodeNameRead, NodeNameWrite, NodeNameSize);
	gFrameIdOffset = Frame::registerPlugin(4, MAKECHUNKID(VENDOR_ROCKSTAR, 0x00), FrameIdCtor, nil, nil);
	printf("NodeName plugin offset = %d, frame id offset = %d\n", gNodeNameOffset, gFrameIdOffset);
	registerMatFXPlugin();	/* reVC: RpMatFXPluginAttach() */

	/* ---------- TXD first (materials reference textures by name) ---------- */
	TexDictionary *txd = nil;
	if (argc >= 3) {
		StreamFile tf;
		tf.open(argv[2], "rb");
		if(!findChunk(&tf, ID_TEXDICTIONARY, nil, nil)) { fprintf(stderr, "!! no TXD chunk\n"); return 3; }
		txd = TexDictionary::streamRead(&tf);
		if (txd == nil) { fprintf(stderr, "!! TexDictionary::streamRead FAILED\n"); return 3; }
		TexDictionary::setCurrent(txd);
		int n = 0;
		FORLIST(lnk2, txd->textures) {
			Texture *t = Texture::fromDict(lnk2);
			TestTexture(t);
			if (t->raster && platform != PLATFORM_NULL) {
				Raster *conv = Raster::convertTexToCurrentPlatform(t->raster);
				printf("        -> converted to platform %d, %dx%d fmt=0x%04X levels=%d\n",
				       conv->platform, conv->width, conv->height, conv->format, conv->getNumLevels());
				t->raster = conv;
			}
			n++;
		}
		printf("TXD ok: %d textures\n", n);
		tf.close();
	}

	/* ---------- DFF ---------- */
	StreamFile sf;
	sf.open(argv[1], "rb");
	if(!findChunk(&sf, ID_CLUMP, nil, nil)) { fprintf(stderr, "!! no CLUMP chunk\n"); return 2; }
	Clump *clump = Clump::streamRead(&sf);
	if (clump == nil) { fprintf(stderr, "!! Clump::streamRead FAILED\n"); return 2; }
	FORLIST(lnk, clump->atomics) TotalAtomics++;
	printf("CLUMP ok: frames=%d atomics=%d\n", clump->getFrame()->count(), TotalAtomics);
	/* VC vehicle colour convention check (what CVehicleModelInfo::FindEditableMaterialList does) */
	int nMats = 0, nMagic1 = 0, nMagic2 = 0, nTextured = 0, nTexMissing = 0, nRemap = 0, nMatFX = 0;
	FORLIST(lnkM, clump->atomics) {
		Geometry *gm = Atomic::fromClump(lnkM)->geometry;
		if (gm == nil) continue;
		for (int32 i = 0; i < gm->matList.numMaterials; i++) {
			Material *m = gm->matList.materials[i];
			nMats++;
			if (m->color.red == 0x3C && m->color.green == 0xFF && m->color.blue == 0) nMagic1++;
			if (m->color.red == 0xFF && m->color.green == 0 && m->color.blue == 0xAF) nMagic2++;
			if (MatFX::getEffects(m) != MatFX::NOTHING) nMatFX++;
			if (m->texture) {
				nTextured++;
				if (strstr(m->texture->name, "remap")) nRemap++;
			} else nTexMissing++;
		}
	}
	printf("MATERIALS: total=%d (VC-primary-magic=%d, VC-secondary-magic=%d) textured=%d untextured=%d remap-tex=%d matfx=%d\n",
	       nMats, nMagic1, nMagic2, nTextured, nTexMissing, nRemap, nMatFX);
	printf("NOTE: reVC m_materials1[] holds %d entries, m_materials2[] %d\n", 24, 20);
	FORLIST(lnkA, clump->atomics) PrintAtomic(Atomic::fromClump(lnkA), nil);
	printf("-- hierarchy --\n");
	{ int zero = 0; PrintFrame(clump->getFrame(), &zero); }
	sf.close();

	printf("RESULT: %s (atomics=%d, errors=%d)\n", gErrCount == 0 ? "PASS" : "FAIL", TotalAtomics, gErrCount);
	return gErrCount == 0 ? 0 : 4;
}
