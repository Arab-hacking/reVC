#include "common.h"

#include "General.h"
#include "TempColModels.h"
#include "ModelIndices.h"
#include "ModelInfo.h"

CBaseModelInfo *CModelInfo::ms_modelInfoPtrs[MODELINFOSIZE];

CModelInfoStore<CSimpleModelInfo, SIMPLEMODELSIZE> CModelInfo::ms_simpleModelStore;
CModelInfoStore<CTimeModelInfo, TIMEMODELSIZE> CModelInfo::ms_timeModelStore;
CModelInfoStore<CWeaponModelInfo, WEAPONMODELSIZE> CModelInfo::ms_weaponModelStore;
CModelInfoStore<CClumpModelInfo, CLUMPMODELSIZE> CModelInfo::ms_clumpModelStore;
CModelInfoStore<CPedModelInfo, PEDMODELSIZE> CModelInfo::ms_pedModelStore;
CModelInfoStore<CVehicleModelInfo, VEHICLEMODELSIZE> CModelInfo::ms_vehicleModelStore;
CModelInfoStore<C2dEffect, TWODFXSIZE> CModelInfo::ms_2dEffectStore;

void
CModelInfo::Initialise(void)
{
	int i;
	CSimpleModelInfo *m;

	debug("sizeof SimpleModelStore %d\n", sizeof(ms_simpleModelStore));
	debug("sizeof TimeModelStore %d\n", sizeof(ms_timeModelStore));
	debug("sizeof WeaponModelStore %d\n", sizeof(ms_weaponModelStore));
	debug("sizeof ClumpModelStore %d\n", sizeof(ms_clumpModelStore));
	debug("sizeof VehicleModelStore %d\n", sizeof(ms_vehicleModelStore));
	debug("sizeof PedModelStore %d\n", sizeof(ms_pedModelStore));
	debug("sizeof 2deffectsModelStore %d\n", sizeof(ms_2dEffectStore));

	for(i = 0; i < MODELINFOSIZE; i++)
		ms_modelInfoPtrs[i] = nil;
	ms_2dEffectStore.Clear();
	ms_simpleModelStore.Clear();
	ms_timeModelStore.Clear();
	ms_weaponModelStore.Clear();
	ms_clumpModelStore.Clear();
	ms_pedModelStore.Clear();
	ms_vehicleModelStore.Clear();

	m = AddSimpleModel(MI_CAR_DOOR);
	m->SetColModel(&CTempColModels::ms_colModelDoor1);
	m->SetTexDictionary("generic");
	m->SetNumAtomics(1);
	m->m_lodDistances[0] = 80.0f;

	m = AddSimpleModel(MI_CAR_BUMPER);
	m->SetColModel(&CTempColModels::ms_colModelBumper1);
	m->SetTexDictionary("generic");
	m->SetNumAtomics(1);
	m->m_lodDistances[0] = 80.0f;

	m = AddSimpleModel(MI_CAR_PANEL);
	m->SetColModel(&CTempColModels::ms_colModelPanel1);
	m->SetTexDictionary("generic");
	m->SetNumAtomics(1);
	m->m_lodDistances[0] = 80.0f;

	m = AddSimpleModel(MI_CAR_BONNET);
	m->SetColModel(&CTempColModels::ms_colModelBonnet1);
	m->SetTexDictionary("generic");
	m->SetNumAtomics(1);
	m->m_lodDistances[0] = 80.0f;

	m = AddSimpleModel(MI_CAR_BOOT);
	m->SetColModel(&CTempColModels::ms_colModelBoot1);
	m->SetTexDictionary("generic");
	m->SetNumAtomics(1);
	m->m_lodDistances[0] = 80.0f;

	m = AddSimpleModel(MI_CAR_WHEEL);
	m->SetColModel(&CTempColModels::ms_colModelWheel1);
	m->SetTexDictionary("generic");
	m->SetNumAtomics(1);
	m->m_lodDistances[0] = 80.0f;

	m = AddSimpleModel(MI_BODYPARTA);
	m->SetColModel(&CTempColModels::ms_colModelBodyPart1);
	m->SetTexDictionary("generic");
	m->SetNumAtomics(1);
	m->m_lodDistances[0] = 80.0f;

	m = AddSimpleModel(MI_BODYPARTB);
	m->SetColModel(&CTempColModels::ms_colModelBodyPart2);
	m->SetTexDictionary("generic");
	m->SetNumAtomics(1);
	m->m_lodDistances[0] = 80.0f;
}

