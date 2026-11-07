#include "common.h"
#include "crossplatform.h"
#include "platform.h"
#include "MobileTouchControls.h"
#include "MobileTouchMath.h"

#include "Camera.h"
#include "Clock.h"
#include "CutsceneMgr.h"
#include "DebugLog.h"
#include "Frontend.h"
#include "General.h"
#include "Hud.h"
#include "ModelIndices.h"
#include "ModelInfo.h"
#include "NoClip.h"
#include "Pad.h"
#include "Pickups.h"
#include "PlayerPed.h"
#include "Pools.h"
#include "Radar.h"
#include "Replay.h"
#include "RwHelper.h"
#include "Sprite2d.h"
#include "Timer.h"
#include "TxdStore.h"
#include "Vehicle.h"
#include "WeaponInfo.h"
#include "WeaponType.h"
#include "World.h"
#include "main.h"

#include <cmath>
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
	GLYPH_FLY
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
};

static const int MAX_TOUCH_POINTERS = 10;

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

// One-shot pulse latches consumed in UpdatePad / ApplyKeyState
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

	// High-resolution mobile weapon icons for the top-right HUD slot
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

static void
SetCircleBtn(MobileBtnId id, float cx, float cy, float radius, MobileTexId tex, FallbackGlyph glyph)
{
	VirtualButton &b = s_buttons[id];
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
SetRectBtn(MobileBtnId id, float cx, float cy, float halfW, float halfH, MobileTexId tex, FallbackGlyph glyph, bool invisible = false, bool preserveAspect = false)
{
	VirtualButton &b = s_buttons[id];
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
UpdateLayout(void)
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

	const float W = (float)SCREEN_WIDTH;
	const float H = (float)SCREEN_HEIGHT;
	if (W <= 1.0f || H <= 1.0f)
		return;

	const float u = Min(W, H);
	const float safeX = Max(u * 0.048f, W * 0.032f);
	const float safeY = u * 0.038f;

	// 1. Top-Left Radar tap zone (opens Pause / Map menu) or standalone Pause icon if radar hidden
	const bool radarVisible = FrontEndMenuManager.m_PrefsRadarMode != 2 && !CHud::m_HideRadar;
	if (radarVisible) {
		const float rx1 = SCREEN_SCALE_X(RADAR_LEFT) - SCREEN_SCALE_X(6.0f);
		const float ry1 = SCREEN_SCALE_Y(24.0f) - SCREEN_SCALE_Y(6.0f);
		const float rx2 = SCREEN_SCALE_X(RADAR_LEFT + RADAR_WIDTH) + SCREEN_SCALE_X(6.0f);
		const float ry2 = SCREEN_SCALE_Y(24.0f + RADAR_HEIGHT) + SCREEN_SCALE_Y(6.0f);
		SetRectBtn(MBTN_PAUSE, 0.5f * (rx1 + rx2), 0.5f * (ry1 + ry2),
			0.5f * (rx2 - rx1), 0.5f * (ry2 - ry1), MTEX_PAUSE, GLYPH_PAUSE, true);
	} else {
		const float rPause = u * 0.048f;
		SetCircleBtn(MBTN_PAUSE, safeX + rPause, safeY + rPause, rPause, MTEX_PAUSE, GLYPH_PAUSE);
	}

	// 2. Top-Right HUD weapon icon tap/swipe zone + Camera mode toggle
	const float wx1 = SCREEN_SCALE_FROM_RIGHT(104.0f);
	const float wy1 = SCREEN_SCALE_Y(22.0f);
	const float wx2 = SCREEN_SCALE_FROM_RIGHT(30.0f);
	const float wy2 = SCREEN_SCALE_Y(96.0f);
	CVehicle *veh = FindPlayerVehicle();
	if (!veh && FrontEndMenuManager.m_PrefsShowHud) {
		SetRectBtn(MBTN_WEAPON_SLOT, 0.5f * (wx1 + wx2), 0.5f * (wy1 + wy2),
			0.5f * (wx2 - wx1), 0.5f * (wy2 - wy1), MTEX_SWAPGUN, GLYPH_NONE, true);
	}

	const float rTopIcon = u * 0.046f;
	const float topIconY = SCREEN_SCALE_Y(24.0f) + rTopIcon;
	const float camIconX = Min(SCREEN_SCALE_FROM_RIGHT(206.0f), W - safeX - u * 0.30f);
	SetCircleBtn(MBTN_CAM, camIconX, topIconY, rTopIcon, MTEX_CAM_TOGGLE, GLYPH_CAMERA);

	// Collapsible utility dot (FLY / TIME+ / TIME-) kept subtle at top-center-left
	const float rDev = u * 0.036f;
	const float devX = Max(SCREEN_SCALE_X(RADAR_LEFT + RADAR_WIDTH + 22.0f) + rDev, W * 0.28f);
	SetCircleBtn(MBTN_DEV_TOGGLE, devX, topIconY, rDev, MTEX_COUNT, GLYPH_DOTS);
	if (s_devDrawerOpen) {
		const float step = rDev * 2.35f;
		SetCircleBtn(MBTN_DEV_FLY, devX + step, topIconY, rDev, MTEX_COUNT, GLYPH_FLY);
		SetCircleBtn(MBTN_DEV_TIME_PLUS, devX + step * 2.0f, topIconY, rDev, MTEX_ZOOM_IN, GLYPH_PLUS);
		SetCircleBtn(MBTN_DEV_TIME_MINUS, devX + step * 3.0f, topIconY, rDev, MTEX_ZOOM_OUT, GLYPH_MINUS);
	}

	// 3. IN VEHICLE LAYOUT
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

		// Steering mode toggle (Arrows <-> Analog Stick) in top bar + Radio tap zone at top center
		SetCircleBtn(MBTN_DRIVE_MODE, camIconX - rTopIcon * 2.35f, topIconY, rTopIcon, MTEX_DRIVE_TOGGLE, GLYPH_STEER_MODE);
		SetRectBtn(MBTN_RADIO_SLOT, W * 0.50f, SCREEN_SCALE_Y(32.0f), u * 0.14f, u * 0.045f, MTEX_COUNT, GLYPH_NONE, true);

		if (SupportsVehicleSubmission(veh)) {
			SetCircleBtn(MBTN_SUBMISSION, camIconX - rTopIcon * 4.70f, topIconY, rTopIcon, MTEX_SUBMISSION, GLYPH_TARGET);
		}

		// Left side: Analog Stick or Left/Right Steering Arrows
		if (useStick) {
			s_stickVisible = true;
			s_stickRadius = u * 0.128f;
			s_stickBaseX = safeX + s_stickRadius * 1.08f;
			s_stickBaseY = H - safeY - s_stickRadius * 1.12f;
		} else {
			const float rSteer = u * 0.072f;
			const float steerY = H - safeY - rSteer * 1.18f;
			const float leftX = safeX + rSteer * 1.05f;
			const float rightX = leftX + rSteer * 2.38f;
			SetCircleBtn(MBTN_STEER_LEFT, leftX, steerY, rSteer,
				isTank ? MTEX_TANK_LEFT : MTEX_LEFT, GLYPH_ARROW_LEFT);
			SetCircleBtn(MBTN_STEER_RIGHT, rightX, steerY, rSteer,
				isTank ? MTEX_TANK_RIGHT : MTEX_RIGHT, GLYPH_ARROW_RIGHT);
		}

		// Right side: Gas pedal, Brake pedal, Handbrake, Exit Vehicle, Horn, Vehicle Gun
		const float pedalHalfW = u * 0.056f;
		const float gasHalfH = u * 0.105f;
		const float brakeHalfH = u * 0.072f;
		const float gasX = W - safeX - pedalHalfW * 1.05f;
		const float gasY = H - safeY - gasHalfH * 1.02f;
		const float brakeX = gasX - pedalHalfW * 2.35f;
		const float brakeY = H - safeY - brakeHalfH * 1.08f;

		SetRectBtn(MBTN_ACCEL, gasX, gasY, pedalHalfW, gasHalfH, MTEX_ACCELERATE, GLYPH_PEDAL_GAS, false, true);
		SetRectBtn(MBTN_BRAKE, brakeX, brakeY, pedalHalfW, brakeHalfH, MTEX_BRAKE, GLYPH_PEDAL_BRAKE, false, true);

		const float rSub = u * 0.058f;
		SetCircleBtn(MBTN_HANDBRAKE, brakeX - rSub * 2.15f, H - safeY - rSub * 1.15f, rSub, MTEX_HANDBRAKE, GLYPH_HANDBRAKE);

		MobileTexId exitTex = MTEX_EXIT_CAR;
		if (isBike) exitTex = MTEX_EXIT_BIKE;
		else if (isHeli) exitTex = MTEX_EXIT_HELI;
		else if (isPlane) exitTex = MTEX_EXIT_PLANE;
		SetCircleBtn(MBTN_ENTER_EXIT, gasX - rSub * 0.20f, gasY - gasHalfH - rSub * 1.25f, rSub * 0.92f, exitTex, GLYPH_EXIT);

		SetCircleBtn(MBTN_HORN, brakeX, brakeY - brakeHalfH - rSub * 1.25f, rSub * 0.90f, MTEX_HORN, GLYPH_HORN);

		MobileTexId armedTex = MTEX_SHOOT;
		if (IsArmedVehicle(veh, &armedTex)) {
			SetCircleBtn(MBTN_CAR_GUN, brakeX - rSub * 2.15f, brakeY - brakeHalfH - rSub * 1.10f, rSub * 0.98f, armedTex, GLYPH_CROSSHAIR);
		}
		if (isHeli || isPlane) {
			const float rYaw = u * 0.046f;
			const float yawY = H - safeY - u * 0.34f;
			SetCircleBtn(MBTN_YAW_LEFT, safeX + rYaw * 1.1f, yawY, rYaw, MTEX_LEFT, GLYPH_ARROW_LEFT);
			SetCircleBtn(MBTN_YAW_RIGHT, safeX + rYaw * 3.5f, yawY, rYaw, MTEX_RIGHT, GLYPH_ARROW_RIGHT);
		}
		return;
	}

	// 4. ON FOOT / SCOPE LAYOUT
	s_stickVisible = true;
	s_stickRadius = u * 0.128f;
	s_stickBaseX = safeX + s_stickRadius * 1.08f;
	s_stickBaseY = H - safeY - s_stickRadius * 1.12f;

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

	const float rAttack = u * 0.086f;
	const float rSub = u * 0.060f;
	const float attackX = W - safeX - u * 0.132f;
	const float attackY = H - safeY - u * 0.162f;

	SetCircleBtn(MBTN_ATTACK, attackX, attackY, rAttack, attackTex, attackGlyph);

	const bool inScope = IsInFirstPersonScopeCam();
	if (inScope) {
		SetCircleBtn(MBTN_TARGET, attackX + u * 0.015f, attackY - u * 0.176f, rSub, MTEX_LOCKON, GLYPH_TARGET);
		const int32 camMode = TheCamera.Cams[TheCamera.ActiveCam].Mode;
		if (camMode == CCam::MODE_SNIPER || camMode == CCam::MODE_SNIPER_RUNABOUT || camMode == CCam::MODE_CAMERA) {
			SetCircleBtn(MBTN_ZOOM_IN, attackX - u * 0.165f, attackY - u * 0.105f, rSub * 0.94f, MTEX_ZOOM_IN, GLYPH_PLUS);
			SetCircleBtn(MBTN_ZOOM_OUT, attackX - u * 0.165f, attackY + u * 0.045f, rSub * 0.94f, MTEX_ZOOM_OUT, GLYPH_MINUS);
		}
		return;
	}

	// GTA VC Mobile right-thumb arc: Sprint (bottom-left of Attack), Jump (upper-left), Lock-on (top)
	SetCircleBtn(MBTN_SPRINT, attackX - u * 0.168f, H - safeY - rSub * 1.18f, rSub, MTEX_RUN, GLYPH_SPRINT);
	SetCircleBtn(MBTN_JUMP, attackX - u * 0.148f, attackY - u * 0.132f, rSub, MTEX_JUMP, GLYPH_JUMP);

	if (wi && (wi->m_nWeaponSlot >= 2 || wi->IsFlagSet(WEAPONFLAG_CANAIM) || IsFirstPersonScopeWeapon(wt))) {
		SetCircleBtn(MBTN_TARGET, attackX + u * 0.018f, attackY - u * 0.175f, rSub * 0.96f, MTEX_LOCKON, GLYPH_TARGET);
	}

	// Context-sensitive Enter Vehicle button when a vehicle is nearby (or while entering)
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
		SetCircleBtn(MBTN_ENTER_EXIT, attackX - u * 0.285f, attackY - u * 0.030f, rSub * 1.04f, enterTex, GLYPH_CAR);
	}

	// Context-sensitive Pickup / Buy Property / Swap Weapon / Phone button
	MobileTexId interactTex = MTEX_BUY;
	if (CheckNearbyInteractPickup(player, &interactTex)) {
		SetCircleBtn(MBTN_INTERACT, attackX - u * 0.275f, attackY - u * 0.175f, rSub * 0.96f, interactTex, GLYPH_PLUS);
	}
}

