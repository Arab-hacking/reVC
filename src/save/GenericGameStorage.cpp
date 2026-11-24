#define WITHWINDOWS
#include "common.h"
#include "Text.h"
#include "crossplatform.h"
#include "main.h"

#include "DMAudio.h"
#include "AudioScriptObject.h"
#include "Automobile.h"
#include "Bike.h"
#include "Boat.h"
#include "Camera.h"
#include "Clock.h"
#include "Date.h"
#include "FileMgr.h"
#include "Font.h"
#include "Frontend.h"
#include "GameLogic.h"
#include "GenericGameStorage.h"
#include "ModelInfo.h"
#include "Pad.h"
#include "Particle.h"
#include "ParticleObject.h"
#include "PCSave.h"
#include "Pickups.h"
#include "PlayerPed.h"
#include "ProjectileInfo.h"
#include "Pools.h"
#include "Radar.h"
#include "Stats.h"
#include "Streaming.h"
#include "Timer.h"
#include "TimeStep.h"
#include "Weather.h"
#include "World.h"
#include "Zones.h"
#include "Timecycle.h"
#include "SaveBlockBounds.h"
#include "SaveVersion.h"
#include "SavedPoolValidation.h"
#include <vector>

#define BLOCK_COUNT 22
// The simple-vars payload the Vice City format carries, followed by this
// build's version marker. Everything up to and including RadioStationPosition
// keeps the original offsets, so a reader that knows the VC layout still finds
// every field where it was; the marker is what it trips over.
#define SIZE_OF_SIMPLEVARS_VC 0xE4
#define SIZE_OF_SIMPLEVARS (SIZE_OF_SIMPLEVARS_VC + SAVE_VERSION_SIZE)

const uint32 SIZE_OF_ONE_GAME_IN_BYTES = 201729;

#ifdef MISSION_REPLAY
int8 IsQuickSave;
const int PAUSE_SAVE_SLOT = SLOT_COUNT;
#endif

char DefaultPCSaveFileName[260];
char ValidSaveName[260];
char LoadFileName[256];
wchar SlotFileName[SLOT_COUNT][260];
wchar SlotSaveDate[SLOT_COUNT][70];
int CheckSum;
eLevelName m_LevelToLoad;
char SaveFileNameJustSaved[260];
int Slots[SLOT_COUNT];

bool b_FoundRecentSavedGameWantToLoad;
bool JustLoadedDontFadeInYet;
bool StillToFadeOut;
uint32 TimeStartedCountingForFade;
uint32 TimeToStayFadedBeforeFadeOut = 1750;

int32 RadioStationPosition[NUM_RADIOS];

void
InitRadioStationPositionList()
{
	for (int i = 0; i < NUM_RADIOS; i++)
		RadioStationPosition[i] = -1;
}

int32
GetSavedRadioStationPosition(int32 station)
{
	return RadioStationPosition[station];
}

void
PopulateRadioStationPositionList()
{
	for (int i = 0; i < NUM_RADIOS; i++)
		RadioStationPosition[i] = DMAudio.GetRadioPosition(i);
}

#define ReadDataFromBufferPointer(buf, to) memcpy(&to, buf, sizeof(to)); buf += align4bytes(sizeof(to));
#define WriteDataToBufferPointer(buf, from) memcpy(buf, &from, sizeof(from)); buf += align4bytes(sizeof(from));

#define LoadSaveDataBlock()\
do {\
	if (!ReadDataFromFile(file, (uint8 *) &size, 4))\
		return false;\
	if(size > sizeof(work_buff) - 3) {\
		PcSaveHelper.nErrorCode = SAVESTATUS_ERR_DATA_INVALID;\
		REVC_LOG_ERROR("Save block exceeds work buffer: %u bytes", size);\
		CloseFile(file);\
		return false;\
	}\
	size = align4bytes(size);\
	if (!ReadDataFromFile(file, work_buff, size))\
		return false;\
	buf = work_buff;\
} while (0)

#define ReadDataFromBlock(msg,load_func)\
do {\
	debug(msg);\
	ReadDataFromBufferPointer(buf, size);\
	load_func(buf, size);\
	size = align4bytes(size);\
	buf += size;\
} while (0)

#define WriteSaveDataBlock(save_func, msg)\
do {\
	size = 0;\
	buf = work_buff;\
	reserved = 0;\
	MakeSpaceForSizeInBufferPointer(presize, buf, postsize);\
	save_func(buf, &size);\
	debug(msg"== %i \n", size);\
	CopySizeAndPreparePointer(presize, buf, postsize, reserved, size);\
	if (!FitsLegacySave(totalSize, align4bytes(size), SIZE_OF_ONE_GAME_IN_BYTES)) {\
		REVC_LOG_ERROR("Save exceeds legacy file size at " msg " (%u bytes)", size);\
		PcSaveHelper.nErrorCode = SAVESTATUS_ERR_SAVE_WRITE;\
		return false;\
	}\
	if (!PcSaveHelper.PcClassSaveRoutine(file, work_buff, buf - work_buff))\
		return false;\
	totalSize += buf - work_buff;\
} while (0)

