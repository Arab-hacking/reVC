#ifndef __GTA_DEBUGLOG_H__
#define __GTA_DEBUGLOG_H__

// The always-on debug log ("debug.log" in the working directory of the game).
//
// Everything the custom folder does - which archive served which model,
// texture, collision, animation or skin, and why something was NOT taken
// from the archives - is written here, one flushed line at a time, so a
// broken download or a wrong file name can be diagnosed after the fact.
// The file is also the crash report: handlers for abnormal termination
// write what happened (signal/exception code) as the last line, and the
// lines above it show what the game was loading when it went down.
//
// The log is truncated at 16 MB when a new session starts, so it can be
// sent away whole without growing forever.

void DebugLogInit(void);	// session banner + crash handlers, call once at start up
void DebugLogEnsure(void);	// makes sure the file is open (idempotent)
void DebugLogPrintf(const char *fmt, ...);
const char *DebugLogPath(void);

#endif // __GTA_DEBUGLOG_H__
