//#define JUICY_OAL

#ifdef AUDIO_OAL
#include <time.h>

#include "eax.h"
#include "eax-util.h"

#ifdef _WIN32
#include <io.h>
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>
#include <AL/efx.h>
#include <AL/efx-presets.h>

// for user MP3s
#include <direct.h>
#include <shlobj.h>
#include <shlguid.h>
#else
#define _getcwd getcwd
#endif

#if defined _MSC_VER && !defined CMAKE_NO_AUTOLINK
#pragma comment( lib, "OpenAL32.lib" )
#endif

#include "common.h"
#include "Game.h"
#include "crossplatform.h"

#include "sampman.h"

#include "oal/oal_utils.h"
#include "oal/aldlist.h"
#include "oal/channel.h"

#include <utility>
#ifdef MULTITHREADED_AUDIO
#include <mutex>
#include <queue>
#include <condition_variable>
#endif
#include "oal/stream.h"

#include "AudioManager.h"
#include "MusicManager.h"
#include "Frontend.h"
#include "Timer.h"
#ifdef AUDIO_OAL_USE_OPUS
#include <opusfile.h>
#endif

//TODO: fix eax3 reverb

cSampleManager SampleManager;
bool8 _bSampmanInitialised = FALSE;

uint32 BankStartOffset[MAX_SFX_BANKS];

int           prevprovider=-1;
int           curprovider=-1;
int           usingEAX=0;
int           usingEAX3=0;
//int         speaker_type=0;
ALCdevice    *ALDevice = NULL;
ALCcontext   *ALContext = NULL;
unsigned int _maxSamples;
float        _fPrevEaxRatioDestination; 
bool         _effectsSupported = false;
bool         _usingEFX;
float        _fEffectsLevel;
ALuint       ALEffect = AL_EFFECT_NULL;
ALuint       ALEffectSlot = AL_EFFECTSLOT_NULL;
struct
{ 
	const char *id;
	char name[256];
	int sources;
	bool bSupportsFx;
}providers[MAXPROVIDERS];

int defaultProvider;


char SampleBankDescFilename[] = "audio/sfx.SDT";
char SampleBankDataFilename[] = "audio/sfx.RAW";

FILE *fpSampleDescHandle;
#ifdef OPUS_SFX
OggOpusFile *fpSampleDataHandle;
#else
FILE *fpSampleDataHandle;
#endif
int8  gBankLoaded                  [MAX_SFX_BANKS];
int32 nSampleBankDiscStartOffset   [MAX_SFX_BANKS];
int32 nSampleBankSize              [MAX_SFX_BANKS];
uintptr nSampleBankMemoryStartAddress[MAX_SFX_BANKS];
int32 _nSampleDataEndOffset;

int32 nPedSlotSfx    [MAX_PEDSFX];
int32 nPedSlotSfxAddr[MAX_PEDSFX];
uint8 nCurrentPedSlot;

#ifdef FIX_BUGS
uint32 gPlayerTalkSfx = UINT32_MAX;
void *gPlayerTalkData = 0;
size_t gPlayerTalkDataSize = 0;
#endif

CChannel aChannel[NUM_CHANNELS];
uint8 nChannelVolume[NUM_CHANNELS];

uint32 nStreamLength[TOTAL_STREAMED_SOUNDS];
ALuint ALStreamSources[MAX_STREAMS][2];
ALuint ALStreamBuffers[MAX_STREAMS][NUM_STREAMBUFFERS];

CStream    *aStream[MAX_STREAMS];
uint8      nStreamPan   [MAX_STREAMS];
uint8      nStreamVolume[MAX_STREAMS];
bool8      nStreamLoopedFlag[MAX_STREAMS];
///////////////////////////////////////////////////////////////
//	Env		Size	Diffus	Room	RoomHF	RoomLF	DecTm	DcHF	DcLF	Refl	RefDel	Ref Pan				Revb	RevDel		Rev Pan				EchTm	EchDp	ModTm	ModDp	AirAbs	HFRef		LFRef	RRlOff	FLAGS
EAXLISTENERPROPERTIES StartEAX3 =
	{26,	1.7f,	0.8f,	-1000,	-1000,	-100,	4.42f,	0.14f,	1.00f,	429,	0.014f,	0.00f,0.00f,0.00f,	1023,	0.021f,		0.00f,0.00f,0.00f,	0.250f,	0.000f,	0.250f,	0.000f,	-5.0f,	2727.1f,	250.0f,	0.00f,	0x3f };

EAXLISTENERPROPERTIES FinishEAX3 =
	{26,	100.0f,	1.0f,	0,		-1000,	-2200,	20.0f,	1.39f,	1.00f,	1000,	0.069f,	0.00f,0.00f,0.00f,	400,	0.100f,		0.00f,0.00f,0.00f,	0.250f,	1.000f,	3.982f,	0.000f,	-18.0f,	3530.8f,	417.9f,	6.70f,	0x3f };

EAXLISTENERPROPERTIES EAX3Params;


bool IsFXSupported()
{
	return _effectsSupported; // usingEAX || usingEAX3 || _usingEFX;
}

void EAX_SetAll(const EAXLISTENERPROPERTIES *allparameters)
{
	if ( usingEAX || usingEAX3 )
		EAX3_Set(ALEffect, allparameters);
	else
		EFX_Set(ALEffect, allparameters);
}