// Only the three entity pools can grow arbitrarily with custom content. Size
// them before writing: the global work_buff is 55 KB, while a valid legacy PC
// save can still have a larger individual pool block. Reject overflow of the
// whole save instead of scribbling past work_buff or wrapping padding maths.
static bool
WritePoolSaveBlock(int file, uint32 &totalSize, void (*save)(uint8 *, uint32 *), const char *name)
{
	uint32 expected = 0;
	save(nil, &expected);
	// The pool sits behind an INNER size word as well as the outer block
	// length; both words must be included in the on-disk preflight.
	if(expected > SIZE_OF_ONE_GAME_IN_BYTES ||
	   !FitsLegacySave(totalSize, expected + 4, SIZE_OF_ONE_GAME_IN_BYTES)){
		REVC_LOG_ERROR("Cannot save %s: %u bytes exceed the legacy save format", name, expected);
		PcSaveHelper.nErrorCode = SAVESTATUS_ERR_SAVE_WRITE;
		return false;
	}
	uint32 padded = align4bytes(expected);
	std::vector<uint8> block(4 + padded, 0);
	memcpy(block.data(), &expected, sizeof(expected));
	uint32 actual = 0;
	save(block.data() + 4, &actual);
	if(actual != expected){
		REVC_LOG_ERROR("Cannot save %s: pool changed during serialization (%u -> %u)", name, expected, actual);
		PcSaveHelper.nErrorCode = SAVESTATUS_ERR_SAVE_WRITE;
		return false;
	}
	debug("%s== %u \n", name, actual);
	if(!PcSaveHelper.PcClassSaveRoutine(file, block.data(), (uint32)block.size()))
		return false;
	totalSize += (uint32)block.size();
	return true;
}

// Type words stored by the legacy save format: 0 car, 1 boat, 5 bike.
static bool
IsLoadedSaveVehicleModel(int16 id, uint32 type)
{
	if (id < 0 || id >= MODELINFOSIZE)
		return false;
	CBaseModelInfo *base = CModelInfo::GetModelInfo(id);
	if (base == nil || base->GetModelType() != MITYPE_VEHICLE)
		return false;
	const eVehicleType vehicleType = ((CVehicleModelInfo*)base)->m_nVehicleType;
	// The "car" class of the stock save format also covers helis and planes.
	if (type == 0)
		return vehicleType == VEHICLE_TYPE_AUTOMOBILE || vehicleType == VEHICLE_TYPE_HELI ||
		       vehicleType == VEHICLE_TYPE_PLANE || vehicleType == VEHICLE_TYPE_FHELI ||
		       vehicleType == VEHICLE_TYPE_FPLANE;
	if (type == 1)
		return vehicleType == VEHICLE_TYPE_BOAT;
	if (type == 5)
		return vehicleType == VEHICLE_TYPE_BIKE;
	return false;
}

static bool
IsLoadedSaveObjectModel(int16 id)
{
	if (id < 0 || id >= MODELINFOSIZE)
		return false;
	CBaseModelInfo *base = CModelInfo::GetModelInfo(id);
	return base != nil && (base->IsSimple() || base->GetModelType() == MITYPE_CLUMP);
}

static SavedPoolCheck
CheckSavedVehiclePool(const uint8 *buf, std::size_t size)
{
	return ValidateSavedVehiclePool(buf, size,
		CAutomobile::nSaveStructSize, CBoat::nSaveStructSize, CBike::nSaveStructSize,
		0, 1, 5,
		IsLoadedSaveVehicleModel);
}

static SavedPoolCheck
CheckSavedObjectPool(const uint8 *buf, std::size_t size)
{
	// This is the packed record width of CPools::SaveObjectPool; compatible
	// saves write the last two entity-property words via SaveEntityFlags.
	const std::size_t recordBytes = sizeof(int16) + sizeof(int32) +
		2 * sizeof(CCompressedMatrix) + sizeof(float) + sizeof(int8) +
		7 * sizeof(bool) + sizeof(int16) + 2 * sizeof(int8) +
		sizeof(float) + 2 * sizeof(int8) + sizeof(uint32) + 2 * sizeof(uint32);
	return ValidateSavedObjectPool(buf, size, recordBytes, IsLoadedSaveObjectModel);
}

