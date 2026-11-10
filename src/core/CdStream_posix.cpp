#ifndef _WIN32
#include "common.h"
#if defined(GTA_PC) || defined(GTA_MOBILE)
#include "crossplatform.h"
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/time.h>
#ifndef ANDROID
#include <sys/statvfs.h>
#else
#include "AndroidMain.h"
#include <sys/vfs.h>
#define statvfs statfs
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/resource.h>
#include <stdarg.h>
#include <limits.h>

#ifdef __linux__
#include <sys/syscall.h>
#endif

#include "CdStream.h"
#include "FileMgr.h"
#include "MemoryMgr.h"

#define CDDEBUG(f, ...)   debug ("%s: " f "\n", "cdvd_stream", ## __VA_ARGS__)
#define CDTRACE(f, ...)   debug ("%s: " f "\n", "cdvd_stream trace", ## __VA_ARGS__)

#ifdef FLUSHABLE_STREAMING
bool flushStream[MAX_CDCHANNELS];
#endif

#ifdef USE_UNNAMED_SEM

#define RE3_SEM_OPEN(name, ...) re3_sem_open()
sem_t*
re3_sem_open(void)
{
	sem_t* sem = (sem_t*)malloc(sizeof(sem_t));
	if (!sem || sem_init(sem, 0, 1) == -1) {
		free(sem);
		sem = SEM_FAILED;
	}

	return sem;
}

#define RE3_SEM_CLOSE(sem, format, ...) re3_sem_close(sem)
void
re3_sem_close(sem_t* sem)
{
	if (sem && sem != SEM_FAILED) {
		sem_destroy(sem);
		free(sem);
	}
}

#else

#define RE3_SEM_OPEN re3_sem_open
sem_t*
re3_sem_open(const char* format, ...)
{
    char semName[21];
    va_list va;
    va_start(va, format);
    int written = vsnprintf(semName, sizeof(semName), format, va);
    va_end(va);
    if (written < 0 || written >= (int)sizeof(semName))
        return SEM_FAILED;

    return sem_open(semName, O_CREAT, 0644, 1);
}

#define RE3_SEM_CLOSE re3_sem_close
void
re3_sem_close(sem_t* sem, const char* format, ...)
{
    sem_close(sem);

    char semName[21];
    va_list va;
    va_start(va, format);
    int written = vsnprintf(semName, sizeof(semName), format, va);
    va_end(va);

    if (written > 0 && written < (int)sizeof(semName))
        sem_unlink(semName);
}

#endif

struct CdReadInfo
{
	uint32 nSectorOffset;
	uint32 nSectorsToRead;
	void *pBuffer;
	bool bLocked;
	bool bReading;
	int32 nStatus;
#ifdef ONE_THREAD_PER_CHANNEL
	int8 nThreadStatus; // 0: created 1:priority set up 2:abort now
	pthread_t pChannelThread;
	sem_t *pStartSemaphore;
#endif
	sem_t *pDoneSemaphore; // used for CdStreamSync
	int32 hFile;
};

char gCdImageNames[MAX_CDIMAGES+1][256];
int32 gNumImages;
int32 gNumChannels;

int32 gImgFiles[MAX_CDIMAGES]; // -1: error 0:unused otherwise: fd
char *gImgNames[MAX_CDIMAGES];

#ifndef ONE_THREAD_PER_CHANNEL
pthread_t _gCdStreamThread;
sem_t *gCdStreamSema; // released when we have new thing to read(so channel is set)
int8 gCdStreamThreadStatus; // 0: created 1:priority set up 2:abort now
Queue gChannelRequestQ;
// The request queue is written by the game thread and read by the streaming
// thread; without this lock head/tail were updated concurrently and requests
// were silently lost (or the same channel read twice).
pthread_mutex_t gChannelRequestQMutex = PTHREAD_MUTEX_INITIALIZER;
int32 gChannelRequestQCount;
bool _gbCdStreamOverlapped;
#endif

