#include "common.h"

#include "AutoPilot.h"

#include "SaveBuf.h"

#ifdef COMPATIBLE_SAVES
void CAutoPilot::Save(uint8*& buf)
{
	WriteSaveBuf(buf, m_nCurrentRouteNode);
	WriteSaveBuf(buf, m_nNextRouteNode);
	WriteSaveBuf(buf, m_nPrevRouteNode);
	WriteSaveBuf(buf, m_nTimeEnteredCurve);
	WriteSaveBuf(buf, m_nTimeToSpendOnCurrentCurve);
	WriteSaveBuf(buf, m_nCurrentPathNodeInfo);
	WriteSaveBuf(buf, m_nNextPathNodeInfo);
	WriteSaveBuf(buf, m_nPreviousPathNodeInfo);
	WriteSaveBuf(buf, m_nAntiReverseTimer);
	WriteSaveBuf(buf, m_nTimeToStartMission);
	WriteSaveBuf(buf, m_nPreviousDirection);
	WriteSaveBuf(buf, m_nCurrentDirection);
	WriteSaveBuf(buf, m_nNextDirection);
	WriteSaveBuf(buf, m_nCurrentLane);
	WriteSaveBuf(buf, m_nNextLane);
	WriteSaveBuf(buf, m_nDrivingStyle);
	WriteSaveBuf(buf, m_nCarMission);
	WriteSaveBuf(buf, m_nTempAction);
	WriteSaveBuf(buf, m_nTimeTempAction);
	WriteSaveBuf(buf, m_fMaxTrafficSpeed);
	WriteSaveBuf(buf, m_nCruiseSpeed);
	WriteSaveBuf(buf, m_nCruiseSpeedMultiplierType);
	ZeroSaveBuf(buf, 2);
	WriteSaveBuf(buf, m_fCruiseSpeedMultiplier);
	uint8 flags = 0;
	if (m_bSlowedDownBecauseOfCars) flags |= BIT(0);
	if (m_bSlowedDownBecauseOfPeds) flags |= BIT(1);
	if (m_bStayInCurrentLevel) flags |= BIT(2);
	if (m_bStayInFastLane) flags |= BIT(3);
	if (m_bIgnorePathfinding) flags |= BIT(4);
	WriteSaveBuf(buf, flags);
	WriteSaveBuf(buf, m_nSwitchDistance);
	ZeroSaveBuf(buf, 2);
	WriteSaveBuf(buf, m_vecDestinationCoors.x);
	WriteSaveBuf(buf, m_vecDestinationCoors.y);
	WriteSaveBuf(buf, m_vecDestinationCoors.z);
	ZeroSaveBuf(buf, 32);
	WriteSaveBuf(buf, m_nPathFindNodesCount);
	ZeroSaveBuf(buf, 6);
}

void CAutoPilot::Load(uint8*& buf)
{
	ReadSaveBuf(&m_nCurrentRouteNode, buf);
	ReadSaveBuf(&m_nNextRouteNode, buf);
	ReadSaveBuf(&m_nPrevRouteNode, buf);
	ReadSaveBuf(&m_nTimeEnteredCurve, buf);
	ReadSaveBuf(&m_nTimeToSpendOnCurrentCurve, buf);
	ReadSaveBuf(&m_nCurrentPathNodeInfo, buf);
	ReadSaveBuf(&m_nNextPathNodeInfo, buf);
	ReadSaveBuf(&m_nPreviousPathNodeInfo, buf);
	ReadSaveBuf(&m_nAntiReverseTimer, buf);
	ReadSaveBuf(&m_nTimeToStartMission, buf);
	ReadSaveBuf(&m_nPreviousDirection, buf);
	ReadSaveBuf(&m_nCurrentDirection, buf);
	ReadSaveBuf(&m_nNextDirection, buf);
	ReadSaveBuf(&m_nCurrentLane, buf);
	ReadSaveBuf(&m_nNextLane, buf);
	ReadSaveBuf(&m_nDrivingStyle, buf);
	ReadSaveBuf(&m_nCarMission, buf);
	ReadSaveBuf(&m_nTempAction, buf);
	ReadSaveBuf(&m_nTimeTempAction, buf);
	ReadSaveBuf(&m_fMaxTrafficSpeed, buf);
	ReadSaveBuf(&m_nCruiseSpeed, buf);
	ReadSaveBuf(&m_nCruiseSpeedMultiplierType, buf);
	SkipSaveBuf(buf, 2);
	ReadSaveBuf(&m_fCruiseSpeedMultiplier, buf);
	uint8 flags;
	ReadSaveBuf(&flags, buf);
	m_bSlowedDownBecauseOfCars = !!(flags & BIT(0));
	m_bSlowedDownBecauseOfPeds = !!(flags & BIT(1));
	m_bStayInCurrentLevel = !!(flags & BIT(2));
	m_bStayInFastLane = !!(flags & BIT(3));
	m_bIgnorePathfinding = !!(flags & BIT(4));
	ReadSaveBuf(&m_nSwitchDistance, buf);
	SkipSaveBuf(buf, 2);
	ReadSaveBuf(&m_vecDestinationCoors.x, buf);
	ReadSaveBuf(&m_vecDestinationCoors.y, buf);
	ReadSaveBuf(&m_vecDestinationCoors.z, buf);
	SkipSaveBuf(buf, 32);
	ReadSaveBuf(&m_nPathFindNodesCount, buf);
	SkipSaveBuf(buf, 6);
}
#endif
