#include "common.h"

#include "TaskSimpleTrainGetOut.h"

#include "AnimBlendHierarchy.h"
#include "AnimBlendSequence.h"
#include "AnimBlendStaticAssociation.h"
#include "AnimManager.h"
#include "AnimationEnums.h"
#include "eBoneTag.h"
#include "Ped.h"
#include "PedPlacement.h"
#include "Train.h"
#include "Vehicle.h"

CTaskSimpleTrainGetOut::CTaskSimpleTrainGetOut(CVehicle *train)
	: m_pTrain(train)
{
	if (m_pTrain != nil)
		m_pTrain->RegisterReference((CEntity **)&m_pTrain);
}

CTaskSimpleTrainGetOut::CTaskSimpleTrainGetOut(const CTaskSimpleTrainGetOut &other)
	: CTaskSimpleTrainGetOut(other.m_pTrain)
{
}

CTaskSimpleTrainGetOut::~CTaskSimpleTrainGetOut()
{
	if (m_pTrain != nil)
		m_pTrain->CleanUpOldReference((CEntity **)&m_pTrain);
}

// The step-out offset is the root translation of the train idle clip,
// read out of the animation data the way the old callback did it.
static CVector
GetTrainDoorAnimOffset(void)
{
	CAnimBlendStaticAssociation *sa = CAnimManager::GetAnimAssociation(ANIM_GROUP_TRAINCARRANIMS, ANIM_ID_IDLE);
	CAnimBlendHierarchy *hier = sa ? sa->m_BlendHier : nil;
	if (hier == nil || hier->m_pSequences == nil || hier->m_nSeqCount <= 0)
		return CVector(0.0f, 0.0f, 0.0f);
	CAnimManager::UncompressAnimation(hier);
	CAnimBlendSequence *root = nil;
	for (int i = 0; i < hier->m_nSeqCount; i++) {
		CAnimBlendSequence *seq = &hier->m_pSequences[i];
		if (seq->IsUsingBoneTag() && seq->GetBoneTag() == BONE_ROOT) {
			root = seq;
			break;
		}
	}
	if (root == nil)
		root = hier->m_pSequences;
	if (root->m_FramesNum <= 0 || !root->HasTranslation())
		return CVector(0.0f, 0.0f, 0.0f);
	return root->GetUKeyFrame(root->m_FramesNum - 1)->Trans;
}

bool
CTaskSimpleTrainGetOut::ProcessPed(CPed *ped)
{
	CTrain *train = (CTrain *)m_pTrain;
	if (train == nil)
		return true;
	if (train->GetStatus() != STATUS_TRAIN_NOT_MOVING || !train->m_aDoors[0].IsFullyOpen())
		return false; // wait for the train to stop and open up

	CMatrix pedMat(ped->GetMatrix());
	CVector posAfterExit = Multiply3x3(pedMat, GetTrainDoorAnimOffset());
	posAfterExit += ped->GetPosition();
	CPedPlacement::FindZCoorForPed(&posAfterExit);

	ped->bUsesCollision = true;
	ped->bInVehicle = false;
	ped->SetPedState(PED_IDLE);
	ped->SetMoveState(PEDMOVE_STILL);
	ped->m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f);
	ped->m_fRotationCur = HALFPI + train->GetForward().Heading();
	ped->m_fRotationDest = ped->m_fRotationCur;
	ped->SetPosition(posAfterExit);
	ped->SetHeading(ped->m_fRotationCur);
	train->RemovePassenger(ped);
	if (ped->m_pMyVehicle == train) {
		train->CleanUpOldReference((CEntity **)&ped->m_pMyVehicle);
		ped->m_pMyVehicle = nil;
	}
	return true;
}
