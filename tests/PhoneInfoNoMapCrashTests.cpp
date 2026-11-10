// Regression test for CPhoneInfo when native Vice City map IPLs are skipped
// (e.g. CCustomModels::ReplacesNativeWorldMap() on Android):
// 1. BuildingPool contains zero MI_PHONEBOOTH1 buildings (only custom map buildings).
// 2. CPhoneInfo::GrabPhone() (opcode 024A from main.scm) must return -1 without
//    writing m_aPhones[-1] out-of-bounds or incrementing m_nScriptPhonesMax.
// 3. SetPhoneMessage_JustOnce/Repeatedly, HasMessageBeenDisplayed, IsMessageBeingDisplayed
//    must tolerate phoneId == -1 and out-of-range IDs.
// 4. CPhoneInfo::Update() must never dereference a null m_pEntity (previously crashed
//    at offsetof(CEntity, m_randomSeed) == 0x68 on frame 1 of CGame::Process).
// 5. CPhoneInfo::Load() must reject stale BuildingPool slots that are empty or hold
//    non-MI_PHONEBOOTH1 custom map buildings.

#include "common.h"

#include <cassert>
#include <cstdio>
#include <cstring>

#include "Phones.h"
#include "Pools.h"
#include "ModelIndices.h"
#include "Ped.h"
#include "PlayerPed.h"
#include "Pad.h"
#include "Messages.h"
#include "Camera.h"
#include "World.h"
#include "General.h"
#include "AudioScriptObject.h"
#include "RpAnimBlend.h"
#include "AnimBlendAssociation.h"
#include "SaveBuf.h"
#include "Replay.h"
#include "Timer.h"

// Minimal globals and stubs required to link src/control/Phones.cpp standalone.
int16 MI_PHONEBOOTH1 = 1331;
extern "C" {
alignas(CCamera) unsigned char gTheCameraStorage[sizeof(CCamera)] __asm__("TheCamera");
alignas(CPlayerInfo) unsigned char gPlayersStorage[sizeof(CPlayerInfo) * NUMPLAYERS] __asm__("_ZN6CWorld7PlayersE");
}
CBuildingPool *CPools::ms_pBuildingPool = nil;
uint32 CTimer::m_FrameCounter = 0;
uint32 CTimer::m_snTimeInMilliseconds = 1000;
uint32 CTimer::m_snPreviousTimeInMilliseconds = 984;
uint8 CWorld::PlayerInFocus = 0;
uint8 CReplay::Mode = 0;
int _saveBufCount = 0;

alignas(CPlayerPed) static unsigned char gDummyPlayerStorage[sizeof(CPlayerPed)];
static CPad gDummyPad;

void CPools::Initialise(void) {
	ms_pBuildingPool = new CBuildingPool(16, "Buildings");
}
void CPools::ShutDown(void) {
	delete ms_pBuildingPool;
	ms_pBuildingPool = nil;
}

void re3_assert(const char *expr, const char *filename, unsigned int lineno, const char *func) {
	std::fprintf(stderr, "ASSERT FAILED: %s at %s:%u (%s)\n", expr, filename, lineno, func);
	std::abort();
}
CPlayerPed *FindPlayerPed(void) {
	return reinterpret_cast<CPlayerPed *>(gDummyPlayerStorage);
}
CVehicle *FindPlayerVehicle(void) { return nil; }
RwUInt8 RwObjectGetType(const RwObject *) { return 0; }
void PlayOneShotScriptObject(uint8, const CVector &) {}
CAnimBlendAssociation *RpAnimBlendClumpGetAssociation(RpClump *, uint32) { return nil; }
int myrand(void) { return 42; }
void CPlaceable::SetHeading(float) {}
void CPlayerInfo::MakePlayerSafe(bool) {}
CAnimBlendAssociation *CAnimManager::BlendAnimation(RpClump *, AssocGroupId, AnimationId, float) { return nil; }
void CAnimBlendAssociation::SetFinishCallback(void (*)(CAnimBlendAssociation *, void *), void *) {}
CPad *CPad::GetPad(int) { return &gDummyPad; }
bool CPed::IsPedInControl(void) { return true; }
void CPed::ClearFollowPath(void) {}
void CCamera::SetWideScreenOn(void) {}
void CCamera::SetWideScreenOff(void) {}
void CEntity::UpdateRwFrame(void) {}
void CMatrix::UpdateRW(void) {}
CMatrix::~CMatrix(void) {}
void CMatrix::SetTranslate(float x, float y, float z) {
	rx = 1.0f; ry = 0.0f; rz = 0.0f;
	fx = 0.0f; fy = 1.0f; fz = 0.0f;
	ux = 0.0f; uy = 0.0f; uz = 1.0f;
	px = x;    py = y;    pz = z;
}
void CMessages::AddMessage(wchar *, uint32, uint16) {}

static CBuilding *AllocDummyBuilding(CBuildingPool &pool, int16 modelId, const CVector &pos) {
	CBuilding *b = pool.New();
	assert(b != nil);
	std::memset(b, 0, sizeof(CBuilding));
	b->GetMatrix().SetTranslate(pos.x, pos.y, pos.z);
	b->GetMatrix().GetUp() = CVector(0.0f, 0.0f, 1.0f);
	b->m_modelIndex = modelId;
	b->m_randomSeed = 7;
	return b;
}

