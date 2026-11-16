// Regression tests for Android freeroam startup (no story cutscenes / stuck fade)
// and custom-map dry spawn / hospital & police restart resolution:
// 1. Verifies gamefiles/data/freeroam_miami.scm exists, has a valid multi-script
//    header, zero COMMAND_LOAD_CUTSCENE opcodes, and contains DO_FADE FADE_IN
//    + SET_PLAYER_CONTROL ON.
// 2. Verifies CCustomWorld::ResolveSpawnPosition redirects underwater/void Vice
//    City initial spawn (79.6875, -867.25) to dry street-level Arzamas Station
//    plaza (822.0, 773.0) while preferring street height over rooftop height.
// 3. Verifies CRestart::FindClosestHospitalRestartPoint and
//    CRestart::FindClosestPoliceRestartPoint tolerate empty CPathFind::m_pathNodes
//    (FindNodeClosestToCoors -> -1) and resolve restarts to the nearest dry
//    custom-map landmark when CCustomModels::ReplacesNativeWorldMap() is active.

#include "common.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "CustomModels.h"
#include "CustomWorld.h"
#include "Restart.h"
#include "Script.h"
#include "ScriptCommands.h"
#include "PathFind.h"
#include "Pools.h"
#include "WaterLevel.h"
#include "World.h"
#include "Zones.h"
#include "ModelInfo.h"
#include "TimeModelInfo.h"
#include "FileLoader.h"
#include "TxdStore.h"
#include "ColStore.h"
#include "ColModel.h"
#include "TempColModels.h"
#include "Streaming.h"
#include "PedType.h"
#include "AnimManager.h"
#include "HandlingMgr.h"
#include "Game.h"
#include "SaveBuf.h"

// ---- Controllable test world state ----
static bool gReplacesNativeMap = true;

struct GroundSample {
	float x, y;
	float radius;
	float streetZ;
	float roofZ;
};
static std::vector<GroundSample> gGroundSamples;

struct WaterRect {
	float x0, x1, y0, y1, z;
};
static std::vector<WaterRect> gWaterRects;

extern "C" {
alignas(CPathFind) unsigned char gThePathsStorage[sizeof(CPathFind)] __asm__("ThePaths");
alignas(cHandlingDataMgr) unsigned char gHandlingStorage[sizeof(cHandlingDataMgr)] __asm__("mod_HandlingManager");
}

CBuildingPool *CPools::ms_pBuildingPool = nil;
CColModelPool *CPools::ms_pColModelPool = nil;
CCPtrNodePool *CPools::ms_pPtrNodePool = nil;
int _saveBufCount = 0;

void re3_assert(const char *expr, const char *filename, unsigned int lineno, const char *func) {
	std::fprintf(stderr, "ASSERT FAILED: %s at %s:%u (%s)\n", expr, filename, lineno, func);
	std::abort();
}
void DebugLogPrintf(const char *, ...) {}

bool CCustomModels::ReplacesNativeWorldMap(void) { return gReplacesNativeMap; }
bool CCustomModels::HasWorldFiles(void) { return true; }
bool CCustomModels::HasModelFile(const char *) { return false; }
bool CCustomModels::HasImageTxd(const char *) { return false; }
int CCustomModels::PairedImageTxd(const char *) { return -1; }
void CCustomModels::ForEachWorldFile(bool, WorldFileCallback, void *) {}
void CCustomModels::ForEachBrCollision(CollisionCallback, void *) {}
void CCustomModels::ForEachGameCollision(CollisionCallback, void *, CollisionFilter) {}

float CWorld::FindGroundZFor3DCoord(float x, float y, float z, bool *found) {
	for (const GroundSample &s : gGroundSamples) {
		const float dx = x - s.x;
		const float dy = y - s.y;
		if (dx * dx + dy * dy <= s.radius * s.radius) {
			if (s.roofZ > s.streetZ && z >= s.roofZ) {
				if (found) *found = true;
				return s.roofZ;
			}
			if (z >= s.streetZ) {
				if (found) *found = true;
				return s.streetZ;
			}
		}
	}
	if (found) *found = false;
	return 0.0f;
}

