#include "common.h"
#include "crossplatform.h"
#include "platform.h"
#include "MobileTouchControls.h"
#include "MobileTouchMath.h"
#include "MobileSpawnerMath.h"

#include "Automobile.h"
#include "Bike.h"
#include "Boat.h"
#include "Camera.h"
#include "Clock.h"
#include "CustomModels.h"
#include "CutsceneMgr.h"
#include "DMAudio.h"
#include "DebugLog.h"
#include "Font.h"
#include "Frontend.h"
#include "General.h"
#include "Hud.h"
#include "ModelIndices.h"
#include "ModelInfo.h"
#include "NoClip.h"
#include "Pad.h"
#include "PedModelInfo.h"
#include "Pickups.h"
#include "PlayerPed.h"
#include "Pools.h"
#include "Radar.h"
#include "Renderer.h"
#include "Replay.h"
#include "RwHelper.h"
#include "Shadows.h"
#include "Sprite2d.h"
#include "Streaming.h"
#include "Timer.h"
#include "TxdStore.h"
#include "Vehicle.h"
#include "VehicleModelInfo.h"
#include "WeaponInfo.h"
#include "WeaponType.h"
#include "World.h"
#include "main.h"

#ifdef ANDROID
#include <jni.h>
#endif

#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

enum MobileTexId {
	MTEX_CIRCLE = 0,
	MTEX_NUB,
	MTEX_PUNCH,
	MTEX_SHOOT,
	MTEX_THROW,
	MTEX_BOMB,
	MTEX_CAMERA_WPN,
	MTEX_RUN,
	MTEX_JUMP,
	MTEX_LOCKON,
	MTEX_SWAPGUN,
	MTEX_ZOOM_IN,
	MTEX_ZOOM_OUT,
	MTEX_CAR,
	MTEX_BIKE,
	MTEX_BOAT,
	MTEX_CHOPPER,
	MTEX_PLANE,
	MTEX_ENTER_EXIT,
	MTEX_EXIT,
	MTEX_EXIT_CAR,
	MTEX_EXIT_BIKE,
	MTEX_EXIT_HELI,
	MTEX_EXIT_PLANE,
	MTEX_LEFT,
	MTEX_RIGHT,
	MTEX_TANK_LEFT,
	MTEX_TANK_RIGHT,
	MTEX_ACCELERATE,
	MTEX_BRAKE,
	MTEX_HANDBRAKE,
	MTEX_HORN,
	MTEX_ROCKETS,
	MTEX_HELI_DROP,
	MTEX_DRIVE_TOGGLE,
	MTEX_CAM_TOGGLE,
	MTEX_PAUSE,
	MTEX_BUY,
	MTEX_CELLPHONE,
	MTEX_SUBMISSION,
	MTEX_COUNT
};

enum MobileBtnId {
	MBTN_NONE = -1,
	MBTN_PAUSE = 0,
	MBTN_CAM,
	MBTN_WEAPON_SLOT,
	MBTN_RADIO_SLOT,
	MBTN_DRIVE_MODE,
	MBTN_ATTACK,
	MBTN_SPRINT,
	MBTN_JUMP,
	MBTN_TARGET,
	MBTN_ZOOM_IN,
	MBTN_ZOOM_OUT,
	MBTN_ENTER_EXIT,
	MBTN_INTERACT,
	MBTN_STEER_LEFT,
	MBTN_STEER_RIGHT,
	MBTN_ACCEL,
	MBTN_BRAKE,
	MBTN_HANDBRAKE,
	MBTN_HORN,
	MBTN_CAR_GUN,
	MBTN_SUBMISSION,
	MBTN_YAW_LEFT,
	MBTN_YAW_RIGHT,
	MBTN_DEV_SKIN,
	MBTN_DEV_VEHICLE,
	MBTN_DEV_TOGGLE,
	MBTN_DEV_FLY,
	MBTN_DEV_TIME_PLUS,
	MBTN_DEV_TIME_MINUS,
	MBTN_COUNT
};

enum FallbackGlyph {
	GLYPH_NONE = 0,
	GLYPH_PAUSE,
	GLYPH_CAMERA,
	GLYPH_FIST,
	GLYPH_CROSSHAIR,
	GLYPH_GRENADE,
	GLYPH_SPRINT,
	GLYPH_JUMP,
	GLYPH_TARGET,
	GLYPH_PLUS,
	GLYPH_MINUS,
	GLYPH_CAR,
	GLYPH_EXIT,
	GLYPH_ARROW_LEFT,
	GLYPH_ARROW_RIGHT,
	GLYPH_PEDAL_GAS,
	GLYPH_PEDAL_BRAKE,
	GLYPH_HANDBRAKE,
	GLYPH_HORN,
	GLYPH_STEER_MODE,
	GLYPH_DOTS,
	GLYPH_FLY,
	GLYPH_LEGEND,
	GLYPH_PERSON,
	GLYPH_CLOSE
};

struct VirtualButton {
	bool visible;
	bool invisibleHitZone;
	bool circular;
	bool preserveAspect;
	float cx, cy;
	float halfW, halfH;
	MobileTexId texId;
	FallbackGlyph glyph;
};

struct ActivePointer {
	bool active;
	int64 fingerId;
	float x, y;
	float startX, startY;
	bool isStick;
	bool isLook;
	MobileBtnId btn;
	bool swipedWeapon;
	bool swipedRadio;
};

enum MenuOverlayBtn {
	MOBTN_NONE = -1,
	MOBTN_BACK = 0,
	MOBTN_MAP_ZOOM_IN,
	MOBTN_MAP_ZOOM_OUT,
	MOBTN_MAP_LEGEND,
	MOBTN_COUNT
};

struct MenuPointer {
	bool active;
	int64 fingerId;
	int32 screenAtDown;
	uint32 downTimeMs;
	float x, y;
	float prevX, prevY;
	float startX, startY;
	float maxTravel;
	bool draggingScroll;
	bool draggingSlider;
	bool swipedHorizontal;
	bool pinched;
	bool longPressedMap;
	int32 downRow;
	MenuOverlayBtn overlayBtn;
	float scrollResidualPx;
};

static const int MAX_TOUCH_POINTERS = 10;

// Device Display & DP state
static float s_deviceDensity = 0.0f;
static float s_deviceXdpi = 0.0f;
static float s_deviceYdpi = 0.0f;
static float s_fallbackDpi = 0.0f;
static int32 s_safeLeft = 0;
static int32 s_safeTop = 0;
static int32 s_safeRight = 0;
static int32 s_safeBottom = 0;

static int32 s_mobileTxdSlot = -1;
static bool s_hasCustomTxd = false;
static RwTexture *s_tex[MTEX_COUNT];
static RwTexture *s_weaponIcons[WEAPONTYPE_TOTALWEAPONS];

static VirtualButton s_buttons[MBTN_COUNT];
static ActivePointer s_pointers[MAX_TOUCH_POINTERS];

static bool s_stickVisible = false;
static float s_stickBaseX = 0.0f;
static float s_stickBaseY = 0.0f;
static float s_stickAnchorX = 0.0f;
static float s_stickAnchorY = 0.0f;
static float s_stickKnobX = 0.0f;
static float s_stickKnobY = 0.0f;
static float s_stickRadius = 64.0f;
static int16 s_stickAxisX = 0;
static int16 s_stickAxisY = 0;
static uint32 s_lastStickTapMs = 0;

static float s_accumLookDX = 0.0f;
static float s_accumLookDY = 0.0f;

static bool s_vehicleStickMode = false;
static bool s_scopeLatched = false;
static int32 s_scopeLatchedWeapon = -1;
static bool s_devDrawerOpen = false;

// Gameplay one-shot pulses
static bool s_pulsePause = false;
static bool s_pulseCam = false;
static bool s_pulseWeaponNext = false;
static bool s_pulseWeaponPrev = false;
static bool s_pulseRadio = false;
static bool s_pulseInteract = false;
static bool s_pulseCrouch = false;
static bool s_pulseSubmission = false;
static bool s_pulseSkipCutscene = false;
static bool s_pulseFlyToggle = false;
static MobileTouchMath::SprintJumpDoubleTap s_sprintJumpTap;

// Menu touch & gesture state
static MenuPointer s_menuPointers[MAX_TOUCH_POINTERS];
static VirtualButton s_menuOverlayBtns[MOBTN_COUNT];
static bool s_menuPendingSelect = false;
static bool s_menuPendingBack = false;
static int8 s_menuPendingChange = 0;
static bool s_menuDirtySettings = false;
static float s_statsScrollDeltaPx = 0.0f;
static float s_statsVelocityPxPerSec = 0.0f;
static uint32 s_lastMapTapMs = 0;
static float s_lastMapTapX = 0.0f;
static float s_lastMapTapY = 0.0f;

static MobileTouchMath::MobileScreenMetrics
GetMetrics(void)
{
	const float dpi = (s_deviceXdpi > 40.0f) ? s_deviceXdpi : s_fallbackDpi;
	return MobileTouchMath::ComputeScreenMetrics(
		(int)SCREEN_WIDTH, (int)SCREEN_HEIGHT,
		s_deviceDensity, dpi,
		s_safeLeft, s_safeTop, s_safeRight, s_safeBottom);
}

// ---------------------------------------------------------------------------
// Mobile Skin Switcher & Vehicle Spawner (Custom + Stock Catalog & Actions)
// ---------------------------------------------------------------------------

static mobilespawner::PanelMode s_spawnerMode = mobilespawner::PANEL_NONE;
static std::vector<mobilespawner::CatalogItem> s_skinCatalog;
static std::vector<mobilespawner::CatalogItem> s_vehCatalog;
static int32 s_skinSelectedIdx = 0;
static int32 s_skinFirstRow = 0;
static int32 s_vehSelectedIdx = 0;
static int32 s_vehFirstRow = 0;
static int32 s_lastSpawnedVehRef = -1;
static char s_spawnerToast[64] = { 0 };
static uint32 s_spawnerToastUntilMs = 0;

struct SpawnerPointerState {
	bool active;
	int64 fingerId;
	float startX, startY;
	float prevY;
	float maxTravel;
	float scrollResidualPx;
	bool draggingScroll;
	mobilespawner::PanelHit downHit;
};
static SpawnerPointerState s_spawnerPtr = { false, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, false, mobilespawner::PHIT_NONE };

static void
SetSpawnerToast(const char *msg)
{
	if (!msg)
		return;
	std::snprintf(s_spawnerToast, sizeof(s_spawnerToast), "%s", msg);
	s_spawnerToastUntilMs = CTimer::GetTimeInMilliseconds() + 2600;
}

static bool
IsStreamableModelRegistered(int32 id, const char *modelName)
{
	if (id < 0 || id >= MODELINFOSIZE)
		return false;
	if (CStreaming::HasModelLoaded(id) || CStreaming::ms_aInfoForModel[id].m_size != 0)
		return true;
#ifdef CUSTOM_MODELS
	if (modelName && *modelName && CCustomModels::HasModelFile(modelName))
		return true;
#else
	(void)modelName;
#endif
	return false;
}

static void
RebuildSkinCatalog(void)
{
	s_skinCatalog.clear();
	for (int32 id = 0; id < MODELINFOSIZE; id++) {
		CBaseModelInfo *base = CModelInfo::GetModelInfo(id);
		if (!base || base->GetModelType() != MITYPE_PED)
			continue;
		CPedModelInfo *pmi = (CPedModelInfo*)base;
		if (pmi->m_animGroup >= NUM_ANIM_ASSOC_GROUPS)
			continue;
		const char *name = pmi->GetModelName();
		if (!name || !*name)
			continue;
		if (!IsStreamableModelRegistered(id, name))
			continue;

		mobilespawner::CatalogItem item;
		item.modelId = id;
		item.isCustom = (id >= 4900);
		item.isSecondaryTag = pmi->UsesSAAnimations();
		std::snprintf(item.name, sizeof(item.name), "%s", name);
		s_skinCatalog.push_back(item);
	}
	mobilespawner::SortCatalogItems(s_skinCatalog, true);

	CPlayerPed *ped = FindPlayerPed();
	if (ped && !s_skinCatalog.empty()) {
		const int32 curId = ped->GetModelIndex();
		for (size_t i = 0; i < s_skinCatalog.size(); i++) {
			if (s_skinCatalog[i].modelId == curId) {
				s_skinSelectedIdx = (int32)i;
				break;
			}
		}
		s_skinSelectedIdx = Clamp(s_skinSelectedIdx, 0, (int32)s_skinCatalog.size() - 1);
		if (s_skinFirstRow > s_skinSelectedIdx)
			s_skinFirstRow = s_skinSelectedIdx;
		if (s_skinFirstRow + 6 <= s_skinSelectedIdx)
			s_skinFirstRow = Max(0, s_skinSelectedIdx - 5);
	}
}

static void
RebuildVehicleCatalog(void)
{
	s_vehCatalog.clear();
	for (int32 id = 0; id < MODELINFOSIZE; id++) {
		if (mobilespawner::IsExcludedStockVehicleId(id))
			continue;
		CBaseModelInfo *base = CModelInfo::GetModelInfo(id);
		if (!base || base->GetModelType() != MITYPE_VEHICLE)
			continue;
		CVehicleModelInfo *vmi = (CVehicleModelInfo*)base;
		if (vmi->m_vehicleType != VEHICLE_TYPE_CAR &&
		    vmi->m_vehicleType != VEHICLE_TYPE_BIKE &&
		    vmi->m_vehicleType != VEHICLE_TYPE_BOAT)
			continue;
		const char *name = vmi->GetModelName();
		if (!name || !*name)
			continue;
		if (!IsStreamableModelRegistered(id, name))
			continue;

		mobilespawner::CatalogItem item;
		item.modelId = id;
		item.isCustom = (id >= 4900);
		item.isSecondaryTag = (vmi->m_vehicleType == VEHICLE_TYPE_BIKE);
		std::snprintf(item.name, sizeof(item.name), "%s", name);
		s_vehCatalog.push_back(item);
	}
	mobilespawner::SortCatalogItems(s_vehCatalog, false);
	if (!s_vehCatalog.empty())
		s_vehSelectedIdx = Clamp(s_vehSelectedIdx, 0, (int32)s_vehCatalog.size() - 1);
}

static bool
ApplyPlayerSkinModel(int32 modelId)
{
	CPlayerPed *ped = FindPlayerPed();
	if (!ped || !ped->m_rwObject)
		return false;
	if (ped->InVehicle() || FindPlayerVehicle() != nil) {
		SetSpawnerToast("EXIT VEHICLE TO CHANGE SKIN");
		return false;
	}
	CBaseModelInfo *base = CModelInfo::GetModelInfo(modelId);
	if (!base || base->GetModelType() != MITYPE_PED)
		return false;
	CPedModelInfo *info = (CPedModelInfo*)base;

	const uint8 previousFlags = CStreaming::ms_aInfoForModel[modelId].m_flags;
	{
		CCustomModels::ScopedBlockingLoad blockingScope;
		CCustomModels::ScopedImmediateTextures immTexScope;
		CStreaming::RequestModel(modelId, STREAMFLAGS_DEPENDENCY | STREAMFLAGS_DONT_REMOVE);
		CStreaming::LoadAllRequestedModels(false);
		CCustomModels::UpgradeModelTexturesImmediate(modelId);
	}
	if (!CStreaming::HasModelLoaded(modelId) || !info->GetRwObject()) {
		if (!(previousFlags & STREAMFLAGS_DONT_REMOVE))
			CStreaming::SetModelIsDeletable(modelId);
		char errBuf[64];
		std::snprintf(errBuf, sizeof(errBuf), "LOAD FAILED: %s (%d)", info->GetModelName(), (int)modelId);
		SetSpawnerToast(errBuf);
		return false;
	}

	const int32 oldId = ped->GetModelIndex();
	ped->ClearAttack();
	ped->ClearWeaponTarget();
	CWorld::Remove(ped);
	ped->RemoveWeaponModel(-1);
#ifdef USE_CUTSCENE_SHADOW_FOR_PED
	if (ped->m_pRTShadow) {
		delete ped->m_pRTShadow;
		ped->m_pRTShadow = nil;
	}
#endif
	ped->DeleteRwObject();
	// CPed::SetModelIndex binds VC skeletons strictly to native VC anim groups
	// and SA skeletons strictly to custom SA anim groups (usesSAAnimations).
	ped->SetModelIndex(modelId);
	CWorld::Add(ped);

	const int32 weaponId = CWeaponInfo::GetWeaponInfo(ped->GetWeapon()->m_eWeaponType)->m_nModelId;
	if (weaponId >= 0 && CStreaming::HasModelLoaded(weaponId))
		ped->AddWeaponModel(weaponId);
	if (!(previousFlags & STREAMFLAGS_DONT_REMOVE))
		CStreaming::SetModelIsDeletable(modelId);

	// Update catalog SA/VC tag now that clump skeleton has been inspected
	for (size_t i = 0; i < s_skinCatalog.size(); i++) {
		if (s_skinCatalog[i].modelId == modelId) {
			s_skinCatalog[i].isSecondaryTag = info->UsesSAAnimations();
			break;
		}
	}

	char msg[64];
	std::snprintf(msg, sizeof(msg), "SKIN %d: %s [%s]",
		(int)modelId, info->GetModelName(), info->UsesSAAnimations() ? "SA" : "VC");
	SetSpawnerToast(msg);
	DMAudio.PlayFrontEndSound(SOUND_FRONTEND_ENTER_OR_ADJUST, 0);
	REVC_LOG_INFO("Mobile skin switched: %d -> %d (%s, %s animations)",
		(int)oldId, (int)modelId, info->GetModelName(), info->UsesSAAnimations() ? "SA" : "VC");
	return true;
}