CdReadInfo *gpReadInfo;

int32 lastPosnRead;

int _gdwCdStreamFlags;

void *CdStreamThread(void* channelId);

void
CdStreamInitThread(void)
{
	int status;
#ifndef ONE_THREAD_PER_CHANNEL
	gChannelRequestQ.items = (int32 *)calloc(gNumChannels + 1, sizeof(int32));
	gChannelRequestQ.head = 0;
	gChannelRequestQ.tail = 0;
	gChannelRequestQ.size = gNumChannels + 1;
	gChannelRequestQCount = 0;
	ASSERT(gChannelRequestQ.items != nil);
    gCdStreamSema = RE3_SEM_OPEN("/semaphore_cd_stream");

    if (gCdStreamSema == SEM_FAILED) {
        CDTRACE("failed to create stream semaphore");
        ASSERT(0);
        return;
    }
#endif

	if ( gNumChannels > 0 )
	{
		for ( int32 i = 0; i < gNumChannels; i++ )
		{
            gpReadInfo[i].pDoneSemaphore = RE3_SEM_OPEN("/semaphore_done%d", i);

			if (gpReadInfo[i].pDoneSemaphore == SEM_FAILED)
			{
				CDTRACE("failed to create sync semaphore");
				ASSERT(0);
				return;
			}

#ifdef ONE_THREAD_PER_CHANNEL
			gpReadInfo[i].pStartSemaphore = RE3_SEM_OPEN("/semaphore_start%d", i);

			if (gpReadInfo[i].pStartSemaphore == SEM_FAILED)
			{
				CDTRACE("failed to create start semaphore");
				ASSERT(0);
				return;
			}
			gpReadInfo[i].nThreadStatus = 0;
			int *channelI = (int*)malloc(sizeof(int));
			*channelI = i;
			status = pthread_create(&gpReadInfo[i].pChannelThread, NULL, CdStreamThread, (void*)channelI);

			if (status == -1)
			{
				CDTRACE("failed to create sync thread");
				ASSERT(0);
				return;
			}
#endif
		}
	}

#ifndef ONE_THREAD_PER_CHANNEL
	debug("Using one streaming thread for all channels\n");
	gCdStreamThreadStatus = 0;
	status = pthread_create(&_gCdStreamThread, NULL, CdStreamThread, nil);

	if (status == -1)
	{
		CDTRACE("failed to create sync thread");
		ASSERT(0);
		return;
	}
#else
	debug("Using separate streaming threads for each channel\n");
#endif
}

void
CdStreamInit(int32 numChannels)
{
	struct statvfs fsInfo;
	memset(&fsInfo, 0, sizeof(fsInfo));
#if defined ANDROID
	const char *rootDir = CFileMgr::GetRootDirName();
	if (StorageRootBuffer == NULL && rootDir != NULL && rootDir[0] != '\0') {
		setenv("STORAGE_ROOT", rootDir, 1);
		StorageRootBuffer = getenv("STORAGE_ROOT");
	}
	const char *vfsTarget = (rootDir != NULL && rootDir[0] != '\0') ? rootDir : ".";
	if (statvfs(vfsTarget, &fsInfo) < 0 || fsInfo.f_bsize <= 0) {
		fsInfo.f_bsize = 4096;
	}
#else
	if (statvfs("models/gta3.img", &fsInfo) < 0 && statvfs(".", &fsInfo) < 0) {
		CDTRACE("can't get filesystem info");
		fsInfo.f_bsize = 4096;
	}
#endif
#if defined ANDROID
	_gdwCdStreamFlags = O_RDONLY;
#elif defined __linux__
	_gdwCdStreamFlags = O_RDONLY | O_NOATIME;
#else
	_gdwCdStreamFlags = O_RDONLY;
#endif
	RwUInt32 alignSize = 4096;
	if (fsInfo.f_bsize >= 512 && ((RwUInt32)fsInfo.f_bsize & ((RwUInt32)fsInfo.f_bsize - 1)) == 0)
		alignSize = (RwUInt32)fsInfo.f_bsize;
	void *pBuffer = (void *)RwMallocAlign(CDSTREAM_SECTOR_SIZE, alignSize);
	ASSERT( pBuffer != nil );

	gNumImages = 0;

	gNumChannels = numChannels;
	ASSERT( gNumChannels != 0 );

	gpReadInfo = (CdReadInfo *)calloc(numChannels, sizeof(CdReadInfo));
	ASSERT( gpReadInfo != nil );

	CDDEBUG("read info %p", gpReadInfo);

	CdStreamInitThread();

	ASSERT( pBuffer != nil );
	RwFreeAlign(pBuffer);
}

