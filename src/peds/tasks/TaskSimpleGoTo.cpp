#include "common.h"

#include "TaskSimpleGoTo.h"
#include "Ped.h"

// SA keeps this as a tunable global (0xC18D48): how far off the movement
// direction the target has to be before the ped bothers looking at it.
float CTaskSimpleGoTo::ms_fLookAtThresholdDotProduct = 0.5f;

// SA CTaskSimpleGoTo::CTaskSimpleGoTo (0x6679C0).
CTaskSimpleGoTo::CTaskSimpleGoTo(eMoveState moveState, const CVector &targetPoint, float fRadius)
{
	m_moveState = moveState;
	m_vecTargetPoint = targetPoint;
	m_fRadius = fRadius;
	m_GoToFlags = 0;
}

// SA CTaskSimpleGoTo::HasCircledTarget (0x667A10): the ped has reached the
// target once it has been on all four sides of it (or, with the height
// check enabled, is at least on the same level).
bool
CTaskSimpleGoTo::HasCircledTarget(CPed *ped)
{
	const CVector &pedPos = ped->GetPosition();
	if(!ped->bIgnoreHeightCheckOnGotoPointTask && Abs(pedPos.z - m_vecTargetPoint.z) >= 2.0f)
		return false;

	if(m_vecTargetPoint.x > pedPos.x)
		gotoFlags.m_targetCircledFlags |= 1;
	else if(m_vecTargetPoint.x < pedPos.x)
		gotoFlags.m_targetCircledFlags |= 1 << 1;

	if(m_vecTargetPoint.y > pedPos.y)
		gotoFlags.m_targetCircledFlags |= 1 << 2;
	else if(m_vecTargetPoint.y < pedPos.y)
		gotoFlags.m_targetCircledFlags |= 1 << 3;

	return gotoFlags.m_targetCircledFlags == 0xF;
}

// SA CTaskSimpleGoTo::SetUpIK (0x667AD0) and QuitIK (0x667CA0) drive SA's
// IK chain manager so the ped looks towards where it is walking. That
// manager is not ported yet, so there is never a chain to set up or quit;
// m_bIsIKChainSet therefore stays false.
void
CTaskSimpleGoTo::SetUpIK(CPed *ped)
{
	(void)ped;
}

void
CTaskSimpleGoTo::QuitIK(CPed *ped)
{
	(void)ped;
	gotoFlags.m_bIsIKChainSet = false;
}
