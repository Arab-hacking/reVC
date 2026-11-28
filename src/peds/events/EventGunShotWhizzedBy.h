#pragma once

// SA CEventGunShotWhizzedBy: a shot passed close by the ped's head -
// same payload as CEventGunShot, different (closer, positional)
// AffectsPed and a duck-oriented response
// (CEventHandler::ComputeShotFiredWhizzedByResponse).

#include "EventGunShot.h"

class CEventGunShotWhizzedBy : public CEventGunShot
{
public:
	CEventGunShotWhizzedBy(CEntity *entity, const CVector &startPoint, const CVector &endPoint, bool bHasNoSound);
	~CEventGunShotWhizzedBy() {}

	eEventType GetEventType() const override { return EVENT_SHOT_FIRED_WHIZZED_BY; }
	int32 GetEventPriority() const override { return 36; }
	int32 GetLifeTime() override { return 0; }
	bool AffectsPed(CPed *ped) override;
	bool CanBeInterruptedBySameEvent() override { return true; }
	CEventEditableResponse *CloneEditable() const override { return new CEventGunShotWhizzedBy(m_firedBy, m_startPoint, m_endPoint, m_bHasNoSound); }
};

VALIDATE_SIZE(CEventGunShotWhizzedBy, 0x34);