static bool
HitTestButton(const VirtualButton &b, float x, float y)
{
	if (!b.visible)
		return false;
	const float dx = x - b.cx;
	const float dy = y - b.cy;
	if (b.circular) {
		const float hitR = b.halfW * 1.18f;
		return (dx * dx + dy * dy) <= (hitR * hitR);
	}
	const float padX = b.halfW * 0.14f;
	const float padY = b.halfH * 0.14f;
	return Abs(dx) <= (b.halfW + padX) && Abs(dy) <= (b.halfH + padY);
}

static MobileBtnId
FindHitButton(float x, float y)
{
	// Prioritize explicit visible buttons before invisible HUD zones
	for (int pass = 0; pass < 2; pass++) {
		for (int i = MBTN_COUNT - 1; i >= 0; i--) {
			const VirtualButton &b = s_buttons[i];
			if (!b.visible)
				continue;
			if ((pass == 0 && b.invisibleHitZone) || (pass == 1 && !b.invisibleHitZone))
				continue;
			if (HitTestButton(b, x, y))
				return (MobileBtnId)i;
		}
	}
	return MBTN_NONE;
}

static bool
IsButtonHeld(MobileBtnId id)
{
	for (int i = 0; i < MAX_TOUCH_POINTERS; i++) {
		if (s_pointers[i].active && s_pointers[i].btn == id)
			return true;
	}
	return false;
}

