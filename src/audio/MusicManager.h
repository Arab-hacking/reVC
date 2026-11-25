#ifndef __GTA_MUSICMANAGER_H__
#define __GTA_MUSICMANAGER_H__

#include "audio_enums.h"

class cMusicManager
{
public:
	bool8 m_bIsInitialised;
	tTrack m_nPlayingTrack;

	cMusicManager();
	bool8 IsInitialised() const { return m_bIsInitialised; }
	bool8 IsGameMode() const { return m_nMusicMode == MUSICMODE_GAME; }
	tTrack GetCurrentTrack() const { return m_nPlayingTrack; }

	bool8 Initialise();
	void Terminate();
	void ResetTimers(uint32 time);
	void ResetMusicAfterReload();
	void Service();
	void ChangeMusicMode(uint8 mode);
	void SetMalibuClubTrackPos(uint8 scriptObject);
	void SetStripClubTrackPos(uint8 scriptObject);
	void PreloadCutSceneMusic(tTrack track);
	void PlayPreloadedCutSceneMusic();
	void StopCutSceneMusic();

private:
	struct tAmbientTrackPosition {
		uint32 m_nLength;
		uint32 m_nPosition;
	};

	static const uint32 NUM_AMBIENT_TRACKS = STREAMED_SOUND_AMBSIL_AMBIENT - STREAMED_SOUND_CITY_AMBIENT + 1;
	tAmbientTrackPosition m_aAmbientTracks[NUM_AMBIENT_TRACKS];
	uint8 m_nMusicMode;
	uint32 m_nResetTime;

	void StopAmbientTrack();
	void SetUpCorrectAmbienceTrack(tTrack &track);
	void ComputeAmbienceVol(bool8 reset, uint8 &outVolume);
};

extern cMusicManager MusicManager;
float GetHeightScale();

#endif // __GTA_MUSICMANAGER_H__
