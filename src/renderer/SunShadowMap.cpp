#include "common.h"

#include "SunShadowMap.h"
#include "SunShadowMath.h"
#include "GL3Atmosphere.h"
#include "main.h"
#include "Game.h"
#include "Clock.h"
#include "Weather.h"
#include "Timecycle.h"
#include "Camera.h"
#include "World.h"
#include "Renderer.h"
#include "ModelInfo.h"
#include "VisibilityPlugins.h"
#include "Entity.h"
#include "Vehicle.h"
#include "Ped.h"
#include "PlayerPed.h"
#ifdef ANDROID
#include "Frontend.h"
#include "Timer.h"
#endif

static bool s_sunShadowActive = false;

bool
CSunShadowMap::IsActive(void)
{
	return s_sunShadowActive;
}

#if defined(LIBRW) && defined(RW_GL3)

static bool
ShouldRenderShadowEntity(CEntity *ent, const SunShadowMath::ShadowSetup &setup, float skyFloaterMinZ)
{
	if(ent == nil || !ent->bIsVisible || ent->bIsBIGBuilding || ent->bIsSubway || ent->bUnderwater)
		return false;

	if(ent->m_area != CGame::currArea && ent->m_area != AREA_EVERYWHERE)
		return false;

	CBaseModelInfo *mi = CModelInfo::GetModelInfo(ent->GetModelIndex());
	if(mi == nil)
		return false;

	if(mi->IsSimple()){
		CSimpleModelInfo *smi = (CSimpleModelInfo*)mi;
		if(smi->m_noShadows || smi->m_additive || smi->m_isSubway)
			return false;
	}

	if(mi->GetModelType() == MITYPE_TIME){
		CTimeModelInfo *ti = (CTimeModelInfo*)mi;
		if(!CClock::GetIsTimeInRange(ti->GetTimeOn(), ti->GetTimeOff()))
			return false;
	}

	CVector boundCenter = ent->GetBoundCentre();
#ifdef ANDROID
	float rawBoundRadius = ent->GetBoundRadius();
	if((ent->IsBuilding() || ent->IsObject() || ent->IsDummy()) && rawBoundRadius < 2.2f)
		return false;
	float boundRadius = Max(rawBoundRadius, 1.5f);
#else
	float boundRadius = Max(ent->GetBoundRadius(), 1.5f);
#endif

	// Skip ungrounded interior shells / floating map blocks suspended high in empty sky
	// (custom SA/BR IPLs often place interiors at Z = +80..+1500 with interior=0).
	if((ent->IsBuilding() || ent->IsDummy()) && (boundCenter.z - boundRadius) > skyFloaterMinZ)
		return false;

	if(!SunShadowMath::DoesSphereOverlapShadowVolume(
		setup,
		SunShadowMath::MakeVec3(boundCenter.x, boundCenter.y, boundCenter.z),
		boundRadius))
		return false;

	if(ent->m_rwObject == nil){
		if((ent->IsBuilding() || ent->IsObject() || ent->IsDummy()) && mi->GetRwObject() != nil)
			ent->CreateRwObject();
		if(ent->m_rwObject == nil)
			return false;
	}

	return true;
}

static void
RenderShadowEntity(CEntity *ent)
{
	if(ent->IsVehicle()){
		CVehicle *veh = (CVehicle*)ent;
		CVisibilityPlugins::SetupVehicleVariables((RpClump*)veh->m_rwObject);
		CVisibilityPlugins::InitAlphaAtomicList();

		if(veh->pDriver && veh->pDriver->m_nPedState == PED_DRIVING && veh->pDriver->m_rwObject){
			veh->pDriver->UpdateRpHAnim();
			veh->pDriver->Render();
		}
		for(int i = 0; i < 8; i++){
			if(veh->pPassengers[i] && veh->pPassengers[i]->m_nPedState == PED_DRIVING && veh->pPassengers[i]->m_rwObject){
				veh->pPassengers[i]->UpdateRpHAnim();
				veh->pPassengers[i]->Render();
			}
		}

		veh->Render();
		veh->bImBeingRendered = true;
		CVisibilityPlugins::RenderAlphaAtomics();
		CVisibilityPlugins::RenderBoatAlphaAtomics();
		veh->bImBeingRendered = false;
		return;
	}

	if(ent->IsPed()){
		CPed *ped = (CPed*)ent;
		if(ped->bInVehicle && ped->m_nPedState == PED_DRIVING)
			return; // rendered with vehicle above
		ped->UpdateRpHAnim();
	}

	ent->Render();
}

static void
RenderShadowSectorList(CPtrList &list, const SunShadowMath::ShadowSetup &setup, float skyFloaterMinZ)
{
	for(CPtrNode *node = list.first; node != nil; node = node->next){
		CEntity *ent = (CEntity*)node->item;
		if(ent == nil || ent->m_scanCode == CWorld::GetCurrentScanCode())
			continue;
		ent->m_scanCode = CWorld::GetCurrentScanCode();

		if(!ShouldRenderShadowEntity(ent, setup, skyFloaterMinZ))
			continue;

		RenderShadowEntity(ent);
	}
}

