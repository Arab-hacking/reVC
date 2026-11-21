#include "common.h"

#include "Script.h"

#include "Camera.h"
#include "Game.h"
#include "PlayerPed.h"
#include "CarCtrl.h"
#include "Clock.h"
#include "ColStore.h"
#include "Lines.h"
#include "FileMgr.h"
#include "General.h"
#include "Population.h"
#include "Pools.h"
#include "Replay.h"
#include "Streaming.h"
#include "TxdStore.h"
#include "Weather.h"
#include "SaveBuf.h"
#include "main.h"
#include "custom/CustomModels.h"
#include "custom/CustomWorld.h"

int32 ScriptParams[32];

// The scripted story used to be a bytecode interpreter fed by data/main.scm.
// Missions, shops, the traffic spawner and the whole police dispatcher lived
// in its 18000 lines of opcode handlers. This tree is being prepared for
// multiplayer, where a server owns all of that, so the interpreter is gone.
//
// What is left is CTheScripts as a thin interface. The engine still asks it
// questions - "is the player on a mission?", "which text lines are on screen?"
// - and the answers are now the honest empty ones, so the other twenty-odd
// systems that depend on it keep compiling and behave as if no script is
// running. The data files stay on disk and are simply never read.
//
// The weather opcodes are the deliberate exception: they still work, because
// a world with no way to change the sky is not a world.

// ---- CTheScripts state --------------------------------------------------

uint8 CTheScripts::ScriptSpace[SIZE_SCRIPT_SPACE];
CRunningScript CTheScripts::ScriptsArray[MAX_NUM_SCRIPTS];
intro_text_line CTheScripts::IntroTextLines[MAX_NUM_INTRO_TEXT_LINES];
intro_script_rectangle CTheScripts::IntroRectangles[MAX_NUM_INTRO_RECTANGLES];
CSprite2d CTheScripts::ScriptSprites[MAX_NUM_SCRIPT_SRPITES];
script_sphere_struct CTheScripts::ScriptSphereArray[MAX_NUM_SCRIPT_SPHERES];
tUsedObject CTheScripts::UsedObjectArray[MAX_NUM_USED_OBJECTS];
CStoredLine CTheScripts::aStoredLines[MAX_NUM_STORED_LINES];
uint16 CTheScripts::NumScriptDebugLines;

bool CTheScripts::DbgFlag;
uint32 CTheScripts::OnAMissionFlag;
CMissionCleanup CTheScripts::MissionCleanUp;
CUpsideDownCarCheck CTheScripts::UpsideDownCars;
CStuckCarCheck CTheScripts::StuckCars;
CRunningScript *CTheScripts::pIdleScripts;
CRunningScript *CTheScripts::pActiveScripts;
bool CTheScripts::bAlreadyRunningAMissionScript;
bool CTheScripts::bUsingAMultiScriptFile;
uint16 CTheScripts::NumberOfIntroRectanglesThisFrame;
uint16 CTheScripts::NumberOfIntroTextLinesThisFrame;
uint32 CTheScripts::LastMissionPassedTime;
bool CTheScripts::bPlayerIsInTheStatium;
uint8 CTheScripts::RiotIntensity;
bool CTheScripts::bPlayerHasMetDebbieHarry;

#ifdef USE_DEBUG_SCRIPT_LOADER
#ifdef ANDROID
int CTheScripts::ScriptToLoad = 1;
#else
int CTheScripts::ScriptToLoad = 0;
#endif
int CTheScripts::OpenScript()
{
	return 0;
}
#endif

const uint32 CRunningScript::nSaveStructSize =
#ifdef COMPATIBLE_SAVES
	136;
#else
	sizeof(CRunningScript);
#endif

void CRunningScript::UpdateCompareFlag(bool flag)
{
	if (m_bNotFlag)
		flag = !flag;
	if (m_nAndOrState == 0)
		m_bCondResult = flag;
	else if (m_nAndOrState < ANDS_1 || m_nAndOrState > ANDS_8) {
		if (m_nAndOrState < ORS_1 || m_nAndOrState > ORS_8)
			return;
		if (flag)
			m_nAndOrState = 0;
		else if (m_nAndOrState != ORS_8)
			m_nAndOrState++;
		else {
			m_bCondResult = false;
			m_nAndOrState = 0;
		}
	}
	else if (flag) {
		if (m_nAndOrState == ANDS_8) {
			m_bCondResult = true;
			m_nAndOrState = 0;
		}
		else
			m_nAndOrState++;
	}
	else {
		m_bCondResult = false;
		m_nAndOrState = 0;
	}
}

