// Verify CTheScripts::LoadAllScripts rejects incompatible or corrupt SCR\0
// save blocks cleanly (returning false instead of aborting on script_assert)
// and restores clean script state via CTheScripts::Init().
#include "common.h"

#include "DebugLog.h"
#include "FileMgr.h"
#include "ModelInfo.h"
#include "Pools.h"
#include "SaveBuf.h"
#include "Script.h"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void
Check(bool condition, const char *description)
{
	if(!condition){
		fprintf(stderr, "Script save validation test failed: %s\n", description);
		abort();
	}
}

void
re3_assert(const char *expr, const char *file, unsigned int line, const char *func)
{
	fprintf(stderr, "Unexpected assertion failure: %s (%s:%u %s)\n", expr, file, line, func);
	abort();
}

void DebugLogWrite(const char*, const char*, int, const char*, const char*, ...) {}
void re3_log(const char*, const char*, unsigned int, const char*, const char*, ...) {}

#ifdef VALIDATE_SAVE_SIZE
int32 _saveBufCount = 0;
#endif

const uint32 CRunningScript::nSaveStructSize =
#ifdef COMPATIBLE_SAVES
	136;
#else
	sizeof(CRunningScript);
#endif

// Minimal script globals & stubs exercised by SaveAllScripts / LoadAllScripts.
uint8 CTheScripts::ScriptSpace[SIZE_SCRIPT_SPACE];
CRunningScript CTheScripts::ScriptsArray[MAX_NUM_SCRIPTS];
CRunningScript *CTheScripts::pIdleScripts = nil;
CRunningScript *CTheScripts::pActiveScripts = nil;
uint32 CTheScripts::OnAMissionFlag = 0;
uint32 CTheScripts::LastMissionPassedTime = 0;
tBuildingSwap CTheScripts::BuildingSwapArray[MAX_NUM_BUILDING_SWAPS];
CEntity *CTheScripts::InvisibilitySettingArray[MAX_NUM_INVISIBILITY_SETTINGS];
bool CTheScripts::bUsingAMultiScriptFile = true;
bool CTheScripts::bPlayerHasMetDebbieHarry = false;
uint32 CTheScripts::MainScriptSize = 0;
uint32 CTheScripts::LargestMissionScriptSize = 0;
uint16 CTheScripts::NumberOfMissionScripts = 0;
uint16 CTheScripts::NumberOfExclusiveMissionScripts = 0;
uint16 CTheScripts::CommandsExecuted = 0;
uint16 CTheScripts::ScriptsUpdated = 0;
#ifdef USE_DEBUG_SCRIPT_LOADER
int CTheScripts::ScriptToLoad = 0;
int CTheScripts::OpenScript(void) { return 0; }
#endif

int CFileMgr::OpenFile(const char*, const char*) { return 0; }
int CFileMgr::CloseFile(int) { return 0; }
void CFileMgr::SetDir(const char*) {}

CTreadablePool *CPools::ms_pTreadablePool = nil;
CBuildingPool *CPools::ms_pBuildingPool = nil;
CObjectPool *CPools::ms_pObjectPool = nil;
CDummyPool *CPools::ms_pDummyPool = nil;
CBaseModelInfo *CModelInfo::ms_modelInfoPtrs[MODELINFOSIZE];

void CBuilding::ReplaceWithNewModel(int32) {}

static uint32 gInstalledVarSpace = 1024;
static uint32 gInstalledMainSize = 240000;
static uint32 gInstalledLargestMission = 32000;
static uint16 gInstalledNumMissions = 88;
static uint16 gInstalledNumExclMissions = 80;

void
CTheScripts::Init(void)
{
	memset(ScriptSpace, 0, sizeof(ScriptSpace));
	// Encode JMP instruction layout: opcode + param type at [0..2], 4-byte target offset at [3..6].
	memcpy(&ScriptSpace[3], &gInstalledVarSpace, sizeof(gInstalledVarSpace));

	pActiveScripts = nil;
	pIdleScripts = nil;
	for(int i = 0; i < MAX_NUM_SCRIPTS; i++){
		ScriptsArray[i].Init();
		ScriptsArray[i].AddScriptToList(&pIdleScripts);
	}
	OnAMissionFlag = 0;
	LastMissionPassedTime = 0;
	for(int i = 0; i < MAX_NUM_BUILDING_SWAPS; i++){
		BuildingSwapArray[i].m_pBuilding = nil;
		BuildingSwapArray[i].m_nOldModel = -1;
		BuildingSwapArray[i].m_nNewModel = -1;
	}
	for(int i = 0; i < MAX_NUM_INVISIBILITY_SETTINGS; i++)
		InvisibilitySettingArray[i] = nil;

	bUsingAMultiScriptFile = true;
	bPlayerHasMetDebbieHarry = false;
	MainScriptSize = gInstalledMainSize;
	LargestMissionScriptSize = gInstalledLargestMission;
	NumberOfMissionScripts = gInstalledNumMissions;
	NumberOfExclusiveMissionScripts = gInstalledNumExclMissions;
}

