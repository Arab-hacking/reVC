#pragma once

// SA CEventGunShot: a shot was fired - who fired it, from where, and
// towards where. Posted to the global event group by the firing code
// and handed out to nearby peds from there (CEventGlobalGroup), which
// answer through CEventHandler::ComputeShotFiredResponse.
//
// Adaptations for this codebase: VC proof/created-by stand-ins in
// AffectsPed (MISSION_CHAR for SA's mission-created check), and the
// cop-pursuit clause is approximated - SA also asks the wanted system
// whether this cop is in (or could join) the pursuit, which the local
// CWanted cannot answer yet.

#include "EventEditableResponse.h"
#include "math/Vector.h"

class CEventGunShot : public CEventEditableResponse
{
public:
	CEntity *m_firedBy;
	CVector m_startPoint;
	CVector m_endPoint;
	bool m_bHasNoSound;

	static float ms_fGunShotSenseRangeForRiot2;

public:
	CEventGunShot(CEntity *entity, CVector startPoint, CVector endPoint, bool bHasNoSound);
	~CEventGunShot();

	eEventType GetEventType() const override { return EVENT_SHOT_FIRED; }
	int32 GetEventPriority() const override { return 35; }
	int32 GetLifeTime() override { return 0; }
	bool AffectsPed(CPed *ped) override;
	bool IsCriminalEvent() override;
	void ReportCriminalEvent(CPed *ped) override {} // empty in SA
	CEntity *GetSourceEntity() const override { return m_firedBy; }
	bool TakesPriorityOver(const CEvent &refEvent) override;
	float GetLocalSoundLevel() override { return m_bHasNoSound ? 0.0f : 160.0f; }
	bool CanBeInterruptedBySameEvent() override { return true; }
	CEventEditableResponse *CloneEditable() const override;
};

VALIDATE_SIZE(CEventGunShot, 0x34);