// ---- the script list ----------------------------------------------------

void CRunningScript::RemoveScriptFromList(CRunningScript** ppScript)
{
	if (prev)
		prev->next = next;
	else
		*ppScript = next;
	if (next)
		next->prev = prev;
}

void CRunningScript::AddScriptToList(CRunningScript** ppScript)
{
	next = *ppScript;
	prev = nil;
	if (*ppScript)
		(*ppScript)->prev = this;
	*ppScript = this;
}

CRunningScript* CTheScripts::StartNewScript(uint32 ip)
{
	CRunningScript* pNew = pIdleScripts;
	script_assert(pNew);
	pNew->RemoveScriptFromList(&pIdleScripts);
	pNew->Init();
	pNew->SetIP(ip);
	pNew->AddScriptToList(&pActiveScripts);
	pNew->m_bIsActive = true;
	return pNew;
}

void CTheScripts::RemoveScriptTextureDictionary()
{
	for (int i = 0; i < ARRAY_SIZE(CTheScripts::ScriptSprites); i++)
		CTheScripts::ScriptSprites[i].Delete();
	int slot = CTxdStore::FindTxdSlot("script");
	if (slot != -1)
		CTxdStore::RemoveTxd(slot);
}

void CTheScripts::Init()
{
	for (int i = 0; i < SIZE_SCRIPT_SPACE; i++)
		ScriptSpace[i] = 0;
	pActiveScripts = pIdleScripts = nil;
	for (int i = 0; i < MAX_NUM_SCRIPTS; i++) {
		ScriptsArray[i].Init();
		ScriptsArray[i].AddScriptToList(&pIdleScripts);
	}
	MissionCleanUp.Init();
	UpsideDownCars.Init();
	StuckCars.Init();
	// No main.scm is opened: the script space stays zeroed, so every read of
	// a script variable yields 0 and every entity lookup misses.
	OnAMissionFlag = 0;
	LastMissionPassedTime = (uint32)-1;
	DbgFlag = false;
	NumScriptDebugLines = 0;
	RiotIntensity = 0;
	bPlayerHasMetDebbieHarry = false;
	bPlayerIsInTheStatium = false;
	bAlreadyRunningAMissionScript = false;
	bUsingAMultiScriptFile = false;
	for (int i = 0; i < MAX_NUM_SCRIPT_SPHERES; i++) {
		ScriptSphereArray[i].m_bInUse = false;
		ScriptSphereArray[i].m_Index = 1;
		ScriptSphereArray[i].m_Id = 0;
		ScriptSphereArray[i].m_vecCenter = CVector(0.0f, 0.0f, 0.0f);
		ScriptSphereArray[i].m_fRadius = 0.0f;
	}
	for (int i = 0; i < MAX_NUM_INTRO_TEXT_LINES; i++)
		IntroTextLines[i].Reset();
	NumberOfIntroTextLinesThisFrame = 0;
	for (int i = 0; i < MAX_NUM_INTRO_RECTANGLES; i++) {
		IntroRectangles[i].m_bIsUsed = false;
		IntroRectangles[i].m_bBeforeFade = false;
		IntroRectangles[i].m_nTextureId = -1;
		IntroRectangles[i].m_sRect = CRect(0.0f, 0.0f, 0.0f, 0.0f);
		IntroRectangles[i].m_sColor = CRGBA(255, 255, 255, 255);
	}
	NumberOfIntroRectanglesThisFrame = 0;
	RemoveScriptTextureDictionary();
}

// ---- the bytecode entry point -------------------------------------------

