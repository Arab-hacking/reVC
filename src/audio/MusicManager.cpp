#include "common.h"
#include "MusicManager.h"
#include "AudioManager.h"
#include "Camera.h"
#include "ZoneCull.h"
#include "Game.h"
#include "AudioSamples.h"
#include "soundlist.h"
#include "Timer.h"
#include "Weather.h"
#include "sampman.h"

#ifdef GTA_PS2
#include <libcdvd.h>
extern "C" void WaitVBlank(void);
#define SURROUND_PAN(LeftRight, FrontRear) LeftRight, FrontRear
#else
#define SURROUND_PAN(LeftRight, FrontRear) LeftRight
#endif

namespace {

enum {
	MAX_WATER_DIST = 140,
	WATER_DIST_STEP = 50,
	MAX_AMBIENCE_VOLUME = 60,
};

bool IsAmbientTrack(tTrack track)
{
	return track >= STREAMED_SOUND_CITY_AMBIENT && track <= STREAMED_SOUND_AMBSIL_AMBIENT;
}

uint32 AmbientTrackIndex(tTrack track)
{
	return track - STREAMED_SOUND_CITY_AMBIENT;
}

} // namespace

cMusicManager MusicManager;

cMusicManager::cMusicManager()
	: m_bIsInitialised(FALSE), m_nPlayingTrack(NO_TRACK), m_nMusicMode(MUSICMODE_DISABLED), m_nResetTime(0)
{
	for (uint32 i = 0; i < NUM_AMBIENT_TRACKS; i++) {
		m_aAmbientTracks[i].m_nLength = 1;
		m_aAmbientTracks[i].m_nPosition = 0;
	}
}

bool8
cMusicManager::Initialise()
{
	if (m_bIsInitialised)
		return TRUE;

	m_bIsInitialised = TRUE;
	m_nPlayingTrack = NO_TRACK;
	m_nMusicMode = MUSICMODE_DISABLED;
	for (uint32 i = 0; i < NUM_AMBIENT_TRACKS; i++) {
		tTrack track = STREAMED_SOUND_CITY_AMBIENT + i;
		int32 length = SampleManager.GetStreamedFileLength(track);
		m_aAmbientTracks[i].m_nLength = length > 0 ? length : 1;
		m_aAmbientTracks[i].m_nPosition = AudioManager.m_anRandomTable[i % 5] % m_aAmbientTracks[i].m_nLength;
	}
	return TRUE;
}

void
cMusicManager::StopAmbientTrack()
{
	if (SampleManager.IsStreamPlaying()) {
		if (IsAmbientTrack(m_nPlayingTrack))
			m_aAmbientTracks[AmbientTrackIndex(m_nPlayingTrack)].m_nPosition = SampleManager.GetStreamedFilePosition();
		SampleManager.SetStreamedVolumeAndPan(0, SURROUND_PAN(63, 30), TRUE);
		SampleManager.StopStreamedFile();
	}
	m_nPlayingTrack = NO_TRACK;
}

void
cMusicManager::Terminate()
{
	if (!m_bIsInitialised)
		return;
	StopAmbientTrack();
	m_bIsInitialised = FALSE;
	m_nMusicMode = MUSICMODE_DISABLED;
}

void
cMusicManager::ResetTimers(uint32 time)
{
	m_nResetTime = time;
}

void
cMusicManager::ResetMusicAfterReload()
{
	StopAmbientTrack();
}

void
cMusicManager::ChangeMusicMode(uint8 mode)
{
	CGame::SetInCutscene(mode == MUSICMODE_CUTSCENE);
	m_nMusicMode = mode;
	if (mode != MUSICMODE_GAME)
		StopAmbientTrack();
}

void
cMusicManager::Service()
{
	if (!m_bIsInitialised || m_nMusicMode != MUSICMODE_GAME)
		return;

	tTrack targetTrack = NO_TRACK;
	SetUpCorrectAmbienceTrack(targetTrack);
	if (!IsAmbientTrack(targetTrack))
		return;

	if (m_nPlayingTrack != targetTrack || !SampleManager.IsStreamPlaying()) {
		if (SampleManager.IsStreamPlaying()) {
			StopAmbientTrack();
			return;
		}

		uint32 index = AmbientTrackIndex(targetTrack);
		uint32 length = m_aAmbientTracks[index].m_nLength;
		uint32 position = length > 0 ? m_aAmbientTracks[index].m_nPosition % length : 0;
		SampleManager.SetStreamedFileLoopFlag(TRUE);
		SampleManager.StartStreamedFile(targetTrack, position);
		m_nPlayingTrack = targetTrack;
		uint8 volume = 0;
		ComputeAmbienceVol(TRUE, volume);
		SampleManager.SetStreamedVolumeAndPan(volume, SURROUND_PAN(63, 30), TRUE);
		return;
	}

	m_aAmbientTracks[AmbientTrackIndex(m_nPlayingTrack)].m_nPosition = SampleManager.GetStreamedFilePosition();
	uint8 volume = 0;
	ComputeAmbienceVol(FALSE, volume);
	SampleManager.SetStreamedVolumeAndPan(volume, SURROUND_PAN(63, 30), TRUE);
}

