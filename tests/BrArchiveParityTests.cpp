#ifndef ANDROID
#define ANDROID 1
#endif

#include <cstdint>
#ifndef nil
#define nil nullptr
#endif
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

#include "../src/extras/custom/BrCollisionFormat.h"
#include "../src/extras/custom/CustomColFormat.h"
#include "../src/extras/custom/CustomWorldFormat.h"
#include "../src/extras/custom/MobileSpawnerMath.h"
#include "../src/extras/custom/br/brformats.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

static void W16(std::vector<uint8_t> &b, size_t off, uint16_t n) {
	b[off] = (uint8_t)(n & 0xFF);
	b[off + 1] = (uint8_t)(n >> 8);
}

static void W32(std::vector<uint8_t> &b, size_t off, uint32_t n) {
	for (int k = 0; k < 4; k++)
		b[off + k] = (uint8_t)(n >> (k * 8));
}

static void WF(std::vector<uint8_t> &b, size_t off, float x) {
	uint32_t n = 0;
	std::memcpy(&n, &x, 4);
	W32(b, off, n);
}

static std::vector<uint8_t> MakeColHeaderBlock(const char *tag, const char *modelName, uint32_t bodyBytes = 76) {
	std::vector<uint8_t> b(8 + bodyBytes, 0);
	std::memcpy(b.data(), tag, 4);
	W32(b, 4, bodyBytes);
	const size_t nlen = std::strlen(modelName);
	assert(nlen <= 21);
	std::memcpy(b.data() + 8, modelName, nlen);
	W16(b, 30, 0);
	WF(b, 32, 0.0f); WF(b, 36, 0.0f); WF(b, 40, 0.0f); WF(b, 44, 5.0f);
	WF(b, 48, -1.0f); WF(b, 52, -1.0f); WF(b, 56, -1.0f);
	WF(b, 60,  1.0f); WF(b, 64,  1.0f); WF(b, 68,  1.0f);
	return b;
}

static void TestBollAndClsMixedIndexing() {
	// Mix CLST + BOLL (with 21-char name) + CLSF + COLL in a single container
	std::vector<uint8_t> clst = MakeColHeaderBlock("CLST", "skip_clst_entry", 80);
	std::vector<uint8_t> boll = MakeColHeaderBlock("BOLL", "arzamas_lamp_pole_001", 92);
	std::vector<uint8_t> clsf = MakeColHeaderBlock("CLSF", "skip_clsf_entry", 72);
	std::vector<uint8_t> coll = MakeColHeaderBlock("COLL", "vc_box_simple", 92);

	std::vector<uint8_t> container;
	container.insert(container.end(), clst.begin(), clst.end());
	container.insert(container.end(), boll.begin(), boll.end());
	container.insert(container.end(), clsf.begin(), clsf.end());
	container.insert(container.end(), coll.begin(), coll.end());

	std::vector<customcolformat::Block> indexed;
	const size_t bad = customcolformat::Index(container.data(), container.size(), indexed);
	assert(bad == 0);
	assert(indexed.size() == 2);
	assert(indexed[0].name == "arzamas_lamp_pole_001" && !indexed[0].vc);
	assert(indexed[1].name == "vc_box_simple" && indexed[1].vc);
}