static void
add_providers()
{
	SampleManager.SetNum3DProvidersAvailable(0);

	static ALDeviceList DeviceList;
	ALDeviceList *pDeviceList = &DeviceList;

	if ((pDeviceList) && (pDeviceList->GetNumDevices()))
	{
		const int devNumber = Min(pDeviceList->GetNumDevices(), MAXPROVIDERS);
		int n = 0;
		
		//for (int i = 0; i < devNumber; i++) 
		int i = pDeviceList->GetDefaultDevice();
		{
			if ( n < MAXPROVIDERS )
			{ 
				providers[n].id = pDeviceList->GetDeviceName(i);
				strcpy(providers[n].name, "OPENAL SOFT");
				providers[n].sources = pDeviceList->GetMaxNumSources(i);
				SampleManager.Set3DProviderName(n, providers[n].name);
				n++;
			}

			if ( alGetEnumValue("AL_EFFECT_EAXREVERB") != 0
				|| pDeviceList->IsExtensionSupported(i, ADEXT_EAX2)
				|| pDeviceList->IsExtensionSupported(i, ADEXT_EAX3) 
				|| pDeviceList->IsExtensionSupported(i, ADEXT_EAX4)
				|| pDeviceList->IsExtensionSupported(i, ADEXT_EAX5) )
			{ 
				providers[n - 1].bSupportsFx = true;
				if ( n < MAXPROVIDERS )
				{ 
					providers[n].id = pDeviceList->GetDeviceName(i);
					strcpy(providers[n].name, "OPENAL SOFT EAX");
					providers[n].sources = pDeviceList->GetMaxNumSources(i);
					providers[n].bSupportsFx = true;
					SampleManager.Set3DProviderName(n, providers[n].name);
					n++;
				}
				
				if ( n < MAXPROVIDERS )
				{ 
					providers[n].id = pDeviceList->GetDeviceName(i);
					strcpy(providers[n].name, "OPENAL SOFT EAX3");
					providers[n].sources = pDeviceList->GetMaxNumSources(i);
					providers[n].bSupportsFx = true;
					SampleManager.Set3DProviderName(n, providers[n].name);
					n++;
				}
			}
		}
		SampleManager.SetNum3DProvidersAvailable(n);
	
		for(int j=n;j<MAXPROVIDERS;j++)
			SampleManager.Set3DProviderName(j, NULL);

		// devices are gone now
		//defaultProvider = pDeviceList->GetDefaultDevice();
		//if ( defaultProvider > MAXPROVIDERS )
		defaultProvider = 0;
	}
}

static void
release_existing()
{
	if ( IsFXSupported() )
	{
		if ( alIsEffect(ALEffect) )
		{
			alEffecti(ALEffect, AL_EFFECT_TYPE, AL_EFFECT_NULL);
		}
		
		if (alIsAuxiliaryEffectSlot(ALEffectSlot))
		{
			alAuxiliaryEffectSloti(ALEffectSlot, AL_EFFECTSLOT_EFFECT, AL_EFFECT_NULL);
		}
	}

	DEV("release_existing()\n");
}

static bool8
set_new_provider(int index)
{
	if ( curprovider == index )
		return TRUE;
	
	curprovider = index;
	
	release_existing();
	
	if ( curprovider != -1 )
	{
		DEV("set_new_provider()\n");
		
		usingEAX = 0;
		usingEAX3 = 0;
		_usingEFX = false;
		
		if ( !strcmp(&providers[index].name[strlen(providers[index].name) - strlen(" EAX3")], " EAX3") 
				&& alcIsExtensionPresent(ALDevice, (ALCchar*)ALC_EXT_EFX_NAME) )
		{
			
			usingEAX = 1;
			usingEAX3 = 1;
			alAuxiliaryEffectSloti(ALEffectSlot, AL_EFFECTSLOT_EFFECT, ALEffect);
			EAX_SetAll(&FinishEAX3);

			DEV("EAX3\n");
		}
		else if ( alcIsExtensionPresent(ALDevice, (ALCchar*)ALC_EXT_EFX_NAME) )
		{
			
			if ( !strcmp(&providers[index].name[strlen(providers[index].name) - strlen(" EAX")], " EAX"))
			{
				usingEAX = 1;
				DEV("EAX1\n");
			}
			else
			{
				_usingEFX = true;
				DEV("EFX\n");
			}
			alAuxiliaryEffectSloti(ALEffectSlot, AL_EFFECTSLOT_EFFECT, ALEffect);
			EAX_SetAll(&EAX30_ORIGINAL_PRESETS[EAX_ENVIRONMENT_CAVE]);
		}
		
		//SampleManager.SetSpeakerConfig(speaker_type);
		
		if ( IsFXSupported() )
		{
			for ( int32 i = 0; i < MAXCHANNELS; i++ )
				aChannel[i].SetReverbMix(ALEffectSlot, 0.0f);
		}
		
		return TRUE;
	}
	
	return FALSE;
}

static bool8
IsThisTrackAt16KHz(uint32 track)
{
	return track == STREAMED_SOUND_RADIO_KCHAT || track == STREAMED_SOUND_RADIO_VCPR || track == STREAMED_SOUND_RADIO_POLICE;
}

cSampleManager::cSampleManager(void)
{
	;
}

cSampleManager::~cSampleManager(void)
{
	
}

void cSampleManager::SetSpeakerConfig(int32 nConfig)
{

}

uint32 cSampleManager::GetMaximumSupportedChannels(void)
{
	if ( _maxSamples > MAXCHANNELS )
		return MAXCHANNELS;
	
	return _maxSamples;
}

uint32 cSampleManager::GetNum3DProvidersAvailable()
{
	return m_nNumberOfProviders;
}

void cSampleManager::SetNum3DProvidersAvailable(uint32 num)
{
	m_nNumberOfProviders = num;
}

char *cSampleManager::Get3DProviderName(uint8 id)
{
	return m_aAudioProviders[id];
}

void cSampleManager::Set3DProviderName(uint8 id, char *name)
{
	m_aAudioProviders[id] = name;
}

int8 cSampleManager::GetCurrent3DProviderIndex(void)
{
	return curprovider;
}

int8 cSampleManager::SetCurrent3DProvider(uint8 nProvider)
{
	int savedprovider = curprovider;

	nProvider = Clamp(nProvider, 0, m_nNumberOfProviders - 1);

	if ( set_new_provider(nProvider) )
		return curprovider;
	else if ( savedprovider != -1 && savedprovider < m_nNumberOfProviders && set_new_provider(savedprovider) )
		return curprovider;
	else
		return curprovider;
}

int8
cSampleManager::AutoDetect3DProviders()
{
	if (!AudioManager.IsAudioInitialised())
		return -1;

	if (defaultProvider >= 0 && defaultProvider < m_nNumberOfProviders) {
		if (set_new_provider(defaultProvider))
			return defaultProvider;
	}

	for (uint32 i = 0; i < GetNum3DProvidersAvailable(); i++)
	{
		char* providername = Get3DProviderName(i);

		if (!strcasecmp(providername, "OPENAL SOFT")) {
			SetCurrent3DProvider(i);
			if (GetCurrent3DProviderIndex() == i)
				return i;
		}
	}

	return -1;
}

