#pragma once

// SA CTaskComplexFleeEntity: flee an entity - tracks its position with a
// periodic check and (re)issues CTaskComplexFleePoint at it. 95% wrapper
// around the point version, as SA's own comment admits.

#include "TaskComplex.h"
#include "TaskTimer.h"
#include "Vector.h"

class CEntity;

class CTaskComplexFleeEntity : public CTaskComplex
{
public:
	CEntity *m_entity;
	CVector m_pos;
	int32 m_time;
	CTaskTimer m_posCheckTimer;
	float m_safeDistance;
	bool m_pedScream;
	int32 m_posCheckPeriod;
	float m_posChangeTolerance;

public:
	CTaskComplexFleeEntity(CEntity *fleeEntity, bool scream, float safeDistance = 60.0f, int32 fleeTime = 1000000,
		int32 posCheckPeriod = 1000, float posChangeTolerance = 1.0f);
	CTaskComplexFleeEntity(const CTaskComplexFleeEntity &other);
	~CTaskComplexFleeEntity();

	CTask *Clone() const { return new CTaskComplexFleeEntity(*this); }
	eTaskType GetTaskType() const { return TASK_COMPLEX_FLEE_ENTITY; }
	CTask *CreateNextSubTask(CPed *ped);
	CTask *CreateFirstSubTask(CPed *ped);
	CTask *ControlSubTask(CPed *ped);

	CTask *CreateSubTask(eTaskType taskType);
};

VALIDATE_SIZE(CTaskComplexFleeEntity, 0x34);
