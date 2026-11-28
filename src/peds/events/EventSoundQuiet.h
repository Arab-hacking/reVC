#pragma once

#include "EventEditableResponse.h"
#include "Vector.h"

// SA CEventSoundQuiet: a faint sound at a position (a stealth kill, a
// silenced shot). Peds in hearing range may investigate it.
class CEventSoundQuiet : public CEventEditableResponse
{
public:
	CEntity *m_entity;
	float m_fLocalSoundLevel;
	uint32 m_startTimeInMs;
	CVector m_position;

public:
	CEventSoundQuiet(CEntity *entity, float fLocalSoundLevel, uint32 startTime, const CVector &position);
	~CEventSoundQuiet() {}

	eEventType GetEventType() const { return EVENT_SOUND_QUIET; }
	int32 GetEventPriority() const { return 8; }
	int32 GetLifeTime() { return 0; }
	bool AffectsPed(CPed *ped);
	CEntity *GetSourceEntity() const { return m_entity; }
	float GetLocalSoundLevel() { return m_fLocalSoundLevel; }
	bool CanBeInterruptedBySameEvent() { return true; }
	CEventEditableResponse *CloneEditable() const;
};

VALIDATE_SIZE(CEventSoundQuiet, 0x28);
