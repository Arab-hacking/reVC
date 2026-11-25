#pragma once

#include "Event.h"
#include "eFacialExpression.h"
#include "eTaskType.h"

class CPedGroup;

// SA CEventEditableResponse (0x4AC480): an event a decision maker can
// answer with a task (and a facial expression). The event group asks it
// for its response when the event is added, so by the time the event
// handler sees the event it already knows what the ped wants to do.
class CEventEditableResponse : public CEvent
{
public:
	bool m_bAddToEventGroup;
	int16 m_TaskId; // eTaskType the decision maker chose
	int16 m_FacialExpressionType; // eFacialExpression

public:
	CEventEditableResponse(eTaskType taskType = TASK_NONE);
	~CEventEditableResponse() {}

	CEvent *Clone() const;
	bool HasEditableResponse() const { return true; }
	virtual CEventEditableResponse *CloneEditable() const = 0;

	bool WillRespond() const;
	void InformVehicleOccupants(CPed *ped);
	void InformRespectedFriends(CPed *ped);
	void InformGroup(CPed *ped);
	void TriggerLookAt(CPed *ped) const;
	void ComputeResponseTaskType(CPed *ped, bool bDecisionMakerTypeInGroup);
	void ComputeResponseTaskType(CPedGroup *pedGroup);
	bool ComputeResponseTaskOfType(CPed *ped, eTaskType taskId);

	eFacialExpression GetFacialExpressionType() const { return (eFacialExpression)m_FacialExpressionType; }
	eTaskType GetTaskId() const { return (eTaskType)m_TaskId; }
};

VALIDATE_SIZE(CEventEditableResponse, 0x14);
