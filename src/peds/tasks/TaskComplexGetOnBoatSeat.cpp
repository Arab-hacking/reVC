#include "common.h"

#include "TaskComplexGetOnBoatSeat.h"

#include "TaskSimpleRunAnim.h"
#include "Vehicle.h"
#include "tHandlingData.h"

CTaskComplexGetOnBoatSeat::CTaskComplexGetOnBoatSeat(CVehicle *vehicle)
	: m_Vehicle(vehicle)
{
	if (m_Vehicle != nil)
		m_Vehicle->RegisterReference((CEntity **)&m_Vehicle);
}

CTaskComplexGetOnBoatSeat::CTaskComplexGetOnBoatSeat(const CTaskComplexGetOnBoatSeat &other)
	: CTaskComplexGetOnBoatSeat(other.m_Vehicle)
{
}

CTaskComplexGetOnBoatSeat::~CTaskComplexGetOnBoatSeat()
{
	if (m_Vehicle != nil)
		m_Vehicle->CleanUpOldReference((CEntity **)&m_Vehicle);
}

bool
CTaskComplexGetOnBoatSeat::MakeAbortable(CPed *ped, eAbortPriority priority, const CEvent *event)
{
	return priority == ABORT_PRIORITY_IMMEDIATE && m_pSubTask->MakeAbortable(ped, priority, event);
}

CTask *
CTaskComplexGetOnBoatSeat::CreateFirstSubTask(CPed *ped)
{
	const AnimationId animId = (m_Vehicle->m_pHandlingData != nil && m_Vehicle->m_pHandlingData->m_bSitInBoat)
		? ANIM_ID_CAR_SIT : ANIM_ID_DRIVE_BOAT;
	return new CTaskSimpleRunAnim(ANIM_GROUP_DEFAULT, animId, 1.0f);
}