uint32
GetGTA3ImgSize(void)
{
	ASSERT( gImgFiles[0] > 0 );
	struct stat statbuf;
	if (fstat(gImgFiles[0] - 1, &statbuf) == 0)
		return (uint32)statbuf.st_size;

	if (stat(gImgNames[0], &statbuf) == -1) {
		char* real = casepath(gImgNames[0], false);
		if (real) {
			int ok = stat(real, &statbuf);
			free(real);
			if (ok != -1)
				return (uint32)statbuf.st_size;
		}
		CDTRACE("can't get size of gta3.img");
		ASSERT(0);
		return 0;
	}
	return (uint32)statbuf.st_size;
}

void
CdStreamShutdown(void)
{
    // Semaphores are destroyed by the streaming thread(s); the read info array
    // is only released here, once every thread has been joined.
#ifndef ONE_THREAD_PER_CHANNEL
	if (gCdStreamSema == nil || gpReadInfo == nil)
		return;
	gCdStreamThreadStatus = 2;
	sem_post(gCdStreamSema);
	pthread_join(_gCdStreamThread, nil);
	gCdStreamThreadStatus = 0;
#else
	if (gpReadInfo == nil)
		return;
	for ( int32 i = 0; i < gNumChannels; i++ ) {
		gpReadInfo[i].nThreadStatus = 2;
		sem_post(gpReadInfo[i].pStartSemaphore);
		pthread_join(gpReadInfo[i].pChannelThread, nil);
	}
#endif
	free(gpReadInfo);
	gpReadInfo = nil;
	gNumChannels = 0;
}


int32
CdStreamRead(int32 channel, void *buffer, uint32 offset, uint32 size)
{
	// None of these are assertions any more: ASSERT() compiles to nothing in
	// release builds, so an out-of-range channel used to index a null pointer.
	if ( gpReadInfo == nil || channel < 0 || channel >= gNumChannels || buffer == nil ) {
		REVC_LOG_ERROR("CdStreamRead: invalid channel %d (num %d)", channel, gNumChannels);
		return STREAM_ERROR;
	}

	lastPosnRead = size + offset;

	ASSERT( _GET_INDEX(offset) < MAX_CDIMAGES );
	int32 hImage = gImgFiles[_GET_INDEX(offset)];
	ASSERT( hImage > 0 );

	CdReadInfo *pChannel = &gpReadInfo[channel];
	ASSERT( pChannel != nil );

	if ( pChannel->nSectorsToRead != 0 || pChannel->bReading ) {
		// Only a request for exactly the same file/offset/buffer can be
		// coalesced. Comparing just the sector count reported STREAM_SUCCESS for
		// a *different* destination buffer, which the streaming thread was not
		// filling - the caller then decoded uninitialised memory.
		if (pChannel->hFile == hImage - 1 &&
		    pChannel->nSectorOffset == _GET_OFFSET(offset) &&
		    pChannel->pBuffer == buffer &&
		    pChannel->nSectorsToRead >= size)
			return STREAM_SUCCESS;
#ifdef FLUSHABLE_STREAMING
		flushStream[channel] = 1;
		CdStreamSync(channel);
#else
		return STREAM_NONE;
#endif
	}

	pChannel->hFile = hImage - 1;
	pChannel->nStatus = STREAM_NONE;
	pChannel->nSectorOffset = _GET_OFFSET(offset);
	pChannel->nSectorsToRead = size;
	pChannel->pBuffer = buffer;
	pChannel->bLocked = 0;

#ifndef ONE_THREAD_PER_CHANNEL
	pthread_mutex_lock(&gChannelRequestQMutex);
	AddToQueue(&gChannelRequestQ, channel);
	pthread_mutex_unlock(&gChannelRequestQMutex);
    if ( sem_post(gCdStreamSema) != 0 )
		printf("Signal Sema Error\n");
#else
	if ( sem_post(pChannel->pStartSemaphore) != 0 )
		printf("Signal Sema Error\n");
#endif

	return STREAM_SUCCESS;
}

