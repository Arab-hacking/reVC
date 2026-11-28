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

// SA eVehicleUpgradePosn (VehicleModelInfo.h). The index of an attach slot of
// a vehicle model: the slot tells CVehicle·where a bought part hangs and what
// it is replaced with when the panel it sits on takes damage. The order is
// SA's and is baked into the model data, so it is not renumbered here.
enum eVehicleUpgradePosn
{
	UPGRADE_BONNET,
	UPGRADE_BONNET_LEFT,
	UPGRADE_BONNET_RIGHT,
	UPGRADE_BONNET_DAM,
	UPGRADE_BONNET_LEFT_DAM,
	UPGRADE_BONNET_RIGHT_DAM,
	UPGRADE_SPOILER,
	UPGRADE_SPOILER_DAM,
	UPGRADE_WING_LEFT,
	UPGRADE_WING_RIGHT,
	UPGRADE_FRONTBULLBAR,
	UPGRADE_BACKBULLBAR,
	UPGRADE_LIGHTS,
	UPGRADE_LIGHTS_DAM,
	UPGRADE_ROOF,
	UPGRADE_NITRO,
	UPGRADE_BOOT,
	UPGRADE_CHASSIS,
	UPGRADE_EXHAUST,

	NUM_UPGRADE_POSNS
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
	// An atomic that is part of a bought upgrade. Set on the model info of the
	// part when carmods.dat names it, so the fitting code can tell a fitted
	// part from the car's own geometry when it walks the clump.
	ATOMIC_FLAG_UPGRADE	= 0x1000,
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
	//
	// m_nParentComponentId is SA's field - the eCarNodes the part hangs off,
	// or -1 when this model has no such slot. m_nodeIndex/m_vecOffset are the
	// reVC view of the same thing: the fitting code needs the node id and the
	// offset from it, and the two are filled from the model's own frames.
	struct UpgradePosnDesc
	{
		int32 m_nodeIndex;         // eCarNodes the part hangs off, or CAR_NODE_NONE
		CVector m_vecOffset;       // offset from that node in model space
		int32 m_nParentComponentId; // SA name for the same node, -1 = no slot
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
	bool IsUpgradeSlotUsed(eVehicleUpgradePosn slot) const
	{
		return slot >= 0 && slot < NUM_UPGRADES && m_aUpgrades[slot].m_nParentComponentId >= 0;
	}
};

// SA's plate texts are at most eight characters (see CCustomCarPlateMgr).
enum { MAX_PLATE_TEXT = 8 };

// Sizes of the per-model tuning tables, SA's own numbers.
enum {
	NUM_MODEL_UPGRADES = 18, // 15 buyable slots + hydraulics + stereo + spare
	// carmods.dat lists up to fifteen parts per vehicle; hydraulics and stereo
	// follow them at fixed slots, which is what SA's loader does.
	NUM_VEHICLE_UPGRADE_SLOTS = 15,
	NUM_MODEL_REMAPS   = 4,  // a model wears at most four paintjobs
	NUM_CURRENT_COLORS = 4,  // SA renders cars with four colour slots
	NUM_LIGHTS         = 4,  // SA ms_lightsOn: LF, RF, LR, RR
	NUM_WHEELS         = 4,  // SA numbers wheel sets, not individual wheels
	NUM_WHEEL_UPGRADES = 15  // wheel models per set (one per rim design)
};

class CVehicle;

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
	// SA CVehicleModelInfo::m_nCurrentTertiaryColor / QuaternaryColor. The
	// third and fourth material slots the four-colour carcols scheme paints;
	// -1 means "this model has no such slot", the same "not applied yet"
	// marker the first two use.
	uint8 m_currentColour3;
	uint8 m_currentColour4;
	RpAtomic *m_comps[6];
	// This is stupid, CClumpModelInfo already has it!
	union {
		int32 m_animFileIndex;
		char *m_animFileName;
	};
	// The animation set this model's occupants play through. SA keeps one set
	// per body type, so a limousine shares the truck set and a lowrider its own.
	AssocGroupId m_animGroup;
	// SA CVehicleModelInfo::m_nPlateType. Which of the three plate backgrounds
	// this model wears, or CARPLATE_DEFAULT to decide from the map region when
	// the plate is generated. Set by the model's own data; SA clears it to -1
	// on the shared model once a custom plate has been baked in.
	uint8 m_nPlateType;
	// SA CVehicleModelInfo::m_szPlateText. A script-supplied plate text for the
	// next vehicle built on this model: the car generator writes it before the
	// car is constructed, and the constructor consumes it.
	char m_szPlateText[MAX_PLATE_TEXT + 1];
	// SA CVehicleModelInfo::m_pPlateMaterial. The material named "carplate" in
	// this model's clump, remembered so a plate can be regenerated without
	// walking the clump again.
	RpMaterial *m_pPlateMaterial;
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
	// Whether this model uses the SA node naming convention. Computed once in
	// SetClump and read by the renderer-callback selection and by the hierarchy
	// preprocessor. It is per model, so it belongs to the model info instance
	// (and to the callbacks that this instance invokes), not to the file.
	bool m_bSAHierarchy;

