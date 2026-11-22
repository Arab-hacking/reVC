#include "SavehDiag.h"
#include "common.h"
#include <ctype.h>
#include <rpmatfx.h>

#include "RwHelper.h"
#include "General.h"
#include "NodeName.h"
#include "TxdStore.h"
#include "Weather.h"
#include "cHandlingDataMgr.h"
#include "VisibilityPlugins.h"
#include "FileMgr.h"
#include "FileLoader.h"
#include "World.h"
#include "Vehicle.h"
#include "Automobile.h"
#include "Boat.h"
#include "Train.h"
#include "Plane.h"
#include "Heli.h"
#include "Bike.h"
#include "ModelIndices.h"
#include "ModelInfo.h"
#include "CustomCarPlateMgr.h"
#include "custompipes.h"
#include "VehicleMaterialClassify.h"
#ifdef CUSTOM_MODELS
#include "CustomModels.h"
#include "CommonCarcols.h"
#endif

int8 CVehicleModelInfo::ms_compsToUse[2] = { -2, -2 };
int8 CVehicleModelInfo::ms_compsUsed[2];
RwRGBA CVehicleModelInfo::ms_vehicleColourTable[256];
RwTexture *CVehicleModelInfo::ms_colourTextureTable[256];

// SA per-model tuning state. The lists are shared between models the way SA
// shares them: a model's own table lives in its model info, but the wheel sets
// and the link list describe the whole catalogue and are filled once from
// carmods.dat.
CVehicleModelInfo::CLinkedUpgradeList CVehicleModelInfo::ms_linkedUpgrades;
uint8 CVehicleModelInfo::ms_currentCol[NUM_CURRENT_COLORS] = { 0, 0, 0, 0 };
RwTexture *CVehicleModelInfo::ms_pRemapTexture;
RwTexture *CVehicleModelInfo::ms_pLightsTexture;
RwTexture *CVehicleModelInfo::ms_pLightsOnTexture;
uint8 CVehicleModelInfo::ms_lightsOn[NUM_LIGHTS] = { 0, 0, 0, 0 };
int16 CVehicleModelInfo::ms_numWheelUpgrades[NUM_WHEELS];
int16 CVehicleModelInfo::ms_upgradeWheels[NUM_WHEELS][NUM_WHEEL_UPGRADES];
int32 CVehicleModelInfo::ms_wheelFrameIDs[NUM_WHEELS] = {
	CAR_WHEEL_RF, CAR_WHEEL_RM, CAR_WHEEL_RB, CAR_WHEEL_LF
};

RwTexture *gpWhiteTexture;
static RwTexture *gpFallbackEnvTexture;
RwFrame *pMatFxIdentityFrame;

// Whether the model being processed uses the GTA:San Andreas node naming
// convention (see "GTA:San Andreas vehicle support" below) used to live in a
// file static that SetClump wrote and every renderer callback read. It is now
// an argument of the callbacks (CVehicleModelInfo::AtomicRendererCBData), so
// the answer always belongs to the model being walked.

// Pulls the caller's payload out of the RpClumpForAllAtomics data pointer.
// Callers that have nothing to say pass nil.
static inline bool
IsSAHierarchyFromCBData(void *data)
{
	CVehicleModelInfo::AtomicRendererCBData *cbdata = (CVehicleModelInfo::AtomicRendererCBData*)data;
	return cbdata != nil && cbdata->saHierarchy;
}

enum {
	VEHICLE_FLAG_COLLAPSE	= 0x2,
	VEHICLE_FLAG_ADD_WHEEL	= 0x4,
	VEHICLE_FLAG_POS	= 0x8,
	VEHICLE_FLAG_DOOR	= 0x10,
	VEHICLE_FLAG_LEFT	= 0x20,
	VEHICLE_FLAG_RIGHT	= 0x40,
	VEHICLE_FLAG_FRONT	= 0x80,
	VEHICLE_FLAG_REAR	= 0x100,
	VEHICLE_FLAG_COMP	= 0x200,
	VEHICLE_FLAG_DRAWLAST	= 0x400,
	VEHICLE_FLAG_WINDSCREEN	= 0x800,
	// SA dummy position, stored in the upper bits of flags as
	// (eVehicleDummy + 1) << VEHICLE_FLAG_DUMMY_SHIFT. 0 means "not a dummy".
	VEHICLE_FLAG_DUMMY	= 0x1000,
	VEHICLE_FLAG_DUMMY_SHIFT = 16,
	VEHICLE_FLAG_ANGLECULL	= 0x1000,
	VEHICLE_FLAG_REARDOOR	= 0x2000,
	VEHICLE_FLAG_FRONTDOOR	= 0x4000,
};

RwObjectNameIdAssocation carIds[] = {
	// SA CVehicleModelInfo name table, in eCarNodes order. The order matters:
	// SetFrameIds walks this table and assigns hierarchy ids in sequence, so the
	// nth entry lands on frame id n. That is why CAR_CHASSIS sits at index 1 and
	// every component follows it by one.
	{ "chassis_dummy",	CAR_CHASSIS,	VEHICLE_FLAG_COLLAPSE },
	{ "wheel_rf_dummy",	CAR_WHEEL_RF,	VEHICLE_FLAG_RIGHT | VEHICLE_FLAG_ADD_WHEEL },
	{ "wheel_rm_dummy",	CAR_WHEEL_RM,	VEHICLE_FLAG_RIGHT | VEHICLE_FLAG_ADD_WHEEL },
	{ "wheel_rb_dummy",	CAR_WHEEL_RB,	VEHICLE_FLAG_RIGHT | VEHICLE_FLAG_ADD_WHEEL },
	{ "wheel_lf_dummy",	CAR_WHEEL_LF,	VEHICLE_FLAG_LEFT | VEHICLE_FLAG_ADD_WHEEL },
	{ "wheel_lm_dummy",	CAR_WHEEL_LM,	VEHICLE_FLAG_LEFT | VEHICLE_FLAG_ADD_WHEEL },
	{ "wheel_lb_dummy",	CAR_WHEEL_LB,	VEHICLE_FLAG_LEFT | VEHICLE_FLAG_ADD_WHEEL },
	{ "door_rf_dummy",	CAR_DOOR_RF,	VEHICLE_FLAG_FRONTDOOR | VEHICLE_FLAG_ANGLECULL | VEHICLE_FLAG_RIGHT | VEHICLE_FLAG_DOOR | VEHICLE_FLAG_COLLAPSE },
	{ "door_rr_dummy",	CAR_DOOR_RR,	VEHICLE_FLAG_REARDOOR | VEHICLE_FLAG_ANGLECULL | VEHICLE_FLAG_REAR | VEHICLE_FLAG_RIGHT | VEHICLE_FLAG_DOOR | VEHICLE_FLAG_COLLAPSE },
	{ "door_lf_dummy",	CAR_DOOR_LF,	VEHICLE_FLAG_FRONTDOOR | VEHICLE_FLAG_ANGLECULL | VEHICLE_FLAG_LEFT | VEHICLE_FLAG_DOOR | VEHICLE_FLAG_COLLAPSE },
	{ "door_lr_dummy",	CAR_DOOR_LR,	VEHICLE_FLAG_REARDOOR | VEHICLE_FLAG_ANGLECULL | VEHICLE_FLAG_REAR | VEHICLE_FLAG_LEFT | VEHICLE_FLAG_DOOR | VEHICLE_FLAG_COLLAPSE },
	{ "bump_front_dummy",	CAR_BUMP_FRONT,	VEHICLE_FLAG_FRONT | VEHICLE_FLAG_COLLAPSE },
	{ "bump_rear_dummy",	CAR_BUMP_REAR,	VEHICLE_FLAG_REAR | VEHICLE_FLAG_COLLAPSE },
	{ "wing_rf_dummy",	CAR_WING_RF,	VEHICLE_FLAG_COLLAPSE },
	{ "wing_lf_dummy",	CAR_WING_LF,	VEHICLE_FLAG_COLLAPSE },
	{ "bonnet_dummy",	CAR_BONNET,	VEHICLE_FLAG_COLLAPSE },
	{ "boot_dummy",		CAR_BOOT,	VEHICLE_FLAG_REAR | VEHICLE_FLAG_COLLAPSE },
	{ "windscreen_dummy",	CAR_WINDSCREEN,	VEHICLE_FLAG_WINDSCREEN | VEHICLE_FLAG_DRAWLAST | VEHICLE_FLAG_FRONT | VEHICLE_FLAG_COLLAPSE },
	{ "exhaust_ok",		CAR_EXHAUST,	VEHICLE_FLAG_POS | CLUMP_FLAG_NO_HIERID },
	// The five misc slots exist as nodes in SA (CAR_MISC_A..E). They carry
	// aftermarket parts, so they are components rather than positions.
	{ "misc_a",		CAR_MISC_A,	VEHICLE_FLAG_DRAWLAST | VEHICLE_FLAG_COMP | CLUMP_FLAG_NO_HIERID },
	{ "misc_b",		CAR_MISC_B,	VEHICLE_FLAG_DRAWLAST | VEHICLE_FLAG_COMP | CLUMP_FLAG_NO_HIERID },
	{ "misc_c",		CAR_MISC_C,	VEHICLE_FLAG_DRAWLAST | VEHICLE_FLAG_COMP | CLUMP_FLAG_NO_HIERID },
	{ "misc_d",		CAR_MISC_D,	VEHICLE_FLAG_DRAWLAST | VEHICLE_FLAG_COMP | CLUMP_FLAG_NO_HIERID },
	{ "misc_e",		CAR_MISC_E,	VEHICLE_FLAG_DRAWLAST | VEHICLE_FLAG_COMP | CLUMP_FLAG_NO_HIERID },

	// Dummy positions the light, seat and exhaust code reads. These are not
	// nodes; they live in m_positions/m_avDummyPos.
	{ "ped_frontseat",	CAR_POS_FRONTSEAT,	VEHICLE_FLAG_POS | CLUMP_FLAG_NO_HIERID },
	{ "ped_backseat",	CAR_POS_BACKSEAT,	VEHICLE_FLAG_POS | CLUMP_FLAG_NO_HIERID },
	{ "headlights",		CAR_POS_HEADLIGHTS,	VEHICLE_FLAG_POS | CLUMP_FLAG_NO_HIERID | VEHICLE_FLAG_DUMMY | ((DUMMY_LIGHT_FRONT_MAIN + 1) << VEHICLE_FLAG_DUMMY_SHIFT) },
	{ "taillights",		CAR_POS_TAILLIGHTS,	VEHICLE_FLAG_POS | CLUMP_FLAG_NO_HIERID | VEHICLE_FLAG_DUMMY | ((DUMMY_LIGHT_REAR_MAIN + 1) << VEHICLE_FLAG_DUMMY_SHIFT) },
	{ "headlights2",	0,	VEHICLE_FLAG_DUMMY | ((DUMMY_LIGHT_FRONT_SECONDARY + 1) << VEHICLE_FLAG_DUMMY_SHIFT) },
	{ "taillights2",		0,	VEHICLE_FLAG_DUMMY | ((DUMMY_LIGHT_REAR_SECONDARY + 1) << VEHICLE_FLAG_DUMMY_SHIFT) },
	{ "exhaust",		CAR_POS_EXHAUST,	VEHICLE_FLAG_POS | CLUMP_FLAG_NO_HIERID },

	// SA also ships vehicles whose frames are named without the "_dummy"
	// suffix. They are accepted only if the canonical name above was not
	// found, because SetFrameIds ignores frames that already carry an id.
	{ "wheel_rf",		CAR_WHEEL_RF,	VEHICLE_FLAG_RIGHT | VEHICLE_FLAG_ADD_WHEEL },
	{ "wheel_rm",		CAR_WHEEL_RM,	VEHICLE_FLAG_RIGHT | VEHICLE_FLAG_ADD_WHEEL },
	{ "wheel_rb",		CAR_WHEEL_RB,	VEHICLE_FLAG_RIGHT | VEHICLE_FLAG_ADD_WHEEL },
	{ "wheel_lf",		CAR_WHEEL_LF,	VEHICLE_FLAG_LEFT | VEHICLE_FLAG_ADD_WHEEL },
	{ "wheel_lm",		CAR_WHEEL_LM,	VEHICLE_FLAG_LEFT | VEHICLE_FLAG_ADD_WHEEL },
	{ "wheel_lb",		CAR_WHEEL_LB,	VEHICLE_FLAG_LEFT | VEHICLE_FLAG_ADD_WHEEL },
	{ "door_rf",		CAR_DOOR_RF,	VEHICLE_FLAG_FRONTDOOR | VEHICLE_FLAG_ANGLECULL | VEHICLE_FLAG_RIGHT | VEHICLE_FLAG_DOOR | VEHICLE_FLAG_COLLAPSE },
	{ "door_rr",		CAR_DOOR_RR,	VEHICLE_FLAG_REARDOOR | VEHICLE_FLAG_ANGLECULL | VEHICLE_FLAG_REAR | VEHICLE_FLAG_RIGHT | VEHICLE_FLAG_DOOR | VEHICLE_FLAG_COLLAPSE },
	{ "door_lf",		CAR_DOOR_LF,	VEHICLE_FLAG_FRONTDOOR | VEHICLE_FLAG_ANGLECULL | VEHICLE_FLAG_LEFT | VEHICLE_FLAG_DOOR | VEHICLE_FLAG_COLLAPSE },
	{ "door_lr",		CAR_DOOR_LR,	VEHICLE_FLAG_REARDOOR | VEHICLE_FLAG_ANGLECULL | VEHICLE_FLAG_REAR | VEHICLE_FLAG_LEFT | VEHICLE_FLAG_DOOR | VEHICLE_FLAG_COLLAPSE },
	{ "bump_front",		CAR_BUMP_FRONT,	VEHICLE_FLAG_FRONT | VEHICLE_FLAG_COLLAPSE },
	{ "bump_rear",		CAR_BUMP_REAR,	VEHICLE_FLAG_REAR | VEHICLE_FLAG_COLLAPSE },
	{ "bonnet",		CAR_BONNET,	VEHICLE_FLAG_COLLAPSE },
	{ "boot",		CAR_BOOT,	VEHICLE_FLAG_REAR | VEHICLE_FLAG_COLLAPSE },
	{ "windscreen",		CAR_WINDSCREEN,	VEHICLE_FLAG_WINDSCREEN | VEHICLE_FLAG_DRAWLAST | VEHICLE_FLAG_FRONT | VEHICLE_FLAG_COLLAPSE },

	// Generic leftovers. VC called these extra1..extra6; SA numbers them misc.
	{ "extra1",		0, VEHICLE_FLAG_DRAWLAST | VEHICLE_FLAG_COMP | CLUMP_FLAG_NO_HIERID },
	{ "extra2",		0, VEHICLE_FLAG_DRAWLAST | VEHICLE_FLAG_COMP | CLUMP_FLAG_NO_HIERID },
	{ "extra3",		0, VEHICLE_FLAG_DRAWLAST | VEHICLE_FLAG_COMP | CLUMP_FLAG_NO_HIERID },
	{ "extra4",		0, VEHICLE_FLAG_DRAWLAST | VEHICLE_FLAG_COMP | CLUMP_FLAG_NO_HIERID },
	{ "extra5",		0, VEHICLE_FLAG_DRAWLAST | VEHICLE_FLAG_COMP | CLUMP_FLAG_NO_HIERID },
	{ "extra6",		0, VEHICLE_FLAG_DRAWLAST | VEHICLE_FLAG_COMP | CLUMP_FLAG_NO_HIERID },
	{ nil, 0, 0 }
};

RwObjectNameIdAssocation boatIds[] = {
	{ "boat_moving_hi",	BOAT_MOVING,	0 },
	{ "boat_rudder_hi",	BOAT_RUDDER,	0 },
	{ "boat_flap_left",	BOAT_FLAP_LEFT,	0 },
	{ "boat_flap_right",	BOAT_FLAP_RIGHT,	0 },
	{ "boat_rearflap_left",	BOAT_REARFLAP_LEFT,	0 },
	{ "boat_rearflap_right",	BOAT_REARFLAP_RIGHT,	0 },
#ifdef FIX_BUGS
	// let's just accept both
	{ "windscreen",		BOAT_WINDSCREEN,	VEHICLE_FLAG_WINDSCREEN | VEHICLE_FLAG_DRAWLAST },
	{ "windscreen_hi_ok",		BOAT_WINDSCREEN,	VEHICLE_FLAG_WINDSCREEN | VEHICLE_FLAG_DRAWLAST },
#else
#ifdef GTA_PS2
	{ "windscreen",		BOAT_WINDSCREEN,	VEHICLE_FLAG_WINDSCREEN | VEHICLE_FLAG_DRAWLAST },
#else
	{ "windscreen_hi_ok",		BOAT_WINDSCREEN,	VEHICLE_FLAG_WINDSCREEN | VEHICLE_FLAG_DRAWLAST },
#endif
#endif
	{ "ped_frontseat",	BOAT_POS_FRONTSEAT,	VEHICLE_FLAG_POS | CLUMP_FLAG_NO_HIERID },
	{ nil, 0, 0 }
};

