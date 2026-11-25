#pragma once

#include "common.h"
#include "TickCounter.h"

class CEntity;
class CPed;

// Which world sector list a scanner walks (SA eRepeatSectorList; the VC
// world keeps the same four lists per sector).
enum eRepeatSectorList
{
	REPEATSECTOR_VEHICLES = 0,
	REPEATSECTOR_PEDS,
	REPEATSECTOR_OBJECTS,
	REPEATSECTOR_BUILDINGS,
	REPEATSECTOR_DUMMIES
};

#define MAX_NUM_SCANNED_ENTITIES 16

// SA CEntityScanner (0x5FF990): base of the ped/vehicle scanners. Every
// Period ticks it collects the closest entities around its ped, sorted by
// distance, holding references so they cannot be deleted mid-frame.
class CEntityScanner
{
	friend class CPedIntelligence;

protected:
	CTickCounter m_timer;

public:
	CEntity *m_apEntities[MAX_NUM_SCANNED_ENTITIES]; // may contain holes, sorted by distance

protected:
	CEntity *m_pClosestEntityInRange;

public:
	CEntityScanner();
	~CEntityScanner();

	CTickCounter *GetTimer() { return &m_timer; }

protected:
	virtual void ScanForEntitiesInRange(eRepeatSectorList sectorList, CPed &ped);
	CEntity *GetClosestEntity() const { return m_pClosestEntityInRange; }

	void Clear();
};

VALIDATE_SIZE(CEntityScanner, 0x50);
