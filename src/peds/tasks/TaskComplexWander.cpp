#include "common.h"

#include "TaskComplexWander.h"
#include "General.h"
#include "PathFind.h"
#include "Ped.h"
#include "PedIntelligence.h"
#include "TaskComplexSequence.h"
#include "TaskComplexWanderMedic.h"
#include "TaskComplexWanderStandard.h"
#include "TaskSimpleGoToPoint.h"
#include "TaskSimpleRunAnim.h"
#include "TaskSimpleScratchHead.h"
#include "TaskSimpleStandStill.h"
#include "Timer.h"
#include "Vehicle.h"

// SA addresses path nodes by (area, node); reVC's network is one global
// array, so the node id is the global index and area 0 is "valid".
static CPathNode*
NodeFromAddress(const CNodeAddress &address)
{
	return address.IsValid() ? ThePaths.GetNode(address.m_wNodeId) : nil;
}

static CNodeAddress
AddressFromNode(CPathNode *node)
{
	return node ? CNodeAddress(0, ThePaths.GetIndex(node)) : CNodeAddress();
}

// SA CTaskComplexWander::CTaskComplexWander (0x66F450).
CTaskComplexWander::CTaskComplexWander(eMoveState moveState, uint8 dir, bool bWanderSensibly, float fTargetRadius)
	: CTaskComplex()
{
	m_nMoveState = moveState;
	m_nDir = dir;
	m_fTargetRadius = fTargetRadius;
	m_bWanderSensibly = bWanderSensibly;
	m_bNewDir = false;
	m_bNewNodes = false;
	m_bAllNodesBlocked = false;
	m_nLastUpdateDirFrameCount = 0;
}

// SA CTaskComplexWander::CreateNextSubTask (0x674140).
CTask*
CTaskComplexWander::CreateNextSubTask(CPed *ped)
{
	switch(m_pSubTask->GetTaskType()){
	case TASK_SIMPLE_SCRATCH_HEAD: {
		m_nDir++;
		UpdatePathNodes(ped, m_nDir, m_LastNode, m_NextNode, m_nDir);

		if(ValidNodes())
			return CreateSubTask(ped, TASK_SIMPLE_GO_TO_POINT);

		return new CTaskSimpleScratchHead();
	}
	case TASK_COMPLEX_CROSS_ROAD_LOOK_AND_ACHIEVE_HEADING:
	case TASK_COMPLEX_OBSERVE_TRAFFIC_LIGHTS_AND_ACHIEVE_HEADING:
		return CreateSubTask(ped, TASK_SIMPLE_GO_TO_POINT);

	case TASK_SIMPLE_CAR_DRIVE_TIMED:
		return CreateFirstSubTask(ped);

	case TASK_COMPLEX_LEAVE_CAR:
		if(ped->bInVehicle)
			return CreateSubTask(ped, TASK_SIMPLE_CAR_DRIVE_TIMED);
		return CreateFirstSubTask(ped);

	case TASK_COMPLEX_SEQUENCE:
	case TASK_SIMPLE_GO_TO_POINT: {
		if(m_bAllNodesBlocked){
			m_bAllNodesBlocked = false;
			return new CTaskSimpleScratchHead();
		}

		UpdateDir(ped);
		const CNodeAddress prevLastNode = m_LastNode;
		UpdatePathNodes(ped, m_nDir, m_LastNode, m_NextNode, m_nDir);

		if(m_NextNode == prevLastNode){
			// Turning back the way we came: pause, look, think, then go.
			CVector outTargetPos;
			ComputeTargetPos(ped, outTargetPos, m_NextNode);
			CTaskComplexSequence *sequence = new CTaskComplexSequence();
			sequence->AddTask(new CTaskSimpleStandStill(500, false, false, 8.0f));
			sequence->AddTask(new CTaskSimpleRunAnim(ped->m_animGroup, ANIM_ID_ROADCROSS, 4.0f, false));
			sequence->AddTask(new CTaskSimpleScratchHead());
			sequence->AddTask(new CTaskSimpleGoToPoint(m_nMoveState, outTargetPos, m_fTargetRadius, false, false));
			return sequence;
		}

		if(!ValidNodes())
			return new CTaskSimpleScratchHead();

		// SA's sensible wander detours into the traffic-light and
		// cross-road look tasks here; those are not ported yet, so a
		// sensible ped simply walks on.
		return CreateSubTask(ped, TASK_SIMPLE_GO_TO_POINT);
	}
	case TASK_COMPLEX_IN_AIR_AND_LAND:
	default:
		return nil;
	}
}