bool8
cSampleManager::Initialise(void)
{
	if ( _bSampmanInitialised )
		return TRUE;

	EFXInit();

	for(int i = 0; i < MAX_STREAMS; i++)
		aStream[i] = new CStream(ALStreamSources[i], ALStreamBuffers[i]);

	CStream::Initialise();

	{
		for ( int32 i = 0; i < TOTAL_AUDIO_SAMPLES; i++ )
		{
			m_aSamples[i].nOffset    = 0;
			m_aSamples[i].nSize      = 0;
			m_aSamples[i].nFrequency = 22050;
			m_aSamples[i].nLoopStart = 0;
			m_aSamples[i].nLoopEnd   = -1;
		}
		
		m_nEffectsVolume     = MAX_VOLUME;
		m_nMusicVolume       = MAX_VOLUME;
		m_nEffectsFadeVolume = MAX_VOLUME;
		m_nMusicFadeVolume   = MAX_VOLUME;
	
		m_nMonoMode = 0;
	}
	
	{
		curprovider = -1;
		prevprovider = -1;
			
		_usingEFX = false;
		usingEAX =0;
		usingEAX3=0;
			
		_fEffectsLevel = 0.0f;
			
		_maxSamples = 0;
		
		ALDevice = NULL;
		ALContext = NULL;
	}
	
	{
		fpSampleDescHandle = NULL;
		fpSampleDataHandle = NULL;
		
		for ( int32 i = 0; i < MAX_SFX_BANKS; i++ )
		{
			gBankLoaded[i]                   = LOADING_STATUS_NOT_LOADED;
			nSampleBankDiscStartOffset[i]    = 0;
			nSampleBankSize[i]               = 0;
			nSampleBankMemoryStartAddress[i] = 0;
		}
	}
	
	{
		for ( int32 i = 0; i < MAX_PEDSFX; i++ )
		{
			nPedSlotSfx[i]     = NO_SAMPLE;
			nPedSlotSfxAddr[i] = 0;
		}
		
		nCurrentPedSlot = 0;
	}
	
	{
		for ( int32 i = 0; i < NUM_CHANNELS; i++ )
			nChannelVolume[i] = 0;
	}

	add_providers();

	{
		int index = 0;
		_maxSamples = Min(MAXCHANNELS, providers[index].sources);
		
		ALCint attr[] = {ALC_FREQUENCY,MAX_FREQ,
						ALC_MONO_SOURCES, MAX_DIGITAL_MIXER_CHANNELS - MAX2DCHANNELS,
						ALC_STEREO_SOURCES, MAX2DCHANNELS,
						0,
						};
		
		ALDevice  = alcOpenDevice(providers[index].id);
		ASSERT(ALDevice != NULL);
		
		ALContext = alcCreateContext(ALDevice, attr);
		ASSERT(ALContext != NULL);
		
		alcMakeContextCurrent(ALContext);
	
		const char* ext=(const char*)alGetString(AL_EXTENSIONS);
		if ( strstr(ext,"AL_SOFT_loop_points")==NULL )
		{
			ASSERT(strstr(ext, "AL_SOFT_loop_points") != NULL);
			Terminate();
			return FALSE;
		}
		
		alListenerf (AL_GAIN,     1.0f);
		alListener3f(AL_POSITION, 0.0f, 0.0f, 0.0f);
		alListener3f(AL_VELOCITY, 0.0f, 0.0f, 0.0f);
		ALfloat orientation[6] = { 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f };
		alListenerfv(AL_ORIENTATION, orientation);
		
		alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
		
		if ( alcIsExtensionPresent(ALDevice, (ALCchar*)ALC_EXT_EFX_NAME) )
		{ 
			_effectsSupported = providers[index].bSupportsFx;
			alGenAuxiliaryEffectSlots(1, &ALEffectSlot);
			alGenEffects(1, &ALEffect);
		}

		alGenSources(MAX_STREAMS*2, ALStreamSources[0]);
		for ( int32 i = 0; i < MAX_STREAMS; i++ )
		{
			alGenBuffers(NUM_STREAMBUFFERS, ALStreamBuffers[i]);
			alSourcei(ALStreamSources[i][0], AL_SOURCE_RELATIVE, AL_TRUE);
			alSource3f(ALStreamSources[i][0], AL_POSITION, 0.0f, 0.0f, 0.0f);
			alSourcef(ALStreamSources[i][0], AL_GAIN, 1.0f);
			alSourcei(ALStreamSources[i][1], AL_SOURCE_RELATIVE, AL_TRUE);
			alSource3f(ALStreamSources[i][1], AL_POSITION, 0.0f, 0.0f, 0.0f);
			alSourcef(ALStreamSources[i][1], AL_GAIN, 1.0f);
		} 
		
		CChannel::InitChannels();

		for ( int32 i = 0; i < MAXCHANNELS; i++ )
			aChannel[i].Init(i);
		for ( int32 i = 0; i < MAX2DCHANNELS; i++ )
			aChannel[MAXCHANNELS+i].Init(MAXCHANNELS+i, true);
		
		if ( IsFXSupported() )
		{
			/**/
			alAuxiliaryEffectSloti(ALEffectSlot, AL_EFFECTSLOT_EFFECT, ALEffect);
			/**/
			
			for ( int32 i = 0; i < MAXCHANNELS; i++ )
				aChannel[i].SetReverbMix(ALEffectSlot, 0.0f);
		}
	}

	{	
		for ( int32 i = 0; i < TOTAL_STREAMED_SOUNDS; i++ )
			nStreamLength[i] = 1;
	}

#ifdef AUDIO_CACHE
	FILE *cacheFile = fcaseopen("audio\\sound.cache", "rb");
	if (cacheFile) {
		debug("Loadind audio cache (If game crashes around here, then your cache is corrupted, remove audio/sound.cache)\n");
		fread(nStreamLength, sizeof(uint32), TOTAL_STREAMED_SOUNDS, cacheFile);
		fclose(cacheFile);
		for ( int32 i = 0; i < TOTAL_STREAMED_SOUNDS; i++ ) {
			if ( nStreamLength[i] == 0 )
				nStreamLength[i] = 1;
		}
	} else
	{
		debug("Cannot load audio cache\n");
#endif

		int32 openedStreamCount = 0;
		for ( int32 i = 0; i < TOTAL_STREAMED_SOUNDS; i++ )
		{	
			if ( aStream[0] && (
#ifdef PS2_AUDIO_PATHS
				aStream[0]->Open(PS2StreamedNameTable[i], IsThisTrackAt16KHz(i) ? 16000 : 32000) || 
#endif
				aStream[0]->Open(StreamedNameTable[i], IsThisTrackAt16KHz(i) ? 16000 : 32000)) )
			{
				uint32 tatalms = aStream[0]->GetLengthMS();
				aStream[0]->Close();
				
				nStreamLength[i] = tatalms > 0 ? tatalms : 1;
				openedStreamCount++;
			} else {
				nStreamLength[i] = 1;
				debug("Audio stream missing (skipped): %s\n", StreamedNameTable[i]);
			}
		}
#ifdef AUDIO_CACHE
		if (openedStreamCount > 0) {
			cacheFile = fcaseopen("audio\\sound.cache", "wb");
			if(cacheFile) {
				debug("Saving audio cache\n");
				fwrite(nStreamLength, sizeof(uint32), TOTAL_STREAMED_SOUNDS, cacheFile);
				fclose(cacheFile);
			} else {
				debug("Cannot save audio cache\n");
			}
		}
	}
#endif

	{
		if ( !InitialiseSampleBanks() )
		{
			Terminate();
			return FALSE;
		}
		
		nSampleBankMemoryStartAddress[SFX_BANK_0] = (uintptr)malloc(nSampleBankSize[SFX_BANK_0]);
		ASSERT(nSampleBankMemoryStartAddress[SFX_BANK_0] != 0);
		
		if ( nSampleBankMemoryStartAddress[SFX_BANK_0] == 0 )
		{
			Terminate();
			return FALSE;
		}
		
		nSampleBankMemoryStartAddress[SFX_BANK_PED_COMMENTS] = (uintptr)malloc(PED_BLOCKSIZE*MAX_PEDSFX);
		ASSERT(nSampleBankMemoryStartAddress[SFX_BANK_PED_COMMENTS] != 0);

#ifdef FIX_BUGS
		// Find biggest player comment
		uint32 nMaxPedSize = 0;
		for (uint32 i = PLAYER_COMMENTS_START; i <= PLAYER_COMMENTS_END; i++)
			nMaxPedSize = Max(nMaxPedSize, m_aSamples[i].nSize);

		gPlayerTalkData = malloc(nMaxPedSize);
		ASSERT(gPlayerTalkData != 0);

		gPlayerTalkDataSize = nMaxPedSize;
#endif

		LoadSampleBank(SFX_BANK_0);
	}
	
	{
		for ( int32 i = 0; i < MAX_STREAMS; i++ )
		{
			aStream[i]->Close();

			nStreamVolume[i] = 100;
			nStreamPan[i]    = 63;
		}
	}

	{
		_bSampmanInitialised = TRUE;
		
		if ( defaultProvider >= 0 && defaultProvider < m_nNumberOfProviders )
		{
			set_new_provider(defaultProvider);
		}
		else
		{
			Terminate();
			return FALSE;
		}
	}

	return TRUE;
}

