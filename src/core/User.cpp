#include "common.h"

#include "GameLogic.h"
#include "Hud.h"
#include "PlayerInfo.h"
#include "PlayerPed.h"
#include "Replay.h"
#include "Text.h"
#include "User.h"
#include "Vehicle.h"
#include "World.h"
#include "Zones.h"

CPlaceName CUserDisplay::PlaceName;
COnscreenTimer CUserDisplay::OnscnTimer;
CPager CUserDisplay::Pager;
CCurrentVehicle CUserDisplay::CurrentVehicle;

CPlaceName::CPlaceName()
{
	Init();
}

void
CPlaceName::Init()
{
	m_pZone = nil;
	m_pZone2 = nil;
	m_nAdditionalTimer = 0;
}

void
CPlaceName::Process()
{
	CVector pos = CWorld::Players[CWorld::PlayerInFocus].GetPos();
	CZone *navigZone = CTheZones::FindSmallestNavigationZoneForPosition(&pos, false, true);
	CZone *defaultZone = CTheZones::FindSmallestNavigationZoneForPosition(&pos, true, false);

	if (navigZone == nil) m_pZone = nil;
	if (defaultZone == nil) m_pZone2 = nil;

	if (navigZone == m_pZone) {
		if (defaultZone == m_pZone2 || m_pZone != nil) {
			if (navigZone != nil || defaultZone != nil) {
				if (m_nAdditionalTimer != 0)
					m_nAdditionalTimer--;
			} else {
				m_nAdditionalTimer = 0;
				m_pZone = nil;
				m_pZone2 = nil;
			}
		} else {
			m_pZone2 = defaultZone;
			m_nAdditionalTimer = 250;
		}
	} else {
		m_pZone = navigZone;
		m_nAdditionalTimer = 250;
	}
	Display();
}

void
CPlaceName::Display()
{
	wchar *text;
	if (m_pZone != nil)
		text = m_pZone->GetTranslatedName();
	else if (m_pZone2 != nil)
		text = m_pZone2->GetTranslatedName();
#ifdef FIX_BUGS
	else
		text = nil;
#endif
	CHud::SetZoneName(text);
}

void
CPlaceName::ProcessAfterFrontEndShutDown(void)
{
	CHud::m_pLastZoneName = nil;
	CHud::m_ZoneState = 0;
	m_nAdditionalTimer = 250;
}

CCurrentVehicle::CCurrentVehicle()
{
	Init();
}

void
CCurrentVehicle::Init()
{
	m_pCurrentVehicle = nil;
}

// SA CCurrentVehicle::Process (0x572040).
//
// SA asks FindPlayerVehicle(), which already accounts for "the player is in a
// vehicle" - the ped pointer this used to go through can be in the middle of a
// get-in and already point at the vehicle while the door animation is still
// running, which is exactly when the name should not appear yet.
void
CCurrentVehicle::Process()
{
	m_pCurrentVehicle = FindPlayerVehicle();
	Display();
}

// SA CCurrentVehicle::Display (0x571EA0).
//
// The short-cut taxi is the instantly-warped cab from a mission failure: it is
// a real vehicle in the world but not one the player is driving, so SA hides
// its name rather than claiming the player is in a taxi.
void
CCurrentVehicle::Display()
{
	wchar *text = nil;
	if (m_pCurrentVehicle != nil && m_pCurrentVehicle != CGameLogic::pShortCutTaxi){
		CVehicleModelInfo *mi = m_pCurrentVehicle->GetModelInfo();
		if (mi)
			text = TheText.Get(mi->m_gameName);
	}
	CHud::SetVehicleName(text);
}

void
CUserDisplay::Init()
{
	PlaceName.Init();
	OnscnTimer.Init();
	Pager.Init();
	CurrentVehicle.Init();
}

void
CUserDisplay::Process()
{
#ifdef FIX_BUGS
	if (CReplay::IsPlayingBack())
		return;
#endif
	PlaceName.Process();
	OnscnTimer.Process();
	Pager.Process();
	CurrentVehicle.Process();
}