	// SA CVehicleModelInfo::m_anUpgrades. The parts this model accepts, one
	// model id per slot of the carmods.dat `mods` list, in file order. The
	// first fifteen entries are the buyable slots (bonnet, bumper, exhaust,
	// ...), then hydraulics and stereo are appended for every car, exactly as
	// the SA loader does. -1 means "the model has nothing in this slot".
	int16 m_anUpgrades[NUM_MODEL_UPGRADES];
	// SA CVehicleModelInfo::m_anRemapTxds. The paintjob texture dictionaries
	// this model can wear, in the order remap files were assigned to it. -1
	// terminates the list, so the vehicle's remap index is a position here.
	int16 m_anRemapTxds[NUM_MODEL_REMAPS];
	// How many of m_anUpgrades were actually filled from the file. hydraulics
	// and stereo are always the last two, but they only count when the model
	// exists in the model table at all.
	int32 m_nNumUpgrades;

	static int8 ms_compsToUse[2];
	static int8 ms_compsUsed[2];
	static RwRGBA ms_vehicleColourTable[256];
	static RwTexture *ms_colourTextureTable[256];
	static RwObjectNameIdAssocation *ms_vehicleDescs[NUM_VEHICLE_TYPES];

	// SA CVehicleModelInfo::CLinkedUpgradeList. Two upgrade models that have to
	// be streamed together because the car cannot show one without the other:
	// a left wing panel and its right twin, a bumper and the matching exhaust.
	// The list is built from the `link` section of carmods.dat.
	class CLinkedUpgradeList
	{
	public:
		static constexpr int32 NUM_LINKS = 30;

		int16 m_anUpgrade1[NUM_LINKS];
		int16 m_anUpgrade2[NUM_LINKS];
		uint32 m_nLinksCount;

		CLinkedUpgradeList(void) : m_nLinksCount(0)
		{
			for(int32 i = 0; i < NUM_LINKS; i++){
				m_anUpgrade1[i] = -1;
				m_anUpgrade2[i] = -1;
			}
		}
		// SA CVehicleModelInfo::CLinkedUpgradeList::AddUpgradeLink (0x4C74B0).
		void AddUpgradeLink(int16 upgrade1, int16 upgrade2);
		// SA ...::FindOtherUpgrade (0x4C74D0). The other end of a link, or -1.
		// Searched back to front: a later line overrides an earlier one.
		int16 FindOtherUpgrade(int16 upgrade) const;
	};

	static CLinkedUpgradeList ms_linkedUpgrades;

