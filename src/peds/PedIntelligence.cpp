#include "common.h"

#include "PedIntelligence.h"
#include "Ped.h"
#include "Vehicle.h"
#include "EventScriptCommand.h"
#include "DecisionMakerTypes.h"
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

// SA CPedIntelligence::FlushIntelligence (0x600F10 in part): drop every
// queued event and the handler's memory of them.
void
CPedIntelligence::FlushIntelligence()
{
	m_eventGroup.Flush();
	m_eventHandler.GetHistory().Flush();
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