void
cMusicManager::SetUpCorrectAmbienceTrack(tTrack &track)
{
	track = STREAMED_SOUND_CITY_AMBIENT;
	switch (CGame::currArea)
	{
	case AREA_MAIN_MAP:
	case AREA_EVERYWHERE:
		if (TheCamera.DistanceToWater > (MAX_WATER_DIST - WATER_DIST_STEP)) {
			if ((CWeather::OldWeatherType == WEATHER_HURRICANE || CWeather::NewWeatherType == WEATHER_HURRICANE) && CWeather::Wind > 1.0f)
				track = STREAMED_SOUND_HAVANA_CITY_AMBIENT;
			else
				track = STREAMED_SOUND_CITY_AMBIENT;
		} else if (CCullZones::bAtBeachForAudio) {
			if ((CWeather::OldWeatherType == WEATHER_HURRICANE || CWeather::NewWeatherType == WEATHER_HURRICANE) && CWeather::Wind > 1.0f)
				track = STREAMED_SOUND_HAVANA_BEACH_AMBIENT;
			else
				track = STREAMED_SOUND_BEACH_AMBIENT;
		} else {
			if ((CWeather::OldWeatherType == WEATHER_HURRICANE || CWeather::NewWeatherType == WEATHER_HURRICANE) && CWeather::Wind > 1.0f)
				track = STREAMED_SOUND_HAVANA_WATER_AMBIENT;
			else
				track = STREAMED_SOUND_WATER_AMBIENT;
		}
		break;
	case AREA_STRIP_CLUB: track = STREAMED_SOUND_STRIPCLUB_AMBIENT; break;
	case AREA_MALL: track = STREAMED_SOUND_MALL_AMBIENT; break;
	case AREA_DIRT:
	case AREA_BLOOD:
	case AREA_OVALRING: track = STREAMED_SOUND_DIRTRING_AMBIENT; break;
	case AREA_HOTEL: track = STREAMED_SOUND_HOTEL_AMBIENT; break;
	case AREA_MALIBU_CLUB: track = STREAMED_SOUND_MALIBU_AMBIENT; break;
	case AREA_MANSION:
	case AREA_BANK:
	case AREA_LAWYERS:
	case AREA_COFFEE_SHOP:
	case AREA_CONCERT_HALL:
	case AREA_STUDIO:
	case AREA_RIFLE_RANGE:
	case AREA_BIKER_BAR:
	case AREA_POLICE_STATION: track = STREAMED_SOUND_AMBSIL_AMBIENT; break;
	default: break;
	}
}

float
GetHeightScale()
{
	if (TheCamera.GetPosition().z <= 20.0f) return 1.0f;
	if (TheCamera.GetPosition().z >= 50.0f) return 0.0f;
	return 1.0f - (TheCamera.GetPosition().z - 20.0f) / 30.0f;
}

