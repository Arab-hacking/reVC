#ifndef __GTA_VEHICLEMODELINFO_H__
#define __GTA_VEHICLEMODELINFO_H__

#include "ClumpModelInfo.h"

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

enum eVehicleType {
	VEHICLE_TYPE_CAR,
	VEHICLE_TYPE_BOAT,
	VEHICLE_TYPE_TRAIN,
	VEHICLE_TYPE_HELI,
	VEHICLE_TYPE_PLANE,
	VEHICLE_TYPE_BIKE,
	NUM_VEHICLE_TYPES
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

class CVehicleModelInfo : public CClumpModelInfo
{
public:
	uint8 m_lastColour1;
	uint8 m_lastColour2;
	char m_gameName[10];
	int32 m_vehicleType;
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
	CVector m_positions[NUM_VEHICLE_POSITIONS];
	uint32 m_compRules;
	float m_bikeSteerAngle;
	RpMaterial *m_materials1[NUM_FIRST_MATERIALS];
	RpMaterial *m_materials2[NUM_SECOND_MATERIALS];
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
#ifdef SA_VEHICLE_MODELS
	// set when the collision of this model came from inside the DFF (SA style);
	// such a model must not get its collision replaced by the VC .col entry of
	// the vehicle whose model id it replaces
	bool m_bHasEmbeddedColModel;
#endif

	static int8 ms_compsToUse[2];
	static int8 ms_compsUsed[2];
	static RwRGBA ms_vehicleColourTable[256];
	static RwTexture *ms_colourTextureTable[256];
	static RwObjectNameIdAssocation *ms_vehicleDescs[NUM_VEHICLE_TYPES];

	CVehicleModelInfo(void);
	void DeleteRwObject(void);
	RwObject *CreateInstance(void);
	void SetClump(RpClump *);
	void SetAnimFile(const char *file);
	void ConvertAnimFileIndex(void);
	int GetAnimFileIndex(void) { return m_animFileIndex; }

	static RwFrame *CollapseFramesCB(RwFrame *frame, void *data);
	// instantiates the separate VC wheel model on a wheel dummy frame
	// (used by VC models and by SA models that carry no wheel mesh at all)
	void AddGenericWheelModel(RwFrame *frame);
#ifdef SA_VEHICLE_MODELS
	// GTA:SA models contain their wheel meshes; this clones the single "wheel"
	// mesh of such models onto all four wheel dummies
	void CloneSAWheelMeshes(RwFrame **wheelFrames, int32 numWheels);
	// collision that came inside the DFF (SA style COL3 block)
	bool HasEmbeddedColModel(void) { return m_bHasEmbeddedColModel; }
	void SetEmbeddedColModel(CColModel *col);
#endif
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
	const CVector &GetFrontSeatPosn(void) { return m_vehicleType == VEHICLE_TYPE_BOAT ? m_positions[BOAT_POS_FRONTSEAT] : m_positions[CAR_POS_FRONTSEAT]; }

	int32 ChooseComponent(void);
	int32 ChooseSecondComponent(void);

	static RpMaterial *GetEditableMaterialListCB(RpMaterial *material, void *data);
	static RpAtomic *GetEditableMaterialListCB(RpAtomic *atomic, void *data);
	void FindEditableMaterialList(void);
	void SetVehicleColour(uint8 c1, uint8 c2);
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
