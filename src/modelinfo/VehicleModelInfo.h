#ifndef __GTA_VEHICLEMODELINFO_H__
#define __GTA_VEHICLEMODELINFO_H__

#include "ClumpModelInfo.h"
#include "eVehicleType.h"
#include "animation/AnimManager.h"

enum {
#ifdef SA_VEHICLE_MODELS
	// GTA:SA models (and SA conversions in particular) can share the vehicle
	// colour between far more materials than VC models do - one SA car easily
	// has 70+ materials carrying the primary car colour marker (60,255,0).
	// The original VC values are too small for those and would overflow.
	NUM_FIRST_MATERIALS = 96,
	NUM_SECOND_MATERIALS = 48,
#else
	NUM_FIRST_MATERIALS = 24,
	NUM_SECOND_MATERIALS = 20,
#endif
	NUM_VEHICLE_COLOURS = 8,
};

enum {
	ATOMIC_FLAG_NONE	= 0x0,
	ATOMIC_FLAG_OK		= 0x1,
	ATOMIC_FLAG_DAM		= 0x2,
	ATOMIC_FLAG_LEFT	= 0x4,
	ATOMIC_FLAG_RIGHT	= 0x8,
	ATOMIC_FLAG_FRONT	= 0x10,
	ATOMIC_FLAG_REAR	= 0x20,
	ATOMIC_FLAG_DRAWLAST	= 0x40,
	ATOMIC_FLAG_WINDSCREEN	= 0x80,
	ATOMIC_FLAG_ANGLECULL	= 0x100,
	ATOMIC_FLAG_REARDOOR	= 0x200,
	ATOMIC_FLAG_FRONTDOOR	= 0x400,
	ATOMIC_FLAG_NOCULL	= 0x800,
};

// SA eVehicleDummy (VehicleModelInfo.h). The clump model loader fills
// m_avDummyPos from frames named "headlights", "headlights2", "taillights",
// "taillights2" and so on; the light code reads the positions back through
// these indices. reVC only had the five eCarPositions entries, which is why
// there was nowhere to look up a second set of light dummies from.
enum eVehicleDummy
{
	DUMMY_LIGHT_FRONT_MAIN      = 0,
	DUMMY_LIGHT_REAR_MAIN       = 1,

	DUMMY_LIGHT_FRONT_SECONDARY = 2,
	DUMMY_LIGHT_REAR_SECONDARY  = 3,

	DUMMY_SEAT_FRONT            = 4,
	DUMMY_SEAT_REAR             = 5,

	DUMMY_EXHAUST               = 6,
	DUMMY_ENGINE                = 7,
	DUMMY_GAS_CAP               = 8,
	DUMMY_TRAILER_ATTACH        = 9,
	DUMMY_HAND_REST             = 10,
	DUMMY_EXHAUST_SECONDARY     = 11,
	DUMMY_WING_AIR_TRAIL        = 12,
	DUMMY_VEHICLE_GUN           = 13,

	MAX_VEHICLE_DUMMIES
};

enum eCarPositions
{
	CAR_POS_HEADLIGHTS,
	CAR_POS_TAILLIGHTS,
	CAR_POS_FRONTSEAT,
	CAR_POS_BACKSEAT,
	CAR_POS_EXHAUST
};

enum eBoatPositions
{
	BOAT_POS_FRONTSEAT
};

enum eTrainPositions
{
	TRAIN_POS_LIGHT_FRONT,
	TRAIN_POS_LIGHT_REAR,
	TRAIN_POS_LEFT_ENTRY,
	TRAIN_POS_MID_ENTRY,
	TRAIN_POS_RIGHT_ENTRY
};

enum ePlanePositions
{
	PLANE_POS_LIGHT_LEFT,
	PLANE_POS_LIGHT_RIGHT,
	PLANE_POS_LIGHT_TAIL,
};

enum {
	NUM_VEHICLE_POSITIONS = 5
};

// The positions the light code needs, in eVehicleDummy terms. Kept as a table
// entry field so the same ms_pCTModelNameTable drives both m_positions (VC
// layout) and m_avDummyPos (SA layout) without duplicating names.
enum eVehicleDummyId
{
	VEHICLE_DUMMY_ID_NONE = 0xFF,
};

// SA CVehicleModelInfo::CVehicleStructure. The per-model data that is not
// colour or geometry: the dummy positions the light/seat code looks up, the
// per-slot upgrade attach points, the extra atomics and the mask of nodes that
// may take damage. SA keeps it behind a pool pointer; reVC keeps the same
// members inline, but the layout and the names are SA's so the tuning and
// damage code can be ported against it directly.
class CVehicleStructure
{
public:
	static constexpr int32 NUM_DUMMIES = 15;
	static constexpr int32 NUM_UPGRADES = 18;
	static constexpr int32 NUM_EXTRAS = 6;