static bool
ReadPoolSaveBlock(int file, void (*load)(uint8 *, uint32), const char *name,
                  SavedPoolCheck (*validate)(const uint8 *, std::size_t) = nil)
{
	uint32 outerSize = 0;
	if(!ReadDataFromFile(file, (uint8*)&outerSize, sizeof(outerSize)))
		return false;
	if(outerSize < 4 || outerSize > SIZE_OF_ONE_GAME_IN_BYTES){
		REVC_LOG_ERROR("Invalid %s save block length: %u", name, outerSize);
		PcSaveHelper.nErrorCode = SAVESTATUS_ERR_DATA_INVALID;
		return false;
	}
	std::vector<uint8> block(align4bytes(outerSize));
	if(!ReadDataFromFile(file, block.data(), (uint32)block.size()))
		return false;
	uint32 innerSize = 0;
	memcpy(&innerSize, block.data(), sizeof(innerSize));
	if(innerSize > outerSize - 4 || align4bytes(innerSize) > outerSize - 4){
		REVC_LOG_ERROR("Invalid nested %s save block length: %u / %u", name, innerSize, outerSize);
		PcSaveHelper.nErrorCode = SAVESTATUS_ERR_DATA_INVALID;
		return false;
	}
	if(validate != nil){
		SavedPoolCheck check = validate(block.data() + 4, innerSize);
		if(!check.ok()){
			if(check.error == SavedPoolError::UnavailableModel)
				REVC_LOG_ERROR("Cannot load %s save: model %d in record %u (type %u) is unavailable/incompatible with current IDEs. Use a save made for this map.",
					name, check.model, (uint32)check.entry, check.type);
			else
				REVC_LOG_ERROR("Cannot load %s save: invalid record layout/type at entry %u (type %u)",
					name, (uint32)check.entry, check.type);
			PcSaveHelper.nErrorCode = SAVESTATUS_ERR_DATA_INVALID;
			return false;
		}
	}
	REVC_LOG_INFO("Loading saved %s pool (%u bytes)", name, innerSize);
	load(block.data() + 4, innerSize);
	return true;
}

