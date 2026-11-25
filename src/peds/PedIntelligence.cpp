#include "common.h"

#include "PedIntelligence.h"
#include "Ped.h"
#include "PlayerPed.h"
#include "Vehicle.h"
#include "EventScriptCommand.h"
#include "DecisionMakerTypes.h"
#include "TaskComplexWander.h"
#include "TaskSimpleCarDrive.h"
#include "TaskSimpleCarDriveTimed.h"
#include "TaskSimpleDuck.h"
#include "TaskSimpleGoTo.h"
#include "TaskSimpleStandStill.h"
#include "Timer.h"

// SA statics (0x8D2380..).
const float CPedIntelligence::STEALTH_KILL_RANGE = 2.5f;
const float CPedIntelligence::LIGHT_AI_LEVEL_MAX = 0.3f;
const float CPedIntelligence::flt_8D2384 = 30.0f;
const float CPedIntelligence::flt_8D2388 = 50.0f;

// SA CPedIntelligencePool: 140 intelligences, one per live ped. Plain fixed
// storage here; SA keeps the same count in a CPool.
#define MAX_PED_INTELLIGENCES 140

static uint8 s_PedIntelligencePoolStorage[MAX_PED_INTELLIGENCES][sizeof(CPedIntelligence)];
static bool s_PedIntelligencePoolUsed[MAX_PED_INTELLIGENCES];
static int32 s_PedIntelligencePoolAllocPtr;

// SA CPedIntelligence::operator new (pool allocate).
void*
CPedIntelligence::operator new(size_t size)
{
	(void)size;
	for(int32 n = 0; n < MAX_PED_INTELLIGENCES; n++){
		int32 i = (s_PedIntelligencePoolAllocPtr + n) % MAX_PED_INTELLIGENCES;
		if(!s_PedIntelligencePoolUsed[i]){
			s_PedIntelligencePoolUsed[i] = true;
			s_PedIntelligencePoolAllocPtr = i + 1;
			return s_PedIntelligencePoolStorage[i];
		}
	}
	return nil;
}

// SA CPedIntelligence::operator delete (pool free).
void
CPedIntelligence::operator delete(void *object)
{
	if(object == nil)
		return;
	for(int32 i = 0; i < MAX_PED_INTELLIGENCES; i++){
		if((void*)s_PedIntelligencePoolStorage[i] == object){
			s_PedIntelligencePoolUsed[i] = false;
			return;
		}
	}
}

// SA CPedIntelligence::CPedIntelligence (0x607140).
CPedIntelligence::CPedIntelligence(CPed *ped)
	: m_pPed(ped)
	, m_TaskMgr(ped)
	, m_eventHandler(ped)
	, m_eventGroup(ped)
{
	m_nDecisionMakerType          = DM_EVENT_UNDEFINED;
	m_nDecisionMakerTypeInGroup   = -1;
	m_fHearingRange               = 15.0f;
	m_fSeeingRange                = 15.0f;
	m_nDmNumPedsToScan            = 3;
	m_fDmRadius                   = 15.0f;
	m_FollowNodeThresholdDistance = 30.0f;
	m_NextEventResponseSequence   = -1;
	m_nEventId                    = 0;
	m_nEventPriority              = 0;
	field_188                     = 0;
	m_collisionScanner.m_bAlreadyHitByCar = false;
	m_AnotherStaticCounter        = 0;
	m_StaticCounter               = 0;
	// Gang peds watch a much wider world than civilians.
	if(m_pPed->m_nPedType >= PEDTYPE_GANG1 && m_pPed->m_nPedType <= PEDTYPE_GANG10){
		m_fSeeingRange = 40.0f;
		m_fHearingRange = 40.0f;
	}
	for(int i = 0; i < 3; i++)
		m_apInterestingEntities[i] = nil;
}

// SA CPedIntelligence::~CPedIntelligence (0x607300). (SA also clears the
// ped's entry in the player relationship recorder; that recorder lands
// with the SA player-relationship port.)
CPedIntelligence::~CPedIntelligence()
{
}