void
CModelInfo::ShutDown(void)
{
	int i;
	for(i = 0; i < ms_simpleModelStore.allocPtr; i++)
		ms_simpleModelStore.GetItem(i)->Shutdown();
	for(i = 0; i < ms_timeModelStore.allocPtr; i++)
		ms_timeModelStore.GetItem(i)->Shutdown();
	for(i = 0; i < ms_weaponModelStore.allocPtr; i++)
		ms_weaponModelStore.GetItem(i)->Shutdown();
	for(i = 0; i < ms_clumpModelStore.allocPtr; i++)
		ms_clumpModelStore.GetItem(i)->Shutdown();
	for(i = 0; i < ms_vehicleModelStore.allocPtr; i++)
		ms_vehicleModelStore.GetItem(i)->Shutdown();
	for(i = 0; i < ms_pedModelStore.allocPtr; i++)
		ms_pedModelStore.GetItem(i)->Shutdown();
	for(i = 0; i < ms_2dEffectStore.allocPtr; i++)
		ms_2dEffectStore.GetItem(i)->Shutdown();

	ms_2dEffectStore.Clear();
	ms_simpleModelStore.Clear();
	ms_timeModelStore.Clear();
	ms_weaponModelStore.Clear();
	ms_pedModelStore.Clear();
	ms_clumpModelStore.Clear();
	ms_vehicleModelStore.Clear();
}

CSimpleModelInfo*
CModelInfo::AddSimpleModel(int id)
{
	CSimpleModelInfo *modelinfo;
	modelinfo = CModelInfo::ms_simpleModelStore.Alloc();
	CModelInfo::ms_modelInfoPtrs[id] = modelinfo;
	modelinfo->Init();
	return modelinfo;
}

CTimeModelInfo*
CModelInfo::AddTimeModel(int id)
{
	CTimeModelInfo *modelinfo;
	modelinfo = CModelInfo::ms_timeModelStore.Alloc();
	CModelInfo::ms_modelInfoPtrs[id] = modelinfo;
	modelinfo->Init();
	return modelinfo;
}

CWeaponModelInfo*
CModelInfo::AddWeaponModel(int id)
{
	CWeaponModelInfo *modelinfo;
	modelinfo = CModelInfo::ms_weaponModelStore.Alloc();
	CModelInfo::ms_modelInfoPtrs[id] = modelinfo;
	modelinfo->Init();
	return modelinfo;
}

CClumpModelInfo*
CModelInfo::AddClumpModel(int id)
{
	CClumpModelInfo *modelinfo;
	modelinfo = CModelInfo::ms_clumpModelStore.Alloc();
	CModelInfo::ms_modelInfoPtrs[id] = modelinfo;
	modelinfo->m_clump = nil;
	modelinfo->m_animFileIndex = -1;
	return modelinfo;
}

CPedModelInfo*
CModelInfo::AddPedModel(int id)
{
	CPedModelInfo *modelinfo;
	modelinfo = CModelInfo::ms_pedModelStore.Alloc();
	CModelInfo::ms_modelInfoPtrs[id] = modelinfo;
	modelinfo->m_clump = nil;
	// The base store is reused across new games without re-running its C++
	// constructors. Never reinterpret an old animation block index as a char*.
	modelinfo->m_animFileIndex = -1;
	return modelinfo;
}

