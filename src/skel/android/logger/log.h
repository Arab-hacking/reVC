//
// Created by mrxenginner on 11/05/2025.
//

#ifndef REVC_LOGGER_H
#define REVC_LOGGER_H

#if defined ANDROID

namespace Logger {
    // Prepares the log files inside the current storage root. Safe to call
    // more than once; must be called after the storage root is known.
    void Init();
    // Flushes and closes the log files. Called on shutdown only.
    void Shutdown();

    // Thread-safe, may allocate, may block.
    void Log(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
    // Called from signal handlers and other fatal paths: no locks, no
    // allocation, only async-signal-safe calls plus a bounded vsnprintf.
    void CrashLog(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
}

#endif

#endif
