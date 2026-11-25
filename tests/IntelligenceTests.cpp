// Standalone tests for the SA ped-intelligence support layer: decision-maker
// tables and the event-handler history. These exercise the real game sources
// with a minimal local event; dead-code elimination keeps the rest of the
// game out of the link.
#include "common.h"

#include <cstdio>
#include <cstring>

#include "Event.h"
#include "TaskSimpleNone.h"
#include "EventHandler.h"
#include "EventHandlerHistory.h"
#include "DecisionMakerTypes.h"

// Link stubs for the game services the sources under test reference but the
// test does not run: assertions, the memory manager and the game clock.
#include <cstdlib>
#include "Timer.h"

uint32 CTimer::m_snTimeInMilliseconds;

void
re3_assert(const char *expr, const char*, unsigned int, const char*)
{
	printf("Assertion failed: %s\n", expr);
	abort();
}

void*
MemoryMgrMalloc(size_t size)
{
	return malloc(size);
}

void
MemoryMgrFree(void *ptr)
{
	free(ptr);
}

int
myrand(void)
{
	return rand();
}

static int s_failures;

#define CHECK(cond) do { if(!(cond)){ printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); s_failures++; } } while(0)

class CTestEvent : public CEvent
{
	eEventType m_type;
public:
	explicit CTestEvent(eEventType type) : m_type(type) {}
	eEventType GetEventType() const { return m_type; }
	int32 GetEventPriority() const { return 50; }
	int32 GetLifeTime() { return 1000; }
	CEvent *Clone() const { return new CTestEvent(m_type); }
};

int
main()
{
	// Decision-maker event index mapping.
	CDecisionMakerTypes *dm = CDecisionMakerTypes::GetInstance();
	dm->LoadEventIndices();
	CHECK(dm->m_EventIndices[EVENT_DAMAGE] == DM_EVENT_DAMAGE);
	CHECK(dm->m_EventIndices[EVENT_GUN_AIMED_AT] == DM_EVENT_GUN_AIMED_AT);
	CHECK(dm->m_EventIndices[EVENT_LEAN_ON_VEHICLE] == DM_EVENT_UNDEFINED);

	// A 100%-chance response fires; avoiding that task suppresses it.
	float chances[4] = { 100.0f, 100.0f, 100.0f, 100.0f };
	int32 flags[2] = { 1, 0 }; // on foot only
	dm->AddEventResponse(DECISION_MAKER_PED_RANDOM1, EVENT_DAMAGE, TASK_COMPLEX_FLEE_ENTITY, chances, flags);

	int16 taskType, facialTaskType;
	dm->MakeDecision(nil, EVENT_DAMAGE, DECISION_RELATIONSHIP_THREAT, false,
		TASK_INVALID, TASK_INVALID, TASK_INVALID, TASK_INVALID, false, taskType, facialTaskType);
	CHECK(taskType == TASK_COMPLEX_FLEE_ENTITY);

	dm->MakeDecision(nil, EVENT_DAMAGE, DECISION_RELATIONSHIP_THREAT, false,
		TASK_COMPLEX_FLEE_ENTITY, TASK_INVALID, TASK_INVALID, TASK_INVALID, false, taskType, facialTaskType);
	CHECK(taskType == TASK_INVALID);

	// The same response must not fire in a vehicle (flags say on-foot only).
	dm->MakeDecision(nil, EVENT_DAMAGE, DECISION_RELATIONSHIP_THREAT, true,
		TASK_INVALID, TASK_INVALID, TASK_INVALID, TASK_INVALID, false, taskType, facialTaskType);
	CHECK(taskType == TASK_INVALID);

	// Flushing the row removes the response again.
	dm->FlushDecisionMakerEventResponse(DECISION_MAKER_PED_RANDOM1, EVENT_DAMAGE);
	dm->MakeDecision(nil, EVENT_DAMAGE, DECISION_RELATIONSHIP_THREAT, false,
		TASK_INVALID, TASK_INVALID, TASK_INVALID, TASK_INVALID, false, taskType, facialTaskType);
	CHECK(taskType == TASK_INVALID);

	// History: a non-temporary event becomes the standing response; a
	// temporary one parks it, and it resumes when the temp event clears.
	{
		CEventHandlerHistory history;
		CTestEvent damage(EVENT_DAMAGE);
		history.RecordCurrentEvent(nil, damage);
		CHECK(history.IsRespondingToEvent(EVENT_DAMAGE));
		CHECK(history.GetCurrentEventPriority() == 50);

		CTestEvent fire(EVENT_FIRE_NEARBY);
		CHECK(CEventHandler::IsTemporaryEvent(fire));
		history.RecordCurrentEvent(nil, fire);
		CHECK(history.GetCurrentEvent() != nil);
		CHECK(history.GetCurrentEvent()->GetEventType() == EVENT_FIRE_NEARBY);
		CHECK(history.IsRespondingToEvent(EVENT_DAMAGE)); // parked, not lost

		history.ClearTempEvent();
		history.TickStoredEvent(nil);
		CHECK(history.GetCurrentEvent() != nil);
		CHECK(history.GetCurrentEvent()->GetEventType() == EVENT_DAMAGE);
	}

	// Task vocabulary: the none task finishes instantly and clones cleanly.
	{
		CTaskSimpleNone *none = new CTaskSimpleNone();
		CHECK(none->GetTaskType() == TASK_NONE);
		CHECK(none->ProcessPed(nil));
		CTask *noneClone = none->Clone();
		CHECK(noneClone->GetTaskType() == TASK_NONE);
		delete noneClone;
		delete none;
	}

	if(s_failures == 0)
		printf("SA ped intelligence tests OK\n");
	return s_failures != 0;
}