static void TestAndroidIdeIplExactParity() {
	// 1. 23-char model name + 31-char TXD name + FEET_OFFSET skip in peds.ide
	const std::string ideText =
		"objs\n"
		"6510, arzamas_building_ext_23, arzamas_shared_texture_atlas_31, 280.0, 0\n"
		"end\n"
		"peds\n"
		"11950, MK_wfesteraddams_man23x, MK_wfesteraddams_txd_name_len31, CRIMINAL, STAT_CRIMINAL, man, 110F, 0, man, 4, 4\n"
		"FEET_OFFSET 0.05 -0.02\n"
		"11951, br_skin_after_feet_ofs, br_skin_after_feet_txd, CIVMALE, STAT_SENSIBLE_GUY, man, 0, 0, man, 1, 2\n"
		"end\n";

	customworld::ModelIndex idx;
	std::vector<customworld::Model> parsedModels;
	customworld::Stats ideStats = customworld::ParseIde(
		(const uint8_t*)ideText.data(), ideText.size(),
		[&](const customworld::Model &m) {
			parsedModels.push_back(m);
			idx.Add(m, false);
		});
	assert(ideStats.records == 3 && ideStats.rejected == 0);
	assert(parsedModels[0].name == "arzamas_building_ext_23");
	assert(parsedModels[0].txd == "arzamas_shared_texture_atlas_31");
	assert(parsedModels[1].name == "MK_wfesteraddams_man23x");
	assert(parsedModels[2].name == "br_skin_after_feet_ofs");

	// 2. veh_mods.ide preserveExistingId=true must NOT clobber map building's lastNameForId[6510]
	customworld::Model vehMod;
	vehMod.kind = customworld::Model::OBJECT;
	vehMod.id = 6510;
	vehMod.name = "nto_b_l";
	vehMod.txd = "veh_mods";
	vehMod.distance = 100.0f;
	vehMod.flags = 0;
	idx.Add(vehMod, true);
	assert(idx.lastNameForId.at(6510) == "arzamas_building_ext_23");
	assert(idx.definitions.count("nto_b_l") == 1);

	// 3. IPL 0-based instIndex (ignoring comments/blanks) + -1-1 concatenated terminator
	const std::string iplText =
		"inst\n"
		"# leading comment does NOT increment instIndex\n"
		"\n"
		"6510, arzamas_building_ext_23, 0, 822.0, 773.0, 12.5, 0.0, 0.0, 0.0, 1.0, 1\n"
		"6511, lod_arzamas_bldg_ext23, 0, 822.0, 773.0, 12.5, 0.0, 0.0, 0.0, 1.0, -1\n"
		"-1-1\n"
		"end\n";
	std::vector<customworld::Instance> insts;
	customworld::Stats iplStats = customworld::ParseIpl(
		(const uint8_t*)iplText.data(), iplText.size(),
		[&](const customworld::Instance &in) { insts.push_back(in); });
	assert(iplStats.records == 2 && iplStats.rejected == 0);
	assert(insts[0].instIndex == 0 && insts[0].lod == 1);
	assert(insts[1].instIndex == 1 && insts[1].lod == -1);
}

static void TestMobileSpawnerMath() {
	using namespace mobilespawner;

	// 1. Skin catalog sort: MI_PLAYER (0) first, then custom (>=4900) ascending, then stock ascending
	std::vector<CatalogItem> skins = {
		{ 7, false, false, "male01" },
		{ 6512, true, true, "br_skin_6512" },
		{ 0, false, false, "player" },
		{ 4901, true, true, "br_skin_4901" },
		{ 2, false, false, "cop" }
	};
	SortCatalogItems(skins, true);
	assert(skins[0].modelId == 0);
	assert(skins[1].modelId == 4901 && skins[1].isCustom);
	assert(skins[2].modelId == 6512 && skins[2].isCustom);
	assert(skins[3].modelId == 2 && !skins[3].isCustom);
	assert(skins[4].modelId == 7 && !skins[4].isCustom);

	// 2. Vehicle catalog sort & non-drivable exclusions
	assert(IsExcludedStockVehicleId(165)); // MI_CHOPPER
	assert(IsExcludedStockVehicleId(180)); // MI_AIRTRAIN
	assert(IsExcludedStockVehicleId(181)); // MI_DEADDODO
	assert(IsExcludedStockVehicleId(171)); // MI_RCBANDIT
	assert(!IsExcludedStockVehicleId(130)); // Landstalker
	assert(!IsExcludedStockVehicleId(191)); // PCJ-600
	assert(!IsExcludedStockVehicleId(5200)); // Custom BR car

	// 3. Spawn transform math (including zero-forward fallback)
	float sx = 0, sy = 0, sz = 0, rx = 0, ry = 0;
	ComputeVehicleSpawnTransform(100.0f, 200.0f, 15.0f, 0.6f, 0.8f, 5.0f, &sx, &sy, &sz, &rx, &ry);
	assert(std::fabs(sx - 103.0f) < 1.0e-4f);
	assert(std::fabs(sy - 204.0f) < 1.0e-4f);
	assert(std::fabs(sz - 15.0f) < 1.0e-4f);
	assert(std::fabs(rx - 0.8f) < 1.0e-4f && std::fabs(ry + 0.6f) < 1.0e-4f);

	ComputeVehicleSpawnTransform(10.0f, 20.0f, 5.0f, 0.0f, 0.0f, 5.5f, &sx, &sy, &sz, &rx, &ry);
	assert(std::fabs(sx - 10.0f) < 1.0e-4f && std::fabs(sy - 25.5f) < 1.0e-4f);

	// 4. Touch picker layout & hit-testing on 2400x1080 landscape phone
	PickerLayout L = BuildPickerLayout(2400.0f, 1080.0f, 680.0f, 88.0f, 2.75f);
	assert(HitTestPicker(L, L.left - 10.0f, L.top + 10.0f) == PHIT_OUTSIDE);
	assert(HitTestPicker(L, L.BtnCenterX(0), L.BtnCenterY()) == PHIT_PREV);
	assert(HitTestPicker(L, L.BtnCenterX(1), L.BtnCenterY()) == PHIT_STEP_MINUS10);
	assert(HitTestPicker(L, L.BtnCenterX(2), L.BtnCenterY()) == PHIT_STEP_PLUS10);
	assert(HitTestPicker(L, L.BtnCenterX(3), L.BtnCenterY()) == PHIT_NEXT);
	assert(HitTestPicker(L, L.BtnCenterX(4), L.BtnCenterY()) == PHIT_CLOSE);
	assert(HitTestPicker(L, L.left + 30.0f, L.rowTop + L.rowHeight * 0.5f) == PHIT_ROW_0);
	assert(HitTestPicker(L, L.left + 30.0f, L.rowTop + L.rowHeight * 5.5f) == PHIT_ROW_5);
	assert(WrapIndex(-1, 12) == 11 && WrapIndex(12, 12) == 0 && WrapIndex(5, 0) == 0);
}

