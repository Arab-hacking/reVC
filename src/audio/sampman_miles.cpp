#define WITHWINDOWS
#include "common.h"
#include "Game.h"

#ifdef AUDIO_MSS
#include <shlobj.h>
#include <shlguid.h>

#include <time.h>

#include "eax.h"
#include "eax-util.h"
#include "mss.h"

#include "sampman.h"
#include "AudioManager.h"
#include "MusicManager.h"
#include "Frontend.h"
#include "Timer.h"
#include "crossplatform.h"

#pragma comment( lib, "mss32.lib" )

cSampleManager SampleManager;
uint32 BankStartOffset[MAX_SFX_BANKS];
///////////////////////////////////////////////////////////////

char SampleBankDescFilename[] = "AUDIO\\SFX.SDT";
char SampleBankDataFilename[] = "AUDIO\\SFX.RAW";

FILE *fpSampleDescHandle;
FILE *fpSampleDataHandle;
int8  gBankLoaded                  [MAX_SFX_BANKS];
int32 nSampleBankDiscStartOffset   [MAX_SFX_BANKS];
int32 nSampleBankSize              [MAX_SFX_BANKS];
int32 nSampleBankMemoryStartAddress[MAX_SFX_BANKS];
int32 _nSampleDataEndOffset;

int32 nPedSlotSfx    [MAX_PEDSFX];
int32 nPedSlotSfxAddr[MAX_PEDSFX];
uint8 nCurrentPedSlot;

#ifdef FIX_BUGS
uint32 gPlayerTalkSfx = UINT32_MAX;
void *gPlayerTalkData = 0;
#endif

uint8 nChannelVolume[MAXCHANNELS+MAX2DCHANNELS];

uint32 nStreamLength[TOTAL_STREAMED_SOUNDS];

///////////////////////////////////////////////////////////////
HSTREAM mp3Stream [MAX_STREAMS];
int8 nStreamPan   [MAX_STREAMS];
int8 nStreamVolume[MAX_STREAMS];
bool8 nStreamLoopedFlag[MAX_STREAMS];
///////////////////////////////////////////////////////////////


bool8 _bSampmanInitialised = FALSE;
#ifdef EXTERNAL_3D_SOUND
//
// Miscellaneous globals / defines

//	Env		Size	Diffus	Room	RoomHF	RoomLF	DecTm	DcHF	DcLF	Refl	RefDel	Ref Pan				Revb	RevDel		Rev Pan				EchTm	EchDp	ModTm	ModDp	AirAbs	HFRef		LFRef	RRlOff	FLAGS

EAXLISTENERPROPERTIES StartEAX3 =
	{26,	1.7f,	0.8f,	-1000,	-1000,	-100,	4.42f,	0.14f,	1.00f,	429,	0.014f,	0.00f,0.00f,0.00f,	1023,	0.021f,		0.00f,0.00f,0.00f,	0.250f,	0.000f,	0.250f,	0.000f,	-5.0f,	2727.1f,	250.0f,	0.00f,	0x3f };

EAXLISTENERPROPERTIES FinishEAX3 =
	{26,	100.0f,	1.0f,	0,		-1000,	-2200,	20.0f,	1.39f,	1.00f,	1000,	0.069f,	0.00f,0.00f,0.00f,	400,	0.100f,		0.00f,0.00f,0.00f,	0.250f,	1.000f,	3.982f,	0.000f,	-18.0f,	3530.8f,	417.9f,	6.70f,	0x3f };

EAXLISTENERPROPERTIES EAX3Params;

S32         prevprovider=-1;
S32         curprovider=-1;
S32         usingEAX=0;
S32         usingEAX3=0;
HPROVIDER   opened_provider=0;
H3DSAMPLE   opened_samples[MAXCHANNELS] = {0};
#endif
HSAMPLE     opened_2dsamples[MAX2DCHANNELS] = {0};
HDIGDRIVER  DIG;
#ifdef EXTERNAL_3D_SOUND
S32         speaker_type=0;

U32 _maxSamples;
float _fPrevEaxRatioDestination;
bool8 _usingMilesFast2D;
float _fEffectsLevel;


struct
{
	HPROVIDER id;
	char name[80];
}providers[MAXPROVIDERS];

typedef struct provider_stuff
{
  char* name;
  HPROVIDER id;
} provider_stuff;


static int __cdecl comp(const provider_stuff*s1,const provider_stuff*s2)
{
  return( _stricmp(s1->name,s2->name) );
}

static void
add_providers()
{
	provider_stuff pi[MAXPROVIDERS];
	U32   n,i,j;
	
	SampleManager.SetNum3DProvidersAvailable(0);
	
	HPROENUM next = HPROENUM_FIRST;
	
	n=0;
	while (AIL_enumerate_3D_providers(&next, &pi[n].id, &pi[n].name) && (n<MAXPROVIDERS))
	{
		++n;
	}
	
	qsort(pi,n,sizeof(pi[0]), (int(__cdecl*)(void const*, void const*))comp);
	
	for(i=0;i<n;i++)
	{
		providers[i].id=pi[i].id;
		strcpy(providers[i].name, pi[i].name);
		SampleManager.Set3DProviderName(i, providers[i].name);
	}
	
	SampleManager.SetNum3DProvidersAvailable(n);
	
	for(j=n;j<MAXPROVIDERS;j++)
		SampleManager.Set3DProviderName(j, NULL);
}

static void
release_existing()
{
	for ( U32 i = 0; i < _maxSamples; i++ )
	{
		if ( opened_samples[i] )
		{
			AIL_release_3D_sample_handle(opened_samples[i]);
			opened_samples[i] = NULL;
		}
	}

	if ( opened_provider )
	{
		AIL_close_3D_provider(opened_provider);
		opened_provider = NULL;
	}

	_fPrevEaxRatioDestination = 0.0f;
	_usingMilesFast2D = FALSE;
	_fEffectsLevel = 0.0f;
}

