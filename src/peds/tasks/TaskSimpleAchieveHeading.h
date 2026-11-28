#pragma once

// SA CTaskSimpleAchieveHeading: turn on the spot until the ped faces the
// desired heading (within tolerance). SA also drives an IK look-at while
// turning; the IK chain manager is not ported yet, so the turn here is
// body-only (m_fRotationDest is what VC's ped update turns towards).

#include "TaskSimple.h"

class CTaskSimpleAchieveHeading : public CTaskSimple
{
public:
	float m_DesiredHeadingRad;
	float m_HeadingChangeRate;
	float m_HeadingToleranceRad;
	bool m_IsDoingIK;

public:
	CTaskSimpleAchieveHeading(float desiredHeading, float changeRateMult, float tolerance);

	CTask *Clone() const { return new CTaskSimpleAchieveHeading(m_DesiredHeadingRad, m_HeadingChangeRate, m_HeadingToleranceRad); }
	eTaskType GetTaskType() const { return TASK_SIMPLE_ACHIEVE_HEADING; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_LEISURE, const CEvent *event = nil);
	bool ProcessPed(CPed *ped);

	void SetHeading(float heading, float maxHeading, float changeRateMult);
};

VALIDATE_SIZE(CTaskSimpleAchieveHeading, 0x14);