static ActivePointer*
FindPointer(int64 fingerId)
{
	for (int i = 0; i < MAX_TOUCH_POINTERS; i++) {
		if (s_pointers[i].active && s_pointers[i].fingerId == fingerId)
			return &s_pointers[i];
	}
	return nil;
}

static ActivePointer*
AllocPointer(int64 fingerId)
{
	ActivePointer *existing = FindPointer(fingerId);
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
		s_stickRadius, 0.14f, &offX, &offY, &s_stickAxisX, &s_stickAxisY);
	s_stickKnobX = s_stickBaseX + offX;
	s_stickKnobY = s_stickBaseY + offY;
}

static void
HandleButtonDownPulse(MobileBtnId btn)
{
	switch (btn) {
	case MBTN_PAUSE:
		s_pulsePause = true;
		break;
	case MBTN_CAM:
		s_pulseCam = true;
		break;
	case MBTN_RADIO_SLOT:
		s_pulseRadio = true;
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
	default:
		break;
	}
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
		DrawThickSegment(cx + r * 0.22f, cy - r * 0.38f, cx - r * 0.24f, cy, t * 1.4f, col);
		DrawThickSegment(cx - r * 0.24f, cy, cx + r * 0.22f, cy + r * 0.38f, t * 1.4f, col);
		break;
	case GLYPH_ARROW_RIGHT:
		DrawThickSegment(cx - r * 0.22f, cy - r * 0.38f, cx + r * 0.24f, cy, t * 1.4f, col);
		DrawThickSegment(cx + r * 0.24f, cy, cx - r * 0.22f, cy + r * 0.38f, t * 1.4f, col);
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

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

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
	s_stickAxisX = 0;
	s_stickAxisY = 0;
	s_accumLookDX = 0.0f;
	s_accumLookDY = 0.0f;
}