int32
CdStreamGetStatus(int32 channel)
{
	if ( gpReadInfo == nil || channel < 0 || channel >= gNumChannels )
		return STREAM_NONE;
	CdReadInfo *pChannel = &gpReadInfo[channel];

#ifdef ONE_THREAD_PER_CHANNEL
	if (pChannel->nThreadStatus == 2)
		return STREAM_NONE;
#else
	if (gCdStreamThreadStatus == 2)
		return STREAM_NONE;
#endif

	if ( pChannel->bReading )
		return STREAM_READING;

	if ( pChannel->nSectorsToRead != 0 )
		return STREAM_WAITING;

	if ( pChannel->nStatus != STREAM_NONE )
	{
		int32 status = pChannel->nStatus;
		pChannel->nStatus = STREAM_NONE;

		return status;
	}

	return STREAM_NONE;
}

int32
CdStreamGetLastPosn(void)
{
	return lastPosnRead;
}

// wait for channel to finish reading
int32
CdStreamSync(int32 channel)
{
	if ( gpReadInfo == nil || channel < 0 || channel >= gNumChannels )
		return STREAM_NONE;
	CdReadInfo *pChannel = &gpReadInfo[channel];

#ifdef FLUSHABLE_STREAMING
	if (flushStream[channel]) {
		pChannel->nSectorsToRead = 0;
#ifdef ONE_THREAD_PER_CHANNEL
		pthread_kill(pChannel->pChannelThread, SIGUSR1);
		if (pChannel->bReading) {
			pChannel->bLocked = true;
#else
		if (pChannel->bReading) {
			pChannel->bLocked = true;
			pthread_kill(_gCdStreamThread, SIGUSR1);
#endif
			while (pChannel->bLocked)
				sem_wait(pChannel->pDoneSemaphore);
		}
		pChannel->bReading = false;
		flushStream[channel] = false;
		return STREAM_NONE;
	}
#endif

	if ( pChannel->nSectorsToRead != 0 || pChannel->bReading )
	{
		pChannel->bLocked = true;
		// `nSectorsToRead` alone is not enough: the streaming thread clears it
		// before it has finished writing the buffer, so the caller could start
		// decoding while the read was still in flight. Wait for bReading too.
		while (pChannel->bLocked && (pChannel->nSectorsToRead != 0 || pChannel->bReading)){
			sem_wait(pChannel->pDoneSemaphore);
		}
		pChannel->bLocked = false;
	}

	pChannel->bReading = false;

	return pChannel->nStatus;
}

// All three helpers are called with gChannelRequestQMutex held.
void
AddToQueue(Queue *queue, int32 item)
{
	if ( queue == nil || queue->items == nil )
		return;
	if ( gChannelRequestQCount >= queue->size - 1 ) {
		// The queue used to overwrite its oldest entry and report the full
		// state only through the debug log: the read request then never
		// completed and the game stalled waiting for it.
		debug("CdStream request queue is full\n");
		return;
	}
	queue->items[queue->tail] = item;
	queue->tail = (queue->tail + 1) % queue->size;
	gChannelRequestQCount++;
}

int32
GetFirstInQueue(Queue *queue)
{
	if ( queue == nil || queue->items == nil || gChannelRequestQCount == 0 )
		return -1;
	return queue->items[queue->head];
}

void
RemoveFirstInQueue(Queue *queue)
{
	if ( queue == nil || queue->items == nil || gChannelRequestQCount == 0 )
		return;
	queue->head = (queue->head + 1) % queue->size;
	gChannelRequestQCount--;
}

void *CdStreamThread(void *param)
{
	debug("Created cdstream thread\n");

#ifndef ONE_THREAD_PER_CHANNEL
	while (gCdStreamThreadStatus != 2) {
		if (sem_wait(gCdStreamSema) != 0) {
			if (errno == EINTR)
				continue;
			break;
		}

		pthread_mutex_lock(&gChannelRequestQMutex);
		int32 channel = GetFirstInQueue(&gChannelRequestQ);
		if (channel != -1)
			RemoveFirstInQueue(&gChannelRequestQ);
		pthread_mutex_unlock(&gChannelRequestQMutex);

		// spurious wakeup
		if (channel == -1)
			continue;
#else
	int channel = *((int*)param);
	free(param);
	while (gpReadInfo[channel].nThreadStatus != 2){
		sem_wait(gpReadInfo[channel].pStartSemaphore);
#endif

		CdReadInfo *pChannel = &gpReadInfo[channel];
		if (pChannel->nSectorsToRead == 0)
			continue;

		pChannel->bReading = true;

		// Not standard POSIX :shrug:
#ifdef __linux__
#ifdef ONE_THREAD_PER_CHANNEL
		if (gpReadInfo[channel].nThreadStatus == 0){
			gpReadInfo[channel].nThreadStatus = 1;
#else
		if (gCdStreamThreadStatus == 0){
			gCdStreamThreadStatus = 1;
#endif
			pid_t tid = syscall(SYS_gettid);
			int ret = setpriority(PRIO_PROCESS, tid, getpriority(PRIO_PROCESS, getpid()) + 1);
		}
#endif
		if ( pChannel->nStatus == STREAM_NONE )
		{
			if (pChannel->hFile < 0 || pChannel->pBuffer == nil) {
				pChannel->nStatus = STREAM_ERROR;
			} else {
				off_t wanted = (off_t)pChannel->nSectorsToRead * CDSTREAM_SECTOR_SIZE;
				off_t position = (off_t)pChannel->nSectorOffset * CDSTREAM_SECTOR_SIZE;
				if (lseek(pChannel->hFile, position, SEEK_SET) != position) {
					pChannel->nStatus = pChannel->nSectorsToRead == 0 ? STREAM_WAITING : STREAM_ERROR;
				} else {
					// A single read() may return less than asked for (large
					// request, low memory, EINTR). The old code accepted that as
					// success and the game decoded a half-filled buffer.
					uint8 *destination = (uint8*)pChannel->pBuffer;
					off_t remaining = wanted;
					bool failed = false;
					while (remaining > 0) {
						ssize_t got = read(pChannel->hFile, destination, (size_t)remaining);
						if (got < 0) {
							if (errno == EINTR) {
								if (pChannel->nSectorsToRead == 0) { failed = false; break; }
								continue;
							}
							failed = true;
							break;
						}
						if (got == 0) { // unexpected EOF
							failed = true;
							break;
						}
						destination += got;
						remaining -= got;
					}
					if (failed || remaining != 0)
						// nSectorsToRead == 0 means we wanted to flush channel;
						// STREAM_WAITING is a little hack to make CStreaming not process this data
						pChannel->nStatus = pChannel->nSectorsToRead == 0 ? STREAM_WAITING : STREAM_ERROR;
					else
						pChannel->nStatus = STREAM_NONE;
				}
			}
		}

		// Clear the request before waking the waiter, otherwise CdStreamSync()
		// can spin on a request that is already finished.
		pChannel->nSectorsToRead = 0;
		pChannel->bReading = false;
		if ( pChannel->bLocked )
		{
			pChannel->bLocked = 0;
			sem_post(pChannel->pDoneSemaphore);
		}
	}

#ifndef ONE_THREAD_PER_CHANNEL
	for ( int32 i = 0; i < gNumChannels; i++ )
	{
		RE3_SEM_CLOSE(gpReadInfo[i].pDoneSemaphore, "/semaphore_done%d", i);
	}
	RE3_SEM_CLOSE(gCdStreamSema, "/semaphore_cd_stream");
	gCdStreamSema = nil;
	free(gChannelRequestQ.items);
	gChannelRequestQ.items = nil;
#else
	RE3_SEM_CLOSE(gpReadInfo[channel].pStartSemaphore, "/semaphore_start%d", channel);
	gpReadInfo[channel].pStartSemaphore = nil;

	RE3_SEM_CLOSE(gpReadInfo[channel].pDoneSemaphore, "/semaphore_done%d", channel);
	gpReadInfo[channel].pDoneSemaphore = nil;
#endif
	// gpReadInfo is deliberately NOT freed here: CdStreamShutdown() releases it
	// after pthread_join(), so the other channels (one thread per channel) and
	// the game thread can never see a dangling array. The old free() here was
	// the double-free/use-after-free seen right after a level restart.
	pthread_exit(nil);
	return nil;
}

bool
CdStreamAddImage(char const *path)
{
	ASSERT(path != nil);
	if(gNumImages >= MAX_CDIMAGES || strlen(path) >= sizeof(gCdImageNames[0])){
		REVC_LOG_ERROR("Cannot mount IMG %s: too many images or path too long", path);
		return false;
	}

	gImgFiles[gNumImages] = open(path, _gdwCdStreamFlags);

	// Fix case sensitivity and backslashes.
	if(gImgFiles[gNumImages] == -1){
		char *real = casepath(path, false);
		if(real){
			gImgFiles[gNumImages] = open(real, _gdwCdStreamFlags);
			free(real);
		}
	}

	if(gImgFiles[gNumImages] == -1){
		int error = errno;
		REVC_LOG_ERROR("Cannot mount IMG %s (errno=%d): %s", path, error, strerror(error));
		return false;
	}

	struct stat st;
	if(fstat(gImgFiles[gNumImages], &st) == 0)
		REVC_LOG_INFO("Mounted IMG %d: %s (%llu bytes)", gNumImages, path,
			(unsigned long long)st.st_size);
	else
		REVC_LOG_WARNING("Mounted IMG %d: %s (size unavailable)", gNumImages, path);
	gImgNames[gNumImages] = strdup(path);
	gImgFiles[gNumImages]++; // because -1: error 0: not used
	strcpy(gCdImageNames[gNumImages], path);
	gNumImages++;
	return true;
}

char *
CdStreamGetImageName(int32 cd)
{
	ASSERT(cd < MAX_CDIMAGES);
	if ( gImgFiles[cd] > 0)
		return gCdImageNames[cd];

	return nil;
}

void
CdStreamRemoveImages(void)
{
	for ( int32 i = 0; i < gNumChannels; i++ ) {
#ifdef FLUSHABLE_STREAMING
		flushStream[i] = 1;
#endif
		CdStreamSync(i);
	}

	for ( int32 i = 0; i < gNumImages; i++ )
	{
		close(gImgFiles[i] - 1);
		free(gImgNames[i]);
		gImgFiles[i] = 0;
	}

	gNumImages = 0;
}

int32
CdStreamGetNumImages(void)
{
	return gNumImages;
}
#endif
#endif
