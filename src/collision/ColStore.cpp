#include "common.h"

#include "templates.h"
#include "General.h"
#include "ModelInfo.h"
#include "Streaming.h"
#include "FileLoader.h"
#include "Script.h"
#include "Timer.h"
#include "Camera.h"
#include "Frontend.h"
#include "Physical.h"
#include "ColStore.h"
#define COLSLOTSELECTION_IMPLEMENTATION
#include "ColSlotSelection.h"
#include "VarConsole.h"
#include "Pools.h"
#if defined(ANDROID) && defined(CUSTOM_MODELS)
#include "CustomModels.h"
#endif

CPool<ColDef,ColDef> *CColStore::ms_pColPool;
#ifndef MASTER
bool bDispColInMem;
#endif

void
CColStore::Initialise(void)
{
	if(ms_pColPool == nil)
		ms_pColPool = new CPool<ColDef,ColDef>(COLSTORESIZE, "CollisionFiles");
	AddColSlot("generic");	// slot 0. not streamed
#ifndef MASTER
	VarConsole.Add("Display collision in memory", &bDispColInMem, true);
#endif
}

void
CColStore::Shutdown(void)
{
	int i;
	for(i = 0; i < COLSTORESIZE; i++)
		RemoveColSlot(i);
	if(ms_pColPool)
		delete ms_pColPool;
	ms_pColPool = nil;
}

int
CColStore::AddColSlot(const char *name)
{
	ColDef *def = ms_pColPool->New();
	assert(def);
	def->isLoaded = false;
	def->unused = 0;
	def->bounds.left = 1000000.0f;
	def->bounds.top = 1000000.0f;
	def->bounds.right = -1000000.0f;
	def->bounds.bottom = -1000000.0f;
	def->minIndex = INT16_MAX;
	def->maxIndex = INT16_MIN;
	strcpy(def->name, name);
	return ms_pColPool->GetJustIndex(def);
}

void
CColStore::RemoveColSlot(int slot)
{
	if(GetSlot(slot)){
		if(GetSlot(slot)->isLoaded)
			RemoveCol(slot);
		ms_pColPool->Delete(GetSlot(slot));
	}
}

int
CColStore::FindColSlot(const char *name)
{
	ColDef *def;
	int size = ms_pColPool->GetSize();
	for(int i = 0; i < size; i++){
		def = GetSlot(i);
		if(def && !CGeneral::faststricmp(def->name, name))
			return i;
	}
	return -1;
}

char*
CColStore::GetColName(int32 slot)
{
	return GetSlot(slot)->name;
}

CRect&
CColStore::GetBoundingBox(int32 slot)
{
	return GetSlot(slot)->bounds;
}

void
CColStore::IncludeModelIndex(int32 slot, int32 modelIndex)
{
	ColDef *def = GetSlot(slot);
	if(modelIndex < def->minIndex)
		def->minIndex = modelIndex;
	if(modelIndex > def->maxIndex)
		def->maxIndex = modelIndex;
}

bool
CColStore::LoadCol(int32 slot, uint8 *buffer, int32 bufsize)
{
	bool success;
	ColDef *def = GetSlot(slot);
	if(def->minIndex > def->maxIndex)
		success = CFileLoader::LoadCollisionFileFirstTime(buffer, bufsize, slot);
	else
		success = CFileLoader::LoadCollisionFile(buffer, bufsize, slot);
	if(success)
		def->isLoaded = true;
	else
		debug("Failed to load Collision\n");
	return success;
}

void
CColStore::RemoveCol(int32 slot)
{
	int id;
	GetSlot(slot)->isLoaded = false;
	for(id = 0; id < MODELINFOSIZE; id++){
		CBaseModelInfo *mi = CModelInfo::GetModelInfo(id);
		if(mi){
			CColModel *col = mi->GetColModel();
			if(col && col->level == slot)
				col->RemoveCollisionVolumes();
		}
	}
}

void
CColStore::LoadAllCollision(void)
{
	int i;
	for(i = 1; i < COLSTORESIZE; i++)
		if(GetSlot(i))
			CStreaming::RequestCol(i, 0);

	CStreaming::LoadAllRequestedModels(false);
}

void
CColStore::RemoveAllCollision(void)
{
	int i;
	for(i = 1; i < COLSTORESIZE; i++)
		if(GetSlot(i))
			if(CStreaming::CanRemoveCol(i))
				CStreaming::RemoveCol(i);
}

static bool bLoadAtSecondPosition;
static CVector2D secondPosition;

void
CColStore::AddCollisionNeededAtPosn(const CVector2D &pos)
{
	bLoadAtSecondPosition = true;
	secondPosition = pos;
}