void
cMusicManager::ComputeAmbienceVol(bool8 reset, uint8 &outVolume)
{
	static float volume = 0.0f;
	float heightScale = GetHeightScale();
	if (reset)
		volume = 0.0f;
	else if (volume < MAX_AMBIENCE_VOLUME) {
		if (m_nPlayingTrack >= STREAMED_SOUND_HAVANA_CITY_AMBIENT && m_nPlayingTrack <= STREAMED_SOUND_HAVANA_BEACH_AMBIENT)
			volume += 20.0f;
		else
			volume += 1.0f;
		volume = Min(volume, MAX_AMBIENCE_VOLUME);
	}

	if (m_nPlayingTrack >= STREAMED_SOUND_MALL_AMBIENT && m_nPlayingTrack <= STREAMED_SOUND_AMBSIL_AMBIENT) {
		outVolume = volume;
		return;
	}
	if (CWeather::OldWeatherType == WEATHER_HURRICANE || CWeather::NewWeatherType == WEATHER_HURRICANE) {
		if (CWeather::Wind > 1.0f) {
			outVolume = (CWeather::Wind - 1.0f) * volume;
			return;
		}
		volume = (1.0f - CWeather::Wind) * volume;
	}
	if (TheCamera.DistanceToWater <= MAX_WATER_DIST) {
		if (TheCamera.DistanceToWater > (MAX_WATER_DIST - WATER_DIST_STEP))
			outVolume = ((TheCamera.DistanceToWater - (MAX_WATER_DIST - WATER_DIST_STEP)) / WATER_DIST_STEP * volume * heightScale);
		else if (TheCamera.DistanceToWater > (MAX_WATER_DIST - WATER_DIST_STEP * 2))
			outVolume = ((MAX_WATER_DIST - WATER_DIST_STEP) - heightScale) / WATER_DIST_STEP * volume;
		else
			outVolume = volume;
	} else
		outVolume = volume;
}

void
cMusicManager::SetMalibuClubTrackPos(uint8 scriptObject)
{
	if (!m_bIsInitialised)
		m_aAmbientTracks[AmbientTrackIndex(STREAMED_SOUND_MALIBU_AMBIENT)].m_nPosition = 8640;
	if (m_nPlayingTrack != STREAMED_SOUND_MALIBU_AMBIENT) {
		uint32 &position = m_aAmbientTracks[AmbientTrackIndex(STREAMED_SOUND_MALIBU_AMBIENT)].m_nPosition;
		switch (scriptObject) {
		case SCRIPT_SOUND_NEW_BUILDING_MALIBU_1: position = AudioManager.m_anRandomTable[0] % 128 + 8640; break;
		case SCRIPT_SOUND_NEW_BUILDING_MALIBU_2: position = AudioManager.m_anRandomTable[0] % 128 + 286720; break;
		case SCRIPT_SOUND_NEW_BUILDING_MALIBU_3: position = AudioManager.m_anRandomTable[0] % 128 + 509120; break;
		default: break;
		}
	}
}

void
cMusicManager::PreloadCutSceneMusic(tTrack track)
{
	if (!m_bIsInitialised || m_nMusicMode != MUSICMODE_CUTSCENE || track >= TOTAL_STREAMED_SOUNDS)
		return;

	while (SampleManager.IsStreamPlaying()) {
		SampleManager.StopStreamedFile();
#ifdef GTA_PS2
		WaitVBlank();
#endif
	}
	SampleManager.PreloadStreamedFile(track);
	SampleManager.SetStreamedVolumeAndPan(MAX_VOLUME, SURROUND_PAN(63, 30), TRUE);
	m_nPlayingTrack = track;
}

void
cMusicManager::PlayPreloadedCutSceneMusic()
{
	if (m_bIsInitialised && m_nMusicMode == MUSICMODE_CUTSCENE)
		SampleManager.StartPreloadedStreamedFile();
}

void
cMusicManager::StopCutSceneMusic()
{
	if (!m_bIsInitialised || m_nMusicMode != MUSICMODE_CUTSCENE)
		return;

#ifdef GTA_PS2
	while (SampleManager.IsStreamPlaying()) {
		SampleManager.StopStreamedFile();
		WaitVBlank();
	}
#else
	SampleManager.StopStreamedFile();
#endif
	m_nPlayingTrack = NO_TRACK;
}

void
cMusicManager::SetStripClubTrackPos(uint8 scriptObject)
{
	if (!m_bIsInitialised)
		m_aAmbientTracks[AmbientTrackIndex(STREAMED_SOUND_STRIPCLUB_AMBIENT)].m_nPosition = 0;
	if (m_nPlayingTrack != STREAMED_SOUND_STRIPCLUB_AMBIENT) {
		uint32 &position = m_aAmbientTracks[AmbientTrackIndex(STREAMED_SOUND_STRIPCLUB_AMBIENT)].m_nPosition;
		switch (scriptObject) {
		case SCRIPT_SOUND_NEW_BUILDING_STRIP_1: position = AudioManager.m_anRandomTable[0] % 128; break;
		case SCRIPT_SOUND_NEW_BUILDING_STRIP_2: position = AudioManager.m_anRandomTable[0] % 128 + 320200; break;
		case SCRIPT_SOUND_NEW_BUILDING_STRIP_3: position = AudioManager.m_anRandomTable[0] % 128 + 672000; break;
		default: break;
		}
	}
}
