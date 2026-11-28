#pragma once

// SA CTaskComplexSmartFleeEntity: flee an entity - like the plain flee
// entity task, but the inner flee is the smart (path-network) one and
// the flee speed is carried into it every tick.

#include "PedType.h"
#include "TaskComplex.h"
#include "TaskTimer.h"
#include "Vector.h"

class CEntity;

class CTaskComplexSmartFleeEntity : public CTaskComplex
{
public:
	CEntity *m_fleeFrom;
	CVector m_pos;
	int32 m_time;
	float m_safeDistance;
	bool m_pedScream;
	int32 m_posCheckPeriod;
	float m_posChangeTolerance;
	eMoveState m_moveState;
	CTaskTimer m_posCheckTimer;

public:
	CTaskComplexSmartFleeEntity(CEntity *fleeEntity, bool scream, float safeDistance, int32 fleeTime = 1000000,
		int32 posCheckPeriod = 1000, float posChangeTolerance = 1.0f, eMoveState moveState = PEDMOVE_SPRINT);
	CTaskComplexSmartFleeEntity(const CTaskComplexSmartFleeEntity &other);
	~CTaskComplexSmartFleeEntity();

	CTask *Clone() const { return new CTaskComplexSmartFleeEntity(*this); }
	eTaskType GetTaskType() const { return TASK_COMPLEX_SMART_FLEE_ENTITY; }
	CTask *CreateNextSubTask(CPed *ped);
	CTask *CreateFirstSubTask(CPed *ped);
	CTask *ControlSubTask(CPed *ped);

	CTask *CreateSubTask(eTaskType taskType);
};

VALIDATE_SIZE(CTaskComplexSmartFleeEntity, 0x40);