int main() {
	std::memset(gDummyPlayerStorage, 0, sizeof(gDummyPlayerStorage));
	CPlayerPed *dummyPlayer = FindPlayerPed();
	dummyPlayer->GetMatrix().SetTranslate(0.0f, 0.0f, 12.0f);
	CWorld::Players[0].m_pPed = dummyPlayer;

	CPools::Initialise();
	CBuildingPool &pool = *CPools::GetBuildingPool();

	// Populate pool ONLY with custom map buildings (no MI_PHONEBOOTH1).
	AllocDummyBuilding(pool, 6812, CVector(10.0f, 20.0f, 5.0f));
	AllocDummyBuilding(pool, 6813, CVector(-240.0f, -1300.0f, 8.0f));

	// 1. New Game init + main.scm GrabPhone/SetPhoneMessage/Update sequence.
	gPhoneInfo.Initialise();
	assert(gPhoneInfo.m_nMax == 0);
	assert(gPhoneInfo.m_nScriptPhonesMax == 0);

	int grabbed0 = gPhoneInfo.GrabPhone(-240.0f, -1300.0f);
	int grabbed1 = gPhoneInfo.GrabPhone(15.0f, 25.0f);
	assert(grabbed0 == -1);
	assert(grabbed1 == -1);
	assert(gPhoneInfo.m_nMax == 0);
	assert(gPhoneInfo.m_nScriptPhonesMax == 0);

	wchar sampleMsg[8] = { 'T', 'E', 'S', 'T', 0 };
	gPhoneInfo.SetPhoneMessage_JustOnce(grabbed0, nil, nil, nil, nil, nil, nil);
	gPhoneInfo.SetPhoneMessage_JustOnce(grabbed0, sampleMsg, nil, nil, nil, nil, nil);
	gPhoneInfo.SetPhoneMessage_Repeatedly(grabbed0, sampleMsg, nil, nil, nil, nil, nil);
	gPhoneInfo.SetPhoneMessage_JustOnce(NUMPHONES + 5, sampleMsg, nil, nil, nil, nil, nil);
	assert(!gPhoneInfo.HasMessageBeenDisplayed(grabbed0));
	assert(!gPhoneInfo.IsMessageBeingDisplayed(grabbed0));
	assert(!gPhoneInfo.HasMessageBeenDisplayed(NUMPHONES + 5));
	assert(!gPhoneInfo.IsMessageBeingDisplayed(NUMPHONES + 5));

	// Even if a corrupted state sets m_nScriptPhonesMax > 0 with null m_pEntity,
	// Update() across 32 frames must never dereference nullptr + 0x68.
	gPhoneInfo.m_nScriptPhonesMax = 2;
	gPhoneInfo.m_aPhones[0].m_pEntity = nil;
	gPhoneInfo.m_aPhones[0].m_visibleToCam = false;
	gPhoneInfo.m_aPhones[1].m_pEntity = nil;
	gPhoneInfo.m_aPhones[1].m_visibleToCam = true;
	gPhoneInfo.m_aPhones[1].m_nState = PHONE_STATE_ONETIME_MESSAGE_SET;
	for (uint32 f = 0; f < 32; ++f) {
		CTimer::SetFrameCounter(f);
		gPhoneInfo.Update();
	}
	assert(!gPhoneInfo.m_aPhones[0].m_visibleToCam);
	assert(!gPhoneInfo.m_aPhones[1].m_visibleToCam);

	// 2. Verify normal behaviour when an actual MI_PHONEBOOTH1 exists in BuildingPool.
	CBuilding *booth = AllocDummyBuilding(pool, MI_PHONEBOOTH1, CVector(50.0f, 60.0f, 10.0f));
	gPhoneInfo.Initialise();
	assert(gPhoneInfo.m_nMax == 1);
	assert(gPhoneInfo.m_nScriptPhonesMax == 0);
	assert(gPhoneInfo.GrabPhone(5000.0f, 5000.0f) == -1); // out of 100m range -> -1, no OOB!
	assert(gPhoneInfo.m_nScriptPhonesMax == 0);
	int validId = gPhoneInfo.GrabPhone(52.0f, 61.0f);
	assert(validId == 0);
	assert(gPhoneInfo.m_nScriptPhonesMax == 1);
	assert(gPhoneInfo.GrabPhone(52.0f, 61.0f) == -1); // already grabbed -> -1

	// 3. Save buffer roundtrip when booth's slot is subsequently occupied by a custom map building.
	uint8 saveBuf[4096];
	std::memset(saveBuf, 0, sizeof(saveBuf));
	uint32 savedSize = 0;
	gPhoneInfo.Save(saveBuf, &savedSize);
	assert(savedSize > 0 && savedSize <= sizeof(saveBuf));

	// Replace booth's model with a custom road/building (6814 != MI_PHONEBOOTH1).
	booth->m_modelIndex = 6814;
	booth->GetMatrix().GetUp().z = 1.0f;

	gPhoneInfo.Load(saveBuf, savedSize);
	assert(gPhoneInfo.m_aPhones[0].m_pEntity == nil);
	assert(!gPhoneInfo.m_aPhones[0].m_visibleToCam);
	for (uint32 f = 0; f < 32; ++f) {
		CTimer::SetFrameCounter(f);
		gPhoneInfo.Update();
	}
	assert(booth->GetMatrix().GetUp().z == 1.0f);

	CPools::ShutDown();
	std::printf("PhoneInfoNoMapCrashTests: ALL PASSED\n");
	return 0;
}