bool
GenericSave(int file)
{
	uint8 *buf, *presize, *postsize;
	uint32 size;
	uint32 reserved;

	uint32 totalSize;
	
	wchar *lastMissionPassed;
	wchar suffix[6];
	wchar saveName[24];
	SYSTEMTIME saveTime;
	CPad *currPad;

	CheckSum = 0;
	buf = work_buff;
	reserved = 0;

	// Save simple vars
	lastMissionPassed = TheText.Get(CStats::LastMissionPassedName[0] ? CStats::LastMissionPassedName : "ITBEG");
	AsciiToUnicode("...'", suffix);
	suffix[3] = L'\0';
#ifdef FIX_BUGS
	// fix buffer overflow
	int len = UnicodeStrlen(lastMissionPassed);
	if (len > ARRAY_SIZE(saveName)-1)
		len = ARRAY_SIZE(saveName)-1;
	memcpy(saveName, lastMissionPassed, sizeof(wchar) * len);
#else
	TextCopy(saveName, lastMissionPassed);
	int len = UnicodeStrlen(saveName);
#endif
	saveName[len] = '\0';
	if (len > ARRAY_SIZE(saveName)-2)
		TextCopy(&saveName[ARRAY_SIZE(saveName)-ARRAY_SIZE(suffix)], suffix);
	saveName[ARRAY_SIZE(saveName)-1] = '\0';
	WriteDataToBufferPointer(buf, saveName);
	GetLocalTime(&saveTime);
	WriteDataToBufferPointer(buf, saveTime);
#ifdef MISSION_REPLAY
	int32 data = IsQuickSave << 24 | SIZE_OF_ONE_GAME_IN_BYTES;
	WriteDataToBufferPointer(buf, data);
#else
	WriteDataToBufferPointer(buf, SIZE_OF_ONE_GAME_IN_BYTES);
#endif
	WriteDataToBufferPointer(buf, CGame::currLevel);
	WriteDataToBufferPointer(buf, TheCamera.GetPosition().x);
	WriteDataToBufferPointer(buf, TheCamera.GetPosition().y);
	WriteDataToBufferPointer(buf, TheCamera.GetPosition().z);
	WriteDataToBufferPointer(buf, CClock::ms_nMillisecondsPerGameMinute);
	WriteDataToBufferPointer(buf, CClock::ms_nLastClockTick);
	WriteDataToBufferPointer(buf, CClock::ms_nGameClockHours);
	WriteDataToBufferPointer(buf, CClock::ms_nGameClockMinutes);
	currPad = CPad::GetPad(0);
	WriteDataToBufferPointer(buf, currPad->Mode);
	WriteDataToBufferPointer(buf, CTimer::m_snTimeInMilliseconds);
	WriteDataToBufferPointer(buf, CTimer::ms_fTimeScale);
	WriteDataToBufferPointer(buf, CTimer::ms_fTimeStep);
	WriteDataToBufferPointer(buf, CTimer::ms_fTimeStepNonClipped);
	WriteDataToBufferPointer(buf, CTimer::m_FrameCounter);
	WriteDataToBufferPointer(buf, CTimeStep::ms_fTimeStep);
	WriteDataToBufferPointer(buf, CTimeStep::ms_fFramesPerUpdate);
	WriteDataToBufferPointer(buf, CTimeStep::ms_fTimeScale);
	WriteDataToBufferPointer(buf, CWeather::OldWeatherType);
	WriteDataToBufferPointer(buf, CWeather::NewWeatherType);
	WriteDataToBufferPointer(buf, CWeather::ForcedWeatherType);
	WriteDataToBufferPointer(buf, CWeather::InterpolationValue);
	WriteDataToBufferPointer(buf, CWeather::WeatherTypeInList);
#ifdef COMPATIBLE_SAVES
	// converted to float for compatibility with original format
	// TODO: maybe remove this? not really gonna break anything vital
	float f = TheCamera.CarZoomIndicator;
	WriteDataToBufferPointer(buf, f);
	f = TheCamera.PedZoomIndicator;
	WriteDataToBufferPointer(buf, f);
#else
	WriteDataToBufferPointer(buf, TheCamera.CarZoomIndicator);
	WriteDataToBufferPointer(buf, TheCamera.PedZoomIndicator);
#endif
	WriteDataToBufferPointer(buf, CGame::currArea);
	WriteDataToBufferPointer(buf, CVehicle::bAllTaxisHaveNitro);
	WriteDataToBufferPointer(buf, CPad::bInvertLook4Pad);
	WriteDataToBufferPointer(buf, CTimeCycle::m_ExtraColour);
	WriteDataToBufferPointer(buf, CTimeCycle::m_bExtraColourOn);
	WriteDataToBufferPointer(buf, CTimeCycle::m_ExtraColourInter);
	PopulateRadioStationPositionList();
	WriteDataToBufferPointer(buf, RadioStationPosition);
	assert(buf - work_buff == SIZE_OF_SIMPLEVARS_VC);
	WriteSaveVersion(buf);
	assert(buf - work_buff == SIZE_OF_SIMPLEVARS);

	// The script block is nested within the same block as simple vars; it is
	// written empty so the save layout stays as the original game expects.
	presize = buf;
	buf += 4;
	postsize = buf;
	size = 0;
	CopySizeAndPreparePointer(presize, buf, postsize, reserved, size);
	if (!PcSaveHelper.PcClassSaveRoutine(file, work_buff, buf - work_buff))
		return false;

	totalSize = buf - work_buff;

	// Save the rest
	if(!WritePoolSaveBlock(file, totalSize, CPools::SavePedPool, "PedPoolSize")) return false;
	if(!WritePoolSaveBlock(file, totalSize, CPools::SaveVehiclePool, "VehPoolSize")) return false;
	// Size the object pool only after removing transient objects: otherwise
	// its count could change between the sizing pass and serialization.
	CProjectileInfo::RemoveAllProjectiles();
	CObject::DeleteAllTempObjects();
	if(!WritePoolSaveBlock(file, totalSize, CPools::SaveObjectPool, "ObjectPoolSize")) return false;
	WriteSaveDataBlock(CPickups::Save, "PickUpsSize");
	WriteSaveDataBlock(CRadar::SaveAllRadarBlips, "RadarBlipsBufferSize");
	WriteSaveDataBlock(CTheZones::SaveAllZones, "AllZonesBufferSize");
	WriteSaveDataBlock(CParticleObject::SaveParticle, "ParticlesSize");
	WriteSaveDataBlock(cAudioScriptObject::SaveAllAudioScriptObjects, "AllAudioScriptObjectsSize");
	WriteSaveDataBlock(CWorld::Players[CWorld::PlayerInFocus].SavePlayerInfo, "PlayerInfoSize");
	WriteSaveDataBlock(CStats::SaveStats, "StatsSize");
	WriteSaveDataBlock(CStreaming::MemoryCardSave, "StreamingSize");
	if(SIZE_OF_ONE_GAME_IN_BYTES - totalSize - 4 <= 4){
		REVC_LOG_ERROR("No room for a valid padding block in the legacy save format");
		PcSaveHelper.nErrorCode = SAVESTATUS_ERR_SAVE_WRITE;
		return false;
	}

	// sure just write garbage data repeatedly ...
#ifndef THIS_IS_STUPID
	memset(work_buff, 0, sizeof(work_buff));
#endif

	// Write padding
	for (int i = 0; i < 4; i++) {
		size = align4bytes(SIZE_OF_ONE_GAME_IN_BYTES - totalSize - 4);
		if (size > sizeof(work_buff))
			size = sizeof(work_buff);
		if (size > 4) {
			if (!PcSaveHelper.PcClassSaveRoutine(file, work_buff, size))
				return false;
			totalSize += size;
		}
	}
	
	// Write checksum and close
	CFileMgr::Write(file, (const char *) &CheckSum, sizeof(CheckSum));
	if (CFileMgr::GetErrorReadWrite(file)) {
		PcSaveHelper.nErrorCode = SAVESTATUS_ERR_SAVE_WRITE;
		if (!CloseFile(file))
			PcSaveHelper.nErrorCode = SAVESTATUS_ERR_SAVE_CLOSE;

		return false;
	}

	CPad::FixPadsAfterSave();	
	return true;
}