static bool
SpawnVehicleByModel(int32 modelId)
{
	CPlayerPed *ped = FindPlayerPed();
	if (!ped)
		return false;
	CBaseModelInfo *base = CModelInfo::GetModelInfo(modelId);
	if (!base || base->GetModelType() != MITYPE_VEHICLE)
		return false;
	CVehicleModelInfo *vmi = (CVehicleModelInfo*)base;

	const uint8 previousFlags = CStreaming::ms_aInfoForModel[modelId].m_flags;
	{
		CCustomModels::ScopedBlockingLoad blockingScope;
		CCustomModels::ScopedImmediateTextures immTexScope;
		CStreaming::RequestModel(modelId, STREAMFLAGS_DEPENDENCY | STREAMFLAGS_DONT_REMOVE);
		CStreaming::LoadAllRequestedModels(false);
		CCustomModels::UpgradeModelTexturesImmediate(modelId);
	}
	if (!CStreaming::HasModelLoaded(modelId) || !vmi->GetRwObject()) {
		if (!(previousFlags & STREAMFLAGS_DONT_REMOVE))
			CStreaming::SetModelIsDeletable(modelId);
		char errBuf[64];
		std::snprintf(errBuf, sizeof(errBuf), "VEH LOAD FAILED: %s (%d)", vmi->GetModelName(), (int)modelId);
		SetSpawnerToast(errBuf);
		return false;
	}

	// Clean up previous test-spawned vehicle if empty so repeated testing never fills pool
	if (s_lastSpawnedVehRef >= 0) {
		CVehicle *prevVeh = CPools::GetVehicle(s_lastSpawnedVehRef);
		if (prevVeh && prevVeh != FindPlayerVehicle() &&
		    prevVeh->pDriver == nil && prevVeh->m_nNumPassengers == 0) {
			CWorld::Remove(prevVeh);
			delete prevVeh;
		}
		s_lastSpawnedVehRef = -1;
	}

	CVehicle *curPlayerVeh = FindPlayerVehicle();
	const CVector basePos = curPlayerVeh ? curPlayerVeh->GetPosition() : ped->GetPosition();
	CVector fwd = curPlayerVeh ? curPlayerVeh->GetForward() : ped->GetForward();
	if (fwd.x * fwd.x + fwd.y * fwd.y < 1.0e-4f)
		fwd = TheCamera.GetForward();

	const float distAhead = (vmi->m_vehicleType == VEHICLE_TYPE_BOAT) ? 8.0f : 5.5f;
	float sx = basePos.x, sy = basePos.y, sz = basePos.z;
	float rx = 1.0f, ry = 0.0f;
	mobilespawner::ComputeVehicleSpawnTransform(
		basePos.x, basePos.y, basePos.z, fwd.x, fwd.y, distAhead,
		&sx, &sy, &sz, &rx, &ry);

	CVehicle *spawned = nil;
	if (vmi->m_vehicleType == VEHICLE_TYPE_BOAT) {
		CBoat *boat = new CBoat(modelId, RANDOM_VEHICLE);
		spawned = boat;
	} else if (vmi->m_vehicleType == VEHICLE_TYPE_BIKE) {
		CBike *bike = new CBike(modelId, RANDOM_VEHICLE);
		if (bike)
			bike->bIsStanding = true;
		spawned = bike;
	} else {
		CAutomobile *car = new CAutomobile(modelId, RANDOM_VEHICLE);
		spawned = car;
	}

	if (!spawned) {
		if (!(previousFlags & STREAMFLAGS_DONT_REMOVE))
			CStreaming::SetModelIsDeletable(modelId);
		SetSpawnerToast("VEHICLE POOL FULL");
		return false;
	}

	bool groundFound = false;
	float groundZ = CWorld::FindGroundZFor3DCoord(sx, sy, basePos.z + 5.0f, &groundFound);
	if (!groundFound) {
		CColPoint cp;
		CEntity *hitEnt = nil;
		if (CWorld::ProcessVerticalLine(CVector(sx, sy, basePos.z + 30.0f), basePos.z - 40.0f,
		                                cp, hitEnt, true, false, false, false, false, false, nil)) {
			groundZ = cp.point.z;
			groundFound = true;
		}
	}
	CVector finalPos(sx, sy, (groundFound ? groundZ : basePos.z) + spawned->GetDistanceFromCentreOfMassToBaseOfModel() + 0.15f);
	spawned->bIsStatic = false;
	spawned->bEngineOn = false;
	spawned->bLightsOn = false;
	spawned->SetPosition(finalPos);
	const float headingRad = std::atan2(-fwd.x, fwd.y);
	spawned->SetOrientation(0.0f, 0.0f, headingRad);
	spawned->SetStatus(STATUS_ABANDONED);
	spawned->m_nDoorLock = CARLOCK_UNLOCKED;
	CWorld::Add(spawned);

	s_lastSpawnedVehRef = CPools::GetVehicleRef(spawned);
	if (!(previousFlags & STREAMFLAGS_DONT_REMOVE)) {
		CStreaming::SetModelIsDeletable(modelId);
		CStreaming::SetModelTxdIsDeletable(modelId);
	}

	char msg[64];
	std::snprintf(msg, sizeof(msg), "SPAWNED %d: %s", (int)modelId, vmi->GetModelName());
	SetSpawnerToast(msg);
	DMAudio.PlayFrontEndSound(SOUND_FRONTEND_ENTER_OR_ADJUST, 0);
	REVC_LOG_INFO("Mobile vehicle spawned: %d (%s) at (%.1f, %.1f, %.1f)",
		(int)modelId, vmi->GetModelName(), finalPos.x, finalPos.y, finalPos.z);
	return true;
}

static mobilespawner::PickerLayout
GetCurrentSpawnerLayout(const MobileTouchMath::MobileScreenMetrics &m)
{
	const float rDev = m.Dp(18.0f);
	const float topIconY = m.marginTopPx + m.Dp(22.0f);
	const float devX = Max(m.radarLeftPx + m.radarSizePx + m.Dp(24.0f) + rDev, m.screenW * 0.27f);
	const float anchorX = (s_spawnerMode == mobilespawner::PANEL_VEHICLE) ? (devX + m.Dp(44.0f)) : devX;
	return mobilespawner::BuildPickerLayout(m.screenW, m.screenH, anchorX, topIconY + rDev, m.Dp(1.0f));
}

static void
StepAndTriggerSpawner(int32 delta, bool triggerAction)
{
	std::vector<mobilespawner::CatalogItem> &cat =
		(s_spawnerMode == mobilespawner::PANEL_SKIN) ? s_skinCatalog : s_vehCatalog;
	if (cat.empty())
		return;
	int32 &sel = (s_spawnerMode == mobilespawner::PANEL_SKIN) ? s_skinSelectedIdx : s_vehSelectedIdx;
	int32 &firstRow = (s_spawnerMode == mobilespawner::PANEL_SKIN) ? s_skinFirstRow : s_vehFirstRow;

	sel = mobilespawner::WrapIndex(sel + delta, (int)cat.size());
	if (sel < firstRow)
		firstRow = sel;
	else if (sel >= firstRow + 6)
		firstRow = Max(0, sel - 5);
	firstRow = Clamp(firstRow, 0, Max(0, (int32)cat.size() - 6));

	if (triggerAction) {
		if (s_spawnerMode == mobilespawner::PANEL_SKIN)
			ApplyPlayerSkinModel(cat[sel].modelId);
		else
			SpawnVehicleByModel(cat[sel].modelId);
	} else {
		DMAudio.PlayFrontEndSound(SOUND_FRONTEND_HIGHLIGHT_OPTION, 0);
	}
}

// ---------------------------------------------------------------------------
// Texture Lookup & Optional custom/hud/hud.txd Loader
// ---------------------------------------------------------------------------

struct TexLookupCtx {
	const char *target;
	RwTexture *found;
};

static RwTexture*
FindTexIgnoreCaseCB(RwTexture *tex, void *data)
{
	TexLookupCtx *ctx = (TexLookupCtx*)data;
	if (!tex || !ctx->target)
		return tex;
	if (MobileTouchMath::MatchesHudTextureName(tex->name, ctx->target)) {
		ctx->found = tex;
		return nil;
	}
	return tex;
}

static RwTexture*
CountTexturesCB(RwTexture *tex, void *data)
{
	if (tex && RwTextureGetRaster(tex))
		(*(int32*)data)++;
	return tex;
}

static RwTexture*
FindInDict(RwTexDictionary *dict, const char *name)
{
	if (!dict || !name || !*name)
		return nil;

	RwTexture *exact = RwTexDictionaryFindNamedTexture(dict, name);
	if (exact && RwTextureGetRaster(exact))
		return exact;

	TexLookupCtx ctx = { name, nil };
	RwTexDictionaryForAllTextures(dict, FindTexIgnoreCaseCB, &ctx);
	return (ctx.found && RwTextureGetRaster(ctx.found)) ? ctx.found : nil;
}

static RwTexture*
FindMobileTex(RwTexDictionary *dict, const char *n1, const char *n2 = nil, const char *n3 = nil)
{
	RwTexture *t = FindInDict(dict, n1);
	if (!t && n2) t = FindInDict(dict, n2);
	if (!t && n3) t = FindInDict(dict, n3);
	return t;
}

static void
ReplaceSpriteTex(CSprite2d &sprite, RwTexture *tex)
{
	if (!tex || !RwTextureGetRaster(tex))
		return;
	RwTextureAddRef(tex);
	sprite.Delete();
	sprite.m_pTexture = tex;
}

static RwTexDictionary*
GetMobileDict(void)
{
	if (s_mobileTxdSlot < 0)
		return nil;
	TxdDef *def = CTxdStore::GetSlot(s_mobileTxdSlot);
	return def ? def->texDict : nil;
}

static void
ResolveMobileTextures(void)
{
	memset(s_tex, 0, sizeof(s_tex));
	memset(s_weaponIcons, 0, sizeof(s_weaponIcons));

	RwTexDictionary *dict = GetMobileDict();
	if (!dict)
		return;

	s_tex[MTEX_CIRCLE]       = FindMobileTex(dict, "hud_circle", "circle");
	s_tex[MTEX_NUB]          = FindMobileTex(dict, "hud_analognub", "hud_Nipple", "analognub");
	s_tex[MTEX_PUNCH]        = FindMobileTex(dict, "hud_punch", "hud_fist");
	s_tex[MTEX_SHOOT]        = FindMobileTex(dict, "hud_shoot", "hud_target");
	s_tex[MTEX_THROW]        = FindMobileTex(dict, "hud_throw", "hud_grenade");
	s_tex[MTEX_BOMB]         = FindMobileTex(dict, "hud_bomb", "hud_detonator", "hud_shoot");
	s_tex[MTEX_CAMERA_WPN]   = FindMobileTex(dict, "hud_camera", "hud_shoot");
	s_tex[MTEX_RUN]          = FindMobileTex(dict, "hud_run", "run");
	s_tex[MTEX_JUMP]         = FindMobileTex(dict, "hud_jump", "jump");
	s_tex[MTEX_LOCKON]       = FindMobileTex(dict, "hud_lockon", "hud_target");
	s_tex[MTEX_SWAPGUN]      = FindMobileTex(dict, "hud_swapgun", "swapgun");
	s_tex[MTEX_ZOOM_IN]      = FindMobileTex(dict, "hud_zoom+", "hud_tut_zoomin");
	s_tex[MTEX_ZOOM_OUT]     = FindMobileTex(dict, "hud_zoom-", "hud_tut_zoomout");

	s_tex[MTEX_CAR]          = FindMobileTex(dict, "hud_car", "hud_enterandexit");
	s_tex[MTEX_BIKE]         = FindMobileTex(dict, "hud_bike", "hud_car", "hud_enterandexit");
	s_tex[MTEX_BOAT]         = FindMobileTex(dict, "hud_boat", "hud_car", "hud_enterandexit");
	s_tex[MTEX_CHOPPER]      = FindMobileTex(dict, "hud_chopper", "hud_car", "hud_enterandexit");
	s_tex[MTEX_PLANE]        = FindMobileTex(dict, "hud_plane", "hud_chopper", "hud_enterandexit");
	s_tex[MTEX_ENTER_EXIT]   = FindMobileTex(dict, "hud_enterandexit", "hud_car");

	s_tex[MTEX_EXIT]         = FindMobileTex(dict, "hud_exit", "hud_exitcar", "hud_enterandexit");
	s_tex[MTEX_EXIT_CAR]     = FindMobileTex(dict, "hud_exitcar", "hud_exit", "hud_enterandexit");
	s_tex[MTEX_EXIT_BIKE]    = FindMobileTex(dict, "hud_exitbike", "hud_exit", "hud_exitcar");
	s_tex[MTEX_EXIT_HELI]    = FindMobileTex(dict, "hud_exithelicopter", "hud_exit", "hud_exitcar");
	s_tex[MTEX_EXIT_PLANE]   = FindMobileTex(dict, "hud_exitplane", "hud_exithelicopter", "hud_exit");

	s_tex[MTEX_LEFT]         = FindMobileTex(dict, "hud_left", "left");
	s_tex[MTEX_RIGHT]        = FindMobileTex(dict, "hud_right", "right");
	s_tex[MTEX_TANK_LEFT]    = FindMobileTex(dict, "hud_tank_left", "hud_left");
	s_tex[MTEX_TANK_RIGHT]   = FindMobileTex(dict, "hud_tank_right", "hud_right");
	s_tex[MTEX_ACCELERATE]   = FindMobileTex(dict, "hud_accelerate", "accelerate");
	s_tex[MTEX_BRAKE]        = FindMobileTex(dict, "hud_break", "hud_brake", "brake");
	s_tex[MTEX_HANDBRAKE]    = FindMobileTex(dict, "hud_handbrake", "handbrake");
	s_tex[MTEX_HORN]         = FindMobileTex(dict, "hud_horn", "horn");
	s_tex[MTEX_ROCKETS]      = FindMobileTex(dict, "hud_rockets", "hud_shoot");
	s_tex[MTEX_HELI_DROP]    = FindMobileTex(dict, "hud_heliDrop", "hud_shoot");
	s_tex[MTEX_DRIVE_TOGGLE] = FindMobileTex(dict, "hud_drive-toggle", "drive-toggle");
	s_tex[MTEX_CAM_TOGGLE]   = FindMobileTex(dict, "hud_cam-toggle", "hud_camera");
	s_tex[MTEX_PAUSE]        = FindMobileTex(dict, "hud_pause", "pause");
	s_tex[MTEX_BUY]          = FindMobileTex(dict, "hud_buy", "buy");
	s_tex[MTEX_CELLPHONE]    = FindMobileTex(dict, "hud_Cellphone", "hud_phone");
	s_tex[MTEX_SUBMISSION]   = FindMobileTex(dict, "hud_tut_activate", "hud_target", "hud_tommy");

	s_weaponIcons[WEAPONTYPE_UNARMED]           = FindMobileTex(dict, "hud_fist");
	s_weaponIcons[WEAPONTYPE_BRASSKNUCKLE]      = FindMobileTex(dict, "hud_Brass-Knuckles", "hud_brassk");
	s_weaponIcons[WEAPONTYPE_SCREWDRIVER]       = FindMobileTex(dict, "hud_screwdriver");
	s_weaponIcons[WEAPONTYPE_GOLFCLUB]          = FindMobileTex(dict, "hud_golf");
	s_weaponIcons[WEAPONTYPE_NIGHTSTICK]        = FindMobileTex(dict, "hud_baton");
	s_weaponIcons[WEAPONTYPE_KNIFE]             = FindMobileTex(dict, "hud_knife");
	s_weaponIcons[WEAPONTYPE_BASEBALLBAT]       = FindMobileTex(dict, "hud_bat");
	s_weaponIcons[WEAPONTYPE_HAMMER]            = FindMobileTex(dict, "hud_hammer");
	s_weaponIcons[WEAPONTYPE_CLEAVER]           = FindMobileTex(dict, "hud_cleaver");
	s_weaponIcons[WEAPONTYPE_MACHETE]           = FindMobileTex(dict, "hud_machette", "hud_machete");
	s_weaponIcons[WEAPONTYPE_KATANA]            = FindMobileTex(dict, "hud_katana");
	s_weaponIcons[WEAPONTYPE_CHAINSAW]          = FindMobileTex(dict, "hud_chainsaw");
	s_weaponIcons[WEAPONTYPE_GRENADE]           = FindMobileTex(dict, "hud_grenade");
	s_weaponIcons[WEAPONTYPE_DETONATOR_GRENADE] = FindMobileTex(dict, "hud_grenade");
	s_weaponIcons[WEAPONTYPE_TEARGAS]           = FindMobileTex(dict, "hud_teargas");
	s_weaponIcons[WEAPONTYPE_MOLOTOV]           = FindMobileTex(dict, "hud_molotov");
	s_weaponIcons[WEAPONTYPE_ROCKET]            = FindMobileTex(dict, "hud_rocket");
	s_weaponIcons[WEAPONTYPE_COLT45]            = FindMobileTex(dict, "hud_pistol");
	s_weaponIcons[WEAPONTYPE_PYTHON]            = FindMobileTex(dict, "hud_python");
	s_weaponIcons[WEAPONTYPE_SHOTGUN]           = FindMobileTex(dict, "hud_chromedshotgun", "hud_shotgun");
	s_weaponIcons[WEAPONTYPE_SPAS12_SHOTGUN]    = FindMobileTex(dict, "hud_spaz");
	s_weaponIcons[WEAPONTYPE_STUBBY_SHOTGUN]    = FindMobileTex(dict, "hud_stubbs");
	s_weaponIcons[WEAPONTYPE_TEC9]              = FindMobileTex(dict, "hud_tec9");
	s_weaponIcons[WEAPONTYPE_UZI]               = FindMobileTex(dict, "hud_uzi");
	s_weaponIcons[WEAPONTYPE_SILENCED_INGRAM]   = FindMobileTex(dict, "hud_uzi2");
	s_weaponIcons[WEAPONTYPE_MP5]               = FindMobileTex(dict, "hud_mp5");
	s_weaponIcons[WEAPONTYPE_M4]                = FindMobileTex(dict, "hud_m4", "hud_m16");
	s_weaponIcons[WEAPONTYPE_RUGER]             = FindMobileTex(dict, "hud_rugger", "hud_ak47");
	s_weaponIcons[WEAPONTYPE_SNIPERRIFLE]       = FindMobileTex(dict, "hud_sniper");
	s_weaponIcons[WEAPONTYPE_LASERSCOPE]        = FindMobileTex(dict, "hud_sniper2");
	s_weaponIcons[WEAPONTYPE_ROCKETLAUNCHER]    = FindMobileTex(dict, "hud_rpg");
	s_weaponIcons[WEAPONTYPE_FLAMETHROWER]      = FindMobileTex(dict, "hud_flame");
	s_weaponIcons[WEAPONTYPE_M60]               = FindMobileTex(dict, "hud_m60");
	s_weaponIcons[WEAPONTYPE_MINIGUN]           = FindMobileTex(dict, "hud_minigun");
	s_weaponIcons[WEAPONTYPE_DETONATOR]         = FindMobileTex(dict, "hud_detonator");
	s_weaponIcons[WEAPONTYPE_CAMERA]            = FindMobileTex(dict, "hud_camera");
}

static bool
TryLoadMobileHudTxd(void)
{
	static const char *const kCandidatePaths[] = {
		"custom/hud/hud.txd",
		"CUSTOM/HUD/HUD.TXD",
		"custom/hud/mobile.txd",
		"models/mobile_hud.txd"
	};

#ifdef GTA_PC
	_rwD3D8TexDictionaryEnableRasterFormatConversion(true);
#endif

	for (size_t i = 0; i < ARRAY_SIZE(kCandidatePaths); i++) {
		RwStream *stream = RwStreamOpen(rwSTREAMFILENAME, rwSTREAMREAD, kCandidatePaths[i]);
		if (!stream)
			continue;

		if (s_mobileTxdSlot < 0)
			s_mobileTxdSlot = CTxdStore::AddTxdSlot("mobile_hud");

		bool ok = CTxdStore::LoadTxd(s_mobileTxdSlot, stream);
		RwStreamClose(stream, nil);
		if (ok && GetMobileDict()) {
			CTxdStore::AddRef(s_mobileTxdSlot);
			int32 count = 0;
			RwTexDictionaryForAllTextures(GetMobileDict(), CountTexturesCB, &count);
			REVC_LOG_INFO("Mobile HUD: loaded %s (%d textures)", kCandidatePaths[i], count);
			s_hasCustomTxd = count > 0;
			ResolveMobileTextures();
			return true;
		}
	}

	s_hasCustomTxd = false;
	return false;
}

// ---------------------------------------------------------------------------
// Context Helpers (Vehicles, Pickups, Scopes)
// ---------------------------------------------------------------------------

static bool
IsHelicopterVehicle(CVehicle *veh)
{
	if (!veh)
		return false;
	if (veh->GetVehicleAppearance() == VEHICLE_APPEARANCE_HELI)
		return true;
	const int32 mi = veh->GetModelIndex();
	return mi == MI_CHOPPER || mi == MI_HUNTER || mi == MI_SEASPAR ||
	       mi == MI_SPARROW || mi == MI_MAVERICK || mi == MI_VCNMAV ||
	       mi == MI_POLMAV || mi == MI_RCRAIDER || mi == MI_RCGOBLIN;
}

static bool
IsPlaneVehicle(CVehicle *veh)
{
	if (!veh)
		return false;
	if (veh->IsPlane() || veh->GetVehicleAppearance() == VEHICLE_APPEARANCE_PLANE)
		return true;
	const int32 mi = veh->GetModelIndex();
	return mi == MI_SKIMMER || mi == MI_RCBARON || mi == MI_DEADDODO;
}