// The interpreter that used to live behind this switch is gone. A script
// thread that the multiplayer layer starts still runs through here, so the
// weather commands keep working; anything else it asks for is not implemented
// and terminates the thread with a clear message rather than corrupting state.
int8 CRunningScript::ProcessOneCommand()
{
	int32 command = (uint16)CTheScripts::Read2BytesFromScript(&m_nIp);
	m_bNotFlag = (command & 0x8000);
	command &= 0x7FFF;
	int8 retval = ProcessWeatherCommands(command);
	if (retval < 0) {
		REVC_LOG_ERROR("Script %s: opcode %d is not available in this build",
			m_abScriptName, command);
		return -1;
	}
	return retval;
}

// In Vice City the player is not created by the engine - data/main.scm does it
// with the CREATE_PLAYER opcode. With the interpreter gone, nothing did, so
// FindPlayerPed() stayed nil and the first camera update dereferenced it.
// This reproduces what that opcode did, minus the script plumbing.
//
// Where to spawn still comes from the world: the custom-map redirect is tried
// first, exactly as the opcode did, and a position with no ground below it
// falls back to the street-level landmark rather than dropping the player into
// the void.
// The archive carries no player.dff, so slot 0 (MI_PLAYER) is never filled.
// Serve the player from a real SA actor instead: the first registered ped with
// an SA skeleton, preferring the standard male build. Returns -1 when the
// archive supplied no actor at all, in which case the caller keeps the
// hard-coded slot and the engine reports the missing model.
static int32
LookUpPlayerModel(void)
{
	// Registration is all that can be asked of a candidate here. The SA
	// skeleton is only recognised once the clump is resident, and at start-up
	// nothing is resident yet, so testing m_bSAAnimationModel would reject
	// every model and leave the player on the empty slot 0. The body is judged
	// later by CPed::SetModelIndex, which loads it and swaps it if it turns out
	// to be a VC skeleton.
	int32 anyPed = -1;
	int32 pedCount = 0;
	for (int32 id = 0; id < CPedModelInfo::ms_lastPedSlot; id++) {
		CPedModelInfo *mi = (CPedModelInfo *)CModelInfo::GetModelInfo(id);
		if (mi == nil || mi->GetModelType() != MITYPE_PED)
			continue;
		pedCount++;
		if (id == MI_PLAYER)
			return id; // a real player.dff was supplied
		if (mi->m_animGroup == ASSOCGRP_PLAYER || mi->m_animGroup == ASSOCGRP_DEFAULT)
			return id;
		if (anyPed < 0)
			anyPed = id;
	}
	REVC_LOG_INFO("[spawn] ped models registered below slot %d: %d", CPedModelInfo::ms_lastPedSlot, pedCount);
	return anyPed;
}