RwObjectNameIdAssocation trainIds[] = {
	// SA train frames. Two doors plus twelve wheel nodes and two bogies, in
	// eTrainNodes order.
	{ "door_lf_dummy",	TRAIN_DOOR_LF,	VEHICLE_FLAG_LEFT | VEHICLE_FLAG_COLLAPSE },
	{ "door_rf_dummy",	TRAIN_DOOR_RF,	VEHICLE_FLAG_RIGHT | VEHICLE_FLAG_COLLAPSE },
	{ "wheel_rf1_dummy",	TRAIN_WHEEL_RF1,	VEHICLE_FLAG_RIGHT },
	{ "wheel_rf2_dummy",	TRAIN_WHEEL_RF2,	VEHICLE_FLAG_RIGHT },
	{ "wheel_rf3_dummy",	TRAIN_WHEEL_RF3,	VEHICLE_FLAG_RIGHT },
	{ "wheel_rb1_dummy",	TRAIN_WHEEL_RB1,	VEHICLE_FLAG_RIGHT },
	{ "wheel_rb2_dummy",	TRAIN_WHEEL_RB2,	VEHICLE_FLAG_RIGHT },
	{ "wheel_rb3_dummy",	TRAIN_WHEEL_RB3,	VEHICLE_FLAG_RIGHT },
	{ "wheel_lf1_dummy",	TRAIN_WHEEL_LF1,	VEHICLE_FLAG_LEFT },
	{ "wheel_lf2_dummy",	TRAIN_WHEEL_LF2,	VEHICLE_FLAG_LEFT },
	{ "wheel_lf3_dummy",	TRAIN_WHEEL_LF3,	VEHICLE_FLAG_LEFT },
	{ "wheel_lb1_dummy",	TRAIN_WHEEL_LB1,	VEHICLE_FLAG_LEFT },
	{ "wheel_lb2_dummy",	TRAIN_WHEEL_LB2,	VEHICLE_FLAG_LEFT },
	{ "wheel_lb3_dummy",	TRAIN_WHEEL_LB3,	VEHICLE_FLAG_LEFT },
	{ "bogie_front",	TRAIN_BOGIE_FRONT,	0 },
	{ "bogie_rear",		TRAIN_BOGIE_REAR,	0 },

	// Dummy positions.
	{ "light_front",	TRAIN_POS_LIGHT_FRONT,	VEHICLE_FLAG_POS | CLUMP_FLAG_NO_HIERID },
	{ "light_rear",		TRAIN_POS_LIGHT_REAR,	VEHICLE_FLAG_POS | CLUMP_FLAG_NO_HIERID },
	{ "ped_left_entry",	TRAIN_POS_LEFT_ENTRY,	VEHICLE_FLAG_DOOR | VEHICLE_FLAG_POS | CLUMP_FLAG_NO_HIERID },
	{ "ped_mid_entry",	TRAIN_POS_MID_ENTRY,	VEHICLE_FLAG_DOOR | VEHICLE_FLAG_POS | CLUMP_FLAG_NO_HIERID },
	{ "ped_right_entry",	TRAIN_POS_RIGHT_ENTRY,	VEHICLE_FLAG_DOOR | VEHICLE_FLAG_POS | CLUMP_FLAG_NO_HIERID },
	{ nil, 0, 0 }
};

RwObjectNameIdAssocation heliIds[] = {
	// SA heli frames, in eHeliNodes order: chassis, the full wheel and door set,
	// two rotor pairs, rudder and elevators, then four misc slots.
	{ "chassis_dummy",	HELI_CHASSIS,	VEHICLE_FLAG_COLLAPSE },
	{ "wheel_rf_dummy",	HELI_WHEEL_RF,	VEHICLE_FLAG_RIGHT },
	{ "wheel_rm_dummy",	HELI_WHEEL_RM,	VEHICLE_FLAG_RIGHT },
	{ "wheel_rb_dummy",	HELI_WHEEL_RB,	VEHICLE_FLAG_RIGHT },
	{ "wheel_lf_dummy",	HELI_WHEEL_LF,	VEHICLE_FLAG_LEFT },
	{ "wheel_lm_dummy",	HELI_WHEEL_LM,	VEHICLE_FLAG_LEFT },
	{ "wheel_lb_dummy",	HELI_WHEEL_LB,	VEHICLE_FLAG_LEFT },
	{ "door_rf_dummy",	HELI_DOOR_RF,	VEHICLE_FLAG_FRONTDOOR | VEHICLE_FLAG_RIGHT | VEHICLE_FLAG_DOOR | VEHICLE_FLAG_COLLAPSE },
	{ "door_rr_dummy",	HELI_DOOR_RR,	VEHICLE_FLAG_REARDOOR | VEHICLE_FLAG_REAR | VEHICLE_FLAG_RIGHT | VEHICLE_FLAG_DOOR | VEHICLE_FLAG_COLLAPSE },
	{ "door_lf_dummy",	HELI_DOOR_LF,	VEHICLE_FLAG_FRONTDOOR | VEHICLE_FLAG_LEFT | VEHICLE_FLAG_DOOR | VEHICLE_FLAG_COLLAPSE },
	{ "door_lr_dummy",	HELI_DOOR_LR,	VEHICLE_FLAG_REARDOOR | VEHICLE_FLAG_REAR | VEHICLE_FLAG_LEFT | VEHICLE_FLAG_DOOR | VEHICLE_FLAG_COLLAPSE },
	{ "static_rotor",	HELI_STATIC_ROTOR,	0 },
	{ "moving_rotor",	HELI_MOVING_ROTOR,	0 },
	{ "static_rotor2",	HELI_STATIC_ROTOR2,	0 },
	{ "moving_rotor2",	HELI_MOVING_ROTOR2,	0 },
	{ "rudder",		HELI_RUDDER,	0 },
	{ "elevators",		HELI_ELEVATORS,	0 },
	{ "misc_a",		HELI_MISC_A,	VEHICLE_FLAG_COMP | CLUMP_FLAG_NO_HIERID },
	{ "misc_b",		HELI_MISC_B,	VEHICLE_FLAG_COMP | CLUMP_FLAG_NO_HIERID },
	{ "misc_c",		HELI_MISC_C,	VEHICLE_FLAG_COMP | CLUMP_FLAG_NO_HIERID },
	{ "misc_d",		HELI_MISC_D,	VEHICLE_FLAG_COMP | CLUMP_FLAG_NO_HIERID },

	// Dummy positions the rotor/light code reads.
	{ "ped_frontseat",	CAR_POS_FRONTSEAT,	VEHICLE_FLAG_POS | CLUMP_FLAG_NO_HIERID },
	{ "ped_backseat",	CAR_POS_BACKSEAT,	VEHICLE_FLAG_POS | CLUMP_FLAG_NO_HIERID },
	{ "headlights",		CAR_POS_HEADLIGHTS,	VEHICLE_FLAG_POS | CLUMP_FLAG_NO_HIERID | VEHICLE_FLAG_DUMMY | ((DUMMY_LIGHT_FRONT_MAIN + 1) << VEHICLE_FLAG_DUMMY_SHIFT) },
	{ "taillights",		CAR_POS_TAILLIGHTS,	VEHICLE_FLAG_POS | CLUMP_FLAG_NO_HIERID | VEHICLE_FLAG_DUMMY | ((DUMMY_LIGHT_REAR_MAIN + 1) << VEHICLE_FLAG_DUMMY_SHIFT) },
	{ "exhaust",		CAR_POS_EXHAUST,	VEHICLE_FLAG_POS | CLUMP_FLAG_NO_HIERID },
	{ nil, 0, 0 }
};

RwObjectNameIdAssocation planeIds[] = {
	// SA plane frames, in ePlaneNodes order.
	{ "chassis_dummy",	PLANE_CHASSIS,	VEHICLE_FLAG_COLLAPSE },
	{ "wheel_rf_dummy",	PLANE_WHEEL_RF,	VEHICLE_FLAG_RIGHT },
	{ "wheel_rm_dummy",	PLANE_WHEEL_RM,	VEHICLE_FLAG_RIGHT },
	{ "wheel_rb_dummy",	PLANE_WHEEL_RB,	VEHICLE_FLAG_RIGHT },
	{ "wheel_lf_dummy",	PLANE_WHEEL_LF,	VEHICLE_FLAG_LEFT },
	{ "wheel_lm_dummy",	PLANE_WHEEL_LM,	VEHICLE_FLAG_LEFT },
	{ "wheel_lb_dummy",	PLANE_WHEEL_LB,	VEHICLE_FLAG_LEFT },
	{ "door_rf_dummy",	PLANE_DOOR_RF,	VEHICLE_FLAG_FRONTDOOR | VEHICLE_FLAG_RIGHT | VEHICLE_FLAG_DOOR | VEHICLE_FLAG_COLLAPSE },
	{ "door_rr_dummy",	PLANE_DOOR_RR,	VEHICLE_FLAG_REARDOOR | VEHICLE_FLAG_REAR | VEHICLE_FLAG_RIGHT | VEHICLE_FLAG_DOOR | VEHICLE_FLAG_COLLAPSE },
	{ "door_lf_dummy",	PLANE_DOOR_LF,	VEHICLE_FLAG_FRONTDOOR | VEHICLE_FLAG_LEFT | VEHICLE_FLAG_DOOR | VEHICLE_FLAG_COLLAPSE },
	{ "door_lr_dummy",	PLANE_DOOR_LR,	VEHICLE_FLAG_REARDOOR | VEHICLE_FLAG_REAR | VEHICLE_FLAG_LEFT | VEHICLE_FLAG_DOOR | VEHICLE_FLAG_COLLAPSE },
	{ "static_prop",	PLANE_STATIC_PROP,	0 },
	{ "moving_prop",	PLANE_MOVING_PROP,	0 },
	{ "static_prop2",	PLANE_STATIC_PROP2,	0 },
	{ "moving_prop2",	PLANE_MOVING_PROP2,	0 },
	{ "rudder",		PLANE_RUDDER,	0 },
	{ "elevator_l",		PLANE_ELEVATOR_L,	0 },
	{ "elevator_r",		PLANE_ELEVATOR_R,	0 },
	{ "aileron_l",		PLANE_AILERON_L,	0 },
	{ "aileron_r",		PLANE_AILERON_R,	0 },
	{ "gear_l",		PLANE_GEAR_L,	0 },
	{ "gear_r",		PLANE_GEAR_R,	0 },
	{ "misc_a",		PLANE_MISC_A,	VEHICLE_FLAG_COMP | CLUMP_FLAG_NO_HIERID },
	{ "misc_b",		PLANE_MISC_B,	VEHICLE_FLAG_COMP | CLUMP_FLAG_NO_HIERID },
	{ "misc_c",		PLANE_MISC_C,	VEHICLE_FLAG_COMP | CLUMP_FLAG_NO_HIERID },

	// Dummy positions.
	{ "ped_frontseat",	CAR_POS_FRONTSEAT,	VEHICLE_FLAG_POS | CLUMP_FLAG_NO_HIERID },
	{ "ped_backseat",	CAR_POS_BACKSEAT,	VEHICLE_FLAG_POS | CLUMP_FLAG_NO_HIERID },
	{ "light_tailplane",	PLANE_POS_LIGHT_TAIL,	VEHICLE_FLAG_POS | CLUMP_FLAG_NO_HIERID },
	{ "light_left",		PLANE_POS_LIGHT_LEFT,	VEHICLE_FLAG_POS | CLUMP_FLAG_NO_HIERID },
	{ "light_right",	PLANE_POS_LIGHT_RIGHT,	VEHICLE_FLAG_POS | CLUMP_FLAG_NO_HIERID },
	{ nil, 0, 0 }
};

RwObjectNameIdAssocation bikeIds[] = {
	{ "chassis_dummy",	BIKE_CHASSIS,	0 },
	{ "forks_front",	BIKE_FORKS_FRONT,	0 },
	{ "forks_rear",		BIKE_FORKS_REAR,	0 },
	{ "wheel_front",	BIKE_WHEEL_FRONT,	0 },
	{ "wheel_rear",		BIKE_WHEEL_REAR,	0 },
	{ "mudguard",		BIKE_MUDGUARD,	0 },
	{ "handlebars",		BIKE_HANDLEBARS,	0 },
	{ "ped_frontseat",	CAR_POS_FRONTSEAT,	VEHICLE_FLAG_POS | CLUMP_FLAG_NO_HIERID },
	{ "ped_backseat",	CAR_POS_BACKSEAT,	VEHICLE_FLAG_POS | CLUMP_FLAG_NO_HIERID },
	{ "headlights",		CAR_POS_HEADLIGHTS,	VEHICLE_FLAG_POS | CLUMP_FLAG_NO_HIERID | VEHICLE_FLAG_DUMMY | ((DUMMY_LIGHT_FRONT_MAIN + 1) << VEHICLE_FLAG_DUMMY_SHIFT) },
	{ "taillights",		CAR_POS_TAILLIGHTS,	VEHICLE_FLAG_POS | CLUMP_FLAG_NO_HIERID | VEHICLE_FLAG_DUMMY | ((DUMMY_LIGHT_REAR_MAIN + 1) << VEHICLE_FLAG_DUMMY_SHIFT) },
	// SA second lamp quad (one corona per entry of this kind, so a car with
	// four lamp dummies gets four lit lenses). Models without them leave the
	// position at zero and the SA code skips it.
	{ "headlights2",	0,	VEHICLE_FLAG_DUMMY | ((DUMMY_LIGHT_FRONT_SECONDARY + 1) << VEHICLE_FLAG_DUMMY_SHIFT) },
	{ "taillights2",	0,	VEHICLE_FLAG_DUMMY | ((DUMMY_LIGHT_REAR_SECONDARY + 1) << VEHICLE_FLAG_DUMMY_SHIFT) },
	{ "exhaust",		CAR_POS_EXHAUST,	VEHICLE_FLAG_POS | CLUMP_FLAG_NO_HIERID },
	{ "extra1",		0,	VEHICLE_FLAG_DRAWLAST | VEHICLE_FLAG_COMP | CLUMP_FLAG_NO_HIERID },
	{ "extra2",		0,	VEHICLE_FLAG_DRAWLAST | VEHICLE_FLAG_COMP | CLUMP_FLAG_NO_HIERID },
	{ "extra3",		0,	VEHICLE_FLAG_DRAWLAST | VEHICLE_FLAG_COMP | CLUMP_FLAG_NO_HIERID },
	{ "extra4",		0,	VEHICLE_FLAG_DRAWLAST | VEHICLE_FLAG_COMP | CLUMP_FLAG_NO_HIERID },
	{ "extra5",		0,	VEHICLE_FLAG_DRAWLAST | VEHICLE_FLAG_COMP | CLUMP_FLAG_NO_HIERID },
	{ "extra6",		0,	VEHICLE_FLAG_DRAWLAST | VEHICLE_FLAG_COMP | CLUMP_FLAG_NO_HIERID },
	{ nil, 0, 0 }
};

// One hierarchy table per eVehicleType. The classes the VC model set does not
// have yet reuse the table of the closest class they behave like, so the array
// is indexed by the type word without holes or fallback arithmetic.
RwObjectNameIdAssocation *CVehicleModelInfo::ms_vehicleDescs[NUM_VEHICLE_TYPES] = {
	/* AUTOMOBILE */ carIds,
	/* MTRUCK     */ carIds,
	/* QUAD       */ carIds,
	/* HELI       */ heliIds,
	/* PLANE      */ planeIds,
	/* BOAT       */ boatIds,
	/* TRAIN      */ trainIds,
	/* FHELI      */ heliIds,
	/* FPLANE     */ planeIds,
	/* BIKE       */ bikeIds,
	/* BMX        */ bikeIds,
	/* TRAILER    */ carIds,
};

bool gbBlackCars;
bool gbPinkCars;

// SA CVehicleStructure::CVehicleStructure (0x6D5BF0). Everything starts zeroed:
// a dummy that is not in the model reads as inactive, an upgrade slot with no
// node reads as CAR_NODE_NONE, and the damageable mask starts empty.
CVehicleStructure::CVehicleStructure(void)
{
	for(int32 i = 0; i < NUM_DUMMIES; i++)
		m_avDummyPos[i] = CVector(0.0f, 0.0f, 0.0f);

	// SA CVehicleStructure::CVehicleStructure (0x4C8D60) sets only the parent
	// component id: -1 means "this model has no such upgrade slot". The reVC
	// node index starts at CAR_NODE_NONE for the same reason, and the offset is
	// zero until the model's own hierarchy fills it in.
	for(int32 i = 0; i < NUM_UPGRADES; i++){
		m_aUpgrades[i].m_nodeIndex = CAR_NODE_NONE;
		m_aUpgrades[i].m_vecOffset = CVector(0.0f, 0.0f, 0.0f);
		m_aUpgrades[i].m_nParentComponentId = -1;
	}

	for(int32 i = 0; i < NUM_EXTRAS; i++)
		m_apExtras[i] = nil;

	m_nNumExtras = 0;
	m_nMaskComponentsDamagable = 0;
}

CVehicleModelInfo::CVehicleModelInfo(void)
 : CClumpModelInfo(MITYPE_VEHICLE)
{
	int32 i;
	for(i = 0; i < NUM_VEHICLE_POSITIONS; i++){
		m_positions[i].x = 0.0f;
		m_positions[i].y = 0.0f;
		m_positions[i].z = 0.0f;
	}
	m_numColours = 0;
	m_animFileIndex = -1;
	// A model starts with no plate of its own: it wears whatever the map region
	// gives it, and it has no material remembered until its clump is walked.
	m_nPlateType = 0;	// CARPLATE_SF; see CCustomCarPlateMgr
	m_szPlateText[0] = '\0';
	m_pPlateMaterial = nil;
#ifdef SA_VEHICLE_MODELS
	m_bHasEmbeddedColModel = false;
#endif
	m_bSAWheelMesh = false;
	m_bSAHierarchy = false;
	// SA starts every slot empty. -1 rather than 0 because model id 0 is a
	// valid model, so "no part here" needs its own value.
	for(i = 0; i < NUM_MODEL_UPGRADES; i++)
		m_anUpgrades[i] = -1;
	for(i = 0; i < NUM_MODEL_REMAPS; i++)
		m_anRemapTxds[i] = -1;
	m_nNumUpgrades = 0;
	m_currentColour3 = -1;
	m_currentColour4 = -1;
	m_nNumRemapEntries = 0;
	for(i = 0; i < NUM_REMAP_ENTRIES; i++){
		m_aRemapEntries[i].material = nil;
		m_aRemapEntries[i].texture = nil;
	}
}

void
CVehicleModelInfo::DeleteRwObject(void)
{
	int32 i;
	RwFrame *f;

	// The remap entries point at materials of the clump that is about to be
	// destroyed; forgetting them here keeps a later Render from writing into
	// freed memory.
	m_nNumRemapEntries = 0;

	for(i = 0; i < m_numComps; i++){
		f = RpAtomicGetFrame(m_comps[i]);
		RpAtomicDestroy(m_comps[i]);
		RwFrameDestroy(f);
	}
	m_numComps = 0;
	CClumpModelInfo::DeleteRwObject();
}

