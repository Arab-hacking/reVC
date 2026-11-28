#include "common.h"

#include "EventEditableResponse.h"
#include "EventSource.h"
#include "EventGroup.h"
#include "DecisionMakerTypes.h"
#include "Ped.h"
#include "PedGroup.h"
#include "PedGroups.h"
#include "PedIntelligence.h"
#include "Vehicle.h"

// SA CEventEditableResponse::CEventEditableResponse (0x4AC480).
CEventEditableResponse::CEventEditableResponse(eTaskType taskType)
	: CEvent()
{
	m_bAddToEventGroup     = true;
	m_TaskId               = (int16)taskType;
	m_FacialExpressionType = (int16)eFacialExpression::NONE;
}

// SA CEventEditableResponse::Clone (0x420ED0): the clone carries the
// response the decision maker already chose.
CEvent*
CEventEditableResponse::Clone() const
{
	CEventEditableResponse *clone = CloneEditable();
	clone->m_TaskId               = m_TaskId;
	clone->m_FacialExpressionType = m_FacialExpressionType;
	clone->m_bAddToEventGroup     = m_bAddToEventGroup;
	return clone;
}

// SA CEventEditableResponse::WillRespond (0x4AC490).
bool
CEventEditableResponse::WillRespond() const
{
	return m_TaskId != TASK_NONE;
}

// SA CEventEditableResponse::InformVehicleOccupants (0x4AC4A0): everybody
// else in the car hears about it too, but only as witnesses - their copy
// carries no response of its own and does not enter through the group's
// usual response gate the same way.
void
CEventEditableResponse::InformVehicleOccupants(CPed *ped)
{
	CVehicle *vehicle = ped->m_pMyVehicle;
	if(!ped->bInVehicle || vehicle == nil || !DoInformVehicleOccupants(ped) || !m_bAddToEventGroup)
		return;

	CPed *occupants[9];
	int32 count = 0;
	if(vehicle->pDriver)
		occupants[count++] = vehicle->pDriver;
	for(int32 i = 0; i < 8; i++)
		if(vehicle->GetPassenger(i))
			occupants[count++] = vehicle->GetPassenger(i);

	for(int32 i = 0; i < count; i++){
		CPed *occupant = occupants[i];
		if(occupant == nil || occupant == ped || occupant->m_pIntelligence == nil)
			continue;
		CEventEditableResponse *clone = (CEventEditableResponse*)Clone();
		clone->m_TaskId           = TASK_NONE;
		clone->m_bAddToEventGroup = false;
		occupant->m_pIntelligence->GetEventGroup().Add(clone);
		delete clone;
	}
}

// SA CEventEditableResponse::InformRespectedFriends (0x4B2B00) routes
// through SA's acquaintance system, which is not ported yet.
void
CEventEditableResponse::InformRespectedFriends(CPed *ped)
{
	(void)ped;
}

// SA CEventEditableResponse::InformGroup (0x4B7DF0): SA routes the
// event through CInformGroupEventQueue and lets the group intelligence
// hand it out; that queue is not ported yet, so here every other group
// member is informed directly, the same way InformVehicleOccupants
// does it. The m_bAddToEventGroup check keeps an informed copy from
// informing the group all over again.
void
CEventEditableResponse::InformGroup(CPed *ped)
{
	if(!m_bAddToEventGroup)
		return;

	CPedGroup *pedGroup = CPedGroups::GetPedsGroup(ped);
	if(!pedGroup)
		return;

	for(int32 i = 0; i < TOTAL_PED_GROUP_MEMBERS; i++){
		CPed *member = pedGroup->GetMembership().GetMember(i);
		if(member == nil || member == ped || member->m_pIntelligence == nil)
			continue;
		CEventEditableResponse *clone = (CEventEditableResponse*)Clone();
		clone->m_TaskId           = TASK_NONE;
		clone->m_bAddToEventGroup = false;
		member->m_pIntelligence->GetEventGroup().Add(clone);
		delete clone;
	}
}

