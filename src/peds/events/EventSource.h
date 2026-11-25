#ifndef __GTA_EVENTSOURCE_H__
#define __GTA_EVENTSOURCE_H__

class CEvent;
class CPed;

// SA CEventSource: classifies where an event came from, in the terms the
// decision-maker tables use (neutral, player, friend, threat - see
// eDecisionRelationship).
class CEventSource
{
public:
	static int32 ComputeEventSourceType(const CEvent &event, const CPed &ped);
};

#endif // __GTA_EVENTSOURCE_H__