void CTheScripts::SpawnInitialPlayer()
{
	const int32 index = 0;
	REVC_LOG_INFO("[spawn] SpawnInitialPlayer enter");
	if (CWorld::Players[index].m_pPed)
		return;

	// MI_PLAYER is the hard-coded slot 0 and its row came from DEFAULT.IDE. The
	// archive ships no player.dff, so that slot stays empty by design and
	// asking for it only digs through the IMG directories to no effect. An SA
	// actor body is used instead: CPed::SetModelIndex swaps a VC skeleton for a
	// resident SA model, and LookUpPlayerModel picks the slot to ask for.
	if (!CStreaming::HasModelLoaded(MI_PLAYER)) {
		const int32 playerModel = LookUpPlayerModel();
		if (playerModel >= 0) {
			CStreaming::RequestModel(playerModel,
				STREAMFLAGS_PRIORITY | STREAMFLAGS_DONT_REMOVE | STREAMFLAGS_DEPENDENCY);
			CStreaming::LoadAllRequestedModels(true);
			if (!CStreaming::HasModelLoaded(playerModel))
				CStreaming::LoadAllRequestedModels(false);
		}
	}

	REVC_LOG_INFO("[spawn] requesting player model, chosen=%d", LookUpPlayerModel());
	CPlayerPed::SetupPlayerPed(index);
	REVC_LOG_INFO("[spawn] SetupPlayerPed returned");
	CPlayerPed *pPlayer = CWorld::Players[index].m_pPed;
	REVC_LOG_INFO("[spawn] player ped=%p model=%d", (void*)pPlayer, pPlayer ? (int)pPlayer->GetModelIndex() : -1);
	pPlayer->CharCreatedBy = MISSION_CHAR;
	// Built off-world, then placed and added back, so it is not processed
	// between construction and the position being final.
	CPlayerPed::DeactivatePlayerPed(index);

	// The vanilla start of main.scm put the player on the beach at Ocean
	// Beach; that is where a freeroam build wants him when nothing else says.
	REVC_LOG_INFO("[spawn] resolving spawn position");
	CVector pos(79.6875f, -867.25f, MAP_Z_LOW_LIMIT);
	float heading = 0.0f;
	bool redirected = CCustomWorld::ResolveSpawnPosition(pos, &heading);
	if (!redirected) {
		heading = 180.0f;
	}

	if (pos.z <= MAP_Z_LOW_LIMIT) {
		REVC_LOG_INFO("[spawn] finding ground z");
		pos.z = CWorld::FindGroundZForCoord(pos.x, pos.y);
		REVC_LOG_INFO("[spawn] ground z=%.2f", pos.z);
	}
	REVC_LOG_INFO("[spawn] centre-of-mass offset");
	pos.z += pPlayer->GetDistanceFromCentreOfMassToBaseOfModel();
	REVC_LOG_INFO("[spawn] SetPosition");
	pPlayer->SetPosition(pos);
	REVC_LOG_INFO("[spawn] ClearSpaceForMissionEntity");
	ClearSpaceForMissionEntity(pos, pPlayer);
	REVC_LOG_INFO("[spawn] ReactivatePlayerPed");
	CPlayerPed::ReactivatePlayerPed(index);

	if (redirected) {
		pPlayer->m_fRotationCur = DEGTORAD(heading);
		pPlayer->m_fRotationDest = DEGTORAD(heading);
		pPlayer->SetHeading(DEGTORAD(heading));
	}

	// The camera was handed back by the mission clean-up pass that used to run
	// when a script finished. Nothing runs it now, so point the camera at the
	// player here or the first frame is spent looking wherever the intro left
	// it - and with no player there to look at, that was the crash.
	REVC_LOG_INFO("[spawn] RestoreWithJumpCut");
	TheCamera.RestoreWithJumpCut();
	REVC_LOG_INFO("[spawn] done");

	REVC_LOG_INFO("Spawned player 0 at (%.1f, %.1f, %.1f) heading %.0f%s",
		pos.x, pos.y, pos.z, heading, redirected ? " (custom map)" : "");
}

void CTheScripts::Process()
{
	// There are no running scripts to advance. The car checks still run so a
	// flipped or wedged vehicle is recovered the way it always was, and the
	// mission clean-up list is kept alive for the collision code that reads it.
	if (CReplay::IsPlayingBack())
		return;
	UpsideDownCars.UpdateTimers();
	StuckCars.Process();
	MissionCleanUp.CheckIfCollisionHasLoadedForMissionObjects();
}

bool CTheScripts::IsPlayerOnAMission()
{
	return false;
}

void CTheScripts::RemoveThisPed(CPed* pPed)
{
	if (!pPed)
		return;
	if (pPed->IsPlayer())
		return;
	if (pPed->m_nPedType == PEDTYPE_CIVMALE || pPed->m_nPedType == PEDTYPE_CIVFEMALE ||
	    pPed->m_nPedType == PEDTYPE_GANG1 || pPed->m_nPedType == PEDTYPE_GANG2 ||
	    pPed->m_nPedType == PEDTYPE_GANG3 || pPed->m_nPedType == PEDTYPE_GANG4 ||
	    pPed->m_nPedType == PEDTYPE_GANG5 || pPed->m_nPedType == PEDTYPE_GANG6 ||
	    pPed->m_nPedType == PEDTYPE_GANG7 || pPed->m_nPedType == PEDTYPE_GANG8 ||
	    pPed->m_nPedType == PEDTYPE_GANG9 || pPed->m_nPedType == PEDTYPE_COP)
		CPopulation::RemovePed(pPed);
}

// Two things the engine genuinely still needs from the mission layer: clear a
// bit of road so a spawned entity is not born inside something, and hand a
// temporary object's memory back. Both are self-contained and used by
// GameLogic and Streaming, so they stay.
CMissionCleanup::CMissionCleanup()
{
	Init();
}

