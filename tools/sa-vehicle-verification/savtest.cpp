/*
 * savtest.cpp - verification harness for reVC's GTA:San Andreas vehicle support.
 *
 * It loads a real SA (RW 3.6.0.3) vehicle DFF through librw's real stream readers
 * (exactly the code path reVC uses) and runs the same vehicle preprocessing rules
 * that reVC's CVehicleModelInfo applies after the SA_VEHICLE_MODELS patch:
 *
 *   - hierarchy detection (SA models have no *_hi LOD markers),
 *   - frame name -> hierarchy id assignment incl. the SA dummy aliases
 *     (bonnet / boot / bump_front / bump_rear / windscreen),
 *   - COLLAPSE processing (child mesh frames are collapsed onto their dummies),
 *   - wheel handling (SA models bring their own "wheel" mesh which has to be
 *     cloned onto the remaining wheel_*_dummy frames),
 *   - render callback classification (VC only renders *_hi atomics, SA models
 *     have to be drawn in hi detail by name-independence).
 *
 * build (librw built with LIBRW_PLATFORM=NULL):
 *   g++ -std=c++11 -I<librw> -I<librw>/src savtest.cpp <lwbuild>/src/librw.a -lm -o savtest
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rw.h>

using namespace rw;

/* ---------------- reVC NodeName plugin (src/rw/NodeName.cpp) ---------------- */
#define VENDOR_ROCKSTAR 0x0253F2
#define MAKECHUNKID(vendor, id) (((vendor & 0xFFFFFF) << 8) | (id & 0xFF))
enum { ID_NODENAME = MAKECHUNKID(VENDOR_ROCKSTAR, 0xFE) };

static int32 gNodeNameOffset = -1;
static int32 gFrameIdOffset = -1;

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
static const char *FName(Frame *f) {
	if (gNodeNameOffset < 0 || f == nil) return "<nil>";
	return (char*)f + gNodeNameOffset;
}

/* reVC: CVisibilityPlugins frame hierarchy id plugin */
static void *FrameIdCtor(void *o, int32, int32) { *(int32*)((char*)o + gFrameIdOffset) = 0; return o; }
static int32 FId(Frame *f) { return gFrameIdOffset < 0 || f == nil ? -1 : *(int32*)((char*)f + gFrameIdOffset); }
static void SetFId(Frame *f, int32 id) { *(int32*)((char*)f + gFrameIdOffset) = id; }

/* ------------- reVC vehicle descriptors (VehicleModelInfo.cpp) ------------- */
enum {
	VEHICLE_FLAG_COLLAPSE	= 0x2,
	VEHICLE_FLAG_ADD_WHEEL	= 0x4,
	VEHICLE_FLAG_POS	= 0x8,
	VEHICLE_FLAG_DOOR	= 0x10,
	VEHICLE_FLAG_LEFT	= 0x20,
	VEHICLE_FLAG_RIGHT	= 0x40,
	VEHICLE_FLAG_FRONT	= 0x80,
	VEHICLE_FLAG_REAR	= 0x100,
	VEHICLE_FLAG_COMP	= 0x200,
	VEHICLE_FLAG_DRAWLAST	= 0x400,
	VEHICLE_FLAG_WINDSCREEN	= 0x800,
	CLUMP_FLAG_NO_HIERID	= 0x1,
	VEHICLE_FLAG_ANGLECULL	= 0x1000,
	VEHICLE_FLAG_REARDOOR	= 0x2000,
	VEHICLE_FLAG_FRONTDOOR	= 0x4000,
};

struct Desc { const char *name; int32 id; uint32 flags; };

/* ids are local (the harness only needs them to be unique), names and flags are
 * copied verbatim from CVehicleModelInfo::carIds in the patched reVC */
