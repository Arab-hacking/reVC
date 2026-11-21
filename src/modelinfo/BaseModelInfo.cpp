#include "common.h"

#include "templates.h"
#include "TxdStore.h"
#include "2dEffect.h"
#include "BaseModelInfo.h"
#include "ModelInfo.h"
#include "ColModel.h"

CBaseModelInfo::CBaseModelInfo(ModelInfoType type)
{
	m_colModel = nil;
	m_2dEffectsID = -1;
	m_objectId = -1;
	m_refCount = 0;
	m_txdSlot = -1;
	m_type = type;
	m_num2dEffects = 0;
	m_bOwnsColModel = false;
	m_nVehicleMod = VEHICLE_MOD_NONE;
	m_bUsesVehDummy = false;
}

void
CBaseModelInfo::Shutdown(void)
{
	DeleteCollisionModel();
	DeleteRwObject();
	m_2dEffectsID = -1;
	m_num2dEffects = 0;
	m_txdSlot = -1;
	m_objectId = -1;
}

void
CBaseModelInfo::DeleteCollisionModel(void)
{
	if(m_colModel && m_bOwnsColModel){
		if(m_colModel)
			delete m_colModel;
		m_colModel = nil;
	}
}

void
CBaseModelInfo::AddRef(void)
{
	m_refCount++;
	AddTexDictionaryRef();
}

void
CBaseModelInfo::RemoveRef(void)
{
	m_refCount--;
	RemoveTexDictionaryRef();
}

void
CBaseModelInfo::SetTexDictionary(const char *name)
{
	int slot = CTxdStore::FindTxdSlot(name);
	if(slot == -1)
		slot = CTxdStore::AddTxdSlot(name);
	m_txdSlot = slot;
}

void
CBaseModelInfo::AddTexDictionaryRef(void)
{
	CTxdStore::AddRef(m_txdSlot);
}

void
CBaseModelInfo::RemoveTexDictionaryRef(void)
{
	CTxdStore::RemoveRef(m_txdSlot);
}

void
CBaseModelInfo::Init2dEffects(void)
{
	m_2dEffectsID = -1;
	m_num2dEffects = 0;
}

void
CBaseModelInfo::Add2dEffect(C2dEffect *fx)
{
	if(m_2dEffectsID >= 0)
		m_num2dEffects++;
	else{
		m_2dEffectsID = CModelInfo::Get2dEffectStore().GetIndex(fx);
		m_num2dEffects = 1;
	}
}

C2dEffect*
CBaseModelInfo::Get2dEffect(int n)
{
	if(m_2dEffectsID >= 0)
		return CModelInfo::Get2dEffectStore().GetItem(m_2dEffectsID+n);
	else
		return nil;
}

// SA CAtomicModelInfo::SetupVehicleUpgradeFlags (0x4C54C0).
//
// carmods.dat names parts by their model name, and the name is the only thing
// that says what the part is: an exhaust starts with `exh_`, a left wing panel
// with `wg_l_`, hydraulics and the stereo are named in full. The two tables are
// SA's, prefix and slot id included - the ids are the eVehicleMod values the
// shop and the fitting code switch on, so they must not be renumbered.
//
// SA also sets bUseCommonVehicleDictionary here, which makes the part's
// geometry come from the shared vehicle dictionary. reVC has one dictionary per
// model, so that part has no equivalent; what matters is the kind of part and
// whether it hangs on a dummy node.
void
CBaseModelInfo::SetupVehicleUpgradeFlags(const char *name)
{
	static const struct { const char *name; uint8 mod; } aDummyComps[] = {
		{ "chss_",   VEHICLE_MOD_REPLACEMENT_CHASSIS },
		{ "wheel_",  VEHICLE_MOD_REPLACEMENT_WHEEL },
		{ "exh_",    VEHICLE_MOD_REPLACEMENT_EXHAUST },
		{ "fbmp_",   VEHICLE_MOD_REPLACEMENT_FRONT_BUMPER },
		{ "rbmp_",   VEHICLE_MOD_REPLACEMENT_REAR_BUMPER },
		{ "misc_a_", VEHICLE_MOD_REPLACEMENT_MISC },
		{ "misc_b_", VEHICLE_MOD_REPLACEMENT_MISC + 1 },
		{ "misc_c_", VEHICLE_MOD_REPLACEMENT_MISC + 2 },
		{ nil,       VEHICLE_MOD_NONE }
	};

	static const struct { const char *name; uint8 mod; } aChassisComps[] = {
		{ "bnt_",      VEHICLE_MOD_UPGRADE_BONNET },
		{ "bntl_",     VEHICLE_MOD_UPGRADE_BONNET_LEFT_RIGHT },
		{ "bntr_",     VEHICLE_MOD_UPGRADE_BONNET_LEFT_RIGHT },
		{ "spl_",      VEHICLE_MOD_UPGRADE_SPOILER },
		{ "wg_l_",     VEHICLE_MOD_UPGRADE_WING },
		{ "wg_r",      VEHICLE_MOD_UPGRADE_WING },
		{ "fbb_",      VEHICLE_MOD_UPGRADE_FRONT_BULLBAR },
		{ "bbb_",      VEHICLE_MOD_UPGRADE_REAR_BULLBAR },
		{ "lgt_",      VEHICLE_MOD_UPGRADE_FRONT_LIGHTS },
		{ "rf_",       VEHICLE_MOD_UPGRADE_ROOF },
		{ "nto_",      VEHICLE_MOD_UPGRADE_NITRO },
		{ "hydralics", VEHICLE_MOD_UPGRADE_HYDRAULICS },
		{ "stereo",    VEHICLE_MOD_UPGRADE_STEREO },
		{ nil,         VEHICLE_MOD_NONE }
	};

	if(name == nil || name[0] == '\0')
		return;

	for(int32 i = 0; aChassisComps[i].name; i++){
		if(strncmp(aChassisComps[i].name, name, strlen(aChassisComps[i].name)) != 0)
			continue;
		m_nVehicleMod = aChassisComps[i].mod;
		m_bUsesVehDummy = false;
		return;
	}

	for(int32 i = 0; aDummyComps[i].name; i++){
		if(strncmp(aDummyComps[i].name, name, strlen(aDummyComps[i].name)) != 0)
			continue;
		m_nVehicleMod = aDummyComps[i].mod;
		// These parts hang on a node of the car rather than on the chassis
		// slot their name spells out; the fitting code has to look the node up.
		m_bUsesVehDummy = true;
		return;
	}
}
