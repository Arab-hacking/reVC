#pragma once

#include "common.h"
#include "Vector.h"
#include "eEventType.h"

// SA CEvent (0x4ABFC0). An event is something that happened to (or near) a
// ped: a collision, a gunshot, damage, a seen ped. Scanners create events,
// the ped's CEventGroup queues them, and CEventHandler turns the highest
// priority one into a response task through the ped's decision maker.
// Events are allocated from CEventPool, never from the heap.
class CPed;
class CEntity;
class CPedGroup;
class CEventEditableResponse;

class CEvent
{
public:
	int32 m_nTimeActive;
	bool m_bValid;

public:
	static void *operator new(size_t size);
	static void operator delete(void *object);

	CEvent();
	virtual ~CEvent();

	virtual eEventType GetEventType() const = 0;
	virtual int32 GetEventPriority() const = 0;
	virtual int32 GetLifeTime() = 0;
	virtual CEvent *Clone() const = 0;
	virtual bool AffectsPed(CPed *ped) { return true; }
	virtual bool AffectsPedGroup(CPedGroup *pedGroup) { return true; }
	virtual bool IsCriminalEvent() { return false; }
	virtual void ReportCriminalEvent(CPed *ped) {}
	virtual bool HasEditableResponse() const { return false; }
	virtual CEntity *GetSourceEntity() const { return nil; }
	virtual bool TakesPriorityOver(const CEvent &other) { return GetEventPriority() >= other.GetEventPriority(); }
	virtual float GetLocalSoundLevel() { return 0.0f; }
	virtual bool DoInformVehicleOccupants(CPed *ped) { return false; }
	virtual bool IsValid(CPed *ped) { return m_bValid || m_nTimeActive <= GetLifeTime(); }
	virtual bool CanBeInterruptedBySameEvent() { return false; }

	float GetSoundLevel(const CEntity *entity, CVector &position);
	static float CalcSoundLevelIncrement(float level1, float level2);

	void UnTick() { m_nTimeActive--; }
	void Tick() { m_nTimeActive++; }
};

VALIDATE_SIZE(CEvent, 0xC);