bool
GenericLoad()
{
	REVC_LOG_INFO("Loading saved game from %s", LoadFileName);
	uint8 *buf;
	int32 file;
	uint32 size;
#ifdef MISSION_REPLAY
	int8 qs;
#endif

	int32 saveSize;
	CPad *currPad;

	// Load SimpleVars and Scripts
	CheckSum = 0;
	CDate dummy; // unused
	CPad::ResetCheats();
	if (!ReadInSizeofSaveFileBuffer(file, size))
		return false;
	if(size < SIZE_OF_SIMPLEVARS + sizeof(uint32) || size > sizeof(work_buff) - 3){
		PcSaveHelper.nErrorCode = SAVESTATUS_ERR_DATA_INVALID;
		CloseFile(file);
		return false;
	}
	size = align4bytes(size);
	if(!ReadDataFromFile(file, work_buff, size)) return false;
	buf = (work_buff + 0x40);
	ReadDataFromBufferPointer(buf, saveSize);
#ifdef MISSION_REPLAY // a hack to keep compatibility but get new data from save
	qs = saveSize >> 24;
#endif
	ReadDataFromBufferPointer(buf, CGame::currLevel);
	ReadDataFromBufferPointer(buf, TheCamera.GetMatrix().GetPosition().x);
	ReadDataFromBufferPointer(buf, TheCamera.GetMatrix().GetPosition().y);
	ReadDataFromBufferPointer(buf, TheCamera.GetMatrix().GetPosition().z);
	ReadDataFromBufferPointer(buf, CClock::ms_nMillisecondsPerGameMinute);
	ReadDataFromBufferPointer(buf, CClock::ms_nLastClockTick);
	ReadDataFromBufferPointer(buf, CClock::ms_nGameClockHours);
	ReadDataFromBufferPointer(buf, CClock::ms_nGameClockMinutes);
	currPad = CPad::GetPad(0);
	ReadDataFromBufferPointer(buf, currPad->Mode);
	ReadDataFromBufferPointer(buf, CTimer::m_snTimeInMilliseconds);
	ReadDataFromBufferPointer(buf, CTimer::ms_fTimeScale);
	ReadDataFromBufferPointer(buf, CTimer::ms_fTimeStep);
	ReadDataFromBufferPointer(buf, CTimer::ms_fTimeStepNonClipped);
	ReadDataFromBufferPointer(buf, CTimer::m_FrameCounter);
	ReadDataFromBufferPointer(buf, CTimeStep::ms_fTimeStep);
	ReadDataFromBufferPointer(buf, CTimeStep::ms_fFramesPerUpdate);
	ReadDataFromBufferPointer(buf, CTimeStep::ms_fTimeScale);
	ReadDataFromBufferPointer(buf, CWeather::OldWeatherType);
	ReadDataFromBufferPointer(buf, CWeather::NewWeatherType);
	ReadDataFromBufferPointer(buf, CWeather::ForcedWeatherType);
	ReadDataFromBufferPointer(buf, CWeather::InterpolationValue);
	ReadDataFromBufferPointer(buf, CWeather::WeatherTypeInList);
#ifdef COMPATIBLE_SAVES
	// converted to float for compatibility with original format
	// TODO: maybe remove this? not really gonna break anything vital
	float f;
	ReadDataFromBufferPointer(buf, f);
	TheCamera.CarZoomIndicator = f;
	ReadDataFromBufferPointer(buf, f);
	TheCamera.PedZoomIndicator = f;
#else
	ReadDataFromBufferPointer(buf, TheCamera.CarZoomIndicator);
	ReadDataFromBufferPointer(buf, TheCamera.PedZoomIndicator);
#endif
	ReadDataFromBufferPointer(buf, CGame::currArea);
	ReadDataFromBufferPointer(buf, CVehicle::bAllTaxisHaveNitro);
#ifdef LOAD_INI_SETTINGS
	buf += align4bytes(sizeof(CPad::bInvertLook4Pad));
#else
	ReadDataFromBufferPointer(buf, CPad::bInvertLook4Pad);
#endif
	ReadDataFromBufferPointer(buf, CTimeCycle::m_ExtraColour);
	ReadDataFromBufferPointer(buf, CTimeCycle::m_bExtraColourOn);
	ReadDataFromBufferPointer(buf, CTimeCycle::m_ExtraColourInter);
	ReadDataFromBufferPointer(buf, RadioStationPosition);
	assert(buf - work_buff == SIZE_OF_SIMPLEVARS_VC);
	{
		// A save this build did not write stops here, before a single pool
		// record is reconstructed: the per-type arrays and the ped state
		// numbering changed, so the blocks past this point mean something
		// else in an older file.
		uint32 found = 0;
		if(!ReadSaveVersion(buf, &found)){
			PcSaveHelper.nErrorCode = SAVESTATUS_ERR_DATA_INVALID;
			REVC_LOG_ERROR("Cannot load %s: the file was written by an incompatible build "
			               "(save format version %u, this build reads %u). Start a new game.",
			               LoadFileName, found, (uint32)SAVE_VERSION);
			CloseFile(file);
			return false;
		}
	}
	assert(buf - work_buff == SIZE_OF_SIMPLEVARS);
	debug("Loading Scripts \n");
	{
		const uint32 simpleAndScriptsBytes = size;
		uint32 scriptsSize = 0;
		ReadDataFromBufferPointer(buf, scriptsSize);
		const size_t usedBytes = (size_t)(buf - work_buff);
		if (usedBytes > simpleAndScriptsBytes || scriptsSize > simpleAndScriptsBytes - usedBytes) {
			PcSaveHelper.nErrorCode = SAVESTATUS_ERR_DATA_INVALID;
			REVC_LOG_ERROR("Invalid Scripts save block length: %u (available %zu)",
				scriptsSize, (size_t)(simpleAndScriptsBytes - usedBytes));
			CloseFile(file);
			return false;
		}
		buf += align4bytes(scriptsSize);
	}

	// Load the rest
	if(!ReadPoolSaveBlock(file, CPools::LoadPedPool, "PedPool")) { CloseFile(file); return false; }
	LoadSaveDataBlock();
	if(!ReadPoolSaveBlock(file, CPools::LoadVehiclePool, "Vehicles", CheckSavedVehiclePool)) { CloseFile(file); return false; }
	CProjectileInfo::RemoveAllProjectiles();
	CObject::DeleteAllTempObjects();
	if(!ReadPoolSaveBlock(file, CPools::LoadObjectPool, "Objects", CheckSavedObjectPool)) { CloseFile(file); return false; }
	LoadSaveDataBlock();
	LoadSaveDataBlock();
	ReadDataFromBlock("Loading Pickups \n", CPickups::Load);
	LoadSaveDataBlock();
	ReadDataFromBlock("Loading Radar Blips \n", CRadar::LoadAllRadarBlips);
	LoadSaveDataBlock();
	ReadDataFromBlock("Loading Zones \n", CTheZones::LoadAllZones);
	LoadSaveDataBlock();
	CParticle::ReloadConfig();
	LoadSaveDataBlock();
	ReadDataFromBlock("Loading Particles \n", CParticleObject::LoadParticle);
	LoadSaveDataBlock();
	ReadDataFromBlock("Loading AudioScript Objects \n", cAudioScriptObject::LoadAllAudioScriptObjects);
	LoadSaveDataBlock();
	ReadDataFromBlock("Loading Player Info \n", CWorld::Players[CWorld::PlayerInFocus].LoadPlayerInfo);
	LoadSaveDataBlock();
	ReadDataFromBlock("Loading Stats \n", CStats::LoadStats);
	LoadSaveDataBlock();
	ReadDataFromBlock("Loading Streaming Stuff \n", CStreaming::MemoryCardLoad);
	LoadSaveDataBlock();

	DMAudio.SetMusicMasterVolume(FrontEndMenuManager.m_PrefsMusicVolume);
	DMAudio.SetEffectsMasterVolume(FrontEndMenuManager.m_PrefsSfxVolume);
	if (!CloseFile(file)) {
		PcSaveHelper.nErrorCode = SAVESTATUS_ERR_LOAD_CLOSE;
		REVC_LOG_ERROR("Failed to close saved game %s", LoadFileName);
		return false;
	}

	DoGameSpecificStuffAfterSucessLoad();
	REVC_LOG_INFO("Saved game loaded successfully from %s", LoadFileName);
	debug("Game successfully loaded \n");
	return true;
}