void
cSampleManager::Terminate(void)
{
	for (int32 i = 0; i < MAX_STREAMS; i++) {
		if (aStream[i])
			aStream[i]->Close();
	}

	for ( int32 i = 0; i < NUM_CHANNELS; i++ )
		aChannel[i].Term();

	if ( IsFXSupported() )
	{
		if ( alIsEffect(ALEffect) )
		{
			alEffecti(ALEffect, AL_EFFECT_TYPE, AL_EFFECT_NULL);
			alDeleteEffects(1, &ALEffect);
			ALEffect = AL_EFFECT_NULL;
		}
		
		if (alIsAuxiliaryEffectSlot(ALEffectSlot))
		{
			alAuxiliaryEffectSloti(ALEffectSlot, AL_EFFECTSLOT_EFFECT, AL_EFFECT_NULL);
			
			alDeleteAuxiliaryEffectSlots(1, &ALEffectSlot);
			ALEffectSlot = AL_EFFECTSLOT_NULL;
		}
	}

	for ( int32 i = 0; i < MAX_STREAMS; i++ )
	{
		alDeleteBuffers(NUM_STREAMBUFFERS, ALStreamBuffers[i]);
	}
	alDeleteSources(MAX_STREAMS*2, ALStreamSources[0]);
	
	CChannel::DestroyChannels();
	
	if ( ALContext )
	{
		alcMakeContextCurrent(NULL);
		alcSuspendContext(ALContext);
		alcDestroyContext(ALContext);
	}
	if ( ALDevice )
		alcCloseDevice(ALDevice);
	
	ALDevice = NULL;
	ALContext = NULL;
	
	_fPrevEaxRatioDestination = 0.0f;
	_usingEFX                 = false;
	_fEffectsLevel            = 0.0f;
	

	CStream::Terminate();

	for(int32 i = 0; i < MAX_STREAMS; i++) {
		delete aStream[i];
		aStream[i] = NULL;
	}

	if ( nSampleBankMemoryStartAddress[SFX_BANK_0] != 0 )
	{
		free((void *)nSampleBankMemoryStartAddress[SFX_BANK_0]);
		nSampleBankMemoryStartAddress[SFX_BANK_0] = 0;
	}

	if ( nSampleBankMemoryStartAddress[SFX_BANK_PED_COMMENTS] != 0 )
	{
		free((void *)nSampleBankMemoryStartAddress[SFX_BANK_PED_COMMENTS]);
		nSampleBankMemoryStartAddress[SFX_BANK_PED_COMMENTS] = 0;
	}

#ifdef FIX_BUGS
	if ( gPlayerTalkData != 0 )
	{
		free(gPlayerTalkData);
		gPlayerTalkData = 0;
		gPlayerTalkDataSize = 0;
		gPlayerTalkSfx = UINT32_MAX;
	}
#endif
	
	_bSampmanInitialised = FALSE;
}

bool8 cSampleManager::CheckForAnAudioFileOnCD(void)
{
	return TRUE;
}

char cSampleManager::GetCDAudioDriveLetter(void)
{
	return '\0';
}

void
cSampleManager::UpdateEffectsVolume(void)
{
	if ( _bSampmanInitialised )
	{
		for ( int32 i = 0; i < NUM_CHANNELS; i++ )
		{
			if ( GetChannelUsedFlag(i) )
			{
				if ( nChannelVolume[i] != 0 )
					aChannel[i].SetVolume(m_nEffectsFadeVolume*nChannelVolume[i]*m_nEffectsVolume >> 14);
			}
		}
	}
}

