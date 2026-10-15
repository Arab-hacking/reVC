#ifndef __GTA_DEBUGLOG_H__
#define __GTA_DEBUGLOG_H__

#include <stdarg.h>

// Independent of RenderWare and CFileMgr: the log is available during startup
// and keeps writing to the same file when the game changes its working directory.
namespace DebugLog
{
	bool Init(const char *path);
	void Shutdown();
	void WriteMessage(const char *level, const char *file, unsigned int line,
		const char *function, const char *message);
	void WriteV(const char *level, const char *file, unsigned int line,
		const char *function, const char *format, va_list args);
	void Write(const char *level, const char *file, unsigned int line,
		const char *function, const char *format, ...);
}

#endif // __GTA_DEBUGLOG_H__