static bool
IsArmedVehicle(CVehicle *veh, MobileTexId *outTex)
{
	if (!veh)
		return false;
	const int32 mi = veh->GetModelIndex();
	if (mi == MI_HUNTER) {
		if (outTex) *outTex = MTEX_ROCKETS;
		return true;
	}
	if (mi == MI_RHINO || mi == MI_PREDATOR || mi == MI_SEASPAR ||
	    mi == MI_FIRETRUCK || mi == MI_RCBARON) {
		if (outTex) *outTex = MTEX_SHOOT;
		return true;
	}
	return false;
}

static bool
SupportsVehicleSubmission(CVehicle *veh)
{
	if (!veh)
		return false;
	if (veh->UsesSiren())
		return true;
	const int32 mi = veh->GetModelIndex();
	return mi == MI_TAXI || mi == MI_CABBIE || mi == MI_ZEBRA ||
	       mi == MI_KAUFMAN || mi == MI_PIZZABOY || mi == MI_TRASH ||
	       mi == MI_COACH || mi == MI_BUS;
}

static CVehicle*
FindNearbyEnterableVehicle(CPlayerPed *player, float maxDist)
{
	if (!player || !CPools::GetVehiclePool())
		return nil;

	const CVector playerPos = player->GetPosition();
	const float maxDistSq = maxDist * maxDist;
	CVehicle *bestVeh = nil;
	float bestDistSq = maxDistSq;

	CVehiclePool *pool = CPools::GetVehiclePool();
	const int32 poolSize = pool->GetSize();
	for (int32 i = 0; i < poolSize; i++) {
		CVehicle *v = pool->GetSlot(i);
		if (!v || v->GetStatus() == STATUS_WRECKED)
			continue;
		if (v->m_nDoorLock == CARLOCK_LOCKED || v->m_nDoorLock == CARLOCK_LOCKED_INITIALLY ||
		    v->m_nDoorLock == CARLOCK_LOCKOUT_PLAYER_ONLY)
			continue;
		const CVector diff = v->GetPosition() - playerPos;
		if (Abs(diff.z) > 6.5f)
			continue;
		const float dSq = diff.MagnitudeSqr();
		if (dSq < bestDistSq) {
			bestDistSq = dSq;
			bestVeh = v;
		}
	}
	return bestVeh;
}

static bool
CheckNearbyInteractPickup(CPlayerPed *player, MobileTexId *outTex)
{
	if (!player)
		return false;
	if (player->m_nPedState == PED_MAKE_CALL) {
		if (outTex) *outTex = MTEX_CELLPHONE;
		return true;
	}
	if (CPickups::PlayerOnWeaponPickup > 0) {
		if (outTex) *outTex = MTEX_SWAPGUN;
		return true;
	}
	const CVector p = player->GetPosition();
	for (int32 i = 0; i < NUMPICKUPS; i++) {
		const CPickup &pk = CPickups::aPickUps[i];
		if (pk.m_eType != PICKUP_PROPERTY_FORSALE && pk.m_eType != PICKUP_IN_SHOP)
			continue;
		if (pk.m_bRemoved)
			continue;
		const float dx = pk.m_vecPos.x - p.x;
		const float dy = pk.m_vecPos.y - p.y;
		const float dz = pk.m_vecPos.z - p.z;
		if (dx * dx + dy * dy + dz * dz <= 6.25f) {
			if (outTex) *outTex = MTEX_BUY;
			return true;
		}
	}
	return false;
}

static bool
IsFirstPersonScopeWeapon(eWeaponType wt)
{
	return wt == WEAPONTYPE_SNIPERRIFLE || wt == WEAPONTYPE_LASERSCOPE ||
	       wt == WEAPONTYPE_ROCKETLAUNCHER || wt == WEAPONTYPE_CAMERA;
}

static bool
IsInFirstPersonScopeCam(void)
{
	const int32 mode = TheCamera.Cams[TheCamera.ActiveCam].Mode;
	return mode == CCam::MODE_SNIPER || mode == CCam::MODE_SNIPER_RUNABOUT ||
	       mode == CCam::MODE_ROCKETLAUNCHER || mode == CCam::MODE_ROCKETLAUNCHER_RUNABOUT ||
	       mode == CCam::MODE_M16_1STPERSON || mode == CCam::MODE_M16_1STPERSON_RUNABOUT ||
	       mode == CCam::MODE_CAMERA || mode == CCam::MODE_HELICANNON_1STPERSON;
}

static bool
IsGameplayTouchHudActive(void)
{
	if (gGameState != GS_PLAYING_GAME)
		return false;
	if (FrontEndMenuManager.m_bMenuActive)
		return false;
	if (CReplay::IsPlayingBack() || CCutsceneMgr::IsRunning() || TheCamera.m_WideScreenOn)
		return false;
	return CHud::m_Wants_To_Draw_Hud;
}

// ---------------------------------------------------------------------------
// DP-Responsive Gameplay Layout
// ---------------------------------------------------------------------------

static void
SetCircleBtn(VirtualButton &b, float cx, float cy, float radius, MobileTexId tex, FallbackGlyph glyph)
{
	b.visible = true;
	b.invisibleHitZone = false;
	b.circular = true;
	b.preserveAspect = false;
	b.cx = cx;
	b.cy = cy;
	b.halfW = radius;
	b.halfH = radius;
	b.texId = tex;
	b.glyph = glyph;
}

static void
SetRectBtn(VirtualButton &b, float cx, float cy, float halfW, float halfH, MobileTexId tex, FallbackGlyph glyph, bool invisible = false, bool preserveAspect = false)
{
	b.visible = true;
	b.invisibleHitZone = invisible;
	b.circular = false;
	b.preserveAspect = preserveAspect;
	b.cx = cx;
	b.cy = cy;
	b.halfW = halfW;
	b.halfH = halfH;
	b.texId = tex;
	b.glyph = glyph;
}

static void
UpdateGameplayLayout(void)
{
	for (int i = 0; i < MBTN_COUNT; i++) {
		s_buttons[i].visible = false;
		s_buttons[i].invisibleHitZone = false;
	}
	s_stickVisible = false;

	if (!IsGameplayTouchHudActive())
		return;

	CPlayerPed *player = FindPlayerPed();
	if (!player)
		return;

	const MobileTouchMath::MobileScreenMetrics m = GetMetrics();
	const float W = m.screenW;
	const float H = m.screenH;
	bool stickCurrentlyOwned = false;
	for (int i = 0; i < MAX_TOUCH_POINTERS; i++) {
		if (s_pointers[i].active && s_pointers[i].isStick) {
			stickCurrentlyOwned = true;
			break;
		}
	}

	// 1. Top-Left Radar tap zone (Pause / Map) or standalone 46dp Pause button if radar hidden
	const bool radarVisible = FrontEndMenuManager.m_PrefsRadarMode != 2 && !CHud::m_HideRadar;
	if (radarVisible) {
		const float pad = m.Dp(6.0f);
		const float rx1 = m.radarLeftPx - pad;
		const float ry1 = m.radarTopPx - pad;
		const float rx2 = m.radarLeftPx + m.radarSizePx + pad;
		const float ry2 = m.radarTopPx + m.radarSizePx + pad;
		SetRectBtn(s_buttons[MBTN_PAUSE], 0.5f * (rx1 + rx2), 0.5f * (ry1 + ry2),
			0.5f * (rx2 - rx1), 0.5f * (ry2 - ry1), MTEX_PAUSE, GLYPH_PAUSE, true);
	} else {
		const float rPause = m.Dp(23.0f);
		SetCircleBtn(s_buttons[MBTN_PAUSE], m.marginLeftPx + rPause, m.marginTopPx + rPause, rPause, MTEX_PAUSE, GLYPH_PAUSE);
	}

	// 2. Top-Right HUD weapon icon tap/swipe zone + DP-sized Camera toggle
	const float hudRightBase = W - m.safeRightPx;
	const float wx1 = hudRightBase - m.hudUnitPx * 104.0f;
	const float wx2 = hudRightBase - m.hudUnitPx * 28.0f;
	const float wy1 = m.safeTopPx + m.hudUnitPx * 20.0f;
	const float wy2 = m.safeTopPx + m.hudUnitPx * 98.0f;

	CVehicle *veh = FindPlayerVehicle();
	if (!veh && FrontEndMenuManager.m_PrefsShowHud) {
		SetRectBtn(s_buttons[MBTN_WEAPON_SLOT], 0.5f * (wx1 + wx2), 0.5f * (wy1 + wy2),
			0.5f * (wx2 - wx1), 0.5f * (wy2 - wy1), MTEX_SWAPGUN, GLYPH_NONE, true);
	}

	const float rTopIcon = m.Dp(22.0f); // 44dp visual, >52dp touch hitbox
	const float topIconY = m.marginTopPx + rTopIcon;
	const float camIconX = Min(hudRightBase - m.hudUnitPx * 212.0f, W - m.marginRightPx - m.Dp(165.0f));
	SetCircleBtn(s_buttons[MBTN_CAM], camIconX, topIconY, rTopIcon, MTEX_CAM_TOGGLE, GLYPH_CAMERA);

	// Quick-access top bar buttons: Skin Switcher, Vehicle Spawner, and collapsible utility drawer (FLY / TIME+ / TIME-)
	const float rDev = m.Dp(18.0f);
	const float devX = Max(m.radarLeftPx + m.radarSizePx + m.Dp(24.0f) + rDev, W * 0.27f);
	const float devStep = m.Dp(44.0f);
	SetCircleBtn(s_buttons[MBTN_DEV_SKIN], devX, topIconY, rDev, MTEX_COUNT, GLYPH_PERSON);
	SetCircleBtn(s_buttons[MBTN_DEV_VEHICLE], devX + devStep, topIconY, rDev, MTEX_CAR, GLYPH_CAR);
	SetCircleBtn(s_buttons[MBTN_DEV_TOGGLE], devX + devStep * 2.0f, topIconY, rDev, MTEX_COUNT, GLYPH_DOTS);
	if (s_devDrawerOpen) {
		SetCircleBtn(s_buttons[MBTN_DEV_FLY], devX + devStep * 3.0f, topIconY, rDev, MTEX_COUNT, GLYPH_FLY);
		SetCircleBtn(s_buttons[MBTN_DEV_TIME_PLUS], devX + devStep * 4.0f, topIconY, rDev, MTEX_ZOOM_IN, GLYPH_PLUS);
		SetCircleBtn(s_buttons[MBTN_DEV_TIME_MINUS], devX + devStep * 5.0f, topIconY, rDev, MTEX_ZOOM_OUT, GLYPH_MINUS);
	}

	// 3. IN VEHICLE LAYOUT (DP-sized pedals, steering, and context actions)
	if (veh) {
		if (s_scopeLatched) {
			s_scopeLatched = false;
			s_scopeLatchedWeapon = -1;
		}

		const bool isHeli = IsHelicopterVehicle(veh);
		const bool isPlane = IsPlaneVehicle(veh);
		const bool isBike = veh->IsBike();
		const bool isTank = (veh->GetModelIndex() == MI_RHINO);
		const bool useStick = isHeli || isPlane || s_vehicleStickMode;

		SetCircleBtn(s_buttons[MBTN_DRIVE_MODE], camIconX - m.Dp(54.0f), topIconY, rTopIcon, MTEX_DRIVE_TOGGLE, GLYPH_STEER_MODE);
		SetRectBtn(s_buttons[MBTN_RADIO_SLOT], W * 0.50f, m.marginTopPx + m.Dp(22.0f),
			m.Dp(68.0f), m.Dp(24.0f), MTEX_COUNT, GLYPH_NONE, true);

		if (SupportsVehicleSubmission(veh)) {
			SetCircleBtn(s_buttons[MBTN_SUBMISSION], camIconX - m.Dp(108.0f), topIconY, rTopIcon, MTEX_SUBMISSION, GLYPH_TARGET);
		}

		const float defStickX = m.marginLeftPx + m.Dp(56.0f) + m.Dp(6.0f);
		const float defStickY = H - m.marginBottomPx - m.Dp(56.0f) - m.Dp(4.0f);
		if (useStick) {
			s_stickVisible = true;
			s_stickRadius = m.Dp(56.0f); // 112dp dynamic thumbstick
			if (stickCurrentlyOwned) {
				s_stickBaseX = s_stickAnchorX;
				s_stickBaseY = s_stickAnchorY;
			} else {
				s_stickBaseX = defStickX;
				s_stickBaseY = defStickY;
			}
		} else {
			const float rSteer = m.Dp(34.0f); // 68dp steering buttons
			const float steerY = H - m.marginBottomPx - rSteer - m.Dp(8.0f);
			const float leftX = m.marginLeftPx + rSteer + m.Dp(4.0f);
			const float rightX = leftX + rSteer * 2.0f + m.Dp(16.0f);
			SetCircleBtn(s_buttons[MBTN_STEER_LEFT], leftX, steerY, rSteer,
				isTank ? MTEX_TANK_LEFT : MTEX_LEFT, GLYPH_ARROW_LEFT);
			SetCircleBtn(s_buttons[MBTN_STEER_RIGHT], rightX, steerY, rSteer,
				isTank ? MTEX_TANK_RIGHT : MTEX_RIGHT, GLYPH_ARROW_RIGHT);
		}

		const float gasHalfW = m.Dp(27.0f);   // 54dp x 98dp gas pedal
		const float gasHalfH = m.Dp(49.0f);
		const float brakeHalfW = m.Dp(25.0f); // 50dp x 68dp brake pedal
		const float brakeHalfH = m.Dp(34.0f);

		const float gasX = W - m.marginRightPx - gasHalfW - m.Dp(2.0f);
		const float gasY = H - m.marginBottomPx - gasHalfH;
		const float brakeX = gasX - gasHalfW - brakeHalfW - m.Dp(16.0f);
		const float brakeY = H - m.marginBottomPx - brakeHalfH - m.Dp(2.0f);

		SetRectBtn(s_buttons[MBTN_ACCEL], gasX, gasY, gasHalfW, gasHalfH, MTEX_ACCELERATE, GLYPH_PEDAL_GAS, false, true);
		SetRectBtn(s_buttons[MBTN_BRAKE], brakeX, brakeY, brakeHalfW, brakeHalfH, MTEX_BRAKE, GLYPH_PEDAL_BRAKE, false, true);

		const float rSub = m.Dp(27.0f); // 54dp secondary vehicle buttons
		SetCircleBtn(s_buttons[MBTN_HANDBRAKE], brakeX - brakeHalfW - rSub - m.Dp(14.0f),
			H - m.marginBottomPx - rSub - m.Dp(4.0f), rSub, MTEX_HANDBRAKE, GLYPH_HANDBRAKE);

		MobileTexId exitTex = MTEX_EXIT_CAR;
		if (isBike) exitTex = MTEX_EXIT_BIKE;
		else if (isHeli) exitTex = MTEX_EXIT_HELI;
		else if (isPlane) exitTex = MTEX_EXIT_PLANE;
		SetCircleBtn(s_buttons[MBTN_ENTER_EXIT], gasX - m.Dp(4.0f),
			gasY - gasHalfH - m.Dp(32.0f), m.Dp(25.0f), exitTex, GLYPH_EXIT);

		SetCircleBtn(s_buttons[MBTN_HORN], brakeX,
			brakeY - brakeHalfH - m.Dp(32.0f), m.Dp(24.0f), MTEX_HORN, GLYPH_HORN);

		MobileTexId armedTex = MTEX_SHOOT;
		if (IsArmedVehicle(veh, &armedTex)) {
			SetCircleBtn(s_buttons[MBTN_CAR_GUN], brakeX - brakeHalfW - rSub - m.Dp(14.0f),
				brakeY - brakeHalfH - m.Dp(28.0f), m.Dp(27.0f), armedTex, GLYPH_CROSSHAIR);
		}
		if (isHeli || isPlane) {
			const float rYaw = m.Dp(22.0f);
			const float yawY = defStickY - m.Dp(56.0f) - rYaw - m.Dp(14.0f);
			SetCircleBtn(s_buttons[MBTN_YAW_LEFT], m.marginLeftPx + rYaw + m.Dp(4.0f), yawY, rYaw, MTEX_LEFT, GLYPH_ARROW_LEFT);
			SetCircleBtn(s_buttons[MBTN_YAW_RIGHT], m.marginLeftPx + rYaw * 3.0f + m.Dp(16.0f), yawY, rYaw, MTEX_RIGHT, GLYPH_ARROW_RIGHT);
		}
		return;
	}

	// 4. ON FOOT / SCOPE LAYOUT (Dynamic transparent joystick + Run hold / double-tap Jump)
	s_stickVisible = true;
	s_stickRadius = m.Dp(56.0f); // 112dp diameter
	if (stickCurrentlyOwned) {
		s_stickBaseX = s_stickAnchorX;
		s_stickBaseY = s_stickAnchorY;
	} else {
		s_stickBaseX = m.marginLeftPx + s_stickRadius + m.Dp(6.0f);
		s_stickBaseY = H - m.marginBottomPx - s_stickRadius - m.Dp(4.0f);
	}

	const eWeaponType wt = player->GetWeapon()->m_eWeaponType;
	CWeaponInfo *wi = CWeaponInfo::GetWeaponInfo(wt);
	if (s_scopeLatched && s_scopeLatchedWeapon != (int32)wt) {
		s_scopeLatched = false;
		s_scopeLatchedWeapon = -1;
	}

	MobileTexId attackTex = MTEX_SHOOT;
	FallbackGlyph attackGlyph = GLYPH_CROSSHAIR;
	if (wt == WEAPONTYPE_UNARMED || (wi && wi->m_eWeaponFire == WEAPON_FIRE_MELEE)) {
		attackTex = MTEX_PUNCH;
		attackGlyph = GLYPH_FIST;
	} else if (wt == WEAPONTYPE_GRENADE || wt == WEAPONTYPE_DETONATOR_GRENADE ||
	           wt == WEAPONTYPE_TEARGAS || wt == WEAPONTYPE_MOLOTOV) {
		attackTex = MTEX_THROW;
		attackGlyph = GLYPH_GRENADE;
	} else if (wt == WEAPONTYPE_DETONATOR) {
		attackTex = MTEX_BOMB;
	} else if (wt == WEAPONTYPE_CAMERA) {
		attackTex = MTEX_CAMERA_WPN;
		attackGlyph = GLYPH_CAMERA;
	}

	const float rAttack = m.Dp(39.0f); // 78dp primary fire/melee button
	const float rSub = m.Dp(28.0f);    // 56dp secondary arc buttons (>= 48dp Material standard)
	const float attackX = W - m.marginRightPx - rAttack - m.Dp(14.0f);
	const float attackY = H - m.marginBottomPx - rAttack - m.Dp(20.0f);

	SetCircleBtn(s_buttons[MBTN_ATTACK], attackX, attackY, rAttack, attackTex, attackGlyph);

	if (IsInFirstPersonScopeCam()) {
		SetCircleBtn(s_buttons[MBTN_TARGET], attackX + m.Dp(6.0f), attackY - rAttack - rSub - m.Dp(14.0f), rSub, MTEX_LOCKON, GLYPH_TARGET);
		const int32 camMode = TheCamera.Cams[TheCamera.ActiveCam].Mode;
		if (camMode == CCam::MODE_SNIPER || camMode == CCam::MODE_SNIPER_RUNABOUT || camMode == CCam::MODE_CAMERA) {
			SetCircleBtn(s_buttons[MBTN_ZOOM_IN], attackX - rAttack - rSub - m.Dp(16.0f), attackY - m.Dp(34.0f), m.Dp(26.0f), MTEX_ZOOM_IN, GLYPH_PLUS);
			SetCircleBtn(s_buttons[MBTN_ZOOM_OUT], attackX - rAttack - rSub - m.Dp(16.0f), attackY + m.Dp(28.0f), m.Dp(26.0f), MTEX_ZOOM_OUT, GLYPH_MINUS);
		}
		return;
	}

	const float rRun = m.Dp(32.0f); // 64dp combined Sprint (hold) + Jump (double-tap) button
	const float arcR = rAttack + rSub + m.Dp(14.0f);
	const float runArcR = rAttack + rRun + m.Dp(16.0f);
	const float runY = Min(attackY + m.Dp(14.0f), H - m.marginBottomPx - rRun - m.Dp(4.0f));
	SetCircleBtn(s_buttons[MBTN_SPRINT], attackX - runArcR * 0.94f, runY, rRun, MTEX_RUN, GLYPH_SPRINT);

	if (wi && (wi->m_nWeaponSlot >= 2 || wi->IsFlagSet(WEAPONFLAG_CANAIM) || IsFirstPersonScopeWeapon(wt))) {
		SetCircleBtn(s_buttons[MBTN_TARGET], attackX + m.Dp(6.0f), attackY - arcR, m.Dp(27.0f), MTEX_LOCKON, GLYPH_TARGET);
	}

	CVehicle *nearVeh = FindNearbyEnterableVehicle(player, 13.0f);
	const bool enteringCar = (player->m_nPedState == PED_ENTER_CAR || player->m_nPedState == PED_CARJACK);
	if (nearVeh || enteringCar) {
		MobileTexId enterTex = MTEX_CAR;
		if (nearVeh) {
			if (nearVeh->IsBike()) enterTex = MTEX_BIKE;
			else if (nearVeh->IsBoat()) enterTex = MTEX_BOAT;
			else if (IsHelicopterVehicle(nearVeh)) enterTex = MTEX_CHOPPER;
			else if (IsPlaneVehicle(nearVeh)) enterTex = MTEX_PLANE;
		}
		SetCircleBtn(s_buttons[MBTN_ENTER_EXIT], attackX - arcR * 1.62f, attackY - m.Dp(12.0f), m.Dp(29.0f), enterTex, GLYPH_CAR);
	}

	MobileTexId interactTex = MTEX_BUY;
	if (CheckNearbyInteractPickup(player, &interactTex)) {
		SetCircleBtn(s_buttons[MBTN_INTERACT], attackX - arcR * 1.52f, attackY - m.Dp(78.0f), m.Dp(27.0f), interactTex, GLYPH_PLUS);
	}
}