	// SA CVehicleModelInfo state used while a vehicle is being drawn: the four
	// colour slots of the material lists, the paintjob texture currently being
	// applied, and the wheel upgrade / frame tables the wheel code reads back.
	static uint8 ms_currentCol[NUM_CURRENT_COLORS];
	static RwTexture *ms_pRemapTexture;
	// SA CVehicleModelInfo::ms_pLightsTexture / ms_pLightsOnTexture
	// (0xB4E68C / 0xB4E690). The two lamp sheets of vehicle.txd: the dark one
	// every car model paints its lamps with, and the lit one a lamp material is
	// swapped to while that lamp is on. Both are resolved once, when
	// vehicle.txd has been loaded, and stay nil when the dictionary has no such
	// texture - the swap is then skipped and the lamps keep their material
	// colour, which is what a VC-era car without lamp textures wants.
	static RwTexture *ms_pLightsTexture;
	static RwTexture *ms_pLightsOnTexture;
	// SA CVehicleModelInfo::ms_lightsOn[4] (0xB4E3E8). Which of the four lamp
	// slots of the car being drawn are lit: 0 left front, 1 right front,
	// 2 left rear, 3 right rear. CVehicle::SetupLightFlags fills this from the
	// vehicle's m_renderLights before the clump is drawn, and
	// SetEditableMaterials reads it per material.
	static uint8 ms_lightsOn[NUM_LIGHTS];
	static int16 ms_numWheelUpgrades[NUM_WHEELS];
	static int16 ms_upgradeWheels[NUM_WHEELS][NUM_WHEEL_UPGRADES];
	static int32 ms_wheelFrameIDs[NUM_WHEELS];

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

	// SA CVehicleModelInfo::GetCustomCarPlateText (0x4C8970): the pending plate
	// text, or nil when this model has none.
	char *GetCustomCarPlateText(void) { return m_szPlateText[0] ? m_szPlateText : nil; }
	// SA CVehicleModelInfo::SetCustomCarPlateText (0x4C8980).
	void SetCustomCarPlateText(const char *text);
	// SA CVehicleModelInfo::SetCarCustomPlate (0x4C9450): find the model's plate
	// material and give it a freshly generated text.
	void SetCarCustomPlate(void);
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

	// ---- Tuning (SA CVehicleModelInfo upgrade API) ----------------------
	//
	// SA CVehicleModelInfo::IsUpgradeAvailable (0x4C8200). True when this
	// model has geometry for the given upgrade slot, i.e. the slot was seen in
	// the model's own hierarchy and the parent node is known.
	bool IsUpgradeAvailable(eVehicleUpgradePosn upgrade) const
	{
		return m_vehicleStruct.IsUpgradeSlotUsed(upgrade);
	}

	// SA CVehicleModelInfo::AddWheelUpgrade (0x4C8700). One wheel model is
	// appended to the set carmods.dat assigns it to. There is no bound check
	// in SA either, but the table is 15 entries per set and the shipped file
	// never exceeds it, so an over-long line is dropped instead of overrunning.
	static void AddWheelUpgrade(int32 wheelSetNumber, int32 modelId);
	// SA CVehicleModelInfo::GetWheelUpgrade (0x4C8750).
	static int32 GetWheelUpgrade(int32 wheelSetNumber, int32 wheelUpgradeNumber);
	// SA CVehicleModelInfo::GetNumWheelUpgrades (0x4C8740).
	static int32 GetNumWheelUpgrades(int32 wheelSetNumber);

	// SA CVehicleModelInfo::GetNumRemaps (0x4C86B0): how many paintjobs the
	// model can wear, counted up to the first -1 entry.
	int32 GetNumRemaps(void) const;
	// SA CVehicleModelInfo::AddRemap (0x4C86D0): remember a remap texture
	// dictionary slot for this model.
	void AddRemap(int32 txd);
	// SA CVehicleModelInfo::AssignRemapTxd (0x4C9360). A file called
	// `remap<name><n>.txd` belongs to the vehicle whose model name is
	// `<name>`; the trailing digits are the paintjob number.
	static void AssignRemapTxd(const char *name, int16 txdSlot);

	// SA CVehicleModelInfo::LoadVehicleUpgrades (0x5B65A0). Reads
	// DATA\CARMODS.DAT: the `mods` section fills m_anUpgrades, `wheel` fills
	// the wheel sets, `link` fills ms_linkedUpgrades. Called once, after the
	// IDE files have created every model, because it resolves model names to
	// ids.
	static void LoadVehicleUpgrades(void);