// SA CTaskComplexWander::CreateFirstSubTask (0x6740E0).
CTask*
CTaskComplexWander::CreateFirstSubTask(CPed *ped)
{
	if(ped->bInVehicle)
		return CreateSubTask(ped, TASK_COMPLEX_LEAVE_CAR);

	UpdateDir(ped);
	UpdatePathNodes(ped, m_nDir, m_LastNode, m_NextNode, m_nDir);
	return CreateSubTask(ped, TASK_SIMPLE_GO_TO_POINT);
}

// SA CTaskComplexWander::ControlSubTask (0x674C30).
CTask*
CTaskComplexWander::ControlSubTask(CPed *ped)
{
	eTaskType subTaskId = m_pSubTask->GetTaskType();
	if(subTaskId == TASK_COMPLEX_LEAVE_CAR || subTaskId == TASK_SIMPLE_CAR_DRIVE_TIMED)
		return m_pSubTask;

	if(m_bNewDir && subTaskId == TASK_SIMPLE_GO_TO_POINT){
		m_bNewDir = false;
		m_NextNode.ResetAreaId();
		m_LastNode.ResetAreaId();
		return CreateFirstSubTask(ped);
	}

	if(m_bNewNodes && subTaskId == TASK_SIMPLE_GO_TO_POINT){
		m_bNewNodes = false;
		CVector outTargetPos;
		ComputeTargetPos(ped, outTargetPos, m_NextNode);
		((CTaskSimpleGoToPoint*)m_pSubTask)->UpdatePoint(outTargetPos, 0.5f, false);
	}else{
		ScanForBlockedNodes(ped);
		ScanForStuff(ped);
	}

	if(subTaskId == TASK_SIMPLE_GO_TO_POINT){
		((CTaskSimpleGoToPoint*)m_pSubTask)->m_moveState = m_nMoveState;
		// SA makes a walking ped chat here (Say(CTX_GLOBAL_CHAT)); the
		// SA speech-context system is not ported yet.
	}

	if(ped->GetIntelligence() && ped->GetIntelligence()->m_AnotherStaticCounter > 30){
		if(m_pSubTask->MakeAbortable(ped))
			return CreateSubTask(ped, TASK_SIMPLE_SCRATCH_HEAD);
	}
	return m_pSubTask;
}

// SA CTaskComplexWander::UpdateDir (0x669DA0): at a junction, usually
// carry on, sometimes turn.
void
CTaskComplexWander::UpdateDir(CPed *ped)
{
	uint8 newDir = m_nDir;
	if(m_NextNode.IsAreaValid()){
		CPathNode *pathNode = NodeFromAddress(m_NextNode);
		if(pathNode){
			if(pathNode->numLinks >= 3 && CTimer::GetFrameCounter() != m_nLastUpdateDirFrameCount && m_bWanderSensibly){
				m_nLastUpdateDirFrameCount = CTimer::GetFrameCounter();
				uint8 remainder = (3 * CTimer::GetFrameCounter() + ped->m_nRandomSeed) % 100;
				if(remainder <= 90){
					if(remainder > 80)
						newDir = (newDir + 10) % 8;
				}else{
					newDir = (m_nDir + 6) % 8;
				}
			}
		}
	}

	if(newDir != m_nDir){
		uint8 outDir;
		UpdatePathNodes(ped, newDir, m_LastNode, m_NextNode, outDir);

		uint8 currentDir = m_nDir;
		uint8 theDir = outDir;
		if(outDir <= currentDir)
			theDir = m_nDir;

		if(theDir == currentDir)
			currentDir = outDir;

		if(((theDir - currentDir + 8) % 8) <= 3)
			m_nDir = newDir;
	}
}