void
cSampleManager::SetEffectsMasterVolume(uint8 nVolume)
{
	m_nEffectsVolume = nVolume;
	UpdateEffectsVolume();
}

void
cSampleManager::SetMusicMasterVolume(uint8 nVolume)
{
	m_nMusicVolume = nVolume;
}

void
cSampleManager::SetEffectsFadeVolume(uint8 nVolume)
{
	m_nEffectsFadeVolume = nVolume;
	UpdateEffectsVolume();
}

void
cSampleManager::SetMusicFadeVolume(uint8 nVolume)
{
	m_nMusicFadeVolume = nVolume;
}

void
cSampleManager::SetMonoMode(bool8 nMode)
{
	m_nMonoMode = nMode;
}

bool8
cSampleManager::LoadSampleBank(uint8 nBank)
{
	ASSERT( nBank < MAX_SFX_BANKS);
	
	if ( CTimer::GetIsCodePaused() )
		return FALSE;
	
	if ( MusicManager.IsInitialised()
		&& CGame::InCutscene()
		&& nBank != SFX_BANK_0 )
	{
		return FALSE;
	}
	
#ifdef OPUS_SFX
	int samplesRead = 0;
	int samplesSize = nSampleBankSize[nBank] / 2;
	op_pcm_seek(fpSampleDataHandle, 0);
	while (samplesSize > 0) {
		int size = op_read(fpSampleDataHandle, (opus_int16 *)(nSampleBankMemoryStartAddress[nBank] + samplesRead), samplesSize, NULL);
		if (size <= 0) {
			// huh?
			//assert(0);
			break;
		}
		samplesRead += size*2;
		samplesSize -= size;
	}
#else
	if ( fseek(fpSampleDataHandle, nSampleBankDiscStartOffset[nBank], SEEK_SET) != 0 )
		return FALSE;
	
	if ( fread((void *)nSampleBankMemoryStartAddress[nBank], 1, nSampleBankSize[nBank], fpSampleDataHandle) != nSampleBankSize[nBank] )
		return FALSE;
#endif
	gBankLoaded[nBank] = LOADING_STATUS_LOADED;
	
	return TRUE;
}

void
cSampleManager::UnloadSampleBank(uint8 nBank)
{
	ASSERT( nBank < MAX_SFX_BANKS);
	
	gBankLoaded[nBank] = LOADING_STATUS_NOT_LOADED;
}

int8
cSampleManager::IsSampleBankLoaded(uint8 nBank)
{
	ASSERT( nBank < MAX_SFX_BANKS);
	
	return gBankLoaded[nBank];
}

#ifdef FIX_BUGS
uint8
cSampleManager::IsMissionAudioLoaded(uint8 nSlot, uint32 nSample)
{
	ASSERT(nSlot == MISSION_AUDIO_PLAYER_COMMENT); // only MISSION_AUDIO_PLAYER_COMMENT is supported on PC
	
	return nSample == gPlayerTalkSfx ? LOADING_STATUS_LOADED : LOADING_STATUS_NOT_LOADED;
}

bool8
cSampleManager::LoadMissionAudio(uint8 nSlot, uint32 nSample)
{
	ASSERT(nSlot == MISSION_AUDIO_PLAYER_COMMENT); // only MISSION_AUDIO_PLAYER_COMMENT is supported on PC
	ASSERT(nSample < TOTAL_AUDIO_SAMPLES);
	
	if (fseek(fpSampleDataHandle, m_aSamples[nSample].nOffset, SEEK_SET) != 0)
		return FALSE;

	if (fread(gPlayerTalkData, 1, m_aSamples[nSample].nSize, fpSampleDataHandle) != m_aSamples[nSample].nSize)
		return FALSE;

	gPlayerTalkSfx = nSample;

	return TRUE;
}
#endif

uint8
cSampleManager::IsPedCommentLoaded(uint32 nComment)
{
	ASSERT( nComment < TOTAL_AUDIO_SAMPLES );
	
	for ( int32 i = 0; i < _TODOCONST(3); i++ )
	{
#ifdef FIX_BUGS
		int8 slot = (int8)nCurrentPedSlot - i - 1;
		if (slot < 0)
			slot += ARRAY_SIZE(nPedSlotSfx);
#else
		uint8 slot = nCurrentPedSlot - i - 1;
#endif
		if ( nComment == nPedSlotSfx[slot] )
			return LOADING_STATUS_LOADED;
	}
	
	return LOADING_STATUS_NOT_LOADED;
}


int32
cSampleManager::_GetPedCommentSlot(uint32 nComment)
{
	for (int32 i = 0; i < _TODOCONST(3); i++)
	{
#ifdef FIX_BUGS
		int8 slot = (int8)nCurrentPedSlot - i - 1;
		if (slot < 0)
			slot += ARRAY_SIZE(nPedSlotSfx);
#else
		uint8 slot = nCurrentPedSlot - i - 1;
#endif
		if (nComment == nPedSlotSfx[slot])
			return slot;
	}

	return -1;
}

bool8
cSampleManager::LoadPedComment(uint32 nComment)
{
	ASSERT( nComment < TOTAL_AUDIO_SAMPLES );
	
	if ( CTimer::GetIsCodePaused() )
		return FALSE;
	
	// no talking peds during cutsenes or the game end
	if (MusicManager.IsInitialised() && CGame::InCutscene())
		return FALSE;

#ifdef OPUS_SFX
	int samplesRead = 0;
	int samplesSize = m_aSamples[nComment].nSize / 2;
	op_pcm_seek(fpSampleDataHandle, m_aSamples[nComment].nOffset / 2);
	while (samplesSize > 0) {
		int size = op_read(fpSampleDataHandle, (opus_int16 *)(nSampleBankMemoryStartAddress[SAMPLEBANK_PED] + PED_BLOCKSIZE * nCurrentPedSlot + samplesRead),
		                   samplesSize, NULL);
		if (size <= 0) {
			return FALSE;
		}
		samplesRead += size * 2;
		samplesSize -= size;
	}
#else
	if ( fseek(fpSampleDataHandle, m_aSamples[nComment].nOffset, SEEK_SET) != 0 )
		return FALSE;
	
	if ( fread((void *)(nSampleBankMemoryStartAddress[SFX_BANK_PED_COMMENTS] + PED_BLOCKSIZE*nCurrentPedSlot), 1, m_aSamples[nComment].nSize, fpSampleDataHandle) != m_aSamples[nComment].nSize )
		return FALSE;

#endif
	nPedSlotSfx[nCurrentPedSlot] = nComment;
		
	if ( ++nCurrentPedSlot >= MAX_PEDSFX )
		nCurrentPedSlot = 0;
	
	return TRUE;
}