static bool8
set_new_provider(S32 index)
{
	DWORD result;
	
	if ( curprovider == index )
		return TRUE;

	//close the already opened provider
	curprovider = index;
	
	release_existing();
	
	if ( curprovider != -1 )
	{
		if ( !strcmp(providers[index].name, "Dolby Surround") )
			_maxSamples = MAXCHANNELS_SURROUND;
		else
			_maxSamples = MAXCHANNELS;
		
		AIL_set_3D_provider_preference(providers[index].id, "Maximum supported samples", &_maxSamples);
		
		//load the new provider
		result = AIL_open_3D_provider(providers[index].id);
		
		if (result != M3D_NOERR) 
		{
			curprovider=-1;
			
			OutputDebugStringA(AIL_last_error());
			
			release_existing();
			
			return FALSE;
		}
		else
		{
			opened_provider=providers[index].id;
			
			//see if we're running under an EAX compatible provider
			
			if ( !strcmp(providers[index].name, "Creative Labs EAX 3 (TM)") )
			{
				usingEAX = 1;
				usingEAX3 = 1;
			}
			else
			{
				usingEAX3 = 0;

				result=AIL_3D_room_type(opened_provider);
				usingEAX=(((S32)result)!=-1)?1:0; // will be something other than -1 on EAX				
			}
			
			if ( usingEAX3 )
			{
				OutputDebugString("DOING SPECIAL EAX 3 STUFF!");
				AIL_set_3D_provider_preference(opened_provider, "EAX all parameters", &FinishEAX3);
			}
			else if ( usingEAX )
			{
				AIL_set_3D_room_type(opened_provider, ENVIRONMENT_CAVE);
				
				if ( !strcmp(providers[index].name, "Miles Fast 2D Positional Audio") )
					_usingMilesFast2D = TRUE;
			}
			
			AIL_3D_provider_attribute(opened_provider, "Maximum supported samples", &_maxSamples);
			
			if ( _maxSamples > MAXCHANNELS )
				_maxSamples = MAXCHANNELS;
			
			SampleManager.SetSpeakerConfig(speaker_type);
			
			//obtain a 3D sample handles
			for ( U32 i = 0; i < _maxSamples; ++i )
			{
				opened_samples[i] = AIL_allocate_3D_sample_handle(opened_provider);
				if ( opened_samples[i] != NULL )
					AIL_set_3D_sample_effects_level(opened_samples[i], 0.0f);
			}
			
			return TRUE;
		}	
	}
	
	return FALSE;
}
#endif

U32 RadioHandlers[9];

U32 WINAPI vfs_open_callback(char const* Filename, U32* FileHandle)
{
	*FileHandle = (U32)fopen(Filename, "rb");

	// couldn't they just use stricmp once? and strlen? this is very inefficient
	if ((strcmp(Filename + strlen(Filename) - 4, ".adf") == 0) || (strcmp(Filename + strlen(Filename) - 4, ".ADF") == 0)) {
		for (int i = 0; i < ARRAY_SIZE(RadioHandlers); i++) {
			if (RadioHandlers[i] == NULL) {
				RadioHandlers[i] = *FileHandle;
				break;
			}
		}
		strcpy((char*)Filename + strlen(Filename) - 4, ".mp3");
	}
	return *FileHandle;
}

void WINAPI vfs_close_callback(U32 FileHandle)
{
	for (int i = 0; i < ARRAY_SIZE(RadioHandlers); i++) {
		if (RadioHandlers[i] == FileHandle) {
			RadioHandlers[i] = NULL;
			break;
		}
	}
	fclose((FILE*)FileHandle);
}

S32 WINAPI vfs_seek_callback(U32 FileHandle, S32 Offset, U32 Type)
{
	fseek((FILE*)FileHandle, Offset, Type);
	return ftell((FILE*)FileHandle);
}

U32 WINAPI vfs_read_callback(U32 FileHandle, void* Buffer, U32 Bytes)
{
	fread(Buffer, Bytes, 1, (FILE*)FileHandle);
	uint8* _Buffer = (uint8*)Buffer;

	for (int i = 0; i < ARRAY_SIZE(RadioHandlers); i++) {
		if (FileHandle == RadioHandlers[i]) {
			for (U32 k = 0; k < Bytes; k++)
				_Buffer[k] ^= 0x22;
			break;
		}
	}
	return Bytes;
}

cSampleManager::cSampleManager(void) : 
	m_nNumberOfProviders(0)
{
	;

	AIL_set_file_callbacks(vfs_open_callback, vfs_close_callback, vfs_seek_callback, vfs_read_callback);
}

cSampleManager::~cSampleManager(void)
{
	
}

#ifdef EXTERNAL_3D_SOUND
void
cSampleManager::SetSpeakerConfig(int32 which)
{
	switch ( which )
	{
		case 1:
			speaker_type=AIL_3D_2_SPEAKER;
			break;
		
		case 2:
			speaker_type=AIL_3D_HEADPHONE;
			break;
		
		case 3:
			speaker_type=AIL_3D_4_SPEAKER;
			break;
			
		default:
			return;
			break;
	}
	
	if (opened_provider)
		AIL_set_3D_speaker_type(opened_provider, speaker_type);
}

uint32
cSampleManager::GetMaximumSupportedChannels(void)
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

int8
cSampleManager::GetCurrent3DProviderIndex(void)
{
	return curprovider;
}

int8
cSampleManager::SetCurrent3DProvider(uint8 nProvider)
{
	S32 savedprovider = curprovider;
	
	if ( nProvider < m_nNumberOfProviders )
	{
		if ( set_new_provider(nProvider) )
			return curprovider;
		else if ( savedprovider != -1 && savedprovider < m_nNumberOfProviders && set_new_provider(savedprovider) )
			return curprovider;
		else
			return -1;
	}
	else
		return curprovider;
}

int8
cSampleManager::AutoDetect3DProviders()
{
	if (!AudioManager.IsAudioInitialised())
		return -1;

	int eax = -1, eax2 = -1, eax3 = -1, ds3dh = -1, ds3ds = -1;

	for (uint32 i = 0; i < GetNum3DProvidersAvailable(); i++)
	{
		char* providername = Get3DProviderName(i);

		if (!strcasecmp(providername, "CREATIVE LABS EAX (TM)")) {
			AudioManager.SetCurrent3DProvider(i);
			if (GetCurrent3DProviderIndex() == i)
				eax = i;
		}

		if (!strcasecmp(providername, "CREATIVE LABS EAX 2 (TM)")) {
			AudioManager.SetCurrent3DProvider(i);
			if (GetCurrent3DProviderIndex() == i)
				eax2 = i;
		}

		if (!strcasecmp(providername, "CREATIVE LABS EAX 3 (TM)")) {
			AudioManager.SetCurrent3DProvider(i);
			if (GetCurrent3DProviderIndex() == i) {
				eax3 = i;
			}
		}

		if (!strcasecmp(providername, "DIRECTSOUND3D HARDWARE SUPPORT")) {
			AudioManager.SetCurrent3DProvider(i);
			if (GetCurrent3DProviderIndex() == i)
				ds3dh = i;
		}

		if (!strcasecmp(providername, "DIRECTSOUND3D SOFTWARE EMULATION")) {
			AudioManager.SetCurrent3DProvider(i);
			if (GetCurrent3DProviderIndex() == i)
				ds3ds = i;
		}
	}

	if (eax3 != -1)
		return eax3;
	if (eax2 != -1)
		return eax2;
	if (eax != -1)
		return eax;
	if (ds3dh != -1)
		return ds3dh;
	if (ds3ds != -1)
		return ds3ds;
	return -1;
}
#endif