// SA CEventEditableResponse::TriggerLookAt (0x4AC5A0) drives the SA IK
// chain manager; lands with the SA look-at task/IK port.
void
CEventEditableResponse::TriggerLookAt(CPed *ped) const
{
	(void)ped;
}

// SA CEventEditableResponse::ComputeResponseTaskType (0x4B56C0): ask the
// ped's decision maker what to do about this event. The inform/look-at
// responses are excluded here - EventGroup checks those separately.
void
CEventEditableResponse::ComputeResponseTaskType(CPed *ped, bool bDecisionMakerTypeInGroup)
{
	if(m_TaskId != TASK_NONE)
		return;
	int16 taskId = m_TaskId;
	int16 facialTaskId = m_FacialExpressionType;
	CDecisionMakerTypes::GetInstance()->MakeDecision(
		ped,
		GetEventType(),
		CEventSource::ComputeEventSourceType(*this, *ped),
		ped->bInVehicle,
		TASK_SIMPLE_INFORM_RESPECTED_FRIENDS,
		TASK_SIMPLE_INFORM_GROUP,
		TASK_SIMPLE_LOOK_AT_ENTITY_OR_COORD,
		TASK_INVALID,
		bDecisionMakerTypeInGroup,
		taskId,
		facialTaskId);
	m_TaskId = taskId;
	m_FacialExpressionType = facialTaskId;
}

// SA CEventEditableResponse::ComputeResponseTaskType, group form
// (0x4B57A0): the group answers as a whole, through a representative
// member - the leader, or (when the leader is the player, or there is
// no leader) the first follower; if nobody answered and the leader is
// the player, the player-led group still gets its say.
void
CEventEditableResponse::ComputeResponseTaskType(CPedGroup *pedGroup)
{
	if(m_TaskId != TASK_NONE)
		return;

	CPed *groupLeader = pedGroup->GetMembership().GetLeader();
	CPed *member = groupLeader;
	if(groupLeader && groupLeader->IsPlayer())
		member = nil;
	if(!member){
		for(int32 memberId = 0; memberId < TOTAL_PED_GROUP_FOLLOWERS; memberId++){
			if((member = pedGroup->GetMembership().GetMember(memberId)) != nil)
				break;
		}
	}
	if(member){
		m_TaskId = (int16)CDecisionMakerTypes::GetInstance()->MakeDecision(
			pedGroup,
			GetEventType(),
			CEventSource::ComputeEventSourceType(*this, *member),
			member->bInVehicle,
			TASK_SIMPLE_INFORM_GROUP,
			TASK_SIMPLE_INFORM_RESPECTED_FRIENDS,
			TASK_SIMPLE_LOOK_AT_ENTITY_OR_COORD,
			TASK_INVALID);
	}else{
		m_TaskId = TASK_NONE;
	}

	groupLeader = pedGroup->GetMembership().GetLeader();
	if(m_TaskId == TASK_NONE && groupLeader && groupLeader->IsPlayer()){
		m_TaskId = (int16)CDecisionMakerTypes::GetInstance()->MakeDecision(
			pedGroup,
			GetEventType(),
			CEventSource::ComputeEventSourceType(*this, *groupLeader),
			groupLeader->bInVehicle,
			TASK_SIMPLE_INFORM_GROUP,
			TASK_SIMPLE_INFORM_RESPECTED_FRIENDS,
			TASK_SIMPLE_LOOK_AT_ENTITY_OR_COORD,
			TASK_INVALID);
	}
}

// SA CEventEditableResponse::ComputeResponseTaskOfType (0x4B5730): would
// the decision maker answer this event with exactly this task?
bool
CEventEditableResponse::ComputeResponseTaskOfType(CPed *ped, eTaskType taskId)
{
	int16 outTaskId      = TASK_INVALID;
	int16 facialTaskType = TASK_INVALID;
	CDecisionMakerTypes::GetInstance()->MakeDecision(
		ped,
		GetEventType(),
		CEventSource::ComputeEventSourceType(*this, *ped),
		ped->bInVehicle,
		TASK_INVALID,
		TASK_INVALID,
		TASK_INVALID,
		taskId,
		false,
		outTaskId,
		facialTaskType);
	return taskId == outTaskId;
}