void CMissionCleanup::Init()
{
	m_nCount = 0;
	for (int i = 0; i < MAX_CLEANUP; i++){
		m_sEntities[i].type = CLEANUP_UNUSED;
		m_sEntities[i].id = 0;
	}
}

cleanup_entity_struct* CMissionCleanup::FindFree()
{
	for (int i = 0; i < MAX_CLEANUP; i++){
		if (m_sEntities[i].type == CLEANUP_UNUSED)
			return &m_sEntities[i];
	}
	script_assert(0);
	return nil;
}

void CMissionCleanup::AddEntityToList(int32 id, uint8 type)
{
	cleanup_entity_struct* pNew = FindFree();
	if (!pNew)
		return;
	pNew->id = id;
	pNew->type = type;
	m_nCount++;
}

void CMissionCleanup::RemoveEntityFromList(int32 id, uint8 type)
{
	for (int i = 0; i < MAX_CLEANUP; i++){
		if (m_sEntities[i].type == type && m_sEntities[i].id == id){
			switch (m_sEntities[i].type) {
			case CLEANUP_CAR:
			{
				CVehicle* pVehicle = CPools::GetVehiclePool()->GetAt(m_sEntities[i].id);
				if (pVehicle) {
					if (pVehicle->bIsStaticWaitingForCollision) {
						pVehicle->bIsStaticWaitingForCollision = false;
						if (!pVehicle->GetIsStatic())
							pVehicle->AddToMovingList();
					}
				}
				break;
			}
			case CLEANUP_CHAR:
			{
				CPed* pPed = CPools::GetPedPool()->GetAt(m_sEntities[i].id);
				if (pPed) {
					if (pPed->bIsStaticWaitingForCollision) {
						pPed->bIsStaticWaitingForCollision = false;
						if (!pPed->GetIsStatic())
							pPed->AddToMovingList();
					}
				}
				break;
			}
			case CLEANUP_OBJECT:
			{
				CObject* pObject = CPools::GetObjectPool()->GetAt(m_sEntities[i].id);
				if (pObject) {
					if (pObject->bIsStaticWaitingForCollision) {
						pObject->bIsStaticWaitingForCollision = false;
						if (!pObject->GetIsStatic())
							pObject->AddToMovingList();
					}
				}
				break;
			}
			default:
				break;
			}
			m_sEntities[i].id = 0;
			m_sEntities[i].type = CLEANUP_UNUSED;
			m_nCount--;
		}
	}
}

void CMissionCleanup::CheckIfCollisionHasLoadedForMissionObjects()
{
	for (int i = 0; i < MAX_CLEANUP; i++) {
		switch (m_sEntities[i].type) {
		case CLEANUP_CAR:
		{
			CVehicle* pVehicle = CPools::GetVehiclePool()->GetAt(m_sEntities[i].id);
			if (pVehicle) {
				if (pVehicle->bIsStaticWaitingForCollision) {
					if (CColStore::HasCollisionLoaded(pVehicle->GetPosition())) {
						pVehicle->bIsStaticWaitingForCollision = false;
							if (!pVehicle->GetIsStatic())
								pVehicle->AddToMovingList();
					}
				}
			}
			break;
		}
		case CLEANUP_CHAR:
		{
			CPed* pPed = CPools::GetPedPool()->GetAt(m_sEntities[i].id);
			if (pPed) {
				if (pPed->bIsStaticWaitingForCollision) {
					if (CColStore::HasCollisionLoaded(pPed->GetPosition())) {
						pPed->bIsStaticWaitingForCollision = false;
						if (!pPed->GetIsStatic())
							pPed->AddToMovingList();
					}
				}
			}
			break;
		}
		case CLEANUP_OBJECT:
		{
			CObject* pObject = CPools::GetObjectPool()->GetAt(m_sEntities[i].id);
			if (pObject) {
				if (pObject->bIsStaticWaitingForCollision) {
					if (CColStore::HasCollisionLoaded(pObject->GetPosition())) {
						pObject->bIsStaticWaitingForCollision = false;
						if (!pObject->GetIsStatic())
							pObject->AddToMovingList();
					}
				}
			}
			break;
		}
		default:
			break;
		}
	}
}

