#ifndef __GTA_EVEHICLETYPE_H__
#define __GTA_EVEHICLETYPE_H__

// Vehicle class of an entity and of a model. VEHICLE_TYPE_IGNORE is the "no
// vehicle" value the reference uses instead of a separate boolean.
enum eVehicleType : int32 {
	VEHICLE_TYPE_IGNORE = -1,
	VEHICLE_TYPE_AUTOMOBILE = 0,
	VEHICLE_TYPE_MTRUCK,	// monster truck
	VEHICLE_TYPE_QUAD,
	VEHICLE_TYPE_HELI,
	VEHICLE_TYPE_PLANE,
	VEHICLE_TYPE_BOAT,
	VEHICLE_TYPE_TRAIN,
	VEHICLE_TYPE_FHELI,		// fake heli, has no rotor simulation
	VEHICLE_TYPE_FPLANE,	// fake plane
	VEHICLE_TYPE_BIKE,
	VEHICLE_TYPE_BMX,
	VEHICLE_TYPE_TRAILER,
	NUM_VEHICLE_TYPES
};

#endif // __GTA_EVEHICLETYPE_H__