	// SA CVehicleModelInfo::SetVehicleColour (0x4C84B0) publishes the colour
	// of the car being drawn into ms_currentCol, which the material callback
	// reads. This overload only does the publishing half; the material
	// recolour half is the two/four-argument version below.
	static void SetCurrentColours(uint8 c1, uint8 c2, uint8 c3, uint8 c4);

	// The four-colour equivalents of the two-colour list. A model that never
	// had third/fourth slots keeps them empty, so these are safe on VC data.
	void GetCurrentColours(uint8 &c1, uint8 &c2, uint8 &c3, uint8 &c4) const
	{
		c1 = m_currentColour1; c2 = m_currentColour2;
		c3 = m_currentColour3; c4 = m_currentColour4;
	}

	static RwFrame *CollapseFramesCB(RwFrame *frame, void *data);
	// instantiates the separate VC wheel model on a wheel dummy frame
	// (used by VC models and by SA models that carry no wheel mesh at all)
	void AddGenericWheelModel(RwFrame *frame);
#ifdef SA_VEHICLE_MODELS
	// GTA:SA models contain their wheel meshes; this clones the single "wheel"
	// mesh of such models onto all four wheel dummies
	void CloneSAWheelMeshes(RwFrame **wheelFrames, int32 numWheels);
	void PreprocessSACarHierarchy(void);
	// SA CVehicleModelInfo::PreprocessHierarchy (0x4C8E60), upgrade half: for
	// every eVehicleUpgradePosn this model has a frame for, remember the node
	// the part hangs on and the offset from it. Called once per model from
	// PreprocessSACarHierarchy, so the fitting code has somewhere to attach a
	// bought part without walking the clump again.
	void SetupUpgradeSlots(void);
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

	// Passed to the atomic renderer callbacks below. `clump` is the clump the
	// atomics are being walked in, `saHierarchy` says whether this model uses the
	// SA node naming convention. Both used to be reachable from the callbacks
	// only through a file static and a cast of the data pointer; carrying them
	// explicitly means loading one model can never observe another's state.
	struct AtomicRendererCBData {
		RpClump *clump;
		bool saHierarchy;
	};
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
	// `saHierarchy` is this model's naming convention; it is passed in rather
	// than read from the instance so the two callers (SetClump and the SA car
	// path) stay explicit about which hierarchy they are walking.
	void PreprocessHierarchy(bool saHierarchy);
	void GetWheelPosn(int32 n, CVector &pos);
	const CVector &GetFrontSeatPosn(void) { return m_nVehicleType == VEHICLE_TYPE_BOAT ? m_positions[BOAT_POS_FRONTSEAT] : m_positions[CAR_POS_FRONTSEAT]; }

	int32 ChooseComponent(void);
	int32 ChooseSecondComponent(void);

	static RpMaterial *GetEditableMaterialListCB(RpMaterial *material, void *data);
	static RpAtomic *GetEditableMaterialListCB(RpAtomic *atomic, void *data);
	void FindEditableMaterialList(void);

	// ---- Editable materials: paintjob and lamps -------------------------
	//
	// SA CVehicleModelInfo::SetEditableMaterials (0x4C8430) walks the clump
	// once per drawn vehicle and rewrites the materials that carry state which
	// is not in the model:
	//
	//   * a body material (its texture name starts with '#') is pointed at the
	//     paintjob texture of the car being drawn;
	//   * a lamp material (its texture is one of the two lamp sheets) has its
	//     RGBA reset to white, so the four marker colours used by the modeller
	//     to say "this is the left front lamp" do not tint the sheet, and is
	//     then swapped to the lit sheet when that particular lamp is on.
	//
	// Everything that was written is recorded so that
	// ResetEditableMaterials (0x4C8460) can put the model back: the model is
	// shared by every vehicle of that model, and one car's headlights must not
	// stay on its next owner's.
	//
	// SA keeps the restore list on the stack (gRestoreEntries) and reuses it
	// for everything the callbacks changed, the body colour included. reVC
	// keeps a list per model instead, because a Render is not always bracketed
	// by the same call: the sorted alpha list can draw a vehicle later.
	void SetEditableMaterials(void);
	void RestoreEditableMaterials(void);
	static RpMaterial *SetEditableMaterialsCB(RpMaterial *material, void *data);
	static RpAtomic *SetEditableMaterialsAtomicCB(RpAtomic *atomic, void *data);