// SA CPedIntelligence::SetPedDecisionMakerType (0x600B50): the personal
// table parks into the group slot when it is cleared, so a ped rejoining a
// group thinks with its old table again.
void
CPedIntelligence::SetPedDecisionMakerType(int32 newType)
{
	int32 oldType = m_nDecisionMakerType;
	if(oldType){
		if(!newType)
			m_nDecisionMakerTypeInGroup = oldType;
		m_nDecisionMakerType = newType;
	}else{
		m_nDecisionMakerTypeInGroup = newType;
	}
	if(m_nDecisionMakerType == DM_EVENT_PED_ENTERED_MY_VEHICLE){
		m_fDmRadius = 5.0f;
		m_nDmNumPedsToScan = 15;
	}
}

// SA CPedIntelligence::SetPedDecisionMakerTypeInGroup (0x600BB0).
void
CPedIntelligence::SetPedDecisionMakerTypeInGroup(int32 newType)
{
	m_nDecisionMakerTypeInGroup = newType;
}

// SA CPedIntelligence::RestorePedDecisionMakerType (0x600BC0).
void
CPedIntelligence::RestorePedDecisionMakerType()
{
	if(!m_nDecisionMakerType)
		m_nDecisionMakerType = m_nDecisionMakerTypeInGroup;
}

// SA CPedIntelligence::SetHearingRange (0x600BE0).
void
CPedIntelligence::SetHearingRange(float range)
{
	m_fHearingRange = range;
}

// SA CPedIntelligence::SetSeeingRange (0x600BF0).
void
CPedIntelligence::SetSeeingRange(float range)
{
	m_fSeeingRange = range;
}

// SA CPedIntelligence::IsInHearingRange (0x600C00): straight distance
// check against the hearing range.
bool
CPedIntelligence::IsInHearingRange(const CVector &posn)
{
	CVector dist = posn - m_pPed->GetPosition();
	return dist.MagnitudeSqr() < m_fHearingRange * m_fHearingRange;
}

// SA CPedIntelligence::IsInSeeingRange (0x600C60): in range and in front.
bool
CPedIntelligence::IsInSeeingRange(const CVector &posn) const
{
	CVector distance = posn - m_pPed->GetPosition();
	if(m_fSeeingRange * m_fSeeingRange > distance.MagnitudeSqr()){
		if(DotProduct(distance, m_pPed->GetForward()) > 0.0f)
			return true;
	}
	return false;
}

// SA CPedIntelligence::IsRespondingToEvent (0x600DB0).
bool
CPedIntelligence::IsRespondingToEvent(eEventType eventType)
{
	return m_eventHandler.GetHistory().IsRespondingToEvent(eventType);
}

// SA CPedIntelligence::AddTaskPhysResponse (0x600DC0).
void
CPedIntelligence::AddTaskPhysResponse(CTask *task, int32 unUsed)
{
	(void)unUsed;
	m_TaskMgr.SetTask(task, TASK_PRIMARY_PHYSICAL_RESPONSE);
}

// SA CPedIntelligence::AddTaskEventResponseTemp (0x600DE0).
void
CPedIntelligence::AddTaskEventResponseTemp(CTask *task, int32 unUsed)
{
	(void)unUsed;
	m_TaskMgr.SetTask(task, TASK_PRIMARY_EVENT_RESPONSE_TEMP);
}

// SA CPedIntelligence::AddTaskEventResponseNonTemp (0x600E00).
void
CPedIntelligence::AddTaskEventResponseNonTemp(CTask *task, int32 unUsed)
{
	(void)unUsed;
	m_TaskMgr.SetTask(task, TASK_PRIMARY_EVENT_RESPONSE_NONTEMP);
}

// SA CPedIntelligence::AddTaskPrimaryMaybeInGroup (0x600E20): a lone ped
// (or the player) takes the task through its own event group as a script
// command. SA routes group members through CPedGroupIntelligence instead;
// ped groups land with the SA group port.
void
CPedIntelligence::AddTaskPrimaryMaybeInGroup(CTask *task, bool bAffectsPed)
{
	CEventScriptCommand *event = new CEventScriptCommand(TASK_PRIMARY_PRIMARY, task, bAffectsPed);
	m_eventGroup.Add(event);
	delete event; // the group queued a clone; the original task goes with it
}