void
cSampleManager::ReleaseDigitalHandle(void)
{
	if ( DIG )
	{
#ifdef EXTERNAL_3D_SOUND
		prevprovider = curprovider;
		release_existing();
		curprovider = -1;
#endif
		AIL_digital_handle_release(DIG);
	}
}

void
cSampleManager::ReacquireDigitalHandle(void)
{
	if ( DIG )
	{
		AIL_digital_handle_reacquire(DIG);
#ifdef EXTERNAL_3D_SOUND
		if ( prevprovider != -1 )
			set_new_provider(prevprovider);
#endif
	}
}

bool8
cSampleManager::Initialise(void)
{
	TRACE("start");
	
	if ( _bSampmanInitialised )
		return TRUE;

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

#ifdef EXTERNAL_3D_SOUND
	// miles 
	TRACE("MILES");
	{
		curprovider = -1;
		prevprovider = -1;
		
		_usingMilesFast2D = FALSE;
		usingEAX=0;
		usingEAX3=0;
		
		_fEffectsLevel = 0.0f;
		
		_maxSamples = 0;
	
		opened_provider = NULL;
		DIG = NULL;
		
		for ( int32 i = 0; i < MAXCHANNELS; i++ )
			opened_samples[i] = NULL;
	}
#endif

	// banks
	TRACE("banks");
	{
		fpSampleDescHandle = NULL;
		fpSampleDataHandle = NULL;
		
		_nSampleDataEndOffset = 0;
		
		for ( int32 i = 0; i < MAX_SFX_BANKS; i++ )
		{
			gBankLoaded[i]                   = LOADING_STATUS_NOT_LOADED;
			nSampleBankDiscStartOffset[i]    = 0;
			nSampleBankSize[i]               = 0;
			nSampleBankMemoryStartAddress[i] = 0;
		}
	}
	
	// pedsfx
	TRACE("pedsfx");
	{
		for ( int32 i = 0; i < MAX_PEDSFX; i++ )
		{
			nPedSlotSfx[i]     = NO_SAMPLE;
			nPedSlotSfxAddr[i] = 0;
		}
		
		nCurrentPedSlot = 0;
	}
	
	// channel volume
	TRACE("vol");
	{
		for ( int32 i = 0; i < MAXCHANNELS+MAX2DCHANNELS; i++ )
			nChannelVolume[i] = 0;
	}
	
	TRACE("mss");
	{
		AIL_set_redist_directory( "mss" );
		
		AIL_startup();
		
		AIL_set_preference(DIG_MIXER_CHANNELS, MAX_DIGITAL_MIXER_CHANNELS);
		
		DIG = AIL_open_digital_driver(DIGITALRATE, DIGITALBITS, DIGITALCHANNELS, 0);		
		
	}
	
#ifdef AUDIO_CACHE
	TRACE("cache");
	FILE *cacheFile = fcaseopen("audio\\sound.cache", "rb");
	bool8 CreateCache = FALSE;
	if (cacheFile) {
		fread(nStreamLength, sizeof(uint32), TOTAL_STREAMED_SOUNDS, cacheFile);
		fclose(cacheFile);
	}else
		CreateCache = TRUE;
#endif
	
	char filepath[MAX_PATH];
	bool8 bFileNotFound;
	S32 tatalms;

	TRACE("cdrom");
	{
		m_bInitialised = FALSE;

		
		while (TRUE)
		{

			// Find path of WAVs (originally in HDD)
			int32 drive = 'C';
			
#ifndef NO_CDCHECK
			do
			{
				char latter[2];
				
				latter[0] = drive;
				latter[1] = '\0';
				
				strcpy(m_szCDRomRootPath, latter);
				strcat(m_szCDRomRootPath, ":\\");
				
				if ( GetDriveType(m_szCDRomRootPath) == DRIVE_CDROM )
				{
					FILE *f;
#ifdef PS2_AUDIO_PATHS
					strcpy(filepath, m_szCDRomRootPath);
					strcat(filepath, PS2StreamedNameTable[0]);
					f = fopen(filepath, "rb");

					if ( !f )
#endif
					{
						strcpy(filepath, m_szCDRomRootPath);
						strcat(filepath, StreamedNameTable[0]);
					
						f = fopen(filepath, "rb");
					}
					if ( f )
					{
						fclose(f);
						strcpy(m_MiscomPath, m_szCDRomRootPath);
						break;
					}
				}

			} while ( ++drive <= 'Z' );
#else
			m_MiscomPath[0] = '\0';
#endif

			if ( DIG == NULL )
			{
				OutputDebugString(AIL_last_error());
				Terminate();
				return FALSE;
			}

#ifdef EXTERNAL_3D_SOUND
			add_providers();
#endif

			m_szCDRomRootPath[0] = '\0';

			strcpy(m_WavFilesPath, m_szCDRomRootPath);

#ifdef AUDIO_CACHE
			if ( CreateCache )
#endif
			for ( int32 i = SFX_MISSION_MOBR1; i < TOTAL_STREAMED_SOUNDS; i++ )
			{
#ifdef PS2_AUDIO_PATHS
				strcpy(filepath, m_szCDRomRootPath);
				strcat(filepath, PS2StreamedNameTable[i]);

				mp3Stream[0] = AIL_open_stream(DIG, filepath, 0);

				if ( !mp3Stream[0] )
#endif
				{
					strcpy(filepath, m_szCDRomRootPath);
					strcat(filepath, StreamedNameTable[i]);

					mp3Stream[0] = AIL_open_stream(DIG, filepath, 0);
				}
				
				if ( mp3Stream[0] )
				{
					AIL_stream_ms_position(mp3Stream[0], &tatalms, NULL);
					
					AIL_close_stream(mp3Stream[0]);
					mp3Stream[0] = NULL;
					
					nStreamLength[i] = tatalms;
				}
				else
				{
					m_bInitialised = FALSE;
					Terminate();
					return FALSE;
				}
			}

			// Find path of MP3s (originally in CD-Rom)
			// if NO_CDCHECK is NOT defined but AUDIO_CACHE is defined, we still need to find MP3s' path, but will exit after the first file
#ifndef NO_CDCHECK
			int32 drive = 'C';
			do
			{
				latter[0] = drive;
				latter[1] = '\0';
				
				strcpy(m_szCDRomRootPath, latter);
				strcat(m_szCDRomRootPath, ":");
				strcat(m_MP3FilesPath, m_szCDRomRootPath);
#else
			m_MP3FilesPath[0] = '\0';
			{
#endif

				for (int32 i = 0; i < STREAMED_SOUND_CD_CHECK_1; i++)
				{
#ifdef PS2_AUDIO_PATHS
					strcpy(filepath, m_MP3FilesPath);
					strcat(filepath, PS2StreamedNameTable[i]);

					mp3Stream[0] = AIL_open_stream(DIG, filepath, 0);

					if ( !mp3Stream[0] )
#endif
					{
						strcpy(filepath, m_MP3FilesPath);
						strcat(filepath, StreamedNameTable[i]);

						mp3Stream[0] = AIL_open_stream(DIG, filepath, 0);
					}

					if (mp3Stream[0])
					{
						AIL_stream_ms_position(mp3Stream[0], &tatalms, NULL);

						AIL_close_stream(mp3Stream[0]);
						mp3Stream[0] = NULL;

						bFileNotFound = FALSE;
#ifdef AUDIO_CACHE
						if (!CreateCache)
							break;
						else
#endif
							nStreamLength[i] = tatalms;

					}
					else
					{
						bFileNotFound = TRUE;
						break;
					}
				}

#ifndef NO_CDCHECK
				if (!bFileNotFound) // otherwise try next drive
					break;

			}
			while (++drive <= 'Z');
#else
			}
#endif

			if ( !bFileNotFound ) {

#ifdef AUDIO_CACHE
			if ( CreateCache )
#endif
				for ( int32 i = STREAMED_SOUND_CD_CHECK_1; i < SFX_MISSION_MOBR1; i++ )
				{
#ifdef PS2_AUDIO_PATHS
					strcpy(filepath, m_MiscomPath);
					strcat(filepath, PS2StreamedNameTable[i]);

					mp3Stream[0] = AIL_open_stream(DIG, filepath, 0);

					if ( !mp3Stream[0] )
#endif
					{
						strcpy(filepath, m_MiscomPath);
						strcat(filepath, StreamedNameTable[i]);
					
						mp3Stream[0] = AIL_open_stream(DIG, filepath, 0);
					}
					
					if ( mp3Stream[0] )
					{
						AIL_stream_ms_position(mp3Stream[0], &tatalms, NULL);
						
						AIL_close_stream(mp3Stream[0]);
						mp3Stream[0] = NULL;
						
						nStreamLength[i] = tatalms;
						bFileNotFound = FALSE;
					}
					else
					{
						bFileNotFound = TRUE;
						break;
					}
				}
			}
			
			m_bInitialised = !bFileNotFound;

			if ( !m_bInitialised )
			{
#if !defined(GTA3_STEAM_PATCH) && !defined(NO_CDCHECK)
				FrontEndMenuManager.WaitForUserCD();
				if ( FrontEndMenuManager.m_bQuitGameNoCD )
				{
					Terminate();
					return FALSE;
				}
				continue;
#else
				m_bInitialised = TRUE;
#endif
			}
			
			break;
		}
	}

#ifdef AUDIO_CACHE
	if (CreateCache) {
		cacheFile = fcaseopen("audio\\sound.cache", "wb");
		fwrite(nStreamLength, sizeof(uint32), TOTAL_STREAMED_SOUNDS, cacheFile);
		fclose(cacheFile);
	}
#endif

	if ( !InitialiseSampleBanks() )
	{
		Terminate();
		return FALSE;
	}
	
	nSampleBankMemoryStartAddress[SFX_BANK_0] = (int32)AIL_mem_alloc_lock(nSampleBankSize[SFX_BANK_0]);
	if ( !nSampleBankMemoryStartAddress[SFX_BANK_0] )
	{
		Terminate();
		return FALSE;
	}

	nSampleBankMemoryStartAddress[SFX_BANK_PED_COMMENTS] = (int32)AIL_mem_alloc_lock(PED_BLOCKSIZE*MAX_PEDSFX);

#ifdef FIX_BUGS
	// Find biggest player comment
	uint32 nMaxPedSize = 0;
	for (uint32 i = PLAYER_COMMENTS_START; i <= PLAYER_COMMENTS_END; i++)
		nMaxPedSize = Max(nMaxPedSize, m_aSamples[i].nSize);

	gPlayerTalkData = AIL_mem_alloc_lock(nMaxPedSize);
	if ( !gPlayerTalkData )
	{
		Terminate();
		return FALSE;
	}
#endif

	LoadSampleBank(SFX_BANK_0);

	TRACE("stream");
	{
		for ( int32 i = 0; i < MAX_STREAMS; i++ )
		{
			mp3Stream    [i] = NULL;
			nStreamPan   [i] = 63;
			nStreamVolume[i] = 100;
		}
	}
	
	for ( int32 i = 0; i < MAX2DCHANNELS; i++ )
	{
		opened_2dsamples[i] = AIL_allocate_sample_handle(DIG);
		if ( opened_2dsamples[i] )
		{
			AIL_init_sample(opened_2dsamples[i]);
			AIL_set_sample_type(opened_2dsamples[i], DIG_F_MONO_16, DIG_PCM_SIGN);
		}
	}
	
	TRACE("providerset");
	{
		_bSampmanInitialised = TRUE;

#ifdef EXTERNAL_3D_SOUND
		U32 n = 0;
		
		while ( n < m_nNumberOfProviders )
		{
			if ( !strcmp(strupr(providers[n].name), "DIRECTSOUND3D SOFTWARE EMULATION") )
			{
				set_new_provider(n);
				break;
			}
			n++;
		}
		
		if ( n == m_nNumberOfProviders )
		{
			Terminate();
			return FALSE;
		}
#endif
	}
	
	// mp3
	TRACE("mp3");
	return TRUE;
}

