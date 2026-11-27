#ifndef __GTA_TASKSIMPLEBIKEJACKED_H__
#define __GTA_TASKSIMPLEBIKEJACKED_H__

#include "TaskSimple.h"
#include "AnimationEnums.h"

class CAnimBlendAssociation;
class CTaskUtilityLineUpPedWithCar;
class CVehicle;

// Thrown off the bike by its thief: takes the hit, falls off, and
// tells the event system he has been knocked off so he can get up
// properly afterwards.
class CTaskSimpleBikeJacked : public CTaskSimple
{
public:
	bool m_animWasPlayed;
	CAnimBlendAssociation *m_firstAnim;
	AnimationId m_secondAnimId;
	CVehicle *m_vehicle;
	uint32 m_door;
	uint32 m_time;
	CPed *m_jacker;
	bool m_isVictimDriver;
	CTaskUtilityLineUpPedWithCar *m_taskUtilityLineUpPedWithCar;

public:
	static const eTaskType Type = TASK_SIMPLE_BIKE_JACKED;

	CTaskSimpleBikeJacked(CVehicle *vehicle, uint32 door, uint32 time, CPed *carJacker, bool isVictimDriver);
	CTaskSimpleBikeJacked(const CTaskSimpleBikeJacked &other);
	~CTaskSimpleBikeJacked();

	static void FinishAnimBikeHitCB(CAnimBlendAssociation *anim, void *data);

	CTask *Clone() const { return new CTaskSimpleBikeJacked(*this); }
	eTaskType GetTaskType() const { return Type; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil);
	bool ProcessPed(CPed *ped);
	bool SetPedPosition(CPed *ped);
};

VALIDATE_SIZE(CTaskSimpleBikeJacked, 0x30);

#endif // __GTA_TASKSIMPLEBIKEJACKED_H__