static Desc carIds[] = {
	{ "wheel_rf_dummy",	1,  VEHICLE_FLAG_RIGHT | VEHICLE_FLAG_ADD_WHEEL },
	{ "wheel_rm_dummy",	2,  VEHICLE_FLAG_RIGHT | VEHICLE_FLAG_ADD_WHEEL },
	{ "wheel_rb_dummy",	3,  VEHICLE_FLAG_RIGHT | VEHICLE_FLAG_ADD_WHEEL },
	{ "wheel_lf_dummy",	4,  VEHICLE_FLAG_LEFT | VEHICLE_FLAG_ADD_WHEEL },
	{ "wheel_lm_dummy",	5,  VEHICLE_FLAG_LEFT | VEHICLE_FLAG_ADD_WHEEL },
	{ "wheel_lb_dummy",	6,  VEHICLE_FLAG_LEFT | VEHICLE_FLAG_ADD_WHEEL },
	{ "bump_front_dummy",	7,  VEHICLE_FLAG_FRONT | VEHICLE_FLAG_COLLAPSE },
	{ "bonnet_dummy",	8,  VEHICLE_FLAG_COLLAPSE },
	{ "wing_rf_dummy",	9,  VEHICLE_FLAG_COLLAPSE },
	{ "wing_rr_dummy",	10, VEHICLE_FLAG_RIGHT | VEHICLE_FLAG_COLLAPSE },
	{ "door_rf_dummy",	11, VEHICLE_FLAG_FRONTDOOR | VEHICLE_FLAG_ANGLECULL | VEHICLE_FLAG_RIGHT | VEHICLE_FLAG_DOOR | VEHICLE_FLAG_COLLAPSE },
	{ "door_rr_dummy",	12, VEHICLE_FLAG_REARDOOR | VEHICLE_FLAG_ANGLECULL | VEHICLE_FLAG_REAR | VEHICLE_FLAG_RIGHT | VEHICLE_FLAG_DOOR | VEHICLE_FLAG_COLLAPSE },
	{ "wing_lf_dummy",	13, VEHICLE_FLAG_COLLAPSE },
	{ "wing_lr_dummy",	14, VEHICLE_FLAG_LEFT | VEHICLE_FLAG_COLLAPSE },
	{ "door_lf_dummy",	15, VEHICLE_FLAG_FRONTDOOR | VEHICLE_FLAG_ANGLECULL | VEHICLE_FLAG_LEFT | VEHICLE_FLAG_DOOR | VEHICLE_FLAG_COLLAPSE },
	{ "door_lr_dummy",	16, VEHICLE_FLAG_REARDOOR | VEHICLE_FLAG_ANGLECULL | VEHICLE_FLAG_REAR | VEHICLE_FLAG_LEFT | VEHICLE_FLAG_DOOR | VEHICLE_FLAG_COLLAPSE },
	{ "boot_dummy",		17, VEHICLE_FLAG_REAR | VEHICLE_FLAG_COLLAPSE },
	{ "bump_rear_dummy",	18, VEHICLE_FLAG_REAR | VEHICLE_FLAG_COLLAPSE },
	{ "windscreen_dummy",	19, VEHICLE_FLAG_WINDSCREEN | VEHICLE_FLAG_DRAWLAST | VEHICLE_FLAG_FRONT | VEHICLE_FLAG_COLLAPSE },
	/* --- added by SA_VEHICLE_MODELS --- */
	{ "bump_front",		7,  VEHICLE_FLAG_FRONT | VEHICLE_FLAG_COLLAPSE },
	{ "bump_rear",		18, VEHICLE_FLAG_REAR | VEHICLE_FLAG_COLLAPSE },
	{ "bonnet",		8,  VEHICLE_FLAG_COLLAPSE },
	{ "boot",		17, VEHICLE_FLAG_REAR | VEHICLE_FLAG_COLLAPSE },
	{ "windscreen",		19, VEHICLE_FLAG_WINDSCREEN | VEHICLE_FLAG_DRAWLAST | VEHICLE_FLAG_FRONT | VEHICLE_FLAG_COLLAPSE },
	/* --- --- */
	{ "ped_frontseat",	20, VEHICLE_FLAG_POS | CLUMP_FLAG_NO_HIERID },
	{ "ped_backseat",	21, VEHICLE_FLAG_POS | CLUMP_FLAG_NO_HIERID },
	{ "headlights",		22, VEHICLE_FLAG_POS | CLUMP_FLAG_NO_HIERID },
	{ "taillights",		23, VEHICLE_FLAG_POS | CLUMP_FLAG_NO_HIERID },
	{ "exhaust",		24, VEHICLE_FLAG_POS | CLUMP_FLAG_NO_HIERID },
	{ "extra1",		25, VEHICLE_FLAG_DRAWLAST | VEHICLE_FLAG_COMP | CLUMP_FLAG_NO_HIERID },
	{ "extra2",		26, VEHICLE_FLAG_DRAWLAST | VEHICLE_FLAG_COMP | CLUMP_FLAG_NO_HIERID },
	{ "extra3",		27, VEHICLE_FLAG_DRAWLAST | VEHICLE_FLAG_COMP | CLUMP_FLAG_NO_HIERID },
	{ "extra4",		28, VEHICLE_FLAG_DRAWLAST | VEHICLE_FLAG_COMP | CLUMP_FLAG_NO_HIERID },
	{ "extra5",		29, VEHICLE_FLAG_DRAWLAST | VEHICLE_FLAG_COMP | CLUMP_FLAG_NO_HIERID },
	{ "extra6",		30, VEHICLE_FLAG_DRAWLAST | VEHICLE_FLAG_COMP | CLUMP_FLAG_NO_HIERID },
	{ nil, 0, 0 }
};

