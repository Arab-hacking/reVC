#ifndef __GTA_EVEHICLETYPE_H__
#define __GTA_EVEHICLETYPE_H__

// Runtime class of a vehicle instance, and of the model it was created from.
// VEHICLE_TYPE_IGNORE is the "no vehicle type" value the engine checks against
// instead of keeping a separate validity flag.
enum eVehicleType : int32 {
	VEHICLE_TYPE_IGNORE     = -1,
	VEHICLE_TYPE_AUTOMOBILE = 0,
	VEHICLE_TYPE_MTRUCK,	// monster truck
	VEHICLE_TYPE_QUAD,
	VEHICLE_TYPE_HELI,
	VEHICLE_TYPE_PLANE,
	VEHICLE_TYPE_BOAT,
	VEHICLE_TYPE_FHELI,		// fake heli, no rotor simulation
	VEHICLE_TYPE_FPLANE,	// fake plane
	VEHICLE_TYPE_BIKE,
	VEHICLE_TYPE_BMX,
	VEHICLE_TYPE_TRAILER,

	NUM_VEHICLE_TYPES
};

#endif // __GTA_EVEHICLETYPE_H__
