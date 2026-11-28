#include "common.h"

#include "TaskUtilityLineUpPedWithCar.h"

#include "AnimBlendAssociation.h"
#include "CarEnterExit.h"
#include "General.h"
#include "Ped.h"
#include "PedPlacement.h"
#include "Timer.h"
#include "Vehicle.h"
#include "maths.h"

// The positioning work below is the line-up calculation the ped code has
// always done while an entry/exit animation plays (place the ped at the
// door, keep him on the ground or on the seat height as the animation
// slides him in, and ease his heading onto the car's), driven here by
// the animation passed in instead of by ped state. The reference leaves
// these three functions unreversed; the behaviour is reconstructed from
// that same calculation.

CTaskUtilityLineUpPedWithCar::CTaskUtilityLineUpPedWithCar(const CVector &offset, int32 time, int32 doorOpenPosType, int32 doorIdx)
	: m_Offset(offset), m_fDoorOpenPosZ(-999.99f), m_fTime(time), m_nDoorOpenPosType(doorOpenPosType), m_nDoorIdx(doorIdx)
{
}

CVector
CTaskUtilityLineUpPedWithCar::GetLocalPositionToOpenCarDoor(CVehicle *vehicle, float animProgress, CAnimBlendAssociation *assoc)
{
	CVector world = GetPositionToOpenCarDoor(vehicle, animProgress, assoc);
	return Multiply3x3(world - vehicle->GetPosition(), vehicle->GetMatrix());
}

CVector
CTaskUtilityLineUpPedWithCar::GetPositionToOpenCarDoor(CVehicle *vehicle, float animProgress, CAnimBlendAssociation *assoc)
{
	(void)animProgress;
	(void)assoc;
	return CCarEnterExit::GetPositionToOpenCarDoor(vehicle, m_nDoorIdx);
}

bool
CTaskUtilityLineUpPedWithCar::ProcessPed(CPed *ped, CVehicle *vehicle, CAnimBlendAssociation *assoc)
{
	if (ped == nil || vehicle == nil)
		return false;

	// Time left to finish lining up.
	if (m_fTime > 0)
		m_fTime -= (int32)CTimer::GetTimeStepInMilliseconds();
	const float timeLeft = m_fTime > 0 ? m_fTime / 600.0f : 0.0f;

	const float progress = assoc != nil ? assoc->GetTimeProgress() : 0.0f;
	if (assoc == nil || progress == 0.0f)
		ped->m_vecMoveSpeed = CVector(0.0f, 0.0f, 0.0f);

	// Upside-down cars are entered any way the ped can manage.
	const bool vehIsUpsideDown = vehicle->GetMatrix().GetUp().z <= -0.8f;

	// Heading the ped ends up with: facing along the car, from the side
	// of the door he is using.
	const float vehHeading = vehicle->GetMatrix().GetForward().Heading();
	float rotationDest;
	if (m_nDoorIdx == CAR_DOOR_RF || m_nDoorIdx == CAR_DOOR_RR) {
		if (vehIsUpsideDown)
			rotationDest = -PI + vehHeading;
		else if (vehicle->bIsBus)
			rotationDest = 0.5f * PI + vehHeading;
		else
			rotationDest = vehHeading;
	} else if (m_nDoorIdx == CAR_DOOR_LF || m_nDoorIdx == CAR_DOOR_LR) {
		if (vehIsUpsideDown)
			rotationDest = vehHeading;
		else if (vehicle->bIsBus)
			rotationDest = -0.5f * PI + vehHeading;
		else
			rotationDest = vehHeading;
	} else {
		if (vehIsUpsideDown)
			rotationDest = vehHeading;
		else if (vehicle->bIsBus)
			rotationDest = 0.5f * PI + vehHeading;
		else if (m_nDoorIdx == CAR_WINDSCREEN)
			rotationDest = vehHeading + PI;
		else
			rotationDest = vehHeading;
	}

	// Where the ped has to be: the door-open position, slid towards the
	// seat as the animation progresses.
	CVector neededPos = CPed::GetPositionToOpenCarDoor(vehicle, (uint32)m_nDoorIdx, progress, ped);

	// ... but never below the ground under the door. The ground height
	// at the door is remembered: re-finding it every frame makes the ped
	// bob as the car moves.
	CVector groundPos = neededPos;
	if (vehicle->bIsInWater) {
		if (vehicle->IsBoat() && vehicle->IsUpsideDown())
			groundPos.z += 1.0f;
	} else {
		CPedPlacement::FindZCoorForPed(&groundPos);
	}
	if (m_fDoorOpenPosZ == -999.99f)
		m_fDoorOpenPosZ = groundPos.z;
	if (neededPos.z < m_fDoorOpenPosZ)
		neededPos.z = m_fDoorOpenPosZ;

	// Start displaced by the offset the ped had to the door when the
	// line-up was set up, and converge onto the door.
	neededPos += m_Offset * timeLeft;

	// Ease the heading onto the car's over the same time.
	float rotationCur = ped->GetHeading();
	{
		float limitedDest = CGeneral::LimitRadianAngle(rotationDest);
		if (PI + rotationCur < limitedDest)
			limitedDest -= 2.0f * PI;
		else if (rotationCur - PI > limitedDest)
			limitedDest += 2.0f * PI;
		rotationCur -= (rotationCur - limitedDest) * (1.0f - timeLeft);
	}

	if (progress > 0.2f || vehIsUpsideDown || vehicle->IsBike()) {
		ped->SetPosition(neededPos);
		ped->SetHeading(rotationCur);
	} else {
		// Early on, pin the ped's frame to the door frame so he moves
		// with the car exactly.
		CMatrix vehDoorMat(vehicle->GetMatrix());
		vehDoorMat.GetPosition() += Multiply3x3(vehDoorMat, CPed::GetLocalPositionToOpenCarDoor(vehicle, (uint32)m_nDoorIdx, 0.0f, ped));

		if (m_nDoorIdx == CAR_WINDSCREEN || vehicle->bIsBus) {
			CMatrix correctionMat;
			if (vehicle->bIsBus && (m_nDoorIdx == CAR_DOOR_LF || m_nDoorIdx == CAR_DOOR_LR))
				correctionMat.SetRotateZ(-HALFPI);
			else if (vehicle->bIsBus && (m_nDoorIdx == CAR_DOOR_RF || m_nDoorIdx == CAR_DOOR_RR))
				correctionMat.SetRotateZ(HALFPI);
			else
				correctionMat.SetRotateZ(PI);
			vehDoorMat = vehDoorMat * correctionMat;
		}
		ped->GetMatrix() = vehDoorMat;
	}

	return m_fTime <= 0;
}
