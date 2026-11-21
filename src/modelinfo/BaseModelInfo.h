#ifndef __GTA_BASEMODELINFO_H__
#define __GTA_BASEMODELINFO_H__

struct CColModel;

#ifdef ANDROID
#define MAX_MODEL_NAME (24)
#else
#define MAX_MODEL_NAME (21)
#endif

enum ModelInfoType
{
	MITYPE_NA,
	MITYPE_SIMPLE,
	MITYPE_MLO,	// unused but still in enum
	MITYPE_TIME,
	MITYPE_WEAPON,
	MITYPE_CLUMP,
	MITYPE_VEHICLE,
	MITYPE_PED,
	MITYPE_XTRACOMPS,	// unused but still in enum
	MITYPE_HAND	// xbox and mobile
};

class C2dEffect;

// SA eVehicleMod (BaseModelInfo.h). Which kind of vehicle part a model is,
// used as the CarMod field of a bought part: it tells the fitting code which
// panel the part belongs to and which panel it repairs when bought.
enum eVehicleMod
{
	VEHICLE_MOD_UPGRADE_BONNET            = 0,
	VEHICLE_MOD_UPGRADE_BONNET_LEFT_RIGHT = 1,
	VEHICLE_MOD_UPGRADE_SPOILER           = 2,
	VEHICLE_MOD_UPGRADE_WING              = 3,
	VEHICLE_MOD_UPGRADE_FRONT_BULLBAR     = 4,
	VEHICLE_MOD_UPGRADE_REAR_BULLBAR      = 5,
	VEHICLE_MOD_UPGRADE_FRONT_LIGHTS      = 6,
	VEHICLE_MOD_UPGRADE_ROOF              = 7,
	VEHICLE_MOD_UPGRADE_NITRO             = 8,
	VEHICLE_MOD_UPGRADE_HYDRAULICS        = 9,
	VEHICLE_MOD_UPGRADE_STEREO            = 10,

	VEHICLE_MOD_REPLACEMENT_CHASSIS       = 11,
	VEHICLE_MOD_REPLACEMENT_WHEEL         = 12,
	VEHICLE_MOD_REPLACEMENT_EXHAUST       = 13,
	VEHICLE_MOD_REPLACEMENT_FRONT_BUMPER  = 14,
	VEHICLE_MOD_REPLACEMENT_REAR_BUMPER   = 15,
	VEHICLE_MOD_REPLACEMENT_MISC          = 16,

	VEHICLE_MOD_NONE                      = 0xFF
};

class CBaseModelInfo
{
protected:
	char         m_name[MAX_MODEL_NAME];
	uint8        m_type;
	uint8        m_num2dEffects;
	bool         m_bOwnsColModel;
	CColModel   *m_colModel;
	int16        m_2dEffectsID;
	int16        m_objectId;
	uint16       m_refCount;
	int16        m_txdSlot;

public:
	// SA CBaseModelInfo::bUseCommonVehicleDictionary / CarMod / bUsesVehDummy.
	// A model that is a vehicle part rather than a world object: which kind of
	// part it is (eVehicleMod) and whether it hangs on the dummy of a node
	// instead of on the chassis slot it names. Set by
	// SetupVehicleUpgradeFlags from the model's name, exactly as SA does.
	uint8        m_nVehicleMod;
	bool         m_bUsesVehDummy;

public:
	CBaseModelInfo(ModelInfoType type);
	virtual ~CBaseModelInfo() {}
	virtual void Shutdown(void);
	virtual void DeleteRwObject(void) = 0;
	virtual RwObject *CreateInstance(void) = 0;
	virtual RwObject *CreateInstance(RwMatrix *) = 0;
	virtual RwObject *GetRwObject(void) = 0;
	virtual void SetAnimFile(const char *file) {}
	virtual void ConvertAnimFileIndex(void) {}
	virtual int GetAnimFileIndex(void) { return -1; }

	// one day it becomes virtual
	uint8 GetModelType() const { return m_type; }
	bool IsBuilding(void) { return m_type == MITYPE_SIMPLE || m_type == MITYPE_TIME; }
	bool IsSimple(void) { return m_type == MITYPE_SIMPLE || m_type == MITYPE_TIME || m_type == MITYPE_WEAPON; }
	bool IsClump(void) { return m_type == MITYPE_CLUMP || m_type == MITYPE_PED || m_type == MITYPE_VEHICLE;	}
	char *GetModelName(void) { return m_name; }
	void SetModelName(const char *name) { strncpy(m_name, name, MAX_MODEL_NAME - 1); m_name[MAX_MODEL_NAME - 1] = '\0'; }
	void SetColModel(CColModel *col, bool owns = false){
		m_colModel = col; m_bOwnsColModel = owns; }
	CColModel *GetColModel(void) { return m_colModel; }
	bool DoesOwnColModel(void) { return m_bOwnsColModel; }
	void DeleteCollisionModel(void);
	void ClearTexDictionary(void) { m_txdSlot = -1; }
	int16 GetObjectID(void) { return m_objectId; }
	void SetObjectID(int16 id) { m_objectId = id; }
	int16 GetTxdSlot(void) { return m_txdSlot; }
	void AddRef(void);
	void RemoveRef(void);
	void SetTexDictionary(const char *name);
	void AddTexDictionaryRef(void);
	void RemoveTexDictionaryRef(void);
	// SA CAtomicModelInfo::SetupVehicleUpgradeFlags (0x4C54C0). Classifies a
	// vehicle part model from its name: which eVehicleMod it is and whether it
	// hangs on a node dummy. Called by CVehicleModelInfo::LoadVehicleUpgrades
	// for every name carmods.dat lists.
	void SetupVehicleUpgradeFlags(const char *name);
	void Init2dEffects(void);
	void Add2dEffect(C2dEffect *fx);
	C2dEffect *Get2dEffect(int n);
	uint8 GetNum2dEffects() const { return m_num2dEffects; }
	uint16 GetNumRefs() const { return m_refCount; }
};

#endif // __GTA_BASEMODELINFO_H__
