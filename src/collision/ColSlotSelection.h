#ifndef __GTA_COLSLOTSELECTION_H__
#define __GTA_COLSLOTSELECTION_H__

// Which collision store slots a frame wants loaded.
//
// Extracted from CColStore::LoadCollision so the policy is testable without a
// world. The rules, in the order they are applied:
//
//   1. the position itself,
//   2. an optional second position (a remote player's vehicle) with a -119
//      margin, so the slot is wanted slightly before it is reached,
//   3. the literal slot name "yacht", which is always kept,
//   4. mission entities - a vehicle or ped the script asked to protect - within
//      -80 of the box.
//
// The margins are negative on purpose: CRect::IsPointInside takes an extra
// radius and shrinks the box, so a slot only counts as wanted once the point is
// that far INSIDE it. That is the anti-thrash hysteresis and it must not be
// "fixed" into an expansion.

// How far inside its box a point has to be for the slot to be wanted.
enum {
	COL_SECOND_POSITION_MARGIN = -119,
	COL_MISSION_ENTITY_MARGIN = -80,
	COL_NEARBY_MARGIN = -110,
	COL_REQUEST_MARGIN = -115,
	COL_HAS_LOADED_MARGIN = -115
};

#include <string.h>

class CRect;
class CVector2D;

inline bool
ColSlotWantedFor(CRect &bounds, const char *name, const CVector2D &pos,
                 bool hasSecondPosition, const CVector2D &secondPosition,
                 const CVector2D *missionEntities, int numMissionEntities);
inline bool ColSlotNearby(CRect &bounds, const CVector2D &pos);
inline bool ColSlotWithinRequestRadius(CRect &bounds, const CVector2D &pos);
inline bool ColSlotWithinLoadedRadius(CRect &bounds, const CVector2D &pos);

#ifdef COLSLOTSELECTION_IMPLEMENTATION
inline bool
ColSlotWantedFor(CRect &bounds, const char *name, const CVector2D &pos,
                 bool hasSecondPosition, const CVector2D &secondPosition,
                 const CVector2D *missionEntities, int numMissionEntities)
{
	if(bounds.IsPointInside(pos))
		return true;
	if(hasSecondPosition && bounds.IsPointInside(secondPosition, COL_SECOND_POSITION_MARGIN))
		return true;
	if(name && strcmp(name, "yacht") == 0)
		return true;
	for(int j = 0; j < numMissionEntities; j++)
		if(bounds.IsPointInside(missionEntities[j], COL_MISSION_ENTITY_MARGIN))
			return true;
	return false;
}

inline bool ColSlotNearby(CRect &bounds, const CVector2D &pos)
{
	return bounds.IsPointInside(pos, COL_NEARBY_MARGIN);
}

inline bool ColSlotWithinRequestRadius(CRect &bounds, const CVector2D &pos)
{
	return bounds.IsPointInside(pos, COL_REQUEST_MARGIN);
}

inline bool ColSlotWithinLoadedRadius(CRect &bounds, const CVector2D &pos)
{
	return bounds.IsPointInside(pos, COL_HAS_LOADED_MARGIN);
}
#endif

#endif // __GTA_COLSLOTSELECTION_H__