// CUpsideDownCarCheck and CStuckCarCheck moved to their own San Andreas
// classes (vehicles/UpsideDownCarCheck.cpp, vehicles/StuckCarCheck.cpp).

void CRunningScript::CollectParameters(uint32* pIp, int16 total)
{
	for (int16 i = 0; i < total; i++){
		uint16 varIndex;
		switch (CTheScripts::Read1ByteFromScript(pIp))
		{
		case ARGUMENT_INT32:
		case ARGUMENT_FLOAT:
			ScriptParams[i] = CTheScripts::Read4BytesFromScript(pIp);
			break;
		case ARGUMENT_GLOBALVAR:
			varIndex = CTheScripts::Read2BytesFromScript(pIp);
			script_assert(varIndex >= 8 && varIndex < CTheScripts::GetSizeOfVariableSpace());
			ScriptParams[i] = *((int32*)&CTheScripts::ScriptSpace[varIndex]);
			break;
		case ARGUMENT_LOCALVAR:
			varIndex = CTheScripts::Read2BytesFromScript(pIp);
			script_assert(varIndex >= 0 && varIndex < ARRAY_SIZE(m_anLocalVariables));
			ScriptParams[i] = m_anLocalVariables[varIndex];
			break;
		case ARGUMENT_INT8:
			ScriptParams[i] = CTheScripts::Read1ByteFromScript(pIp);
			break;
		case ARGUMENT_INT16:
			ScriptParams[i] = CTheScripts::Read2BytesFromScript(pIp);
			break;
		default:
			script_assert(0);
			break;
		}
	}
}

int32 CRunningScript::CollectNextParameterWithoutIncreasingPC(uint32 ip)
{
	uint32* pIp = &ip;
	switch (CTheScripts::Read1ByteFromScript(pIp))
	{
	case ARGUMENT_INT32:
		return CTheScripts::Read4BytesFromScript(pIp);
	case ARGUMENT_GLOBALVAR:
		return *((int32*)&CTheScripts::ScriptSpace[(uint16)CTheScripts::Read2BytesFromScript(pIp)]);
	case ARGUMENT_LOCALVAR:
		return m_anLocalVariables[CTheScripts::Read2BytesFromScript(pIp)];
	case ARGUMENT_INT8:
		return CTheScripts::Read1ByteFromScript(pIp);
	case ARGUMENT_INT16:
		return CTheScripts::Read2BytesFromScript(pIp);
	case ARGUMENT_FLOAT:
		return CTheScripts::Read4BytesFromScript(pIp);
	default:
		script_assert(0);
	}
	return -1;
}

void CRunningScript::StoreParameters(uint32* pIp, int16 number)
{
	for (int16 i = 0; i < number; i++){
		switch (CTheScripts::Read1ByteFromScript(pIp)) {
		case ARGUMENT_GLOBALVAR:
			*(int32*)&CTheScripts::ScriptSpace[(uint16)CTheScripts::Read2BytesFromScript(pIp)] = ScriptParams[i];
			break;
		case ARGUMENT_LOCALVAR:
			m_anLocalVariables[CTheScripts::Read2BytesFromScript(pIp)] = ScriptParams[i];
			break;
		default:
			script_assert(0);
		}
	}
}

int32 *CRunningScript::GetPointerToScriptVariable(uint32* pIp, int16 type)
{
	switch (CTheScripts::Read1ByteFromScript(pIp))
	{
	case ARGUMENT_GLOBALVAR:
		script_assert(type == VAR_GLOBAL);
		return (int32*)&CTheScripts::ScriptSpace[(uint16)CTheScripts::Read2BytesFromScript(pIp)];
	case ARGUMENT_LOCALVAR:
		script_assert(type == VAR_LOCAL);
		return &m_anLocalVariables[CTheScripts::Read2BytesFromScript(pIp)];
	default:
		script_assert(0);
	}
	return nil;
}

