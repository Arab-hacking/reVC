#include "TaskSimpleAchieveHeading.h"

#include "General.h"
#include "Ped.h"

// SA CTaskSimpleAchieveHeading (0x667E20). Adaptations: SA steers through
// m_fAimingRotation and an IK look-at; this codebase turns the ped
// towards m_fRotationDest in its own update and has no IK chain manager,
// so the IK hooks (SetUpIK/QuitIK) are absent and m_IsDoingIK stays
// false. SA scales m_fHeadingChangeRate; here that is m_headingRate.

CTaskSimpleAchieveHeading::CTaskSimpleAchieveHeading(float desiredHeading, float changeRateMult, float tolerance)
: m_DesiredHeadingRad(desiredHeading), m_HeadingChangeRate(changeRateMult), m_HeadingToleranceRad(tolerance)
{
	m_IsDoingIK = false;
}

// 0x668060
bool
CTaskSimpleAchieveHeading::ProcessPed(CPed *ped)
{
	if(ped->bInVehicle)
		return true;

	ped->RestoreHeadingRate();
	ped->m_headingRate *= m_HeadingChangeRate;
	ped->SetMoveState(PEDMOVE_STILL);
	ped->m_fRotationDest = m_DesiredHeadingRad;

	if(Abs(CGeneral::LimitRadianAngle(m_DesiredHeadingRad - ped->m_fRotationCur)) < m_HeadingToleranceRad){
		ped->m_fRotationDest = ped->m_fRotationCur;
		ped->RestoreHeadingRate();
		return true;
	}

	return false;
}

// 0x667EB0
bool
CTaskSimpleAchieveHeading::MakeAbortable(CPed *ped, eAbortPriority priority, const CEvent *event)
{
	(void)event;

	switch(priority){
	case ABORT_PRIORITY_IMMEDIATE:
		ped->m_fRotationDest = ped->m_fRotationCur;
		ped->RestoreHeadingRate();
		return true;
	default:
		m_HeadingToleranceRad = TWOPI;
		ped->m_fRotationDest = ped->m_fRotationCur;
		return false;
	}
}

void
CTaskSimpleAchieveHeading::SetHeading(float heading, float maxHeading, float changeRateMult)
{
	m_DesiredHeadingRad = heading;
	m_HeadingChangeRate = changeRateMult;
	m_HeadingToleranceRad = maxHeading;
}
