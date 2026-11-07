#include "common.h"
#include "TxdStore.h"
#include "CustomModels.h"
#include "CustomWorldFormat.h"
#include "CustomSkyMath.h"
#include "VehicleMaterialClassify.h"
#include "brformats.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#define CHECK(cond) do { \
	if(!(cond)){ \
		std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
		std::abort(); \
	} \
} while(0)

void re3_assert(const char *, const char *, unsigned int, const char *) { std::abort(); }
void re3_log(const char *, const char *, unsigned int, const char *, const char *, ...) {}
RwBool RwTexDictionaryDestroy(RwTexDictionary *dict)
{
	if(dict)
		((rw::TexDictionary*)dict)->destroy();
	return true;
}
RwTexDictionary *RwTexDictionaryGetCurrent(void) { return (RwTexDictionary*)rw::TexDictionary::getCurrent(); }
RwTexDictionary *RwTexDictionarySetCurrent(RwTexDictionary *dict)
{
	rw::TexDictionary::setCurrent((rw::TexDictionary*)dict);
	return dict;
}
RwTexture *RwTexDictionaryFindNamedTexture(RwTexDictionary *dict, const char *name)
{
	return dict ? (RwTexture*)((rw::TexDictionary*)dict)->find(name) : nil;
}
void CCustomModels::ForgetTxdSlot(int32) {}

static rw::Texture *MakeDummyTex(const char *name)
{
	rw::Texture *t = rw::Texture::create(nil);
	CHECK(t != nil);
	std::strncpy(t->name, name, sizeof(t->name) - 1);
	t->name[sizeof(t->name) - 1] = '\0';
	return t;
}

static void TestTxdHierarchyAndVehicleFallback(void)
{
	CTxdStore::Initialise();

	const int sVeh = CTxdStore::AddTxdSlot("vehicle");
	const int sRip = CTxdStore::AddTxdSlot("rip");
	const int sSkyChild = CTxdStore::AddTxdSlot("br_sky_day_child");
	const int sCar = CTxdStore::AddTxdSlot("infernus_br");

	rw::TexDictionary *vehDict = rw::TexDictionary::create();
	rw::TexDictionary *ripDict = rw::TexDictionary::create();
	rw::TexDictionary *childDict = rw::TexDictionary::create();
	rw::TexDictionary *carDict = rw::TexDictionary::create();

	rw::Texture *tEnv = MakeDummyTex("vehicleenvmap128");
	rw::Texture *tSky = MakeDummyTex("sky_clouds_horizon");
	rw::Texture *tBody = MakeDummyTex("remap_body");
	vehDict->add(tEnv);
	ripDict->add(tSky);
	carDict->add(tBody);

	CTxdStore::GetSlot(sVeh)->texDict = (RwTexDictionary*)vehDict;
	CTxdStore::GetSlot(sRip)->texDict = (RwTexDictionary*)ripDict;
	CTxdStore::GetSlot(sSkyChild)->texDict = (RwTexDictionary*)childDict;
	CTxdStore::GetSlot(sCar)->texDict = (RwTexDictionary*)carDict;

	CTxdStore::SetTxdParent(sSkyChild, sRip);
	CHECK(CTxdStore::GetParentTxdSlot(sSkyChild) == sRip);

	// Parent TXD lookup resolves rip textures from child TXD
	CHECK(CTxdStore::FindTextureInHierarchy(sSkyChild, "sky_clouds_horizon") == tSky);

	// Shared vehicle.txd fallback resolves common SA/BR vehicle textures
	CHECK(CTxdStore::FindTextureInHierarchy(sCar, "remap_body") == tBody);
	CHECK(CTxdStore::FindTextureInHierarchy(sCar, "vehicleenvmap128") == tEnv);
	CHECK(CTxdStore::FindTextureInHierarchy(sCar, "nonexistent_tex") == nil);

	// Cycle guard: attempting to form A -> B -> A is rejected by SetTxdParent
	CTxdStore::SetTxdParent(sRip, sSkyChild);
	CHECK(CTxdStore::GetParentTxdSlot(sRip) == -1);
	CHECK(CTxdStore::FindTextureInHierarchy(sSkyChild, "missing_in_cycle") == nil);

	CTxdStore::GameShutdown();
	CTxdStore::Shutdown();
}