// SA CTaskComplexWander::UpdatePathNodes (0x669ED0).
void
CTaskComplexWander::UpdatePathNodes(const CPed *ped, uint8 dir, CNodeAddress &originNode, CNodeAddress &targetNode, uint8 &outDir)
{
	originNode = targetNode;
	targetNode.ResetAreaId();
	const CVector &pos = ped->GetPosition();
	CPathNode *origin = NodeFromAddress(originNode);
	CPathNode *target = nil;
	ThePaths.FindNextNodeWandering(PATH_PED, pos, &origin, &target, dir, &outDir);
	originNode = AddressFromNode(origin);
	targetNode = AddressFromNode(target);
}

// SA CTaskComplexWander::CreateSubTask (0x671CB0). The car-drive,
// leave-car and road-crossing tasks are not ported yet and yield nil
// (which ends the wander, as an exhausted sequence would).
CTask*
CTaskComplexWander::CreateSubTask(CPed *ped, eTaskType taskType)
{
	switch(taskType){
	case TASK_SIMPLE_GO_TO_POINT: {
		CVector outTargetPos;
		ComputeTargetPos(ped, outTargetPos, m_NextNode);
		return new CTaskSimpleGoToPoint(m_nMoveState, outTargetPos, m_fTargetRadius, false, false);
	}
	case TASK_FINISHED:
		return nil;
	case TASK_SIMPLE_SCRATCH_HEAD:
		return new CTaskSimpleScratchHead();
	default:
		return nil;
	}
}

// SA CTaskComplexWander::ComputeTargetHeading (0x66F530).
float
CTaskComplexWander::ComputeTargetHeading(CPed *ped)
{
	CPathNode *nextNode = NodeFromAddress(m_NextNode);
	CVector position = nextNode ? CPathFind::TakeWidthIntoAccountForWandering(nextNode, ped->m_nRandomSeed) : ped->GetPosition();
	position -= ped->GetPosition();
	float radianAngle = CGeneral::GetRadianAngleBetweenPoints(position.x, position.y, 0.0f, 0.0f);
	return CGeneral::LimitRadianAngle(radianAngle);
}

// SA CTaskComplexWander::ComputeTargetPos (0x669F60).
void
CTaskComplexWander::ComputeTargetPos(const CPed *ped, CVector &outTargetPos, const CNodeAddress &targetNodeAddress)
{
	CPathNode *node = NodeFromAddress(targetNodeAddress);
	outTargetPos = node ? CPathFind::TakeWidthIntoAccountForWandering(node, ped->m_nRandomSeed) : ped->GetPosition();
	outTargetPos.z += 1.0f;
}

// SA CTaskComplexWander::ValidNodes (0x669F30).
bool
CTaskComplexWander::ValidNodes() const
{
	return m_NextNode.IsValid() && m_LastNode.IsValid() && m_NextNode != m_LastNode;
}

// SA CTaskComplexWander::ScanForBlockedNodes (0x674560).
void
CTaskComplexWander::ScanForBlockedNodes(CPed *ped)
{
	if(m_pSubTask->GetTaskType() == TASK_SIMPLE_GO_TO_POINT && m_NextNode.IsValid()){
		if(ScanForBlockedNode(ped, m_NextNode)){
			m_pSubTask->MakeAbortable(ped, ABORT_PRIORITY_LEISURE, nil);

			uint8 outDir = 0;
			UpdatePathNodes(ped, m_nDir, m_LastNode, m_NextNode, outDir);
			// SA compares the node addresses here (its own text compares
			// the member addresses, which can never be equal).
			if(ScanForBlockedNode(ped, m_NextNode) || m_NextNode == m_LastNode)
				m_bAllNodesBlocked = true;
		}
	}
}