void
CSunShadowMap::Render(void)
{
	s_sunShadowActive = false;

	if(!GL3Atmosphere::Enabled || Scene.camera == nil || !CGame::CanSeeOutSideFromCurrArea()){
		rw::gl3::disableShadowMap();
		return;
	}
#ifdef ANDROID
	int8 mobTier = FrontEndMenuManager.m_PrefsMobileGraphics;
	if(mobTier <= CMenuManager::MOBILE_GFX_LOW){
		rw::gl3::disableShadowMap();
		return;
	}
	static bool s_haveCachedShadow = false;
	static CVector s_lastShadowCamPos(0.0f, 0.0f, 0.0f);
	if(mobTier == CMenuManager::MOBILE_GFX_MEDIUM &&
	   s_haveCachedShadow &&
	   (CTimer::GetFrameCounter() & 1u) != 0u &&
	   (TheCamera.GetPosition() - s_lastShadowCamPos).MagnitudeSqr() < 16.0f){
		s_sunShadowActive = true;
		return;
	}
#endif

	float rawShad = CTimeCycle::GetShadowStrength() / 255.0f;
	float cloudFactor = 1.0f - 0.55f * GL3Atmosphere::Clamp01(CWeather::CloudCoverage)
	                         - 0.45f * GL3Atmosphere::Clamp01(CWeather::Rain);
	if(cloudFactor < 0.18f)
		cloudFactor = 0.18f;

	CVector sunDir = CTimeCycle::GetSunDirection();
	CVector camPos = TheCamera.GetPosition();
	CVector camFwd = TheCamera.GetForward();

	SunShadowMath::ShadowSetup setup = SunShadowMath::ComputeSunShadowSetup(
		SunShadowMath::MakeVec3(sunDir.x, sunDir.y, sunDir.z),
		SunShadowMath::MakeVec3(camPos.x, camPos.y, camPos.z),
		SunShadowMath::MakeVec3(camFwd.x, camFwd.y, camFwd.z),
#ifdef ANDROID
		(mobTier >= CMenuManager::MOBILE_GFX_HIGH) ? 96.0f : 72.0f,
		(mobTier >= CMenuManager::MOBILE_GFX_HIGH) ? 185.0f : 155.0f,
#else
		112.0f,
		200.0f,
#endif
		rw::gl3::SHADOW_MAP_SIZE);

	float strength = Max(rawShad, 0.72f) * setup.fadeStrength * cloudFactor;
	if(!setup.active || strength <= 0.02f){
#ifdef ANDROID
		s_haveCachedShadow = false;
#endif
		rw::gl3::disableShadowMap();
		return;
	}

	if(!rw::gl3::beginShadowPass(
		setup.lightView,
		setup.lightProj,
		Min(1.0f, strength),
		0.075f,
		setup.halfExtentXY)){
#ifdef ANDROID
		s_haveCachedShadow = false;
#endif
		return;
	}

	CVisibilityPlugins::SetRenderWareCamera(Scene.camera);
	float prevCullComps = CVisibilityPlugins::ms_cullCompsDist;
	CVisibilityPlugins::ms_cullCompsDist = 1000000.0f;

	CPed *playerPed = FindPlayerPed();
	float refZ = playerPed ? Max(camPos.z, playerPed->GetPosition().z) : camPos.z;
	const float skyFloaterMinZ = refZ + 55.0f;

	// Expand XY sector scan upstream toward the sun so tall buildings/trees cast shadows into view
#ifdef ANDROID
	const float casterMargin = 52.0f;
#else
	const float casterMargin = 96.0f;
#endif
	const float minWorldX = setup.focusWorld.x - (setup.halfExtentXY + casterMargin);
	const float maxWorldX = setup.focusWorld.x + (setup.halfExtentXY + casterMargin);
	const float minWorldY = setup.focusWorld.y - (setup.halfExtentXY + casterMargin);
	const float maxWorldY = setup.focusWorld.y + (setup.halfExtentXY + casterMargin);

	const int startSX = CWorld::GetSectorIndexX(minWorldX);
	const int endSX   = CWorld::GetSectorIndexX(maxWorldX);
	const int startSY = CWorld::GetSectorIndexY(minWorldY);
	const int endSY   = CWorld::GetSectorIndexY(maxWorldY);

	CWorld::AdvanceCurrentScanCode();

	for(int sy = startSY; sy <= endSY; sy++){
		for(int sx = startSX; sx <= endSX; sx++){
			CSector *sec = CWorld::GetSector(sx, sy);
			if(sec == nil)
				continue;
			RenderShadowSectorList(sec->m_lists[ENTITYLIST_BUILDINGS], setup, skyFloaterMinZ);
			RenderShadowSectorList(sec->m_lists[ENTITYLIST_BUILDINGS_OVERLAP], setup, skyFloaterMinZ);
			RenderShadowSectorList(sec->m_lists[ENTITYLIST_OBJECTS], setup, skyFloaterMinZ);
			RenderShadowSectorList(sec->m_lists[ENTITYLIST_OBJECTS_OVERLAP], setup, skyFloaterMinZ);
			RenderShadowSectorList(sec->m_lists[ENTITYLIST_DUMMIES], setup, skyFloaterMinZ);
			RenderShadowSectorList(sec->m_lists[ENTITYLIST_DUMMIES_OVERLAP], setup, skyFloaterMinZ);
			RenderShadowSectorList(sec->m_lists[ENTITYLIST_VEHICLES], setup, skyFloaterMinZ);
			RenderShadowSectorList(sec->m_lists[ENTITYLIST_VEHICLES_OVERLAP], setup, skyFloaterMinZ);
			RenderShadowSectorList(sec->m_lists[ENTITYLIST_PEDS], setup, skyFloaterMinZ);
			RenderShadowSectorList(sec->m_lists[ENTITYLIST_PEDS_OVERLAP], setup, skyFloaterMinZ);
		}
	}

	CVisibilityPlugins::InitAlphaAtomicList();
	CVisibilityPlugins::ms_cullCompsDist = prevCullComps;

	rw::gl3::endShadowPass();
#ifdef ANDROID
	s_haveCachedShadow = true;
	s_lastShadowCamPos = camPos;
#endif
	s_sunShadowActive = true;
}

#else

void
CSunShadowMap::Render(void)
{
	s_sunShadowActive = false;
}

#endif
