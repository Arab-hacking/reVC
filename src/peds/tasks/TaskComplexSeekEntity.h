#pragma once

// SA CTaskComplexSeekEntity (standard seek-pos calculator): walk the
// node route to an entity, updating the goal as it moves, until within
// reach - then optionally face it. Upstream this is a class template
// over the seek-pos calculator; only the standard calculator exists
// here, so the calculator is a plain member instead.

#include "EntitySeekPosCalculator.h"
#include "PedType.h"
#include "TaskComplex.h"
#include "TaskTimer.h"
#include "Vector.h"

class CEntity;

class CTaskComplexSeekEntity : public CTaskComplex
{
public:
	CEntity *m_entityToSeek;
	int32 m_seekInterval;
	int32 m_scanInterval;
	float m_maxEntityDist2D;
	float m_moveStateRadius;
	float m_minEntityDist2D;
	float m_followNodeThresholdHeightChange;
	CTaskTimer m_scanTimer;
	CTaskTimer m_seekTimer;
	CEntitySeekPosCalculatorStandard m_entitySeekPosCalculator;

	eMoveState m_moveState;
	union {
		uint8 m_SeekFlags;
		struct {
			uint8 m_bPlayTiredAnim : 1;
			uint8 m_bFaceEntityWhenDone : 1;
			uint8 m_bAchievedSeekEntity : 1;
			uint8 m_bTrackingEntity : 1;
		};
	};

public:
	CTaskComplexSeekEntity(CEntity *entity, int32 seekInterval, int32 scanInterval, float maxEntityDist2D,
		float moveStateRadius, float followNodeThresholdHeightChange, bool bPlayTiredAnim, bool bFaceEntityWhenDone);
	CTaskComplexSeekEntity(const CTaskComplexSeekEntity &other);
	~CTaskComplexSeekEntity();

	CTask *Clone() const { return new CTaskComplexSeekEntity(*this); }
	eTaskType GetTaskType() const { return TASK_COMPLEX_SEEK_ENTITY; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_URGENT, const CEvent *event = nil);
	CTask *CreateNextSubTask(CPed *ped);
	CTask *CreateFirstSubTask(CPed *ped);
	CTask *ControlSubTask(CPed *ped);

	CTask *CreateSubTask(eTaskType taskType, CPed *ped);
	CVector GetSeekPos(CPed *ped);
	void SetMoveState(eMoveState moveState) { m_moveState = moveState; }
	void SetIsTrackingEntity(bool value) { m_bTrackingEntity = value; }
	void SetEntityMinDist2D(float value) { m_minEntityDist2D = value; }
	CEntity *GetEntityToSeek() const { return m_entityToSeek; }
	float GetMoveStateRadius() const { return m_moveStateRadius; }

private:
	CTask *CreateSubTaskWhenPedIsTooFarFromEntity(CPed *ped, float pedToSeekPosDist2DSq);
};

VALIDATE_SIZE(CTaskComplexSeekEntity, 0x4C);