int32
cSampleManager::GetBankContainingSound(uint32 offset)
{
	if ( offset >= BankStartOffset[SFX_BANK_PED_COMMENTS] )
		return SFX_BANK_PED_COMMENTS;
	
	if ( offset >= BankStartOffset[SFX_BANK_0] )
		return SFX_BANK_0;
	
	return INVALID_SFX_BANK;
}

uint32
cSampleManager::GetSampleBaseFrequency(uint32 nSample)
{
	if (nSample >= TOTAL_AUDIO_SAMPLES)
		return 22050;
	return m_aSamples[nSample].nFrequency;
}

uint32
cSampleManager::GetSampleLoopStartOffset(uint32 nSample)
{
	if (nSample >= TOTAL_AUDIO_SAMPLES)
		return 0;
	return m_aSamples[nSample].nLoopStart;
}

int32
cSampleManager::GetSampleLoopEndOffset(uint32 nSample)
{
	if (nSample >= TOTAL_AUDIO_SAMPLES)
		return -1;
	return m_aSamples[nSample].nLoopEnd;
}

uint32
cSampleManager::GetSampleLength(uint32 nSample)
{
	if (nSample >= TOTAL_AUDIO_SAMPLES)
		return 0;
	return m_aSamples[nSample].nSize / sizeof(uint16);
}

bool8 cSampleManager::UpdateReverb(void)
{
	if ( !usingEAX && !_usingEFX )
		return FALSE;

	if ( AudioManager.m_FrameCounter & 15 )
		return FALSE;

	float fRatio = 0.0f;

#ifdef AUDIO_REFLECTIONS
#define MIN_DIST 0.5f
#define CALCULATE_RATIO(value, maxDist, maxRatio) (value > MIN_DIST && value < maxDist ? value / maxDist * maxRatio : 0)

	fRatio += CALCULATE_RATIO(AudioManager.m_afReflectionsDistances[REFLECTION_CEIL_NORTH], 10.0f, 1/2.f);
	fRatio += CALCULATE_RATIO(AudioManager.m_afReflectionsDistances[REFLECTION_CEIL_SOUTH], 10.0f, 1/2.f);
	fRatio += CALCULATE_RATIO(AudioManager.m_afReflectionsDistances[REFLECTION_CEIL_WEST], 10.0f, 1/2.f);
	fRatio += CALCULATE_RATIO(AudioManager.m_afReflectionsDistances[REFLECTION_CEIL_EAST], 10.0f, 1/2.f);

	fRatio += CALCULATE_RATIO((AudioManager.m_afReflectionsDistances[REFLECTION_NORTH] + AudioManager.m_afReflectionsDistances[REFLECTION_SOUTH]) / 2.f, 4.0f, 1/3.f);
	fRatio += CALCULATE_RATIO((AudioManager.m_afReflectionsDistances[REFLECTION_WEST] + AudioManager.m_afReflectionsDistances[REFLECTION_EAST]) / 2.f, 4.0f, 1/3.f);

#undef CALCULATE_RATIO
#undef MIN_DIST
#endif
	
	fRatio = Clamp(fRatio, 0.0f, 0.6f);
	
	if ( fRatio == _fPrevEaxRatioDestination )
		return FALSE;
	
#ifdef JUICY_OAL
	if ( usingEAX3 || _usingEFX )
#else
	if ( usingEAX3 )
#endif
	{
		fRatio = Min(fRatio * 1.67f, 1.0f);
		if ( EAX3ListenerInterpolate(&StartEAX3, &FinishEAX3, fRatio, &EAX3Params, false) )
		{
			EAX_SetAll(&EAX3Params);
			
			/*
			if ( IsFXSupported() )
			{
				alAuxiliaryEffectSloti(ALEffectSlot, AL_EFFECTSLOT_EFFECT, ALEffect);
			
				for ( int32 i = 0; i < MAXCHANNELS; i++ )
					aChannel[i].UpdateReverb(ALEffectSlot);
			}
			*/
			
			_fEffectsLevel = fRatio * 0.75f;
		}
	}
	else
	{
		if ( _usingEFX )
			_fEffectsLevel = fRatio * 0.8f;
		else
			_fEffectsLevel = fRatio * 0.22f;
	}
	_fEffectsLevel = Min(_fEffectsLevel, 1.0f);

	_fPrevEaxRatioDestination = fRatio;
	
	return TRUE;
}

void
cSampleManager::SetChannelReverbFlag(uint32 nChannel, bool8 nReverbFlag)
{
	ASSERT( nChannel < NUM_CHANNELS );
	
	if ( usingEAX || _usingEFX )
	{
		if ( IsFXSupported() )
		{
			alAuxiliaryEffectSloti(ALEffectSlot, AL_EFFECTSLOT_EFFECT, ALEffect);
			
			if ( nReverbFlag != FALSE )
				aChannel[nChannel].SetReverbMix(ALEffectSlot, _fEffectsLevel);
			else
				aChannel[nChannel].SetReverbMix(ALEffectSlot, 0.0f);
		}
	}
}