void
cSampleManager::Terminate(void)
{
	for ( int32 i = 0; i < MAX_STREAMS; i++ )
	{
		if ( mp3Stream[i] )
		{
			AIL_pause_stream(mp3Stream[i], 1);
			AIL_close_stream(mp3Stream[i]);
			mp3Stream[i] = NULL;
		}
	}
	
	for ( int32 i = 0; i < MAX2DCHANNELS; i++ )
	{
		if ( opened_2dsamples[i] )
		{
			AIL_release_sample_handle(opened_2dsamples[i]);
			opened_2dsamples[i] = NULL;
		}
	}

#ifdef EXTERNAL_3D_SOUND
	release_existing();
#endif
	
	
	if ( nSampleBankMemoryStartAddress[SFX_BANK_0] != 0 )
	{
		AIL_mem_free_lock((void *)nSampleBankMemoryStartAddress[SFX_BANK_0]);
		nSampleBankMemoryStartAddress[SFX_BANK_0] = 0;
	}

	if ( nSampleBankMemoryStartAddress[SFX_BANK_PED_COMMENTS] != 0 )
	{
		AIL_mem_free_lock((void *)nSampleBankMemoryStartAddress[SFX_BANK_PED_COMMENTS]);
		nSampleBankMemoryStartAddress[SFX_BANK_PED_COMMENTS] = 0;
	}

#ifdef FIX_BUGS
	if ( gPlayerTalkData != 0)
	{
		AIL_mem_free_lock(gPlayerTalkData);
		gPlayerTalkData = 0;
		gPlayerTalkSfx = UINT32_MAX;
	}
#endif
	
	if ( DIG )
	{
		AIL_close_digital_driver(DIG);
		DIG = NULL;
	}
	
	AIL_shutdown();
	
	_bSampmanInitialised = FALSE;
}