/* ------------------------------ helpers ---------------------------------- */
static int stricmp_(const char *a, const char *b) {
	while (*a && *b) {
		char ca = *a, cb = *b;
		if (ca >= 'A' && ca <= 'Z') ca += 32;
		if (cb >= 'A' && cb <= 'Z') cb += 32;
		if (ca != cb) return ca - cb;
		a++; b++;
	}
	return *a - *b;
}

/* reVC: CClumpModelInfo::FindFrameFromNameWithoutIdCB */
static Frame *FindByName(Frame *f, const char *name, bool requireNoId, bool &stop) {
	stop = false;
	if (f == nil) return nil;
	if (stricmp_(FName(f), name) == 0 && (!requireNoId || FId(f) == 0))
		return f;
	for (Frame *c = f->child; c; c = c->next) {
		Frame *r = FindByName(c, name, requireNoId, stop);
		if (r) return r;
	}
	return nil;
}
/* reVC: CClumpModelInfo::FindFrameFromIdCB */
static Frame *FindById(Frame *f, int32 id) {
	if (f == nil) return nil;
	if (FId(f) == id) return f;
	for (Frame *c = f->child; c; c = c->next) {
		Frame *r = FindById(c, id);
		if (r) return r;
	}
	return nil;
}

/* is there an atomic on this frame or on any frame below it? */
static int FrameTreeHasAtomic(Clump *clump, Frame *f) {
	FORLIST(lnk, clump->atomics) {
		for (Frame *p = Atomic::fromClump(lnk)->getFrame(); p; p = p->getParent())
			if (p == f) return 1;
	}
	return 0;
}
static Atomic *FindAtomicOn(Clump *clump, Frame *dummy) {
	FORLIST(lnk, clump->atomics) {
		Atomic *a = Atomic::fromClump(lnk);
		for (Frame *p = a->getFrame(); p; p = p->getParent())
			if (p == dummy) return a;
	}
	return nil;
}

/* reVC: CollapseFramesCB - destroy the child frames of a dummy and move their
 * objects onto the dummy itself */
static void CollapseChildren(Clump *clump, Frame *target, Frame *f) {
	Frame *c, *next;
	for (c = f->child; c; c = next) {
		next = c->next;
		CollapseChildren(clump, target, c);
	}
	/* collect first, setFrame() unlinks from the list */
	Atomic *moved[16];
	int n = 0;
	FORLIST(lnk, f->objectList) {
		if (n < 16) moved[n++] = (Atomic*)ObjectWithFrame::fromFrame(lnk);
	}
	for (int i = 0; i < n; i++)
		moved[i]->setFrame(target);
	if (f != target)
		f->destroy();
}