static bool
HitTestButton(const VirtualButton &b, float x, float y, float minTouchRadiusPx)
{
	if (!b.visible)
		return false;
	const float dx = x - b.cx;
	const float dy = y - b.cy;
	if (b.circular) {
		const float hitR = Max(b.halfW * 1.18f, minTouchRadiusPx);
		return (dx * dx + dy * dy) <= (hitR * hitR);
	}
	const float padX = Max(b.halfW * 1.14f, minTouchRadiusPx);
	const float padY = Max(b.halfH * 1.14f, minTouchRadiusPx);
	return Abs(dx) <= padX && Abs(dy) <= padY;
}

static MobileBtnId
FindHitGameplayButton(float x, float y, float minTouchRadiusPx)
{
	for (int pass = 0; pass < 2; pass++) {
		for (int i = MBTN_COUNT - 1; i >= 0; i--) {
			const VirtualButton &b = s_buttons[i];
			if (!b.visible)
				continue;
			if ((pass == 0 && b.invisibleHitZone) || (pass == 1 && !b.invisibleHitZone))
				continue;
			if (HitTestButton(b, x, y, minTouchRadiusPx))
				return (MobileBtnId)i;
		}
	}
	return MBTN_NONE;
}

static bool
IsGameplayButtonHeld(MobileBtnId id)
{
	for (int i = 0; i < MAX_TOUCH_POINTERS; i++) {
		if (s_pointers[i].active && s_pointers[i].btn == id)
			return true;
	}
	return false;
}

static ActivePointer*
FindGameplayPointer(int64 fingerId)
{
	for (int i = 0; i < MAX_TOUCH_POINTERS; i++) {
		if (s_pointers[i].active && s_pointers[i].fingerId == fingerId)
			return &s_pointers[i];
	}
	return nil;
}

static ActivePointer*
AllocGameplayPointer(int64 fingerId)
{
	ActivePointer *existing = FindGameplayPointer(fingerId);
	if (existing)
		return existing;
	for (int i = 0; i < MAX_TOUCH_POINTERS; i++) {
		if (!s_pointers[i].active) {
			memset(&s_pointers[i], 0, sizeof(ActivePointer));
			s_pointers[i].active = true;
			s_pointers[i].fingerId = fingerId;
			s_pointers[i].btn = MBTN_NONE;
			return &s_pointers[i];
		}
	}
	return nil;
}

static bool
IsStickOwned(void)
{
	for (int i = 0; i < MAX_TOUCH_POINTERS; i++) {
		if (s_pointers[i].active && s_pointers[i].isStick)
			return true;
	}
	return false;
}

static void
UpdateStickFromTouch(float x, float y)
{
	float offX = 0.0f, offY = 0.0f;
	MobileTouchMath::EvalStick(x - s_stickAnchorX, y - s_stickAnchorY,
		s_stickRadius, 0.13f, &offX, &offY, &s_stickAxisX, &s_stickAxisY);
	s_stickKnobX = s_stickBaseX + offX;
	s_stickKnobY = s_stickBaseY + offY;
}

static void
HandleGameplayButtonDownPulse(MobileBtnId btn)
{
	switch (btn) {
	case MBTN_PAUSE:
		s_pulsePause = true;
		break;
	case MBTN_CAM:
		s_pulseCam = true;
		break;
	case MBTN_DRIVE_MODE:
		s_vehicleStickMode = !s_vehicleStickMode;
		break;
	case MBTN_INTERACT:
		s_pulseInteract = true;
		break;
	case MBTN_SUBMISSION:
		s_pulseSubmission = true;
		break;
	case MBTN_TARGET: {
		CPlayerPed *ped = FindPlayerPed();
		if (ped && !FindPlayerVehicle()) {
			const eWeaponType wt = ped->GetWeapon()->m_eWeaponType;
			if (IsFirstPersonScopeWeapon(wt) || IsInFirstPersonScopeCam()) {
				s_scopeLatched = !s_scopeLatched;
				s_scopeLatchedWeapon = s_scopeLatched ? (int32)wt : -1;
			}
		}
		break;
	}
	case MBTN_DEV_SKIN:
		if (s_spawnerMode == mobilespawner::PANEL_SKIN) {
			s_spawnerMode = mobilespawner::PANEL_NONE;
		} else {
			RebuildSkinCatalog();
			s_spawnerMode = mobilespawner::PANEL_SKIN;
		}
		DMAudio.PlayFrontEndSound(SOUND_FRONTEND_HIGHLIGHT_OPTION, 0);
		break;
	case MBTN_DEV_VEHICLE:
		if (s_spawnerMode == mobilespawner::PANEL_VEHICLE) {
			s_spawnerMode = mobilespawner::PANEL_NONE;
		} else {
			RebuildVehicleCatalog();
			s_spawnerMode = mobilespawner::PANEL_VEHICLE;
		}
		DMAudio.PlayFrontEndSound(SOUND_FRONTEND_HIGHLIGHT_OPTION, 0);
		break;
	case MBTN_DEV_TOGGLE:
		s_devDrawerOpen = !s_devDrawerOpen;
		break;
	case MBTN_DEV_FLY:
		s_pulseFlyToggle = true;
		break;
	case MBTN_DEV_TIME_PLUS:
		CClock::SetGameClock((CClock::GetHours() + 1) % 24, CClock::GetMinutes());
		break;
	case MBTN_DEV_TIME_MINUS:
		CClock::SetGameClock((CClock::GetHours() + 23) % 24, CClock::GetMinutes());
		break;
	case MBTN_SPRINT:
		s_sprintJumpTap.OnPress(CTimer::GetTimeInMilliseconds());
		break;
	default:
		break;
	}
}

// ---------------------------------------------------------------------------
// Smartphone Menu Touch, Swipe, Slider Scrub, Stats Scroll & Map Pinch Engine
// ---------------------------------------------------------------------------

static float
MenuX(float x)
{
	return SCREEN_SCALE_X(x);
}

static float
MenuY(float y)
{
	return SCREEN_SCALE_Y(y);
}

static float
MenuXLeft(float x)
{
	return CMobileTouchControls::MenuVirtualToScreenX(x);
}

static float
MenuXRight(float x)
{
	return CMobileTouchControls::MenuVirtualToScreenX(DEFAULT_SCREEN_WIDTH - x);
}

static bool
MenuHasNativeList(int32 screen)
{
	return screen == MENUPAGE_SKIN_SELECT || screen == MENUPAGE_KEYBOARD_CONTROLS;
}

static int32
GetMenuMaxVisibleRows(void)
{
#ifdef ANDROID
	return 9;
#else
	return 12;
#endif
}

static float
GetScreenLineHeight(int32 screen)
{
#ifdef CUSTOM_FRONTEND_OPTIONS
	if (aScreens[screen].layout && aScreens[screen].layout->lineHeight > 0)
		return (float)aScreens[screen].layout->lineHeight;
#endif
	return (float)MENU_DEFAULT_LINE_HEIGHT;
}

static bool
IsSliderMenuAction(int32 action)
{
	return action == MENUACTION_BRIGHTNESS || action == MENUACTION_DRAWDIST ||
	       action == MENUACTION_MUSICVOLUME || action == MENUACTION_SFXVOLUME ||
	       action == MENUACTION_MOUSESENS || action == MENUACTION_MP3VOLUMEBOOST
#ifdef CUSTOM_FRONTEND_OPTIONS
	       || action == MENUACTION_CFO_SLIDER
#endif
	       ;
}

static bool
IsCycleChoiceAction(int32 action)
{
	return action == MENUACTION_WIDESCREEN || action == MENUACTION_SPEAKERCONF ||
	       action == MENUACTION_AUDIOHW || action == MENUACTION_SCREENRES ||
	       action == MENUACTION_CTRLMETHOD || action == MENUACTION_RADIO
#ifdef GAMEPAD_MENU
	       || action == MENUACTION_CTRLCONFIG
#endif
	       ;
}

static bool
ShouldShowMenuBackOverlay(int32 screen)
{
	return screen != MENUPAGE_START_MENU &&
	       screen != MENUPAGE_SAVING_IN_PROGRESS &&
	       screen != MENUPAGE_DELETING_IN_PROGRESS &&
	       screen != MENUPAGE_OUTRO &&
	       screen != MENUPAGE_LOAD_SLOT_CONFIRM &&
	       screen != MENUPAGE_DELETE_SLOT_CONFIRM &&
	       screen != MENUPAGE_SAVE_OVERWRITE_CONFIRM &&
	       screen != MENUPAGE_NEW_GAME_RELOAD &&
	       screen != MENUPAGE_EXIT &&
	       screen != MENUPAGE_SAVE_CHEAT_WARNING;
}

static void
UpdateMenuOverlayLayout(void)
{
	for (int i = 0; i < MOBTN_COUNT; i++) {
		s_menuOverlayBtns[i].visible = false;
		s_menuOverlayBtns[i].invisibleHitZone = false;
	}
	if (!FrontEndMenuManager.m_bMenuActive || FrontEndMenuManager.m_firstStartCounter < 255)
		return;

	const MobileTouchMath::MobileScreenMetrics m = GetMetrics();
	const int32 screen = FrontEndMenuManager.m_nCurrScreen;
	const float rBtn = m.Dp(24.0f); // 48dp touch circle
	const float bottomY = m.screenH - m.marginBottomPx - rBtn;

	if (ShouldShowMenuBackOverlay(screen)) {
		const MobileTexId backTex = (screen == MENUPAGE_PAUSE_MENU) ? MTEX_PAUSE : MTEX_LEFT;
		const FallbackGlyph backGlyph = (screen == MENUPAGE_PAUSE_MENU) ? GLYPH_PAUSE : GLYPH_ARROW_LEFT;
		SetCircleBtn(s_menuOverlayBtns[MOBTN_BACK], m.marginLeftPx + rBtn, bottomY, rBtn, backTex, backGlyph);
	}

	if (screen == MENUPAGE_MAP) {
		const float rightX = m.screenW - m.marginRightPx - rBtn;
		const float step = rBtn * 2.0f + m.Dp(12.0f);
		SetCircleBtn(s_menuOverlayBtns[MOBTN_MAP_ZOOM_IN],  rightX, bottomY - step, rBtn, MTEX_ZOOM_IN, GLYPH_PLUS);
		SetCircleBtn(s_menuOverlayBtns[MOBTN_MAP_ZOOM_OUT], rightX, bottomY,        rBtn, MTEX_ZOOM_OUT, GLYPH_MINUS);
		SetCircleBtn(s_menuOverlayBtns[MOBTN_MAP_LEGEND],   rightX - step, bottomY, rBtn, MTEX_COUNT, GLYPH_LEGEND);
	}
}

static MenuOverlayBtn
HitTestMenuOverlay(float x, float y, float minTouchR)
{
	UpdateMenuOverlayLayout();
	for (int i = 0; i < MOBTN_COUNT; i++) {
		if (HitTestButton(s_menuOverlayBtns[i], x, y, minTouchR))
			return (MenuOverlayBtn)i;
	}
	return MOBTN_NONE;
}

static int32
HitTestStandardMenuRow(float x, float y)
{
	const int32 screen = FrontEndMenuManager.m_nCurrScreen;
	if (screen < 0 || screen >= MENUPAGES || MenuHasNativeList(screen) ||
	    screen == MENUPAGE_MAP || screen == MENUPAGE_STATS)
		return -1;

	if (x < MenuXLeft(20.0f) || x > MenuXRight(16.0f))
		return -1;

	const int32 maxVis = GetMenuMaxVisibleRows();
	const bool scrollable = (FrontEndMenuManager.m_nTotalListRow > maxVis && !MenuHasNativeList(screen));
	const int32 firstOpt = scrollable ? FrontEndMenuManager.m_nFirstVisibleRowOnList : 0;
	const float scrollOff = (float)(aScreens[screen].m_aEntries[firstOpt].m_Y - aScreens[screen].m_aEntries[0].m_Y);
	const float lineH = GetScreenLineHeight(screen);

	int32 bestRow = -1;
	float bestDist = 1e9f;

	for (int32 r = firstOpt; r < firstOpt + maxVis && r < NUM_MENUROWS; r++) {
		const int32 action = aScreens[screen].m_aEntries[r].m_Action;
		if (action == MENUACTION_NOTHING || action == MENUACTION_LABEL)
			continue;
		if (aScreens[screen].m_aEntries[r].m_EntryName[0] == '\0')
			continue;

		const float rowY = (float)aScreens[screen].m_aEntries[r].m_Y - scrollOff;
		const float yTop = MenuY(rowY - 4.0f);
		const float yBot = MenuY(rowY + lineH + 2.0f);
		if (y >= yTop && y <= yBot) {
			const float d = Abs(y - 0.5f * (yTop + yBot));
			if (d < bestDist) {
				bestDist = d;
				bestRow = r;
			}
		}
	}
	return bestRow;
}

static void
ApplyDirectSliderValue(int32 screen, int32 row, float touchX, float deltaX, bool directJump)
{
	if (row < 0 || row >= NUM_MENUROWS)
		return;
	const int32 action = aScreens[screen].m_aEntries[row].m_Action;
	if (!IsSliderMenuAction(action))
		return;

	const float sliderLeft = MenuXLeft(MENUSLIDER_X);
	const float sliderW = Max(MenuXLeft(MENUSLIDER_X + MENUSLIDER_UNK) - sliderLeft, 24.0f);

	auto getNorm = [&]() -> float {
		switch (action) {
		case MENUACTION_BRIGHTNESS:     return FrontEndMenuManager.m_PrefsBrightness / 384.0f;
		case MENUACTION_DRAWDIST:       return Clamp((FrontEndMenuManager.m_PrefsLOD - DRAWDIST_MIN) / DRAWDIST_RANGE, 0.0f, 1.0f);
		case MENUACTION_MUSICVOLUME:    return FrontEndMenuManager.m_PrefsMusicVolume / 64.0f;
		case MENUACTION_SFXVOLUME:      return FrontEndMenuManager.m_PrefsSfxVolume / 64.0f;
		case MENUACTION_MP3VOLUMEBOOST: return FrontEndMenuManager.m_PrefsMP3BoostVolume / 64.0f;
		case MENUACTION_MOUSESENS:      return TheCamera.m_fMouseAccelHorzntl * 200.0f;
#ifdef CUSTOM_FRONTEND_OPTIONS
		case MENUACTION_CFO_SLIDER: {
			CMenuScreenCustom::CMenuEntry &opt = aScreens[screen].m_aEntries[row];
			const float span = opt.m_CFOSlider->max - opt.m_CFOSlider->min;
			return (span > 1e-5f) ? ((*(float*)opt.m_CFOSlider->value - opt.m_CFOSlider->min) / span) : 0.0f;
		}
#endif
		default: return 0.5f;
		}
	};

	float t = getNorm();
	if (directJump && touchX >= sliderLeft - MenuX(18.0f)) {
		t = Clamp((touchX - sliderLeft) / sliderW, 0.0f, 1.0f);
	} else {
		t = Clamp(t + deltaX / Max(sliderW * 1.35f, 40.0f), 0.0f, 1.0f);
	}

	switch (action) {
	case MENUACTION_BRIGHTNESS:
		FrontEndMenuManager.m_PrefsBrightness = (int32)Clamp(t * 384.0f + 0.5f, 0.0f, 384.0f);
		break;
	case MENUACTION_DRAWDIST:
		FrontEndMenuManager.m_PrefsLOD = DRAWDIST_MIN + t * DRAWDIST_RANGE;
		CRenderer::ms_lodDistScale = FrontEndMenuManager.m_PrefsLOD;
		break;
	case MENUACTION_MUSICVOLUME:
		if (FrontEndMenuManager.m_nPrefsAudio3DProviderIndex != NO_AUDIO_PROVIDER) {
			FrontEndMenuManager.m_PrefsMusicVolume = (int8)Clamp(t * 64.0f + 0.5f, 0.0f, 64.0f);
			DMAudio.SetMusicMasterVolume(FrontEndMenuManager.m_PrefsMusicVolume);
		}
		break;
	case MENUACTION_SFXVOLUME:
		if (FrontEndMenuManager.m_nPrefsAudio3DProviderIndex != NO_AUDIO_PROVIDER) {
			FrontEndMenuManager.m_PrefsSfxVolume = (int8)Clamp(t * 64.0f + 0.5f, 0.0f, 64.0f);
			DMAudio.SetEffectsMasterVolume(FrontEndMenuManager.m_PrefsSfxVolume);
		}
		break;
	case MENUACTION_MP3VOLUMEBOOST:
		if (FrontEndMenuManager.m_nPrefsAudio3DProviderIndex != NO_AUDIO_PROVIDER && DMAudio.IsMP3RadioChannelAvailable()) {
			FrontEndMenuManager.m_PrefsMP3BoostVolume = (int8)Clamp(t * 64.0f + 0.5f, 0.0f, 64.0f);
			DMAudio.SetMP3BoostVolume(FrontEndMenuManager.m_PrefsMP3BoostVolume);
		}
		break;
	case MENUACTION_MOUSESENS:
		TheCamera.m_fMouseAccelHorzntl = Clamp(t * (1.0f / 200.0f), 1.0f / 3200.0f, 1.0f / 200.0f);
#ifdef FIX_BUGS
		TheCamera.m_fMouseAccelVertical = TheCamera.m_fMouseAccelHorzntl + 0.0005f;
#endif
		break;
#ifdef CUSTOM_FRONTEND_OPTIONS
	case MENUACTION_CFO_SLIDER: {
		CMenuScreenCustom::CMenuEntry &opt = aScreens[screen].m_aEntries[row];
		const float oldVal = *(float*)opt.m_CFOSlider->value;
		const float newVal = Clamp(opt.m_CFOSlider->min + t * (opt.m_CFOSlider->max - opt.m_CFOSlider->min),
			opt.m_CFOSlider->min, opt.m_CFOSlider->max);
		*(float*)opt.m_CFOSlider->value = newVal;
		if (newVal != oldVal && opt.m_CFOSlider->changeFunc)
			opt.m_CFOSlider->changeFunc(oldVal, newVal);
		break;
	}
#endif
	default:
		break;
	}
	s_menuDirtySettings = true;
}

