#include "common.h"

#include "EventDraggedOutCar.h"

#include "Entity.h"
#include "Ped.h"
#include "PedGroup.h"
#include "PlayerInfo.h"
#include "Vehicle.h"

CEventDraggedOutCar::CEventDraggedOutCar(CVehicle *vehicle, CPed *carjacker, bool isDriverSeat)
	: m_CarJacker(carjacker), m_Vehicle(vehicle), m_IsDriverSeat(isDriverSeat)
{
	if (m_CarJacker != nil)
		m_CarJacker->RegisterReference((CEntity **)&m_CarJacker);
	if (m_Vehicle != nil)
		m_Vehicle->RegisterReference((CEntity **)&m_Vehicle);
}

CEventDraggedOutCar::~CEventDraggedOutCar()
{
	if (m_CarJacker != nil)
		m_CarJacker->CleanUpOldReference((CEntity **)&m_CarJacker);
	if (m_Vehicle != nil)
		m_Vehicle->CleanUpOldReference((CEntity **)&m_Vehicle);
}

bool
CEventDraggedOutCar::AffectsPed(CPed *ped)
{
	return ped->m_fHealth > 0.0f && m_CarJacker != nil;
}

bool
CEventDraggedOutCar::AffectsPedGroup(CPedGroup *pedGroup)
{
	return (CPed *)FindPlayerPed() == pedGroup->GetMembership().GetLeader();
}

CEventEditableResponse *
CEventDraggedOutCar::CloneEditable() const
{
	return new CEventDraggedOutCar(m_Vehicle, m_CarJacker, m_IsDriverSeat);
}
