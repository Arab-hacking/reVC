#pragma once

// SA CTaskComplexTurnToFaceEntityOrCoord: turn to face a (possibly
// moving) entity or a fixed point - a CTaskSimpleAchieveHeading whose
// target heading is recomputed every tick for the entity form.

#include "TaskComplex.h"
#include "Vector.h"

class CEntity;

class CTaskComplexTurnToFaceEntityOrCoord : public CTaskComplex
{
public:
	CEntity *m_EntityToFace;
	bool m_bFaceEntity;
	CVector m_CoordsToFace;
	float m_fChangeRateMult;
	float m_fMaxHeading;

public:
	CTaskComplexTurnToFaceEntityOrCoord(CEntity *entity, float changeRateMult = 0.5f, float maxHeading = 0.2f);
	CTaskComplexTurnToFaceEntityOrCoord(const CVector &coords, float changeRateMult = 0.5f, float maxHeading = 0.2f);
	~CTaskComplexTurnToFaceEntityOrCoord();

	CTask *Clone() const;
	eTaskType GetTaskType() const { return TASK_COMPLEX_TURN_TO_FACE_ENTITY; }
	CTask *CreateNextSubTask(CPed *ped) { (void)ped; return nil; }
	CTask *CreateFirstSubTask(CPed *ped);
	CTask *ControlSubTask(CPed *ped);

	float ComputeTargetHeading(CPed *ped) const;
	CVector ComputeTargetPos() const;
};

VALIDATE_SIZE(CTaskComplexTurnToFaceEntityOrCoord, 0x28);