static void
ClampMapCenter(void)
{
	const float minX = MenuX(162.0f);
	const float minY = MenuY(162.0f);
	const float spanX = Max(0.0f, FrontEndMenuManager.m_fMapSize - minX);
	const float spanY = Max(0.0f, FrontEndMenuManager.m_fMapSize - minY);
	FrontEndMenuManager.m_fMapCenterX = Clamp(FrontEndMenuManager.m_fMapCenterX,
		SCREEN_WIDTH * 0.5f - spanX, SCREEN_WIDTH * 0.5f + spanX);
	FrontEndMenuManager.m_fMapCenterY = Clamp(FrontEndMenuManager.m_fMapCenterY,
		SCREEN_HEIGHT * 0.5f - spanY, SCREEN_HEIGHT * 0.5f + spanY);
}

static void
ZoomPauseMap(float factor, float anchorX, float anchorY)
{
	const float minSize = MenuY(162.0f);
	const float maxSize = MenuY(1150.0f);
	const float oldSize = Max(FrontEndMenuManager.m_fMapSize, minSize);
	const float newSize = Clamp(oldSize * factor, minSize, maxSize);
	const float ratio = newSize / oldSize;

	FrontEndMenuManager.m_fMapSize = newSize;
	FrontEndMenuManager.m_fMapCenterX = anchorX - (anchorX - FrontEndMenuManager.m_fMapCenterX) * ratio;
	FrontEndMenuManager.m_fMapCenterY = anchorY - (anchorY - FrontEndMenuManager.m_fMapCenterY) * ratio;
	ClampMapCenter();
}

static void
ToggleWaypointAtScreenPos(float sx, float sy)
{
	const float mapSize = FrontEndMenuManager.m_fMapSize;
	if (mapSize <= 1.0f)
		return;
	if (sx < FrontEndMenuManager.m_fMapCenterX - mapSize || sx > FrontEndMenuManager.m_fMapCenterX + mapSize ||
	    sy < FrontEndMenuManager.m_fMapCenterY - mapSize || sy > FrontEndMenuManager.m_fMapCenterY + mapSize)
		return;

	const float diffX = FrontEndMenuManager.m_fMapCenterX - mapSize;
	const float diffY = FrontEndMenuManager.m_fMapCenterY - mapSize;
	const float mapSpan = MENU_MAP_LENGTH * MENU_MAP_LENGTH_UNIT; // 4000.0f VC map units
	const float worldX = ((sx - diffX) / (mapSize * 2.0f)) * (mapSpan / MENU_MAP_WIDTH_SCALE)
	                     - (mapSpan * 0.5f + MENU_MAP_LEFT_OFFSET * MENU_MAP_LENGTH_UNIT);
	const float worldY = (mapSpan * 0.5f - MENU_MAP_TOP_OFFSET * MENU_MAP_LENGTH_UNIT)
	                     - ((sy - diffY) / (mapSize * 2.0f)) * (mapSpan / MENU_MAP_HEIGHT_SCALE);
	CRadar::ToggleTargetMarker(worldX, worldY);
	DMAudio.PlayFrontEndSound(SOUND_FRONTEND_ENTER_OR_ADJUST, 0);
}

static MenuPointer*
FindMenuPointer(int64 fingerId)
{
	for (int i = 0; i < MAX_TOUCH_POINTERS; i++) {
		if (s_menuPointers[i].active && s_menuPointers[i].fingerId == fingerId)
			return &s_menuPointers[i];
	}
	return nil;
}

static MenuPointer*
AllocMenuPointer(int64 fingerId)
{
	MenuPointer *existing = FindMenuPointer(fingerId);
	if (existing)
		return existing;
	for (int i = 0; i < MAX_TOUCH_POINTERS; i++) {
		if (!s_menuPointers[i].active) {
			memset(&s_menuPointers[i], 0, sizeof(MenuPointer));
			s_menuPointers[i].active = true;
			s_menuPointers[i].fingerId = fingerId;
			s_menuPointers[i].downRow = -1;
			s_menuPointers[i].overlayBtn = MOBTN_NONE;
			return &s_menuPointers[i];
		}
	}
	return nil;
}

static int
CountActiveMenuPointers(MenuPointer **outA, MenuPointer **outB)
{
	int count = 0;
	for (int i = 0; i < MAX_TOUCH_POINTERS; i++) {
		if (s_menuPointers[i].active) {
			if (count == 0 && outA) *outA = &s_menuPointers[i];
			else if (count == 1 && outB) *outB = &s_menuPointers[i];
			count++;
		}
	}
	return count;
}

static bool
IsMenuOverlayHeld(MenuOverlayBtn btn)
{
	for (int i = 0; i < MAX_TOUCH_POINTERS; i++) {
		if (s_menuPointers[i].active && s_menuPointers[i].overlayBtn == btn)
			return true;
	}
	return false;
}

static void
OnMenuFingerDown(int64 fingerId, float x, float y)
{
	const MobileTouchMath::MobileScreenMetrics m = GetMetrics();
	MenuPointer *p = AllocMenuPointer(fingerId);
	if (!p)
		return;

	p->screenAtDown = FrontEndMenuManager.m_nCurrScreen;
	p->downTimeMs = CTimer::GetTimeInMillisecondsPauseMode();
	p->x = p->prevX = p->startX = x;
	p->y = p->prevY = p->startY = y;
	p->maxTravel = 0.0f;
	s_statsVelocityPxPerSec = 0.0f;

	if (FrontEndMenuManager.m_firstStartCounter < 255) {
		FrontEndMenuManager.m_firstStartCounter = 255;
		FrontEndMenuManager.m_nMenuFadeAlpha = 255;
		return;
	}

	MenuOverlayBtn ov = HitTestMenuOverlay(x, y, m.Dp(26.0f));
	if (ov != MOBTN_NONE) {
		p->overlayBtn = ov;
		return;
	}

	const int32 screen = FrontEndMenuManager.m_nCurrScreen;
	const int32 row = HitTestStandardMenuRow(x, y);
	p->downRow = row;

	if (row >= 0) {
		if (FrontEndMenuManager.m_nCurrOption != row) {
			FrontEndMenuManager.m_nCurrOption = row;
			if (FrontEndMenuManager.m_nTotalListRow > GetMenuMaxVisibleRows() && !MenuHasNativeList(screen))
				FrontEndMenuManager.m_nSelectedListRow = row;
			FrontEndMenuManager.m_nOptionHighlightTransitionBlend = 0;
			DMAudio.PlayFrontEndSound(SOUND_FRONTEND_HIGHLIGHT_OPTION, 0);
		}
		const int32 action = aScreens[screen].m_aEntries[row].m_Action;
		if (IsSliderMenuAction(action) && x >= MenuXLeft(MENUSLIDER_X - 18.0f)) {
			p->draggingSlider = true;
			ApplyDirectSliderValue(screen, row, x, 0.0f, true);
		}
	}
}

static void
OnMenuFingerMotion(int64 fingerId, float x, float y)
{
	MenuPointer *p = FindMenuPointer(fingerId);
	if (!p)
		return;
	if (p->screenAtDown != FrontEndMenuManager.m_nCurrScreen)
		return;

	const MobileTouchMath::MobileScreenMetrics m = GetMetrics();
	const float dx = x - p->prevX;
	const float dy = y - p->prevY;
	const float totalDX = x - p->startX;
	const float totalDY = y - p->startY;
	const float travel = Sqrt(totalDX * totalDX + totalDY * totalDY);
	if (travel > p->maxTravel)
		p->maxTravel = travel;

	const int32 screen = FrontEndMenuManager.m_nCurrScreen;

	// 1. Pause Map: 2-finger pinch-zoom + 1-finger 1:1 pan
	if (screen == MENUPAGE_MAP && p->overlayBtn == MOBTN_NONE) {
		MenuPointer *pa = nil, *pb = nil;
		const int nPtr = CountActiveMenuPointers(&pa, &pb);
		if (nPtr >= 2 && pa && pb) {
			pa->pinched = true;
			pb->pinched = true;
			const float oldAx = (pa == p) ? p->prevX : pa->x;
			const float oldAy = (pa == p) ? p->prevY : pa->y;
			const float oldBx = (pb == p) ? p->prevX : pb->x;
			const float oldBy = (pb == p) ? p->prevY : pb->y;
			const float newAx = (pa == p) ? x : pa->x;
			const float newAy = (pa == p) ? y : pa->y;
			const float newBx = (pb == p) ? x : pb->x;
			const float newBy = (pb == p) ? y : pb->y;

			MobileTouchMath::EvalMapPinch(
				oldAx, oldAy, oldBx, oldBy,
				newAx, newAy, newBx, newBy,
				m.Dp(18.0f), MenuY(162.0f), MenuY(1150.0f),
				&FrontEndMenuManager.m_fMapSize,
				&FrontEndMenuManager.m_fMapCenterX,
				&FrontEndMenuManager.m_fMapCenterY);
			ClampMapCenter();
		} else if (!p->pinched) {
			FrontEndMenuManager.m_fMapCenterX += dx;
			FrontEndMenuManager.m_fMapCenterY += dy;
			ClampMapCenter();
		}
		p->prevX = p->x = x;
		p->prevY = p->y = y;
		return;
	}

	p->prevX = p->x = x;
	p->prevY = p->y = y;

	if (p->overlayBtn != MOBTN_NONE)
		return;

	// 2. Left-edge swipe-to-go-back on sub-screens
	if (ShouldShowMenuBackOverlay(screen) && screen != MENUPAGE_PAUSE_MENU &&
	    p->startX <= m.safeLeftPx + m.Dp(26.0f) &&
	    totalDX >= m.Dp(52.0f) && Abs(totalDX) > Abs(totalDY) * 2.0f && !p->swipedHorizontal) {
		p->swipedHorizontal = true;
		s_menuPendingBack = true;
		return;
	}

	// 3. Stats screen: smooth 1:1 vertical drag scrolling + velocity tracking
	if (screen == MENUPAGE_STATS) {
		if (Abs(totalDY) >= m.Dp(6.0f)) {
			p->draggingScroll = true;
			s_statsScrollDeltaPx -= dy;
			const float dt = Max(CTimer::GetTimeStepInSeconds(), 1.0f / 120.0f);
			s_statsVelocityPxPerSec = -dy / dt;
		}
		return;
	}

	// 4. Direct continuous slider scrubbing
	if (p->downRow >= 0 && IsSliderMenuAction(aScreens[screen].m_aEntries[p->downRow].m_Action)) {
		if (p->draggingSlider || (Abs(totalDX) >= m.Dp(9.0f) && Abs(totalDX) > Abs(totalDY) * 1.15f)) {
			p->draggingSlider = true;
			const bool onSliderBar = (x >= MenuXLeft(MENUSLIDER_X - 18.0f));
			ApplyDirectSliderValue(screen, p->downRow, x, dx, onSliderBar);
			return;
		}
	}

	// 5. Sound Settings radio icon carousel horizontal swipe
	if (screen == MENUPAGE_SOUND_SETTINGS &&
	    p->startY >= MenuY(MENURADIO_SELECTOR_START_Y - 16.0f) &&
	    p->startY <= MenuY(MENURADIO_SELECTOR_START_Y + MENURADIO_SELECTOR_HEIGHT + 12.0f)) {
		if (!p->swipedHorizontal && Abs(totalDX) >= m.Dp(26.0f) && Abs(totalDX) > Abs(totalDY)) {
			p->swipedHorizontal = true;
			FrontEndMenuManager.ChangeRadioStation(totalDX < 0.0f ? 1 : -1);
			return;
		}
	}

	// 6. Vertical swipe scrolling on scrollable pages & native lists (Skin Select / Controls)
	const int32 maxVis = MenuHasNativeList(screen) ? 15 : GetMenuMaxVisibleRows();
	if (FrontEndMenuManager.m_nTotalListRow > maxVis || (screen == MENUPAGE_SKIN_SELECT && FrontEndMenuManager.m_nSkinsTotal > maxVis)) {
		const int32 totalRows = (screen == MENUPAGE_SKIN_SELECT) ? FrontEndMenuManager.m_nSkinsTotal : FrontEndMenuManager.m_nTotalListRow;
		if (totalRows > maxVis && (p->draggingScroll || (Abs(totalDY) >= m.Dp(12.0f) && Abs(totalDY) > Abs(totalDX)))) {
			p->draggingScroll = true;
			p->scrollResidualPx -= dy; // finger UP -> scroll list DOWN
			const float stepPx = m.Dp(24.0f);
			while (p->scrollResidualPx >= stepPx) {
				p->scrollResidualPx -= stepPx;
				if (FrontEndMenuManager.m_nFirstVisibleRowOnList < totalRows - maxVis) {
					FrontEndMenuManager.m_nFirstVisibleRowOnList++;
					FrontEndMenuManager.m_nSelectedListRow = Clamp(
						FrontEndMenuManager.m_nSelectedListRow + 1, 0, totalRows - 1);
					FrontEndMenuManager.m_nCurrOption = FrontEndMenuManager.m_nSelectedListRow;
					FrontEndMenuManager.m_nScrollbarTopMargin =
						(263.0f / (float)Max(totalRows, 1)) * FrontEndMenuManager.m_nFirstVisibleRowOnList;
				}
			}
			while (p->scrollResidualPx <= -stepPx) {
				p->scrollResidualPx += stepPx;
				if (FrontEndMenuManager.m_nFirstVisibleRowOnList > 0) {
					FrontEndMenuManager.m_nFirstVisibleRowOnList--;
					FrontEndMenuManager.m_nSelectedListRow = Clamp(
						FrontEndMenuManager.m_nSelectedListRow - 1, 0, totalRows - 1);
					FrontEndMenuManager.m_nCurrOption = FrontEndMenuManager.m_nSelectedListRow;
					FrontEndMenuManager.m_nScrollbarTopMargin =
						(263.0f / (float)Max(totalRows, 1)) * FrontEndMenuManager.m_nFirstVisibleRowOnList;
				}
			}
			return;
		}
	}

	// 7. Horizontal swipe on multi-choice / toggle row
	if (p->downRow >= 0 && !p->swipedHorizontal && !p->draggingScroll &&
	    Abs(totalDX) >= m.Dp(26.0f) && Abs(totalDX) > Abs(totalDY) * 1.4f) {
		p->swipedHorizontal = true;
		s_menuPendingChange = (totalDX > 0.0f) ? 1 : -1;
	}
}

static void
OnMenuFingerUp(int64 fingerId, float x, float y)
{
	MenuPointer *p = FindMenuPointer(fingerId);
	if (!p)
		return;

	const MobileTouchMath::MobileScreenMetrics m = GetMetrics();
	const bool sameScreen = (p->screenAtDown == FrontEndMenuManager.m_nCurrScreen);
	const bool isCleanTap = (p->maxTravel <= m.Dp(12.0f) &&
	                         !p->draggingScroll && !p->draggingSlider &&
	                         !p->swipedHorizontal && !p->pinched && !p->longPressedMap);

	if (s_menuDirtySettings) {
		const int32 curAct = aScreens[FrontEndMenuManager.m_nCurrScreen].m_aEntries[FrontEndMenuManager.m_nCurrOption].m_Action;
		if (curAct == MENUACTION_SFXVOLUME)
			DMAudio.PlayFrontEndSound(SOUND_FRONTEND_AUDIO_TEST, 0);
		else
			DMAudio.PlayFrontEndSound(SOUND_FRONTEND_ENTER_OR_ADJUST, 0);
		FrontEndMenuManager.SaveSettings();
		s_menuDirtySettings = false;
	}

	if (sameScreen && p->overlayBtn != MOBTN_NONE) {
		if (HitTestMenuOverlay(x, y, m.Dp(30.0f)) == p->overlayBtn) {
			switch (p->overlayBtn) {
			case MOBTN_BACK:
				if (FrontEndMenuManager.m_nCurrScreen == MENUPAGE_PAUSE_MENU) {
					DMAudio.PlayFrontEndSound(SOUND_FRONTEND_BACK, 0);
					FrontEndMenuManager.RequestFrontEndShutDown();
				} else {
					s_menuPendingBack = true;
				}
				break;
			case MOBTN_MAP_ZOOM_IN:
				ZoomPauseMap(1.32f, SCREEN_WIDTH * 0.5f, SCREEN_HEIGHT * 0.5f);
				break;
			case MOBTN_MAP_ZOOM_OUT:
				ZoomPauseMap(1.0f / 1.32f, SCREEN_WIDTH * 0.5f, SCREEN_HEIGHT * 0.5f);
				break;
			case MOBTN_MAP_LEGEND:
				FrontEndMenuManager.m_PrefsShowLegends = !FrontEndMenuManager.m_PrefsShowLegends;
				DMAudio.PlayFrontEndSound(SOUND_FRONTEND_ENTER_OR_ADJUST, 0);
				break;
			default:
				break;
			}
		}
		p->active = false;
		return;
	}

	if (sameScreen && isCleanTap) {
		const int32 screen = FrontEndMenuManager.m_nCurrScreen;

		if (screen == MENUPAGE_MAP) {
			// Tap bottom-left Back label OR double-tap map to place/clear Waypoint
			if (x >= MenuXLeft(40.0f) && x <= MenuXLeft(155.0f) &&
			    y >= MenuY(362.0f) && y <= MenuY(418.0f)) {
				s_menuPendingBack = true;
			} else {
				const uint32 nowMs = CTimer::GetTimeInMillisecondsPauseMode();
				const float ddx = x - s_lastMapTapX;
				const float ddy = y - s_lastMapTapY;
				if (nowMs - s_lastMapTapMs <= 340 && (ddx * ddx + ddy * ddy) <= m.Dp(24.0f) * m.Dp(24.0f)) {
					ToggleWaypointAtScreenPos(x, y);
					s_lastMapTapMs = 0;
				} else {
					s_lastMapTapMs = nowMs;
					s_lastMapTapX = x;
					s_lastMapTapY = y;
				}
			}
		} else if (screen == MENUPAGE_SOUND_SETTINGS &&
		           y >= MenuY(MENURADIO_SELECTOR_START_Y - 14.0f) &&
		           y <= MenuY(MENURADIO_SELECTOR_START_Y + MENURADIO_SELECTOR_HEIGHT - 4.0f)) {
			if (x < MenuXLeft(320.0f))
				FrontEndMenuManager.ChangeRadioStation(-1);
			else
				FrontEndMenuManager.ChangeRadioStation(1);
		} else if (screen == MENUPAGE_SKIN_SELECT) {
			if (x > MenuXLeft(PLAYERSETUP_LIST_LEFT) && x < MenuXRight(PLAYERSETUP_LIST_RIGHT) &&
			    y > MenuY(PLAYERSETUP_LIST_BODY_TOP) && y < SCREEN_SCALE_FROM_BOTTOM(PLAYERSETUP_LIST_BOTTOM)) {
				const int32 relRow = (int32)((y - MenuY(PLAYERSETUP_LIST_BODY_TOP)) / Max(MenuY(PLAYERSETUP_ROW_HEIGHT), 1.0f));
				const int32 targetRow = FrontEndMenuManager.m_nFirstVisibleRowOnList + relRow;
				if (targetRow >= 0 && targetRow < FrontEndMenuManager.m_nSkinsTotal) {
					if (FrontEndMenuManager.m_nSelectedListRow == targetRow) {
						strcpy(FrontEndMenuManager.m_PrefsSkinFile, FrontEndMenuManager.m_aSkinName);
						CWorld::Players[0].SetPlayerSkin(FrontEndMenuManager.m_PrefsSkinFile);
						FrontEndMenuManager.SaveSettings();
						DMAudio.PlayFrontEndSound(SOUND_FRONTEND_ENTER_OR_ADJUST, 0);
					} else {
						FrontEndMenuManager.m_nSelectedListRow = targetRow;
						DMAudio.PlayFrontEndSound(SOUND_FRONTEND_HIGHLIGHT_OPTION, 0);
					}
				}
			} else if (y >= SCREEN_SCALE_FROM_BOTTOM(PLAYERSETUP_LIST_BOTTOM + 6.0f)) {
				if (x > MenuXRight(160.0f)) {
					s_menuPendingBack = true;
				} else if (FrontEndMenuManager.m_nSkinsTotal > 0) {
					strcpy(FrontEndMenuManager.m_PrefsSkinFile, FrontEndMenuManager.m_aSkinName);
					CWorld::Players[0].SetPlayerSkin(FrontEndMenuManager.m_PrefsSkinFile);
					FrontEndMenuManager.SaveSettings();
					DMAudio.PlayFrontEndSound(SOUND_FRONTEND_ENTER_OR_ADJUST, 0);
				}
			}
		} else {
			const int32 upRow = HitTestStandardMenuRow(x, y);
			if (upRow >= 0 && upRow == p->downRow) {
				FrontEndMenuManager.m_nCurrOption = upRow;
				const int32 action = aScreens[screen].m_aEntries[upRow].m_Action;
				if (IsCycleChoiceAction(action)) {
					s_menuPendingChange = 1;
				} else if (!IsSliderMenuAction(action)) {
					s_menuPendingSelect = true;
				}
			}
		}
	}

	p->active = false;
}