	// SA CVehicleModelInfo::SetupLightFlags (0x4C8C90). Copies the four lamp
	// flags of the vehicle being drawn into ms_lightsOn, which is the only
	// thing SetEditableMaterialsCB consults. Called from the vehicle's render
	// setup, before the clump is drawn.
	static void SetupLightFlags(CVehicle *vehicle);

	// Resolves ms_pLightsTexture / ms_pLightsOnTexture from vehicle.txd. SA
	// does this once in SetupCommonData (0x4C75E0, right after the dictionary
	// itself is loaded); reVC calls it from CCustomModels after the dictionary
	// has been created, and again whenever the dictionary is rebuilt.
	static void LoadLightTextures(void);
	// SA CVehicleModelInfo::ShutdownLightTexture (0x4C7470).
	static void ShutdownLightTexture(void);

	// The material is one of the two lamp sheets: SA decides by texture
	// pointer, the name is checked as well so that a car carrying its own copy
	// of the sheet inside its own TXD is still recognised.
	static bool IsLightsMaterial(RpMaterial *material);

	// The lit/unlit surface properties of a lamp. SA keeps these in one
	// RwSurfaceProperties at 0x8A645C and copies it over the material's own
	// while the lamp is lit; the ambient term is what makes the lit sheet read
	// as a lamp rather than as a bright sticker under a street light.
	static const RwSurfaceProperties &GetLightsSurfaceProperties(void);

	// SA CVehicleModelInfo::SetRenderPipelines (0x4C8900). The environment-map
	// pipeline caches the material's texture, so a remap or lamp swap has to
	// re-setup the pipeline or the car keeps the old skin.
	void SetRenderPipelines(void);

	// One restored material field. SA's tRestoreEntry stores the *address* of
	// the field and the value it had, so one entry per written field; the enum
	// below is what this fork stores instead, because the fields a vehicle
	// material has are few and known, and a tagged entry keeps the list
	// readable in a debugger.
	struct RemapEntry
	{
		RpMaterial *material;
		// Texture the material had before the swap (paintjob or lit sheet).
		RwTexture *texture;
		// Colour the material had before it was reset to white. Only written
		// when the colour was changed.
		RwRGBA colour;
		// Surface properties the material had before the lit-sheet swap.
		RwSurfaceProperties surfaceProps;
		uint8 swappedTexture : 1;
		uint8 changedColour : 1;
		uint8 changedSurfaceProps : 1;
	};
	// SA's gRestoreEntries is 256 entries; a car has one entry per body
	// material and one per lit lamp, so the same ceiling is kept here.
	enum { NUM_REMAP_ENTRIES = 256 };
	RemapEntry m_aRemapEntries[NUM_REMAP_ENTRIES];
	int32 m_nNumRemapEntries;
	void SetVehicleColour(uint8 c1, uint8 c2);
	void SetVehicleColour(uint8 c1, uint8 c2, uint8 c3, uint8 c4);
	void ChooseVehicleColour(uint8 &col1, uint8 &col2);
	// SA CVehicleModelInfo::ChooseVehicleColour (0x4C8500) takes four channels
	// and a variation shift. carcols.dat lines for SA cars carry two pairs per
	// line; a car whose line has only one pair repeats it, which is why the
	// third and fourth fall back to the first two instead of staying black.
	void ChooseVehicleColour(uint8 &col1, uint8 &col2, uint8 &col3, uint8 &col4, int32 variationShift);
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
