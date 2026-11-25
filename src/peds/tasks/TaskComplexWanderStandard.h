#ifndef __GTA_TASKCOMPLEXWANDERSTANDARD_H__
#define __GTA_TASKCOMPLEXWANDERSTANDARD_H__

#include "TaskComplexWander.h"
#include "TaskTimer.h"

// SA CTaskComplexWanderStandard (0x38): the civilian wander. While
// walking it keeps an eye out for gang members to greet, peds to chat
// with and cars to admire.
class CTaskComplexWanderStandard : public CTaskComplexWander
{
public:
	CTaskComplexWanderStandard(eMoveState moveState, uint8 dir, bool bWanderSensibly = true);

	CTask *Clone() const { return new CTaskComplexWanderStandard(m_nMoveState, m_nDir); }
	eWanderType GetWanderType() { return WANDER_TYPE_STANDARD; }
	void ScanForStuff(CPed *ped);

	bool WillChat(const CPed &first, const CPed &second);
	void SetNextMinScanTime(CPed *ped);
	bool LookForSexyCars(CPed *ped);
	bool LookForChatPartners(CPed *ped);
	bool LookForGangMembers(CPed *ped);

private:
	CTaskTimer m_TaskTimer;
	uint32 m_nMinNextScanTime;
};

VALIDATE_SIZE(CTaskComplexWanderStandard, 0x38);

#endif // __GTA_TASKCOMPLEXWANDERSTANDARD_H__