// ---------------------------------------------------------------------------
// 2D Rendering (Textured Sprites + Clean Circular Vector Fallbacks)
// ---------------------------------------------------------------------------

static void
SetVertex2D(RwIm2DVertex *v, float x, float y, const CRGBA &c, float tu = 0.0f, float tv = 0.0f)
{
	const float nearCamZ = Scene.camera ? RwCameraGetNearClipPlane(Scene.camera) : 0.9f;
	const float recipNear = (nearCamZ != 0.0f) ? (1.0f / nearCamZ) : 1.0f;
	const float nearScreenZ = RwIm2DGetNearScreenZ();

	RwIm2DVertexSetScreenX(v, x);
	RwIm2DVertexSetScreenY(v, y);
	RwIm2DVertexSetScreenZ(v, nearScreenZ);
	RwIm2DVertexSetCameraZ(v, nearCamZ);
	RwIm2DVertexSetRecipCameraZ(v, recipNear);
	RwIm2DVertexSetIntRGBA(v, c.r, c.g, c.b, c.a);
	RwIm2DVertexSetU(v, tu, recipNear);
	RwIm2DVertexSetV(v, tv, recipNear);
}

static void
DrawFilledCircle(float cx, float cy, float radius, const CRGBA &fillCol, const CRGBA &ringCol, float ringThickness)
{
	static const int SEGMENTS = 28;
	RwIm2DVertex fan[SEGMENTS + 2];
	RwRenderStateSet(rwRENDERSTATETEXTURERASTER, nil);

	SetVertex2D(&fan[0], cx, cy, fillCol);
	for (int i = 0; i <= SEGMENTS; i++) {
		const float ang = (float)i * (2.0f * PI / (float)SEGMENTS);
		SetVertex2D(&fan[i + 1], cx + Cos(ang) * radius, cy + Sin(ang) * radius, fillCol);
	}
	RwIm2DRenderPrimitive(rwPRIMTYPETRIFAN, fan, SEGMENTS + 2);

	if (ringCol.a > 0 && ringThickness > 0.5f) {
		RwIm2DVertex strip[(SEGMENTS + 1) * 2];
		const float rIn = Max(0.0f, radius - ringThickness);
		for (int i = 0; i <= SEGMENTS; i++) {
			const float ang = (float)i * (2.0f * PI / (float)SEGMENTS);
			const float ca = Cos(ang);
			const float sa = Sin(ang);
			SetVertex2D(&strip[i * 2 + 0], cx + ca * rIn, cy + sa * rIn, ringCol);
			SetVertex2D(&strip[i * 2 + 1], cx + ca * radius, cy + sa * radius, ringCol);
		}
		RwIm2DRenderPrimitive(rwPRIMTYPETRISTRIP, strip, (SEGMENTS + 1) * 2);
	}
}

static void
DrawThickSegment(float x1, float y1, float x2, float y2, float thickness, const CRGBA &col)
{
	const float dx = x2 - x1;
	const float dy = y2 - y1;
	const float len = Sqrt(dx * dx + dy * dy);
	if (len < 0.01f)
		return;
	const float nx = (-dy / len) * (thickness * 0.5f);
	const float ny = (dx / len) * (thickness * 0.5f);

	RwIm2DVertex quad[4];
	RwRenderStateSet(rwRENDERSTATETEXTURERASTER, nil);
	SetVertex2D(&quad[0], x1 + nx, y1 + ny, col);
	SetVertex2D(&quad[1], x2 + nx, y2 + ny, col);
	SetVertex2D(&quad[2], x2 - nx, y2 - ny, col);
	SetVertex2D(&quad[3], x1 - nx, y1 - ny, col);
	RwIm2DRenderPrimitive(rwPRIMTYPETRIFAN, quad, 4);
}

static void
DrawFallbackGlyph(FallbackGlyph glyph, float cx, float cy, float r, const CRGBA &col)
{
	const float t = Max(2.0f, r * 0.11f);
	switch (glyph) {
	case GLYPH_PAUSE:
		DrawThickSegment(cx - r * 0.22f, cy - r * 0.36f, cx - r * 0.22f, cy + r * 0.36f, t * 1.4f, col);
		DrawThickSegment(cx + r * 0.22f, cy - r * 0.36f, cx + r * 0.22f, cy + r * 0.36f, t * 1.4f, col);
		break;
	case GLYPH_CAMERA:
		DrawFilledCircle(cx, cy, r * 0.26f, CRGBA(0, 0, 0, 0), col, t);
		DrawThickSegment(cx - r * 0.42f, cy - r * 0.28f, cx + r * 0.42f, cy - r * 0.28f, t, col);
		DrawThickSegment(cx - r * 0.42f, cy + r * 0.30f, cx + r * 0.42f, cy + r * 0.30f, t, col);
		DrawThickSegment(cx - r * 0.42f, cy - r * 0.28f, cx - r * 0.42f, cy + r * 0.30f, t, col);
		DrawThickSegment(cx + r * 0.42f, cy - r * 0.28f, cx + r * 0.42f, cy + r * 0.30f, t, col);
		break;
	case GLYPH_FIST:
		DrawFilledCircle(cx, cy, r * 0.34f, col, col, 0.0f);
		break;
	case GLYPH_CROSSHAIR:
	case GLYPH_TARGET:
		DrawFilledCircle(cx, cy, r * 0.38f, CRGBA(0, 0, 0, 0), col, t);
		DrawThickSegment(cx - r * 0.54f, cy, cx - r * 0.16f, cy, t, col);
		DrawThickSegment(cx + r * 0.16f, cy, cx + r * 0.54f, cy, t, col);
		DrawThickSegment(cx, cy - r * 0.54f, cx, cy - r * 0.16f, t, col);
		DrawThickSegment(cx, cy + r * 0.16f, cx, cy + r * 0.54f, t, col);
		break;
	case GLYPH_GRENADE:
		DrawFilledCircle(cx, cy + r * 0.08f, r * 0.30f, col, col, 0.0f);
		DrawThickSegment(cx, cy - r * 0.22f, cx + r * 0.18f, cy - r * 0.42f, t, col);
		break;
	case GLYPH_SPRINT:
		DrawThickSegment(cx - r * 0.30f, cy - r * 0.32f, cx + r * 0.08f, cy, t * 1.2f, col);
		DrawThickSegment(cx + r * 0.08f, cy, cx - r * 0.30f, cy + r * 0.32f, t * 1.2f, col);
		DrawThickSegment(cx + r * 0.02f, cy - r * 0.32f, cx + r * 0.40f, cy, t * 1.2f, col);
		DrawThickSegment(cx + r * 0.40f, cy, cx + r * 0.02f, cy + r * 0.32f, t * 1.2f, col);
		break;
	case GLYPH_JUMP:
		DrawThickSegment(cx, cy - r * 0.40f, cx - r * 0.34f, cy + r * 0.02f, t * 1.2f, col);
		DrawThickSegment(cx, cy - r * 0.40f, cx + r * 0.34f, cy + r * 0.02f, t * 1.2f, col);
		DrawThickSegment(cx, cy - r * 0.36f, cx, cy + r * 0.42f, t * 1.2f, col);
		break;
	case GLYPH_PLUS:
		DrawThickSegment(cx - r * 0.36f, cy, cx + r * 0.36f, cy, t * 1.3f, col);
		DrawThickSegment(cx, cy - r * 0.36f, cx, cy + r * 0.36f, t * 1.3f, col);
		break;
	case GLYPH_MINUS:
		DrawThickSegment(cx - r * 0.36f, cy, cx + r * 0.36f, cy, t * 1.3f, col);
		break;
	case GLYPH_CAR:
		DrawThickSegment(cx - r * 0.42f, cy + r * 0.12f, cx + r * 0.42f, cy + r * 0.12f, t * 1.6f, col);
		DrawThickSegment(cx - r * 0.26f, cy - r * 0.18f, cx + r * 0.26f, cy - r * 0.18f, t * 1.3f, col);
		DrawFilledCircle(cx - r * 0.26f, cy + r * 0.28f, r * 0.13f, col, col, 0.0f);
		DrawFilledCircle(cx + r * 0.26f, cy + r * 0.28f, r * 0.13f, col, col, 0.0f);
		break;
	case GLYPH_EXIT:
		DrawThickSegment(cx - r * 0.36f, cy, cx + r * 0.30f, cy, t * 1.2f, col);
		DrawThickSegment(cx + r * 0.04f, cy - r * 0.28f, cx + r * 0.34f, cy, t * 1.2f, col);
		DrawThickSegment(cx + r * 0.04f, cy + r * 0.28f, cx + r * 0.34f, cy, t * 1.2f, col);
		break;
	case GLYPH_ARROW_LEFT:
		DrawThickSegment(cx + r * 0.20f, cy - r * 0.36f, cx - r * 0.24f, cy, t * 1.4f, col);
		DrawThickSegment(cx - r * 0.24f, cy, cx + r * 0.20f, cy + r * 0.36f, t * 1.4f, col);
		break;
	case GLYPH_ARROW_RIGHT:
		DrawThickSegment(cx - r * 0.20f, cy - r * 0.36f, cx + r * 0.24f, cy, t * 1.4f, col);
		DrawThickSegment(cx + r * 0.24f, cy, cx - r * 0.20f, cy + r * 0.36f, t * 1.4f, col);
		break;
	case GLYPH_PEDAL_GAS:
		DrawThickSegment(cx, cy - r * 0.55f, cx, cy + r * 0.55f, r * 0.52f, col);
		break;
	case GLYPH_PEDAL_BRAKE:
		DrawThickSegment(cx - r * 0.34f, cy, cx + r * 0.34f, cy, r * 0.56f, col);
		break;
	case GLYPH_HANDBRAKE:
		DrawFilledCircle(cx, cy, r * 0.42f, CRGBA(0, 0, 0, 0), col, t * 1.2f);
		DrawThickSegment(cx, cy - r * 0.22f, cx, cy + r * 0.06f, t * 1.3f, col);
		DrawFilledCircle(cx, cy + r * 0.22f, t * 0.7f, col, col, 0.0f);
		break;
	case GLYPH_HORN:
		DrawThickSegment(cx - r * 0.32f, cy, cx + r * 0.24f, cy - r * 0.26f, t * 1.2f, col);
		DrawThickSegment(cx - r * 0.32f, cy, cx + r * 0.24f, cy + r * 0.26f, t * 1.2f, col);
		DrawThickSegment(cx + r * 0.24f, cy - r * 0.28f, cx + r * 0.24f, cy + r * 0.28f, t * 1.2f, col);
		break;
	case GLYPH_STEER_MODE:
		DrawFilledCircle(cx, cy, r * 0.40f, CRGBA(0, 0, 0, 0), col, t * 1.1f);
		DrawThickSegment(cx - r * 0.38f, cy, cx + r * 0.38f, cy, t, col);
		DrawThickSegment(cx, cy, cx, cy + r * 0.38f, t, col);
		break;
	case GLYPH_DOTS:
		DrawFilledCircle(cx - r * 0.34f, cy, r * 0.11f, col, col, 0.0f);
		DrawFilledCircle(cx,             cy, r * 0.11f, col, col, 0.0f);
		DrawFilledCircle(cx + r * 0.34f, cy, r * 0.11f, col, col, 0.0f);
		break;
	case GLYPH_FLY:
		DrawThickSegment(cx - r * 0.42f, cy + r * 0.08f, cx + r * 0.42f, cy - r * 0.08f, t * 1.2f, col);
		DrawThickSegment(cx - r * 0.05f, cy - r * 0.38f, cx + r * 0.12f, cy + r * 0.38f, t * 1.2f, col);
		break;
	case GLYPH_LEGEND:
		DrawThickSegment(cx - r * 0.34f, cy - r * 0.26f, cx + r * 0.34f, cy - r * 0.26f, t * 1.2f, col);
		DrawThickSegment(cx - r * 0.34f, cy,             cx + r * 0.34f, cy,             t * 1.2f, col);
		DrawThickSegment(cx - r * 0.34f, cy + r * 0.26f, cx + r * 0.34f, cy + r * 0.26f, t * 1.2f, col);
		break;
	case GLYPH_PERSON:
		DrawFilledCircle(cx, cy - r * 0.24f, r * 0.21f, col, col, 0.0f);
		DrawThickSegment(cx - r * 0.34f, cy + r * 0.30f, cx - r * 0.14f, cy + r * 0.06f, t * 1.35f, col);
		DrawThickSegment(cx - r * 0.16f, cy + r * 0.08f, cx + r * 0.16f, cy + r * 0.08f, t * 1.45f, col);
		DrawThickSegment(cx + r * 0.14f, cy + r * 0.06f, cx + r * 0.34f, cy + r * 0.30f, t * 1.35f, col);
		break;
	case GLYPH_CLOSE:
		DrawThickSegment(cx - r * 0.30f, cy - r * 0.30f, cx + r * 0.30f, cy + r * 0.30f, t * 1.35f, col);
		DrawThickSegment(cx - r * 0.30f, cy + r * 0.30f, cx + r * 0.30f, cy - r * 0.30f, t * 1.35f, col);
		break;
	default:
		break;
	}
}

static void
DrawTextureQuad(RwTexture *tex, float cx, float cy, float halfW, float halfH, const CRGBA &col, bool preserveAspect)
{
	RwRaster *ras = RwTextureGetRaster(tex);
	if (!ras)
		return;

	if (preserveAspect) {
		const int32 tw = RwRasterGetWidth(ras);
		const int32 th = RwRasterGetHeight(ras);
		if (tw > 0 && th > 0) {
			const float texAspect = (float)tw / (float)th;
			const float boxAspect = halfW / Max(halfH, 1.0f);
			if (texAspect < boxAspect)
				halfW = halfH * texAspect;
			else
				halfH = halfW / texAspect;
		}
	}

	RwIm2DVertex quad[4];
	RwRenderStateSet(rwRENDERSTATETEXTURERASTER, ras);
	SetVertex2D(&quad[0], cx - halfW, cy - halfH, col, 0.0f, 0.0f);
	SetVertex2D(&quad[1], cx + halfW, cy - halfH, col, 1.0f, 0.0f);
	SetVertex2D(&quad[2], cx + halfW, cy + halfH, col, 1.0f, 1.0f);
	SetVertex2D(&quad[3], cx - halfW, cy + halfH, col, 0.0f, 1.0f);
	RwIm2DRenderPrimitive(rwPRIMTYPETRIFAN, quad, 4);
}

static void
DrawVirtualButton(const VirtualButton &b, bool active, uint8 idleAlpha = 195)
{
	const float scale = active ? 0.92f : 1.0f;
	const uint8 alpha = active ? 255 : idleAlpha;
	RwTexture *tex = (b.texId >= 0 && b.texId < MTEX_COUNT) ? s_tex[b.texId] : nil;

	if (tex) {
		const CRGBA tint = active ? CRGBA(255, 215, 245, alpha) : CRGBA(255, 255, 255, alpha);
		DrawTextureQuad(tex, b.cx, b.cy, b.halfW * scale, b.halfH * scale, tint, b.preserveAspect);
	} else {
		const float rad = Min(b.halfW, b.halfH) * scale;
		const CRGBA bg = active ? CRGBA(255, 150, 225, 150) : CRGBA(14, 18, 26, (uint8)(alpha * 0.50f));
		const CRGBA ring = active ? CRGBA(255, 235, 250, 240) : CRGBA(235, 240, 250, (uint8)(alpha * 0.82f));
		DrawFilledCircle(b.cx, b.cy, rad, bg, ring, Max(2.0f, rad * 0.065f));
		DrawFallbackGlyph(b.glyph, b.cx, b.cy, rad, CRGBA(250, 252, 255, alpha));
	}
}

} // namespace

// ---------------------------------------------------------------------------
// Public API + Android JNI Entry Point
// ---------------------------------------------------------------------------

#ifdef ANDROID
extern "C" JNIEXPORT void JNICALL
Java_org_libsdl_app_SDLActivity_nativeSetMobileDisplayMetrics(
	JNIEnv *env, jclass clazz,
	jfloat density, jfloat xdpi, jfloat ydpi,
	jint safeLeft, jint safeTop, jint safeRight, jint safeBottom)
{
	(void)env;
	(void)clazz;
	CMobileTouchControls::SetDeviceDisplayMetrics(
		(float)density, (float)xdpi, (float)ydpi,
		(int32)safeLeft, (int32)safeTop, (int32)safeRight, (int32)safeBottom);
}
#endif

void
CMobileTouchControls::SetDeviceDisplayMetrics(float density, float xdpi, float ydpi,
                                              int32 safeLeft, int32 safeTop, int32 safeRight, int32 safeBottom)
{
	s_deviceDensity = density;
	s_deviceXdpi = xdpi;
	s_deviceYdpi = ydpi;
	s_safeLeft = safeLeft;
	s_safeTop = safeTop;
	s_safeRight = safeRight;
	s_safeBottom = safeBottom;
}

void
CMobileTouchControls::SetFallbackDpi(float dpi)
{
	if (dpi > 40.0f)
		s_fallbackDpi = dpi;
}

float
CMobileTouchControls::Dp(float dp)
{
	return GetMetrics().Dp(dp);
}

float
CMobileTouchControls::ScaleHudPx(float ps2Units)
{
	return GetMetrics().hudUnitPx * ps2Units;
}

float
CMobileTouchControls::GetSafeLeftInsetPx(void)
{
	return GetMetrics().safeLeftPx;
}

float
CMobileTouchControls::GetSafeRightInsetPx(void)
{
	return GetMetrics().safeRightPx;
}

float
CMobileTouchControls::GetSafeTopInsetPx(void)
{
	return GetMetrics().safeTopPx;
}

float
CMobileTouchControls::GetSafeBottomInsetPx(void)
{
	return GetMetrics().safeBottomPx;
}

void
CMobileTouchControls::GetMobileRadarRect(float *outLeft, float *outTop, float *outWidth, float *outHeight)
{
	const MobileTouchMath::MobileScreenMetrics m = GetMetrics();
	if (outLeft)   *outLeft   = m.radarLeftPx;
	if (outTop)    *outTop    = m.radarTopPx;
	if (outWidth)  *outWidth  = m.radarSizePx;
	if (outHeight) *outHeight = m.radarSizePx;
}