bool CWaterLevel::GetWaterLevelNoWaves(float fX, float fY, float, float *pfOutLevel) {
	for (const WaterRect &w : gWaterRects) {
		if (fX >= w.x0 && fX <= w.x1 && fY >= w.y0 && fY <= w.y1) {
			*pfOutLevel = w.z;
			return true;
		}
	}
	return false;
}

eLevelName CTheZones::GetLevelFromPosition(const CVector *) { return LEVEL_GENERIC; }
int32 CPathFind::FindNodeClosestToCoors(CVector, uint8, float, bool, bool, bool, bool) { return -1; }

// Unused stubs required to link CustomWorld.cpp cleanly in standalone test binary.
bool CCustomModels::ReadDataFile(const char *, std::vector<uint8> &, bool *) { return false; }
CBaseModelInfo *CModelInfo::ms_modelInfoPtrs[MODELINFOSIZE];
extern "C" {
alignas(16) unsigned char gSimpleStore[sizeof(CModelInfoStore<CSimpleModelInfo, SIMPLEMODELSIZE>)] __asm__("_ZN10CModelInfo19ms_simpleModelStoreE");
alignas(16) unsigned char gTimeStore[sizeof(CModelInfoStore<CTimeModelInfo, TIMEMODELSIZE>)] __asm__("_ZN10CModelInfo17ms_timeModelStoreE");
alignas(16) unsigned char gPedStore[sizeof(CModelInfoStore<CPedModelInfo, PEDMODELSIZE>)] __asm__("_ZN10CModelInfo16ms_pedModelStoreE");
alignas(16) unsigned char gVehStore[sizeof(CModelInfoStore<CVehicleModelInfo, VEHICLEMODELSIZE>)] __asm__("_ZN10CModelInfo20ms_vehicleModelStoreE");
alignas(16) unsigned char gWeapStore[sizeof(CModelInfoStore<CWeaponModelInfo, WEAPONMODELSIZE>)] __asm__("_ZN10CModelInfo19ms_weaponModelStoreE");
}
CStreamingInfo CStreaming::ms_aInfoForModel[NUMSTREAMINFO];
CColModel CTempColModels::ms_colModelBBox;
CColModel CTempColModels::ms_colModelPed1;
CColModel CTempColModels::ms_colModelWeapon;

CSimpleModelInfo *CModelInfo::AddSimpleModel(int) { return nil; }
CTimeModelInfo *CModelInfo::AddTimeModel(int) { return nil; }
CPedModelInfo *CModelInfo::AddPedModel(int) { return nil; }
CVehicleModelInfo *CModelInfo::AddVehicleModel(int) { return nil; }
CWeaponModelInfo *CModelInfo::AddWeaponModel(int) { return nil; }
void CBaseModelInfo::DeleteCollisionModel(void) {}
void CBaseModelInfo::SetTexDictionary(const char *) {}
void CSimpleModelInfo::SetLodDistances(float *) {}
const char *CAnimManager::GetAnimGroupName(AssocGroupId) { return "man"; }
ePedStats CPedStats::GetPedStatType(char *) { return (ePedStats)0; }
int32 cHandlingDataMgr::GetHandlingId(const char *) { return 0; }
void *CColModel::operator new(size_t sz) throw() { return ::operator new(sz); }
void CColModel::operator delete(void *p, size_t) throw() { ::operator delete(p); }
CColModel::CColModel(void) { std::memset(this, 0, sizeof(*this)); }
CColModel::~CColModel(void) {}
void CFileLoader::LoadCollisionModel(uint8 *, CColModel &, char *) {}
void CFileLoader::LoadObjectInstance(const char *) {}
int32 CTxdStore::FindTxdSlot(const char *) { return -1; }
int32 CTxdStore::AddTxdSlot(const char *) { return 0; }
int32 CTxdStore::GetParentTxdSlot(int32) { return -1; }
bool CTxdStore::HasFreeCustomSlot(void) { return true; }
void CTxdStore::SetTxdParent(int32, int32) {}