/* reVC: CVehicleModelInfo::CloneSAWheelMeshes() */
static char WheelPos(Frame *dummy) {
	char *name = (char*)FName(dummy);
	char *s = name ? strstr(name, "wheel_") : nil;
	if (s == nil) return 0;
	s += 6;
	return s[0] ? s[1] : 0;
}
static int CloneSAWheelMeshes(Clump *clump, Frame **wheelFrames, int numWheels) {
	int i, j, cloned = 0;
	for (i = 0; i < numWheels; i++) {
		Frame *src = nil, *f;
		Atomic *srcAtomic, *atomic;
		char pos;
		if (wheelFrames[i] == nil || FrameTreeHasAtomic(clump, wheelFrames[i]))
			continue;
		pos = WheelPos(wheelFrames[i]);
		for (j = 0; j < numWheels; j++) {
			if (wheelFrames[j] == nil || j == i) continue;
			f = FindAtomicOn(clump, wheelFrames[j]) ? (Frame*)1 : nil;
			if (f == nil) continue;
			src = (Frame*)FindAtomicOn(clump, wheelFrames[j])->getFrame();
			while (src->getParent() && src->getParent() != wheelFrames[j])
				src = src->getParent();
			if (WheelPos(wheelFrames[j]) == pos) break;
		}
		if (src == nil) continue;
		srcAtomic = FindAtomicOn(clump, wheelFrames[j] ? wheelFrames[j] : src);
		/* find the atomic which sits on 'src' */
		FORLIST(lnk, clump->atomics)
			if (Atomic::fromClump(lnk)->getFrame() == src) { srcAtomic = Atomic::fromClump(lnk); break; }
		if (srcAtomic == nil) continue;
		atomic = srcAtomic->clone();
		if (atomic == nil) continue;
		f = Frame::create();
		f->matrix = src->matrix;
		f->updateObjects();
		atomic->setFrame(f);
		wheelFrames[i]->addChild(f);
		clump->addAtomic(atomic);
		cloned++;
	}
	return cloned;
}

/* reVC: CVehicleModelInfo::SetAtomicRendererCB decision, simulated.
 * 0 = not rendered (nil callback), 1 = hi detail, 2 = very low detail, 3 = destroyed */
static int ClassifyRender(Atomic *a, bool saMode) {
	const char *name = FName(a->getFrame());
	bool alpha = false;
	for (uint32 i = 0; i < a->geometry->matList.numMaterials; i++)
		if (a->geometry->matList.materials[i]->color.alpha != 0xFF) alpha = true;
	if (strstr(name, "_hi") || strncmp(name, "extra", 5) == 0)
		return 1;
	if (strstr(name, "_vlo"))
		return 2;
	if (saMode)
		return 1;	/* SA hierarchy: everything that is not a LOD part is hi detail */
	if (strstr(name, "_lo"))
		return 3;
	return 0;
}


/* ------------------ synthetic regression cases ------------------ */
static void SetName(Frame *f, const char *name) {
	if (gNodeNameOffset < 0) return;
	strncpy((char*)f + gNodeNameOffset, name, 23);
	((char*)f)[gNodeNameOffset + 23] = '\0';
}

static Clump *MakeSynthetic(const char **names, int n) {
	Clump *clump = Clump::create();
	Frame *root = Frame::create();
	clump->setFrame(root);
	SetName(root, "testroot");
	for (int i = 0; i < n; i++) {
		Frame *f = Frame::create();
		SetName(f, names[i]);
		Geometry *g = Geometry::create(3, 1, 0);
		Atomic *a = Atomic::create();
		a->setGeometry(g, 0);
		a->setFrame(f);
		root->addChild(f);
		clump->addAtomic(a);
	}
	return clump;
}

/* 1 = SA hierarchy mode detected (same rule as reVC's IsSAHierarchy) */
static int SyntheticSACheck(Clump *clump) {
	FORLIST(lnk, clump->atomics)
		if (strstr(FName(Atomic::fromClump(lnk)->getFrame()), "_hi")) return 0;
	return 1;
}

static void SyntheticCase(const char *title, const char **names, int n, int expectSA) {
	Clump *c = MakeSynthetic(names, n);
	int sa = SyntheticSACheck(c);
	int drawnVanilla = 0, destroyed = 0, drawnSA = 0;
	FORLIST(lnk, c->atomics) {
		Atomic *a = Atomic::fromClump(lnk);
		if (ClassifyRender(a, 0) == 1) drawnVanilla++;
		if (ClassifyRender(a, 0) == 3) destroyed++;
		if (ClassifyRender(a, sa) == 1) drawnSA++;
	}
	printf("   %-44s SA-mode=%d (expect %d)  drawn: vanilla %d/%d, SA %d/%d  %s\n",
	       title, sa, expectSA, drawnVanilla, n, drawnSA, n,
	       sa == expectSA ? "OK" : "MISMATCH");
	c->destroy();
}

