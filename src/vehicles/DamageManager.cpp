#include "common.h"

#include "General.h"
#include "Vehicle.h"
#include "DamageManager.h"


float G_aComponentDamage[] = { 2.5f, 1.25f, 3.2f, 1.4f, 2.5f, 2.8f, 0.5f };

CDamageManager::CDamageManager(void)
{
	ResetDamageStatus();
	m_fWheelDamageEffect = 0.5f;
	field_18 = 1;
}

void
CDamageManager::ResetDamageStatus(void)
{
	int i;
	m_fWheelDamageEffect = 0.0f;
	m_engineStatus = 0;
	for(i = 0; i < ARRAY_SIZE(m_wheelStatus); i++) m_wheelStatus[i] = 0;
	for(i = 0; i < ARRAY_SIZE(m_doorStatus); i++) m_doorStatus[i] = 0;
	m_lightStatus = 0;
	m_panelStatus = 0;
}

void
CDamageManager::FuckCarCompletely(void)
{
	int i;

	m_wheelStatus[0] = WHEEL_STATUS_MISSING;
	// wheels 1-3 not reset?

	for(i = 0; i < ARRAY_SIZE(m_doorStatus); i++)
		m_doorStatus[i] = DOOR_STATUS_MISSING;

	for(i = 0; i < 3; i++){
#ifdef FIX_BUGS
		ProgressPanelDamage(VEHBUMPER_FRONT);
		ProgressPanelDamage(VEHBUMPER_REAR);
#else
		// this can't be right
		ProgressPanelDamage(COMPONENT_BUMPER_FRONT);
		ProgressPanelDamage(COMPONENT_BUMPER_REAR);
#endif
	}
	// Why set to no damage?
#ifndef FIX_BUGS
	m_lightStatus = 0;
	m_panelStatus = 0;
#endif
	SetEngineStatus(250);
}

bool
CDamageManager::ApplyDamage(tComponent component, float damage, float unused)
{
	tComponentGroup group;
	uint8 subComp;

	GetComponentGroup(component, &group, &subComp);
	damage *= G_aComponentDamage[group];
	if(component == COMPONENT_PANEL_WINDSCREEN)
		damage *= 0.6f;
	if(damage > 150.0f){
		switch(group){
		case COMPGROUP_WHEEL:
			ProgressWheelDamage(subComp);
			break;
		case COMPGROUP_DOOR:
		case COMPGROUP_BOOT:
			ProgressDoorDamage((eDoors)subComp);
			break;
		case COMPGROUP_BONNET:
			if(damage > 220.0f)
				ProgressEngineDamage();
			ProgressDoorDamage((eDoors)subComp);
			break;
		case COMPGROUP_PANEL:
			// so windscreen is a light?
			SetLightStatus((eLights)subComp, 1);
			// fall through
		case COMPGROUP_BUMPER:
			if(damage > 220.0f &&
			   (component == COMPONENT_PANEL_FRONT_LEFT ||
			    component == COMPONENT_PANEL_FRONT_RIGHT ||
			    component == COMPONENT_PANEL_WINDSCREEN))
				ProgressEngineDamage();
			ProgressPanelDamage(subComp);
			break;
		default: break;
		}
		return true;
	}
	return false;
}

bool
CDamageManager::GetComponentGroup(tComponent component, tComponentGroup *componentGroup, uint8 *subComp)
{
	*subComp = -2;	// ??

	// This is done very strangely in the game, maybe an optimized switch?
	if(component >= COMPONENT_PANEL_FRONT_LEFT){
		if(component >= COMPONENT_BUMPER_FRONT)
			*componentGroup = COMPGROUP_BUMPER;
		else
			*componentGroup = COMPGROUP_PANEL;
		*subComp = component - COMPONENT_PANEL_FRONT_LEFT;
		return true;
	}else if(component >= COMPONENT_DOOR_BONNET){
		if(component == COMPONENT_DOOR_BONNET)
			*componentGroup = COMPGROUP_BONNET;
		else if(component == COMPONENT_DOOR_BOOT)
			*componentGroup = COMPGROUP_BOOT;
		else
			*componentGroup = COMPGROUP_DOOR;
		*subComp = component - COMPONENT_DOOR_BONNET;
		return true;
	}else if(component >= COMPONENT_WHEEL_FRONT_LEFT){
		*componentGroup = COMPGROUP_WHEEL;
		*subComp = component - COMPONENT_WHEEL_FRONT_LEFT;
		return true;
	}else if(component >= COMPONENT_DEFAULT){
		*componentGroup = COMPGROUP_DEFAULT;
		*subComp = COMPONENT_DEFAULT;
		return true;
	}else
		return false;
}

void
CDamageManager::SetDoorStatus(eDoors door, eDoorStatus status)
{
	if((uint32)door >= (uint32)MAX_DOORS)
		return;
	m_doorStatus[door] = status;
}

// SA CDamageManager::GetDoorStatus (0x6C26B0), verbatim: anything outside the
// six door indices reads as missing rather than indexing past the array.
eDoorStatus
CDamageManager::GetDoorStatus(eDoors door) const
{
	switch(door){
	case DOOR_BONNET:
	case DOOR_BOOT:
	case DOOR_FRONT_LEFT:
	case DOOR_FRONT_RIGHT:
	case DOOR_REAR_LEFT:
	case DOOR_REAR_RIGHT:
		return (eDoorStatus)m_doorStatus[(uint32)door];
	default:
		return DAMSTATE_NOTPRESENT;
	}
}