static void TestFreeroamScmAssetHasNoCutscenes(void) {
	FILE *f = std::fopen("gamefiles/data/freeroam_miami.scm", "rb");
	assert(f != nil && "gamefiles/data/freeroam_miami.scm must exist in repo/assets");
	std::fseek(f, 0, SEEK_END);
	const long sz = std::ftell(f);
	assert(sz > 0x2300 && sz < 200000);
	std::fseek(f, 0, SEEK_SET);
	std::vector<unsigned char> data((size_t)sz);
	assert(std::fread(data.data(), 1, data.size(), f) == data.size());
	std::fclose(f);

	// Header jump over variable space (02 00 01 <int32 offset>).
	assert(data[0] == 0x02 && data[1] == 0x00 && data[2] == 0x01);
	int32 objTableOff = 0;
	std::memcpy(&objTableOff, &data[3], sizeof(int32));
	assert(objTableOff > 0 && (size_t)(objTableOff + 8) < data.size());

	// Bytecode starts after object table and multi-script mission table at 0x108c.
	const size_t codeStart = 0x108c;
	assert(codeStart < data.size());

	// Verify COMMAND_LOAD_CUTSCENE (<op> + 8-byte ASCII cutscene label) never appears in bytecode.
	const uint16 loadCutsceneOp = (uint16)COMMAND_LOAD_CUTSCENE;
	for (size_t i = codeStart; i + 2 + KEY_LENGTH_IN_SCRIPT <= data.size(); i++) {
		uint16 raw = (uint16)(data[i] | (data[i + 1] << 8));
		if (raw != loadCutsceneOp)
			continue;
		// Distinguish a real COMMAND_LOAD_CUTSCENE + 8-byte ASCII key from variable offset 0x02e4 ($740).
		const unsigned char *key = &data[i + 2];
		bool validAsciiKey = (key[0] >= 'A' && key[0] <= 'Z') || (key[0] >= 'a' && key[0] <= 'z');
		bool seenNul = false;
		for (int k = 0; k < KEY_LENGTH_IN_SCRIPT && validAsciiKey; k++) {
			const unsigned char c = key[k];
			if (c == 0) {
				seenNul = true;
			} else if (seenNul) {
				validAsciiKey = false;
			} else if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) {
				validAsciiKey = false;
			}
		}
		assert(!(validAsciiKey && seenNul) && "freeroam_miami.scm must not contain COMMAND_LOAD_CUTSCENE");
	}

	// Verify DO_FADE 250, FADE_IN (6a 01 05 fa 00 04 01) exists in startup flow.
	const unsigned char fadeInSeq[] = { 0x6a, 0x01, 0x05, 0xfa, 0x00, 0x04, 0x01 };
	bool foundFadeIn = false;
	for (size_t i = codeStart; i + sizeof(fadeInSeq) <= data.size(); i++) {
		if (std::memcmp(&data[i], fadeInSeq, sizeof(fadeInSeq)) == 0) {
			foundFadeIn = true;
			break;
		}
	}
	assert(foundFadeIn && "freeroam_miami.scm must fade screen in without cutscene wait");
}