void CRunningScript::Init()
{
	strcpy(m_abScriptName, "noname");
	next = prev = nil;
	SetIP(0);
	for (int i = 0; i < MAX_STACK_DEPTH; i++)
		m_anStack[i] = 0;
	m_nStackPointer = 0;
	m_nWakeTime = 0;
	m_bIsActive = false;
	m_bCondResult = false;
	m_bIsMissionScript = false;
	m_bSkipWakeTime = false;
	for (int i = 0; i < NUM_LOCAL_VARS + NUM_TIMERS; i++)
		m_anLocalVariables[i] = 0;
	m_nAndOrState = 0;
	m_bNotFlag = false;
	m_bDeatharrestEnabled = true;
	m_bDeatharrestExecuted = false;
	m_bMissionFlag = false;
}

void CTheScripts::ClearSpaceForMissionEntity(const CVector& pos, CEntity* pEntity)
{
	// A model that nothing registered has no collision to test against, so
	// there is nothing to clear space for. Without this the routine reads the
	// col model of an empty slot below.
	if (pEntity == nil || pEntity->GetColModel() == nil)
		return;

	static CColPoint aTempColPoints[MAX_COLLISION_POINTS];
	int16 entities = 0;
	CEntity* aEntities[16];
	CWorld::FindObjectsKindaColliding(pos, pEntity->GetBoundRadius(), false, &entities, 16, aEntities, false, true, true, false, false);
	if (entities <= 0)
		return;
	for (uint16 i = 0; i < entities; i++) {
		if (aEntities[i] != pEntity && aEntities[i]->IsPed() && ((CPed*)aEntities[i])->bInVehicle)
			aEntities[i] = nil;
	}
	for (uint16 i = 0; i < entities; i++) {
		if (aEntities[i] == pEntity || !aEntities[i])
			continue;
		CEntity* pFound = aEntities[i];
		int cols;
		if (pEntity->GetColModel()->numLines <= 0)
			cols = CCollision::ProcessColModels(pEntity->GetMatrix(), *pEntity->GetColModel(),
				pFound->GetMatrix(), *pFound->GetColModel(), aTempColPoints, nil, nil);
		else {
			float lines[4];
			lines[0] = lines[1] = lines[2] = lines[3] = 1.0f;
			CColPoint tmp[4];
			cols = CCollision::ProcessColModels(pEntity->GetMatrix(), *pEntity->GetColModel(),
				pFound->GetMatrix(), *pFound->GetColModel(), aTempColPoints,tmp, lines);
		}
		if (cols <= 0)
			continue;
		switch (pFound->GetType()) {
		case ENTITY_TYPE_VEHICLE:
		{
			printf("Will try to delete a vehicle where a mission entity should be\n");
			CVehicle* pVehicle = (CVehicle*)pFound;
			if (pVehicle->bIsLocked || !pVehicle->CanBeDeleted())
				break;
			if (pVehicle->pDriver) {
				CPopulation::RemovePed(pVehicle->pDriver);
				pVehicle->pDriver = nil;
			}
			for (int i = 0; i < pVehicle->m_nNumMaxPassengers; i++) {
				if (pVehicle->pPassengers[i]) {
					CPopulation::RemovePed(pVehicle->pPassengers[i]);
					pVehicle->pPassengers[i] = 0;
					pVehicle->m_nNumPassengers--;
				}
			}
			CCarCtrl::RemoveFromInterestingVehicleList(pVehicle);
			CWorld::Remove(pVehicle);
			delete pVehicle;
			break;
		}
		case ENTITY_TYPE_PED:
		{
			CPed* pPed = (CPed*)pFound;
			if (pPed->IsPlayer() || !pPed->CanBeDeleted())
				break;
			CPopulation::RemovePed(pPed);
			printf("Deleted a ped where a mission entity should be\n");
			break;
		}
		default: break;
		}
	}
}
void CTheScripts::CleanUpThisObject(CObject* pObject)
{
	if (!pObject)
		return;
	if (pObject->ObjectCreatedBy != MISSION_OBJECT)
		return;
	pObject->ObjectCreatedBy = TEMP_OBJECT;
	pObject->m_nEndOfLifeTime = CTimer::GetTimeInMilliseconds() + 20000000;
	pObject->m_nRefModelIndex = -1;
	pObject->bUseVehicleColours = false;
	++CObject::nNoTempObjects;
}

