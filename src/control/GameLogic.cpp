#include "common.h"
#include "Text.h"

#include "GameLogic.h"
#include "Clock.h"
#include "Stats.h"
#include "Pickups.h"
#include "Timer.h"
#include "Streaming.h"
#include "World.h"
#include "PlayerPed.h"
#include "Wanted.h"
#include "Camera.h"
#include "Messages.h"
#include "Pad.h"
#include "References.h"
#include "Fire.h"
#include "General.h"
#include "DMAudio.h"
#include "Radar.h"
#include "Pools.h"
#include "Hud.h"
#include "Particle.h"
#include "ColStore.h"
#include "Automobile.h"
#include "MBlur.h"
#include "screendroplets.h"

uint8 CGameLogic::ActivePlayers;


void
CGameLogic::InitAtStartOfGame()
{
	ActivePlayers = 1;
}

void
CGameLogic::PassTime(uint32 time)
{
	int32 minutes, hours, days;

	minutes = time + CClock::GetMinutes();
	hours = CClock::GetHours();

	for (; minutes >= 60; minutes -= 60)
		hours++;

	if (hours > 23) {
		days = CStats::DaysPassed;
		for (; hours >= 24; hours -= 24)
			days++;
		CStats::DaysPassed = days;
	}

	CClock::SetGameClock(hours, minutes);
	CPickups::PassTime(time * 1000);
}

void 
CGameLogic::SortOutStreamingAndMemory(const CVector &pos)
{
	CTimer::Stop();
	CStreaming::FlushRequestList();
	CStreaming::DeleteRwObjectsAfterDeath(pos);
	CStreaming::RemoveUnusedModelsInLoadedList();
	CGame::DrasticTidyUpMemory(true);
	CWorld::Players[CWorld::PlayerInFocus].m_pPed->Undress("player");
	CStreaming::LoadSceneCollision(pos);
	CStreaming::LoadScene(pos);
	CWorld::Players[CWorld::PlayerInFocus].m_pPed->Dress();
	CTimer::Update();
}

void
CGameLogic::Update()
{
	CPlayerInfo &pPlayerInfo = CWorld::Players[CWorld::PlayerInFocus];

	switch (pPlayerInfo.m_WBState) {
	case WBSTATE_PLAYING:
		if (pPlayerInfo.m_pPed->m_nPedState == PED_DEAD) {
			pPlayerInfo.m_pPed->ClearAdrenaline();
			pPlayerInfo.KillPlayer();
		}
		break;
	case WBSTATE_WASTED:
		if ((CTimer::GetTimeInMilliseconds() - pPlayerInfo.m_nWBTime > 0x800) && (CTimer::GetPreviousTimeInMilliseconds() - pPlayerInfo.m_nWBTime <= 0x800)) {
			TheCamera.SetFadeColour(200, 200, 200);
			TheCamera.Fade(2.0f, FADE_OUT);
		}

		if (CTimer::GetTimeInMilliseconds() - pPlayerInfo.m_nWBTime >= 0x1000) {
			CVector vecRestartPos;
			float fRestartFloat;

			pPlayerInfo.m_WBState = WBSTATE_PLAYING;
			pPlayerInfo.m_nMoney = Max(0, pPlayerInfo.m_nMoney - 100);
			pPlayerInfo.m_pPed->ClearWeapons();

			if (pPlayerInfo.m_pPed->bInVehicle) {
				CVehicle *pVehicle = pPlayerInfo.m_pPed->m_pMyVehicle;
				if (pVehicle != nil) {
					if (pVehicle->pDriver == pPlayerInfo.m_pPed) {
						pVehicle->pDriver = nil;
						if (pVehicle->GetStatus() != STATUS_WRECKED)
							pVehicle->SetStatus(STATUS_ABANDONED);
					} else
						pVehicle->RemovePassenger(pPlayerInfo.m_pPed);
				}
			}
#ifdef SCREEN_DROPLETS
			ScreenDroplets::Initialise();
#endif
			CMessages::ClearMessages();
			CWorld::ClearExcitingStuffFromArea(pPlayerInfo.GetPos(), 4000.0f, true);
			CGame::GetSpawnPosition(vecRestartPos, fRestartFloat);
			PassTime(720);
			RestorePlayerStuffDuringResurrection(pPlayerInfo.m_pPed, vecRestartPos, fRestartFloat);
			SortOutStreamingAndMemory(pPlayerInfo.GetPos());
			TheCamera.m_fCamShakeForce = 0.0f;
			TheCamera.SetMotionBlur(0, 0, 0, 0, MOTION_BLUR_NONE);
			CPad::GetPad(0)->StopShaking(0);
			CReferences::RemoveReferencesToPlayer();
			CPad::GetPad(CWorld::PlayerInFocus)->DisablePlayerControls = PLAYERCONTROL_ENABLED;
			TheCamera.SetFadeColour(200, 200, 200);
			TheCamera.Fade(4.0f, FADE_IN);
		}
		break;
	default:
		break;
	}
}