// SA CPedIntelligence::FindTaskByType (0x600EE0).
CTask*
CPedIntelligence::FindTaskByType(eTaskType taskId)
{
	static const ePrimaryTasks slots[] = {
		TASK_PRIMARY_DEFAULT, TASK_PRIMARY_PRIMARY,
		TASK_PRIMARY_EVENT_RESPONSE_TEMP, TASK_PRIMARY_EVENT_RESPONSE_NONTEMP
	};
	for(int i = 0; i < 4; i++){
		if(CTask *task = m_TaskMgr.FindTaskByType(slots[i], taskId))
			return task;
	}
	return nil;
}

// SA CPedIntelligence::RecordEventForScript (0x6024B0 in the SA numbering
// above): only the strongest event is reported, and script commands are
// not reported at all - the script already knows.
void
CPedIntelligence::RecordEventForScript(int32 eventId, int32 eventPriority)
{
	if(eventId != EVENT_SCRIPT_COMMAND && (eventId == 0 || eventPriority > m_nEventPriority)){
		m_nEventId = (uint8)eventId;
		m_nEventPriority = (uint8)eventPriority;
	}
}

// SA CPedIntelligence::IncrementAngerAtPlayer (0x421050 in the SA
// numbering of the mental state): delegates to CMentalState.
void
CPedIntelligence::IncrementAngerAtPlayer(uint8 anger)
{
	m_mentalState.IncrementAnger(anger);
}

// SA CPedIntelligence::ProcessFirst (0x600A30): the cheap checks that run
// before everything else - is this ped stuck, has it been hit by a car.
void
CPedIntelligence::ProcessFirst()
{
	ProcessStaticCounter();
	m_pedStuckChecker.TestPedStuck(m_pPed, &m_eventGroup);
	m_collisionScanner.ScanForCollisionEvents(m_pPed, &m_eventGroup);
}

// SA CPedIntelligence::Process (0x6009B0): scan the world, turn it into
// events, answer the winning event, then run the tasks. (SA also records
// relationships with the player here; that lands with the relationship
// recorder.)
void
CPedIntelligence::Process()
{
	m_vehicleScanner.ScanForVehiclesInRange(*m_pPed);
	m_pedScanner.ScanForPedsInRange(*m_pPed);
	m_eventScanner.ScanForEvents(*m_pPed);
	m_eventHandler.HandleEvents();
	m_TaskMgr.ManageTasks();
	LookAtInterestingEntities();
}

// SA CPedIntelligence::FlushIntelligence (0x601DA0): drop every task,
// queued event, handler memory and scanner reference.
void
CPedIntelligence::FlushIntelligence()
{
	m_TaskMgr.Flush();
	m_eventHandler.ResetHistory();
	m_eventHandler.ResetResponse();
	m_eventGroup.Flush(false);
	m_vehicleScanner.Clear();
	m_pedScanner.Clear();
	m_eventScanner.Clear();
}

// Task accessors. SA versions dyn-cast the relevant slot; here the task
// manager's type search does the same job (it only returns a task whose
// type id matches, so the casts are as safe as SA's).
CTaskSimpleFight*
CPedIntelligence::GetTaskFighting()
{
	return (CTaskSimpleFight*)m_TaskMgr.FindTaskOfType(TASK_SIMPLE_FIGHT, true);
}

CTaskSimpleUseGun*
CPedIntelligence::GetTaskUseGun()
{
	return (CTaskSimpleUseGun*)m_TaskMgr.FindTaskOfType(TASK_SIMPLE_USE_GUN, true);
}

CTaskSimpleThrowProjectile*
CPedIntelligence::GetTaskThrow()
{
	return (CTaskSimpleThrowProjectile*)m_TaskMgr.FindTaskOfType(TASK_SIMPLE_THROW_PROJECTILE, true);
}

CTaskSimpleHoldEntity*
CPedIntelligence::GetTaskHold(bool bIgnoreCheckingForSimplestActiveTask)
{
	(void)bIgnoreCheckingForSimplestActiveTask;
	return (CTaskSimpleHoldEntity*)m_TaskMgr.FindTaskOfType(TASK_SIMPLE_HOLD_ENTITY, true);
}

CTaskSimpleSwim*
CPedIntelligence::GetTaskSwim()
{
	return (CTaskSimpleSwim*)m_TaskMgr.FindTaskOfType(TASK_SIMPLE_SWIM, true);
}

