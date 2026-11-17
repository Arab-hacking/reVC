#ifndef __GTA_CHANDLINGDATAMGR_H__
#define __GTA_CHANDLINGDATAMGR_H__

#include "tHandlingData.h"
#include "tBikeHandlingData.h"
#include "tBoatHandlingData.h"
#include "tFlyingHandlingData.h"
#include "tVehicleType.h"

// Owns every handling record the game has. One instance, mod_HandlingManager,
// is created by CGame::Initialise(); models store only a handling id
// (CVehicleModelInfo::m_handlingId) and resolve it here.
class cHandlingDataMgr
{
public:
	float field_0;
	float fWheelFriction;	// 0.9
	float field_8;
	float field_C;
	float field_10;

	tHandlingData HandlingData[NUM_HANDLINGS];
	tBikeHandlingData BikeHandlingData[NUM_BIKE_HANDLINGS];
	tFlyingHandlingData FlyingHandlingData[NUM_FLYING_HANDLINGS];
	tBoatHandlingData BoatHandlingData[NUM_BOAT_HANDLINGS];

	cHandlingDataMgr(void);

	void Initialise(void);
	void LoadHandlingData(void);
	int32 FindExactWord(const char *word, const char *names, uint32 entrySize, uint32 numEntries);
	void ConvertDataToGameUnits(tHandlingData *handling);
	void ConvertBikeDataToGameUnits(tBikeHandlingData *handling);
	int32 GetHandlingId(const char *name);
	bool HasRearWheelDrive(uint32 handlingId) { return GetVehiclePointer(handlingId)->m_Transmission.m_nDriveType != 'F'; }
	bool HasFrontWheelDrive(uint32 handlingId) { return GetVehiclePointer(handlingId)->m_Transmission.m_nDriveType != 'R'; }

	// Lookup by handling id. The id is a plain index into the name table, so
	// callers may hand over either their enum or a stored m_handlingId.
	tHandlingData *GetVehiclePointer(uint32 handlingId);
	tBikeHandlingData *GetBikeHandlingPointer(uint32 handlingId);
	tFlyingHandlingData *GetFlyingPointer(uint32 handlingId);
	tBoatHandlingData *GetBoatPointer(uint32 handlingId);
};

extern cHandlingDataMgr mod_HandlingManager;

#endif // __GTA_CHANDLINGDATAMGR_H__