RwObject*
CVehicleModelInfo::CreateInstance(void)
{
	RpClump *clump;
	RpAtomic *atomic;
	RwFrame *clumpframe, *f;
	int32 comp1, comp2;

	clump = (RpClump*)CClumpModelInfo::CreateInstance();
	if(m_numComps != 0){
		clumpframe = RpClumpGetFrame(clump);

		comp1 = ChooseComponent();
		if(comp1 != -1 && m_comps[comp1]){
			atomic = RpAtomicClone(m_comps[comp1]);
			f = RwFrameCreate();
			RwFrameTransform(f,
				RwFrameGetMatrix(RpAtomicGetFrame(m_comps[comp1])),
				rwCOMBINEREPLACE);
			RpAtomicSetFrame(atomic, f);
			RpClumpAddAtomic(clump, atomic);
			RwFrameAddChild(clumpframe, f);
		}
		ms_compsUsed[0] = comp1;

		comp2 = ChooseSecondComponent();
		if(comp2 != -1 && m_comps[comp2]){
			atomic = RpAtomicClone(m_comps[comp2]);
			f = RwFrameCreate();
			RwFrameTransform(f,
				RwFrameGetMatrix(RpAtomicGetFrame(m_comps[comp2])),
				rwCOMBINEREPLACE);
			RpAtomicSetFrame(atomic, f);
			RpClumpAddAtomic(clump, atomic);
			RwFrameAddChild(clumpframe, f);
		}
		ms_compsUsed[1] = comp2;
	}else{
		ms_compsUsed[0] = -1;
		ms_compsUsed[1] = -1;
	}
	return (RwObject*)clump;
}

#ifdef SA_VEHICLE_MODELS
//
// GTA:San Andreas vehicle support
//
// Vice City marks the level of detail of every vehicle mesh in the frame node
// name: only atomics whose name contains "_hi" (or starts with "extra") are
// drawn as part of the vehicle, "_lo" ones are removed and "_vlo" ones are used
// for the very low detail pass. A mesh with any other name is not rendered at
// all.
//
// San Andreas models don't use that convention - their meshes are named "body",
// "interior", "lights_front", "windscreen_stock" and so on, so under VC rules
// such a vehicle is invisible (only its "_vlo" mesh would show up).
//
// A model counts as SA-style when none of its atomic frames carries a VC LOD
// marker. In that case every mesh that is not a LOD/damage part is drawn in hi
// detail, exactly like San Andreas does it.
//
static RpAtomic*
DetectSAHierarchyCB(RpAtomic *atomic, void *data)
{
	char *name = GetFrameNodeName(RpAtomicGetFrame(atomic));
	if(name && strstr(name, "_hi"))
		*(bool*)data = true;
	return atomic;
}

static bool
IsSAHierarchy(RpClump *clump)
{
	bool hasHi = false;
	RpClumpForAllAtomics(clump, DetectSAHierarchyCB, &hasHi);
	return !hasHi;
}

// true if the frame or any of its children has an atomic attached
static RwObject*
FrameHasObjectCB(RwObject *object, void *data)
{
	*(bool*)data = true;
	return object;
}

static RwFrame*
FrameTreeHasObjectCB(RwFrame *frame, void *data)
{
	RwFrameForAllObjects(frame, FrameHasObjectCB, data);
	if(!*(bool*)data)
		RwFrameForAllChildren(frame, FrameTreeHasObjectCB, data);
	return frame;
}

static bool
FrameHasObject(RwFrame *frame)
{
	bool found = false;
	FrameTreeHasObjectCB(frame, &found);
	return found;
}

void
CVehicleModelInfo::AddGenericWheelModel(RwFrame *frame)
{
	RpAtomic *atomic;
	RwV3d scale;

	if(m_wheelId == -1){
		RwFrameDestroy(frame);
		return;
	}
	// The wheel is a separate model, so it may be absent from the model table
	// or not streamed in yet; there is nothing to clone in either case, and
	// a nil instance used to be dereferenced through RpAtomicGetFrame.
	CBaseModelInfo *wheelInfo = CModelInfo::GetModelInfo(m_wheelId);
	atomic = wheelInfo ? (RpAtomic*)wheelInfo->CreateInstance() : nil;
	if(atomic == nil){
		RwFrameDestroy(frame);
		return;
	}
	// The instance comes with its own frame; the wheel is re-parented onto the
	// dummy's frame, so that one has to go first. It may already be absent for
	// an instance whose model had no clump frame.
	RwFrame *instanceFrame = RpAtomicGetFrame(atomic);
	RpAtomicSetFrame(atomic, frame);
	if(instanceFrame)
		RwFrameDestroy(instanceFrame);
	RpClumpAddAtomic(m_clump, atomic);
	CVisibilityPlugins::SetAtomicRenderCallback(atomic,
		CVisibilityPlugins::RenderWheelAtomicCB);
	scale.x = m_wheelScale;
	scale.y = m_wheelScale;
	scale.z = m_wheelScale;
	RwFrameScale(frame, &scale, rwCOMBINEPRECONCAT);
}

#ifdef SA_VEHICLE_MODELS
void
CVehicleModelInfo::SetEmbeddedColModel(CColModel *col)
{
	// the collision that came with the DFF wins over what the .col files gave
	// this model index before
	if(m_colModel != col)
		DeleteCollisionModel();
	SetColModel(col, true);
	m_bHasEmbeddedColModel = true;
}
#endif

// SA models carry their wheel meshes inside the DFF: there is one "wheel" mesh,
// attached to the first wheel dummy, which the SA game clones onto the remaining
// wheel_*_dummy frames. Do the same here, otherwise the car would lose three of
// its wheels (VC vehicles used to have their wheels in a separate model).
static RwFrame*
FindChildAtomicFrameCB(RwFrame *frame, void *data)
{
	RwFrame **result = (RwFrame**)data;
	bool found = false;

	RwFrameForAllObjects(frame, FrameHasObjectCB, &found);
	if(found){
		*result = frame;
		return nil;
	}
	RwFrameForAllChildren(frame, FindChildAtomicFrameCB, data);
	return *result ? nil : frame;
}

// returns the frame holding the wheel mesh of a wheel dummy (or nil)
static RwFrame*
FindWheelMeshFrame(RwFrame *dummy)
{
	RwFrame *f;

	if(dummy == nil)
		return nil;
	if(GetFirstObject(dummy))
		return dummy;
	f = nil;
	RwFrameForAllChildren(dummy, FindChildAtomicFrameCB, &f);
	return f;
}

// "wheel_rf_dummy" -> 'f', "wheel_lb_dummy" -> 'b' etc; 0 if the name doesn't
// follow the usual wheel dummy convention
static char
WheelDummyFrontRearPos(RwFrame *dummy)
{
	char *name = GetFrameNodeName(dummy);
	char *s;

	if(name == nil)
		return 0;
	s = strstr(name, "wheel_");
	if(s == nil)
		return 0;
	s += 6;
	return s[0] != '\0' ? s[1] : 0;
}

// Radius of a wheel mesh around its own origin. An SA wheel is modelled at its
// real size and centred on the axle: x points along the axle, y and z are the
// radial directions, so the largest |y|/|z| of the vertices is the tyre radius.
float
CVehicleModelInfo::GetWheelMeshRadius(RpAtomic *atomic)
{
	RpGeometry *geo;
	RwV3d *verts;
	int32 i, numVerts;
	float radius, v;

	geo = RpAtomicGetGeometry(atomic);
	if(geo == nil || geo->morphTargets == nil || geo->numMorphTargets <= 0)
		return 0.0f;
	verts = geo->morphTargets[0].vertices;
	numVerts = geo->numVertices;
	if(verts == nil || numVerts <= 0)
		return 0.0f;

	radius = 0.0f;
	for(i = 0; i < numVerts; i++){
		v = Abs(verts[i].y);
		if(v > radius) radius = v;
		v = Abs(verts[i].z);
		if(v > radius) radius = v;
	}
	return radius;
}

void
CVehicleModelInfo::CloneSAWheelMeshes(RwFrame **wheelFrames, int32 numWheels)
{
	int32 i, j;
	char pos;

	// The wheel meshes were authored for SA, where the game takes the tyre radius
	// directly from the mesh. VC instead assumes the wheel model was a unit wheel
	// scaled by m_wheelScale, i.e. radius = 0.5*m_wheelScale. Keep both views
	// consistent by deriving m_wheelScale from the mesh: with that every
	// 0.5*m_wheelScale (suspension line, m_fHeightAboveRoad, m_aWheelPosition,
	// wheel collision spheres, ProcessWheelRotation, ...) is the real radius and
	// the tyres of the SA model end up exactly on the road. The mesh itself is
	// then rendered unscaled (GetWheelRenderScale).
	for(i = 0; i < numWheels; i++){
		RwFrame *f = FindWheelMeshFrame(wheelFrames[i]);
		RpAtomic *srcAtomic;
		float radius;

		if(f == nil)
			continue;
		srcAtomic = (RpAtomic*)GetFirstObject(f);
		if(srcAtomic == nil)
			continue;
		radius = GetWheelMeshRadius(srcAtomic);
		if(radius > 0.001f){
			m_wheelScale = 2.0f*radius;
			m_bSAWheelMesh = true;
		}
		break;
	}

	for(i = 0; i < numWheels; i++){
		RwFrame *src, *f;
		RpAtomic *srcAtomic, *atomic;

		if(wheelFrames[i] == nil || FrameHasObject(wheelFrames[i]))
			continue;	// already has a wheel mesh

		// find the most suitable wheel mesh to clone: one from a dummy in the
		// same position (front/middle/rear) is best, any other one will do too
		src = nil;
		pos = WheelDummyFrontRearPos(wheelFrames[i]);
		for(j = 0; j < numWheels; j++){
			if(wheelFrames[j] == nil || j == i)
				continue;
			f = FindWheelMeshFrame(wheelFrames[j]);
			if(f == nil)
				continue;
			if(src == nil)
				src = f;
			if(pos && WheelDummyFrontRearPos(wheelFrames[j]) == pos){
				src = f;
				break;
			}
		}
		if(src == nil)
			continue;
		srcAtomic = (RpAtomic*)GetFirstObject(src);
		if(srcAtomic == nil)
			continue;

		atomic = RpAtomicClone(srcAtomic);
		if(atomic == nil)
			continue;
		// the mesh keeps the offset it had relative to its own dummy, the new
		// dummy contributes the position of the wheel
		f = RwFrameCreate();
		*RwFrameGetMatrix(f) = *RwFrameGetMatrix(src);
		RwFrameUpdateObjects(f);
		RpAtomicSetFrame(atomic, f);
		RwFrameAddChild(wheelFrames[i], f);
		RpClumpAddAtomic(m_clump, atomic);
		// same render callback as the original wheel mesh (VC's RenderWheelAtomicCB
		// belongs to the separate wheel model VC used and must not be used here)
		CVisibilityPlugins::SetAtomicRenderCallback(atomic,
			RpAtomicGetRenderCallBack(srcAtomic));
	}
}
#endif // SA_VEHICLE_MODELS

// Any atomic without a frame is a clump that will crash on its first clone:
// librw reads the frame of every atomic to re-parent it into the clone. The
// hierarchy preprocess keeps model geometry attached to the clump root rather
// than leaving it on a frame that is about to be destroyed, but a converted
// model can reach the same state through a path this code does not know about,
// so the finished clump is checked once here. An atomic that is still
// frame-less is put on the root so the model is usable, and saying so in the
// log is what turns a silent frame leak into something that can be fixed.
struct FramelessAtomicCheck {
	RpClump *clump;
	const char *model;
	int repaired;
};

static RpAtomic*
RepairFramelessAtomicCB(RpAtomic *atomic, void *data)
{
	FramelessAtomicCheck *check = (FramelessAtomicCheck*)data;
	if(atomic == nil || RpAtomicGetFrame(atomic) != nil)
		return atomic;
	RwFrame *root = RpClumpGetFrame(check->clump);
	if(root == nil)
		return atomic;
	RpAtomicSetFrame(atomic, root);
	check->repaired++;
	return atomic;
}

static void
RepairFramelessAtomics(RpClump *clump, const char *model)
{
	if(clump == nil)
		return;
	FramelessAtomicCheck check = { clump, model, 0 };
	RpClumpForAllAtomics(clump, RepairFramelessAtomicCB, &check);
	if(check.repaired)
		REVC_LOG_WARNING("Vehicle model %s: %d atomic(s) had lost their frame; re-attached to the clump root",
			model ? model : "unknown", check.repaired);
}

void
CVehicleModelInfo::SetClump(RpClump *clump)
{
	CClumpModelInfo::SetClump(clump);
#ifdef SA_VEHICLE_MODELS
	// must be known before the render callbacks are chosen
	m_bSAHierarchy = m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE || IsSAHierarchy(clump);
#endif
	SetAtomicRenderCallbacks();
	SetFrameIds(GetVehicleDesc(m_nVehicleType));
	PreprocessHierarchy(m_bSAHierarchy);
	// PreprocessHierarchy may clone wheel meshes and removes frames, so the
	// integrity check happens after it, on the hierarchy that will be cloned
	// for every instance.
	RepairFramelessAtomics(clump, GetModelName());
	FindEditableMaterialList();
	SetEnvironmentMap();
	// SA calls this from SetClump: the clump is the only place the plate
	// material exists, so the model can only find and remember it here. It
	// bakes a random plate into the model's shared texture, which is what every
	// vehicle on the street then references.
	SetCarCustomPlate();
}

// SA CVehicleModelInfo::SetCustomCarPlateText (0x4C8980).
//
// SA truncates to eight characters; this fork would rather the over-long text
// be caught here than show up as a plate with the tail cut off, so the copy
// stops at the buffer and the text is otherwise taken as given.
void
CVehicleModelInfo::SetCustomCarPlateText(const char *text)
{
	if(text == nil){
		m_szPlateText[0] = '\0';
		return;
	}

	uint32 i = 0;
	for(; text[i] != '\0' && i < MAX_PLATE_TEXT; i++)
		m_szPlateText[i] = text[i];
	m_szPlateText[i] = '\0';
}

// SA CVehicleModelInfo::SetCarCustomPlate (0x4C9450).
//
// Called after a clump is set up: find the plate material, remember it, and
// bake a random plate into it. The plate type stays DEFAULT so that a car
// respawned in another city gets that city's background.
void
CVehicleModelInfo::SetCarCustomPlate(void)
{
	m_pPlateMaterial = nil;
	SetCustomCarPlateText(nil);

	m_nPlateType = 0;	// CARPLATE_DEFAULT; see CCustomCarPlateMgr
	char plateBuffer[MAX_PLATE_TEXT + 1];
	memcpy(plateBuffer, "DEFAULT", 8);
	plateBuffer[MAX_PLATE_TEXT] = '\0';

	CCustomCarPlateMgr::GeneratePlateText(plateBuffer, MAX_PLATE_TEXT);
	if(m_clump){
		RpMaterial *material = CCustomCarPlateMgr::SetupClump(m_clump, plateBuffer, m_nPlateType);
		if(material)
			m_pPlateMaterial = material;
	}
}

void
CVehicleModelInfo::SetAnimFile(const char *file)
{
	if(strcasecmp(file, "null") == 0)
		return;

	m_animFileName = new char[strlen(file)+1];
	strcpy(m_animFileName, file);
}

void
CVehicleModelInfo::ConvertAnimFileIndex(void)
{
	if(m_animFileIndex != -1){
		// we have a string pointer in that union
		int32 index = CAnimManager::GetAnimationBlockIndex(m_animFileName);
		delete[] m_animFileName;
		m_animFileIndex = index;
	}
}

// SA CVehicleModelInfo::CollapseFramesCB (0x4C8E30), with one difference that
// matters: the object keeps its own placement.
//
// The function flattens a component's sub-hierarchy, because SA's car code
// addresses a door, a bumper or a wheel through a single frame and everything
// modelled under it has to end up on that frame. The reference moves the objects
// with MoveObjectsCB and destroys the child frames plainly, which throws away the
// transform those frames carried: a door mesh modelled under a child node lands
// at the door frame's origin, not where it was modelled - the part then floats
// inside or next to the car. The child's full transform is folded into the
// frame it lands on instead, so the flattened clump draws exactly what the model
// file drew.
RwFrame*
CVehicleModelInfo::CollapseFramesCB(RwFrame *frame, void *data)
{
	RwFrame *target = (RwFrame*)data;

	// A frame that carries a hierarchy id is a real car node - a door, a bumper,
	// a wheel - not an anonymous container. The collapse exists to flatten the
	// model's own nesting, and the chassis is flagged for it, so without this
	// test every part hanging off the chassis is destroyed here: the nodes the
	// car code looks up by id disappear (m_aCarNodes[CAR_DOOR_RF] stays nil, and
	// opening that door reads a frame that is no longer there), and all of the
	// body's geometry is dumped onto the chassis frame, which is why doors and
	// bumpers are drawn inside the car instead of on it.
	if(frame != target && CVisibilityPlugins::GetFrameHierarchyId(frame) != 0)
		return frame;

	RwFrameForAllChildren(frame, CollapseFramesCB, data);

	// A frame that has nothing on it contributes nothing; destroying it is what
	// the hierarchy collapse is for.
	if(GetFirstObject(frame) == nil){
		RwFrameDestroy(frame);
		return frame;
	}

	// The child's own transform, expressed relative to the target frame, is
	// attached to the objects before they move: an object carries no matrix of
	// its own, so the node it ends up on is its whole placement.
	if(frame != target){
		RwMatrix rel;
		RwMatrixInvert(&rel, RwFrameGetLTM(target));
		RwMatrix relChild;
		RwMatrixMultiply(&relChild, &rel, RwFrameGetLTM(frame));
		RwFrameTransform(frame, &relChild, rwCOMBINEREPLACE);
	}
	RwFrameForAllObjects(frame, MoveObjectsCB, target);
	RwFrameDestroy(frame);
	return frame;
}

RwObject*
CVehicleModelInfo::MoveObjectsCB(RwObject *object, void *data)
{
	RpAtomicSetFrame((RpAtomic*)object, (RwFrame*)data);
	return object;
}