bool
ReadInSizeofSaveFileBuffer(int32 &file, uint32 &size)
{
	file = CFileMgr::OpenFile(LoadFileName, "rb");
	if (file == 0) {
		PcSaveHelper.nErrorCode = SAVESTATUS_ERR_LOAD_OPEN;
		REVC_LOG_ERROR("Could not open saved game %s", LoadFileName);
		return false;
	}
	CFileMgr::Read(file, (char*)&size, sizeof(size));
	if (CFileMgr::GetErrorReadWrite(file)) {
		PcSaveHelper.nErrorCode = SAVESTATUS_ERR_LOAD_READ;
		REVC_LOG_ERROR("Could not read saved game header from %s", LoadFileName);
		if (!CloseFile(file))
			PcSaveHelper.nErrorCode = SAVESTATUS_ERR_LOAD_CLOSE;
		return false;
	}
	return true;
}

bool
ReadDataFromFile(int32 file, uint8 *buf, uint32 size)
{
	if (file == 0) {
		PcSaveHelper.nErrorCode = SAVESTATUS_ERR_LOAD_OPEN;
		REVC_LOG_ERROR("Invalid file handle while loading %s", LoadFileName);
		return false;
	}
	size_t read_size = CFileMgr::Read(file, (char*)buf, size);
	if (CFileMgr::GetErrorReadWrite(file) || read_size != size) {
		PcSaveHelper.nErrorCode = SAVESTATUS_ERR_LOAD_READ;
		REVC_LOG_ERROR("Short read from %s (expected %u bytes, got %zu)", LoadFileName, size, read_size);
		if (!CloseFile(file))
			PcSaveHelper.nErrorCode = SAVESTATUS_ERR_LOAD_CLOSE;
		return false;
	}
	return true;
}