// SA CTaskComplexWander::ScanForBlockedNode (0x671EF0).
bool
CTaskComplexWander::ScanForBlockedNode(CPed *ped, const CNodeAddress &targetNodeAddress)
{
	CPathNode *node = NodeFromAddress(targetNodeAddress);
	if(node == nil)
		return false;
	CVector outVec = CPathFind::TakeWidthIntoAccountForWandering(node, ped->m_nRandomSeed);
	CVector2D dist = outVec - ped->GetPosition();
	if(dist.MagnitudeSqr() <= 3.0f * 3.0f){
		if(ped->GetIntelligence() == nil)
			return false;
		CPed *closestPed = ped->GetIntelligence()->GetPedScanner().GetClosestPedInRange();
		if(ScanForBlockedNode(outVec, closestPed))
			return true;

		CVehicle *closestVehicle = ped->GetIntelligence()->GetVehicleScanner().GetClosestVehicleInRange();
		if(ScanForBlockedNode(outVec, closestVehicle))
			return true;
	}
	return false;
}

// SA CTaskComplexWander::ScanForBlockedNode (0x66F4C0).
bool
CTaskComplexWander::ScanForBlockedNode(const CVector &position, CEntity *entity)
{
	if(entity == nil)
		return false;

	CColModel *colModel = entity->GetColModel();
	if(colModel == nil)
		return false;
	float fRadius = colModel->boundingSphere.radius + 1.0f;
	CVector2D dist = entity->GetPosition() - position;
	if(dist.MagnitudeSqr() < fRadius * fRadius)
		return true;

	return false;
}

// SA CTaskComplexWander::GetWanderTaskByPedType (0x673D00). The cop,
// criminal and prostitute wander variants are not ported yet; their peds
// wander standard until they land.
CTaskComplexWander*
CTaskComplexWander::GetWanderTaskByPedType(CPed *ped)
{
	uint8 randomDir = (uint8)CGeneral::GetRandomNumberInRange(0, 8);
	switch(ped->m_nPedType){
	case PEDTYPE_MEDIC:
		return new CTaskComplexWanderMedic(PEDMOVE_WALK, randomDir);
	default:
		return new CTaskComplexWanderStandard(PEDMOVE_WALK, randomDir);
	}
}

// SA CTaskComplexWander::GetDistSqOfClosestPathNodeToPed (0x669FF0).
float
CTaskComplexWander::GetDistSqOfClosestPathNodeToPed(CPed *ped)
{
	float best = 999999.0f;
	const CNodeAddress nodes[2] = { m_NextNode, m_LastNode };
	for(int i = 0; i < 2; i++){
		CPathNode *node = NodeFromAddress(nodes[i]);
		if(node){
			CVector dist = ped->GetPosition() - node->GetPosition();
			float distSq = dist.MagnitudeSqr();
			if(distSq < best)
				best = distSq;
		}
	}
	return best;
}

// SA CTaskComplexWander::SetDirection (0x669D30).
void
CTaskComplexWander::SetDirection(uint8 dir)
{
	if(m_nDir != dir){
		m_nDir = dir;
		m_bNewDir = true;
	}
}

// SA CTaskComplexWander::SetPathNodesFrom (0x669D50).
void
CTaskComplexWander::SetPathNodesFrom(const CTaskComplexWander &other)
{
	if(m_LastNode != other.m_LastNode || m_NextNode != other.m_NextNode){
		m_LastNode = other.m_LastNode;
		m_NextNode = other.m_NextNode;
		m_nDir = other.m_nDir;
		m_bNewNodes = true;
	}
}