bool8
cSampleManager::CheckForAnAudioFileOnCD(void)
{
#if !defined(NO_CDCHECK) // TODO: check steam, probably GTAVC_STEAM_PATCH needs to be added
	char filepath[MAX_PATH];
	FILE *f;
	
	strcpy(filepath, m_MiscomPath);
	strcat(filepath, StreamedNameTable[STREAMED_SOUND_CD_CHECK_1]);

	FILE *f = fopen(filepath, "rb");

	if ( f )
	{
		fclose(f);
		DMAudio.SetMusicMasterVolume(FrontEndMenuManager.m_PrefsMusicVolume);
		DMAudio.SetEffectsMasterVolume(FrontEndMenuManager.m_PrefsSfxVolume);
		DMAudio.Service();

		return TRUE;
	}

	DMAudio.SetMusicMasterVolume(0);
	DMAudio.SetEffectsMasterVolume(0);
	DMAudio.Service();

	return FALSE;
	
#else
	return TRUE;
#endif // #if !defined(NO_CDCHECK)
}

char
cSampleManager::GetCDAudioDriveLetter(void)
{
	if ( strlen(m_MiscomPath) != 0 )
		return m_MiscomPath[0];
	else
		return '\0';
}

void
cSampleManager::UpdateEffectsVolume(void) //[Y], cSampleManager::UpdateSoundBuffers ?
{
	if ( _bSampmanInitialised )
	{
		for ( int32 i = 0; i < MAXCHANNELS+MAX2DCHANNELS; i++ )
		{
#ifdef EXTERNAL_3D_SOUND
			if ( i < MAXCHANNELS )
			{
				if ( opened_samples[i] && GetChannelUsedFlag(i) )
				{
					if ( nChannelVolume[i] )
					{
						AIL_set_3D_sample_volume(opened_samples[i],
								m_nEffectsFadeVolume * nChannelVolume[i] * m_nEffectsVolume >> 14);
					}
				}
			}
			else
#endif
			{
				if ( opened_2dsamples[i - MAXCHANNELS] )
				{
					if ( GetChannelUsedFlag(i - MAXCHANNELS) )
					{
						if ( nChannelVolume[i - MAXCHANNELS] )
						{
							AIL_set_sample_volume(opened_2dsamples[i - MAXCHANNELS],
									m_nEffectsFadeVolume * nChannelVolume[i - MAXCHANNELS] * m_nEffectsVolume >> 14);
						}
					}
				}
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
	if ( CTimer::GetIsCodePaused() )
		return FALSE;
	
	if ( MusicManager.IsInitialised()
		&& CGame::InCutscene()
		&& nBank != SFX_BANK_0 )
	{
		return FALSE;
	}
	
	if ( fseek(fpSampleDataHandle, nSampleBankDiscStartOffset[nBank], SEEK_SET) != 0 )
		return FALSE;
	
	if ( fread((void *)nSampleBankMemoryStartAddress[nBank], 1, nSampleBankSize[nBank],fpSampleDataHandle) != nSampleBankSize[nBank] )
		return FALSE;
	
	gBankLoaded[nBank] = LOADING_STATUS_LOADED;
	
	return TRUE;
}

void
cSampleManager::UnloadSampleBank(uint8 nBank)
{
	gBankLoaded[nBank] = LOADING_STATUS_NOT_LOADED;
}

int8
cSampleManager::IsSampleBankLoaded(uint8 nBank)
{
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
	int8 slot;

	for ( int32 i = 0; i < _TODOCONST(3); i++ )
	{
		slot = nCurrentPedSlot - i - 1;
#ifdef FIX_BUGS
		if (slot < 0)
			slot += ARRAY_SIZE(nPedSlotSfx);
#endif
		if ( nComment == nPedSlotSfx[slot] )
			return LOADING_STATUS_LOADED;
	}
	
	return LOADING_STATUS_NOT_LOADED;
}

int32
cSampleManager::_GetPedCommentSlot(uint32 nComment)
{
	int8 slot;

	for ( int32 i = 0; i < _TODOCONST(3); i++ )
	{
		slot = nCurrentPedSlot - i - 1;
#ifdef FIX_BUGS
		if (slot < 0)
			slot += ARRAY_SIZE(nPedSlotSfx);
#endif
		if ( nComment == nPedSlotSfx[slot] )
			return slot;
	}
	
	return -1;
}

bool8
cSampleManager::LoadPedComment(uint32 nComment)
{
	if ( CTimer::GetIsCodePaused() )
		return FALSE;
	
	// no talking peds during cutsenes or the game end
	if (MusicManager.IsInitialised() && CGame::InCutscene())
		return FALSE;
	
	if ( fseek(fpSampleDataHandle, m_aSamples[nComment].nOffset, SEEK_SET) != 0 )
		return FALSE;
	
	if ( fread((void *)(nSampleBankMemoryStartAddress[SFX_BANK_PED_COMMENTS] + PED_BLOCKSIZE*nCurrentPedSlot), 1, m_aSamples[nComment].nSize, fpSampleDataHandle) != m_aSamples[nComment].nSize )
		return FALSE;
	
	nPedSlotSfxAddr[nCurrentPedSlot] = nSampleBankMemoryStartAddress[SFX_BANK_PED_COMMENTS] + PED_BLOCKSIZE*nCurrentPedSlot;
	nPedSlotSfx    [nCurrentPedSlot] = nComment;
	
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
	return m_aSamples[nSample].nFrequency;
}

uint32
cSampleManager::GetSampleLoopStartOffset(uint32 nSample)
{
	return m_aSamples[nSample].nLoopStart;
}

int32
cSampleManager::GetSampleLoopEndOffset(uint32 nSample)
{
	return m_aSamples[nSample].nLoopEnd;
}

uint32
cSampleManager::GetSampleLength(uint32 nSample)
{
	return m_aSamples[nSample].nSize >> 1;
}

bool8
cSampleManager::UpdateReverb(void)
{
#ifdef EXTERNAL_3D_SOUND
	if ( !usingEAX )
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
	
	if ( usingEAX3 )
	{
		fRatio = Min(fRatio * 1.67f, 1.0f);
		if ( EAX3ListenerInterpolate(&StartEAX3, &FinishEAX3, fRatio, &EAX3Params, false) )
		{
			AIL_set_3D_provider_preference(opened_provider, "EAX all parameters", &EAX3Params);
			_fEffectsLevel = fRatio * 0.75f;
		}
	}
	else
	{
		if ( _usingMilesFast2D )
			_fEffectsLevel = fRatio * 0.8f;
		else
			_fEffectsLevel = fRatio * 0.22f;
	}
	_fEffectsLevel = Min(_fEffectsLevel, 1.0f);

	_fPrevEaxRatioDestination = fRatio;
	
	return TRUE;
#endif
	return FALSE;
}

void
cSampleManager::SetChannelReverbFlag(uint32 nChannel, bool8 nReverbFlag)
{
#ifdef EXTERNAL_3D_SOUND
	bool8 b2d = FALSE;
	
	switch ( nChannel )
	{
		case CHANNEL_POLICE_RADIO:
		{
			b2d = TRUE;
			break;
		}
	}
	
	if ( usingEAX )
	{
		if ( nReverbFlag != FALSE )
		{
			if ( !b2d )
				AIL_set_3D_sample_effects_level(opened_samples[nChannel], _fEffectsLevel);
		}
		else
		{
			if ( !b2d )
				AIL_set_3D_sample_effects_level(opened_samples[nChannel], 0.0f);
		}
	}
#endif
}

bool8
cSampleManager::InitialiseChannel(uint32 nChannel, uint32 nSfx, uint8 nBank)
{
#ifdef EXTERNAL_3D_SOUND
	bool8 b2d = FALSE;

	switch ( nChannel )
	{
		case CHANNEL_POLICE_RADIO:
		{
			b2d = TRUE;
			break;
		}
	}
#endif
	
	int32 addr;
	
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
				addr = nPedSlotSfxAddr[slot];
				break;
			}
		}

		if (i == _TODOCONST(3))
			return FALSE;
	}

#ifdef EXTERNAL_3D_SOUND
	if ( b2d )
	{
#endif
		if ( opened_2dsamples[nChannel - MAXCHANNELS] )
		{
			AIL_set_sample_address(opened_2dsamples[nChannel - MAXCHANNELS], (void *)addr, m_aSamples[nSfx].nSize);
			return TRUE;
		}
		else
			return FALSE;
#ifdef EXTERNAL_3D_SOUND
	}
	else
	{
		AILSOUNDINFO info;
		
		info.format   = WAVE_FORMAT_PCM;
		info.data_ptr = (void *)addr;
		info.channels = 1;
		info.data_len = m_aSamples[nSfx].nSize;
		info.rate     = m_aSamples[nSfx].nFrequency;
		info.bits     = 16;
	
		if ( AIL_set_3D_sample_info(opened_samples[nChannel], &info) == 0 )
		{
			OutputDebugString(AIL_last_error());
			return FALSE;
		}
		
		return TRUE;
	}
#endif
}