bool
CloseFile(int32 file)
{
	return CFileMgr::CloseFile(file) == 0;
}

void
DoGameSpecificStuffAfterSucessLoad()
{
	CCollision::SortOutCollisionAfterLoad();
	CStreaming::LoadSceneCollision(TheCamera.GetPosition());
	CStreaming::LoadScene(TheCamera.GetPosition());
	CGame::TidyUpMemory(true, false);
	StillToFadeOut = true;
	JustLoadedDontFadeInYet = true;
	TheCamera.Fade(0.0f, FADE_OUT);
}

bool
CheckSlotDataValid(int32 slot)
{
	PcSaveHelper.nErrorCode = SAVESTATUS_SUCCESSFUL;
	if (CheckDataNotCorrupt(slot, LoadFileName)) {
		CStreaming::DeleteAllRwObjects();
		return true;
	}

	PcSaveHelper.nErrorCode = SAVESTATUS_ERR_DATA_INVALID;
	return false;
}

void
MakeSpaceForSizeInBufferPointer(uint8 *&presize, uint8 *&buf, uint8 *&postsize)
{
	presize = buf;
	buf += sizeof(uint32);
	postsize = buf;
}

void
CopySizeAndPreparePointer(uint8 *&buf, uint8 *&postbuf, uint8 *&postbuf2, uint32 &unused, uint32 &size)
{
	memcpy(buf, &size, sizeof(size));
	size = align4bytes(size);
	postbuf2 += size;
	postbuf = postbuf2;
}

void
DoGameSpecificStuffBeforeSave()
{
	CGameLogic::PassTime(360);
	CPlayerPed *ped = FindPlayerPed();
	// A save is taken with a rested player, so the budget written out is the
	// full one the stamina stat is worth.
	ped->ResetSprintEnergy();
	CGame::TidyUpMemory(true, false);
}


void
MakeValidSaveName(int32 slot)
{
	ValidSaveName[0] = '\0';
	sprintf(ValidSaveName, "%s%i", DefaultPCSaveFileName, slot + 1);
	strncat(ValidSaveName, ".b", 5);
}

wchar *
GetSavedGameDateAndTime(int32 slot)
{
	return SlotSaveDate[slot];
}

wchar *
GetNameOfSavedGame(int32 slot)
{
	return SlotFileName[slot];
}