float
CMobileTouchControls::MenuVirtualToScreenX(float vx)
{
	const MobileTouchMath::MobileScreenMetrics m = GetMetrics();
	return MobileTouchMath::MenuVirtualToScreenX(vx, m.screenW, m.screenH, m.safeLeftPx, m.safeRightPx);
}

float
CMobileTouchControls::GetMenuSpanWidthPx(void)
{
	const MobileTouchMath::MobileScreenMetrics m = GetMetrics();
	float left = 0.0f, right = m.screenW;
	MobileTouchMath::ComputeMenuHorizontalSpan(m.screenW, m.screenH, m.safeLeftPx, m.safeRightPx, &left, &right);
	return Max(1.0f, right - left);
}

bool
CMobileTouchControls::IsLookPointerActive(void)
{
	for (int i = 0; i < MAX_TOUCH_POINTERS; i++) {
		if (s_pointers[i].active && (s_pointers[i].isLook || s_pointers[i].btn == MBTN_ATTACK || s_pointers[i].btn == MBTN_TARGET))
			return true;
	}
	return false;
}

void
CMobileTouchControls::Initialise(void)
{
	ResetTouches();
	TryLoadMobileHudTxd();
}

void
CMobileTouchControls::ReloadTXD(void)
{
	Shutdown();
	Initialise();
}

void
CMobileTouchControls::Shutdown(void)
{
	ResetTouches();
	memset(s_tex, 0, sizeof(s_tex));
	memset(s_weaponIcons, 0, sizeof(s_weaponIcons));
	s_hasCustomTxd = false;

	if (s_mobileTxdSlot >= 0) {
		CTxdStore::RemoveTxdSlot(s_mobileTxdSlot);
		s_mobileTxdSlot = -1;
	}
}

bool
CMobileTouchControls::HasCustomHudTxd(void)
{
	return s_hasCustomTxd;
}

RwTexture*
CMobileTouchControls::GetWeaponIcon(int32 weaponType, const char *modelName)
{
	RwTexDictionary *dict = GetMobileDict();
	if (!dict)
		return nil;

	if (weaponType >= 0 && weaponType < WEAPONTYPE_TOTALWEAPONS && s_weaponIcons[weaponType])
		return s_weaponIcons[weaponType];

	if (modelName && *modelName)
		return FindInDict(dict, modelName);

	return nil;
}

void
CMobileTouchControls::UpgradeHudSprites(CSprite2d *hudSprites, int32 numSprites)
{
	RwTexDictionary *dict = GetMobileDict();
	if (!dict || !hudSprites)
		return;

	for (int32 w = 0; w < WEAPONTYPE_TOTALWEAPONS && w < numSprites; w++) {
		if (s_weaponIcons[w])
			ReplaceSpriteTex(hudSprites[w], s_weaponIcons[w]);
	}

	struct HudOverride {
		int32 index;
		const char *texName;
	};
	static const HudOverride kOverrides[] = {
		{ HUD_SITEROCKET, "hud_siterocket" },
		{ HUD_RADARDISC,  "hud_radardisc" },
		{ HUD_SITESNIPER, "hud_sitesniper" },
		{ HUD_SITEM16,    "hud_siteM16" },
		{ HUD_SITELASER,  "hud_sitelaser" },
		{ HUD_LASERDOT,   "hud_laserdot" },
		{ HUD_VIEWFINDER, "hud_viewfinder_128" }
	};

	for (size_t i = 0; i < ARRAY_SIZE(kOverrides); i++) {
		if (kOverrides[i].index < numSprites) {
			RwTexture *t = FindInDict(dict, kOverrides[i].texName);
			if (t)
				ReplaceSpriteTex(hudSprites[kOverrides[i].index], t);
		}
	}
}

void
CMobileTouchControls::UpgradeRadarSprites(void)
{
	RwTexDictionary *dict = GetMobileDict();
	if (!dict)
		return;

	struct BlipOverride {
		int32 blipId;
		const char *texName;
	};
	static const BlipOverride kBlips[] = {
		{ RADAR_SPRITE_CENTRE,          "hud_radar_centre" },
		{ RADAR_SPRITE_MAP_HERE,        "hud_arrow" },
		{ RADAR_SPRITE_NORTH,           "hud_radar_north" },
		{ RADAR_SPRITE_AVERY,           "hud_radar_avery" },
		{ RADAR_SPRITE_BIKER,           "hud_radar_biker" },
		{ RADAR_SPRITE_CORTEZ,          "hud_radar_cortez" },
		{ RADAR_SPRITE_DIAZ,            "hud_radar_diaz" },
		{ RADAR_SPRITE_KENT,            "hud_radar_kent" },
		{ RADAR_SPRITE_LAWYER,          "hud_radar_lawyer" },
		{ RADAR_SPRITE_PHIL,            "hud_radar_phil" },
		{ RADAR_SPRITE_BIKERS,          "hud_bikers" },
		{ RADAR_SPRITE_BOATYARD,        "hud_boatyard" },
		{ RADAR_SPRITE_MALIBU_CLUB,     "hud_club" },
		{ RADAR_SPRITE_CUBANS,          "hud_cubans" },
		{ RADAR_SPRITE_FILM,            "hud_filmstudio" },
		{ RADAR_SPRITE_GUN,             "hud_gun" },
		{ RADAR_SPRITE_HAITIANS,        "hud_haitians" },
		{ RADAR_SPRITE_HARDWARE,        "hud_hardware" },
		{ RADAR_SPRITE_SAVE,            "hud_radar_save" },
		{ RADAR_SPRITE_STRIP,           "hud_radar_strip" },
		{ RADAR_SPRITE_ICE,             "hud_icecream" },
		{ RADAR_SPRITE_KCABS,           "hud_kcabs" },
		{ RADAR_SPRITE_LOVEFIST,        "hud_lovefist" },
		{ RADAR_SPRITE_PRINTWORKS,      "hud_printworks" },
		{ RADAR_SPRITE_PROPERTY,        "hud_property" },
		{ RADAR_SPRITE_SUNYARD,         "hud_SunYard" },
		{ RADAR_SPRITE_SPRAY,           "hud_spray" },
		{ RADAR_SPRITE_TSHIRT,          "hud_tshirt" },
		{ RADAR_SPRITE_TOMMY,           "hud_tommy" },
		{ RADAR_SPRITE_PHONE,           "hud_phone" },
		{ RADAR_SPRITE_RADIO_WILDSTYLE, "hud_RWildstyle" },
		{ RADAR_SPRITE_RADIO_FLASH,     "hud_RFlash" },
		{ RADAR_SPRITE_RADIO_KCHAT,     "hud_RKchat" },
		{ RADAR_SPRITE_RADIO_FEVER,     "hud_RFever" },
		{ RADAR_SPRITE_RADIO_VROCK,     "hud_RVRock" },
		{ RADAR_SPRITE_RADIO_VCPR,      "hud_RVCPR" },
		{ RADAR_SPRITE_RADIO_ESPANTOSO, "hud_REspantoso" },
		{ RADAR_SPRITE_RADIO_EMOTION,   "hud_REmotion" },
		{ RADAR_SPRITE_RADIO_WAVE,      "hud_RWave" }
	};

	for (size_t i = 0; i < ARRAY_SIZE(kBlips); i++) {
		const int32 id = kBlips[i].blipId;
		if (id > 0 && id < RADAR_SPRITE_COUNT && CRadar::RadarSprites[id]) {
			RwTexture *t = FindInDict(dict, kBlips[i].texName);
			if (t)
				ReplaceSpriteTex(*CRadar::RadarSprites[id], t);
		}
	}
}

void
CMobileTouchControls::ResetTouches(void)
{
	memset(s_pointers, 0, sizeof(s_pointers));
	memset(s_menuPointers, 0, sizeof(s_menuPointers));
	s_stickAxisX = 0;
	s_stickAxisY = 0;
	s_accumLookDX = 0.0f;
	s_accumLookDY = 0.0f;
	s_menuPendingSelect = false;
	s_menuPendingBack = false;
	s_menuPendingChange = 0;
	s_statsVelocityPxPerSec = 0.0f;
	s_sprintJumpTap.Reset();
	s_spawnerPtr.active = false;
	s_spawnerMode = mobilespawner::PANEL_NONE;
}

void
CMobileTouchControls::OnFingerDown(int64 fingerId, float normX, float normY)
{
#ifdef ANDROID
	const MobileTouchMath::MobileScreenMetrics m = GetMetrics();
	const float x = normX * m.screenW;
	const float y = normY * m.screenH;

	if (FrontEndMenuManager.m_bMenuActive) {
		memset(s_pointers, 0, sizeof(s_pointers));
		s_spawnerMode = mobilespawner::PANEL_NONE;
		s_spawnerPtr.active = false;
		OnMenuFingerDown(fingerId, x, y);
		return;
	}

	if (gGameState != GS_PLAYING_GAME)
		return;

	if (CCutsceneMgr::IsRunning() || TheCamera.m_WideScreenOn) {
		s_pulseSkipCutscene = true;
		return;
	}

	UpdateGameplayLayout();

	// 0. If Skin / Vehicle picker panel is open, intercept touches inside it first
	if (s_spawnerMode != mobilespawner::PANEL_NONE) {
		const MobileBtnId topHit = FindHitGameplayButton(x, y, m.Dp(24.0f));
		if (topHit == MBTN_DEV_SKIN || topHit == MBTN_DEV_VEHICLE || topHit == MBTN_PAUSE) {
			s_spawnerPtr.active = false;
		} else {
			const mobilespawner::PickerLayout L = GetCurrentSpawnerLayout(m);
			const mobilespawner::PanelHit ph = mobilespawner::HitTestPicker(L, x, y);
			if (ph == mobilespawner::PHIT_OUTSIDE) {
				s_spawnerMode = mobilespawner::PANEL_NONE;
				s_spawnerPtr.active = false;
				return;
			}
			s_spawnerPtr.active = true;
			s_spawnerPtr.fingerId = fingerId;
			s_spawnerPtr.startX = x;
			s_spawnerPtr.startY = y;
			s_spawnerPtr.prevY = y;
			s_spawnerPtr.maxTravel = 0.0f;
			s_spawnerPtr.scrollResidualPx = 0.0f;
			s_spawnerPtr.draggingScroll = false;
			s_spawnerPtr.downHit = ph;
			return;
		}
	}

	ActivePointer *p = AllocGameplayPointer(fingerId);
	if (!p)
		return;
	p->x = p->startX = x;
	p->y = p->startY = y;

	// 1. Explicit virtual buttons (minimum 24dp radius = 48dp touch target)
	MobileBtnId hitBtn = FindHitGameplayButton(x, y, m.Dp(24.0f));
	if (hitBtn != MBTN_NONE) {
		p->btn = hitBtn;
		HandleGameplayButtonDownPulse(hitBtn);
		return;
	}

	// 2. Dynamic Transparent Stick: invisible when idle, spawns at touch point in left movement zone
	if (s_stickVisible && !IsStickOwned()) {
		if (MobileTouchMath::IsInDynamicStickZone(x, y, m)) {
			p->isStick = true;
			MobileTouchMath::ComputeDynamicStickOrigin(x, y, m, s_stickRadius, &s_stickAnchorX, &s_stickAnchorY);
			s_stickBaseX = s_stickAnchorX;
			s_stickBaseY = s_stickAnchorY;
			UpdateStickFromTouch(x, y);

			const uint32 nowMs = CTimer::GetTimeInMilliseconds();
			if (nowMs - s_lastStickTapMs < 280 && !FindPlayerVehicle())
				s_pulseCrouch = true;
			s_lastStickTapMs = nowMs;
			return;
		}
	}

	// 3. Free-look camera anywhere else on screen
	p->isLook = true;
#else
	(void)fingerId; (void)normX; (void)normY;
#endif
}

void
CMobileTouchControls::OnFingerMotion(int64 fingerId, float normX, float normY, float normDX, float normDY)
{
#ifdef ANDROID
	const MobileTouchMath::MobileScreenMetrics m = GetMetrics();
	const float x = normX * m.screenW;
	const float y = normY * m.screenH;

	if (FrontEndMenuManager.m_bMenuActive) {
		OnMenuFingerMotion(fingerId, x, y);
		return;
	}

	if (s_spawnerMode != mobilespawner::PANEL_NONE &&
	    s_spawnerPtr.active && s_spawnerPtr.fingerId == fingerId) {
		const float dx = x - s_spawnerPtr.startX;
		const float dy = y - s_spawnerPtr.startY;
		const float dist = Sqrt(dx * dx + dy * dy);
		if (dist > s_spawnerPtr.maxTravel)
			s_spawnerPtr.maxTravel = dist;

		std::vector<mobilespawner::CatalogItem> &cat =
			(s_spawnerMode == mobilespawner::PANEL_SKIN) ? s_skinCatalog : s_vehCatalog;
		int32 &firstRow = (s_spawnerMode == mobilespawner::PANEL_SKIN) ? s_skinFirstRow : s_vehFirstRow;
		const float stepDy = y - s_spawnerPtr.prevY;
		s_spawnerPtr.prevY = y;

		if (s_spawnerPtr.draggingScroll || Abs(dy) >= m.Dp(10.0f)) {
			s_spawnerPtr.draggingScroll = true;
			s_spawnerPtr.scrollResidualPx -= stepDy;
			const float rowStep = m.Dp(26.0f);
			const int32 maxFirst = Max(0, (int32)cat.size() - 6);
			while (s_spawnerPtr.scrollResidualPx >= rowStep) {
				s_spawnerPtr.scrollResidualPx -= rowStep;
				if (firstRow < maxFirst)
					firstRow++;
			}
			while (s_spawnerPtr.scrollResidualPx <= -rowStep) {
				s_spawnerPtr.scrollResidualPx += rowStep;
				if (firstRow > 0)
					firstRow--;
			}
		}
		return;
	}

	ActivePointer *p = FindGameplayPointer(fingerId);
	if (!p)
		return;

	p->x = x;
	p->y = y;

	if (p->isStick) {
		UpdateStickFromTouch(x, y);
		return;
	}

	// Seamless thumb-slide between Left and Right steering buttons without lifting
	if (p->btn == MBTN_STEER_LEFT || p->btn == MBTN_STEER_RIGHT) {
		if (HitTestButton(s_buttons[MBTN_STEER_LEFT], x, y, m.Dp(26.0f)))
			p->btn = MBTN_STEER_LEFT;
		else if (HitTestButton(s_buttons[MBTN_STEER_RIGHT], x, y, m.Dp(26.0f)))
			p->btn = MBTN_STEER_RIGHT;
		return;
	}

	// DP-calibrated camera free-look & aim-while-holding Attack/Target
	if (p->isLook || p->btn == MBTN_ATTACK || p->btn == MBTN_TARGET) {
		const float deltaDpX = (normDX * m.screenW) / Max(m.pxPerDp, 0.5f);
		const float deltaDpY = (normDY * m.screenH) / Max(m.pxPerDp, 0.5f);
		const float aimFactor = (p->btn == MBTN_ATTACK) ? 1.55f : 2.10f;
		s_accumLookDX += deltaDpX * aimFactor;
		s_accumLookDY += deltaDpY * aimFactor;
		return;
	}

	// Horizontal swipe on top-right weapon slot cycles previous / next weapon
	if (p->btn == MBTN_WEAPON_SLOT && !p->swipedWeapon) {
		const float deltaX = p->x - p->startX;
		if (Abs(deltaX) >= m.Dp(20.0f)) {
			p->swipedWeapon = true;
			if (deltaX > 0.0f)
				s_pulseWeaponNext = true;
			else
				s_pulseWeaponPrev = true;
		}
		return;
	}

	// Horizontal swipe on top-center vehicle radio zone retunes station
	if (p->btn == MBTN_RADIO_SLOT && !p->swipedRadio) {
		if (Abs(p->x - p->startX) >= m.Dp(20.0f)) {
			p->swipedRadio = true;
			s_pulseRadio = true;
		}
	}
#else
	(void)fingerId; (void)normX; (void)normY; (void)normDX; (void)normDY;
#endif
}

void
CMobileTouchControls::OnFingerUp(int64 fingerId, float normX, float normY)
{
#ifdef ANDROID
	const MobileTouchMath::MobileScreenMetrics m = GetMetrics();
	const float x = normX * m.screenW;
	const float y = normY * m.screenH;

	if (FrontEndMenuManager.m_bMenuActive) {
		OnMenuFingerUp(fingerId, x, y);
		return;
	}

	if (s_spawnerMode != mobilespawner::PANEL_NONE &&
	    s_spawnerPtr.active && s_spawnerPtr.fingerId == fingerId) {
		const bool cleanTap = (!s_spawnerPtr.draggingScroll && s_spawnerPtr.maxTravel <= m.Dp(12.0f));
		s_spawnerPtr.active = false;
		if (cleanTap) {
			const mobilespawner::PickerLayout L = GetCurrentSpawnerLayout(m);
			const mobilespawner::PanelHit upHit = mobilespawner::HitTestPicker(L, x, y);
			if (upHit == s_spawnerPtr.downHit) {
				switch (upHit) {
				case mobilespawner::PHIT_PREV:
					StepAndTriggerSpawner(-1, true);
					break;
				case mobilespawner::PHIT_STEP_MINUS10:
					StepAndTriggerSpawner(-10, false);
					break;
				case mobilespawner::PHIT_STEP_PLUS10:
					StepAndTriggerSpawner(10, false);
					break;
				case mobilespawner::PHIT_NEXT:
					StepAndTriggerSpawner(1, true);
					break;
				case mobilespawner::PHIT_CLOSE:
					s_spawnerMode = mobilespawner::PANEL_NONE;
					break;
				default:
					if (upHit >= mobilespawner::PHIT_ROW_0 && upHit <= mobilespawner::PHIT_ROW_5) {
						const int32 rel = (int32)upHit - (int32)mobilespawner::PHIT_ROW_0;
						std::vector<mobilespawner::CatalogItem> &cat =
							(s_spawnerMode == mobilespawner::PANEL_SKIN) ? s_skinCatalog : s_vehCatalog;
						int32 &sel = (s_spawnerMode == mobilespawner::PANEL_SKIN) ? s_skinSelectedIdx : s_vehSelectedIdx;
						const int32 firstRow = (s_spawnerMode == mobilespawner::PANEL_SKIN) ? s_skinFirstRow : s_vehFirstRow;
						const int32 targetIdx = firstRow + rel;
						if (targetIdx >= 0 && targetIdx < (int32)cat.size()) {
							sel = targetIdx;
							if (s_spawnerMode == mobilespawner::PANEL_SKIN)
								ApplyPlayerSkinModel(cat[sel].modelId);
							else
								SpawnVehicleByModel(cat[sel].modelId);
						}
					}
					break;
				}
			}
		}
		return;
	}

	ActivePointer *p = FindGameplayPointer(fingerId);
	if (!p)
		return;

	if (p->isStick) {
		s_stickAxisX = 0;
		s_stickAxisY = 0;
		s_stickKnobX = s_stickBaseX;
		s_stickKnobY = s_stickBaseY;
	} else if (p->btn == MBTN_WEAPON_SLOT && !p->swipedWeapon) {
		s_pulseWeaponNext = true;
	} else if (p->btn == MBTN_RADIO_SLOT && !p->swipedRadio) {
		s_pulseRadio = true;
	}

	p->active = false;
#else
	(void)fingerId; (void)normX; (void)normY;
#endif
}

void
CMobileTouchControls::ConsumeLookDelta(int *dx, int *dy)
{
	const int ix = (int)s_accumLookDX;
	const int iy = (int)s_accumLookDY;
	s_accumLookDX -= (float)ix;
	s_accumLookDY -= (float)iy;
	if (dx) *dx = ix;
	if (dy) *dy = iy;
}

void
CMobileTouchControls::ApplyKeyState(void)
{
#ifdef ANDROID
	if (s_pulseFlyToggle) {
		CPad::TempKeyState.F[9] = 255;
		s_pulseFlyToggle = false;
	} else {
		CPad::TempKeyState.F[9] = 0;
	}
#endif
}