#ifdef EXTERNAL_3D_SOUND
void
cSampleManager::SetChannelEmittingVolume(uint32 nChannel, uint32 nVolume)
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

	if ( opened_samples[nChannel] )
		AIL_set_3D_sample_volume(opened_samples[nChannel], m_nEffectsFadeVolume*nChannelVolume[nChannel]*m_nEffectsVolume >> 14);

}

void
cSampleManager::SetChannel3DPosition(uint32 nChannel, float fX, float fY, float fZ)
{
	if ( opened_samples[nChannel] )
		AIL_set_3D_position(opened_samples[nChannel], -fX, fY, fZ);
}

void
cSampleManager::SetChannel3DDistances(uint32 nChannel, float fMax, float fMin)
{
	if ( opened_samples[nChannel] )
		AIL_set_3D_sample_distances(opened_samples[nChannel], fMax, fMin);
}
#endif

void
cSampleManager::SetChannelVolume(uint32 nChannel, uint32 nVolume)
{
	uint32 vol = nVolume;
	if ( vol > MAX_VOLUME ) vol = MAX_VOLUME;

#ifdef EXTERNAL_3D_SOUND
	switch ( nChannel )
	{
		case CHANNEL_POLICE_RADIO:
		{
#endif
			nChannelVolume[nChannel] = vol;
			
			// increase the volume for JB.MP3 and S4_BDBD.MP3
			if (CGame::InCutscene()
				&& MusicManager.GetCurrentTrack() != STREAMED_SOUND_CUTSCENE_FINALE )
			{
				nChannelVolume[nChannel] >>= 2;
			}

			if ( opened_2dsamples[nChannel - MAXCHANNELS] )
			{
				AIL_set_sample_volume(opened_2dsamples[nChannel - MAXCHANNELS],
						m_nEffectsFadeVolume*vol*m_nEffectsVolume >> 14);
			}

#ifdef EXTERNAL_3D_SOUND
			break;
		}
	}
#endif
}

