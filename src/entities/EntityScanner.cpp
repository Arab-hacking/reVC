#include "common.h"

#include "EntityScanner.h"
#include "Entity.h"
#include "Ped.h"
#include "Vehicle.h"
#include "World.h"
#include "PlayerPed.h"
#include "Timer.h"

// Scanner range. SA reads this from CPedIntelligence::m_fSeeingRange and
// m_fHearingRange; reVC has no intelligence object (its AI is the flat CPed
// code), so the ranges SA ships as defaults are used instead of inventing a
// per-ped value that nothing would maintain. They match SA's civilian values.
static constexpr float SCANNER_SEEING_RANGE = 30.0f;
static constexpr float SCANNER_HEARING_RANGE = 15.0f;

// SA CEntityScanner::CEntityScanner (0x5FF990).
CEntityScanner::CEntityScanner(void)
{
	for(int32 i = 0; i < MAX_NUM_ENTITIES; i++)
		m_apEntities[i] = nil;

	m_pClosestEntityInRange = nil;

	// SA sets the period to MAX_NUM_ENTITIES here, then the owning ped's
	// intelligence overrides it. Keeping the same default means a scanner that
	// is never configured still scans, just rarely.
	m_timer.SetPeriod(MAX_NUM_ENTITIES);
}

// SA CEntityScanner::~CEntityScanner (0x603480): dropping the scanner drops
// every reference it holds, so the entities are free to be deleted.
CEntityScanner::~CEntityScanner(void)
{
	Clear();
}

// SA CEntityScanner::Clear (0x5FF9D0).
void
CEntityScanner::Clear(void)
{
	for(int32 i = 0; i < MAX_NUM_ENTITIES; i++){
		if(m_apEntities[i])
			m_apEntities[i]->CleanUpOldReference(&m_apEntities[i]);
		m_apEntities[i] = nil;
	}

	if(m_pClosestEntityInRange)
		m_pClosestEntityInRange->CleanUpOldReference(&m_pClosestEntityInRange);
	m_pClosestEntityInRange = nil;
}

int32
CEntityScanner::GetNumEntities(void) const
{
	int32 num = 0;
	for(int32 i = 0; i < MAX_NUM_ENTITIES; i++)
		if(m_apEntities[i])
			num++;
	return num;
}

// SA CEntityScanner::ScanForEntitiesInRange (0x5FFA20).
//
// The shape is SA's: tick first, clear, gather everything inside the scan
// radius, keep it sorted closest-first, then take a reference on each and
// point m_pClosestEntityInRange at the nearest. What differs is the sector
// walk: SA iterates the repeat sectors, reVC asks CWorld::FindObjectsInRange,
// which is the same query over the same lists and returns the same entities.
void
CEntityScanner::ScanForEntitiesInRange(eRepeatSectorList sectorList, CPed &ped)
{
	if(!m_timer.Tick())
		return;

	Clear();

	const bool isPlayer = FindPlayerPed() == &ped;
	const float range = SCANNER_SEEING_RANGE > SCANNER_HEARING_RANGE
		? SCANNER_SEEING_RANGE : SCANNER_HEARING_RANGE;
	const CVector &pos = ped.GetPosition();

	// Which world lists to walk depends on what is being scanned.
	bool checkPeds = false, checkVehicles = false, checkObjects = false;
	switch(sectorList){
	case REPEATSECTOR_PEDS:     checkPeds = true; break;
	case REPEATSECTOR_VEHICLES: checkVehicles = true; break;
	case REPEATSECTOR_OBJECTS:  checkObjects = true; break;
	default:
		return;
	}

	int16 numFound = 0;
	CEntity *found[MAX_NUM_ENTITIES];
	for(int32 i = 0; i < MAX_NUM_ENTITIES; i++)
		found[i] = nil;

	CWorld::FindObjectsInRange(pos, range, false, &numFound, MAX_NUM_ENTITIES, found,
		false, checkVehicles, checkPeds, checkObjects, false);

	if(numFound <= 0)
		return;

	// Insert each candidate in distance order. SA does the same with an
	// insertion step that shifts the tail along; a full array drops the
	// furthest entry, which falls out of only ever replacing at the end.
	float distancesSq[MAX_NUM_ENTITIES];
	for(int32 i = 0; i < MAX_NUM_ENTITIES; i++)
		distancesSq[i] = 3.4e38f;	// FLT_MAX
	int32 numEntities = 0;

	for(int16 i = 0; i < numFound; i++){
		CEntity *entity = found[i];
		if(entity == nil || entity == &ped)
			continue;

		// A ped does not scan itself, and a dead ped is not something a living
		// ped reacts to through this path - SA filters the dead out here for
		// the ped list only, and only when the scanning ped is not the player
		// (the player still "sees" corpses, for the death message).
		if(sectorList == REPEATSECTOR_PEDS){
			CPed *other = (CPed*)entity;
			if(!isPlayer && other->Dead())
				continue;
		}

		// No scan-code test here: CWorld::FindObjectsInRange already advanced
		// the scan code and stamped every entity it collected, so each one in
		// `found` is unique to this pass. Touching m_scanCode again would make
		// the next query skip entities the world still considers scanned.

		const float distSq = (entity->GetPosition() - pos).MagnitudeSqr();
		if(distSq >= range*range)
			continue;

		// Find the slot this belongs in, keeping the array sorted closest
		// first. Once the array is full the furthest entry is dropped, which
		// is what SA's shift-right insertion does.
		//
		// The shift has to stop at the end of the *live* range: numEntities
		// counts real entries, but the slots past it are nil, and shifting a
		// nil down would silently lose the tail.
		int32 slot = numEntities;	// default: append
		for(int32 i2 = 0; i2 < numEntities; i2++){
			if(distSq < distancesSq[i2]){
				slot = i2;
				break;
			}
		}
		if(slot >= MAX_NUM_ENTITIES)
			continue;	// full, and further away than everything in it

		if(numEntities < MAX_NUM_ENTITIES){
			const int32 last = numEntities;
			for(int32 j = last; j > slot; j--){
				m_apEntities[j] = m_apEntities[j-1];
				distancesSq[j] = distancesSq[j-1];
			}
			numEntities++;
		}else{
			// Full: shift down from the last slot, dropping whatever was there.
			for(int32 j = MAX_NUM_ENTITIES - 1; j > slot; j--){
				m_apEntities[j] = m_apEntities[j-1];
				distancesSq[j] = distancesSq[j-1];
			}
		}
		m_apEntities[slot] = entity;
		distancesSq[slot] = distSq;
	}

	// Hold a reference on everything kept, so a scan result cannot dangle.
	for(int32 i = 0; i < numEntities; i++){
		if(m_apEntities[i])
			m_apEntities[i]->RegisterReference(&m_apEntities[i]);
	}

	// The nearest one is the array head by construction.
	if(m_apEntities[0]){
		if(m_pClosestEntityInRange)
			m_pClosestEntityInRange->CleanUpOldReference(&m_pClosestEntityInRange);
		m_pClosestEntityInRange = m_apEntities[0];
		m_pClosestEntityInRange->RegisterReference(&m_pClosestEntityInRange);
	}

}