static void TestGl3BgraAnd16BitImageUnpack(void)
{
	// Verify Image::convertTo32 unpacks ARGB1555 (bit 15 = A, 14..10 = R, 9..5 = G, 4..0 = B)
	rw::Image *img = rw::Image::create(2, 1, 16);
	CHECK(img != nil);
	img->allocate();
	uint16_t px0 = (uint16_t)((1u << 15) | (31u << 10)); // Pure red, A=1
	uint16_t px1 = (uint16_t)((1u << 15) | 31u);         // Pure blue, A=1
	std::memcpy(img->pixels + 0, &px0, 2);
	std::memcpy(img->pixels + 2, &px1, 2);
	img->convertTo32();
	CHECK(img->depth == 32);
	CHECK(img->pixels[0] == 255 && img->pixels[1] == 0 && img->pixels[2] == 0 && img->pixels[3] == 255);
	CHECK(img->pixels[4] == 0 && img->pixels[5] == 0 && img->pixels[6] == 255 && img->pixels[7] == 255);
	img->destroy();

	// Verify GL3 C8888 (BGRA byte order) & RGB565 -> Image RGBA conversion without R/B swap
	rw::uint8 bgra[4] = { 12, 34, 210, 250 }; // B=12, G=34, R=210, A=250
	rw::uint8 rgba[4] = { 0, 0, 0, 0 };
	rw::conv_RGBA8888_from_BGRA8888(rgba, bgra);
	CHECK(rgba[0] == 210 && rgba[1] == 34 && rgba[2] == 12 && rgba[3] == 250);

	uint16_t rgba5551Red = (uint16_t)((31u << 11) | 1u);
	uint16_t argb1555Red = 0;
	rw::conv_ARGB1555_from_RGBA5551((rw::uint8*)&argb1555Red, (rw::uint8*)&rgba5551Red);
	rw::conv_RGBA8888_from_ARGB1555(rgba, (rw::uint8*)&argb1555Red);
	CHECK(rgba[0] == 255 && rgba[1] == 0 && rgba[2] == 0 && rgba[3] == 255);
}

static void TestIdeTxdpAndSkyDefs(void)
{
	const char *ideText =
		"objs\n"
		"10656, BR_Sky_Rasv, rip, 500, 0\n"
		"10657, BR_Sky_Day, rip, 500, 0\n"
		"10658, BR_Sky_Zak, rip, 500, 0\n"
		"10659, BR_Sky_Night, rip, 500, 0\n"
		"14389, BR_SkyDome, SkyDome, 100, 0\n"
		"end\n"
		"txdp\n"
		"br_sky_rasv_long_child_txd_name, rip_shared_sky_parent_txd_27chr\n"
		"bad_overlong_thirtytwo_char_txd_name_x, rip\n"
		"end\n";

	std::vector<customworld::Model> models;
	std::vector<std::pair<std::string, std::string> > parents;
	customworld::Stats st = customworld::ParseIde(
		(const uint8_t*)ideText, std::strlen(ideText),
		[&](const customworld::Model &m){ models.push_back(m); },
		[&](const std::string &child, const std::string &parent){
			parents.emplace_back(child, parent);
		});

	CHECK(models.size() == 5);
	CHECK(parents.size() == 1);
	CHECK(parents[0].first == "br_sky_rasv_long_child_txd_name");
	CHECK(parents[0].second == "rip_shared_sky_parent_txd_27chr");
	CHECK(st.rejected == 1);
}