void
cSampleManager::SetChannelPan(uint32 nChannel, uint32 nPan)
{
#ifdef EXTERNAL_3D_SOUND
	switch ( nChannel )
	{
		case CHANNEL_POLICE_RADIO:
		{
#endif
#if !defined(FIX_BUGS) && defined(EXTERNAL_3D_SOUND)
			if ( opened_samples[nChannel - MAXCHANNELS] ) // BUG
#else
			if ( opened_2dsamples[nChannel - MAXCHANNELS] )
#endif
				AIL_set_sample_pan(opened_2dsamples[nChannel - MAXCHANNELS], nPan);

#ifdef EXTERNAL_3D_SOUND
			break;
		}
	}
#endif
}

void
cSampleManager::SetChannelFrequency(uint32 nChannel, uint32 nFreq)
{
#ifdef EXTERNAL_3D_SOUND
	bool8 b2d = FALSE;

	switch ( nChannel )
	{
		case CHANNEL_POLICE_RADIO:
		{
			b2d = TRUE;
			break;
		}
	}

	if ( b2d )
	{
#endif
		if ( opened_2dsamples[nChannel - MAXCHANNELS] )
			AIL_set_sample_playback_rate(opened_2dsamples[nChannel - MAXCHANNELS], nFreq);
#ifdef EXTERNAL_3D_SOUND
	}
	else
	{
#ifdef FIX_BUGS
		if (nFreq == 0) nFreq = 1;
#endif
		if ( opened_samples[nChannel] )
			AIL_set_3D_sample_playback_rate(opened_samples[nChannel], nFreq);
	}
#endif
}

void
cSampleManager::SetChannelLoopPoints(uint32 nChannel, uint32 nLoopStart, int32 nLoopEnd)
{
#ifdef EXTERNAL_3D_SOUND
	bool8 b2d = FALSE;

	switch ( nChannel )
	{
		case CHANNEL_POLICE_RADIO:
		{
			b2d = TRUE;
			break;
		}
	}
	
	if ( b2d )
	{
#endif
		if ( opened_2dsamples[nChannel - MAXCHANNELS] )
			AIL_set_sample_loop_block(opened_2dsamples[nChannel - MAXCHANNELS], nLoopStart, nLoopEnd);
#ifdef EXTERNAL_3D_SOUND
	}
	else
	{
		if ( opened_samples[nChannel] )
			AIL_set_3D_sample_loop_block(opened_samples[nChannel], nLoopStart, nLoopEnd);
	}
#endif
}

void
cSampleManager::SetChannelLoopCount(uint32 nChannel, uint32 nLoopCount)
{
#ifdef EXTERNAL_3D_SOUND
	bool8 b2d = FALSE;

	switch ( nChannel )
	{
		case CHANNEL_POLICE_RADIO:
		{
			b2d = TRUE;
			break;
		}
	}
	
	if ( b2d )
	{
#endif
		if ( opened_2dsamples[nChannel - MAXCHANNELS] )
			AIL_set_sample_loop_count(opened_2dsamples[nChannel - MAXCHANNELS], nLoopCount);
#ifdef EXTERNAL_3D_SOUND
	}
	else
	{
		if ( opened_samples[nChannel] )
			AIL_set_3D_sample_loop_count(opened_samples[nChannel], nLoopCount);
	}
#endif
}

bool8
cSampleManager::GetChannelUsedFlag(uint32 nChannel)
{
#ifdef EXTERNAL_3D_SOUND
	bool8 b2d = FALSE;

	switch ( nChannel )
	{
		case CHANNEL_POLICE_RADIO:
		{
			b2d = TRUE;
			break;
		}
	}
	
	if ( b2d )
	{
#endif
		if ( opened_2dsamples[nChannel - MAXCHANNELS] )
			return AIL_sample_status(opened_2dsamples[nChannel - MAXCHANNELS]) == SMP_PLAYING;
		else
			return FALSE;
#ifdef EXTERNAL_3D_SOUND
	}
	else
	{
		if ( opened_samples[nChannel] )
			return AIL_3D_sample_status(opened_samples[nChannel]) == SMP_PLAYING;
		else
			return FALSE;
	}
#endif
	
}

void
cSampleManager::StartChannel(uint32 nChannel)
{
#ifdef EXTERNAL_3D_SOUND
	bool8 b2d = FALSE;

	switch ( nChannel )
	{
		case CHANNEL_POLICE_RADIO:
		{
			b2d = TRUE;
			break;
		}
	}

	if ( b2d )
	{
#endif
		if ( opened_2dsamples[nChannel - MAXCHANNELS] )
			AIL_start_sample(opened_2dsamples[nChannel - MAXCHANNELS]);
#ifdef EXTERNAL_3D_SOUND
	}
	else
	{
		if ( opened_samples[nChannel] )
			AIL_start_3D_sample(opened_samples[nChannel]);
	}
#endif
}

void
cSampleManager::StopChannel(uint32 nChannel)
{
#ifdef EXTERNAL_3D_SOUND
	bool8 b2d = FALSE;

	switch ( nChannel )
	{
		case CHANNEL_POLICE_RADIO:
		{
			b2d = TRUE;
			break;
		}
	}
	
	if ( b2d )
	{
#endif
		if ( opened_2dsamples[nChannel - MAXCHANNELS] )
			AIL_end_sample(opened_2dsamples[nChannel - MAXCHANNELS]);
#ifdef EXTERNAL_3D_SOUND
	}
	else
	{
		if ( opened_samples[nChannel] )
		{
			if ( AIL_3D_sample_status(opened_samples[nChannel]) == SMP_PLAYING )
				AIL_end_3D_sample(opened_samples[nChannel]);
		}
	}
#endif
}

void
cSampleManager::PreloadStreamedFile(tTrack nFile, uint8 nStream)
{
	if ( m_bInitialised  )
	{
		if ( nFile < TOTAL_STREAMED_SOUNDS )
		{
			if ( mp3Stream[nStream] )
			{
				AIL_pause_stream(mp3Stream[nStream], 1);
				AIL_close_stream(mp3Stream[nStream]);
			}
			
			char filepath[MAX_PATH];
#ifdef PS2_AUDIO_PATHS
			strcpy(filepath, nFile < STREAMED_SOUND_CD_CHECK_1 ? m_MP3FilesPath : (nFile < SFX_MISSION_MOBR1 ? m_MiscomPath : m_WavFilesPath));
			strcat(filepath, PS2StreamedNameTable[nFile]);

			mp3Stream[nStream] = AIL_open_stream(DIG, filepath, 0);

			if ( !mp3Stream[nStream] )
#endif
			{
				strcpy(filepath, nFile < STREAMED_SOUND_CD_CHECK_1 ? m_MP3FilesPath : (nFile < SFX_MISSION_MOBR1 ? m_MiscomPath : m_WavFilesPath));
				strcat(filepath, StreamedNameTable[nFile]);
			
				mp3Stream[nStream] = AIL_open_stream(DIG, filepath, 0);
			}
	
			if ( mp3Stream[nStream] )
			{
				AIL_set_stream_loop_count(mp3Stream[nStream], 1);
				AIL_service_stream(mp3Stream[nStream], 1);
			}
			else
				OutputDebugString(AIL_last_error());
		}
	}
}

