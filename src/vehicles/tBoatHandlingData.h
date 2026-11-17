#ifndef __GTA_TBOATHANDLINGDATA_H__
#define __GTA_TBOATHANDLINGDATA_H__

#include "tVehicleType.h"

// Boat handling line, keyed to HANDLING_PREDATOR..HANDLING_SEAPLANE.
struct tBoatHandlingData
{
	int32 m_nVehicleId;
	float m_fThrustY;
	float m_fThrustZ;
	float m_fThrustAppZ;
	float m_fAqPlaneForce;
	float m_fAqPlaneLimit;
	float m_fAqPlaneOffset;
	float m_fWaveAudioMult;
	float m_fLook_L_R_BehindCamHeight;
	CVector m_vecMoveRes;
	CVector m_vecTurnRes;
};

#endif // __GTA_TBOATHANDLINGDATA_H__
