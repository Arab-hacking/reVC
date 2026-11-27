#include "common.h"

#include "TaskSimpleTrainGetIn.h"

#include "Ped.h"
#include "PlayerPed.h"
#include "Train.h"
#include "Vehicle.h"

CTaskSimpleTrainGetIn::CTaskSimpleTrainGetIn(CVehicle *train)
	: m_pTrain(train)
{
	if (m_pTrain != nil)
		m_pTrain->RegisterReference((CEntity **)&m_pTrain);
}

CTaskSimpleTrainGetIn::CTaskSimpleTrainGetIn(const CTaskSimpleTrainGetIn &other)
	: CTaskSimpleTrainGetIn(other.m_pTrain)
{
}

CTaskSimpleTrainGetIn::~CTaskSimpleTrainGetIn()
{
	if (m_pTrain != nil)
		m_pTrain->CleanUpOldReference((CEntity **)&m_pTrain);
}

bool
CTaskSimpleTrainGetIn::ProcessPed(CPed *ped)
{
	CTrain *train = (CTrain *)m_pTrain;
	if (train == nil || ped->bInVehicle)
		return true;
	if (!train->m_aDoors[0].IsFullyOpen())
		return true;

	ped->m_pMyVehicle = train;
	train->RegisterReference((CEntity **)&ped->m_pMyVehicle);

	CVector enterPos;
	ped->GetNearestTrainPedPosition(train, enterPos);
	enterPos.z += 0.2f;

	ped->m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f);
	ped->m_fRotationCur = train->GetForward().Heading() - HALFPI;
	ped->m_fRotationDest = ped->m_fRotationCur;
	ped->SetPosition(enterPos);
	ped->SetHeading(ped->m_fRotationCur);
	ped->bUsesCollision = false;

	ped->bInVehicle = true;
	ped->SetPedState(PED_DRIVING);
	ped->SetMoveState(PEDMOVE_STILL);
	train->AddPassenger(ped);

	if (ped->IsPlayer() && ((CPlayerPed *)ped)->AdrenalineActive())
		((CPlayerPed *)ped)->ClearAdrenaline();

	return true;
}
