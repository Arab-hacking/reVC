#pragma once

// SA CTaskSimpleDuck: duck/cower, either timed, linked to a controlling
// task or linked to the duck animation itself. Secondary duck instances
// (slot TASK_SECONDARY_DUCK) and embedded ducks fight over the ped:
// exactly one of them owns the duck animations.

#include "TaskSimple.h"
#include "Vector2D.h"

class CAnimBlendAssociation;

enum eDuckControlType
{
	DUCK_STANDALONE = 0,              // duck anim removed when task removed
	DUCK_STANDALONE_WEAPON_CROUCH,    // duck anim removed when task removed
	DUCK_TASK_CONTROLLED,             // duck directly linked to a controlling task
	DUCK_ANIM_CONTROLLED,             // duck linked to duck anim
	DUCK_SCRIPT_CONTROLLED,
};

class CTaskSimpleDuck : public CTaskSimple
{
public:
	uint32 m_StartTime;
	uint16 m_LengthOfDuck;
	int16 m_ShotWhizzingCounter;
	CAnimBlendAssociation *m_DuckAnim;
	CAnimBlendAssociation *m_MoveAnim;
	int8 m_bIsFinished;
	int8 m_bIsAborting;
	bool m_bNeedToSetDuckFlag; // in case ped->bIsDucking gets cleared elsewhere
	int8 m_bIsInControl;       // if duck is controlled by another task it needs continuous control
	CVector2D m_MoveCmd;
	eDuckControlType m_DuckControlType;
	int8 m_CountDownFrames;

public:
	CTaskSimpleDuck(eDuckControlType duckControlType, uint16 lengthOfDuck, int16 shotWhizzingCounter = -1);
	~CTaskSimpleDuck();

	CTask *Clone() const { return new CTaskSimpleDuck(*this); }
	eTaskType GetTaskType() const { return TASK_SIMPLE_DUCK; }
	bool MakeAbortable(CPed *ped, eAbortPriority priority = ABORT_PRIORITY_LEISURE, const CEvent *event = nil);
	bool ProcessPed(CPed *ped);

	static void DeleteDuckAnimCB(CAnimBlendAssociation *assoc, void *task);
	static bool CanPedDuck(CPed *ped);

	bool IsTaskInUseByOtherTasks() const;
	void AbortBecauseOfOtherDuck(CPed *ped);
	void RestartTask(CPed *ped);
	void ControlDuckMove(CVector2D moveSpeed = CVector2D(0.0f, 0.0f));
	void SetMoveAnim(CPed *ped);
	void ForceStopMove();
	bool StopFireGun() const;
	void SetDuckTimer(uint16 time);
	bool SetControlType(eDuckControlType controlType);

private:
	void SetPedIsDucking(CPed *ped, bool value);
};

VALIDATE_SIZE(CTaskSimpleDuck, 0x30);