CTaskSimpleDuck*
CPedIntelligence::GetTaskDuck(bool bIgnoreCheckingForSimplestActiveTask)
{
	(void)bIgnoreCheckingForSimplestActiveTask;
	return (CTaskSimpleDuck*)m_TaskMgr.FindTaskOfType(TASK_SIMPLE_DUCK, true);
}

CTaskSimpleJetPack*
CPedIntelligence::GetTaskJetPack()
{
	return (CTaskSimpleJetPack*)m_TaskMgr.FindTaskOfType(TASK_SIMPLE_JETPACK, true);
}

CTaskSimpleInAir*
CPedIntelligence::GetTaskInAir()
{
	return (CTaskSimpleInAir*)m_TaskMgr.FindTaskOfType(TASK_SIMPLE_IN_AIR, true);
}

CTaskSimpleClimb*
CPedIntelligence::GetTaskClimb()
{
	return (CTaskSimpleClimb*)m_TaskMgr.FindTaskOfType(TASK_SIMPLE_CLIMB, true);
}

CTaskSimpleDuck*
CPedIntelligence::GetTaskSecondaryDuck()
{
	CTask *task = m_TaskMgr.GetTaskSecondary(TASK_SECONDARY_DUCK);
	return task && task->GetTaskType() == TASK_SIMPLE_DUCK ? (CTaskSimpleDuck*)task : nil;
}

// SA CPedIntelligence::GetUsingParachute: no parachute task exists yet.
bool
CPedIntelligence::GetUsingParachute()
{
	return false;
}

// SA CPedIntelligence::SetTaskDuckSecondary (0x601230): give the ped the
// secondary duck task (unless a script already controls ducking) and run
// one tick of it immediately so the pose starts this frame. (SA also
// clears the use-gun task's anim here; not ported yet.)
void
CPedIntelligence::SetTaskDuckSecondary(uint16 nLengthOfDuck)
{
	CTaskSimpleDuck *duck = GetTaskDuck();
	if(duck && duck->m_DuckControlType == DUCK_SCRIPT_CONTROLLED)
		return;

	m_TaskMgr.SetTaskSecondary(new CTaskSimpleDuck(DUCK_TASK_CONTROLLED, nLengthOfDuck), TASK_SECONDARY_DUCK);
	((CTaskSimpleDuck*)m_TaskMgr.GetTaskSecondary(TASK_SECONDARY_DUCK))->ProcessPed(m_pPed);
}

// SA CPedIntelligence::ClearTaskDuckSecondary (0x601390).
void
CPedIntelligence::ClearTaskDuckSecondary()
{
	CTask *secondaryDuck = m_TaskMgr.GetTaskSecondary(TASK_SECONDARY_DUCK);
	if(secondaryDuck == nil)
		return;

	secondaryDuck->MakeAbortable(m_pPed, ABORT_PRIORITY_LEISURE, nil);
	if(m_pPed->IsPlayer())
		((CPlayerPed*)m_pPed)->GetPlayerData()->m_fMoveBlendRatio = 0.0f;
	else
		m_pPed->SetMoveState(PEDMOVE_STILL);
	// SA also clears the use-gun secondary attack task's anim here; not
	// ported yet.
}