void
CColStore::LoadCollision(const CVector2D &pos)
{
	int i;

	if(CStreaming::ms_disableStreaming)
		return;
#if defined(ANDROID) && defined(CUSTOM_MODELS)
	if(CCustomModels::ReplacesNativeWorldMap()){
		bLoadAtSecondPosition = false;
		return;
	}
#endif

	// Read the mission cleanup list once instead of once per collision slot.
	// The old shape ran the full MAX_CLEANUP scan inside the slot loop, so it
	// cost COLSTORESIZE (31) x MAX_CLEANUP (50) iterations - with a pool lookup
	// per iteration - on every call, and this call runs every frame from
	// CStreaming::Update and again from CCollision::SortOutCollisionAfterLoad.
	CPhysical *missionEntities[MAX_CLEANUP];
	int numMissionEntities = 0;
	// m_nCount counts the occupied entries, so in free roam - the common case -
	// there is nothing to scan at all and the whole loop is skipped.
	for(int j = 0; j < MAX_CLEANUP && CTheScripts::MissionCleanUp.m_nCount != 0; j++){
		CPhysical *pEntity = nil;
		cleanup_entity_struct *pCleanup = &CTheScripts::MissionCleanUp.m_sEntities[j];
		if(pCleanup->type == CLEANUP_CAR){
			pEntity = CPools::GetVehiclePool()->GetAt(pCleanup->id);
			if(!pEntity || pEntity->GetStatus() == STATUS_WRECKED)
				continue;
		}else if(pCleanup->type == CLEANUP_CHAR){
			pEntity = CPools::GetPedPool()->GetAt(pCleanup->id);
			if(!pEntity || ((CPed*)pEntity)->DyingOrDead())
				continue;
		}else
			continue;
		if(!pEntity->bDontLoadCollision && !pEntity->bIsFrozen)
			missionEntities[numMissionEntities++] = pEntity;
	}

	// Cache their positions too. Asking each entity for its position inside the
	// slot loop would put a virtual call per slot per entity back into the hot
	// path, which is the cost this rewrite exists to remove.
	CVector2D missionPositions[MAX_CLEANUP];
	for(int j = 0; j < numMissionEntities; j++)
		missionPositions[j] = missionEntities[j]->GetPosition();

	const bool atSecondPosition = bLoadAtSecondPosition;
	const CVector2D second = secondPosition;

	for(i = 1; i < COLSTORESIZE; i++){
		if(GetSlot(i) == nil)
			continue;

		CRect &bounds = GetBoundingBox(i);
		const bool wantThisOne = ColSlotWantedFor(bounds, GetColName(i), pos,
		                                          atSecondPosition, second,
		                                          missionPositions, numMissionEntities);

		// Unconditional, exactly as before: RemoveCol also cancels a slot that
		// is merely queued, so asking first whether it is loaded would leave a
		// pending request alive for a slot we no longer want.
		if(wantThisOne)
			CStreaming::RequestCol(i, STREAMFLAGS_PRIORITY);
		else
			CStreaming::RemoveCol(i);
	}
	bLoadAtSecondPosition = false;
}

void
CColStore::RequestCollision(const CVector2D &pos)
{
	int i;
#if defined(ANDROID) && defined(CUSTOM_MODELS)
	if(CCustomModels::ReplacesNativeWorldMap())
		return;
#endif

	for(i = 1; i < COLSTORESIZE; i++)
		if(GetSlot(i) && ColSlotWithinRequestRadius(GetBoundingBox(i), pos))
			CStreaming::RequestCol(i, STREAMFLAGS_PRIORITY);
}

void
CColStore::EnsureCollisionIsInMemory(const CVector2D &pos)
{
	int i;

	if(CStreaming::ms_disableStreaming)
		return;
#if defined(ANDROID) && defined(CUSTOM_MODELS)
	if(CCustomModels::ReplacesNativeWorldMap())
		return;
#endif

	// Request first, block second. This used to suspend the timer and call
	// LoadAllRequestedModels once per unloaded slot inside the radius, so
	// entering an area with three pending colliders meant three full frame
	// stalls. One LoadAllRequestedModels satisfies every slot that is already
	// queued, so queue them all and block once.
	bool requested = false;
	for(i = 1; i < COLSTORESIZE; i++)
		if(GetSlot(i) && ColSlotNearby(GetBoundingBox(i), pos) &&
		   !CStreaming::HasColLoaded(i)){
			CStreaming::RequestCol(i, 0);
			requested = true;
		}

	if(requested){
		if(TheCamera.GetScreenFadeStatus() == FADE_0)
			FrontEndMenuManager.MessageScreen("LOADCOL", false);
		CTimer::Suspend();
		CStreaming::LoadAllRequestedModels(false);
		CTimer::Resume();
	}
}

bool
CColStore::HasCollisionLoaded(const CVector2D &pos)
{
	int i;
#if defined(ANDROID) && defined(CUSTOM_MODELS)
	if(CCustomModels::ReplacesNativeWorldMap())
		return true;
#endif

	for(i = 1; i < COLSTORESIZE; i++)
		if(GetSlot(i) && ColSlotWithinLoadedRadius(GetBoundingBox(i), pos) &&
		   !GetSlot(i)->isLoaded)
			return false;
	return true;
}