static void TestClsMaterialOffsetsAndClsfBollConversion() {
	// 1. Regression test for fixColBlockMaterials: a CLST block with BOTH boxes and
	// negative int16 vertices (e.g. -150.0m -> -19200 -> 0xB500, high byte 0xB5 = 181 > 178)
	// must NOT treat offV (+96) as offB (+88) and corrupt vertex coordinates!
	std::vector<uint8_t> clst(120, 0);
	std::memcpy(clst.data(), "CLST", 4);
	std::memcpy(clst.data() + 8, "road_arz_chunk_01", 17);
	WF(clst, 32, 0.0f); WF(clst, 36, 0.0f); WF(clst, 40, 0.0f); WF(clst, 44, 220.0f);
	WF(clst, 48, -160.0f); WF(clst, 52, -160.0f); WF(clst, 56, -10.0f);
	WF(clst, 60,  160.0f); WF(clst, 64,  160.0f); WF(clst, 68,  20.0f);
	W16(clst, 72, 0); // nS = 0
	W16(clst, 74, 1); // nB = 1
	W16(clst, 76, 1); // nF = 1
	clst[80] = 0x02;
	W32(clst, 84, 0);   // offS
	W32(clst, 88, 116); // offB -> file offset 120 (28 bytes: 120..147)
	W32(clst, 96, 144); // offV -> file offset 148 (3 * 6 = 18 bytes + 2 pad: 148..167)
	W32(clst, 100, 164);// offF -> file offset 168 (8 bytes: 168..175)
	clst.resize(176, 0);
	// Box at 120..147 with BR surface 255 at +24 (offset 144)
	WF(clst, 120, -2.0f); WF(clst, 124, -2.0f); WF(clst, 128, -1.0f);
	WF(clst, 132,  2.0f); WF(clst, 136,  2.0f); WF(clst, 140,  1.0f);
	clst[144] = 255;
	// 3 vertices at 148..165, with negative coords whose high byte > 178 (0xB500 = -19200 = -150.0m)
	const int16_t vx0 = -19200, vy0 = -19200, vz0 = 128;
	const int16_t vx1 =  19200, vy1 = -19200, vz1 = 128;
	const int16_t vx2 =      0, vy2 =  19200, vz2 = 128;
	W16(clst, 148, (uint16_t)vx0); W16(clst, 150, (uint16_t)vy0); W16(clst, 152, (uint16_t)vz0);
	W16(clst, 154, (uint16_t)vx1); W16(clst, 156, (uint16_t)vy1); W16(clst, 158, (uint16_t)vz1);
	W16(clst, 160, (uint16_t)vx2); W16(clst, 162, (uint16_t)vy2); W16(clst, 164, (uint16_t)vz2);
	// Face at 168..175 with SA grass surface 80
	W16(clst, 168, 0); W16(clst, 170, 1); W16(clst, 172, 2);
	clst[174] = 80;
	W32(clst, 4, (uint32_t)(clst.size() - 8));

	br::ClsStats clsSt;
	std::vector<uint8_t> colOut = br::convertClsToCol(clst, clsSt);
	assert(clsSt.blocks == 1 && clsSt.bad == 0 && clsSt.clst == 1);
	// Box surface 255 mapped to tarmac (1), face surface 80 mapped to grass (2)
	assert(colOut[144] == 1);
	assert(colOut[174] == 2);
	// EVERY negative vertex coordinate MUST remain 100% intact!
	assert((int16_t)(colOut[148] | (colOut[149] << 8)) == vx0);
	assert((int16_t)(colOut[150] | (colOut[151] << 8)) == vy0);
	assert((int16_t)(colOut[154] | (colOut[155] << 8)) == vx1);
	assert((int16_t)(colOut[156] | (colOut[157] << 8)) == vy1);
	assert((int16_t)(colOut[162] | (colOut[163] << 8)) == vy2);

	// 2. BOLL & CLSF with surface 255 must convert directly into ValidVC COLL
	std::vector<uint8_t> boll(120, 0);
	std::memcpy(boll.data(), "BOLL", 4);
	std::memcpy(boll.data() + 8, "boll_curb_wall_01", 17);
	WF(boll, 32, 3.0f); WF(boll, 36, 0.0f); WF(boll, 40, 0.0f); WF(boll, 44, 0.0f);
	WF(boll, 48, -2.0f); WF(boll, 52, -2.0f); WF(boll, 56, -1.0f);
	WF(boll, 60,  2.0f); WF(boll, 64,  2.0f); WF(boll, 68,  1.0f);
	// 0 spheres, 0 lines, 1 box (with surface 255), 0 verts, 0 faces
	W32(boll, 72, 0);
	W32(boll, 76, 0);
	W32(boll, 80, 1);
	WF(boll, 84, -2.0f); WF(boll, 88, -2.0f); WF(boll, 92, -1.0f);
	WF(boll, 96,  2.0f); WF(boll, 100, 2.0f); WF(boll, 104, 1.0f);
	boll[108] = 255;
	W32(boll, 112, 0);
	W32(boll, 116, 0);
	W32(boll, 4, (uint32_t)(boll.size() - 8));

	br::ClsStats bollSt;
	std::vector<uint8_t> bollVc = br::convertClsToCol(boll, bollSt);
	assert(bollSt.blocks == 1 && bollSt.bad == 0);
	assert(customcolformat::ValidVC(bollVc.data(), bollVc.size()));

	// 3. CED2 CLSF with a 20-character name (no terminator, the CED2 marker sits
	// where the padding would be) and a box body: numBoxes at +0x30, boxes from
	// +0x34. This is the layout of PUNCT ANALYSE v45 section 10.3, not the old
	// "counts at +72" one.
	std::vector<uint8_t> ced2Clsf(8 + 0x18 + 0x34 + 28, 0);
	std::memcpy(ced2Clsf.data(), "CLSF", 4);
	std::memcpy(ced2Clsf.data() + 8, "12345678901234567890", 20);
	std::memcpy(ced2Clsf.data() + 8 + 20, "CED2", 4);
	size_t h = 8 + 0x18;
	WF(ced2Clsf, h + 0x00, -2.0f); WF(ced2Clsf, h + 0x04, -2.0f); WF(ced2Clsf, h + 0x08, -1.0f);
	WF(ced2Clsf, h + 0x0c,  2.0f); WF(ced2Clsf, h + 0x10,  2.0f); WF(ced2Clsf, h + 0x14,  1.0f);
	WF(ced2Clsf, h + 0x18,  0.0f); WF(ced2Clsf, h + 0x1c,  0.0f); WF(ced2Clsf, h + 0x20,  0.0f);
	WF(ced2Clsf, h + 0x24,  3.0f);
	W32(ced2Clsf, h + 0x30, 1);                    // numBoxes; the box starts at +0x34
	WF(ced2Clsf, h + 0x34,  -2.0f); WF(ced2Clsf, h + 0x38,  -2.0f); WF(ced2Clsf, h + 0x3c,  -1.0f);
	WF(ced2Clsf, h + 0x40,   2.0f); WF(ced2Clsf, h + 0x44,   2.0f); WF(ced2Clsf, h + 0x48,   1.0f);
	ced2Clsf[h + 0x34 + 24] = 80;                  // SA grass -> VC grass (2)
	W32(ced2Clsf, 4, (uint32_t)(ced2Clsf.size() - 8));

	std::vector<brcollision::Block> brRefs;
	assert(brcollision::Index(ced2Clsf.data(), ced2Clsf.size(), brRefs) == 0);
	assert(brRefs.size() == 1 && brRefs[0].name == "12345678901234567890");
	std::vector<uint8_t> ced2Vc;
	const char *why = nullptr;
	assert(brcollision::Convert(ced2Clsf.data(), ced2Clsf.size(), ced2Vc, &why));
	assert(customcolformat::ValidVC(ced2Vc.data(), ced2Vc.size()));
	// 0 spheres, 0 lines, 1 box, 0 vertices, 0 triangles.
	assert(brcollision::U32(ced2Vc.data() + 72) == 0);
	assert(brcollision::U32(ced2Vc.data() + 76) == 0);
	assert(brcollision::U32(ced2Vc.data() + 80) == 1);
	assert(brcollision::U32(ced2Vc.data() + 112) == 0);
	assert(brcollision::U32(ced2Vc.data() + 116) == 0);
}

} // namespace

int main() {
	TestBollAndClsMixedIndexing();
	TestAndroidIdeIplExactParity();
	TestMobileSpawnerMath();
	TestClsMaterialOffsetsAndClsfBollConversion();
	std::puts("BR archive parity (BOLL/CLS, Android 23/31 IDE/IPL, LinkLods, MobileSpawner) tests passed");
	return 0;
}