	// One entry of the upgrade table: where the part of that slot attaches and
	// how far it may be displaced when it is damaged.
	struct UpgradePosnDesc
	{
		int32 m_nodeIndex;   // eCarNodes the part hangs off, or CAR_NODE_NONE
		CVector m_vecOffset; // offset from that node in model space
	};

public:
	CVector m_avDummyPos[NUM_DUMMIES];
	UpgradePosnDesc m_aUpgrades[NUM_UPGRADES];
	RpAtomic *m_apExtras[NUM_EXTRAS];
	uint8 m_nNumExtras;
	uint32 m_nMaskComponentsDamagable;

	CVehicleStructure(void);

	// SA helpers.
	bool IsDummyActive(int32 dummy) const { return m_avDummyPos[dummy].x != 0.0f || m_avDummyPos[dummy].y != 0.0f || m_avDummyPos[dummy].z != 0.0f; }
	bool IsComponentDamageable(int32 nodeIndex) const { return m_nMaskComponentsDamagable & (1 << nodeIndex); }
};

class CVehicleModelInfo : public CClumpModelInfo
{
public:
	uint8 m_lastColour1;
	uint8 m_lastColour2;
	char m_gameName[10];
	eVehicleType m_nVehicleType;
	float m_wheelScale;
	union {
		int16 m_wheelId;
		int16 m_planeLodId;
	};
	int16 m_handlingId;
	int8 m_numDoors;
	int8 m_vehicleClass;
	int8 m_level;
	int8 m_numComps;
	int16 m_frequency;
	// SA CVehicleModelInfo::m_nTimesUsed. How many times the population code has
	// spawned this model; CLoadedCarGroup sorts on it and picks the least used.
	// Reset when the model is added to the loaded-vehicles list.
	uint8 m_nTimesUsed;
	CVector m_positions[NUM_VEHICLE_POSITIONS];
	// SA CVehicleModelInfo::m_pVehicleStruct. The dummy positions the light,
	// seat and exhaust code reads live in here, as they do in SA.
	CVehicleStructure m_vehicleStruct;
	uint32 m_compRules;
	float m_bikeSteerAngle;
	RpMaterial *m_materials1[NUM_FIRST_MATERIALS];
	RpMaterial *m_materials2[NUM_SECOND_MATERIALS];
	RpMaterial *m_materials3[NUM_FIRST_MATERIALS];
	RpMaterial *m_materials4[NUM_FIRST_MATERIALS];
	uint8 m_colours1[NUM_VEHICLE_COLOURS];
	uint8 m_colours2[NUM_VEHICLE_COLOURS];
	uint8 m_numColours;
	uint8 m_lastColorVariation;
	uint8 m_currentColour1;
	uint8 m_currentColour2;
	RpAtomic *m_comps[6];
	// This is stupid, CClumpModelInfo already has it!
	union {
		int32 m_animFileIndex;
		char *m_animFileName;
	};
	// The animation set this model's occupants play through. SA keeps one set
	// per body type, so a limousine shares the truck set and a lowrider its own.
	AssocGroupId m_animGroup;
#ifdef SA_VEHICLE_MODELS
	// set when the collision of this model came from inside the DFF (SA style);
	// such a model must not get its collision replaced by the VC .col entry of
	// the vehicle whose model id it replaces
	bool m_bHasEmbeddedColModel;
#endif
	// set when the model brought its own wheel meshes (SA style). VC derives the
	// wheel radius from m_wheelScale (radius = 0.5*scale) and scales the wheel
	// mesh by it on rendering. For SA models the mesh is already the right size
	// and m_wheelScale is set to the true diameter (2*radius), so rendering must
	// not scale it again.
	bool m_bSAWheelMesh;

	static int8 ms_compsToUse[2];
	static int8 ms_compsUsed[2];
	static RwRGBA ms_vehicleColourTable[256];
	static RwTexture *ms_colourTextureTable[256];
	static RwObjectNameIdAssocation *ms_vehicleDescs[NUM_VEHICLE_TYPES];

	// m_nVehicleType is a model-info field, so a corrupt or not-yet-initialised
	// entry can hold VEHICLE_TYPE_IGNORE (-1) or anything else. Both hierarchy
	// users go through here instead of indexing the table raw.
	static RwObjectNameIdAssocation *GetVehicleDesc(eVehicleType type)
	{
		if(type < 0 || type >= NUM_VEHICLE_TYPES)
			return ms_vehicleDescs[VEHICLE_TYPE_AUTOMOBILE];
		return ms_vehicleDescs[type];
	}

	CVehicleModelInfo(void);
	void DeleteRwObject(void);
	RwObject *CreateInstance(void);
	void SetClump(RpClump *);
	void SetAnimFile(const char *file);
	void ConvertAnimFileIndex(void);
	// The car animation sets differ by body type: a limousine uses the truck
	// set, a lowrider its own, a bus the bus set. The set is a property of the
	// model, worked out once from the model's own data, and every ped entering
	// or leaving the car plays through it.
	AssocGroupId GetAnimGroup(void) const { return m_animGroup; }
	// SA CVehicleModelInfo::m_nRefCount. Same field CBaseModelInfo keeps as
	// m_refCount - how many instances of this model are in the world - exposed
	// under SA's name for the population code.
	uint16 GetRefCount(void) const { return m_refCount; }
	int GetAnimFileIndex(void) { return m_animFileIndex; }

