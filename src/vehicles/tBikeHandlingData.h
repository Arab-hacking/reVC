#ifndef __GTA_TBIKEHANDLINGDATA_H__
#define __GTA_TBIKEHANDLINGDATA_H__

#include "tVehicleType.h"

// Bike handling line. Angles arrive in the file as sines/exponents and are
// converted by cHandlingDataMgr::ConvertBikeDataToGameUnits().
struct tBikeHandlingData
{
	int32 m_nVehicleId;
	float m_fLeanFwdCOM;
	float m_fLeanFwdForce;
	float m_fLeanBakCOM;
	float m_fLeanBakForce;
	float m_fMaxLean;
	float m_fFullAnimLean;
	float m_fDesLean;
	float m_fSpeedSteer;
	float m_fSlipSteer;
	float m_fNoPlayerCOMz;
	float m_fWheelieAng;
	float m_fStoppieAng;
	float m_fWheelieSteer;
	float m_fWheelieStabMult;
	float m_fStoppieStabMult;
};

#endif // __GTA_TBIKEHANDLINGDATA_H__
