#ifndef __GTA_TRAINNODE_H__
#define __GTA_TRAINNODE_H__

// SA CTrainNode. One sampled point of a rail line.
//
// The coordinates are fixed point in eighths of a unit, not floats: a SA track
// file has thousands of nodes across four tracks, and SA stores each one in ten
// bytes. The accessors below are the only way in and out, so the quantisation
// is invisible to the callers.
//
// The surface lighting byte is baked on first use from a vertical collision
// probe: a train passing through a tunnel has to be lit by the tunnel, not by
// the sun, and the track nodes are where that is sampled.
class CTrainNode
{
public:
	int16 x;						// position in eighths of a unit
	int16 y;
	int16 z;
	uint16 m_nDistanceFromStart;	// in thirds of a unit
	uint8 m_nDayLighting : 4;		// tColLighting.day
	uint8 m_nNightLighting : 4;		// tColLighting.night
	bool m_bSurfLightingFound;

public:
	CTrainNode(void);

	void SetX(float X);
	void SetY(float Y);
	void SetZ(float Z);
	float GetX(void) const;
	float GetY(void) const;
	float GetZ(void) const;
	CVector GetPosn(void) const;
	void SetDistanceFromStart(float dist);
	float GetDistanceFromStart(void) const;
	// Sample the surface lighting once, from the collision under the node.
	uint8 GetLightingFromCollision(void);
};

#endif // __GTA_TRAINNODE_H__