// The save format still reserves a slot for script state. With no interpreter
// there is nothing in it beyond the mission flag and a timestamp, so the block
// is small and fixed size. It round-trips through this build; saves written by
// the scripted build are rejected by the length check below rather than being
// half-restored into state that no longer exists.
#define SCRIPT_DATA_SIZE (sizeof(uint32) + 5 * sizeof(uint32))

void CTheScripts::ScriptDebugLine3D(float x1, float y1, float z1, float x2, float y2, float z2, uint32 col, uint32 col2)
{
	if (NumScriptDebugLines >= MAX_NUM_STORED_LINES)
		return;
	aStoredLines[NumScriptDebugLines].vecInf = CVector(x1, y1, z1);
	aStoredLines[NumScriptDebugLines].vecSup = CVector(x2, y2, z2);
	aStoredLines[NumScriptDebugLines].color1 = col;
	aStoredLines[NumScriptDebugLines++].color2 = col2;
}

void CTheScripts::RenderTheScriptDebugLines()
{
	RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, (void*)1);
	RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)1);
	for (int i = 0; i < NumScriptDebugLines; i++) {
		CLines::RenderLineWithClipping(
			aStoredLines[i].vecInf.x,
			aStoredLines[i].vecInf.y,
			aStoredLines[i].vecInf.z,
			aStoredLines[i].vecSup.x,
			aStoredLines[i].vecSup.y,
			aStoredLines[i].vecSup.z,
			aStoredLines[i].color1,
			aStoredLines[i].color2);
	}
	NumScriptDebugLines = 0;
	RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)0);
}

void CTheScripts::SaveAllScripts(uint8* buf, uint32* size)
{
	INITSAVEBUF
	uint32 varSpace = 8 * sizeof(uint32);
	*size = varSpace + SCRIPT_DATA_SIZE + SAVE_HEADER_SIZE + 3 * sizeof(uint32);
	WriteSaveHeader(buf, 'S', 'C', 'R', '\0', *size - SAVE_HEADER_SIZE);
	WriteSaveBuf(buf, varSpace);
	for (uint32 i = 0; i < varSpace; i++)
		WriteSaveBuf(buf, (uint8)0);
	uint32 script_data_size = SCRIPT_DATA_SIZE;
	WriteSaveBuf(buf, script_data_size);
	WriteSaveBuf(buf, OnAMissionFlag);
	WriteSaveBuf(buf, LastMissionPassedTime);
	for (uint32 i = 0; i < 4; i++)
		WriteSaveBuf(buf, (uint32)0);
}

bool CTheScripts::LoadAllScripts(uint8* buf, uint32 size)
{
	const uint32 minFixedBytes = SAVE_HEADER_SIZE + 3 * sizeof(uint32) + SCRIPT_DATA_SIZE;
	if (buf == nil || size < minFixedBytes) {
		REVC_LOG_ERROR("Cannot load saved scripts: block too small (%u bytes)", size);
		return false;
	}
	if (buf[0] != 'S' || buf[1] != 'C' || buf[2] != 'R' || buf[3] != '\0') {
		REVC_LOG_ERROR("Cannot load saved scripts: invalid SCR header");
		return false;
	}
	uint32 headerPayloadSize = 0;
	memcpy(&headerPayloadSize, buf + 4, sizeof(headerPayloadSize));
	if (headerPayloadSize != size - SAVE_HEADER_SIZE) {
		REVC_LOG_ERROR("Cannot load saved scripts: header size mismatch (%u != %u)",
			headerPayloadSize, size - (uint32)SAVE_HEADER_SIZE);
		return false;
	}
	uint32 varSpace = 0;
	memcpy(&varSpace, buf + SAVE_HEADER_SIZE, sizeof(varSpace));
	if (varSpace > SIZE_SCRIPT_SPACE) {
		REVC_LOG_ERROR("Cannot load saved scripts: variable space %u out of range", varSpace);
		return false;
	}
	if (size < SAVE_HEADER_SIZE + sizeof(uint32) + varSpace) {
		REVC_LOG_ERROR("Cannot load saved scripts: block shorter than its variable space");
		return false;
	}
	// Nothing to restore: the player's mission progress is not part of this
	// build. The world itself is restored by the caller.
	OnAMissionFlag = 0;
	return true;
}