bool8
cSampleManager::InitialiseChannel(uint32 nChannel, uint32 nSfx, uint8 nBank)
{
	ASSERT( nChannel < NUM_CHANNELS );
	
	uintptr addr;
	
	if ( nSfx < SAMPLEBANK_MAX )
	{
		if ( !IsSampleBankLoaded(nBank) )
			return FALSE;
		
		addr = nSampleBankMemoryStartAddress[nBank] + m_aSamples[nSfx].nOffset - m_aSamples[BankStartOffset[nBank]].nOffset;
	}
#ifdef FIX_BUGS
	else if ( nSfx >= PLAYER_COMMENTS_START && nSfx <= PLAYER_COMMENTS_END )
	{
		if ( !IsMissionAudioLoaded(MISSION_AUDIO_PLAYER_COMMENT, nSfx) )
			return FALSE;

		addr = (uintptr)gPlayerTalkData;
	}
#endif
	else
	{
		int32 i;
		for ( i = 0; i < _TODOCONST(3); i++ )
		{
			int32 slot = nCurrentPedSlot - i - 1;
#ifdef FIX_BUGS
			if (slot < 0)
				slot += ARRAY_SIZE(nPedSlotSfx);
#endif
			if ( nSfx == nPedSlotSfx[slot] )
			{
				addr = (nSampleBankMemoryStartAddress[SFX_BANK_PED_COMMENTS] + PED_BLOCKSIZE * slot);
				break;
			}
		}

		if (i == _TODOCONST(3))
			return FALSE;
	}
	
	if ( GetChannelUsedFlag(nChannel) )
	{
		TRACE("Stopping channel %d - really!!!", nChannel);
		StopChannel(nChannel);
	}
	
	aChannel[nChannel].Reset();
	if ( aChannel[nChannel].HasSource() )
	{	
		aChannel[nChannel].SetSampleData   ((void*)addr, m_aSamples[nSfx].nSize, m_aSamples[nSfx].nFrequency);
		aChannel[nChannel].SetLoopPoints   (0, -1);
		aChannel[nChannel].SetPitch        (1.0f);
		return TRUE;
	}
	
	return FALSE;
}

void
cSampleManager::SetChannelEmittingVolume(uint32 nChannel, uint32 nVolume)
{
	ASSERT( nChannel < MAXCHANNELS );
	
	uint32 vol = nVolume;
	if ( vol > MAX_VOLUME ) vol = MAX_VOLUME;
	
	nChannelVolume[nChannel] = vol;
	
	if (CGame::InCutscene() ) {
		if (MusicManager.GetCurrentTrack() == STREAMED_SOUND_CUTSCENE_FINALE)
			nChannelVolume[nChannel] = 0;
		else
			nChannelVolume[nChannel] >>= 2;
	}

	// no idea, does this one looks like a bug or it's SetChannelVolume ?
	aChannel[nChannel].SetVolume(m_nEffectsFadeVolume*nChannelVolume[nChannel]*m_nEffectsVolume >> 14);
}

void
cSampleManager::SetChannel3DPosition(uint32 nChannel, float fX, float fY, float fZ)
{
	ASSERT( nChannel < MAXCHANNELS );
	
	aChannel[nChannel].SetPosition(-fX, fY, fZ);
}

void
cSampleManager::SetChannel3DDistances(uint32 nChannel, float fMax, float fMin)
{
	ASSERT( nChannel < MAXCHANNELS );
	aChannel[nChannel].SetDistances(fMax, fMin);
}

void
cSampleManager::SetChannelVolume(uint32 nChannel, uint32 nVolume)
{
	ASSERT( nChannel >= MAXCHANNELS );
	ASSERT( nChannel < NUM_CHANNELS );
	
	if ( nChannel == CHANNEL_POLICE_RADIO )
	{
		uint32 vol = nVolume;
		if ( vol > MAX_VOLUME ) vol = MAX_VOLUME;
		
		nChannelVolume[nChannel] = vol;
		
		// increase the volume for JB.MP3 and S4_BDBD.MP3
		if (CGame::InCutscene() ) {
			if (MusicManager.GetCurrentTrack() == STREAMED_SOUND_CUTSCENE_FINALE)
				nChannelVolume[nChannel] = 0;
			else
				nChannelVolume[nChannel] >>= 2;
		}

		aChannel[nChannel].SetVolume(m_nEffectsFadeVolume*vol*m_nEffectsVolume >> 14);
	}
}

void
cSampleManager::SetChannelPan(uint32 nChannel, uint32 nPan)
{
	ASSERT( nChannel >= MAXCHANNELS );
	ASSERT( nChannel < NUM_CHANNELS );
	
	if ( nChannel == CHANNEL_POLICE_RADIO )
	{
		aChannel[nChannel].SetPan(nPan);
	}
}

void
cSampleManager::SetChannelFrequency(uint32 nChannel, uint32 nFreq)
{
	ASSERT( nChannel < NUM_CHANNELS );
	
	aChannel[nChannel].SetCurrentFreq(nFreq);
}

void
cSampleManager::SetChannelLoopPoints(uint32 nChannel, uint32 nLoopStart, int32 nLoopEnd)
{
	ASSERT( nChannel < NUM_CHANNELS );
	
	aChannel[nChannel].SetLoopPoints(nLoopStart / (DIGITALBITS / 8), nLoopEnd / (DIGITALBITS / 8));
}

void
cSampleManager::SetChannelLoopCount(uint32 nChannel, uint32 nLoopCount)
{
	ASSERT( nChannel < NUM_CHANNELS );
	
	aChannel[nChannel].SetLoopCount(nLoopCount);
}

bool8
cSampleManager::GetChannelUsedFlag(uint32 nChannel)
{
	ASSERT( nChannel < NUM_CHANNELS );
	
	return aChannel[nChannel].IsUsed();
}

void
cSampleManager::StartChannel(uint32 nChannel)
{
	ASSERT( nChannel < NUM_CHANNELS );
	
	aChannel[nChannel].Start();
}

void
cSampleManager::StopChannel(uint32 nChannel)
{
	ASSERT( nChannel < NUM_CHANNELS );
	
	aChannel[nChannel].Stop();
}

void
cSampleManager::PreloadStreamedFile(tTrack nFile, uint8 nStream)
{	
	ASSERT( nStream < MAX_STREAMS );

	if ( nFile < TOTAL_STREAMED_SOUNDS )
	{
		CStream *stream = aStream[nStream];
		if (!stream)
			return;

		stream->Close();
#ifdef PS2_AUDIO_PATHS
		if(!stream->Open(PS2StreamedNameTable[nFile], IsThisTrackAt16KHz(nFile) ? 16000 : 32000))
#endif
			stream->Open(StreamedNameTable[nFile], IsThisTrackAt16KHz(nFile) ? 16000 : 32000);
		if ( !stream->Setup() )
		{
			stream->Close();
		}
	}
}

void
cSampleManager::PauseStream(bool8 nPauseFlag, uint8 nStream)
{
	ASSERT( nStream < MAX_STREAMS );
	
	CStream *stream = aStream[nStream];
	if (!stream)
		return;
	
	if ( stream->IsOpened() )
	{
		stream->SetPause(nPauseFlag != FALSE);
	}
}

