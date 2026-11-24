#include "common.h"

#include "CarCtrl.h"

#include "Automobile.h"
#include "Bike.h"
#include "Bmx.h"
#include "Boat.h"
#include "MonsterTruck.h"
#include "QuadBike.h"
#include "Trailer.h"
#include "Vehicle.h"
#include "ModelInfo.h"
#include "VehicleModelInfo.h"

// The class of a model decides what gets constructed. Classes the engine does
// not implement are built as their nearest implemented class so that a data
// set naming them still spawns a working vehicle: a helicopter or plane is an
// automobile whose handling line carries the rotor or wing flags.
CVehicle *CCarCtrl::GetNewVehicleDependingOnCarModel(int32 modelId, uint8 createdBy)
{
	CVehicleModelInfo *mi = (CVehicleModelInfo*)CModelInfo::GetModelInfo(modelId);

	switch(mi->m_nVehicleType){
	case VEHICLE_TYPE_BOAT:
		return new CBoat(modelId, createdBy);
	case VEHICLE_TYPE_BIKE:
		return new CBike(modelId, createdBy);
	case VEHICLE_TYPE_BMX:
		return new CBmx(modelId, createdBy);
	case VEHICLE_TYPE_QUAD:
		return new CQuadBike(modelId, createdBy);
	case VEHICLE_TYPE_MTRUCK:
		return new CMonsterTruck(modelId, createdBy);
	case VEHICLE_TYPE_TRAILER:
		return new CTrailer(modelId, createdBy);
	case VEHICLE_TYPE_HELI:
	case VEHICLE_TYPE_PLANE:
	case VEHICLE_TYPE_FPLANE:
	case VEHICLE_TYPE_FHELI:
	case VEHICLE_TYPE_AUTOMOBILE:
		return new CAutomobile(modelId, createdBy);
	default:
		return nil;
	}
}
