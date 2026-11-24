#ifndef __GTA_DMAUDIO_H__
#define __GTA_DMAUDIO_H__

#include "audio_enums.h"
#include "soundlist.h"
#include "Crime.h"

#define AEHANDLE_IS_FAILED(h) ((h)<0)
#define AEHANDLE_IS_OK(h)     ((h)>=0)

#define NO_AUDIO_PROVIDER -3
#define AUDIO_PROVIDER_NOT_DETERMINED -99

class cAudioScriptObject;
class CEntity;

class cDMAudio
{
public:
	~cDMAudio()
	{ }

	void Initialise(void);
	void Terminate(void);
	void Service(void);
	
	int32 CreateEntity(eAudioType type, void *UID);
	void DestroyEntity(int32 audioEntity);
	bool8 GetEntityStatus(int32 audioEntity);
	void SetEntityStatus(int32 audioEntity, bool8 status);
	void PlayOneShot(int32 audioEntity, uint16 oneShot, float volume);
	void DestroyAllGameCreatedEntities(void);
	
	void SetOutputMode(bool8 surround);
	void SetEffectsMasterVolume(uint8 volume);
	void SetMusicMasterVolume(uint8 volume);
	void SetEffectsFadeVol(uint8 volume);
	void SetMusicFadeVol(uint8 volume);
	
	uint8 GetNum3DProvidersAvailable(void);
	char *Get3DProviderName(uint8 id);
	
	int8 AutoDetect3DProviders(void);
	
	int8 GetCurrent3DProviderIndex(void);
	int8 SetCurrent3DProvider(uint8 which);
	
	void SetSpeakerConfig(int32 config);
	
	
	void ReleaseDigitalHandle(void);
	void ReacquireDigitalHandle(void);
	
	void SetDynamicAcousticModelingStatus(bool8 status);
	
	bool8 CheckForAnAudioFileOnCD(void);
	
	char GetCDAudioDriveLetter(void);
	bool8 IsAudioInitialised(void);

	
	int32 CreateLoopingScriptObject(cAudioScriptObject *scriptObject);
	void DestroyLoopingScriptObject(int32 audioEntity);
	void CreateOneShotScriptObject(cAudioScriptObject *scriptObject);
	
	void PlaySuspectLastSeen(float x, float y, float z);
	
	void ReportCollision(CEntity *entityA, CEntity *entityB, uint8 surfaceTypeA, uint8 surfaceTypeB, float collisionPower, float velocity);
	
	void PlayFrontEndSound(uint16 frontend, uint32 volume);
	
	void ResetTimers(uint32 time);
	
	void ChangeMusicMode(uint8 mode);
	void PreloadCutSceneMusic(uint32 track);
	void PlayPreloadedCutSceneMusic();
	void StopCutSceneMusic();
	
	void PreloadMissionAudio(uint8 slot, Const char *missionAudio);
	uint8 GetMissionAudioLoadingStatus(uint8 slot);
	void SetMissionAudioLocation(uint8 slot, float x, float y, float z);
	void PlayLoadedMissionAudio(uint8 slot);
	bool8 IsMissionAudioSamplePlaying(uint8 slot);
	bool8 IsMissionAudioSampleFinished(uint8 slot);
	void ClearMissionAudio(uint8 slot);
	const char *GetMissionAudioLoadedLabel(uint8 slot);


	void SetPedTalkingStatus(class CPed *ped, bool8 status);
	void SetPlayersMood(uint8 mood, uint32 time);
	void ShutUpPlayerTalking(bool8 state);
};
extern cDMAudio DMAudio;

#endif // __GTA_DMAUDIO_H__
