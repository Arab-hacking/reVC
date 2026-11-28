#pragma once

#include "common.h"
#include "Vector.h"
#include "eEventType.h"
#include "PedType.h"
#include "TaskManager.h"
#include "EventHandler.h"
#include "EventGroup.h"
#include "TaskTimer.h"
#include "EventScanner.h"
#include "PedStuckChecker.h"
#include "VehicleScanner.h"
#include "PedScanner.h"
#include "CollisionEventScanner.h"
#include "MentalState.h"

class CPed;
class CEntity;
class CPlayerPed;
class CTaskSimpleUseGun;
class CTaskSimpleFight;
class CTaskSimpleHoldEntity;
class CTaskSimpleThrowProjectile;
class CTaskSimpleSwim;
class CTaskSimpleDuck;
class CTaskSimpleClimb;
class CTaskSimpleJetPack;
class CTaskSimpleInAir;
class C2dEffect;
class CVehicle;

// SA CPedIntelligence (0x607290): everything a ped knows and wants. It
// owns the task manager, the event group/handler and the scanners that
// feed them, plus the decision-maker configuration that decides how this
// ped reacts. Allocated from CPedIntelligencePool, one per ped.
class CPedIntelligence
{
public:
	CPed *m_pPed;
	CTaskManager m_TaskMgr;
	CEventHandler m_eventHandler;
	CEventGroup m_eventGroup;
	int32 m_nDecisionMakerType;
	int32 m_nDecisionMakerTypeInGroup;
	float m_fHearingRange;
	float m_fSeeingRange;
	uint32 m_nDmNumPedsToScan;
	float m_fDmRadius;
	float m_FollowNodeThresholdDistance;
	char m_NextEventResponseSequence;
	uint8 m_nEventId;
	uint8 m_nEventPriority;
	char field_D3;
	CVehicleScanner m_vehicleScanner;
	CPedScanner m_pedScanner;
	CMentalState m_mentalState;
	char field_188;
	CEventScanner m_eventScanner;
	CCollisionEventScanner m_collisionScanner;
	CPedStuckChecker m_pedStuckChecker;
	int32 m_AnotherStaticCounter;
	int32 m_StaticCounter;
	CVector m_vecLastPedPosDuringDamageEntity;
	CEntity *m_apInterestingEntities[3];

	static const float STEALTH_KILL_RANGE; // 2.5
	static const float LIGHT_AI_LEVEL_MAX; // 0.3
	static const float flt_8D2384; // 30.0
	static const float flt_8D2388; // 50.0

public:
	static void *operator new(size_t size);
	static void operator delete(void *object);

	CPedIntelligence(CPed *ped);
	~CPedIntelligence();

	void SetPedDecisionMakerType(int32 newType);
	int32 GetPedDecisionMakerType() const { return m_nDecisionMakerType; }
	void SetPedDecisionMakerTypeInGroup(int32 newType);
	void RestorePedDecisionMakerType();
	void SetHearingRange(float range);
	void SetSeeingRange(float range);
	bool IsInHearingRange(const CVector &posn);
	bool IsInSeeingRange(const CVector &posn) const;
	bool FindRespectedFriendInInformRange();
	bool IsRespondingToEvent(eEventType eventType);
	void AddTaskPhysResponse(CTask *task, int32 unUsed = 1);
	void AddTaskEventResponseTemp(CTask *task, int32 unUsed);
	void AddTaskEventResponseNonTemp(CTask *task, int32 unUsed);
	void AddTaskPrimaryMaybeInGroup(CTask *task, bool bAffectsPed);

	CTask *FindTaskByType(eTaskType taskId);
	CTaskSimpleFight *GetTaskFighting();
	CTaskSimpleUseGun *GetTaskUseGun();
	CTaskSimpleThrowProjectile *GetTaskThrow();
	CTaskSimpleHoldEntity *GetTaskHold(bool bIgnoreCheckingForSimplestActiveTask = true);
	CTaskSimpleSwim *GetTaskSwim();
	CTaskSimpleDuck *GetTaskDuck(bool bIgnoreCheckingForSimplestActiveTask = true);
	CTaskSimpleJetPack *GetTaskJetPack();
	CTaskSimpleInAir *GetTaskInAir();
	CTaskSimpleClimb *GetTaskClimb();
	CTaskSimpleDuck *GetTaskSecondaryDuck();
	bool GetUsingParachute();
	void SetTaskDuckSecondary(uint16 nLengthOfDuck);
	void ClearTaskDuckSecondary();
	void ClearTasks(bool bClearPrimaryTasks, bool bClearSecondaryTasks);
	void FlushImmediately(bool bSetPrimaryDefaultTask);
	C2dEffect *GetEffectInUse() const;
	void SetEffectInUse(C2dEffect *effect);
	bool ProcessAfterProcCol();
	void ProcessAfterPreRender();
	void ProcessEventHandler();
	bool IsFriendlyWith(const CPed &ped) const;
	bool IsThreatenedBy(const CPed &ped) const;
	bool Respects(CPed *ped) const;
	bool IsInACarOrEnteringOne();
	static bool AreFriends(const CPed &ped1, const CPed &ped2);
	bool IsPedGoingSomewhereOnFoot();
	eMoveState GetMoveStateFromGoToTask();
	void FlushIntelligence();
	bool TestForStealthKill(CPed *pTarget, bool bFullTest);
	void RecordEventForScript(int32 eventId, int32 eventPriority);
	bool HasInterestingEntites();
	bool IsInterestingEntity(CEntity *entity);
	void LookAtInterestingEntities();
	void RemoveAllInterestingEntities();
	bool IsPedGoingForCarDoor();
	float CanSeeEntityWithLights(CEntity *entity, int32 unUsed);
	void ProcessStaticCounter();
	void ProcessFirst();
	void Process();
	CTask *GetActivePrimaryTask() const;
	float GetPedFOVRange() const;
	void IncrementAngerAtPlayer(uint8 anger);

	void SetDmRadius(float r) { m_fDmRadius = r; }
	void SetNumPedsToScan(uint32 n) { m_nDmNumPedsToScan = n; }

	bool IsUsingGun();

	// The vehicle the ped is entering now, if any
	CVehicle *GetEnteringVehicle();

	CTaskManager &GetTaskManager() { return m_TaskMgr; }
	CEventHandler &GetEventHandler() { return m_eventHandler; }
	CEventGroup &GetEventGroup() { return m_eventGroup; }
	CEventScanner &GetEventScanner() { return m_eventScanner; }
	CPedScanner &GetPedScanner() { return m_pedScanner; }
	CVehicleScanner &GetVehicleScanner() { return m_vehicleScanner; }
	CEntity **GetPedEntities() { return m_pedScanner.m_apEntities; }
	CEntity *GetPedEntity(uint32 index) { return GetPedEntities()[index]; }
	CEntity **GetVehicleEntities() { return m_vehicleScanner.m_apEntities; }
	CPedStuckChecker &GetStuckChecker() { return m_pedStuckChecker; }
};

VALIDATE_SIZE(CPedIntelligence, 0x294);