static void TestSkyTimeBlendAndScale(void)
{
	// Anchor hours: 00:00 Night, 06:00 Rasv, 12:00 Day, 19:00 Zak
	auto bNight = customsky::EvaluateSkyBlend(0, 0, 0.0f);
	CHECK(bNight.base == customsky::PHASE_NIGHT && bNight.alpha == 0.0f);

	auto bDawn = customsky::EvaluateSkyBlend(6, 0, 0.0f);
	CHECK(bDawn.base == customsky::PHASE_RASV && bDawn.alpha == 0.0f);

	auto bDay = customsky::EvaluateSkyBlend(12, 0, 0.0f);
	CHECK(bDay.base == customsky::PHASE_DAY && bDay.alpha == 0.0f);

	auto bDusk = customsky::EvaluateSkyBlend(19, 0, 0.0f);
	CHECK(bDusk.base == customsky::PHASE_ZAK && bDusk.alpha == 0.0f);

	// Mid-transition hours blend smoothly (0 < alpha < 1)
	auto bMorning = customsky::EvaluateSkyBlend(8, 0, 0.0f);
	CHECK(bMorning.base == customsky::PHASE_RASV && bMorning.overlay == customsky::PHASE_DAY);
	CHECK(std::fabs(bMorning.alpha - 0.5f) < 1e-4f);

	// Continuity across 23:59:59 -> 00:00:00
	auto bPreMidnight = customsky::EvaluateSkyBlend(23, 59, 59.0f);
	CHECK(bPreMidnight.base == customsky::PHASE_NIGHT && bPreMidnight.alpha == 0.0f);

	// Numpad hour step & preset cycling (00:00 / 06:00 / 12:00 / 19:00)
	CHECK(customsky::StepClockHour(0, -1) == 23);
	CHECK(customsky::StepClockHour(23, 1) == 0);
	CHECK(customsky::StepPresetHour(0, 1) == 6);
	CHECK(customsky::StepPresetHour(6, 1) == 12);
	CHECK(customsky::StepPresetHour(12, 1) == 19);
	CHECK(customsky::StepPresetHour(19, 1) == 0);
	CHECK(customsky::StepPresetHour(0, -1) == 19);

	// Normalized sky scale keeps geometry safely inside camera farClip
	float sBig = customsky::ComputeNormalizedSkyScale(0.0f, 500.0f, 65.0f);
	CHECK(std::fabs(500.0f * sBig - 65.0f) < 1e-3f);
	float sUnit = customsky::ComputeNormalizedSkyScale(0.0f, 1.0f, 65.0f);
	CHECK(std::fabs(1.0f * sUnit - 65.0f) < 1e-3f);
}

