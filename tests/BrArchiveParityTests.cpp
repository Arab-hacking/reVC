#ifndef ANDROID
#define ANDROID 1
#endif

#include "../src/extras/custom/CustomColFormat.h"
#include "../src/extras/custom/CustomWorldFormat.h"
#include "../src/extras/custom/MobileSpawnerMath.h"

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

} // namespace

int main() {
	TestBollAndClsMixedIndexing();
	TestAndroidIdeIplExactParity();
	TestMobileSpawnerMath();
	std::puts("BR archive parity (BOLL/CLS, Android 23/31 IDE/IPL, LinkLods, MobileSpawner) tests passed");
	return 0;
}
