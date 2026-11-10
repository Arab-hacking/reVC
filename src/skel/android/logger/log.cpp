//
// Created by mrxenginner on 11/05/2025.
//

#if defined ANDROID

#include "common.h"
#include "log.h"
#include "AndroidMain.h"

#include <android/log.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

extern char* StorageRootBuffer;

namespace {
    // The regular log may be written from many threads (streaming, audio,
    // Java callbacks). A shared static buffer used to interleave partial lines
    // and even crash in vsnprintf when both ran at the same time.
    pthread_mutex_t gLogMutex = PTHREAD_MUTEX_INITIALIZER;
    FILE* gLogFile = nullptr;
    bool gLogFileTried = false;

    // Crash paths cannot take a mutex, so they write into their own buffer and
    // their own file handle. Both are only touched by fatal paths.
    FILE* gCrashFile = nullptr;
    bool gCrashFileTried = false;

    char gCrashBuffer[512];

    void OpenFileIfNeeded() {
        if (gLogFileTried) return;
        gLogFileTried = true;
#ifdef USE_FILE_LOG
        if (StorageRootBuffer == nullptr) return;
        char path[512];
        snprintf(path, sizeof(path), "%s/log.txt", StorageRootBuffer);
        FILE* file = fopen(path, "wb"); // fresh log per run, as documented
        if (file == nullptr) return;
        setvbuf(file, nullptr, _IOLBF, 0);
        gLogFile = file;
#endif
    }

    void OpenCrashFileIfNeeded() {
        if (gCrashFileTried) return;
        gCrashFileTried = true;
#ifdef USE_FILE_LOG
        if (StorageRootBuffer == nullptr) return;
        char path[512];
        int length = snprintf(path, sizeof(path), "%s/crash_log.txt", StorageRootBuffer);
        if (length <= 0 || length >= (int)sizeof(path)) return;
        gCrashFile = fopen(path, "ab");
#endif
    }
}

void Logger::Init()
{
    pthread_mutex_lock(&gLogMutex);
    OpenFileIfNeeded();
    pthread_mutex_unlock(&gLogMutex);
}

void Logger::Shutdown()
{
    pthread_mutex_lock(&gLogMutex);
    if (gLogFile != nullptr) {
        fflush(gLogFile);
        fclose(gLogFile);
        gLogFile = nullptr;
    }
    gLogFileTried = false;
    pthread_mutex_unlock(&gLogMutex);

    if (gCrashFile != nullptr) {
        fclose(gCrashFile);
        gCrashFile = nullptr;
    }
}

void Logger::Log(const char *fmt, ...)
{
    char buffer[512];

    va_list arg;
    va_start(arg, fmt);
    int length = vsnprintf(buffer, sizeof(buffer), fmt, arg);
    va_end(arg);
    if (length < 0) return;

    __android_log_write(ANDROID_LOG_INFO, "reVC", buffer);

    pthread_mutex_lock(&gLogMutex);
    OpenFileIfNeeded();
    if (gLogFile != nullptr) {
        fputs(buffer, gLogFile);
        fputc('\n', gLogFile);
        fflush(gLogFile);
    }
    pthread_mutex_unlock(&gLogMutex);
}

void Logger::CrashLog(const char* fmt, ...)
{
    // Deliberately lock-free: a crash can happen while the mutex above is held
    // by the crashing thread, and waiting there would turn a crash log into a
    // deadlock. Fatal paths only run once, so no serialisation is needed.
    va_list arg;
    va_start(arg, fmt);
    vsnprintf(gCrashBuffer, sizeof(gCrashBuffer), fmt, arg);
    va_end(arg);

    __android_log_write(ANDROID_LOG_FATAL, "reVC", gCrashBuffer);

    OpenCrashFileIfNeeded();
    if (gCrashFile != nullptr) {
        fputs(gCrashBuffer, gCrashFile);
        fputc('\n', gCrashFile);
        fflush(gCrashFile);
    }
}
#endif