void
CMobileTouchControls::UpdatePad(CPad *pad)
{
#ifdef ANDROID
	if (!pad)
		return;

	if (FrontEndMenuManager.m_bMenuActive || gGameState != GS_PLAYING_GAME) {
		memset(s_pointers, 0, sizeof(s_pointers));
		s_stickAxisX = 0;
		s_stickAxisY = 0;
		s_accumLookDX = 0.0f;
		s_accumLookDY = 0.0f;
		s_sprintJumpTap.Reset();
		return;
	}

	memset(s_menuPointers, 0, sizeof(s_menuPointers));
	UpdateGameplayLayout();

	CControllerState &st = pad->PCTempKeyState;

	if (s_pulseSkipCutscene) {
		st.Cross = 255;
		s_pulseSkipCutscene = false;
		return;
	}

	if (s_stickVisible && IsStickOwned()) {
		st.LeftStickX = s_stickAxisX;
		st.LeftStickY = s_stickAxisY;
	} else {
		const bool leftHeld = IsGameplayButtonHeld(MBTN_STEER_LEFT);
		const bool rightHeld = IsGameplayButtonHeld(MBTN_STEER_RIGHT);
		if (leftHeld && !rightHeld)
			st.LeftStickX = -128;
		else if (rightHeld && !leftHeld)
			st.LeftStickX = 127;
	}

	const bool inVeh = (FindPlayerVehicle() != nil);
	const bool inScope = !inVeh && IsInFirstPersonScopeCam();

	if (inVeh) {
		if (IsGameplayButtonHeld(MBTN_ACCEL))      st.Cross = 255;
		if (IsGameplayButtonHeld(MBTN_BRAKE))      st.Square = 255;
		if (IsGameplayButtonHeld(MBTN_HANDBRAKE))  st.RightShoulder1 = 255;
		if (IsGameplayButtonHeld(MBTN_ENTER_EXIT)) st.Triangle = 255;
		if (IsGameplayButtonHeld(MBTN_HORN))       st.LeftShock = 255;
		if (IsGameplayButtonHeld(MBTN_CAR_GUN))    st.Circle = 255;
		if (IsGameplayButtonHeld(MBTN_YAW_LEFT))   st.LeftShoulder2 = 255;
		if (IsGameplayButtonHeld(MBTN_YAW_RIGHT))  st.RightShoulder2 = 255;
	} else if (inScope) {
		if (IsGameplayButtonHeld(MBTN_ATTACK))     st.Circle = 255;
		if (IsGameplayButtonHeld(MBTN_ZOOM_IN))    st.Square = 255;
		if (IsGameplayButtonHeld(MBTN_ZOOM_OUT))   st.Cross = 255;
		if (s_scopeLatched || IsGameplayButtonHeld(MBTN_TARGET))
			st.RightShoulder1 = 255;
	} else {
		const bool sprintHeld = IsGameplayButtonHeld(MBTN_SPRINT);
		const bool jumpPulse = s_sprintJumpTap.ConsumeJumpFrame();
		if (IsGameplayButtonHeld(MBTN_ATTACK))     st.Circle = 255;
		if (sprintHeld) {
			st.Cross = 255;
			// Ensure holding Sprint upgrades partial walk stick deflection to full run
			if (st.LeftStickX != 0 || st.LeftStickY != 0) {
				const float fx = (float)st.LeftStickX;
				const float fy = (float)st.LeftStickY;
				const float mag = Sqrt(fx * fx + fy * fy);
				if (mag > 1.0f && mag < 127.0f) {
					st.LeftStickX = (int16)Clamp((fx / mag) * 127.0f, -128.0f, 127.0f);
					st.LeftStickY = (int16)Clamp((fy / mag) * 127.0f, -128.0f, 127.0f);
				}
			}
		}
		if (jumpPulse)                             st.Square = 255;
		if (IsGameplayButtonHeld(MBTN_ENTER_EXIT)) st.Triangle = 255;
		if (s_scopeLatched || IsGameplayButtonHeld(MBTN_TARGET))
			st.RightShoulder1 = 255;
	}

	if (s_pulsePause)      { st.Start = 255;          s_pulsePause = false; }
	if (s_pulseCam)        { st.Select = 255;         s_pulseCam = false; }
	if (s_pulseWeaponNext) { st.RightShoulder2 = 255; s_pulseWeaponNext = false; }
	if (s_pulseWeaponPrev) { st.LeftShoulder2 = 255;  s_pulseWeaponPrev = false; }
	if (s_pulseRadio)      { st.LeftShoulder1 = 255;  s_pulseRadio = false; }
	if (s_pulseInteract)   { st.LeftShoulder1 = 255;  s_pulseInteract = false; }
	if (s_pulseCrouch)     { st.LeftShock = 255;      s_pulseCrouch = false; }
	if (s_pulseSubmission) { st.RightShock = 255;     s_pulseSubmission = false; }
#else
	(void)pad;
#endif
}

void
CMobileTouchControls::ProcessMenuTouch(bool &goDown, bool &goUp, bool &optionSelected, bool &goBack, int8 &changeAmount)
{
#ifdef ANDROID
	(void)goDown;
	(void)goUp;
	FrontEndMenuManager.m_bShowMouse = false;

	// Stationary long-press on Pause Map toggles Waypoint without lifting finger
	if (FrontEndMenuManager.m_nCurrScreen == MENUPAGE_MAP) {
		MenuPointer *pa = nil, *pb = nil;
		const int nPtr = CountActiveMenuPointers(&pa, &pb);
		if (nPtr == 1 && pa && pa->overlayBtn == MOBTN_NONE && !pa->pinched && !pa->longPressedMap) {
			const MobileTouchMath::MobileScreenMetrics m = GetMetrics();
			const uint32 heldMs = CTimer::GetTimeInMillisecondsPauseMode() - pa->downTimeMs;
			if (heldMs >= 430 && pa->maxTravel <= m.Dp(10.0f)) {
				pa->longPressedMap = true;
				ToggleWaypointAtScreenPos(pa->x, pa->y);
			}
		}
	}

	if (s_menuPendingBack) {
		goBack = true;
		s_menuPendingBack = false;
		s_menuPendingSelect = false;
		s_menuPendingChange = 0;
		return;
	}
	if (s_menuPendingSelect) {
		optionSelected = true;
		s_menuPendingSelect = false;
	}
	if (s_menuPendingChange != 0) {
		changeAmount = s_menuPendingChange;
		s_menuPendingChange = 0;
	}
#else
	(void)goDown; (void)goUp; (void)optionSelected; (void)goBack; (void)changeAmount;
#endif
}

float
CMobileTouchControls::ConsumeStatsScrollDelta(void)
{
#ifdef ANDROID
	MenuPointer *pa = nil, *pb = nil;
	if (CountActiveMenuPointers(&pa, &pb) == 0 && Abs(s_statsVelocityPxPerSec) > 8.0f) {
		const float dt = Clamp(CTimer::GetTimeStepInSeconds(), 0.0f, 0.05f);
		s_statsScrollDeltaPx += s_statsVelocityPxPerSec * dt;
		s_statsVelocityPxPerSec *= std::exp(-5.0f * dt);
		if (Abs(s_statsVelocityPxPerSec) < 8.0f)
			s_statsVelocityPxPerSec = 0.0f;
	}
	const float d = s_statsScrollDeltaPx;
	s_statsScrollDeltaPx = 0.0f;
	return d;
#else
	return 0.0f;
#endif
}

void
CMobileTouchControls::DrawMenuOverlay(void)
{
#ifdef ANDROID
	if (!FrontEndMenuManager.m_bMenuActive || FrontEndMenuManager.m_firstStartCounter < 255)
		return;

	UpdateMenuOverlayLayout();

	RwRenderStateSet(rwRENDERSTATEZTESTENABLE, (void*)FALSE);
	RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, (void*)FALSE);
	RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)TRUE);
	RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDSRCALPHA);
	RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDINVSRCALPHA);
	RwRenderStateSet(rwRENDERSTATETEXTUREFILTER, (void*)rwFILTERLINEAR);
	RwRenderStateSet(rwRENDERSTATETEXTUREADDRESS, (void*)rwTEXTUREADDRESSCLAMP);

	for (int i = 0; i < MOBTN_COUNT; i++) {
		const VirtualButton &b = s_menuOverlayBtns[i];
		if (!b.visible)
			continue;
		bool active = IsMenuOverlayHeld((MenuOverlayBtn)i);
		if (i == MOBTN_MAP_LEGEND && FrontEndMenuManager.m_PrefsShowLegends)
			active = true;
		DrawVirtualButton(b, active, 185);
	}

	RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)FALSE);
#endif
}

#ifdef ANDROID
static void
DrawSpawnerTextLine(float x, float y, float scaleX, float scaleY, const CRGBA &col, const char *ascii, bool centre = false, float wrapRight = 4096.0f)
{
	if (!ascii || !*ascii)
		return;
	wchar wideBuf[96];
	AsciiToUnicode(ascii, wideBuf);

	CFont::SetBackgroundOff();
	CFont::SetBackGroundOnlyTextOff();
	CFont::SetPropOn();
	CFont::SetFontStyle(FONT_STANDARD);
	CFont::SetScale(scaleX, scaleY);
	CFont::SetDropShadowPosition(1);
	CFont::SetDropColor(CRGBA(0, 0, 0, col.a));
	CFont::SetColor(col);
	if (centre) {
		CFont::SetRightJustifyOff();
		CFont::SetCentreOn();
		CFont::SetCentreSize(SCREEN_WIDTH);
	} else {
		CFont::SetCentreOff();
		CFont::SetRightJustifyOff();
		CFont::SetWrapx(wrapRight);
	}
	CFont::PrintString(x, y, wideBuf);
}

static void
DrawSpawnerOverlay(const MobileTouchMath::MobileScreenMetrics &m)
{
	const uint32 nowMs = CTimer::GetTimeInMilliseconds();
	const bool showToast = (s_spawnerToast[0] != '\0' && nowMs < s_spawnerToastUntilMs);
	const bool showPanel = (s_spawnerMode != mobilespawner::PANEL_NONE);
	if (!showToast && !showPanel)
		return;

	const float fontScaleY = Max(0.42f, m.Dp(14.5f) / 22.0f);
	const float fontScaleX = fontScaleY * 0.48f;
	bool queuedAnyText = false;

	if (showPanel) {
		const mobilespawner::PickerLayout L = GetCurrentSpawnerLayout(m);
		const std::vector<mobilespawner::CatalogItem> &cat =
			(s_spawnerMode == mobilespawner::PANEL_SKIN) ? s_skinCatalog : s_vehCatalog;
		const int32 sel = (s_spawnerMode == mobilespawner::PANEL_SKIN) ? s_skinSelectedIdx : s_vehSelectedIdx;
		const int32 firstRow = (s_spawnerMode == mobilespawner::PANEL_SKIN) ? s_skinFirstRow : s_vehFirstRow;

		// Outer subtle border + dark translucent card + header strip
		CSprite2d::DrawRect(CRect(L.left - m.Dp(1.5f), L.top - m.Dp(1.5f), L.right + m.Dp(1.5f), L.bottom + m.Dp(1.5f)),
			CRGBA(255, 165, 220, 185));
		CSprite2d::DrawRect(CRect(L.left, L.top, L.right, L.bottom),
			CRGBA(14, 18, 26, 228));
		CSprite2d::DrawRect(CRect(L.left, L.top, L.right, L.headerBottom),
			CRGBA(28, 35, 50, 240));

		// 5 header buttons: [<] [-10] [+10] [>] [X]
		const float btnCy = L.BtnCenterY();
		const float btnR = m.Dp(14.0f);
		for (int b = 0; b < 5; b++) {
			const float bx = L.BtnCenterX(b);
			const CRGBA fill = (b == 4) ? CRGBA(90, 28, 42, 210) : CRGBA(42, 52, 74, 210);
			DrawFilledCircle(bx, btnCy, btnR, fill, CRGBA(225, 232, 245, 165), m.Dp(1.3f));
			if (b == 0)
				DrawFallbackGlyph(GLYPH_ARROW_LEFT, bx, btnCy, btnR, CRGBA(255, 255, 255, 235));
			else if (b == 3)
				DrawFallbackGlyph(GLYPH_ARROW_RIGHT, bx, btnCy, btnR, CRGBA(255, 255, 255, 235));
			else if (b == 4)
				DrawFallbackGlyph(GLYPH_CLOSE, bx, btnCy, btnR, CRGBA(255, 210, 220, 240));
		}

		// Row highlight bars before text flush
		for (int r = 0; r < L.visibleRows; r++) {
			const int32 idx = firstRow + r;
			if (idx < 0 || idx >= (int32)cat.size())
				break;
			const float ry1 = L.rowTop + L.rowHeight * (float)r;
			const float ry2 = ry1 + L.rowHeight - m.Dp(2.0f);
			if (idx == sel) {
				CSprite2d::DrawRect(CRect(L.left + m.Dp(4.0f), ry1, L.right - m.Dp(4.0f), ry2),
					CRGBA(235, 95, 175, 135));
			} else if ((r & 1) == 1) {
				CSprite2d::DrawRect(CRect(L.left + m.Dp(4.0f), ry1, L.right - m.Dp(4.0f), ry2),
					CRGBA(255, 255, 255, 16));
			}
		}

		// Scrollbar thumb on right edge
		if ((int32)cat.size() > L.visibleRows) {
			const float trackTop = L.rowTop;
			const float trackH = L.rowHeight * (float)L.visibleRows;
			const float thumbH = Max(m.Dp(18.0f), trackH * ((float)L.visibleRows / (float)cat.size()));
			const float maxFirst = (float)Max(1, (int32)cat.size() - L.visibleRows);
			const float thumbY = trackTop + (trackH - thumbH) * Clamp((float)firstRow / maxFirst, 0.0f, 1.0f);
			CSprite2d::DrawRect(CRect(L.right - m.Dp(5.0f), thumbY, L.right - m.Dp(2.0f), thumbY + thumbH),
				CRGBA(255, 180, 225, 190));
		}

		// Header text labels for -10 / +10
		DrawSpawnerTextLine(L.BtnCenterX(1) - m.Dp(11.0f), btnCy - m.Dp(8.0f),
			fontScaleX * 0.88f, fontScaleY * 0.88f, CRGBA(240, 245, 255, 240), "-10");
		DrawSpawnerTextLine(L.BtnCenterX(2) - m.Dp(12.0f), btnCy - m.Dp(8.0f),
			fontScaleX * 0.88f, fontScaleY * 0.88f, CRGBA(240, 245, 255, 240), "+10");
		queuedAnyText = true;

		if (cat.empty()) {
			DrawSpawnerTextLine(L.left + m.Dp(12.0f), L.rowTop + m.Dp(12.0f),
				fontScaleX, fontScaleY, CRGBA(220, 225, 235, 220),
				"NO MODELS AVAILABLE", false, L.right - m.Dp(8.0f));
		} else {
			for (int r = 0; r < L.visibleRows; r++) {
				const int32 idx = firstRow + r;
				if (idx < 0 || idx >= (int32)cat.size())
					break;
				const mobilespawner::CatalogItem &it = cat[idx];
				char line[64];
				if (s_spawnerMode == mobilespawner::PANEL_SKIN) {
					std::snprintf(line, sizeof(line), "%d %s [%s]",
						(int)it.modelId, it.name, it.isSecondaryTag ? "SA" : "VC");
				} else {
					std::snprintf(line, sizeof(line), "%d %s%s",
						(int)it.modelId, it.name, it.isSecondaryTag ? " [BIKE]" : "");
				}
				const CRGBA rowCol = (idx == sel)
					? CRGBA(255, 255, 255, 255)
					: (it.isCustom ? CRGBA(185, 240, 255, 235) : CRGBA(225, 228, 238, 225));
				const float textY = L.rowTop + L.rowHeight * (float)r + m.Dp(6.0f);
				DrawSpawnerTextLine(L.left + m.Dp(10.0f), textY,
					fontScaleX, fontScaleY, rowCol, line, false, L.right - m.Dp(8.0f));
			}
		}
	}

	if (showToast) {
		const float toastY = m.marginTopPx + m.Dp(54.0f);
		const float toastHalfW = m.Dp(145.0f);
		const float cx = m.screenW * 0.50f;
		CSprite2d::DrawRect(CRect(cx - toastHalfW, toastY, cx + toastHalfW, toastY + m.Dp(26.0f)),
			CRGBA(14, 18, 26, 205));
		DrawSpawnerTextLine(cx, toastY + m.Dp(4.0f),
			fontScaleX * 0.95f, fontScaleY * 0.95f, CRGBA(255, 230, 245, 245), s_spawnerToast, true);
		queuedAnyText = true;
	}

	if (queuedAnyText)
		CFont::DrawFonts();
}
#endif

void
CMobileTouchControls::Draw(void)
{
#ifdef ANDROID
	if (!IsGameplayTouchHudActive())
		return;

	UpdateGameplayLayout();
	const MobileTouchMath::MobileScreenMetrics m = GetMetrics();

	RwRenderStateSet(rwRENDERSTATEZTESTENABLE, (void*)FALSE);
	RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, (void*)FALSE);
	RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)TRUE);
	RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDSRCALPHA);
	RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDINVSRCALPHA);
	RwRenderStateSet(rwRENDERSTATETEXTUREFILTER, (void*)rwFILTERLINEAR);
	RwRenderStateSet(rwRENDERSTATETEXTUREADDRESS, (void*)rwTEXTUREADDRESSCLAMP);

	// Dynamic stick: 100% transparent when idle; appears only while actively pressed & held
	if (s_stickVisible && IsStickOwned()) {
		const float knobX = s_stickKnobX;
		const float knobY = s_stickKnobY;
		const uint8 baseAlpha = 225;
		const uint8 nubAlpha = 250;

		if (s_tex[MTEX_CIRCLE]) {
			DrawTextureQuad(s_tex[MTEX_CIRCLE], s_stickBaseX, s_stickBaseY,
				s_stickRadius, s_stickRadius, CRGBA(255, 255, 255, baseAlpha), false);
		} else {
			DrawFilledCircle(s_stickBaseX, s_stickBaseY, s_stickRadius,
				CRGBA(14, 18, 26, 75), CRGBA(235, 240, 250, 145), Max(2.0f, s_stickRadius * 0.035f));
		}

		const float nubR = s_stickRadius * 0.44f;
		if (s_tex[MTEX_NUB]) {
			DrawTextureQuad(s_tex[MTEX_NUB], knobX, knobY,
				nubR, nubR, CRGBA(255, 255, 255, nubAlpha), false);
		} else {
			DrawFilledCircle(knobX, knobY, nubR,
				CRGBA(255, 175, 230, 185),
				CRGBA(255, 255, 255, 210), Max(2.0f, nubR * 0.08f));
		}
	}

	for (int i = 0; i < MBTN_COUNT; i++) {
		const VirtualButton &b = s_buttons[i];
		if (!b.visible || b.invisibleHitZone)
			continue;

		bool active = IsGameplayButtonHeld((MobileBtnId)i);
		if (i == MBTN_TARGET && s_scopeLatched)
			active = true;
		if (i == MBTN_DEV_FLY && CNoClip::IsActive())
			active = true;
		if (i == MBTN_DEV_SKIN && s_spawnerMode == mobilespawner::PANEL_SKIN)
			active = true;
		if (i == MBTN_DEV_VEHICLE && s_spawnerMode == mobilespawner::PANEL_VEHICLE)
			active = true;

		const uint8 idleAlpha = (i == MBTN_DEV_TOGGLE && !s_devDrawerOpen) ? 105 : 195;
		DrawVirtualButton(b, active, idleAlpha);
	}

	DrawSpawnerOverlay(m);

	RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)FALSE);
#endif
}