static void RunSyntheticTests(void) {
	printf("\n-- detection regression (SA vs vanilla VC naming) --\n");
	const char *vcStyle[] = { "windscreen_hi", "banshee_hi", "banshee_lo", "banshee_vlo" };
	const char *saStyle[] = { "body", "wheel", "chassis_vlo", "bump_front" };
	const char *vcStyle2[] = { "cab_hi", "chassis_hi", "chassis_lo" };
	SyntheticCase("VC naming (_hi/_lo/_vlo present)", vcStyle, 4, 0);
	SyntheticCase("SA naming (no _hi at all)", saStyle, 4, 1);
	SyntheticCase("VC naming without _vlo", vcStyle2, 3, 0);
}

int main(int argc, char *argv[]) {
	if (argc < 2) { fprintf(stderr, "usage: %s file.dff\n", argv[0]); return 1; }

	if (!Engine::init()) { fprintf(stderr, "Engine::init failed\n"); return 1; }
	if (!Engine::open(nil)) { fprintf(stderr, "Engine::open failed\n"); return 1; }
	if (!Engine::start()) { fprintf(stderr, "Engine::start failed\n"); return 1; }
	printf("librw platform %d\n\n", platform);

	gNodeNameOffset = Frame::registerPlugin(24, ID_NODENAME, NodeNameCtor, NodeNameDtor, NodeNameCopy);
	Frame::registerPluginStream(ID_NODENAME, NodeNameRead, NodeNameWrite, NodeNameSize);
	gFrameIdOffset = Frame::registerPlugin(4, MAKECHUNKID(VENDOR_ROCKSTAR, 0x00), FrameIdCtor, nil, nil);
	registerMatFXPlugin();

	RunSyntheticTests();

	StreamFile sf;
	sf.open(argv[1], "rb");
	if (!findChunk(&sf, ID_CLUMP, nil, nil)) { fprintf(stderr, "no CLUMP\n"); return 2; }
	Clump *clump = Clump::streamRead(&sf);
	if (clump == nil) { fprintf(stderr, "Clump::streamRead failed\n"); return 2; }

	int nAtomics = clump->atomics.count();
	printf("clump: frames=%d atomics=%d\n", clump->getFrame()->count(), nAtomics);

	/* ---- 1. hierarchy classification (reVC: IsSAHierarchy) ---- */
	bool hasHi = false;
	FORLIST(lnk, clump->atomics) {
		const char *n = FName(Atomic::fromClump(lnk)->getFrame());
		if (strstr(n, "_hi")) hasHi = true;
	}
	bool saMode = !hasHi;
	printf("hierarchy: %s (SA-style=%s)\n", hasHi ? "VC (has _hi parts)" : "SA (no _hi parts at all)", saMode ? "yes" : "no");

	/* ---- 2. frame ids (reVC: SetFrameIds + the SA name aliases) ---- */
	printf("\n-- SetFrameIds (name -> hierarchy id) --\n");
	for (int i = 0; carIds[i].name; i++) {
		Desc *d = &carIds[i];
		Frame *f;
		if (d->flags & CLUMP_FLAG_NO_HIERID) continue;
		f = FindByName(clump->getFrame(), d->name, true, *(new bool));
		if (f) { SetFId(f, d->id); printf("   %-18s -> id %-3d (%s)\n", d->name, d->id, d->name); }
	}
	printf("   (only matched names are listed)\n");

	/* ---- 3. PreprocessHierarchy ---- */
	printf("\n-- PreprocessHierarchy --\n");
	int numDoors = 0, numComps = 0, numPos = 0;
	Atomic *comps[6];
	Frame *wheelFrames[4] = { nil, nil, nil, nil };
	int wheelFramesFound = 0;
	bool stop;

	for (int i = 0; carIds[i].name; i++) {
		Desc *d = &carIds[i];
		if ((d->flags & (VEHICLE_FLAG_COMP | VEHICLE_FLAG_POS)) == 0) continue;
		Frame *f = FindByName(clump->getFrame(), d->name, true, stop);
		if (f == nil) continue;
		if (d->flags & VEHICLE_FLAG_DOOR) numDoors++;
		if (d->flags & VEHICLE_FLAG_POS) {
			numPos++;
			printf("   position  %-18s found (frame destroyed after reading it)\n", d->name);
		} else {
			Atomic *a = FindAtomicOn(clump, f);
			if (a == nil) continue;
			if (numComps >= 6) { printf("   component %-18s: m_comps[] full, kept in clump\n", d->name); continue; }
			printf("   component %-18s -> m_comps[%d]\n", d->name, numComps);
			comps[numComps++] = a;
		}
	}
	for (int i = 0; carIds[i].name; i++) {
		Desc *d = &carIds[i];
		if (d->flags & (VEHICLE_FLAG_COMP | VEHICLE_FLAG_POS)) continue;
		Frame *f = FindById(clump->getFrame(), d->id);
		if (f == nil) continue;
		if (d->flags & VEHICLE_FLAG_DOOR) numDoors++;
		if (d->flags & VEHICLE_FLAG_COLLAPSE) {
			int before = 0;
			FORLIST(lnk, clump->atomics)
				for (Frame *p = Atomic::fromClump(lnk)->getFrame(); p; p = p->getParent())
					if (p == f) { before++; break; }
			CollapseChildren(clump, f, f);
			int after = 0;
			FORLIST(lnk, clump->atomics)
				for (Frame *p = Atomic::fromClump(lnk)->getFrame(); p; p = p->getParent())
					if (p == f) { after++; break; }
			printf("   collapse  %-18s meshes before=%d after=%d%s\n", d->name, before, after,
			       after > before ? "  <-- collapsed onto the dummy" : "");
		}
		if (d->flags & VEHICLE_FLAG_ADD_WHEEL) {
			if (saMode && FrameTreeHasAtomic(clump, f)) {
				printf("   wheel     %-18s keeps its own DFF mesh (VC wheel model NOT used)\n", d->name);
				if (wheelFramesFound < 4) wheelFrames[wheelFramesFound++] = f;
				continue;
			}
			printf("   wheel     %-18s empty -> its mesh is cloned from the wheel dummy below\n", d->name);
			if (wheelFramesFound < 4) wheelFrames[wheelFramesFound++] = f;
		}
	}

	printf("\n-- wheel meshes --\n");
	int have = 0;
	for (int i = 0; i < wheelFramesFound; i++) {
		printf("   %-18s mesh=%s\n", FName(wheelFrames[i]), FrameTreeHasAtomic(clump, wheelFrames[i]) ? "yes" : "no");
		if (FrameTreeHasAtomic(clump, wheelFrames[i])) have++;
	}
	int cloned = saMode ? CloneSAWheelMeshes(clump, wheelFrames, wheelFramesFound) : 0;
	int now = 0;
	for (int i = 0; i < wheelFramesFound; i++)
		if (FrameTreeHasAtomic(clump, wheelFrames[i])) now++;
	printf("   wheels with a mesh: before=%d, after SA cloning=%d (cloned %d)\n", have, now, cloned);

	/* ---- 4. what would actually be drawn ---- */
	int vanillaRender = 0, saRender = 0, destroyed = 0, notRendered = 0, vlo = 0;
	FORLIST(lnk, clump->atomics) {
		int rv = ClassifyRender(Atomic::fromClump(lnk), false);
		int rs = ClassifyRender(Atomic::fromClump(lnk), saMode);
		if (rv == 1) vanillaRender++;
		if (rv == 3) destroyed++;
		if (rv == 0) notRendered++;
		if (rs == 1) saRender++;
		if (rs == 2) vlo++;
	}
	printf("\n-- render callbacks (per atomic) --\n");
	printf("   vanilla VC rules : %d of %d atomics drawn (hi detail), %d not drawn at all, %d destroyed as _lo\n",
	       vanillaRender, clump->atomics.count(), notRendered, destroyed);
	printf("   with SA support  : %d of %d atomics drawn (hi detail) + %d very-low-detail\n",
	       saRender, clump->atomics.count(), vlo);

	printf("\nRESULT: %s\n", (saMode && now == 4 && saRender >= nAtomics - 1) ? "PASS" : "FAIL");
	return 0;
}
