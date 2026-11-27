#ifndef __GTA_EVENTKNOCKOFFBIKE_H__
#define __GTA_EVENTKNOCKOFFBIKE_H__

#include "Event.h"

enum eKnockOffType : uint8 {
	KNOCK_OFF_TYPE_NONE = 0,
	KNOCK_OFF_TYPE_SKIDBACKFRONT = 49,
	KNOCK_OFF_TYPE_EXPLOSION = 51,
	KNOCK_OFF_TYPE_FALL = 53,
	KNOCK_OFF_TYPE_SKIDBACK_FALLR = 54,
	KNOCK_OFF_TYPE_NONE_1 = 55, // same as KNOCK_OFF_TYPE_NONE
};

class CVehicle;

// The ped has been knocked off his bike: by a collision, an
// explosion, or the man who is taking it from him.
class CEventKnockOffBike : public CEvent
{
public:
	CVector m_moveSpeed;
	CVector m_collisionImpactVelocity;
	float m_damageIntensity;
	float m_field_28;
	uint32 m_time;
	CPed *m_ped;
	uint8 m_knockOffDirection;
	uint8 m_isVictimDriver : 1;
	uint8 m_forceKnockOff : 1; // ignores the ped's cannot-be-knocked-off flag
	uint8 m_knockOffType;
	uint8 m_exitDoor;
	CVehicle *m_vehicle;

public:
	CEventKnockOffBike(CVehicle *vehicle, const CVector &moveSpeed, const CVector &collisionImpactVelocity,
		float damageIntensity, float a6, uint8 knockOffType, uint8 knockOutDirection, int32 time,
		CPed *ped, bool isVictimDriver, bool forceKnockOff);
	CEventKnockOffBike();
	~CEventKnockOffBike();

	eEventType GetEventType() const { return EVENT_KNOCK_OFF_BIKE; }
	float GetLocalSoundLevel() { return 60.0f; }
	int32 GetEventPriority() const { return 70; }
	int32 GetLifeTime() { return 0; }
	CEvent *Clone() const;
	bool AffectsPed(CPed *ped);
	bool IsCriminalEvent() { return true; }
	void ReportCriminalEvent(CPed *ped);
};

VALIDATE_SIZE(CEventKnockOffBike, 0x3C);

#endif // __GTA_EVENTKNOCKOFFBIKE_H__