void
cSampleManager::StartPreloadedStreamedFile(uint8 nStream)
{
	ASSERT( nStream < MAX_STREAMS );
	
	CStream *stream = aStream[nStream];
	if (!stream)
		return;
	
	if ( stream->IsOpened() )
	{
		stream->SetPosMS(0);
		stream->Start();
	}
}

bool8
cSampleManager::StartStreamedFile(tTrack nFile, uint32 nPos, uint8 nStream)
{
	uint32 position = nPos;
	
	if ( nFile >= TOTAL_STREAMED_SOUNDS || !aStream[nStream] )
		return FALSE;
#ifndef GTA_PS2
	if (nFile == STREAMED_SOUND_LEGACY_USERTRACK_SLOT)
		return FALSE;
#endif

	aStream[nStream]->Close();

	CStream *stream = aStream[nStream];

#ifdef PS2_AUDIO_PATHS
	if(!stream->Open(PS2StreamedNameTable[nFile], IsThisTrackAt16KHz(nFile) ? 16000 : 32000))
#endif
		stream->Open(StreamedNameTable[nFile], IsThisTrackAt16KHz(nFile) ? 16000 : 32000);
	
	if ( stream->Setup() ) {
		stream->SetLoopCount(nStreamLoopedFlag[nStream] ? 0 : 1);
		nStreamLoopedFlag[nStream] = TRUE;
		if (position != 0)
			stream->SetPosMS(position);	

		stream->Start();
		
		return TRUE;
	} else {
		stream->Close();
	}
	return FALSE;
}

void
cSampleManager::StopStreamedFile(uint8 nStream)
{
	ASSERT( nStream < MAX_STREAMS );

	CStream *stream = aStream[nStream];
	if (!stream)
		return;
	
	stream->Close();

}

int32
cSampleManager::GetStreamedFilePosition(uint8 nStream)
{
	ASSERT( nStream < MAX_STREAMS );
	
	CStream *stream = aStream[nStream];
	if (!stream)
		return 0;
	
	if ( stream->IsOpened() )
	{
		return stream->GetPosMS();
	}
	
	return 0;
}

void
cSampleManager::SetStreamedVolumeAndPan(uint8 nVolume, uint8 nPan, bool8 nEffectFlag, uint8 nStream)
{
	ASSERT( nStream < MAX_STREAMS );
	
	if ( nVolume > MAX_VOLUME )
		nVolume = MAX_VOLUME;
	
	if ( nPan > MAX_VOLUME )
		nPan = MAX_VOLUME;

	nStreamVolume[nStream] = nVolume;
	nStreamPan   [nStream] = nPan;
	
	CStream *stream = aStream[nStream];
	if (!stream)
		return;
	
	if ( stream->IsOpened() )
	{
		if ( nEffectFlag ) {
			if ( nStream == 1 || nStream == 2 )
				stream->SetVolume(128*nVolume*m_nEffectsVolume >> 14);
			else
				stream->SetVolume(m_nEffectsFadeVolume*nVolume*m_nEffectsVolume >> 14);
		}
		else
			stream->SetVolume((m_nMusicFadeVolume*nVolume*(uint32)m_nMusicVolume) >> 14);
		
		stream->SetPan(nPan);
	}
}

int32
cSampleManager::GetStreamedFileLength(uint8 nStream)
{
	ASSERT( nStream < TOTAL_STREAMED_SOUNDS );

	return nStreamLength[nStream] > 0 ? nStreamLength[nStream] : 1;
}

bool8
cSampleManager::IsStreamPlaying(uint8 nStream)
{
	ASSERT( nStream < MAX_STREAMS );
	
	CStream *stream = aStream[nStream];
	if (!stream)
		return FALSE;
	
	if ( stream->IsOpened() )
	{
		if ( stream->IsPlaying() )
			return TRUE;
	}
	
	return FALSE;
}

void
cSampleManager::Service(void)
{
	for ( int32 i = 0; i < MAX_STREAMS; i++ )
	{
		CStream *stream = aStream[i];
		
		if ( stream && stream->IsOpened() )
			stream->Update();
	}
}

bool8
cSampleManager::InitialiseSampleBanks(void)
{
	int32 nBank = SFX_BANK_0;
	
	fpSampleDescHandle = fcaseopen(SampleBankDescFilename, "rb");
	if ( fpSampleDescHandle == NULL )
		return FALSE;
#ifndef OPUS_SFX
	fpSampleDataHandle = fcaseopen(SampleBankDataFilename, "rb");
	if ( fpSampleDataHandle == NULL )
	{
		fclose(fpSampleDescHandle);
		fpSampleDescHandle = NULL;
		
		return FALSE;
	}
	
	fseek(fpSampleDataHandle, 0, SEEK_END);
	int32 _nSampleDataEndOffset = ftell(fpSampleDataHandle);
	rewind(fpSampleDataHandle);
#else
	int e;
	fpSampleDataHandle = op_open_file(SampleBankDataFilename, &e);
#endif
	fread(m_aSamples, sizeof(tSample), TOTAL_AUDIO_SAMPLES, fpSampleDescHandle);
#ifdef OPUS_SFX
	int32 _nSampleDataEndOffset = m_aSamples[TOTAL_AUDIO_SAMPLES - 1].nOffset + m_aSamples[TOTAL_AUDIO_SAMPLES - 1].nSize;
#endif
	fclose(fpSampleDescHandle);
	fpSampleDescHandle = NULL;
	
	for ( uint32 i = 0; i < TOTAL_AUDIO_SAMPLES; i++ )
	{
#ifdef FIX_BUGS
		if (nBank >= MAX_SFX_BANKS) break;
#endif
		if ( BankStartOffset[nBank] == BankStartOffset[SFX_BANK_0] + i )
		{
			nSampleBankDiscStartOffset[nBank] = m_aSamples[i].nOffset;
			nBank++;
		}
	}

	nSampleBankSize[SFX_BANK_0] = nSampleBankDiscStartOffset[SFX_BANK_PED_COMMENTS] - nSampleBankDiscStartOffset[SFX_BANK_0];
	nSampleBankSize[SFX_BANK_PED_COMMENTS]  = _nSampleDataEndOffset                      - nSampleBankDiscStartOffset[SFX_BANK_PED_COMMENTS];

	return TRUE;
}

void
cSampleManager::SetStreamedFileLoopFlag(bool8 nLoopFlag, uint8 nChannel)
{
	nStreamLoopedFlag[nChannel] = nLoopFlag;
}

#endif
