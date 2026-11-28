#include "common.h"

#include "CollisionEventScanner.h"
#include "EventGroup.h"
#include "Ped.h"

// SA CCollisionEventScanner::ScanForCollisionEvents (0x604500). Not yet
// reversed upstream; reconstructed from the class's one field and its
// caller in CPedIntelligence::ProcessFirst: the scanner's job is to make
// sure a ped knocked down by a car reports the hit once, not every frame
// while it lies in the road. The hit-by-car event itself lands with the
// collision event classes; until then the scanner tracks the state the
// rest of the intelligence reads.
void
CCollisionEventScanner::ScanForCollisionEvents(CPed *victim, CEventGroup *eventGroup)
{
	(void)eventGroup;
	if(victim == nil)
		return;

	// A ped back on its feet can be hit again.
	if(!victim->Dead())
		m_bAlreadyHitByCar = false;
}
