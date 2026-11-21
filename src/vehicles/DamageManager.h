#ifndef __GTA_DAMAGEMANAGER_H__
#define __GTA_DAMAGEMANAGER_H__

#include "common.h"

// TODO: move some of this into Vehicle.h

enum eEngineStatus
{
	ENGINE_STATUS_STEAM1 = 100,
	ENGINE_STATUS_STEAM2 = 150,
	ENGINE_STATUS_SMOKE = 200,
	ENGINE_STATUS_ON_FIRE = 225
};

// SA eDoorStatus (DamageManager.h, 0x6C2320 family). The VC enum only had four
// values and conflated "closed but bashed" with "held shut" and "open" with
// "swinging free"; the SA damage flow needs all four combinations of
// open/closed x ok/damaged, so the numbering below is SA's and the VC spellings
// are aliases onto it. DOOR_STATUS_SWINGING in VC meant "open and swinging
// loose", which is SA's DAMSTATE_OPENED_DAMAGED, not DAMSTATE_OPENED.
enum eDoorStatus : uint8
{
	DAMSTATE_OK             = 0, // closed, DT_DOOR_INTACT
	DAMSTATE_OPENED         = 1, // open,   DT_DOOR_SWINGING_FREE
	DAMSTATE_DAMAGED        = 2, // closed, DT_DOOR_BASHED
	DAMSTATE_OPENED_DAMAGED = 3, // open,   DT_DOOR_BASHED_AND_SWINGING_FREE
	DAMSTATE_NOTPRESENT     = 4, // missing, DT_DOOR_MISSING

	DOOR_STATUS_OK       = DAMSTATE_OK,
	DOOR_STATUS_SMASHED  = DAMSTATE_DAMAGED,
	DOOR_STATUS_SWINGING = DAMSTATE_OPENED_DAMAGED,
	DOOR_STATUS_MISSING  = DAMSTATE_NOTPRESENT,
};

enum ePanelStatus
{
	PANEL_STATUS_OK,
	PANEL_STATUS_SMASHED1,
	PANEL_STATUS_SMASHED2,
	PANEL_STATUS_MISSING,
};

enum eWheelStatus
{
	WHEEL_STATUS_OK,
	WHEEL_STATUS_BURST,
	WHEEL_STATUS_MISSING
};

enum eLightStatus
{
	LIGHT_STATUS_OK,
	LIGHT_STATUS_BROKEN
};

enum tComponent
{
	COMPONENT_DEFAULT,
	COMPONENT_WHEEL_FRONT_LEFT,
	COMPONENT_WHEEL_FRONT_RIGHT,
	COMPONENT_WHEEL_REAR_LEFT,
	COMPONENT_WHEEL_REAR_RIGHT,
	COMPONENT_DOOR_BONNET,
	COMPONENT_DOOR_BOOT,
	COMPONENT_DOOR_FRONT_LEFT,
	COMPONENT_DOOR_FRONT_RIGHT,
	COMPONENT_DOOR_REAR_LEFT,
	COMPONENT_DOOR_REAR_RIGHT,
	COMPONENT_PANEL_FRONT_LEFT,
	COMPONENT_PANEL_FRONT_RIGHT,
	COMPONENT_PANEL_REAR_LEFT,
	COMPONENT_PANEL_REAR_RIGHT,
	COMPONENT_PANEL_WINDSCREEN,
	COMPONENT_BUMPER_FRONT,
	COMPONENT_BUMPER_REAR,
};

enum tComponentGroup
{
	COMPGROUP_BUMPER,
	COMPGROUP_WHEEL,
	COMPGROUP_DOOR,
	COMPGROUP_BONNET,
	COMPGROUP_BOOT,
	COMPGROUP_PANEL,
	COMPGROUP_DEFAULT,
};

enum eLights;

class CDamageManager
{
public:

	float m_fWheelDamageEffect;
	uint8 m_engineStatus;
	uint8 m_wheelStatus[4];
	uint8 m_doorStatus[6];
	uint32 m_lightStatus;
	uint32 m_panelStatus;
	uint8 field_18;

	CDamageManager(void);

	void ResetDamageStatus(void);
	void FuckCarCompletely(void);
	bool ApplyDamage(tComponent component, float damage, float unused);
	bool GetComponentGroup(tComponent component, tComponentGroup *componentGroup, uint8 *foo);

	void SetDoorStatus(eDoors door, eDoorStatus status);
	eDoorStatus GetDoorStatus(eDoors door) const;
	bool ProgressDoorDamage(eDoors door);
	// SA CDamageManager (Damagemanager.cpp 0x6C26xx): the flag helpers the door
	// spring model and CAutomobile::ProcessSwingingDoor drive the flow with.
	bool IsDoorOpen(eDoors door) const;
	bool IsDoorClosed(eDoors door) const;
	bool IsDoorPresent(eDoors door) const;
	bool IsDoorDamaged(eDoors door) const;
	void SetDoorOpen(eDoors door);
	void SetDoorClosed(eDoors door);
	static eCarNodes GetCarNodeIndexFromDoor(eDoors door);
	void SetPanelStatus(int32 panel, uint32 status);
	int32 GetPanelStatus(int32 panel);
	bool ProgressPanelDamage(uint8 panel);
	// needed for CReplay
	static int32 GetPanelStatus(uint32 panelstatus, int32 panel) { return ldb(panel*4, 4, panelstatus); }
	void SetLightStatus(eLights light, uint32 status);
	int32 GetLightStatus(eLights light);
	void SetWheelStatus(int32 wheel, uint32 status);
	int32 GetWheelStatus(int32 wheel);
	bool ProgressWheelDamage(uint8 wheel);
	void SetEngineStatus(uint32 status);
	int32 GetEngineStatus(void);
	bool ProgressEngineDamage(void);
};
VALIDATE_SIZE(CDamageManager, 0x1C);

#endif // __GTA_DAMAGEMANAGER_H__
