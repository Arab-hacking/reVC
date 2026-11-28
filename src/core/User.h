#ifndef __GTA_USER_H__
#define __GTA_USER_H__

#include "Pager.h"
#include "OnscreenTimer.h"

class CZone;
class CVehicle;

class CPlaceName
{
	CZone *m_pZone;
	CZone *m_pZone2;
	int16 m_nAdditionalTimer;
public:
	CPlaceName();
	void Init();
	void Process();
	void Display();
	void ProcessAfterFrontEndShutDown();
};

// SA CCurrentVehicle (User.h). Tracks which vehicle the player is in - or was
// in last - so the HUD can show its model name. SA keeps a single pointer and
// refreshes it once per frame; the name it displays comes from the model's
// game-name field, i.e. the same text the vehicle is called in the data files.
class CCurrentVehicle
{
	CVehicle *m_pCurrentVehicle;
public:
	CCurrentVehicle();

	// SA CCurrentVehicle::Init (0x572030).
	void Init();

	// SA CCurrentVehicle::Process (0x572040): pick up the player's vehicle and
	// refresh the HUD name from it.
	void Process();

	// SA CCurrentVehicle::Display (0x571EA0): show the tracked vehicle's name,
	// or clear the HUD name when the player is on foot.
	void Display();

	// The vehicle being tracked, for callers that want the last one the player
	// was in rather than the current one.
	CVehicle *GetVehicle(void) const { return m_pCurrentVehicle; }
};

class CUserDisplay
{
public:
	static CPlaceName PlaceName;
	static COnscreenTimer OnscnTimer;
	static CPager Pager;
	static CCurrentVehicle CurrentVehicle;

	static void Init();
	static void Process();
};

#endif // __GTA_USER_H__