static void TestVehicleMaterialClassification(void)
{
	int slot = 0;
	CHECK(customveh::IsEditableCarColorMarker(0x3C, 0xFF, 0x00, slot) && slot == 1);
	CHECK(customveh::IsEditableCarColorMarker(0x00, 0xFF, 0xFF, slot) && slot == 1);
	CHECK(customveh::IsEditableCarColorMarker(0xFF, 0x00, 0xAF, slot) && slot == 2);
	CHECK(customveh::IsEditableCarColorMarker(0xFF, 0x00, 0xFF, slot) && slot == 2);
	CHECK(!customveh::IsEditableCarColorMarker(0xFF, 0xFF, 0xFF, slot) && slot == 0);

	// 1) Editable body paint without DFF specular still gets strong clearcoat reflection
	customveh::VehicleMatInput paint = {};
	paint.isEditablePaint = true;
	paint.r = paint.g = paint.b = paint.a = 255;
	paint.frameName = "chassis_hi";
	paint.texName = "vehiclegeneric256";
	CHECK(customveh::ComputeVehicleMaterialSpecular(paint) >= 0.78f);

	// 2) Translucent windshield gets strong Fresnel glass reflection
	customveh::VehicleMatInput glass = {};
	glass.r = 200; glass.g = 220; glass.b = 230; glass.a = 90;
	glass.frameName = "windscreen_hi_ok";
	glass.texName = "vehicleshatter128";
	CHECK(customveh::ComputeVehicleMaterialSpecular(glass) >= 0.80f);

	// 3) Rubber tire on wheel frame gets 0 reflection even if explicit specular existed
	customveh::VehicleMatInput tire = {};
	tire.r = tire.g = tire.b = tire.a = 255;
	tire.frameName = "wheel_rf_dummy";
	tire.texName = "wheel_tire_tread";
	tire.rawStructSpecular = 0.8f;
	CHECK(customveh::ComputeVehicleMaterialSpecular(tire) == 0.0f);

	// 4) Interior seats/dash and underbody get 0 reflection
	customveh::VehicleMatInput interior = {};
	interior.r = interior.g = interior.b = interior.a = 255;
	interior.frameName = "interior_hi";
	interior.texName = "salon_leather";
	CHECK(customveh::ComputeVehicleMaterialSpecular(interior) == 0.0f);

	// 5) SA 0x0253F2FC shininess on chrome bumper scales into Neo range
	customveh::VehicleMatInput saChrome = {};
	saChrome.r = saChrome.g = saChrome.b = saChrome.a = 255;
	saChrome.frameName = "bump_front_ok";
	saChrome.texName = "vehiclegeneric256";
	saChrome.hasSaEnv = true;
	saChrome.saShininess = 0.25f;
	CHECK(customveh::ComputeVehicleMaterialSpecular(saChrome) >= 0.80f);

	// 6) Opaque SA headlight emitter color marker stays 0, translucent glass lens reflects
	customveh::VehicleMatInput lightOpaque = {};
	lightOpaque.r = 255; lightOpaque.g = 175; lightOpaque.b = 0; lightOpaque.a = 255;
	lightOpaque.frameName = "chassis";
	lightOpaque.texName = "vehiclelights128";
	CHECK(customveh::ComputeVehicleMaterialSpecular(lightOpaque) == 0.0f);

	customveh::VehicleMatInput lightLens = lightOpaque;
	lightLens.a = 120;
	CHECK(customveh::ComputeVehicleMaterialSpecular(lightLens) >= 0.35f);
}

static void TestBrMatfx10ReflectionExtraction(void)
{
	// Build a minimal BR matfx type-10 blob:
	// header: u32 10, u32 0, u32 0 (no textures) -> 12 bytes
	// u32 0 -> 4 bytes
	// float section: u32 count=2, {len=8,"metallic",0.9f}, {len=9,"roughness",0.1f}
	std::vector<uint8_t> blob;
	auto pushU32 = [&](uint32_t v){
		uint8_t b[4]; std::memcpy(b, &v, 4);
		blob.insert(blob.end(), b, b + 4);
	};
	auto pushF32 = [&](float v){
		uint8_t b[4]; std::memcpy(b, &v, 4);
		blob.insert(blob.end(), b, b + 4);
	};
	auto pushKeyF32 = [&](const char *k, float v){
		uint32_t kl = (uint32_t)std::strlen(k);
		pushU32(kl);
		blob.insert(blob.end(), (const uint8_t*)k, (const uint8_t*)k + kl);
		pushF32(v);
	};

	pushU32(10); pushU32(0); pushU32(0);
	pushU32(0);
	pushU32(2);
	pushKeyF32("metallic", 0.9f);
	pushKeyF32("roughness", 0.1f);

	float met = 0.f, rough = 0.f;
	CHECK(br::matfx10Float(blob.data(), blob.size(), "metallic", met) && std::fabs(met - 0.9f) < 1e-5f);
	CHECK(br::matfx10Float(blob.data(), blob.size(), "roughness", rough) && std::fabs(rough - 0.1f) < 1e-5f);
}

int main()
{
	rw::Engine::init();
	rw::EngineOpenParams openParams = {};
	CHECK(rw::Engine::open(&openParams));
	CHECK(rw::Engine::start());

	TestTxdHierarchyAndVehicleFallback();
	TestGl3BgraAnd16BitImageUnpack();
	TestIdeTxdpAndSkyDefs();
	TestSkyTimeBlendAndScale();
	TestVehicleMaterialClassification();
	TestBrMatfx10ReflectionExtraction();

	rw::Engine::stop();
	rw::Engine::close();
	rw::Engine::term();
	std::puts("Sky, vehicle reflection, TXD hierarchy, and GL3 BGRA/16-bit texture tests passed");
	return 0;
}
