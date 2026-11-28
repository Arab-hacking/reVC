#ifndef __GTA_TASKSIMPLEPICKUPBIKE_H__
#define __GTA_TASKSIMPLEPICKUPBIKE_H__

#include "TaskSimple.h"

class CAnimBlendAssociation;
class CTaskUtilityLineUpPedWithCar;
class CVehicle;

// Pick a fallen bike back up onto its wheels before mounting it: the
// ped heaves it upright with the align animation for the side it fell
// on, then the entry continues with the get-in.
class CTaskSimplePickUpBike : public CTaskSimple
{
public:
	int32 m_targetDoor;
	CVehicle *m_veh;
	CTaskUtilityLineUpPedWithCar *m_lineUpUtil;
	CAnimBlendAssociation *m_anim;
	bool m_hasAnimFinished;

public:
	static const eTaskType Type = TASK_SIMPLE_BIKE_PICK_UP;

	CTaskSimplePickUpBike(CVehicle *veh, int32 targetDoor, CTaskUtilityLineUpPedWithCar *lineUpUtil);
	CTaskSimplePickUpBike(const CTaskSimplePickUpBike &other);
	~CTaskSimplePickUpBike();

	static void FinishAnimPickUpBikeCB(CAnimBlendAssociation *anim, void *data);

	void StartAnim(CPed *ped);

	CTask *Clone() const { return new CTaskSimplePickUpBike(*this); }
	eTaskType GetTaskType() const { return Type; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil);
	bool ProcessPed(CPed *ped);
	bool SetPedPosition(CPed *ped);
};

VALIDATE_SIZE(CTaskSimplePickUpBike, 0x1C);

#endif // __GTA_TASKSIMPLEPICKUPBIKE_H__
