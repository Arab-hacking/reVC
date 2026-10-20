#ifndef __GTA_MODELINFO_H__
#define __GTA_MODELINFO_H__

#include "2dEffect.h"
#include "SimpleModelInfo.h"
#include "MloModelInfo.h"
#include "TimeModelInfo.h"
#include "WeaponModelInfo.h"
#include "ClumpModelInfo.h"
#include "PedModelInfo.h"
#include "VehicleModelInfo.h"
#include "templates.h"

#ifdef CUSTOM_MODELS
// Model IDs and effect offsets are 16-bit; grow model-info storage without
// moving previously registered infos, up to those representable limits.
template<typename T, int32 n>
using CModelInfoStore = CStore<T, n, 256, MODELINFOSIZE>;
#else
template<typename T, int32 n>
using CModelInfoStore = CStore<T, n>;
#endif

class CModelInfo
{
	static CBaseModelInfo *ms_modelInfoPtrs[MODELINFOSIZE];
	static CModelInfoStore<CSimpleModelInfo, SIMPLEMODELSIZE> ms_simpleModelStore;
	static CModelInfoStore<CTimeModelInfo, TIMEMODELSIZE> ms_timeModelStore;
	static CModelInfoStore<CWeaponModelInfo, WEAPONMODELSIZE> ms_weaponModelStore;
	static CModelInfoStore<CClumpModelInfo, CLUMPMODELSIZE> ms_clumpModelStore;
	static CModelInfoStore<CPedModelInfo, PEDMODELSIZE> ms_pedModelStore;
	static CModelInfoStore<CVehicleModelInfo, VEHICLEMODELSIZE> ms_vehicleModelStore;
	static CModelInfoStore<C2dEffect, TWODFXSIZE> ms_2dEffectStore;

public:
	static void Initialise(void);
	static void ShutDown(void);

	static bool CanAddSimpleModel(void) { return ms_simpleModelStore.CanAlloc(); }
	static bool CanAddTimeModel(void) { return ms_timeModelStore.CanAlloc(); }
	static CSimpleModelInfo *AddSimpleModel(int id);
	static CTimeModelInfo *AddTimeModel(int id);
	static CWeaponModelInfo *AddWeaponModel(int id);
	static CClumpModelInfo *AddClumpModel(int id);
	static CPedModelInfo *AddPedModel(int id);
	static CVehicleModelInfo *AddVehicleModel(int id);

	static CModelInfoStore<C2dEffect, TWODFXSIZE> &Get2dEffectStore(void) { return ms_2dEffectStore; }

	static CBaseModelInfo *GetModelInfo(const char *name, int *id);
	static CBaseModelInfo *GetModelInfo(int id){
		return ms_modelInfoPtrs[id];
	}
	static CBaseModelInfo *GetModelInfo(const char *name, int minIndex, int maxIndex);
	static CColModel *GetColModel(int id){
		return ms_modelInfoPtrs[id]->GetColModel();
	}

	static bool IsBoatModel(int32 id);
	static bool IsBikeModel(int32 id);
	static bool IsCarModel(int32 id);
	static bool IsHeliModel(int32 id);
	static bool IsPlaneModel(int32 id);
	static void ReInit2dEffects();
};

#endif // __GTA_MODELINFO_H__