void
CMobileTouchControls::OnFingerDown(int64 fingerId, float normX, float normY)
{
#ifdef ANDROID
	if (gGameState != GS_PLAYING_GAME || FrontEndMenuManager.m_bMenuActive)
		return;

	if (CCutsceneMgr::IsRunning() || TheCamera.m_WideScreenOn) {
		s_pulseSkipCutscene = true;
		return;
	}

	UpdateLayout();

	const float W = (float)SCREEN_WIDTH;
	const float H = (float)SCREEN_HEIGHT;
	const float x = normX * W;
	const float y = normY * H;

	ActivePointer *p = AllocPointer(fingerId);
	if (!p)
		return;
	p->x = p->startX = x;
	p->y = p->startY = y;

	// 1. Explicit virtual buttons first
	MobileBtnId hitBtn = FindHitButton(x, y);
	if (hitBtn != MBTN_NONE) {
		p->btn = hitBtn;
		HandleButtonDownPulse(hitBtn);
		return;
	}

	// 2. Virtual Analog Stick (with forgiving bottom-left grab region)
	if (s_stickVisible && !IsStickOwned()) {
		const float dx = x - s_stickBaseX;
		const float dy = y - s_stickBaseY;
		const float grabR = s_stickRadius * 1.55f;
		const bool inThumbZone = (x < W * 0.34f && y > H * 0.45f);
		if ((dx * dx + dy * dy) <= (grabR * grabR) || inThumbZone) {
			p->isStick = true;
			const float maxAnchorOffset = s_stickRadius * 0.45f;
			const float dist = Sqrt(dx * dx + dy * dy);
			if (dist > maxAnchorOffset && dist > 1e-4f) {
				s_stickAnchorX = s_stickBaseX + (dx / dist) * maxAnchorOffset;
				s_stickAnchorY = s_stickBaseY + (dy / dist) * maxAnchorOffset;
			} else {
				s_stickAnchorX = x;
				s_stickAnchorY = y;
			}
			UpdateStickFromTouch(x, y);

			const uint32 nowMs = CTimer::GetTimeInMilliseconds();
			if (nowMs - s_lastStickTapMs < 280 && !FindPlayerVehicle())
				s_pulseCrouch = true;
			s_lastStickTapMs = nowMs;
			return;
		}
	}

	// 3. Any remaining screen area acts as smooth 360-degree camera free-look
	p->isLook = true;
#else
	(void)fingerId; (void)normX; (void)normY;
#endif
}

