#include "common.h"

#include "TrainNode.h"
#include "World.h"

// SA CTrainNode::CTrainNode (0x6F5370).
CTrainNode::CTrainNode(void)
{
	m_bSurfLightingFound = false;
	m_nDayLighting = 0;
	m_nNightLighting = 0;
}

// SA CTrainNode::SetX/SetY/SetZ (0x6F5380/0x6F53A0/0x6F53C0): store in
// eighths of a unit.
void CTrainNode::SetX(float X) { x = (int16)(X * 8.0f); }
void CTrainNode::SetY(float Y) { y = (int16)(Y * 8.0f); }
void CTrainNode::SetZ(float Z) { z = (int16)(Z * 8.0f); }

// SA CTrainNode::GetX/GetY/GetZ (0x6F53E0/0x6F5400/0x6F5420).
float CTrainNode::GetX(void) const { return (float)x * 0.125f; }
float CTrainNode::GetY(void) const { return (float)y * 0.125f; }
float CTrainNode::GetZ(void) const { return (float)z * 0.125f; }

// SA CTrainNode::GetPosn (0x6F5440).
CVector CTrainNode::GetPosn(void) const
{
	return CVector(GetX(), GetY(), GetZ());
}

// SA CTrainNode::SetDistanceFromStart (0x6F5490): thirds of a unit.
void CTrainNode::SetDistanceFromStart(float dist) { m_nDistanceFromStart = (uint16)(dist * 3.0f); }

// SA CTrainNode::GetDistanceFromStart (0x6F54B0).
float CTrainNode::GetDistanceFromStart(void) const { return (float)m_nDistanceFromStart / 3.0f; }

// SA CTrainNode::GetLightingFromCollision (0x6F5F80).
//
// Probe straight down from just above the node. If nothing is under it, the
// node gets the hardcoded 0x48 lighting (day 8, night 4) - SA's "outdoors in
// the open" value - rather than an uninitialised byte.
uint8 CTrainNode::GetLightingFromCollision(void)
{
	if(m_bSurfLightingFound)
		return (m_nNightLighting << 4) | m_nDayLighting;

	CColPoint cp;
	CEntity *entity = nil;
	CVector pos = GetPosn();
	pos.z += 1.0f;

	if(!CWorld::ProcessVerticalLine(pos, -1000.0f, cp, entity, true, false, false, false, true, false, nil))
		return 0x48;

	// reVC's CColPoint carries no lighting byte: the collision data it loads
	// does not include the SA per-surface day/night pair. The node is marked as
	// sampled so the probe is not repeated every frame, but the value stays at
	// SA's open-air default, which is what an unlit tunnel would get anyway.
	m_nDayLighting = 8;
	m_nNightLighting = 4;
	m_bSurfLightingFound = true;
	return 0x48;
}