// A component is detached from the model hierarchy so that it can be cloned
// independently of the clump. Its frame only carries a local matrix, though, and
// CreateInstance rebuilds the part from RwFrameGetMatrix - so once the frame is
// no longer attached, that local matrix is all the placement the clone gets. The
// part then appears wherever its frame sat relative to its parent, not where the
// part was in the model: doors and bumpers end up inside or beside the car.
// Baking the parent chain into the frame's own matrix before the detach keeps
// the geometry where the model put it.
static void
CaptureComponentTransform(RwFrame *frame)
{
	if(frame == nil)
		return;
	// Already detached: nothing above it can still contribute.
	RwFrame *parent = RwFrameGetParent(frame);
	if(parent == nil)
		return;
	RwFrameTransform(frame, RwFrameGetLTM(parent), rwCOMBINEPRECONCAT);
}

// A frame that is about to be removed may still carry geometry: a converted
// model can hang a mesh on a node that is also a dummy (the light dummies and
// an exhaust are the usual ones), and the frame is what keeps that mesh in the
// clump. RwFrameDestroy nils the frame of every object on it, so an atomic left
// there stays in the clump with no frame - and the first clone of the model
// then reads its frame's hierarchy root at a nil pointer. Move any such object
// onto a node of its own, placed where that geometry was.
static void
EvictFrameObjects(RwFrame *frame, RwFrame *root)
{
	if(frame == nil || root == nil || frame == root)
		return;

	// The dummy's own transform is what put its geometry where it is, and the
	// object only carries a frame while it is attached to the node. Leaving it
	// on the bare root frame would draw it at the model origin, which is why the
	// geometry keeps its position: the node's full transform is baked into the
	// frame the object moves onto.
	bool hasObject = false;
	RwFrameForAllObjects(frame, FrameHasObjectCB, &hasObject);
	if(!hasObject)
		return;

	RwFrame *nodeFrame = RwFrameCreate();
	if(nodeFrame == nil)
		return;
	RwFrameTransform(nodeFrame, RwFrameGetLTM(frame), rwCOMBINEREPLACE);
	RwFrameForAllObjects(frame, [](RwObject *object, void *node) -> RwObject* {
		// Only atomics are moved: a light or a camera on a vehicle dummy has no
		// frame slot of its own to carry it onto the new node.
		if(object && RwObjectGetType(object) == rpATOMIC)
			RpAtomicSetFrame((RpAtomic*)object, (RwFrame*)node);
		return object;
	}, nodeFrame);
	RwFrameAddChild(root, nodeFrame);
}

RpAtomic*
CVehicleModelInfo::HideDamagedAtomicCB(RpAtomic *atomic, void *data)
{
	const char *name = GetFrameNodeName(RpAtomicGetFrame(atomic));
	if(!name) return atomic;
	if(strstr(name, "_dam")){
		RpAtomicSetFlags(atomic, 0);
		CVisibilityPlugins::SetAtomicFlag(atomic, ATOMIC_FLAG_DAM);
	}else if(strstr(name, "_ok"))
		CVisibilityPlugins::SetAtomicFlag(atomic, ATOMIC_FLAG_OK);
	return atomic;
}

RpAtomic*
CVehicleModelInfo::HideAllComponentsAtomicCB(RpAtomic *atomic, void *data)
{
	if(CVisibilityPlugins::GetAtomicId(atomic) & (uintptr)data)
		RpAtomicSetFlags(atomic, 0);
	else
		RpAtomicSetFlags(atomic, rpATOMICRENDER);
	return atomic;
}

RpMaterial*
CVehicleModelInfo::HasAlphaMaterialCB(RpMaterial *material, void *data)
{
	if(RpMaterialGetColor(material)->alpha != 0xFF){
		*(bool*)data = true;
		return nil;
	}
	return material;
}


// SA 0x4C77E0: all ordinary car atomics are high detail except _vlo.
// No VC _hi requirement, _lo stripping or name-based format detection.
RpAtomic*
CVehicleModelInfo::SetAtomicRendererCB(RpAtomic *atomic, void *data)
{
	(void)data;
	const char *name = GetFrameNodeName(RpAtomicGetFrame(atomic));
	if(name == nil) name = "";
	if(strstr(name, "_vlo"))
		CVisibilityPlugins::SetAtomicRenderCallback(atomic, CVisibilityPlugins::RenderVehicleReallyLowDetailCB);
	else{
		bool alpha = false;
		RpGeometryForAllMaterials(RpAtomicGetGeometry(atomic), HasAlphaMaterialCB, &alpha);
		CVisibilityPlugins::SetAtomicRenderCallback(atomic,
			alpha || strncmp(name, "windscreen", 10) == 0 ?
			CVisibilityPlugins::RenderVehicleHiDetailAlphaCB : CVisibilityPlugins::RenderVehicleHiDetailCB);
	}
	HideDamagedAtomicCB(atomic, nil);
	return atomic;
}

RpAtomic*
CVehicleModelInfo::SetAtomicRendererCB_BigVehicle(RpAtomic *atomic, void *data)
{
	char *name;
	bool alpha;
	const bool saHierarchy = IsSAHierarchyFromCBData(data);

	name = GetFrameNodeName(RpAtomicGetFrame(atomic));
	alpha = false;
	RpGeometryForAllMaterials(RpAtomicGetGeometry(atomic), HasAlphaMaterialCB, &alpha);
	if(strstr(name, "_hi") || !CGeneral::faststrncmp(name, "extra", 5)) {
		if(alpha)
			CVisibilityPlugins::SetAtomicRenderCallback(atomic, CVisibilityPlugins::RenderVehicleHiDetailAlphaCB_BigVehicle);
		else
			CVisibilityPlugins::SetAtomicRenderCallback(atomic, CVisibilityPlugins::RenderVehicleHiDetailCB_BigVehicle);
	}else if(strstr(name, "_lo")){
		if(alpha)
			CVisibilityPlugins::SetAtomicRenderCallback(atomic, CVisibilityPlugins::RenderVehicleLowDetailAlphaCB_BigVehicle);
		else
			CVisibilityPlugins::SetAtomicRenderCallback(atomic, CVisibilityPlugins::RenderVehicleLowDetailCB_BigVehicle);
	}else if(strstr(name, "_vlo"))
		CVisibilityPlugins::SetAtomicRenderCallback(atomic, CVisibilityPlugins::RenderVehicleReallyLowDetailCB_BigVehicle);
	else if(saHierarchy){
		if(alpha)
			CVisibilityPlugins::SetAtomicRenderCallback(atomic, CVisibilityPlugins::RenderVehicleHiDetailAlphaCB_BigVehicle);
		else
			CVisibilityPlugins::SetAtomicRenderCallback(atomic, CVisibilityPlugins::RenderVehicleHiDetailCB_BigVehicle);
	}else
		CVisibilityPlugins::SetAtomicRenderCallback(atomic, nil);
	HideDamagedAtomicCB(atomic, nil);
	return atomic;
}

RpAtomic*
CVehicleModelInfo::SetAtomicRendererCB_Train(RpAtomic *atomic, void *data)
{
	char *name;
	bool alpha;
	const bool saHierarchy = IsSAHierarchyFromCBData(data);

	name = GetFrameNodeName(RpAtomicGetFrame(atomic));
	alpha = false;
	RpGeometryForAllMaterials(RpAtomicGetGeometry(atomic), HasAlphaMaterialCB, &alpha);
	if(strstr(name, "_hi")){
		if(alpha)
			CVisibilityPlugins::SetAtomicRenderCallback(atomic, CVisibilityPlugins::RenderTrainHiDetailAlphaCB);
		else
			CVisibilityPlugins::SetAtomicRenderCallback(atomic, CVisibilityPlugins::RenderTrainHiDetailCB);
	}else if(strstr(name, "_vlo"))
		CVisibilityPlugins::SetAtomicRenderCallback(atomic, CVisibilityPlugins::RenderVehicleReallyLowDetailCB_BigVehicle);
	else if(saHierarchy){
		if(alpha)
			CVisibilityPlugins::SetAtomicRenderCallback(atomic, CVisibilityPlugins::RenderTrainHiDetailAlphaCB);
		else
			CVisibilityPlugins::SetAtomicRenderCallback(atomic, CVisibilityPlugins::RenderTrainHiDetailCB);
	}else
		CVisibilityPlugins::SetAtomicRenderCallback(atomic, nil);
	HideDamagedAtomicCB(atomic, nil);
	return atomic;
}

RpAtomic*
CVehicleModelInfo::SetAtomicRendererCB_Boat(RpAtomic *atomic, void *data)
{
	RpClump *clump;
	char *name;
	bool alpha;
	const bool saHierarchy = IsSAHierarchyFromCBData(data);

	clump = ((CVehicleModelInfo::AtomicRendererCBData*)data)->clump;
	name = GetFrameNodeName(RpAtomicGetFrame(atomic));
	alpha = false;
	RpGeometryForAllMaterials(RpAtomicGetGeometry(atomic), HasAlphaMaterialCB, &alpha);
	if(strcmp(name, "boat_hi") == 0 || !CGeneral::faststrncmp(name, "extra", 5))
		CVisibilityPlugins::SetAtomicRenderCallback(atomic, CVisibilityPlugins::RenderVehicleHiDetailCB_Boat);
	else if(strstr(name, "_hi")){
		if(alpha)
			CVisibilityPlugins::SetAtomicRenderCallback(atomic, CVisibilityPlugins::RenderVehicleHiDetailAlphaCB_Boat);
		else
			CVisibilityPlugins::SetAtomicRenderCallback(atomic, CVisibilityPlugins::RenderVehicleHiDetailCB);
	}else if(strstr(name, "_lo")){
		RpClumpRemoveAtomic(clump, atomic);
		RpAtomicDestroy(atomic);
		return atomic;		// BUG: not done by gta
	}else if(strstr(name, "_vlo"))
		CVisibilityPlugins::SetAtomicRenderCallback(atomic, CVisibilityPlugins::RenderVehicleLoDetailCB_Boat);
	else if(saHierarchy){
		if(alpha)
			CVisibilityPlugins::SetAtomicRenderCallback(atomic, CVisibilityPlugins::RenderVehicleHiDetailAlphaCB_Boat);
		else
			CVisibilityPlugins::SetAtomicRenderCallback(atomic, CVisibilityPlugins::RenderVehicleHiDetailCB_Boat);
	}else{
		if(alpha)
			CVisibilityPlugins::SetAtomicRenderCallback(atomic, CVisibilityPlugins::RenderVehicleHiDetailAlphaCB_Boat);
		else
			CVisibilityPlugins::SetAtomicRenderCallback(atomic, nil);
	}
	HideDamagedAtomicCB(atomic, nil);
	return atomic;
}

RpAtomic*
CVehicleModelInfo::SetAtomicRendererCB_Heli(RpAtomic *atomic, void *data)
{
	char *name;

	name = GetFrameNodeName(RpAtomicGetFrame(atomic));
	if(strncmp(name, "toprotor", 8) == 0)
		CVisibilityPlugins::SetAtomicRenderCallback(atomic, CVisibilityPlugins::RenderVehicleRotorAlphaCB);
	else if(strncmp(name, "rearrotor", 9) == 0)
		CVisibilityPlugins::SetAtomicRenderCallback(atomic, CVisibilityPlugins::RenderVehicleTailRotorAlphaCB);
	else
		CVisibilityPlugins::SetAtomicRenderCallback(atomic, nil);
	return atomic;
}

RpAtomic*
CVehicleModelInfo::SetAtomicRendererCB_RealHeli(RpAtomic *atomic, void *data)
{
	RpClump *clump;
	char *name;
	bool alpha;
	const bool saHierarchy = IsSAHierarchyFromCBData(data);

	clump = ((CVehicleModelInfo::AtomicRendererCBData*)data)->clump;
	name = GetFrameNodeName(RpAtomicGetFrame(atomic));
	alpha = false;
	RpGeometryForAllMaterials(RpAtomicGetGeometry(atomic), HasAlphaMaterialCB, &alpha);
	if(strncmp(name, "toprotor", 8) == 0)
		CVisibilityPlugins::SetAtomicRenderCallback(atomic, CVisibilityPlugins::RenderVehicleRotorAlphaCB);
	else if(strncmp(name, "rearrotor", 9) == 0)
		CVisibilityPlugins::SetAtomicRenderCallback(atomic, CVisibilityPlugins::RenderVehicleTailRotorAlphaCB);
	else if(strstr(name, "_hi") || !CGeneral::faststrncmp(name, "extra", 5)) {
		if(alpha || strncmp(name, "windscreen", 10) == 0)
			CVisibilityPlugins::SetAtomicRenderCallback(atomic, CVisibilityPlugins::RenderVehicleHiDetailAlphaCB);
		else
			CVisibilityPlugins::SetAtomicRenderCallback(atomic, CVisibilityPlugins::RenderVehicleHiDetailCB);
	}else if(strstr(name, "_vlo"))
		CVisibilityPlugins::SetAtomicRenderCallback(atomic, CVisibilityPlugins::RenderVehicleReallyLowDetailCB);
	else if(saHierarchy){
		// SA hierarchy: no VC LOD markers at all, so everything that is not a
		// LOD or damage part is drawn in hi detail
		if(alpha)
			CVisibilityPlugins::SetAtomicRenderCallback(atomic, CVisibilityPlugins::RenderVehicleHiDetailAlphaCB);
		else
			CVisibilityPlugins::SetAtomicRenderCallback(atomic, CVisibilityPlugins::RenderVehicleHiDetailCB);
	}else if(strstr(name, "_lo")){
		RpClumpRemoveAtomic(clump, atomic);
		RpAtomicDestroy(atomic);
		return atomic;		// BUG: nil in gta
	}else
		CVisibilityPlugins::SetAtomicRenderCallback(atomic, nil);
	HideDamagedAtomicCB(atomic, nil);
	return atomic;
}

void
CVehicleModelInfo::SetAtomicRenderCallbacks(void)
{
	// One payload, built from this model only, handed to whichever callback the
	// vehicle type selects. The callbacks used to reach the same two values
	// through a file static and a bare cast of m_clump; the payload makes the
	// ownership obvious and lets a future loader walk models concurrently.
	AtomicRendererCBData cbdata;
	cbdata.clump = m_clump;
	cbdata.saHierarchy = m_bSAHierarchy;

	if(m_nVehicleType == VEHICLE_TYPE_TRAIN)
		RpClumpForAllAtomics(m_clump, SetAtomicRendererCB_Train, &cbdata);
	else
	if(m_nVehicleType == VEHICLE_TYPE_HELI || m_nVehicleType == VEHICLE_TYPE_FHELI)
		RpClumpForAllAtomics(m_clump, SetAtomicRendererCB_Heli, &cbdata);
	else if(m_nVehicleType == VEHICLE_TYPE_PLANE || m_nVehicleType == VEHICLE_TYPE_FPLANE)
		RpClumpForAllAtomics(m_clump, SetAtomicRendererCB_BigVehicle, &cbdata);
	else if(m_nVehicleType == VEHICLE_TYPE_BOAT)
		RpClumpForAllAtomics(m_clump, SetAtomicRendererCB_Boat, &cbdata);
	else if(mod_HandlingManager.GetVehiclePointer((uint32)m_handlingId)->m_bIsHeli)
		RpClumpForAllAtomics(m_clump, SetAtomicRendererCB_RealHeli, &cbdata);
	else
		RpClumpForAllAtomics(m_clump, SetAtomicRendererCB, &cbdata);
}

RwObject*
CVehicleModelInfo::SetAtomicFlagCB(RwObject *object, void *data)
{
	RpAtomic *atomic = (RpAtomic*)object;
	assert(RwObjectGetType(object) == rpATOMIC);
	CVisibilityPlugins::SetAtomicFlag(atomic, (uintptr)data);
	return object;
}

RwObject*
CVehicleModelInfo::ClearAtomicFlagCB(RwObject *object, void *data)
{
	RpAtomic *atomic = (RpAtomic*)object;
	assert(RwObjectGetType(object) == rpATOMIC);
	CVisibilityPlugins::ClearAtomicFlag(atomic, (uintptr)data);
	return object;
}

RwObject*
GetOkAndDamagedAtomicCB(RwObject *object, void *data)
{
	RpAtomic *atomic = (RpAtomic*)object;
	if(CVisibilityPlugins::GetAtomicId(atomic) & ATOMIC_FLAG_OK)
		((RpAtomic**)data)[0] = atomic;
	else if(CVisibilityPlugins::GetAtomicId(atomic) & ATOMIC_FLAG_DAM)
		((RpAtomic**)data)[1] = atomic;
	return object;
}