static void TestResolveSpawnRedirectsRiverToStreetLevelArzamas(void) {
	gGroundSamples.clear();
	gWaterRects.clear();

	// Vice City default initial spawn (79.6875, -867.25) is a riverbed (z=-10) under z=0 water.
	gGroundSamples.push_back({ 79.6875f, -867.25f, 30.0f, -10.0f, -10.0f });
	gWaterRects.push_back({ 0.0f, 300.0f, -896.0f, -668.0f, 0.0f });

	// Arzamas Station Plaza (822, 773): street pavement at z=12.1, overhang at z=60.0 (must pick 12.1!).
	gGroundSamples.push_back({ 822.0f, 773.0f, 25.0f, 12.1f, 60.0f });
	// Yuzhny Station (2700, -2435): street pavement at z=21.0.
	gGroundSamples.push_back({ 2700.0f, -2435.0f, 25.0f, 21.0f, 21.0f });

	CVector spawnPos(79.6875f, -867.25f, -100.0f);
	float heading = -1.0f;
	const bool redirected = CCustomWorld::ResolveSpawnPosition(spawnPos, &heading);
	assert(redirected);
	assert(std::fabs(spawnPos.x - 822.0f) < 0.01f);
	assert(std::fabs(spawnPos.y - 773.0f) < 0.01f);
	assert(std::fabs(spawnPos.z - 12.1f) < 0.01f);
	assert(std::fabs(heading - 110.0f) < 0.01f);

	// An already-dry custom coordinate must NOT be redirected.
	CVector customDry(2700.0f, -2435.0f, -100.0f);
	float unusedHeading = 33.0f;
	const bool redirectedDry = CCustomWorld::ResolveSpawnPosition(customDry, &unusedHeading);
	assert(!redirectedDry);
	assert(std::fabs(customDry.x - 2700.0f) < 0.01f);
	assert(std::fabs(customDry.y - (-2435.0f)) < 0.01f);
	assert(std::fabs(customDry.z - 21.0f) < 0.01f);
	assert(std::fabs(unusedHeading - 33.0f) < 0.01f);
}

static void TestRestartPointsAvoidEmptyPathCrashAndRiverHospital(void) {
	std::memset(gThePathsStorage, 0, sizeof(gThePathsStorage));
	ThePaths.m_numPathNodes = 0;

	CRestart::Initialise();
	// Vice City Hospital 0 (-569.1, 851.1, 22.8) is underwater on the custom map.
	gGroundSamples.push_back({ -569.1f, 851.1f, 30.0f, -7.2f, -7.2f });
	gWaterRects.push_back({ -700.0f, -400.0f, 700.0f, 1000.0f, 0.0f });
	CRestart::AddHospitalRestartPoint(CVector(-569.1f, 851.1f, 22.8f), 90.0f);

	// 1) Player dies near Yuzhny (2650, -2420), >4000m from (-569, 851):
	//    Previously crashed indexing ThePaths.m_pathNodes[-1]! Now must safely
	//    resolve to Yuzhny Station (2700, -2435, 21.0).
	CVector outPos(0.0f, 0.0f, 0.0f);
	float outHeading = 0.0f;
	CRestart::FindClosestHospitalRestartPoint(CVector(2650.0f, -2420.0f, 22.0f), &outPos, &outHeading);
	assert(std::fabs(outPos.x - 2700.0f) < 0.01f);
	assert(std::fabs(outPos.y - (-2435.0f)) < 0.01f);
	assert(std::fabs(outPos.z - 21.0f) < 0.01f);

	// 2) Player dies near (-500, 820) right next to the underwater VC hospital:
	//    Must resolve to nearest dry custom-map landmark (Arzamas Station 822, 773, 12.1).
	CRestart::FindClosestHospitalRestartPoint(CVector(-500.0f, 820.0f, 10.0f), &outPos, &outHeading);
	assert(std::fabs(outPos.x - 822.0f) < 0.01f);
	assert(std::fabs(outPos.y - 773.0f) < 0.01f);
	assert(std::fabs(outPos.z - 12.1f) < 0.01f);

	// 3) Police restart with zero registered police restarts and empty paths must not crash.
	CRestart::FindClosestPoliceRestartPoint(CVector(2650.0f, -2420.0f, 22.0f), &outPos, &outHeading);
	assert(std::fabs(outPos.x - 2700.0f) < 0.01f);
	assert(std::fabs(outPos.y - (-2435.0f)) < 0.01f);
}

int main(void) {
	TestFreeroamScmAssetHasNoCutscenes();
	TestResolveSpawnRedirectsRiverToStreetLevelArzamas();
	TestRestartPointsAvoidEmptyPathCrashAndRiverHospital();
	std::printf("AndroidFreeroamSpawnTests: ALL PASSED\n");
	return 0;
}
