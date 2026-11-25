#ifndef __GTA_TASKSIMPLEGOTO_H__
#define __GTA_TASKSIMPLEGOTO_H__

#include "TaskSimple.h"
#include "PedType.h"
#include "Vector.h"

// SA CTaskSimpleGoTo (0x20): shared state for the point-to-point movement
// tasks. It remembers where the ped is going, how close is close enough,
// and whether the ped has walked around all four sides of the target - the
// SA test for "we are there, or have overshot and come back".
class CTaskSimpleGoTo : public CTaskSimple
{
public:
	eMoveState m_moveState;
	CVector m_vecTargetPoint;
	float m_fRadius;
	union {
		struct {
			uint8 m_targetCircledFlags : 4;
			uint8 m_bIsIKChainSet : 1;
			uint8 m_bTargetPointUpdated : 1;
		} gotoFlags;
		uint8 m_GoToFlags;
	};

	static float ms_fLookAtThresholdDotProduct;

public:
	CTaskSimpleGoTo(eMoveState moveState, const CVector &targetPoint, float fRadius);
	~CTaskSimpleGoTo() {}

	bool HasCircledTarget(CPed *ped);
	void SetUpIK(CPed *ped);
	void QuitIK(CPed *ped);

	CVector GetTargetPt() const { return m_vecTargetPoint; }
	void SetTargetPt(CVector pt) { m_vecTargetPoint = pt; }

	float GetTargetPtRadius() const { return m_fRadius; }
	void SetTargetPtRadius(float r) { m_fRadius = r; }
};

VALIDATE_SIZE(CTaskSimpleGoTo, 0x20);

#endif // __GTA_TASKSIMPLEGOTO_H__
