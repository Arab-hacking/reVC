#ifndef TASKSIMPLESWIM_H
#define TASKSIMPLESWIM_H

#include "AnimManager.h"

class CPed;
class CPlayerPed;
class CAnimBlendAssociation;

// SA's swim states, in the order CTaskSimpleSwim uses them. The task switches
// between them from the pad, and the clip that plays follows from the state -
// which is why they are a separate enum and not ped states: a ped can be shot
// at, drown and die while swimming without ever leaving the water.
enum eSwimState {
	SWIM_TREAD = 0,
	SWIM_SPRINT,
	SWIM_SPRINTING,
	SWIM_DIVE_UNDERWATER,
	SWIM_UNDERWATER_SPRINTING,
	SWIM_BACK_TO_SURFACE
};

// A port of SA's CTaskSimpleSwim. reVC has no task manager, so a ped that swims
// owns one of these and steps it from ProcessControl; the ped's own
// m_nSwimmingMoveState mirrors the animation the task has chosen so that the
// rest of the ped code can ask "is this ped swimming" without reaching into the
// task.
class CTaskSimpleSwim
{
public:
	CTaskSimpleSwim();
	~CTaskSimpleSwim();

	bool ProcessPed(CPed *ped);

	// SA's three halves of ProcessPed, kept apart because the AI path calls
	// only the last one.
	void ProcessControlInput(CPed *ped, CPlayerPed *player);
	void ProcessSwimmingResistance(CPed *ped);
	void ProcessEffects(CPed *ped);
	void ProcessControlAI(CPed *ped);

	void StartAnim(CPed *ped, AssocGroupId animGroup, AnimationId animId);
	void StopTask(CPed *ped);

	eSwimState GetSwimState(void) const { return m_nSwimState; }
	// An AI swimmer needs somewhere to swim to; until a target is set it only
	// treads water, which is what SA does for a ped whose seek position was
	// never filled in.
	void SetSwimTarget(float x, float y) { m_vecSwimTarget = CVector2D(x, y); m_bHasTarget = true; }

private:
	CVector2D m_vecSwimTarget;
	float m_fRotationX;
	float m_fSwimProgress;
	float m_fAnimSpeed;
	eSwimState m_nSwimState;
	bool m_bDiving;
	bool m_bHasTarget;
	CAnimBlendAssociation *m_pSwimAnim;
};

#endif // TASKSIMPLESWIM_H