void
CRunningScript::Init(void)
{
	strcpy(m_abScriptName, "noname");
	next = nil;
	prev = nil;
	m_nIp = 0;
	for(int i = 0; i < MAX_STACK_DEPTH; i++)
		m_anStack[i] = 0;
	m_nStackPointer = 0;
	for(int i = 0; i < NUM_LOCAL_VARS + NUM_TIMERS; i++)
		m_anLocalVariables[i] = 0;
	m_bCondResult = false;
	m_bIsMissionScript = false;
	m_bSkipWakeTime = false;
	m_nWakeTime = 0;
	m_nAndOrState = 0;
	m_bNotFlag = false;
	m_bDeatharrestEnabled = true;
	m_bDeatharrestExecuted = false;
	m_bMissionFlag = false;
}

void
CRunningScript::RemoveScriptFromList(CRunningScript **ppScript)
{
	if(prev)
		prev->next = next;
	else
		*ppScript = next;
	if(next)
		next->prev = prev;
}

void
CRunningScript::AddScriptToList(CRunningScript **ppScript)
{
	next = *ppScript;
	prev = nil;
	if(*ppScript)
		(*ppScript)->prev = this;
	*ppScript = this;
}

CRunningScript*
CTheScripts::StartNewScript(uint32 ip)
{
	CRunningScript *pNew = pIdleScripts;
	Check(pNew != nil, "idle script slot available");
	pNew->RemoveScriptFromList(&pIdleScripts);
	pNew->Init();
	pNew->m_nIp = ip;
	pNew->AddScriptToList(&pActiveScripts);
	return pNew;
}

void CTheScripts::UndoBuildingSwaps(void) {}
void CTheScripts::UndoEntityInvisibilitySettings(void) {}

int
main(void)
{
	static uint8 saveBuf[65536];
	uint32 savedSize = 0;

	// 1. Save with main.scm version A (e.g. modded Vice City main.scm).
	gInstalledVarSpace = 1024;
	gInstalledMainSize = 240000;
	CTheScripts::Init();
	CRunningScript *running = CTheScripts::StartNewScript(128);
	strncpy(running->m_abScriptName, "main", 8);
	CTheScripts::ScriptSpace[64] = 0x5A;
	CTheScripts::SaveAllScripts(saveBuf, &savedSize);
	Check(savedSize > 0 && savedSize < sizeof(saveBuf), "SaveAllScripts produced valid block");

	// 2. Round-trip load with matching main.scm succeeds and restores state.
	CTheScripts::ScriptSpace[64] = 0;
	Check(CTheScripts::LoadAllScripts(saveBuf, savedSize),
		"LoadAllScripts succeeds when main.scm matches save");
	Check(CTheScripts::ScriptSpace[64] == 0x5A, "variable space restored");
	Check(CTheScripts::pActiveScripts != nil && CTheScripts::pActiveScripts->m_nIp == 128,
		"running script restored");

	// 3. Reproduce debug-11.log: installed main.scm now has a DIFFERENT MainScriptSize
	//    than userfiles/GTAVCsf1.b. Must return false without crashing and leave
	//    clean initialized script state!
	gInstalledMainSize = 255128;
	Check(!CTheScripts::LoadAllScripts(saveBuf, savedSize),
		"LoadAllScripts rejects save from incompatible main.scm without aborting");
	Check(CTheScripts::MainScriptSize == 255128, "installed MainScriptSize preserved after reject");
	Check(CTheScripts::pActiveScripts == nil, "no half-loaded active scripts left after reject");

	// 4. Different variable space size is also rejected before touching ScriptSpace.
	gInstalledMainSize = 240000;
	gInstalledVarSpace = 2048;
	Check(!CTheScripts::LoadAllScripts(saveBuf, savedSize),
		"LoadAllScripts rejects mismatched variable space size");

	// 5. Corrupt/truncated buffers are rejected cleanly.
	gInstalledVarSpace = 1024;
	Check(!CTheScripts::LoadAllScripts(saveBuf, 32), "truncated SCR block rejected");

	uint8 badStackBuf[65536];
	memcpy(badStackBuf, saveBuf, savedSize);
	const size_t script0Off = savedSize - CRunningScript::nSaveStructSize;
	uint16 badSp = MAX_STACK_DEPTH + 5;
#ifdef COMPATIBLE_SAVES
	memcpy(badStackBuf + script0Off + 44, &badSp, sizeof(badSp));
#else
	memcpy(badStackBuf + script0Off + offsetof(CRunningScript, m_nStackPointer), &badSp, sizeof(badSp));
#endif
	Check(!CTheScripts::LoadAllScripts(badStackBuf, savedSize),
		"corrupt CRunningScript stack pointer rejected cleanly");
	Check(CTheScripts::pActiveScripts == nil, "active scripts reset after corrupt script reject");

	uint8 badIpBuf[65536];
	memcpy(badIpBuf, saveBuf, savedSize);
	int32 badIp = SIZE_SCRIPT_SPACE + 100;
#ifdef COMPATIBLE_SAVES
	memcpy(badIpBuf + script0Off + 16, &badIp, sizeof(badIp));
#else
	memcpy(badIpBuf + script0Off + offsetof(CRunningScript, m_nIp), &badIp, sizeof(badIp));
#endif
	Check(!CTheScripts::LoadAllScripts(badIpBuf, savedSize),
		"corrupt CRunningScript instruction pointer rejected cleanly");

	printf("Script save validation tests OK\n");
	return 0;
}