	static RwFrame *CollapseFramesCB(RwFrame *frame, void *data);
	// instantiates the separate VC wheel model on a wheel dummy frame
	// (used by VC models and by SA models that carry no wheel mesh at all)
	void AddGenericWheelModel(RwFrame *frame);
#ifdef SA_VEHICLE_MODELS
	// GTA:SA models contain their wheel meshes; this clones the single "wheel"
	// mesh of such models onto all four wheel dummies
	void CloneSAWheelMeshes(RwFrame **wheelFrames, int32 numWheels);
	void PreprocessSACarHierarchy(void);
	// radius of a wheel mesh around its own origin (SA wheels are modelled at
	// their real size, so this is the radius the physics has to use)
	static float GetWheelMeshRadius(RpAtomic *atomic);
	// collision that came inside the DFF (SA style COL3 block)
	bool HasEmbeddedColModel(void) { return m_bHasEmbeddedColModel; }
	void SetEmbeddedColModel(CColModel *col);
#endif
	// scale the wheel meshes have to be rendered with: SA models carry their
	// wheels at their final size, VC models get their separate wheel model scaled
	// by m_wheelScale
	float GetWheelRenderScale(void) { return m_nVehicleType == VEHICLE_TYPE_AUTOMOBILE || m_bSAWheelMesh ? 1.0f : m_wheelScale; }
	// true when the wheel meshes came with the model (SA style)
	bool HasOwnWheelMeshes(void) { return m_bSAWheelMesh; }
	static RwObject *MoveObjectsCB(RwObject *object, void *data);
	static RpAtomic *HideDamagedAtomicCB(RpAtomic *atomic, void *data);
	static RpAtomic *HideAllComponentsAtomicCB(RpAtomic *atomic, void *data);
	static RpMaterial *HasAlphaMaterialCB(RpMaterial *material, void *data);

	static RpAtomic *SetAtomicRendererCB(RpAtomic *atomic, void *data);
	static RpAtomic *SetAtomicRendererCB_BigVehicle(RpAtomic *atomic, void *data);
	static RpAtomic *SetAtomicRendererCB_Train(RpAtomic *atomic, void *data);
	static RpAtomic *SetAtomicRendererCB_Boat(RpAtomic *atomic, void *data);
	static RpAtomic *SetAtomicRendererCB_Heli(RpAtomic *atomic, void *data);
	static RpAtomic *SetAtomicRendererCB_RealHeli(RpAtomic *atomic, void *data);
	void SetAtomicRenderCallbacks(void);

	static RwObject *SetAtomicFlagCB(RwObject *object, void *data);
	static RwObject *ClearAtomicFlagCB(RwObject *atomic, void *data);
	void SetVehicleComponentFlags(RwFrame *frame, uint32 flags);
	void PreprocessHierarchy(void);
	void GetWheelPosn(int32 n, CVector &pos);
	const CVector &GetFrontSeatPosn(void) { return m_nVehicleType == VEHICLE_TYPE_BOAT ? m_positions[BOAT_POS_FRONTSEAT] : m_positions[CAR_POS_FRONTSEAT]; }

	int32 ChooseComponent(void);
	int32 ChooseSecondComponent(void);

	static RpMaterial *GetEditableMaterialListCB(RpMaterial *material, void *data);
	static RpAtomic *GetEditableMaterialListCB(RpAtomic *atomic, void *data);
	void FindEditableMaterialList(void);
	void SetVehicleColour(uint8 c1, uint8 c2);
	void SetVehicleColour(uint8 c1, uint8 c2, uint8 c3, uint8 c4);
	void ChooseVehicleColour(uint8 &col1, uint8 &col2);
	void AvoidSameVehicleColour(uint8 *col1, uint8 *col2);
	static void LoadVehicleColours(void);
	static void DeleteVehicleColourTextures(void);

	static RpAtomic *SetEnvironmentMapCB(RpAtomic *atomic, void *data);
	static RpMaterial *SetDefaultEnvironmentMapCB(RpMaterial *material, void *data);
	static RpMaterial *GetMatFXEffectMaterialCB(RpMaterial *material, void *data);
	void SetEnvironmentMap(void);
	static void LoadEnvironmentMaps(void);
	static void ShutdownEnvironmentMaps(void);

	static int GetMaximumNumberOfPassengersFromNumberOfDoors(int id);
	static void SetComponentsToUse(int8 c1, int8 c2) { ms_compsToUse[0] = c1; ms_compsToUse[1] = c2; }
};

extern bool gbBlackCars;
extern bool gbPinkCars;

#endif // __GTA_VEHICLEMODELINFO_H__
