#include "common.h"

#include "EventKnockOffBike.h"

#include "Ped.h"
#include "Vehicle.h"

CEventKnockOffBike::CEventKnockOffBike(CVehicle *vehicle, const CVector &moveSpeed, const CVector &collisionImpactVelocity,
	float damageIntensity, float a6, uint8 knockOffType, uint8 knockOutDirection, int32 time,
	CPed *ped, bool isVictimDriver, bool forceKnockOff)
	: m_moveSpeed(moveSpeed), m_collisionImpactVelocity(collisionImpactVelocity),
	  m_damageIntensity(damageIntensity), m_field_28(a6), m_time((uint32)time), m_ped(ped),
	  m_knockOffDirection(knockOutDirection), m_isVictimDriver(isVictimDriver), m_forceKnockOff(forceKnockOff),
	  m_knockOffType(knockOffType), m_exitDoor(0), m_vehicle(vehicle)
{
	if (m_vehicle != nil)
		m_vehicle->RegisterReference((CEntity **)&m_vehicle);
	if (m_ped != nil)
		m_ped->RegisterReference((CEntity **)&m_ped);
}

CEventKnockOffBike::CEventKnockOffBike()
	: m_damageIntensity(0.0f), m_field_28(0.0f), m_time(0), m_ped(nil), m_knockOffDirection(0),
	  m_isVictimDriver(false), m_forceKnockOff(true), m_knockOffType(0), m_exitDoor(0), m_vehicle(nil)
{
}

CEventKnockOffBike::~CEventKnockOffBike()
{
	if (m_vehicle != nil)
		m_vehicle->CleanUpOldReference((CEntity **)&m_vehicle);
	if (m_ped != nil)
		m_ped->CleanUpOldReference((CEntity **)&m_ped);
}

CEvent *
CEventKnockOffBike::Clone() const
{
	return new CEventKnockOffBike(m_vehicle, m_moveSpeed, m_collisionImpactVelocity, m_damageIntensity, m_field_28,
		m_knockOffType, m_knockOffDirection, (int32)m_time, m_ped, m_isVictimDriver, m_forceKnockOff);
}

bool
CEventKnockOffBike::AffectsPed(CPed *ped)
{
	if (ped->m_fHealth > 0.0f) {
		if (m_vehicle != nil && m_vehicle->GetStatus() == STATUS_GHOST)
			return false;
		// This codebase has no can-not-be-knocked-off flag on the ped;
		// the force flag is recorded, and the response honours it.
		if (m_vehicle != nil) {
			if (m_vehicle->pDriver != ped &&
			    m_vehicle->pPassengers[0] != ped &&
			    m_vehicle->pPassengers[1] != ped &&
			    m_vehicle->pPassengers[2] != ped)
				return false;
			if (m_vehicle->pDriver == ped)
				m_isVictimDriver = true;
			return true;
		}
	}
	return false;
}

void
CEventKnockOffBike::ReportCriminalEvent(CPed *ped)
{
}
