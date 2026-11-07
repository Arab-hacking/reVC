#ifndef __GTA_CUSTOM_DEBUGLOG_H__
#define __GTA_CUSTOM_DEBUGLOG_H__

// Custom-folder diagnostics and crash handlers. Normal messages go through
// the core DebugLog sink, so startup, streaming and mod logs share one file.
void DebugLogInit(void);
void DebugLogEnsure(void);
void DebugLogPrintf(const char *fmt, ...);
const char *DebugLogPath(void);

#endif // __GTA_CUSTOM_DEBUGLOG_H__
