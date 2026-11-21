#ifndef __GTA_ENTITYSCANNER_H__
#define __GTA_ENTITYSCANNER_H__

#include "TickCounter.h"

class CEntity;
class CPed;
class CVehicle;

// Which entity list a scan walks. SA calls this eRepeatSectorList because it
// walks the repeat sectors - the sectors along the wrap-around edge of the map
// plus the main grid - rather than the plain sector grid.
enum eRepeatSectorList
{
	REPEATSECTOR_OBJECTS = 0,
	REPEATSECTOR_PEDS,
	REPEATSECTOR_VEHICLES,
};

// SA CEntityScanner: "what is near this ped", kept as a small distance-sorted
// array rather than queried afresh every time an AI check runs.
//
// reVC asks CWorld::FindObjectsInRange with a hardcoded radius wherever it
// needs this, which means a full sector walk per question and a magic number
// per caller. SA scans once per scan period, keeps the result sorted by
// distance, and every AI check then reads the array. The ports below keep the
// SA bookkeeping and do the actual sector walk through the reVC world query,
// because reVC has no repeat sector lists to iterate directly.
//
// MAX_NUM_ENTITIES is 16: SA Mobile's value, which is what gta-reversed
// carries. The array may contain holes and is sorted closest-first.
class CEntityScanner
{
public:
	static const int32 MAX_NUM_ENTITIES = 16;

	// SA has this as a scan-rate constant of 10; it is unused by the code that
	// reads the scanner, which relies on the tick counter alone.
	static const int32 ms_iScanPeriod = 10;

protected:
	CTickCounter m_timer;

public:
	// Sorted by distance, closest first. Entries past the count are nil.
	CEntity *m_apEntities[MAX_NUM_ENTITIES];

protected:
	CEntity *m_pClosestEntityInRange;

public:
	CEntityScanner(void);
	virtual ~CEntityScanner(void);

	CTickCounter *GetTimer(void) { return &m_timer; }

	// SA keeps this virtual so the ped's intelligence can swap in a specialised
	// scanner (vehicle, ped or object).
	virtual void ScanForEntitiesInRange(eRepeatSectorList sectorList, CPed &ped);

	CEntity *GetClosestEntity(void) const { return m_pClosestEntityInRange; }

	void Clear(void);

	// Number of live entries in m_apEntities.
	int32 GetNumEntities(void) const;
};

#endif // __GTA_ENTITYSCANNER_H__