// SA CPedIntelligence::ClearTasks (0x600FB0 in SA numbering): park the
// ped through a script command (stand still on foot; SA queues a car
// drive task for a ped in a vehicle - the car tasks are not ported yet),
// run the handler so the command lands, then abort the secondary tasks
// (except the facial one, which SA always keeps).
void
CPedIntelligence::ClearTasks(bool bClearPrimaryTasks, bool bClearSecondaryTasks)
{
	if(bClearPrimaryTasks){
		if(m_pPed->bInVehicle && m_pPed->m_pMyVehicle){
			if(!m_eventGroup.HasScriptCommandOfTaskType(TASK_SIMPLE_CAR_DRIVE)){
				CTask *driveTask;
				CTask *defaultTask = m_TaskMgr.GetTaskPrimary(TASK_PRIMARY_DEFAULT);
				if(defaultTask && defaultTask->GetTaskType() == TASK_SIMPLE_CAR_DRIVE)
					driveTask = new CTaskSimpleCarDriveTimed(m_pPed->m_pMyVehicle, 0);
				else
					driveTask = new CTaskSimpleCarDrive(m_pPed->m_pMyVehicle, nil, false);
				CEventScriptCommand eventScriptCommand(TASK_PRIMARY_PRIMARY, driveTask, false);
				m_eventGroup.Add(&eventScriptCommand, false);
			}
		}else if(!m_eventGroup.HasScriptCommandOfTaskType(TASK_SIMPLE_STAND_STILL)){
			CTaskSimpleStandStill *standStill = new CTaskSimpleStandStill(0, false, false, 8.0f);
			CEventScriptCommand eventScriptCommand(TASK_PRIMARY_PRIMARY, standStill, false);
			m_eventGroup.Add(&eventScriptCommand, false);
		}

		m_eventHandler.HandleEvents();
		m_TaskMgr.ManageTasks();
		// SA also processes CPedScriptedTaskRecord here; not ported yet.
	}

	if(!bClearSecondaryTasks)
		return;

	for(int32 i = 0; i < TASK_SECONDARY_MAX; i++){
		CTask *task = m_TaskMgr.GetTaskSecondary((eSecondaryTask)i);
		if(task == nil)
			continue;
		if(i == TASK_SECONDARY_FACIAL_COMPLEX)
			continue;
		if(task->MakeAbortable(m_pPed))
			m_TaskMgr.SetTaskSecondary(nil, (eSecondaryTask)i);
		else
			task->MakeAbortable(m_pPed, ABORT_PRIORITY_LEISURE, nil);
	}
}

// SA CPedIntelligence::FlushImmediately (0x6010C0 in SA numbering). SA
// additionally rescues the in-group, hold-entity and facial tasks across
// the flush; those tasks are not ported yet, so here everything goes and
// the ped is re-parked on its default task.
void
CPedIntelligence::FlushImmediately(bool bSetPrimaryDefaultTask)
{
	m_TaskMgr.FlushImmediately();
	m_eventGroup.Flush();

	if(bSetPrimaryDefaultTask){
		CTask *task;
		if(m_pPed->IsPlayer() || m_pPed->CharCreatedBy == MISSION_CHAR)
			task = new CTaskSimpleStandStill(0, true, false, 8.0f);
		else
			task = CTaskComplexWander::GetWanderTaskByPedType(m_pPed);
		m_TaskMgr.SetTask(task, TASK_PRIMARY_DEFAULT);
	}
}

// SA CPedIntelligence::ProcessEventHandler.
void
CPedIntelligence::ProcessEventHandler()
{
	m_eventHandler.HandleEvents();
}

// SA CPedIntelligence::IsInACarOrEnteringOne. The enter-car tasks are not
// ported yet; the driving task lookup is SA's third check.
bool
CPedIntelligence::IsInACarOrEnteringOne()
{
	CTask *drive = m_TaskMgr.FindTaskOfType(TASK_SIMPLE_CAR_DRIVE, true);
	if(drive && ((CTaskSimpleCarDrive*)drive)->GetVehicle())
		return true;
	return m_pPed->bInVehicle;
}

// SA CPedIntelligence::IsPedGoingSomewhereOnFoot (0x601640).
bool
CPedIntelligence::IsPedGoingSomewhereOnFoot()
{
	CTask *simplest = m_TaskMgr.GetSimplestActiveTask();
	return simplest && CTask::IsGoToTask(simplest);
}

// SA CPedIntelligence::GetMoveStateFromGoToTask (0x601D70).
eMoveState
CPedIntelligence::GetMoveStateFromGoToTask()
{
	CTask *simplest = m_TaskMgr.GetSimplestActiveTask();
	return simplest && CTask::IsGoToTask(simplest)
		? ((CTaskSimpleGoTo*)simplest)->m_moveState
		: PEDMOVE_STILL;
}