#ifdef SA_VEHICLE_MODELS
// Adapted from gta-reversed CVehicleModelInfo::PreprocessHierarchy (0x4C8E60).
// Engine hierarchy IDs remain the reVC ABI; automobile physics is not ported.
void
CVehicleModelInfo::PreprocessSACarHierarchy(void)
{
	RwObjectNameIdAssocation *desc = GetVehicleDesc(VEHICLE_TYPE_AUTOMOBILE);
	m_numDoors = 0;
	m_numComps = 0;
	m_bSAWheelMesh = false;
	RpAtomic *mainWheel = nil;
	for(int i = 0; desc[i].name; i++){
		if((desc[i].flags & (VEHICLE_FLAG_POS|VEHICLE_FLAG_COMP)) == 0) continue;
		RwObjectNameAssociation assoc;
		assoc.frame = nil; assoc.name = desc[i].name;
		RwFrameForAllChildren(RpClumpGetFrame(m_clump), FindFrameFromNameWithoutIdCB, &assoc);
		if(!assoc.frame) continue;
		if(desc[i].flags & (VEHICLE_FLAG_POS|VEHICLE_FLAG_DUMMY)){
			// The light dummies and the seat/exhaust positions share the same
			// frame; m_avDummyPos is the SA indexing the light code reads.
			RwV3d *pos = (desc[i].flags & VEHICLE_FLAG_POS)
				? &m_positions[desc[i].hierId]
				: &m_vehicleStruct.m_avDummyPos[(desc[i].flags >> VEHICLE_FLAG_DUMMY_SHIFT) - 1];
			*pos = *RwMatrixGetPos(RwFrameGetMatrix(assoc.frame));
			RwFrame *parent = RwFrameGetParent(assoc.frame);
			while(parent && RwFrameGetParent(parent)){
				RwV3dTransformPoints(pos, pos, 1, RwFrameGetMatrix(parent));
				parent = RwFrameGetParent(parent);
			}
			if(desc[i].flags & VEHICLE_FLAG_DUMMY){
				m_vehicleStruct.m_avDummyPos[(desc[i].flags >> VEHICLE_FLAG_DUMMY_SHIFT) - 1] = *pos;
			}
			// The dummy is only a position; anything modelled on it has to
			// survive the removal (see EvictFrameObjects).
			EvictFrameObjects(assoc.frame, RpClumpGetFrame(m_clump));
			RwFrameDestroy(assoc.frame);
		}else{
			RpAtomic *extra = (RpAtomic*)GetFirstObject(assoc.frame);
			if(!extra || m_numComps >= (int)(sizeof(m_comps)/sizeof(m_comps[0]))) continue;
			CaptureComponentTransform(assoc.frame);
			RpClumpRemoveAtomic(m_clump, extra);
			RwFrameRemoveChild(assoc.frame);
			SetVehicleComponentFlags(assoc.frame, desc[i].flags);
			m_comps[m_numComps++] = extra;
		}
	}
	// SA's main wheel is wheel_rf_dummy, not whichever wheel happens to be
	// encountered first. Missing wheel geometry never invokes VC wheel models.
	RwObjectIdAssociation wheel;
	wheel.frame = nil; wheel.id = CAR_WHEEL_RF;
	RwFrameForAllChildren(RpClumpGetFrame(m_clump), FindFrameFromIdCB, &wheel);
	RwFrame *mesh = FindWheelMeshFrame(wheel.frame);
	if(mesh){
		mainWheel = (RpAtomic*)GetFirstObject(mesh);
		float radius = mainWheel ? GetWheelMeshRadius(mainWheel) : 0.0f;
		// The wheel mesh is modelled at its real size, so this is the radius the
		// physics has to use; the VC path scales a separate wheel model instead.
		if(radius > 0.001f) m_wheelScale = 2.0f*radius;
		m_bSAWheelMesh = mainWheel != nil;
	}
	for(int i = 0; desc[i].name; i++){
		if(desc[i].flags & (VEHICLE_FLAG_POS|VEHICLE_FLAG_COMP)) continue;
		RwObjectIdAssociation assoc;
		assoc.frame = nil; assoc.id = desc[i].hierId;
		RwFrameForAllChildren(RpClumpGetFrame(m_clump), FindFrameFromIdCB, &assoc);
		if(!assoc.frame) continue;
		if(desc[i].flags & VEHICLE_FLAG_DOOR) m_numDoors++;
		if(desc[i].flags & VEHICLE_FLAG_COLLAPSE){
			RpAtomic *parts[2] = { nil, nil };
			RwFrameForAllChildren(assoc.frame, CollapseFramesCB, assoc.frame);
			RwFrameUpdateObjects(assoc.frame);
			RwFrameForAllObjects(assoc.frame, GetOkAndDamagedAtomicCB, parts);
			if(parts[0] && parts[1]) RpAtomicSetRenderCallBack(parts[1], RpAtomicGetRenderCallBack(parts[0]));
		}
		SetVehicleComponentFlags(assoc.frame, desc[i].flags);
		if(!(desc[i].flags & VEHICLE_FLAG_ADD_WHEEL) || !mainWheel) continue;
		if(desc[i].hierId == CAR_WHEEL_RF){
			RwFrameForAllChildren(assoc.frame, CollapseFramesCB, assoc.frame);
			RwFrameUpdateObjects(assoc.frame);
			CVisibilityPlugins::SetAtomicRenderCallback(mainWheel, CVisibilityPlugins::RenderSAWheelAtomicCB);
		}else{
			RpAtomic *clone = RpAtomicClone(mainWheel);
			if(!clone) continue;
			RpAtomicSetFrame(clone, assoc.frame);
			RpClumpAddAtomic(m_clump, clone);
			CVisibilityPlugins::SetAtomicRenderCallback(clone, CVisibilityPlugins::RenderSAWheelAtomicCB);
		}
	}
	SetupUpgradeSlots();
}
#endif

void
CVehicleModelInfo::PreprocessHierarchy(bool saHierarchy)
{
#ifdef SA_VEHICLE_MODELS
	if(m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE){
		PreprocessSACarHierarchy();
		return;
	}
#endif
	int32 i;
	RwObjectNameIdAssocation *desc;
	RwFrame *f;
	RpAtomic *atomic;
	RwV3d *rwvec;

	desc = GetVehicleDesc(m_nVehicleType);
	m_numDoors = 0;
	m_numComps = 0;
#ifdef SA_VEHICLE_MODELS
	// VC knows up to 6 wheel dummies (wheel_rf/rm/rb/lf/lm/lb_dummy)
	RwFrame *wheelFrames[6] = { nil, nil, nil, nil, nil, nil };
	int32 wheelFramesFound = 0;
#endif

	for(i = 0; desc[i].name; i++){
		RwObjectNameAssociation assoc;

		if((desc[i].flags & (VEHICLE_FLAG_COMP|VEHICLE_FLAG_POS)) == 0)
			continue;
		assoc.frame = nil;
		assoc.name = desc[i].name;
		RwFrameForAllChildren(RpClumpGetFrame(m_clump),
			FindFrameFromNameWithoutIdCB, &assoc);
		if(assoc.frame == nil)
			continue;

		if(desc[i].flags & VEHICLE_FLAG_DOOR)
			m_numDoors++;

		if(desc[i].flags & (VEHICLE_FLAG_POS|VEHICLE_FLAG_DUMMY)){
			f = assoc.frame;
			rwvec = (desc[i].flags & VEHICLE_FLAG_POS)
				? &m_positions[desc[i].hierId]
				: &m_vehicleStruct.m_avDummyPos[(desc[i].flags >> VEHICLE_FLAG_DUMMY_SHIFT) - 1];
			*rwvec = *RwMatrixGetPos(RwFrameGetMatrix(f));
			for(f = RwFrameGetParent(f); f; f = RwFrameGetParent(f))
				RwV3dTransformPoints(rwvec, rwvec, 1, RwFrameGetMatrix(f));
			// The light dummies are also mirrored into the SA-indexed array, so
			// DoHeadLightBeam can look up "the second headlight quad" the way SA
			// does regardless of which loader path ran.
			if(desc[i].flags & VEHICLE_FLAG_DUMMY)
				m_vehicleStruct.m_avDummyPos[(desc[i].flags >> VEHICLE_FLAG_DUMMY_SHIFT) - 1] = *rwvec;
			// Same reason as in the SA path above: a dummy frame must not take
			// the geometry that hangs on it into the destroy.
			EvictFrameObjects(assoc.frame, RpClumpGetFrame(m_clump));
			RwFrameDestroy(assoc.frame);
		}else{
			atomic = (RpAtomic*)GetFirstObject(assoc.frame);
			if(atomic == nil)
				continue;
			if(m_numComps >= (int32)(sizeof(m_comps)/sizeof(m_comps[0]))){
				// models with more extra parts than VC supports (SA car
				// conversions often have 8+) keep the atomic in the clump
				// instead of overflowing m_comps[]
				SetVehicleComponentFlags(assoc.frame, desc[i].flags);
				continue;
			}
			CaptureComponentTransform(assoc.frame);
			RpClumpRemoveAtomic(m_clump, atomic);
			RwFrameRemoveChild(assoc.frame);
			SetVehicleComponentFlags(assoc.frame, desc[i].flags);
			m_comps[m_numComps++] = atomic;
		}
	}

	for(i = 0; desc[i].name; i++){
		RwObjectIdAssociation assoc;

		if(desc[i].flags & (VEHICLE_FLAG_COMP|VEHICLE_FLAG_POS))
			continue;
		assoc.frame = nil;
		assoc.id = desc[i].hierId;
		RwFrameForAllChildren(RpClumpGetFrame(m_clump),
			FindFrameFromIdCB, &assoc);
		if(assoc.frame == nil)
			continue;

		if(desc[i].flags & VEHICLE_FLAG_DOOR)
			m_numDoors++;

		if(desc[i].flags & VEHICLE_FLAG_COLLAPSE){
			RpAtomic *okdam[2] = { nil, nil };
			RwFrameForAllChildren(assoc.frame, CollapseFramesCB, assoc.frame);
			RwFrameUpdateObjects(assoc.frame);
			RwFrameForAllObjects(assoc.frame, GetOkAndDamagedAtomicCB, okdam);
			if(okdam[0] && okdam[1])
				RpAtomicSetRenderCallBack(okdam[1], RpAtomicGetRenderCallBack(okdam[0]));
		}

		SetVehicleComponentFlags(assoc.frame, desc[i].flags);

		if(desc[i].flags & VEHICLE_FLAG_ADD_WHEEL){
#ifdef SA_VEHICLE_MODELS
			if(saHierarchy){
				// SA models bring their own wheel meshes. Just collect the dummies
				// here; what to do with them is decided after the loop, because we
				// have to know whether the model has a wheel mesh at all before
				// falling back to the separate VC wheel model.
				if(wheelFramesFound < (int32)(sizeof(wheelFrames)/sizeof(wheelFrames[0])))
					wheelFrames[wheelFramesFound++] = assoc.frame;
				continue;
			}
#endif
			AddGenericWheelModel(assoc.frame);
		}
	}

#ifdef SA_VEHICLE_MODELS
	// SA vehicles store a single wheel mesh which has to be instantiated on all
	// wheel dummy frames (in SA the game does that itself). Note that the mesh
	// often hangs on a child frame of its dummy (usually "wheel" under
	// "wheel_rf_dummy"), not directly on the dummy.
	if(wheelFramesFound > 0){
		bool hasOwnWheels = false;
		for(i = 0; i < wheelFramesFound; i++)
			if(FrameHasObject(wheelFrames[i]))
				hasOwnWheels = true;
		if(hasOwnWheels)
			CloneSAWheelMeshes(wheelFrames, wheelFramesFound);
		else	// no wheel mesh in the model: use the VC wheel model as before
			for(i = 0; i < wheelFramesFound; i++)
				AddGenericWheelModel(wheelFrames[i]);
		SAVEH_LOG("[SAVEH] load %s type %d saHierarchy %d wheelId %d wheelFrames %d ownWheelMesh %d saMesh %d radius %.4f wheelScale %.4f\n",
			GetModelName(), m_nVehicleType, saHierarchy, m_wheelId, wheelFramesFound,
			hasOwnWheels, m_bSAWheelMesh, 0.5f*m_wheelScale, m_wheelScale);
	}
#endif
	// Bikes, boats, planes and trains accept tuning parts too - a bike has a
	// bonnet-less body but still takes rims and an exhaust - so the upgrade
	// attach points are worked out for every vehicle type, not only for cars.
	SetupUpgradeSlots();
}

void
CVehicleModelInfo::SetVehicleComponentFlags(RwFrame *frame, uint32 flags)
{
	tHandlingData *handling;

	handling = mod_HandlingManager.GetVehiclePointer((uint32)m_handlingId);

#define SETFLAGS(f) RwFrameForAllObjects(frame, SetAtomicFlagCB, (void*)(f))

	if(flags & VEHICLE_FLAG_WINDSCREEN){
		if(this == CModelInfo::GetModelInfo(MI_RHINO))
			return;
		SETFLAGS(ATOMIC_FLAG_WINDSCREEN);
	}

	if(flags & VEHICLE_FLAG_ANGLECULL)
		SETFLAGS(ATOMIC_FLAG_ANGLECULL);

	if(flags & VEHICLE_FLAG_FRONT)
		SETFLAGS(ATOMIC_FLAG_FRONT);
	else if(flags & VEHICLE_FLAG_REAR && (handling->m_bIsVan || (flags & (VEHICLE_FLAG_LEFT|VEHICLE_FLAG_RIGHT)) == 0))
		SETFLAGS(ATOMIC_FLAG_REAR);
	else if(flags & VEHICLE_FLAG_LEFT)
		SETFLAGS(ATOMIC_FLAG_LEFT);
	else if(flags & VEHICLE_FLAG_RIGHT)
		SETFLAGS(ATOMIC_FLAG_RIGHT);

	if(flags & VEHICLE_FLAG_REARDOOR)
		SETFLAGS(ATOMIC_FLAG_REARDOOR);
	else if(flags & VEHICLE_FLAG_FRONTDOOR)
		SETFLAGS(ATOMIC_FLAG_FRONTDOOR);

	if(flags & VEHICLE_FLAG_DRAWLAST)
		SETFLAGS(ATOMIC_FLAG_DRAWLAST);
}

#define COMPRULE_RULE(comprule) (((comprule) >> 12) & 0xF)
#define COMPRULE_COMPS(comprule) ((comprule) & 0xFFF)
#define COMPRULE_COMPN(comps, n) (((comps) >> 4*(n)) & 0xF)
#define COMPRULE2_RULE(comprule) (((comprule) >> (12+16)) & 0xF)
#define COMPRULE2_COMPS(comprule) ((comprule >> 16) & 0xFFF)
#define COMPRULE2_COMPN(comps, n) (((comps >> 16) >> 4*(n)) & 0xF)

bool
IsValidCompRule(int rule)
{
	if(rule == 2)
		return CWeather::OldWeatherType == WEATHER_RAINY ||
		       CWeather::NewWeatherType == WEATHER_RAINY;
	return true;
}

int32
CountCompsInRule(int comps)
{
	int32 n;
	for(n = 0; comps != 0; comps >>= 4)
		if((comps & 0xF) != 0xF)
			n++;
	return n;
}

int32
ChooseComponent(int32 rule, int32 comps)
{
	int32 n;
	switch(rule){
	// identical cases....
	case 1:
		n = CGeneral::GetRandomNumberInRange(0, CountCompsInRule(comps));
		return COMPRULE_COMPN(comps, n);
	case 2:
		// only valid in rain
		n = CGeneral::GetRandomNumberInRange(0, CountCompsInRule(comps));
		return COMPRULE_COMPN(comps, n);
	case 3:
		n = CGeneral::GetRandomNumberInRange(0, 1+CountCompsInRule(comps));
		if(n != 0)
			return COMPRULE_COMPN(comps, n-1);
		return -1;
	case 4:
#ifdef FIX_BUGS
		return CGeneral::GetRandomNumberInRange(0, 6);
#else
		return CGeneral::GetRandomNumberInRange(0, 5);
#endif
	}
	return -1;
}

int32
GetListOfComponentsNotUsedByRules(uint32 comprules, int32 numComps, int32 *comps)
{
	int32 i, n;
	int32 unused[6] = { 0, 1, 2, 3, 4, 5 };

	// first comprule
	if(COMPRULE_RULE(comprules) && IsValidCompRule(COMPRULE_RULE(comprules)))
		for(i = 0; i < 3; i++){
			n = COMPRULE_COMPN(comprules, i);
			if(n != 0xF)
				unused[n] = 0xF;
		}
	// second comprule
	comprules >>= 16;
	if(COMPRULE_RULE(comprules) && IsValidCompRule(COMPRULE_RULE(comprules)))
		for(i = 0; i < 3; i++){
			n = COMPRULE_COMPN(comprules, i);
			if(n != 0xF)
				unused[n] = 0xF;
		}

	n = 0;
	for(i = 0; i < numComps; i++)
		if(unused[i] != 0xF)
			comps[n++] = unused[i];
	return n;
}

int32 wheelIds[] = { CAR_WHEEL_LF, CAR_WHEEL_LB, CAR_WHEEL_RF, CAR_WHEEL_RB };

void
CVehicleModelInfo::GetWheelPosn(int32 n, CVector &pos)
{
	RwMatrix *m = RwFrameGetMatrix(GetFrameFromId(m_clump, wheelIds[n]));
	pos.x = RwMatrixGetPos(m)->x;
	pos.y = RwMatrixGetPos(m)->y;
	pos.z = RwMatrixGetPos(m)->z;
}


int32
CVehicleModelInfo::ChooseComponent(void)
{
	int32 comp;
	int32 comps[8];
	int32 n;

	comp = -1;
	if(ms_compsToUse[0] == -2){
		if(COMPRULE_RULE(m_compRules) && IsValidCompRule(COMPRULE_RULE(m_compRules)))
			comp = ::ChooseComponent(COMPRULE_RULE(m_compRules), COMPRULE_COMPS(m_compRules));
		else if(CGeneral::GetRandomNumberInRange(0, 3) < 2){
			n = GetListOfComponentsNotUsedByRules(m_compRules, m_numComps, comps);
			if(n)
				comp = comps[(int)CGeneral::GetRandomNumberInRange(0, n)];
		}
	}else{
		comp = ms_compsToUse[0];
		ms_compsToUse[0] = -2;
	}
	return comp;
}

int32
CVehicleModelInfo::ChooseSecondComponent(void)
{
	int32 comp;
	int32 comps[8];
	int32 n;

	comp = -1;
	if(ms_compsToUse[1] == -2){
		if(COMPRULE2_RULE(m_compRules) && IsValidCompRule(COMPRULE2_RULE(m_compRules)))
			comp = ::ChooseComponent(COMPRULE2_RULE(m_compRules), COMPRULE2_COMPS(m_compRules));
		else if(COMPRULE_RULE(m_compRules) && IsValidCompRule(COMPRULE_RULE(m_compRules)) &&
		        CGeneral::GetRandomNumberInRange(0, 3) < 2){

			n = GetListOfComponentsNotUsedByRules(m_compRules, m_numComps, comps);
			if(n)
				comp = comps[(int)CGeneral::GetRandomNumberInRange(0, n)];
		}
	}else{
		comp = ms_compsToUse[1];
		ms_compsToUse[1] = -2;
	}
	return comp;
}

struct editableMatCBData
{
	CVehicleModelInfo *vehicle;
	int32 numMats1;
	int32 numMats2;
	int32 numMats3;
	int32 numMats4;
};

