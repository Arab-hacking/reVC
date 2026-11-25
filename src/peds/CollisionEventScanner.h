#pragma once

#include "common.h"

class CPed;
class CEventGroup;

// SA CCollisionEventScanner: turns the collisions a ped suffered this
// frame (cars, other peds, objects) into events on its event group.
class CCollisionEventScanner
{
public:
	bool m_bAlreadyHitByCar;

	CCollisionEventScanner() : m_bAlreadyHitByCar(false) {}

	void ScanForCollisionEvents(CPed *victim, CEventGroup *eventGroup);
};

VALIDATE_SIZE(CCollisionEventScanner, 0x1);
