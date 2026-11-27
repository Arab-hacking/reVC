#include "common.h"

#include "TaskSimpleCarWaitToSlowDown.h"

#include "EventDamage.h"
#include "Ped.h"
#include "Vehicle.h"

CTaskSimpleCarWaitToSlowDown::CTaskSimpleCarWaitToSlowDown(CVehicle *targetVehicle, SlowDownType type)
	: m_vehicle(targetVehicle), m_type(type)
{
	if (m_vehicle != nil)
		m_vehicle->RegisterReference((CEntity **)&m_vehicle);
}

CTaskSimpleCarWaitToSlowDown::CTaskSimpleCarWaitToSlowDown(const CTaskSimpleCarWaitToSlowDown &other)
	: CTaskSimpleCarWaitToSlowDown(other.m_vehicle, other.m_type)
{
}

CTaskSimpleCarWaitToSlowDown::~CTaskSimpleCarWaitToSlowDown()
{
	if (m_vehicle != nil)
		m_vehicle->CleanUpOldReference((CEntity **)&m_vehicle);
}

bool
CTaskSimpleCarWaitToSlowDown::MakeAbortable(CPed *ped, eAbortPriority priority, const CEvent *event)
{
	if (priority != ABORT_PRIORITY_URGENT)
		return false;
	if (event == nil || event->GetEventType() != EVENT_DAMAGE)
		return false;
	return ((CEventDamage *)event)->HasKilledPed();
}

bool
CTaskSimpleCarWaitToSlowDown::ProcessPed(CPed *ped)
{
	if (m_vehicle == nil)
		return true;

	if (!ped->IsPlayer() && m_vehicle->IsDriver(ped)) {
		// Stop the car; the driver of it, that is.
		if (m_vehicle->GetStatus() == STATUS_PLAYER) {
			m_vehicle->AutoPilot.m_nDrivingStyle = DRIVINGSTYLE_AVOID_CARS;
			m_vehicle->AutoPilot.m_nCruiseSpeed = 0;
		}
		if (m_vehicle->GetStatus() == STATUS_PHYSICS)
			m_vehicle->SetStatus(STATUS_ABANDONED);
	}

	switch (m_type) {
	case DONT_WAIT:
		return true;
	case SLOW_ENOUGH_TO_STEP:
		return m_vehicle->CanPedStepOutCar(false);
	default: // SLOW_ENOUGH_TO_STEP_OR_JUMP
		return m_vehicle->CanPedStepOutCar(false) || m_vehicle->CanPedJumpOutCar(ped);
	}
}
