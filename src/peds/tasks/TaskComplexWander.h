#ifndef __GTA_TASKCOMPLEXWANDER_H__
#define __GTA_TASKCOMPLEXWANDER_H__

#include "TaskComplex.h"
#include "NodeAddress.h"
#include "PedType.h"

enum eWanderType : int32
{
	WANDER_TYPE_STANDARD = 0,
	WANDER_TYPE_COP,
	WANDER_TYPE_MEDIC,
	WANDER_TYPE_CRIMINAL,
	WANDER_TYPE_GANG,
	WANDER_TYPE_SHOP,
	WANDER_TYPE_FLEE,
	WANDER_TYPE_PROSTITUTE
};

class CPed;
class CEntity;
class CVector;

// SA CTaskComplexWander (0x28): walk the ped path network indefinitely,
// picking a new node at every junction. This is what ambient peds do when
// nobody is telling them anything - the SA replacement for VC's wander
// objective. The concrete wander tasks differ in what they look for while
// walking (ScanForStuff).
class CTaskComplexWander : public CTaskComplex
{
public:
	eMoveState m_nMoveState;
	uint8 m_nDir;
	float m_fTargetRadius;
	CNodeAddress m_LastNode;
	CNodeAddress m_NextNode;
	uint32 m_nLastUpdateDirFrameCount;
	union {
		uint8 m_nFlags;
		struct {
			uint8 m_bWanderSensibly : 1;
			uint8 m_bNewDir : 1;
			uint8 m_bNewNodes : 1;
			uint8 m_bAllNodesBlocked : 1;
		};
	};

public:
	static const eTaskType Type = TASK_COMPLEX_WANDER;

	CTaskComplexWander(eMoveState moveState, uint8 dir, bool bWanderSensibly = true, float fTargetRadius = 0.5f);
	~CTaskComplexWander() {}

	eTaskType GetTaskType() const { return Type; }
	CTask *CreateNextSubTask(CPed *ped);
	CTask *CreateFirstSubTask(CPed *ped);
	CTask *ControlSubTask(CPed *ped);
	virtual eWanderType GetWanderType() = 0;
	virtual void ScanForStuff(CPed *ped) = 0;
	virtual void UpdateDir(CPed *ped);
	virtual void UpdatePathNodes(const CPed *ped, uint8 dir, CNodeAddress &originNode, CNodeAddress &targetNode, uint8 &outDir);

	CTask *CreateSubTask(CPed *ped, eTaskType taskType);
	void ComputeTargetPos(const CPed *ped, CVector &outTargetPos, const CNodeAddress &targetNodeAddress);
	float ComputeTargetHeading(CPed *ped);
	bool ValidNodes() const;
	void ScanForBlockedNodes(CPed *ped);
	bool ScanForBlockedNode(CPed *ped, const CNodeAddress &targetNodeAddress);
	bool ScanForBlockedNode(const CVector &position, CEntity *entity);

	float GetDistSqOfClosestPathNodeToPed(CPed *ped);

	static CTaskComplexWander *GetWanderTaskByPedType(CPed *ped);
	void SetDirection(uint8 dir);
	void SetPathNodesFrom(const CTaskComplexWander &other);
};

VALIDATE_SIZE(CTaskComplexWander, 0x28);

#endif // __GTA_TASKCOMPLEXWANDER_H__