RpMaterial*
CVehicleModelInfo::GetEditableMaterialListCB(RpMaterial *material, void *data)
{
	const RwRGBA *col;
	editableMatCBData *cbdata;
	int slot = 0;

	cbdata = (editableMatCBData*)data;
	col = RpMaterialGetColor(material);
	if(customveh::IsEditableCarColorMarker(col->red, col->green, col->blue, slot)){
		RwRGBA white = { 255, 255, 255, col->alpha };
		if(slot == 1){
			// one slot is reserved for the terminating nil
			if(cbdata->numMats1 < NUM_FIRST_MATERIALS-1){
				cbdata->vehicle->m_materials1[cbdata->numMats1++] = material;
				RpMaterialSetColor(material, &white);
			}
		}else if(slot == 2){
			if(cbdata->numMats2 < NUM_SECOND_MATERIALS-1){
				cbdata->vehicle->m_materials2[cbdata->numMats2++] = material;
				RpMaterialSetColor(material, &white);
			}
		}else if(slot == 3 && cbdata->numMats3 < NUM_FIRST_MATERIALS-1){
			cbdata->vehicle->m_materials3[cbdata->numMats3++] = material;
			RpMaterialSetColor(material, &white);
		}else if(slot == 4 && cbdata->numMats4 < NUM_FIRST_MATERIALS-1){
			cbdata->vehicle->m_materials4[cbdata->numMats4++] = material;
			RpMaterialSetColor(material, &white);
		}
	}
	return material;
}

RpAtomic*
CVehicleModelInfo::GetEditableMaterialListCB(RpAtomic *atomic, void *data)
{
	RpGeometryForAllMaterials(RpAtomicGetGeometry(atomic), GetEditableMaterialListCB, data);
	return atomic;
}

void
CVehicleModelInfo::FindEditableMaterialList(void)
{
	editableMatCBData cbdata;
	int32 i;

	cbdata.vehicle = this;
	cbdata.numMats1 = 0;
	cbdata.numMats2 = 0;
	cbdata.numMats3 = cbdata.numMats4 = 0;
	RpClumpForAllAtomics(m_clump, GetEditableMaterialListCB, &cbdata);
	for(i = 0; i < m_numComps; i++)
		GetEditableMaterialListCB(m_comps[i], &cbdata);
	m_materials1[cbdata.numMats1] = nil;
	m_materials2[cbdata.numMats2] = nil;
	m_materials3[cbdata.numMats3] = nil;
	m_materials4[cbdata.numMats4] = nil;
	m_currentColour1 = -1;
	m_currentColour2 = -1;
}

void
CVehicleModelInfo::SetVehicleColour(uint8 c1, uint8 c2)
{
	RwRGBA col, *colp;
	RpMaterial **matp;

	if(c1 != m_currentColour1){
		col = ms_vehicleColourTable[c1];
		for(matp = m_materials1; *matp; matp++){
			colp = (RwRGBA*)RpMaterialGetColor(*matp);	// get rid of const
			colp->red = col.red;
			colp->green = col.green;
			colp->blue = col.blue;
		}
		m_currentColour1 = c1;
	}

	if(c2 != m_currentColour2){
		col = ms_vehicleColourTable[c2];
		for(matp = m_materials2; *matp; matp++){
			colp = (RwRGBA*)RpMaterialGetColor(*matp);	// get rid of const
			colp->red = col.red;
			colp->green = col.green;
			colp->blue = col.blue;
		}
		m_currentColour2 = c2;
	}
}

// Explicit four-channel entry point for model/material clients. The existing
// two-colour gameplay API deliberately does not overwrite channels 3 and 4.
void
CVehicleModelInfo::SetVehicleColour(uint8 c1, uint8 c2, uint8 c3, uint8 c4)
{
	SetVehicleColour(c1, c2);
	RpMaterial **lists[] = { m_materials3, m_materials4 };
	const uint8 colours[] = { c3, c4 };
	for(int i = 0; i < 2; ++i){
		const RwRGBA &colour = ms_vehicleColourTable[colours[i]];
		for(RpMaterial **p = lists[i]; *p; ++p){
			RwRGBA value = *RpMaterialGetColor(*p);
			value.red = colour.red;
			value.green = colour.green;
			value.blue = colour.blue;
			RpMaterialSetColor(*p, &value);
		}
	}
}

void
CVehicleModelInfo::ChooseVehicleColour(uint8 &col1, uint8 &col2)
{
	if(m_numColours == 0 || gbBlackCars){
		col1 = 0;
		col2 = 0;
	}else if(gbPinkCars){
		col1 = 68;
		col2 = 68;
	}else{
		m_lastColorVariation = (m_lastColorVariation+1) % m_numColours;
		col1 = m_colours1[m_lastColorVariation];
		col2 = m_colours2[m_lastColorVariation];
		if(m_numColours > 1){
			CVehicle *veh = FindPlayerVehicle();
			if(veh && CModelInfo::GetModelInfo(veh->GetModelIndex()) == this &&
			   veh->m_currentColour1 == col1 &&
			   veh->m_currentColour2 == col2){
				m_lastColorVariation = (m_lastColorVariation+1) % m_numColours;
				col1 = m_colours1[m_lastColorVariation];
				col2 = m_colours2[m_lastColorVariation];
			}
		}
	}
}

void
CVehicleModelInfo::AvoidSameVehicleColour(uint8 *col1, uint8 *col2)
{
	int i, n;

	if(gbBlackCars){
		*col1 = 0;
		*col2 = 0;
	}else if(gbPinkCars){
		*col1 = 68;
		*col2 = 68;
	}else{
		if(m_numColours > 1)
			for(i = 0; i < 8; i++){
				if(*col1 != m_lastColour1 || *col2 != m_lastColour2)
					break;
				n = CGeneral::GetRandomNumberInRange(0, m_numColours);
				*col1 = m_colours1[n];
				*col2 = m_colours2[n];
			}
		m_lastColour1 = *col1;
		m_lastColour2 = *col2;
	}
}

// unused
RwTexture*
CreateCarColourTexture(uint8 r, uint8 g, uint8 b)
{
	RwImage *img;
	RwRaster *ras;
	RwTexture *tex;
	RwUInt8 *pixels;
	RwInt32 width, height, depth, format;

	img = RwImageCreate(2, 2, 32);
	pixels = (RwUInt8*)RwMalloc(2*2*4);
	pixels[0] = r;
	pixels[1] = g;
	pixels[2] = b;
	pixels[3] = 0xFF;
	pixels[4] = r;
	pixels[5] = g;
	pixels[6] = b;
	pixels[7] = 0xFF;
	pixels[8] = r;
	pixels[9] = g;
	pixels[10] = b;
	pixels[11] = 0xFF;
	pixels[12] = r;
	pixels[13] = g;
	pixels[14] = b;
	pixels[15] = 0xFF;
	RwImageSetPixels(img, pixels);
	RwImageSetStride(img, 8);
	RwImageSetPalette(img, nil);
	RwImageFindRasterFormat(img, rwRASTERTYPETEXTURE, &width, &height, &depth, &format);
	ras = RwRasterCreate(width, height, depth, format);
	RwRasterSetFromImage(ras, img);
	RwImageDestroy(img);
	RwFree(pixels);
	tex = RwTextureCreate(ras);
	RwTextureGetName(tex)[0] = '@';
	return tex;
}

void
CVehicleModelInfo::LoadVehicleColours(void)
{
	int fd;
	int i;
	char line[1024];
	int start, end;
	int section, numCols;
	enum {
		NONE,
		COLOURS,
		CARS
	};
	int r, g, b;
	char name[64];
	int colors[16];
	int n;
	int skipped = 0;

#ifdef CUSTOM_MODELS
	// CARCOLS.DAT on disk is a Vice City file: its `car` section names 100+ VC
	// vehicles. Without GTA_VC.DAT those IDEs are never read, so none of those
	// names resolves and the per-car loop below would walk a nil model info.
	// The archive ships the palette only, so the file is not opened at all.
	fd = -1;
	CUSTOM_LOG("data/carcols.dat: palette comes from the archive; native CARCOLS.DAT not read\n");
#else
	CFileMgr::ChangeDir("\\DATA\\");
	fd = CFileMgr::OpenFile("CARCOLS.DAT", "r");
	CFileMgr::ChangeDir("\\");
#endif

	for(i = 0; i < 256; i++)
		ms_colourTextureTable[i] = nil;

	section = 0;
	numCols = 0;
	// fd == -1 means nothing to read: ReadLine is skipped entirely.
	while(fd >= 0 && CFileMgr::ReadLine(fd, line, sizeof(line))){
		// find first valid character in line
		for(start = 0; ; start++)
			if(line[start] > ' ' || line[start] == '\0' || line[start] == '\n')
				break;
		// find end of line
		for(end = start; ; end++){
			if(line[end] == '\0' || line[end] == '\n')
				break;
			if(line[end] == ',' || line[end] == '\r')
				line[end] = ' ';
		}
		line[end] = '\0';

		// empty line
		if(line[start] == '#' || line[start] == '\0')
			continue;

		if(section == NONE){
			if(line[start] == 'c' && line[start + 1] == 'o' && line[start + 2] == 'l')
				section = COLOURS;
			if(line[start] == 'c' && line[start + 1] == 'a' && line[start + 2] == 'r')
				section = CARS;
		}else if(line[start] == 'e' && line[start + 1] == 'n' && line[start + 2] == 'd'){
			section = NONE;
		}else if(section == COLOURS){
			sscanf(&line[start],	// BUG: games doesn't add start
				"%d %d %d", &r, &g, &b);
			ms_vehicleColourTable[numCols].red = r;
			ms_vehicleColourTable[numCols].green = g;
			ms_vehicleColourTable[numCols].blue = b;
			ms_vehicleColourTable[numCols].alpha = 0xFF;
			numCols++;
		}else if(section == CARS){
			n = sscanf(&line[start],	// BUG: games doesn't add start
				"%s %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
				name,
				&colors[0], &colors[1],
				&colors[2], &colors[3],
				&colors[4], &colors[5],
				&colors[6], &colors[7],
				&colors[8], &colors[9],
				&colors[10], &colors[11],
				&colors[12], &colors[13],
				&colors[14], &colors[15]);
			CVehicleModelInfo *mi = (CVehicleModelInfo*)CModelInfo::GetModelInfo(name, nil);
			// A carcols entry may name a vehicle whose IDE was never read (the
			// native map IDEs are gone). Skip it: one stale line must not abort
			// startup, and the model keeps the colours its IDE assigned.
			if(mi == nil || mi->GetModelType() != MITYPE_VEHICLE){
				skipped++;
				continue;
			}
			// A line with only a name parses once; (n-1)/2 then yields 0, which
			// would leave a car with no colours at all. Never go negative either.
			mi->m_numColours = n > 1 ? (n-1)/2 : 0;
			for(i = 0; i < mi->m_numColours; i++){
				mi->m_colours1[i] = colors[i*2 + 0];
				mi->m_colours2[i] = colors[i*2 + 1];
			}
		}
	}

	if(fd >= 0)
		CFileMgr::CloseFile(fd);
	if(skipped)
		REVC_LOG_INFO("LoadVehicleColours: %d carcols entry/entries named a vehicle with no model info", skipped);

#ifdef CUSTOM_MODELS
	// The SA table in common.zip only supplies RGB palette entries (no VC
	// `car` section): every car keeps the colour pair its own IDE gave it.
	// A malformed archive must not change any palette entry or prevent
	// loading the game's stock colours.
	std::vector<uint8> bytes;
	if(CCustomModels::ReadDataFile("data/carcols.dat", bytes)) {
		std::vector<commoncarcols::RGB> palette;
		if(commoncarcols::Parse(bytes.data(), bytes.size(), palette)) {
			for(size_t n = 0; n < palette.size(); n++) {
				ms_vehicleColourTable[n].red = palette[n].r;
				ms_vehicleColourTable[n].green = palette[n].g;
				ms_vehicleColourTable[n].blue = palette[n].b;
				ms_vehicleColourTable[n].alpha = 0xFF;
			}
			CUSTOM_LOG("common.zip data/carcols.dat: applied %u RGB colours; per-car IDE combinations untouched\n",
				(unsigned)palette.size());
		} else
			CUSTOM_LOG("common.zip data/carcols.dat: invalid palette; IDE colour defaults retained\n");
	}
#endif
}

void
CVehicleModelInfo::DeleteVehicleColourTextures(void)
{
	int i;

	for(i = 0; i < 256; i++){
		if(ms_colourTextureTable[i]){
			RwTextureDestroy(ms_colourTextureTable[i]);
			ms_colourTextureTable[i] = nil;
		}
	}
}

RpMaterial*
CVehicleModelInfo::GetMatFXEffectMaterialCB(RpMaterial *material, void *data)
{
	if(RpMatFXMaterialGetEffects(material) == rpMATFXEFFECTNULL)
		return material;
	*(int*)data = RpMatFXMaterialGetEffects(material);
	return nil;
}

struct VehicleReflSetupCtx
{
	CVehicleModelInfo *vehicle;
	const char *frameName;
};

static bool
IsRegisteredEditableMaterial(CVehicleModelInfo *veh, RpMaterial *mat)
{
	if(!veh || !mat)
		return false;
	for(RpMaterial **p = veh->m_materials1; *p; ++p)
		if(*p == mat)
			return true;
	for(RpMaterial **p = veh->m_materials2; *p; ++p)
		if(*p == mat)
			return true;
	for(RpMaterial **p = veh->m_materials3; *p; ++p)
		if(*p == mat) return true;
	for(RpMaterial **p = veh->m_materials4; *p; ++p)
		if(*p == mat) return true;
	return false;
}

static RpMaterial*
ConfigureVehicleMaterialReflectionCB(RpMaterial *material, void *data)
{
	VehicleReflSetupCtx *ctx = (VehicleReflSetupCtx*)data;
	const RwRGBA *col = RpMaterialGetColor(material);
	RwTexture *baseTex = RpMaterialGetTexture(material);

	customveh::VehicleMatInput in;
	memset(&in, 0, sizeof(in));
	in.isEditablePaint = ctx && IsRegisteredEditableMaterial(ctx->vehicle, material);
	in.r = col ? col->red : 255;
	in.g = col ? col->green : 255;
	in.b = col ? col->blue : 255;
	in.a = col ? col->alpha : 255;
	in.frameName = ctx ? ctx->frameName : nil;
	in.texName = baseTex ? RwTextureGetName(baseTex) : nil;
#ifdef LIBRW
	in.rawStructSpecular = ((rw::Material*)material)->surfaceProps.specular;
#else
	in.rawStructSpecular = RpMaterialGetSurfaceProperties(material)->specular;
#endif
	if(RpMatFXMaterialGetEffects(material) == rpMATFXEFFECTENVMAP){
		in.hasMatFxEnv = true;
		in.rawMatFxCoef = RpMatFXMaterialGetEnvMapCoefficient(material);
	}
#ifdef EXTENDED_PIPELINES
	if(CustomPipes::CustomMatOffset > 0){
		CustomPipes::CustomMatExt *ext = CustomPipes::GetCustomMatExt((rw::Material*)material);
		if(ext){
			in.hasSaEnv = ext->hasSaEnv;
			in.saShininess = ext->saShininess;
			in.hasSaSpec = ext->hasSaSpec;
			in.saSpecularity = ext->saSpecularity;
		}
	}
#endif

	const float spec = customveh::ComputeVehicleMaterialSpecular(in);
#ifdef LIBRW
	((rw::Material*)material)->surfaceProps.specular = spec;
#else
	RwSurfaceProperties surf = *RpMaterialGetSurfaceProperties(material);
	surf.specular = spec;
	RpMaterialSetSurfaceProperties(material, &surf);
#endif

	if(spec > 0.0f && in.isEditablePaint && baseTex == nil && gpWhiteTexture)
		RpMaterialSetTexture(material, gpWhiteTexture);

	// Ensure MatFX fallback pipeline also has a valid envmap setup on SA/BR cars
	if(spec > 0.0f && gpFallbackEnvTexture &&
	   RpMatFXMaterialGetEffects(material) == rpMATFXEFFECTNULL){
		RpMatFXMaterialSetEffects(material, rpMATFXEFFECTENVMAP);
		RpMatFXMaterialSetupEnvMap(material, gpFallbackEnvTexture, pMatFxIdentityFrame, FALSE, spec);
	}
	return material;
}

static RpAtomic*
ConfigureAtomicReflectionCB(RpAtomic *atomic, void *data)
{
	if(!atomic || !RpAtomicGetGeometry(atomic))
		return atomic;
	VehicleReflSetupCtx ctx;
	ctx.vehicle = (CVehicleModelInfo*)data;
	RwFrame *f = RpAtomicGetFrame(atomic);
	ctx.frameName = f ? GetFrameNodeName(f) : nil;
	RpGeometryForAllMaterials(RpAtomicGetGeometry(atomic), ConfigureVehicleMaterialReflectionCB, &ctx);
	return atomic;
}

RpMaterial*
CVehicleModelInfo::SetDefaultEnvironmentMapCB(RpMaterial *material, void *data)
{
	if(RpMatFXMaterialGetEffects(material) == rpMATFXEFFECTENVMAP){
		RpMatFXMaterialSetEnvMapFrame(material, pMatFxIdentityFrame);
		if(RpMaterialGetTexture(material) == nil)
			RpMaterialSetTexture(material, gpWhiteTexture);
		if(RpMatFXMaterialGetEnvMapTexture(material) == nil && gpFallbackEnvTexture)
			RpMatFXMaterialSetEnvMapTexture(material, gpFallbackEnvTexture);
		RpMatFXMaterialSetEffects(material, rpMATFXEFFECTENVMAP);
#ifndef PS2_MATFX
		float coef = RpMatFXMaterialGetEnvMapCoefficient(material);
		coef *= 0.25f;	// Tone down a bit for PC
		RpMatFXMaterialSetEnvMapCoefficient(material, coef);
#endif
	}
	return material;
}

RpAtomic*
CVehicleModelInfo::SetEnvironmentMapCB(RpAtomic *atomic, void *data)
{
	int fx;
	RpGeometry *geo;

	geo = RpAtomicGetGeometry(atomic);
	fx = rpMATFXEFFECTNULL;
	RpGeometryForAllMaterials(geo, GetMatFXEffectMaterialCB, &fx);
	if(fx != rpMATFXEFFECTNULL){
		RpMatFXAtomicEnableEffects(atomic);
		RpGeometryForAllMaterials(geo, SetDefaultEnvironmentMapCB, data);
	}
	return atomic;
}