void
CGameLogic::RestorePlayerStuffDuringResurrection(CPlayerPed *pPlayerPed, CVector pos, float angle)
{
	CPlayerInfo* pPlayerInfo = pPlayerPed->GetPlayerInfoForThisPlayerPed();
	pPlayerPed->m_fHealth = pPlayerInfo->m_nMaxHealth;
	pPlayerPed->m_fArmour = 0.0f;
	pPlayerPed->bIsVisible = true;
	pPlayerPed->m_bloodyFootprintCountOrDeathTime = 0;
	pPlayerPed->bDoBloodyFootprints = false;
	pPlayerPed->Drunkenness() = 0;
	pPlayerPed->FadeDrunkenness() = 0;
	CMBlur::ClearDrunkBlur();
	pPlayerPed->m_nDrunkCountdown = 0;
	pPlayerPed->ClearAdrenaline();
	// SA CGameLogic::ClearPlayerWantedLevelAndResetPlayer restores the sprint
	// budget from the stamina stat rather than to a fixed value.
	pPlayerPed->ResetSprintEnergy();
	if (pPlayerPed->m_pFire)
		pPlayerPed->m_pFire->Extinguish();
	pPlayerPed->bInVehicle = false;
	pPlayerPed->m_pMyVehicle = nil;
	pPlayerPed->m_pVehicleAnim = nil;
	pPlayerPed->Wanted()->Reset();
	pPlayerPed->bCancelEnteringCar = false;
	pPlayerPed->RestartNonPartialAnims();
	pPlayerPed->bRemoveFromWorld = false;
	pPlayerPed->ClearWeaponTarget();
	pPlayerPed->SetInitialState();
	pPlayerPed->Teleport(pos + CVector(0.0f, 0.0f, 1.0f));
	pPlayerPed->SetMoveSpeed(0.0f, 0.0f, 0.0f);
	pPlayerPed->m_fRotationCur = DEGTORAD(angle);
	pPlayerPed->m_fRotationDest = pPlayerPed->m_fRotationCur;
	pPlayerPed->SetHeading(pPlayerPed->m_fRotationCur);
	CWorld::ClearExcitingStuffFromArea(pos, 4000.0f, true);
	pPlayerPed->RestoreHeadingRate();
	CGame::currArea = AREA_MAIN_MAP;
	CStreaming::RemoveBuildingsNotInArea(AREA_MAIN_MAP);
	TheCamera.SetCameraDirectlyInFrontForFollowPed_CamOnAString();
	TheCamera.Restore();
	CReferences::RemoveReferencesToPlayer();
	CWorld::Remove(pPlayerPed);
	CWorld::Add(pPlayerPed);
	CHud::ResetWastedText();
	clearWaterDrop = true;
}
