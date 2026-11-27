#ifndef __GTA_EVENTDRAGGEDOUTCAR_H__
#define __GTA_EVENTDRAGGEDOUTCAR_H__

#include "EventEditableResponse.h"

class CPed;
class CVehicle;

// Told to a ped whose vehicle someone else is entering by force: the
// carjacker is dragging him (or the ped in the seat next to him) out.
class CEventDraggedOutCar : public CEventEditableResponse
{
public:
	CPed *m_CarJacker;
	CVehicle *m_Vehicle;
	bool m_IsDriverSeat;

public:
	CEventDraggedOutCar(CVehicle *vehicle, CPed *carjacker, bool isDriverSeat);
	~CEventDraggedOutCar();

	eEventType GetEventType() const { return EVENT_DRAGGED_OUT_CAR; }
	int32 GetEventPriority() const { return 40; }
	int32 GetLifeTime() { return 0; }
	bool AffectsPed(CPed *ped);
	bool AffectsPedGroup(CPedGroup *pedGroup);
	CEvent *Clone() const { return (CEvent *)CloneEditable(); }
	CEventEditableResponse *CloneEditable() const;
	CEntity *GetSourceEntity() const { return (CEntity *)m_CarJacker; }
	float GetLocalSoundLevel() { return 100.0f; }

	CPed *GetCarJacker() const { return m_CarJacker; }
	CVehicle *GetVehicle() const { return m_Vehicle; }
	bool IsDriverSeat() const { return m_IsDriverSeat; }
};

VALIDATE_SIZE(CEventDraggedOutCar, 0x20);

#endif // __GTA_EVENTDRAGGEDOUTCAR_H__