void
CVehicleModelInfo::SetEnvironmentMap(void)
{
	CSimpleModelInfo *wheelmi;
	int32 i;

	if(pMatFxIdentityFrame == nil){
		RwV3d axis = { 1.0f, 0.0f, 0.0f };
		pMatFxIdentityFrame = RwFrameCreate();
		RwMatrixRotate(RwFrameGetMatrix(pMatFxIdentityFrame), &axis, 60.0f, rwCOMBINEREPLACE);
		RwFrameUpdateObjects(pMatFxIdentityFrame);
		RwFrameGetLTM(pMatFxIdentityFrame);
	}

	// Populate Neo specular/shininess from SA/BR/MatFX/paint/glass metadata
	// BEFORE SetDefaultEnvironmentMapCB scales MatFX coef by 0.25f.
	RpClumpForAllAtomics(m_clump, ConfigureAtomicReflectionCB, this);
	for(i = 0; i < m_numComps; i++)
		ConfigureAtomicReflectionCB(m_comps[i], this);

	RpClumpForAllAtomics(m_clump, SetEnvironmentMapCB, nil);
	for(i = 0; i < m_numComps; i++)
		SetEnvironmentMapCB(m_comps[i], nil);
	if(m_wheelId != -1){
		wheelmi = (CSimpleModelInfo*)CModelInfo::GetModelInfo(m_wheelId);
		if(wheelmi){
			for(i = 0; i < wheelmi->m_numAtomics; i++){
				ConfigureAtomicReflectionCB(wheelmi->m_atomics[i], nil);
				SetEnvironmentMapCB(wheelmi->m_atomics[i], nil);
#ifdef EXTENDED_PIPELINES
				if(wheelmi->m_atomics[i])
					CustomPipes::AttachVehiclePipe((rw::Atomic*)wheelmi->m_atomics[i]);
#endif
			}
		}
	}

#ifdef EXTENDED_PIPELINES
	CustomPipes::AttachVehiclePipe(m_clump);
	for(i = 0; i < m_numComps; i++)
		if(m_comps[i])
			CustomPipes::AttachVehiclePipe((rw::Atomic*)m_comps[i]);
#endif
}

void
CVehicleModelInfo::LoadEnvironmentMaps(void)
{
	int32 txdslot;

	txdslot = CTxdStore::FindTxdSlot("particle");
	CTxdStore::PushCurrentTxd();
	CTxdStore::SetCurrentTxd(txdslot);
	if(gpWhiteTexture == nil){
		gpWhiteTexture = RwTextureRead("white", nil);
		RwTextureGetName(gpWhiteTexture)[0] = '@';
		RwTextureSetFilterMode(gpWhiteTexture, rwFILTERLINEAR);
	}
	if(gpFallbackEnvTexture == nil){
		RwImage *img = RwImageCreate(16, 16, 32);
		RwUInt8 *pixels = (RwUInt8*)RwMalloc(16 * 16 * 4);
		for(int y = 0; y < 16; y++){
			float t = (float)y / 15.0f;
			// Smooth sky-to-horizon reflection fallback for MatFX
			RwUInt8 r = (RwUInt8)(155.0f + 75.0f * t);
			RwUInt8 g = (RwUInt8)(190.0f + 45.0f * t);
			RwUInt8 b = (RwUInt8)(235.0f + 15.0f * t);
			for(int x = 0; x < 16; x++){
				int idx = (y * 16 + x) * 4;
				pixels[idx + 0] = r;
				pixels[idx + 1] = g;
				pixels[idx + 2] = b;
				pixels[idx + 3] = 255;
			}
		}
		RwImageSetPixels(img, pixels);
		RwImageSetStride(img, 16 * 4);
		RwImageSetPalette(img, nil);
		RwInt32 w, h, d, fmt;
		RwImageFindRasterFormat(img, rwRASTERTYPETEXTURE, &w, &h, &d, &fmt);
		RwRaster *ras = RwRasterCreate(w, h, d, fmt);
		RwRasterSetFromImage(ras, img);
		RwImageDestroy(img);
		RwFree(pixels);
		gpFallbackEnvTexture = RwTextureCreate(ras);
		if(gpFallbackEnvTexture){
			RwTextureGetName(gpFallbackEnvTexture)[0] = '@';
			RwTextureSetFilterMode(gpFallbackEnvTexture, rwFILTERLINEAR);
			RwTextureSetAddressing(gpFallbackEnvTexture, rwTEXTUREADDRESSCLAMP);
		}
	}
	CTxdStore::PopCurrentTxd();
}

void
CVehicleModelInfo::ShutdownEnvironmentMaps(void)
{
	RwTextureDestroy(gpWhiteTexture);
	gpWhiteTexture = nil;
	if(gpFallbackEnvTexture){
		RwTextureDestroy(gpFallbackEnvTexture);
		gpFallbackEnvTexture = nil;
	}
	RwFrameDestroy(pMatFxIdentityFrame);
	pMatFxIdentityFrame = nil;
}

int
CVehicleModelInfo::GetMaximumNumberOfPassengersFromNumberOfDoors(int id)
{
	int n;

	switch(id){
	case MI_TRAIN:
		n = 3;
		break;
	case MI_FIRETRUCK:
		n = 2;
		break;
	case MI_HUNTER:
		n = 1;
		break;
	default:
		n = ((CVehicleModelInfo*)CModelInfo::GetModelInfo(id))->m_numDoors;
	}

	if(n == 0)
		return id == MI_RCBANDIT || id == MI_PIZZABOY || id == MI_BAGGAGE ? 0 : 1;

	if(id == MI_COACH)
		return 8;

	return n - 1;
}

// ---------------------------------------------------------------------------
// SA vehicle tuning tables (carmods.dat)
// ---------------------------------------------------------------------------

// SA CVehicleModelInfo::CLinkedUpgradeList::AddUpgradeLink (0x4C74B0).
//
// Two parts that must be streamed together. The list is fixed size - SA checks
// nothing either, but a table longer than 30 links would run past the array, so
// the extra ones are dropped with a log line instead.
void
CVehicleModelInfo::CLinkedUpgradeList::AddUpgradeLink(int16 upgrade1, int16 upgrade2)
{
	if(m_nLinksCount >= NUM_LINKS){
		REVC_LOG_WARNING("carmods.dat: more than %d upgrade links, link %d-%d dropped",
			(int)NUM_LINKS, upgrade1, upgrade2);
		return;
	}
	m_anUpgrade1[m_nLinksCount] = upgrade1;
	m_anUpgrade2[m_nLinksCount] = upgrade2;
	m_nLinksCount++;
}

// SA ...::FindOtherUpgrade (0x4C74D0). Searched from the end: a later line for
// the same part overrides an earlier one, so the last match is the live link.
int16
CVehicleModelInfo::CLinkedUpgradeList::FindOtherUpgrade(int16 upgrade) const
{
	for(int32 i = (int32)m_nLinksCount - 1; i >= 0; i--){
		if(m_anUpgrade1[i] == upgrade)
			return m_anUpgrade2[i];
		if(m_anUpgrade2[i] == upgrade)
			return m_anUpgrade1[i];
	}
	return -1;
}

// SA CVehicleModelInfo::AddWheelUpgrade (0x4C8700).
void
CVehicleModelInfo::AddWheelUpgrade(int32 wheelSetNumber, int32 modelId)
{
	if(wheelSetNumber < 0 || wheelSetNumber >= NUM_WHEELS){
		REVC_LOG_WARNING("carmods.dat: wheel set %d out of range, model %d dropped",
			wheelSetNumber, modelId);
		return;
	}
	int32 count = ms_numWheelUpgrades[wheelSetNumber];
	if(count >= NUM_WHEEL_UPGRADES){
		REVC_LOG_WARNING("carmods.dat: wheel set %d already has %d models, %d dropped",
			wheelSetNumber, (int)NUM_WHEEL_UPGRADES, modelId);
		return;
	}
	ms_upgradeWheels[wheelSetNumber][count] = (int16)modelId;
	ms_numWheelUpgrades[wheelSetNumber] = (int16)(count + 1);
}

// SA CVehicleModelInfo::GetWheelUpgrade (0x4C8750).
int32
CVehicleModelInfo::GetWheelUpgrade(int32 wheelSetNumber, int32 wheelUpgradeNumber)
{
	if(wheelSetNumber < 0 || wheelSetNumber >= NUM_WHEELS)
		return -1;
	if(wheelUpgradeNumber < 0 || wheelUpgradeNumber >= ms_numWheelUpgrades[wheelSetNumber])
		return -1;
	return ms_upgradeWheels[wheelSetNumber][wheelUpgradeNumber];
}

// SA CVehicleModelInfo::GetNumWheelUpgrades (0x4C8740).
int32
CVehicleModelInfo::GetNumWheelUpgrades(int32 wheelSetNumber)
{
	if(wheelSetNumber < 0 || wheelSetNumber >= NUM_WHEELS)
		return 0;
	return ms_numWheelUpgrades[wheelSetNumber];
}

// SA CVehicleModelInfo::GetNumRemaps (0x4C86B0).
int32
CVehicleModelInfo::GetNumRemaps(void) const
{
	int32 count = 0;
	while(count < NUM_MODEL_REMAPS && m_anRemapTxds[count] != -1)
		count++;
	return count;
}

// SA CVehicleModelInfo::AddRemap (0x4C86D0).
void
CVehicleModelInfo::AddRemap(int32 txd)
{
	int32 num = GetNumRemaps();
	if(num >= NUM_MODEL_REMAPS){
		REVC_LOG_WARNING("%s: already has %d paintjobs, remap txd %d dropped",
			m_name, (int)NUM_MODEL_REMAPS, txd);
		return;
	}
	m_anRemapTxds[num] = (int16)txd;
}

// SA CVehicleModelInfo::AssignRemapTxd (0x4C9360).
//
// A paintjob texture dictionary is called `remap<vehicle><n>`, e.g.
// `remapblistac1` or `remapsultan3`. The trailing digits are the paintjob
// number, the rest is the vehicle's model name. SA accepts any name that ends
// in a digit and looks the prefix up in the vehicle id range; a name with no
// vehicle behind it (a `remap` texture of a non-car or a custom archive's
// palette) simply does not match and nothing is recorded.
void
CVehicleModelInfo::AssignRemapTxd(const char *name, int16 txdSlot)
{
	if(name == nil)
		return;
	size_t len = strlen(name);
	if(len < 2 || !isdigit((unsigned char)name[len-1]))
		return;

	// Skip the trailing paintjob number, then the bare `remap` prefix.
	size_t prefixEnd = len - 1;
	while(prefixEnd > 0 && isdigit((unsigned char)name[prefixEnd-1]))
		prefixEnd--;
	if(prefixEnd <= 5 || strncasecmp(name, "remap", 5) != 0)
		return;

	char modelName[MAX_MODEL_NAME];
	size_t modelLen = prefixEnd - 5;
	if(modelLen == 0 || modelLen >= sizeof(modelName))
		return;
	memcpy(modelName, name + 5, modelLen);
	modelName[modelLen] = '\0';

	int id = -1;
	CBaseModelInfo *mi = CModelInfo::GetModelInfo(modelName, &id);
	if(mi == nil || id < 0 || mi->GetModelType() != MITYPE_VEHICLE){
		REVC_LOG_INFO("remap txd %s: no vehicle model named %s", name, modelName);
		return;
	}
	((CVehicleModelInfo*)mi)->AddRemap(txdSlot);
}

// SA CVehicleModelInfo::SetVehicleColour, colour-publishing half (0x4C84B0).
// The material lists are recoloured by the SetVehicleColour overloads; this is
// the shared state the remap/material callback reads while the car is drawn.
void
CVehicleModelInfo::SetCurrentColours(uint8 c1, uint8 c2, uint8 c3, uint8 c4)
{
	ms_currentCol[0] = c1;
	ms_currentCol[1] = c2;
	ms_currentCol[2] = c3;
	ms_currentCol[3] = c4;
}

// SA CVehicleModelInfo::LoadVehicleUpgrades (0x5B65A0).
//
// Reads DATA\CARMODS.DAT. Three sections, each closed by `end`:
//   mods   <vehicle> <part> <part> ...   parts a model accepts, in slot order
//   wheel  <set> <wheelModel> ...        rim models of one wheel set
//   link   <part> <part>                 two parts that stream together
//
// SA appends `hydralics` and `stereo` to every `mods` line - every car can be
// given them, so they are not listed per vehicle - at slots 15 and 16 of the
// 18-entry table. The last entry stays free, which is what SA's -1 sentinel
// looks like.
//
// This fork reads the file through CFileMgr like the rest of its data tables
// and logs, rather than asserts, on anything malformed: a single bad line must
// not take the game down at startup.
void
CVehicleModelInfo::LoadVehicleUpgrades(void)
{
	enum eCarModsSection
	{
		CAR_MODS_NONE,
		CAR_MODS_LINK,
		CAR_MODS_MODS,
		CAR_MODS_WHEEL
	};

	for(int32 i = 0; i < NUM_WHEELS; i++)
		ms_numWheelUpgrades[i] = 0;
	ms_linkedUpgrades.m_nLinksCount = 0;

	CFileMgr::ChangeDir("\\DATA\\");
	int fd = CFileMgr::OpenFile("CARMODS.DAT", "r");
	CFileMgr::ChangeDir("\\");
	if(fd <= 0){
		REVC_LOG_WARNING("LoadVehicleUpgrades: DATA\\CARMODS.DAT not found, no tuning available");
		return;
	}

	int32 section = CAR_MODS_NONE;
	int32 numMods = 0;
	int32 numWheels = 0;
	int32 numLinks = 0;

	char *line;
	while((line = CFileLoader::LoadLine(fd)) != nil){
		if(line[0] == '\0')
			continue;

		if(section == CAR_MODS_NONE){
			if(strncmp(line, "link", 4) == 0)
				section = CAR_MODS_LINK;
			else if(strncmp(line, "mods", 4) == 0)
				section = CAR_MODS_MODS;
			else if(strncmp(line, "wheel", 5) == 0)
				section = CAR_MODS_WHEEL;
			continue;
		}

		if(strncmp(line, "end", 3) == 0){
			section = CAR_MODS_NONE;
			continue;
		}

		// LoadLine already turned commas and control characters into spaces,
		// so strtok is enough to walk the tokens of a line.
		char *context = nil;
		char *token = strtok_r(line, " \t", &context);
		if(token == nil)
			continue;

		switch(section){
		case CAR_MODS_LINK: {
			int id1 = -1;
			CBaseModelInfo *first = CModelInfo::GetModelInfo(token, &id1);
			if(first == nil){
				REVC_LOG_WARNING("carmods.dat link: unknown model %s", token);
				break;
			}
			first->SetupVehicleUpgradeFlags(token);

			char *secondName = strtok_r(nil, " \t", &context);
			if(secondName == nil){
				REVC_LOG_WARNING("carmods.dat link: %s has no partner", token);
				break;
			}
			int id2 = -1;
			CBaseModelInfo *second = CModelInfo::GetModelInfo(secondName, &id2);
			if(second == nil){
				REVC_LOG_WARNING("carmods.dat link: unknown model %s", secondName);
				break;
			}
			second->SetupVehicleUpgradeFlags(secondName);

			if(id1 >= 0 && id2 >= 0 && id1 <= INT16_MAX && id2 <= INT16_MAX){
				ms_linkedUpgrades.AddUpgradeLink((int16)id1, (int16)id2);
				numLinks++;
			}
			break;
		}

		case CAR_MODS_MODS: {
			int modelId = -1;
			CBaseModelInfo *base = CModelInfo::GetModelInfo(token, &modelId);
			if(base == nil || base->GetModelType() != MITYPE_VEHICLE){
				REVC_LOG_WARNING("carmods.dat mods: %s is not a vehicle model", token);
				break;
			}
			CVehicleModelInfo *mi = (CVehicleModelInfo*)base;

			// The first fifteen slots are the buyable ones, in file order.
			int32 slot = 0;
			char *partName;
			while((partName = strtok_r(nil, " \t", &context)) != nil){
				if(slot >= NUM_VEHICLE_UPGRADE_SLOTS){
					REVC_LOG_WARNING("carmods.dat mods: %s lists more than %d parts",
						base->GetModelName(), (int)NUM_VEHICLE_UPGRADE_SLOTS);
					break;
				}
				int partId = -1;
				CBaseModelInfo *part = CModelInfo::GetModelInfo(partName, &partId);
				if(part == nil){
					REVC_LOG_WARNING("carmods.dat mods: %s: unknown part %s",
						base->GetModelName(), partName);
					continue;
				}
				part->SetupVehicleUpgradeFlags(partName);
				mi->m_anUpgrades[slot++] = (int16)partId;
			}

			// Every car accepts hydraulics and an upgraded stereo. SA resolves
			// both by name and does not check whether the lookup succeeded; this
			// fork only records a part that actually exists, because a -1 slot
			// and a missing model look the same to the fitting code otherwise.
			static const char *const extraParts[] = { "hydralics", "stereo" };
			for(int32 e = 0; e < 2; e++){
				int partId = -1;
				CBaseModelInfo *part = CModelInfo::GetModelInfo(extraParts[e], &partId);
				if(part == nil){
					REVC_LOG_WARNING("carmods.dat mods: part %s is missing from the model table",
						extraParts[e]);
					continue;
				}
				part->SetupVehicleUpgradeFlags(extraParts[e]);
				mi->m_anUpgrades[NUM_VEHICLE_UPGRADE_SLOTS + e] = (int16)partId;
			}
			mi->m_nNumUpgrades = NUM_VEHICLE_UPGRADE_SLOTS + 2;
			numMods++;
			break;
		}

		case CAR_MODS_WHEEL: {
			int wheelSet = atoi(token);
			char *wheelName;
			while((wheelName = strtok_r(nil, " \t", &context)) != nil){
				int partId = -1;
				CBaseModelInfo *part = CModelInfo::GetModelInfo(wheelName, &partId);
				if(part == nil){
					REVC_LOG_WARNING("carmods.dat wheel: unknown model %s", wheelName);
					continue;
				}
				part->SetupVehicleUpgradeFlags(wheelName);
				AddWheelUpgrade(wheelSet, partId);
				numWheels++;
			}
			break;
		}

		default:
			break;
		}
	}

	CFileMgr::CloseFile(fd);
	REVC_LOG_INFO("LoadVehicleUpgrades: %d vehicle mod lists, %d wheel models, %d links",
		numMods, numWheels, numLinks);
}