CVehicleModelInfo*
CModelInfo::AddVehicleModel(int id)
{
	CVehicleModelInfo *modelinfo;
	modelinfo = CModelInfo::ms_vehicleModelStore.Alloc();
	CModelInfo::ms_modelInfoPtrs[id] = modelinfo;
	modelinfo->m_clump = nil;
	modelinfo->CClumpModelInfo::m_animFileIndex = -1;
	modelinfo->m_animFileIndex = -1; // separate CVehicleModelInfo anim union
	modelinfo->m_numColours = 0;
	modelinfo->m_lastColorVariation = 0;
	modelinfo->m_bSAWheelMesh = false;
#ifdef SA_VEHICLE_MODELS
	modelinfo->m_bHasEmbeddedColModel = false;
#endif
	for(int i = 0; i < NUM_VEHICLE_POSITIONS; i++)
		modelinfo->m_positions[i] = CVector(0.0f, 0.0f, 0.0f);
	modelinfo->m_vehicleType = -1;
	modelinfo->m_wheelId = -1;
	modelinfo->m_numComps = 0;
	modelinfo->m_numDoors = 0;
	modelinfo->m_materials1[0] = nil;
	modelinfo->m_materials2[0] = nil;
	modelinfo->m_bikeSteerAngle = 999.99f;
	return modelinfo;
}

CBaseModelInfo*
CModelInfo::GetModelInfo(const char *name, int *id)
{
	CBaseModelInfo *modelinfo;
	for(int i = 0; i < MODELINFOSIZE; i++){
		modelinfo = CModelInfo::ms_modelInfoPtrs[i];
	 	if(modelinfo && !CGeneral::faststricmp(modelinfo->GetModelName(), name)){
			if(id)
				*id = i;
			return modelinfo;
		}
	}
	return nil;
}

CBaseModelInfo*
CModelInfo::GetModelInfo(const char *name, int minIndex, int maxIndex)
{
	if (minIndex > maxIndex)
		return 0;

	CBaseModelInfo *modelinfo;
	for(int i = minIndex; i <= maxIndex; i++){
		modelinfo = CModelInfo::ms_modelInfoPtrs[i];
	 	if(modelinfo && !CGeneral::faststricmp(modelinfo->GetModelName(), name))
			return modelinfo;
	}
	return nil;
}

bool
CModelInfo::IsBoatModel(int32 id)
{
	return GetModelInfo(id)->GetModelType() == MITYPE_VEHICLE &&
		((CVehicleModelInfo*)GetModelInfo(id))->m_vehicleType == VEHICLE_TYPE_BOAT;
}

bool
CModelInfo::IsBikeModel(int32 id)
{
	return GetModelInfo(id)->GetModelType() == MITYPE_VEHICLE &&
		((CVehicleModelInfo*)GetModelInfo(id))->m_vehicleType == VEHICLE_TYPE_BIKE;
}

bool
CModelInfo::IsCarModel(int32 id)
{
	return GetModelInfo(id)->GetModelType() == MITYPE_VEHICLE &&
		((CVehicleModelInfo*)GetModelInfo(id))->m_vehicleType == VEHICLE_TYPE_CAR;
}

bool
CModelInfo::IsHeliModel(int32 id)
{
	return GetModelInfo(id)->GetModelType() == MITYPE_VEHICLE &&
		((CVehicleModelInfo*)GetModelInfo(id))->m_vehicleType == VEHICLE_TYPE_HELI;
}

bool
CModelInfo::IsPlaneModel(int32 id)
{
	return GetModelInfo(id)->GetModelType() == MITYPE_VEHICLE &&
		((CVehicleModelInfo*)GetModelInfo(id))->m_vehicleType == VEHICLE_TYPE_PLANE;
}

void
CModelInfo::ReInit2dEffects()
{
	ms_2dEffectStore.Clear();

	for (int i = 0; i < MODELINFOSIZE; i++) {
		if (ms_modelInfoPtrs[i])
			ms_modelInfoPtrs[i]->Init2dEffects();
	}
}
