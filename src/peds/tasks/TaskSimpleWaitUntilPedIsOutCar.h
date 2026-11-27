#ifndef __GTA_TASKSIMPLEWAITUNTILPEDISOUTCAR_H__
#define __GTA_TASKSIMPLEWAITUNTILPEDISOUTCAR_H__

#include "TaskSimple.h"

// Stand here (watching the spot) until the other ped is out of his
// car.
class CTaskSimpleWaitUntilPedIsOutCar : public CTaskSimple
{
public:
	CPed *m_PedToWaitFor;
	bool m_bIsNotGoingTowardsPos;
	CVector m_Pos;

public:
	static const eTaskType Type = TASK_SIMPLE_WAIT_UNTIL_PED_OUT_CAR;

	CTaskSimpleWaitUntilPedIsOutCar(CPed *ped, const CVector &pos);
	CTaskSimpleWaitUntilPedIsOutCar(const CTaskSimpleWaitUntilPedIsOutCar &other);
	~CTaskSimpleWaitUntilPedIsOutCar();

	CTask *Clone() const { return new CTaskSimpleWaitUntilPedIsOutCar(*this); }
	eTaskType GetTaskType() const { return Type; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil) { return true; }
	bool ProcessPed(CPed *ped);
};

VALIDATE_SIZE(CTaskSimpleWaitUntilPedIsOutCar, 0x1C);

#endif // __GTA_TASKSIMPLEWAITUNTILPEDISOUTCAR_H__