#ifdef SA_VEHICLE_MODELS
// SA CVehicleModelInfo::PreprocessHierarchy (0x4C8E60), upgrade half.
//
// A vehicle model that accepts tuning parts carries a frame named after the
// slot instead of a mesh: `spl_dummy` for the spoiler, `bntl_dummy` for the
// left bonnet half, `misc_a_dummy` for the first misc slot and so on. SA walks
// its name table, and for an entry flagged IS_UPGRADE remembers where that
// frame sits - the hierarchy id of its parent, its local position and its
// rotation - then destroys the frame so the car has no empty node hanging in
// it.
//
// reVC has no such frames in its models: the car hierarchy it loads is the one
// the game's own DFFs describe, and those name their parts directly. What the
// fitting code needs is the same thing SA stores: for each eVehicleUpgradePosn,
// the eCarNodes the part hangs off and the offset from it. That is derived
// here from the nodes the model actually has, using SA's own slot-to-node
// mapping. A slot whose node is missing stays unused (-1), which is what
// IsUpgradeAvailable reports.
void
CVehicleModelInfo::SetupUpgradeSlots(void)
{
	// slot -> eCarNodes, in eVehicleUpgradePosn order. SA's table is the same
	// mapping: a bonnet part hangs on the bonnet node (with the damaged halves
	// on the same one), a spoiler on the boot, the wings on the two wing nodes,
	// the bullbars on the two bumper nodes, lights on the windscreen node
	// (where the lamp geometry lives), the roof on the misc_a slot and nitro on
	// misc_b. Damaged variants share their intact slot's node because they are
	// the same part in its damaged form.
	static const int32 slotToNode[NUM_UPGRADE_POSNS] = {
		/* BONNET            */ CAR_BONNET,
		/* BONNET_LEFT       */ CAR_BONNET,
		/* BONNET_RIGHT      */ CAR_BONNET,
		/* BONNET_DAM        */ CAR_BONNET,
		/* BONNET_LEFT_DAM   */ CAR_BONNET,
		/* BONNET_RIGHT_DAM  */ CAR_BONNET,
		/* SPOILER           */ CAR_BOOT,
		/* SPOILER_DAM       */ CAR_BOOT,
		/* WING_LEFT         */ CAR_WING_LF,
		/* WING_RIGHT        */ CAR_WING_RF,
		/* FRONTBULLBAR      */ CAR_BUMP_FRONT,
		/* BACKBULLBAR       */ CAR_BUMP_REAR,
		/* LIGHTS            */ CAR_WINDSCREEN,
		/* LIGHTS_DAM        */ CAR_WINDSCREEN,
		/* ROOF              */ CAR_MISC_A,
		/* NITRO             */ CAR_MISC_B,
	};

	for(int32 slot = 0; slot < NUM_UPGRADE_POSNS; slot++){
		CVehicleStructure::UpgradePosnDesc &desc = m_vehicleStruct.m_aUpgrades[slot];
		desc.m_nodeIndex = slotToNode[slot];
		desc.m_vecOffset = CVector(0.0f, 0.0f, 0.0f);

		// A slot is usable only when the node it names is one of the frames the
		// model was built with. Frames are looked up by the hierarchy id the
		// clump loader assigned, which is the eCarNodes value itself.
		RwObjectIdAssociation assoc;
		assoc.frame = nil;
		assoc.id = desc.m_nodeIndex;
		RwFrameForAllChildren(RpClumpGetFrame(m_clump), FindFrameFromIdCB, &assoc);
		desc.m_nParentComponentId = assoc.frame ? desc.m_nodeIndex : -1;
	}
}
#endif

// SA CVehicleModelInfo::ChooseVehicleColour (0x4C8500), four-channel form.
//
// carcols.dat gives a model up to eight colour *pairs*. SA paints four material
// lists, so a line that names two colours per entry has to fill four channels:
// the entry's two colours are used for the first pair, and the next entry in
// the model's own list supplies the second pair. A model with a single entry
// repeats its two colours, which is what the shipped SA table relies on for the
// overwhelming majority of cars.
//
// variationShift is SA's "skip this many variations" argument; the car
// generator passes 1 so a freshly spawned car is not the same paint as the last
// one built on the same model.
void
CVehicleModelInfo::ChooseVehicleColour(uint8 &col1, uint8 &col2, uint8 &col3, uint8 &col4, int32 variationShift)
{
	if(m_numColours == 0 || gbBlackCars){
		col1 = col2 = col3 = col4 = 0;
		return;
	}
	if(gbPinkCars){
		col1 = col2 = col3 = col4 = 68;
		return;
	}

	m_lastColorVariation = (m_lastColorVariation + variationShift) % m_numColours;
	col1 = m_colours1[m_lastColorVariation];
	col2 = m_colours2[m_lastColorVariation];

	// The second pair comes from the following entry when the model has one.
	// Consecutive entries of one model's carcols line are the alternative
	// paints of the same car, so this keeps a two-colour entry from leaving the
	// extra slots black.
	if(m_numColours > 1){
		int32 second = (m_lastColorVariation + 1) % m_numColours;
		col3 = m_colours1[second];
		col4 = m_colours2[second];
	}else{
		col3 = col1;
		col4 = col2;
	}

	// Never hand the player a car painted exactly like the one they are in.
	if(m_numColours > 1){
		CVehicle *veh = FindPlayerVehicle();
		if(veh && CModelInfo::GetModelInfo(veh->GetModelIndex()) == this &&
		   veh->m_currentColour1 == col1 && veh->m_currentColour2 == col2){
			m_lastColorVariation = (m_lastColorVariation + variationShift) % m_numColours;
			col1 = m_colours1[m_lastColorVariation];
			col2 = m_colours2[m_lastColorVariation];
			col3 = col1;
			col4 = col2;
		}
	}
}

// ---------------------------------------------------------------------------
// Editable materials: the paintjob and the lamps
// ---------------------------------------------------------------------------
//
// SA CVehicleModelInfo::SetEditableMaterialsCB (0x4C8220).
//
// One callback, two jobs, in this order:
//
//  1. A body material - its texture name starts with '#' - is the car's skin.
//     The image is not in the model, it is the paintjob dictionary of the car
//     being drawn, so the material is pointed at it.
//  2. A lamp material - it wears one of the two lamp sheets - carries state,
//     not paint: its RGBA is a marker that says which of the four lamps this
//     quad is, and the game has to answer with the lit sheet or the dark one.
//     The marker colour is never meant to be seen, so it is reset to white and
//     the sheet itself provides the colour.
//
// Anything else - glass, the plate, interior - is left alone.
//
// Every write is recorded in the model's restore list. The two jobs share the
// list because they share the material: a bumper with a lamp quad on it is both
// a body material and a lamp material, which is exactly what SA does too (it
// checks the paintjob condition first and falls through to the lamp condition
// afterwards, so such a material gets both writes).

// The three surface properties SA copies over a lit lamp material
// (gLightSurfProps at 0x8A645C). Full ambient: a lamp is a light source of its
// own and must not be shaded by the scene, so the material keeps its sheet at
// full brightness. No specular: a lamp lens is not a mirror, and letting the
// sun reflect off it is what makes an unlit car look wet. Full diffuse, so the
// last-listed lighting component does not dim the sheet either.
//
// SA keeps a single instance of this structure and copies it wholesale; the
// exact numbers are not recoverable from the binary beyond "the lamp stays at
// its sheet brightness", which is what the three values below produce.
const RwSurfaceProperties &
CVehicleModelInfo::GetLightsSurfaceProperties(void)
{
	static const RwSurfaceProperties props = { 1.0f, 0.0f, 1.0f };
	return props;
}

bool
CVehicleModelInfo::IsLightsMaterial(RpMaterial *material)
{
	RwTexture *texture = RpMaterialGetTexture(material);
	if(texture == nil)
		return false;
	// SA compares the texture pointer against the one it resolved from
	// vehicle.txd. A converted car can carry its own copy of the sheet inside
	// its own TXD, so the name is checked as well.
	if(ms_pLightsTexture && texture == ms_pLightsTexture)
		return true;
	if(ms_pLightsOnTexture && texture == ms_pLightsOnTexture)
		return true;
	return customveh::IsVehicleLightsTextureName(RwTextureGetName(texture));
}

RpMaterial *
CVehicleModelInfo::SetEditableMaterialsCB(RpMaterial *material, void *data)
{
	CVehicleModelInfo *mi = (CVehicleModelInfo*)data;
	if(material == nil || mi == nil)
		return material;

	const RwRGBA *color = RpMaterialGetColor(material);
	RwTexture *texture = RpMaterialGetTexture(material);

	// 1. The paintjob. SA tests the '#' before anything else, and does not
	// make the swap conditional on the material not being a lamp: a '#'
	// texture is a skin, and a skin is never a lamp sheet.
	if(ms_pRemapTexture && texture && RwTextureGetName(texture) &&
	   RwTextureGetName(texture)[0] == '#'){
		if(mi->m_nNumRemapEntries < NUM_REMAP_ENTRIES){
			RemapEntry &entry = mi->m_aRemapEntries[mi->m_nNumRemapEntries++];
			entry.material = material;
			entry.texture = texture;
			entry.swappedTexture = 1;
			entry.changedColour = 0;
			entry.changedSurfaceProps = 0;
			RpMaterialSetTexture(material, ms_pRemapTexture);
		}
		// Re-read: the swap above already changed what the material wears,
		// and the lamp job below must look at the *new* texture, as SA does.
		texture = RpMaterialGetTexture(material);
	}

	// 2. The lamps.
	if(IsLightsMaterial(material)){
		int lightIndex = -1;
		const bool isMarker = customveh::IsVehicleLightMarker(color->red, color->green,
			color->blue, lightIndex);

		// The marker colour is a label for the modeller, not the lamp colour:
		// it is cleared to white for every lamp material, marker or not, so
		// that a model which painted its lamp quads with an arbitrary colour
		// still gets an untinted sheet. A marker is additionally what chooses
		// the lamp slot whose flag decides whether the sheet is lit.
		if(color->red != 255 || color->green != 255 || color->blue != 255){
			if(mi->m_nNumRemapEntries < NUM_REMAP_ENTRIES){
				RemapEntry &entry = mi->m_aRemapEntries[mi->m_nNumRemapEntries++];
				entry.material = material;
				entry.colour = *color;
				entry.changedColour = 1;
				entry.swappedTexture = 0;
				entry.changedSurfaceProps = 0;
			}
			RwRGBA white = { 255, 255, 255, color->alpha };
			RpMaterialSetColor(material, &white);
		}

		// Only a marker names a lamp of the car, so only a marker can be lit.
		// A lamp sheet used for something that is not one of the four lamps
		// (an indicator strip painted on the same sheet, a taxi sign) stays
		// dark, which is what SA does by leaving iLightIndex at -1.
		if(isMarker && lightIndex >= 0 && lightIndex < NUM_LIGHTS &&
		   ms_lightsOn[lightIndex] && ms_pLightsOnTexture){
			// Two fields may need restoring - the sheet and the surface
			// properties - so the room for both is checked together: a
			// half-swapped lamp would stay lit after the car is done.
			if(mi->m_nNumRemapEntries + 2 > NUM_REMAP_ENTRIES)
				return material;

			RwTexture *currentTexture = RpMaterialGetTexture(material);
			const RwSurfaceProperties &props = GetLightsSurfaceProperties();
			const RwSurfaceProperties *oldProps = RpMaterialGetSurfaceProperties(material);
			const bool propsDiffer = oldProps == nil ||
				oldProps->ambient != props.ambient ||
				oldProps->specular != props.specular ||
				oldProps->diffuse != props.diffuse;

			if(currentTexture != ms_pLightsOnTexture){
				RemapEntry &entry = mi->m_aRemapEntries[mi->m_nNumRemapEntries++];
				entry.material = material;
				entry.texture = currentTexture;
				entry.swappedTexture = 1;
				entry.changedColour = 0;
				entry.changedSurfaceProps = 0;
				RpMaterialSetTexture(material, ms_pLightsOnTexture);
			}
			if(propsDiffer){
				RemapEntry &entry = mi->m_aRemapEntries[mi->m_nNumRemapEntries++];
				entry.material = material;
				entry.surfaceProps = *oldProps;
				entry.changedSurfaceProps = 1;
				entry.swappedTexture = 0;
				entry.changedColour = 0;
				RpMaterialSetSurfaceProperties(material, &props);
			}
		}
		return material;
	}

	// 3. Not a lamp, so it can only be one of the four body colour markers.
	// The marker was already turned white by FindEditableMaterialList, which is
	// what keeps the colour of a material that is later repainted from being
	// lost; SA instead resolves the colour here, through ms_currentCol, and
	// does it for every material it walks rather than for a list built when the
	// model was loaded. Both end up painting every body marker with the colour
	// of the slot it names, which is why reVC keeps its own path.
	return material;
}

RpAtomic *
CVehicleModelInfo::SetEditableMaterialsAtomicCB(RpAtomic *atomic, void *data)
{
	CVehicleModelInfo *mi = (CVehicleModelInfo*)data;
	if(atomic == nil || RpAtomicGetGeometry(atomic) == nil)
		return atomic;
	// SA skips an atomic that is not being drawn: its materials are not on
	// screen and swapping them would only be undone again.
	if((RpAtomicGetFlags(atomic) & rpATOMICRENDER) == 0)
		return atomic;
	RpGeometryForAllMaterials(RpAtomicGetGeometry(atomic), SetEditableMaterialsCB, mi);
	return atomic;
}

// SA CVehicleModelInfo::SetEditableMaterials (0x4C8430).
void
CVehicleModelInfo::SetEditableMaterials(void)
{
	if(m_clump == nil)
		return;
	m_nNumRemapEntries = 0;
	RpClumpForAllAtomics(m_clump, SetEditableMaterialsAtomicCB, this);
}

// SA CVehicleModelInfo::ResetEditableMaterials (0x4C8460). The other half:
// every field the callbacks wrote goes back, in reverse order, so that a
// material touched twice (paintjob then lamp, colour then surface properties)
// ends up exactly as it was found. The model is shared by every vehicle of that
// model, so this is what stops one car's paintjob and lit lamps from being
// inherited by the next.
void
CVehicleModelInfo::RestoreEditableMaterials(void)
{
	for(int32 i = m_nNumRemapEntries - 1; i >= 0; i--){
		RemapEntry &entry = m_aRemapEntries[i];
		if(entry.material == nil)
			continue;
		if(entry.swappedTexture)
			RpMaterialSetTexture(entry.material, entry.texture);
		if(entry.changedColour)
			RpMaterialSetColor(entry.material, &entry.colour);
		if(entry.changedSurfaceProps)
			RpMaterialSetSurfaceProperties(entry.material, &entry.surfaceProps);
	}
	m_nNumRemapEntries = 0;
	// The static lamp flags belong to the vehicle that was just drawn, not to
	// the model: clear them so a second Render of the same model - the alpha
	// pass, a mirror - does not light lamps the vehicle no longer has on.
	for(int32 i = 0; i < NUM_LIGHTS; i++)
		ms_lightsOn[i] = 0;
}

// SA CVehicleModelInfo::SetupLightFlags (0x4C8C90). The four flags are read by
// SetEditableMaterialsCB through ms_lightsOn, which is why they are copied out
// of the vehicle instead of being handed to the callback: SA's callback takes
// only the material and a restore-list pointer.
void
CVehicleModelInfo::SetupLightFlags(CVehicle *vehicle)
{
	if(vehicle == nil){
		for(int32 i = 0; i < NUM_LIGHTS; i++)
			ms_lightsOn[i] = 0;
		return;
	}
	ms_lightsOn[0] = vehicle->m_renderLights.m_bLeftFront;
	ms_lightsOn[1] = vehicle->m_renderLights.m_bRightFront;
	ms_lightsOn[2] = vehicle->m_renderLights.m_bLeftRear;
	ms_lightsOn[3] = vehicle->m_renderLights.m_bRightRear;
}

// SA resolves both lamp sheets once, from vehicle.txd, in SetupCommonData
// (0x4C75E0). reVC's dictionary is built by CCustomModels, so the lookup is
// repeated whenever that code has finished with it: a dictionary that carried
// no lamp sheets the first time (BR ships them as standalone archives) must not
// leave the swap permanently off.
void
CVehicleModelInfo::LoadLightTextures(void)
{
	int slot = CTxdStore::GetVehicleTxdSlot();
	if(slot < 0)
		slot = CTxdStore::FindTxdSlot("vehicle");
	if(slot < 0)
		return;
	TxdDef *def = CTxdStore::GetSlot(slot);
	if(def == nil || def->texDict == nil)
		return;

	ms_pLightsTexture = RwTexDictionaryFindNamedTexture(def->texDict, "vehiclelights128");
	ms_pLightsOnTexture = RwTexDictionaryFindNamedTexture(def->texDict, "vehiclelightson128");
}

// SA CVehicleModelInfo::ShutdownLightTexture (0x4C7470).
void
CVehicleModelInfo::ShutdownLightTexture(void)
{
	if(ms_pLightsTexture){
		RwTextureDestroy(ms_pLightsTexture);
		ms_pLightsTexture = nil;
	}
	if(ms_pLightsOnTexture){
		RwTextureDestroy(ms_pLightsOnTexture);
		ms_pLightsOnTexture = nil;
	}
}

// SA CVehicleModelInfo::SetRenderPipelines (0x4C8900).
//
// SA walks the clump and re-setups the environment-map pipeline of every
// atomic, because the custom car pipeline caches the material's texture. reVC's
// vehicle material path reads the texture from the material each frame, so the
// walk is a no-op here; it stays as the named place the remap code calls, and
// as the hook a future cached-pipeline path would need.
void
CVehicleModelInfo::SetRenderPipelines(void)
{
}