// SA CPedIntelligence::ProcessStaticCounter (0x6020B0 in SA numbering):
// watches whether a ped with a go-to task is actually getting anywhere.
// m_AnotherStaticCounter is the "has been stuck for a while" signal the
// wander task reacts to.
void
CPedIntelligence::ProcessStaticCounter()
{
	CTask *task = m_TaskMgr.GetSimplestActiveTask();
	if(task == nil || !CTask::IsGoToTask(task)){
		if(m_StaticCounter == -2)
			m_StaticCounter = 4;
		else
			m_StaticCounter++;
		m_AnotherStaticCounter = 0;
		return;
	}

	int32 moveState = ((CTaskSimpleGoTo*)task)->m_moveState;
	if(moveState != PEDMOVE_WALK && moveState != PEDMOVE_RUN && moveState != PEDMOVE_SPRINT){
		if(m_StaticCounter == -2)
			m_StaticCounter = 4;
		else
			m_StaticCounter++;
		m_AnotherStaticCounter = 0;
		return;
	}

	if(m_pPed->m_pDamageEntity){
		if(m_StaticCounter > 4)
			m_vecLastPedPosDuringDamageEntity = m_pPed->GetPosition();
		m_StaticCounter = 0;
	}else{
		if(m_StaticCounter == -2)
			m_StaticCounter = 4;
		else
			m_StaticCounter++;
	}

	if(m_StaticCounter > 4){
		m_AnotherStaticCounter = 0;
		return;
	}

	CVector dist = m_pPed->GetPosition() - m_vecLastPedPosDuringDamageEntity;
	if(dist.MagnitudeSqr() < 1.0f / 16.0f){
		m_AnotherStaticCounter++;
	}else{
		m_AnotherStaticCounter = 0;
		m_vecLastPedPosDuringDamageEntity = m_pPed->GetPosition();
	}
}

// SA CPedIntelligence::GetActivePrimaryTask (0x4ABEA0 is GetPedFOVRange;
// this one sits next to it). SA's own loop re-checks the first slot three
// times; the intended reading - first non-empty response slot - is used.
CTask*
CPedIntelligence::GetActivePrimaryTask() const
{
	static const ePrimaryTasks slots[] = {
		TASK_PRIMARY_PHYSICAL_RESPONSE, TASK_PRIMARY_EVENT_RESPONSE_TEMP, TASK_PRIMARY_EVENT_RESPONSE_NONTEMP
	};
	for(int i = 0; i < 3; i++){
		if(CTask *task = m_TaskMgr.GetTaskPrimary(slots[i]))
			return task;
	}
	return nil;
}

// SA CPedIntelligence::GetPedFOVRange (0x4ABEA0).
float
CPedIntelligence::GetPedFOVRange() const
{
	return Max(m_fHearingRange, m_fSeeingRange);
}

// SA CPedIntelligence::IsUsingGun.
bool
CPedIntelligence::IsUsingGun()
{
	return GetTaskUseGun() != nil;
}

// SA CPedIntelligence::HasInterestingEntites (sic, SA spelling).
bool
CPedIntelligence::HasInterestingEntites()
{
	for(int i = 0; i < 3; i++)
		if(m_apInterestingEntities[i])
			return true;
	return false;
}

// SA CPedIntelligence::IsInterestingEntity.
bool
CPedIntelligence::IsInterestingEntity(CEntity *entity)
{
	for(int i = 0; i < 3; i++)
		if(m_apInterestingEntities[i] == entity)
			return true;
	return false;
}

// SA CPedIntelligence::RemoveAllInterestingEntities.
void
CPedIntelligence::RemoveAllInterestingEntities()
{
	for(int i = 0; i < 3; i++){
		if(m_apInterestingEntities[i])
			m_apInterestingEntities[i]->CleanUpOldReference(&m_apInterestingEntities[i]);
		m_apInterestingEntities[i] = nil;
	}
}

// SA CPedIntelligence::LookAtInterestingEntities: SA makes the ped glance
// at the most interesting nearby entity through a look task; the glance
// itself lands with CTaskSimpleLookAtEntity. For now the list is kept
// clean of dead references.
void
CPedIntelligence::LookAtInterestingEntities()
{
	for(int i = 0; i < 3; i++){
		if(m_apInterestingEntities[i] && m_apInterestingEntities[i]->IsPed() && ((CPed*)m_apInterestingEntities[i])->Dead()){
			m_apInterestingEntities[i]->CleanUpOldReference(&m_apInterestingEntities[i]);
			m_apInterestingEntities[i] = nil;
		}
	}
}
