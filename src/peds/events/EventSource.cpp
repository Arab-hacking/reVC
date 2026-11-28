#include "common.h"

#include "EventSource.h"
#include "Event.h"
#include "Ped.h"
#include "Decision.h"

// SA CEventSource::ComputeEventSourceType (0x4B0A00). Not yet reversed
// upstream; reconstructed from how the result is consumed: it indexes the
// probability column of a decision row, so events caused by the player use
// the player column and everything without a living source reads neutral.
// The friend/threat refinement needs SA's acquaintance system and lands
// with it.
int32
CEventSource::ComputeEventSourceType(const CEvent &event, const CPed &ped)
{
	(void)ped;
	CEntity *source = event.GetSourceEntity();
	if(source == nil)
		return DECISION_RELATIONSHIP_NEUTRAL;
	if(source->IsPed() && ((CPed*)source)->IsPlayer())
		return DECISION_RELATIONSHIP_PLAYER;
	return DECISION_RELATIONSHIP_NEUTRAL;
}
