#include "common.h"

#include "Pools.h"

#include "Bike.h"
#include "Bmx.h"
#include "Boat.h"
#include "MonsterTruck.h"
#include "QuadBike.h"
#include "Trailer.h"
#include "VehicleModelInfo.h"
#include "CarCtrl.h"
#include "Population.h"
#include "ProjectileInfo.h"
#include "Streaming.h"
#include "Wanted.h"
#include "World.h"
#include "MemoryHeap.h"

CCPtrNodePool *CPools::ms_pPtrNodePool;
CEntryInfoNodePool *CPools::ms_pEntryInfoNodePool;
CPedPool *CPools::ms_pPedPool;
CVehiclePool *CPools::ms_pVehiclePool;
CBuildingPool *CPools::ms_pBuildingPool;
CTreadablePool *CPools::ms_pTreadablePool;
CObjectPool *CPools::ms_pObjectPool;
CDummyPool *CPools::ms_pDummyPool;
CAudioScriptObjectPool *CPools::ms_pAudioScriptObjectPool;
CColModelPool *CPools::ms_pColModelPool;

#if defined GTA_PS2 && !defined MASTER	// or USE_CUSTOM_ALLOCATOR
// not in VC. perhaps ifdef'ed away
#define CHECKMEM(msg) CMemCheck::AllocateMemCheckBlock(msg)
#else
#define CHECKMEM(msg)
#endif

// A size-only pool pass must never wrap a uint32 and trick the legacy-save
// preflight into allocating a buffer smaller than the serialized records.
static uint32
PoolSaveSize(uint64 bytes)
{
	return bytes > UINT32_MAX ? UINT32_MAX : (uint32)bytes;
}

void
CPools::Initialise(void)
{
	PUSH_MEMID(MEMID_POOLS);
	CHECKMEM("before pools");
	// Keep VC's initial footprint; every gameplay pool grows in stable blocks
	// as needed, even in builds without the custom model/asset loader. Pointers
	// held by world sectors, scripts and live entities must not be relocated.
	ms_pPtrNodePool = new CCPtrNodePool(NUMPTRNODES, "PtrNode", 65536);
	CHECKMEM("after CPtrNodePool");
	ms_pEntryInfoNodePool = new CEntryInfoNodePool(NUMENTRYINFOS, "EntryInfoNode", 2048);
	ms_pPedPool = new CPedPool(NUMPEDS, "Peds", 64);
	ms_pVehiclePool = new CVehiclePool(NUMVEHICLES, "Vehicles", 64);
	CHECKMEM("after CEntryInfoNodePool");
	CHECKMEM("after CPedPool");
	CHECKMEM("after CVehiclePool");
	ms_pBuildingPool = new CBuildingPool(NUMBUILDINGS, "Buildings", 8192);
	CHECKMEM("after CBuildingPool");
	ms_pTreadablePool = new CTreadablePool(NUMTREADABLES, "Treadables", 64);
	ms_pObjectPool = new CObjectPool(NUMOBJECTS, "Objects", 256);
	ms_pDummyPool = new CDummyPool(NUMDUMMIES, "Dummys", 512);
	ms_pAudioScriptObjectPool = new CAudioScriptObjectPool(NUMAUDIOSCRIPTOBJECTS, "AudioScriptObj", 128);
	ms_pColModelPool = new CColModelPool(NUMCOLMODELS, "ColModel", 4096);
	CHECKMEM("after CTreadablePool");
	CHECKMEM("after CObjectPool");
	CHECKMEM("after CDummyPool");
	CHECKMEM("after cAudioScriptObjectPool");
	CHECKMEM("after pools");
	POP_MEMID();
}

void
CPools::ShutDown(void)
{
	debug("PtrNodes left %d\n", ms_pPtrNodePool->GetNoOfUsedSpaces());
	debug("EntryInfoNodes left %d\n", ms_pEntryInfoNodePool->GetNoOfUsedSpaces());
	debug("Peds left %d\n", ms_pPedPool->GetNoOfUsedSpaces());
	debug("Vehicles left %d\n", ms_pVehiclePool->GetNoOfUsedSpaces());
	debug("Buildings left %d\n", ms_pBuildingPool->GetNoOfUsedSpaces());
	debug("Treadables left %d\n", ms_pTreadablePool->GetNoOfUsedSpaces());
	debug("Objects left %d\n", ms_pObjectPool->GetNoOfUsedSpaces());
	debug("Dummys left %d\n", ms_pDummyPool->GetNoOfUsedSpaces());
	debug("AudioScriptObjects left %d\n", ms_pAudioScriptObjectPool->GetNoOfUsedSpaces());
	debug("ColModels left %d\n", ms_pColModelPool->GetNoOfUsedSpaces());
	printf("Shutdown pool started\n");

	delete ms_pPtrNodePool;
	delete ms_pEntryInfoNodePool;
	delete ms_pPedPool;
	delete ms_pVehiclePool;
	delete ms_pBuildingPool;
	delete ms_pTreadablePool;
	delete ms_pObjectPool;
	delete ms_pDummyPool;
	delete ms_pAudioScriptObjectPool;
	delete ms_pColModelPool;

	printf("Shutdown pool done\n");
}

int32 CPools::GetPedRef(CPed *ped) { return ms_pPedPool->GetIndex(ped); }
CPed *CPools::GetPed(int32 handle) { return ms_pPedPool->GetAt(handle); }
int32 CPools::GetVehicleRef(CVehicle *vehicle) { return ms_pVehiclePool->GetIndex(vehicle); }
CVehicle *CPools::GetVehicle(int32 handle) { return ms_pVehiclePool->GetAt(handle); }
int32 CPools::GetObjectRef(CObject *object) { return ms_pObjectPool->GetIndex(object); }
CObject *CPools::GetObject(int32 handle) { return ms_pObjectPool->GetAt(handle); }

void
CPools::CheckPoolsEmpty()
{
	assert(ms_pPedPool->GetNoOfUsedSpaces() == 0);
	assert(ms_pVehiclePool->GetNoOfUsedSpaces() == 0);
	printf("pools have been cleared\n");
}

// Thankfully unused, it would break the game!
void
CPools::MakeSureSlotInObjectPoolIsEmpty(int32 slot)
{
	if (ms_pObjectPool->GetIsFree(slot)) return;

	CObject *object = ms_pObjectPool->GetSlot(slot);
	if (object->ObjectCreatedBy == TEMP_OBJECT) {
		CWorld::Remove(object);
		delete object;
	} else if (!CProjectileInfo::RemoveIfThisIsAProjectile(object)) {
		// relocate to another slot??
		CObject *newObject = new CObject(object->GetModelIndex(), false);
		CWorld::Remove(object);
#if 0 // todo better
		*newObject = *object;
#else
		memcpy(newObject, object, ms_pObjectPool->GetMaxEntrySize());
#endif
		CWorld::Add(newObject);
		object->m_rwObject = nil;
		delete object;
		newObject->m_pFirstReference = nil;
	}
}