void
cSampleManager::PauseStream(bool8 nPauseFlag, uint8 nStream)
{
	if ( m_bInitialised )
	{
		if ( mp3Stream[nStream] )
			AIL_pause_stream(mp3Stream[nStream], nPauseFlag != FALSE);
	}
}

void
cSampleManager::StartPreloadedStreamedFile(uint8 nStream)
{
	if ( m_bInitialised )
	{
		if ( mp3Stream[nStream] )
			AIL_start_stream(mp3Stream[nStream]);
	}
}

bool8
cSampleManager::StartStreamedFile(tTrack nFile, uint32 nPos, uint8 nStream)
{
	uint32 position = nPos;
	char filename[MAX_PATH];
	
	if ( !m_bInitialised || nFile >= TOTAL_STREAMED_SOUNDS )
		return FALSE;
#ifndef GTA_PS2
	if (nFile == STREAMED_SOUND_LEGACY_USERTRACK_SLOT)
		return FALSE;
#endif

	if ( mp3Stream[nStream] )
	{
		AIL_pause_stream(mp3Stream[nStream], 1);
		AIL_close_stream(mp3Stream[nStream]);
	}
#ifdef PS2_AUDIO_PATHS
	strcpy(filename, m_MiscomPath);
	strcat(filename, PS2StreamedNameTable[nFile]);

	mp3Stream[nStream] = AIL_open_stream(DIG, filename, 0);

	if ( !mp3Stream[nStream] )
#endif
	{
		strcpy(filename, m_MiscomPath);
		strcat(filename, StreamedNameTable[nFile]);
		mp3Stream[nStream] = AIL_open_stream(DIG, filename, 0);
	}

	if ( mp3Stream[nStream] )
	{
		AIL_set_stream_loop_count(mp3Stream[nStream], nStreamLoopedFlag[nStream] ? 0 : 1);
		nStreamLoopedFlag[nStream] = TRUE;
		AIL_set_stream_ms_position(mp3Stream[nStream], position);
		AIL_pause_stream(mp3Stream[nStream], 0);
		return TRUE;
	}
	return FALSE;
}

void
cSampleManager::StopStreamedFile(uint8 nStream)
{
	if ( m_bInitialised )
	{
		if ( mp3Stream[nStream] )
		{
			AIL_pause_stream(mp3Stream[nStream], 1);
			
			AIL_close_stream(mp3Stream[nStream]);
			mp3Stream[nStream] = NULL;
			
		}
	}
}

int32
cSampleManager::GetStreamedFilePosition(uint8 nStream)
{
	S32 currentms;
	
	if ( m_bInitialised )
	{
		if ( mp3Stream[nStream] )
		{
			AIL_stream_ms_position(mp3Stream[nStream], NULL, &currentms);
			return currentms;
		}
	}
	
	return 0;
}

void
cSampleManager::SetStreamedVolumeAndPan(uint8 nVolume, uint8 nPan, bool8 nEffectFlag, uint8 nStream)
{
	uint8 vol = nVolume;
	if ( m_bInitialised )
	{
		if ( vol > MAX_VOLUME ) vol = MAX_VOLUME;
		
		nStreamVolume[nStream] = vol;
		nStreamPan[nStream]    = nPan;
		
		if ( mp3Stream[nStream] )
		{
			if ( nEffectFlag )
			{
				if ( nStream == 1 || nStream == 2 )
					AIL_set_stream_volume(mp3Stream[nStream], 128*vol*m_nEffectsVolume >> 14);
				else
					AIL_set_stream_volume(mp3Stream[nStream], m_nEffectsFadeVolume*vol*m_nEffectsVolume >> 14);
			}
			else
				AIL_set_stream_volume(mp3Stream[nStream], (m_nMusicFadeVolume*vol*(uint32)m_nMusicVolume) >> 14);
			
			AIL_set_stream_pan(mp3Stream[nStream], nPan);
		}
	}
}

int32
cSampleManager::GetStreamedFileLength(uint8 nStream)
{
	if ( m_bInitialised )
		return nStreamLength[nStream];
	
	return 0;
}

bool8
cSampleManager::IsStreamPlaying(uint8 nStream)
{
	if ( m_bInitialised )
	{
		if ( mp3Stream[nStream] )
		{
			if ( AIL_stream_status(mp3Stream[nStream]) == SMP_PLAYING )
				return TRUE;
			else
				return FALSE;
		}
	}
	
	return FALSE;
}

bool8
cSampleManager::InitialiseSampleBanks(void)
{
	int32 nBank = SFX_BANK_0;
	
	fpSampleDescHandle = fopen(SampleBankDescFilename, "rb");
	if ( fpSampleDescHandle == NULL )
		return FALSE;
	
	fpSampleDataHandle = fopen(SampleBankDataFilename, "rb");
	if ( fpSampleDataHandle == NULL )
	{
		fclose(fpSampleDescHandle);
		fpSampleDescHandle = NULL;
		
		return FALSE;
	}
	
	fseek(fpSampleDataHandle, 0, SEEK_END);
	_nSampleDataEndOffset = ftell(fpSampleDataHandle);
	rewind(fpSampleDataHandle);
	
	fread(m_aSamples, sizeof(tSample), TOTAL_AUDIO_SAMPLES, fpSampleDescHandle);
	
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
	nSampleBankSize[SFX_BANK_PED_COMMENTS] = _nSampleDataEndOffset                       - nSampleBankDiscStartOffset[SFX_BANK_PED_COMMENTS];
	
	return TRUE;
}


void
cSampleManager::SetStreamedFileLoopFlag(bool8 nLoopFlag, uint8 nChannel)
{
	if (m_bInitialised)
		nStreamLoopedFlag[nChannel] = nLoopFlag;
}

#endif