// SA CDamageManager::GetCarNodeIndexFromDoor (0x6C26F0), verbatim.
eCarNodes
CDamageManager::GetCarNodeIndexFromDoor(eDoors door)
{
	switch(door){
	case DOOR_BONNET:      return CAR_BONNET;
	case DOOR_BOOT:        return CAR_BOOT;
	case DOOR_FRONT_LEFT:  return CAR_DOOR_LF;
	case DOOR_FRONT_RIGHT: return CAR_DOOR_RF;
	case DOOR_REAR_LEFT:   return CAR_DOOR_LR;
	case DOOR_REAR_RIGHT:  return CAR_DOOR_RR;
	default:               return CAR_BONNET;
	}
}

// SA CDamageManager::IsDoorOpen (0x6C27xx), verbatim.
bool
CDamageManager::IsDoorOpen(eDoors door) const
{
	switch(GetDoorStatus(door)){
	case DAMSTATE_OPENED:
	case DAMSTATE_OPENED_DAMAGED:
		return true;
	case DAMSTATE_NOTPRESENT:
		// SA asserts here too; the assert is a debug-only note that
		// !IsDoorOpen() == IsDoorClosed() does not hold for a missing door.
		return false;
	}
	return false;
}

// SA CDamageManager::IsDoorClosed, verbatim.
bool
CDamageManager::IsDoorClosed(eDoors door) const
{
	switch(GetDoorStatus(door)){
	case DAMSTATE_OK:
	case DAMSTATE_DAMAGED:
		return true;
	case DAMSTATE_NOTPRESENT:
		return false;
	}
	return false;
}

bool
CDamageManager::IsDoorPresent(eDoors door) const
{
	return GetDoorStatus(door) != DAMSTATE_NOTPRESENT;
}

bool
CDamageManager::IsDoorDamaged(eDoors door) const
{
	switch(GetDoorStatus(door)){
	case DAMSTATE_OK:
	case DAMSTATE_OPENED:
		return false;
	default:
		return true;
	}
}

void
CDamageManager::SetDoorOpen(eDoors door)
{
	switch(GetDoorStatus(door)){
	case DAMSTATE_OK:
		SetDoorStatus(door, DAMSTATE_OPENED);
		break;
	case DAMSTATE_DAMAGED:
		SetDoorStatus(door, DAMSTATE_OPENED_DAMAGED);
		break;
	case DAMSTATE_NOTPRESENT:
		break; /* do nothing */
	}
}

void
CDamageManager::SetDoorClosed(eDoors door)
{
	switch(GetDoorStatus(door)){
	case DAMSTATE_OPENED:
		SetDoorStatus(door, DAMSTATE_OK);
		break;
	case DAMSTATE_OPENED_DAMAGED:
		SetDoorStatus(door, DAMSTATE_DAMAGED);
		break;
	case DAMSTATE_NOTPRESENT:
		break; /* do nothing */
	}
}

// SA CDamageManager::ProgressDoorDamage (0x6C2320), verbatim. The VC version
// just walked DOOR_STATUS_OK..MISSING in order, which with SA's numbering would
// turn a bashed-but-shut door straight into an open one.
bool
CDamageManager::ProgressDoorDamage(eDoors door)
{
	if((uint32)door >= (uint32)MAX_DOORS)
		return false;

	switch(GetDoorStatus(door)){
	case DAMSTATE_OK:
	case DAMSTATE_OPENED:
		SetDoorStatus(door, DAMSTATE_DAMAGED);
		break;
	case DAMSTATE_DAMAGED:
		SetDoorStatus(door, DAMSTATE_OPENED_DAMAGED);
		break;
	case DAMSTATE_OPENED_DAMAGED:
		if(CGeneral::GetRandomNumberInRange(0, 8))
			return false;
		SetDoorStatus(door, DAMSTATE_NOTPRESENT);
		break;
	case DAMSTATE_NOTPRESENT:
		return false;
	}
	return true;
}

void
CDamageManager::SetPanelStatus(int32 panel, uint32 status)
{
	m_panelStatus = dpb(status, panel*4, 4, m_panelStatus);
}

int32
CDamageManager::GetPanelStatus(int32 panel)
{
	return ldb(panel*4, 4, m_panelStatus);
}

bool
CDamageManager::ProgressPanelDamage(uint8 panel)
{
	int status = GetPanelStatus(panel);
	if(status == DOOR_STATUS_MISSING)
		return false;
	SetPanelStatus(panel, status+1);
	return true;
}

void
CDamageManager::SetLightStatus(eLights light, uint32 status)
{
	m_lightStatus = dpb(status, light*2, 2, m_lightStatus);
}

int32
CDamageManager::GetLightStatus(eLights light)
{
	return ldb(light*2, 2, m_lightStatus);
}

void
CDamageManager::SetWheelStatus(int32 wheel, uint32 status)
{
	m_wheelStatus[wheel] = status;
}

int32
CDamageManager::GetWheelStatus(int32 wheel)
{
	return m_wheelStatus[wheel];
}

bool
CDamageManager::ProgressWheelDamage(uint8 wheel)
{
	int status = GetWheelStatus(wheel);
	if(status == WHEEL_STATUS_MISSING)
		return false;
	SetWheelStatus(wheel, status+1);
	return true;
}

void
CDamageManager::SetEngineStatus(uint32 status)
{
	if(status > 250)
		m_engineStatus = 250;
	else
		m_engineStatus = status;
}

int32
CDamageManager::GetEngineStatus(void)
{
	return m_engineStatus;
}

bool
CDamageManager::ProgressEngineDamage(void)
{
	// gone in VC
	return false;
}
