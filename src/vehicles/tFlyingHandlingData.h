#ifndef __GTA_TFLYINGHANDLINGDATA_H__
#define __GTA_TFLYINGHANDLINGDATA_H__

#include "tVehicleType.h"

// Flying handling line, keyed to HANDLING_SEAPLANE..HANDLING_RCRAIDER.
struct tFlyingHandlingData
{
	int32 m_nVehicleId;
	float m_fThrust;
	float m_fThrustFallOff;
	float m_fYaw;
	float m_fYawStab;
	float m_fSideSlip;
	float m_fRoll;
	float m_fRollStab;
	float m_fPitch;
	float m_fPitchStab;
	float m_fFormLift;
	float m_fAttackLift;
	float m_fGearUpR;
	float m_fGearDownL;
	float m_fWindMult;
	float m_fMoveRes;
	CVector m_vecTurnRes;
	CVector m_vecSpeedRes;
};

#endif // __GTA_TFLYINGHANDLINGDATA_H__