bool
CheckDataNotCorrupt(int32 slot, char *name)
{
#ifdef FIX_BUGS
	char filename[MAX_PATH];
#else
	char filename[100];
#endif

	int32 blocknum = 0;
	eLevelName level = LEVEL_GENERIC;
	CheckSum = 0;
	uint32 bytes_processed = 0;
	sprintf(filename, "%s%i%s", DefaultPCSaveFileName, slot + 1, ".b");
	int file = CFileMgr::OpenFile(filename, "rb");
	if (file == 0)
		return false;
	strcpy(name, filename);
	while (SIZE_OF_ONE_GAME_IN_BYTES - sizeof(uint32) > bytes_processed && blocknum < 40) {
		int32 blocksize;
		if (!ReadDataFromFile(file, (uint8*)&blocksize, sizeof(blocksize))) {
			CloseFile(file);
			return false;
		}
		if(blocksize < 0 || (uint64)bytes_processed +
		   (((uint64)(uint32)blocksize + 3u) & ~uint64(3u)) >
		   SIZE_OF_ONE_GAME_IN_BYTES - sizeof(CheckSum) + 3u ||
		   (blocknum == 0 && blocksize < 4 + (int32)sizeof(level))){
			CloseFile(file);
			return false;
		}
		CheckSum += ((uint8*)&blocksize)[0];
		CheckSum += ((uint8*)&blocksize)[1];
		CheckSum += ((uint8*)&blocksize)[2];
		CheckSum += ((uint8*)&blocksize)[3];
		uint32 remaining = align4bytes(blocksize);
		bool firstChunk = blocknum == 0;
		while(remaining){
			uint32 chunk = Min(remaining, (uint32)sizeof(work_buff));
			if(!ReadDataFromFile(file, work_buff, chunk)){
				CloseFile(file);
				return false;
			}
			if(firstChunk){
				memcpy(&level, work_buff + 4, sizeof(level));
				firstChunk = false;
			}
			for(uint32 i = 0; i < chunk; i++)
				CheckSum += work_buff[i];
			remaining -= chunk;
			bytes_processed += chunk;
		}
		blocknum++;
	}
	int32 _checkSum;
	if (ReadDataFromFile(file, (uint8*)&_checkSum, sizeof(_checkSum))) {
		if (CloseFile(file)) {
			if (CheckSum == _checkSum) {
				m_LevelToLoad = level;
				return true;
			}
			return false;
		}
		return false;
	}

	CloseFile(file);
	return false;
}

bool
RestoreForStartLoad()
{
	uint8 buf[999];

	int file = CFileMgr::OpenFile(LoadFileName, "rb");
	if (file == 0) {
		PcSaveHelper.nErrorCode = SAVESTATUS_ERR_LOAD_OPEN;
		return false;
	}
	ReadDataFromFile(file, buf, sizeof(buf));
	if (CFileMgr::GetErrorReadWrite(file)) {
		PcSaveHelper.nErrorCode = SAVESTATUS_ERR_LOAD_READ;
		if (!CloseFile(file))
			PcSaveHelper.nErrorCode = SAVESTATUS_ERR_LOAD_CLOSE;
		return false;
	} else {
		uint8 *_buf = buf + sizeof(int32) + sizeof(wchar[24]) + sizeof(SYSTEMTIME) + sizeof(SIZE_OF_ONE_GAME_IN_BYTES);
		ReadDataFromBufferPointer(_buf, CGame::currLevel);
		ReadDataFromBufferPointer(_buf, TheCamera.GetMatrix().GetPosition().x);
		ReadDataFromBufferPointer(_buf, TheCamera.GetMatrix().GetPosition().y);
		ReadDataFromBufferPointer(_buf, TheCamera.GetMatrix().GetPosition().z);
		CStreaming::RemoveUnusedBigBuildings(CGame::currLevel);
		CStreaming::RemoveUnusedBuildings(CGame::currLevel);

		if (CloseFile(file)) {
			return true;
		} else {
			PcSaveHelper.nErrorCode = SAVESTATUS_ERR_LOAD_CLOSE;
			return false;
		}
	}
}

int
align4bytes(int32 size)
{
	return (size + 3) & 0xFFFFFFFC;
}

#ifdef MISSION_REPLAY

void DisplaySaveResult(int unk, char* name)
{}

bool SaveGameForPause(int type)
{
	if (gGameState != GS_PLAYING_GAME) {
		DisplaySaveResult(3, CStats::LastMissionPassedName);
		return false;
	}
	debug("SaveGameForPause ******************************** %s doSave %d", CStats::LastMissionPassedName, true);
	IsQuickSave = type;
	int res = PcSaveHelper.SaveSlot(PAUSE_SAVE_SLOT);
	PcSaveHelper.PopulateSlotInfo();
	IsQuickSave = 0;
	DisplaySaveResult(res, CStats::LastMissionPassedName);
	return true;
}
#endif