void
CMobileTouchControls::OnFingerMotion(int64 fingerId, float normX, float normY, float normDX, float normDY)
{
#ifdef ANDROID
	ActivePointer *p = FindPointer(fingerId);
	if (!p)
		return;

	const float W = (float)SCREEN_WIDTH;
	const float H = (float)SCREEN_HEIGHT;
	p->x = normX * W;
	p->y = normY * H;

	if (p->isStick) {
		UpdateStickFromTouch(p->x, p->y);
		return;
	}

	// Free-look OR aim-while-holding Attack/Target button (GTA VC Mobile thumb-aim!)
	if (p->isLook || p->btn == MBTN_ATTACK || p->btn == MBTN_TARGET) {
		const float sensScale = (p->btn == MBTN_ATTACK) ? 0.55f : 0.72f;
		s_accumLookDX += normDX * W * sensScale;
		s_accumLookDY += normDY * H * sensScale;
		return;
	}

	// Horizontal swipe on top-right weapon slot cycles previous / next weapon
	if (p->btn == MBTN_WEAPON_SLOT && !p->swipedWeapon) {
		const float swipeThresh = Min(W, H) * 0.040f;
		const float deltaX = p->x - p->startX;
		if (Abs(deltaX) >= swipeThresh) {
			p->swipedWeapon = true;
			if (deltaX > 0.0f)
				s_pulseWeaponNext = true;
			else
				s_pulseWeaponPrev = true;
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
	(void)normX; (void)normY;
	ActivePointer *p = FindPointer(fingerId);
	if (!p)
		return;

	if (p->isStick) {
		s_stickAxisX = 0;
		s_stickAxisY = 0;
		s_stickKnobX = s_stickBaseX;
		s_stickKnobY = s_stickBaseY;
	} else if (p->btn == MBTN_WEAPON_SLOT && !p->swipedWeapon) {
		s_pulseWeaponNext = true;
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
		ResetTouches();
		return;
	}

	UpdateLayout();

	CControllerState &st = pad->PCTempKeyState;

	if (s_pulseSkipCutscene) {
		st.Cross = 255;
		s_pulseSkipCutscene = false;
		return;
	}

	// Analog joystick (-128..+127) or left/right steering arrows
	if (s_stickVisible && IsStickOwned()) {
		st.LeftStickX = s_stickAxisX;
		st.LeftStickY = s_stickAxisY;
	} else {
		const bool leftHeld = IsButtonHeld(MBTN_STEER_LEFT);
		const bool rightHeld = IsButtonHeld(MBTN_STEER_RIGHT);
		if (leftHeld && !rightHeld)
			st.LeftStickX = -128;
		else if (rightHeld && !leftHeld)
			st.LeftStickX = 127;
	}

	// Continuous held gameplay buttons
	const bool inVeh = (FindPlayerVehicle() != nil);
	const bool inScope = !inVeh && IsInFirstPersonScopeCam();

	if (inVeh) {
		if (IsButtonHeld(MBTN_ACCEL))      st.Cross = 255;
		if (IsButtonHeld(MBTN_BRAKE))      st.Square = 255;
		if (IsButtonHeld(MBTN_HANDBRAKE))  st.RightShoulder1 = 255;
		if (IsButtonHeld(MBTN_ENTER_EXIT)) st.Triangle = 255;
		if (IsButtonHeld(MBTN_HORN))       st.LeftShock = 255;
		if (IsButtonHeld(MBTN_CAR_GUN))    st.Circle = 255;
		if (IsButtonHeld(MBTN_YAW_LEFT))   st.LeftShoulder2 = 255;
		if (IsButtonHeld(MBTN_YAW_RIGHT))  st.RightShoulder2 = 255;
	} else if (inScope) {
		if (IsButtonHeld(MBTN_ATTACK))     st.Circle = 255;
		if (IsButtonHeld(MBTN_ZOOM_IN))    st.Square = 255;
		if (IsButtonHeld(MBTN_ZOOM_OUT))   st.Cross = 255;
		if (s_scopeLatched || IsButtonHeld(MBTN_TARGET))
			st.RightShoulder1 = 255;
	} else {
		if (IsButtonHeld(MBTN_ATTACK))     st.Circle = 255;
		if (IsButtonHeld(MBTN_SPRINT))     st.Cross = 255;
		if (IsButtonHeld(MBTN_JUMP))       st.Square = 255;
		if (IsButtonHeld(MBTN_ENTER_EXIT)) st.Triangle = 255;
		if (s_scopeLatched || IsButtonHeld(MBTN_TARGET))
			st.RightShoulder1 = 255;
	}

	// One-shot pulse actions
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
CMobileTouchControls::Draw(void)
{
#ifdef ANDROID
	if (!IsGameplayTouchHudActive())
		return;

	UpdateLayout();

	RwRenderStateSet(rwRENDERSTATEZTESTENABLE, (void*)FALSE);
	RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, (void*)FALSE);
	RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)TRUE);
	RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDSRCALPHA);
	RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDINVSRCALPHA);
	RwRenderStateSet(rwRENDERSTATETEXTUREFILTER, (void*)rwFILTERLINEAR);
	RwRenderStateSet(rwRENDERSTATETEXTUREADDRESS, (void*)rwTEXTUREADDRESSCLAMP);

	// 1. Virtual Analog Stick (hud_circle + hud_analognub / hud_Nipple)
	if (s_stickVisible) {
		const bool stickActive = IsStickOwned();
		const float knobX = stickActive ? s_stickKnobX : s_stickBaseX;
		const float knobY = stickActive ? s_stickKnobY : s_stickBaseY;
		const uint8 baseAlpha = stickActive ? 225 : 165;
		const uint8 nubAlpha = stickActive ? 250 : 200;

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
				stickActive ? CRGBA(255, 175, 230, 185) : CRGBA(235, 240, 250, 130),
				CRGBA(255, 255, 255, 210), Max(2.0f, nubR * 0.08f));
		}
	}

	// 2. Context-Sensitive Action Buttons
	for (int i = 0; i < MBTN_COUNT; i++) {
		const VirtualButton &b = s_buttons[i];
		if (!b.visible || b.invisibleHitZone)
			continue;

		bool active = IsButtonHeld((MobileBtnId)i);
		if (i == MBTN_TARGET && s_scopeLatched)
			active = true;
		if (i == MBTN_DEV_FLY && CNoClip::IsActive())
			active = true;

		const float scale = active ? 0.92f : 1.0f;
		const uint8 alpha = (i == MBTN_DEV_TOGGLE && !s_devDrawerOpen) ? 105 : (active ? 255 : 195);
		RwTexture *tex = (b.texId >= 0 && b.texId < MTEX_COUNT) ? s_tex[b.texId] : nil;

		if (tex) {
			const CRGBA tint = active ? CRGBA(255, 215, 245, alpha) : CRGBA(255, 255, 255, alpha);
			DrawTextureQuad(tex, b.cx, b.cy, b.halfW * scale, b.halfH * scale, tint, b.preserveAspect);
		} else {
			const float rad = Min(b.halfW, b.halfH) * scale;
			const CRGBA bg = active ? CRGBA(255, 150, 225, 145) : CRGBA(14, 18, 26, (uint8)(alpha * 0.45f));
			const CRGBA ring = active ? CRGBA(255, 235, 250, 235) : CRGBA(235, 240, 250, (uint8)(alpha * 0.80f));
			DrawFilledCircle(b.cx, b.cy, rad, bg, ring, Max(2.0f, rad * 0.065f));
			DrawFallbackGlyph(b.glyph, b.cx, b.cy, rad, CRGBA(250, 252, 255, alpha));
		}
	}

	RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)FALSE);
#endif
}
