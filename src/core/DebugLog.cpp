#define _CRT_SECURE_NO_WARNINGS
#include "DebugLog.h"

#include <chrono>
#include <errno.h>
#include <mutex>
#include <stdio.h>
#include <string.h>
#include <time.h>

namespace DebugLog
{
	static FILE *logFile;
	static std::mutex logMutex;

	bool
	Init(const char *path)
	{
		std::lock_guard<std::mutex> lock(logMutex);
		if(logFile != NULL)
			return true;

		logFile = fopen(path, "w");
		if(logFile == NULL){
			// Never call the game logger here: opening the log must not recurse.
			fprintf(stderr, "reVC: cannot open debug log '%s': %s\n", path, strerror(errno));
			return false;
		}
		return true;
	}

	void
	Shutdown()
	{
		std::lock_guard<std::mutex> lock(logMutex);
		if(logFile != NULL){
			fclose(logFile);
			logFile = NULL;
		}
	}

	void
	WriteMessage(const char *level, const char *file, unsigned int line,
		const char *function, const char *message)
	{
		std::lock_guard<std::mutex> lock(logMutex);
		if(logFile == NULL)
			return;

		const std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
		const time_t seconds = std::chrono::system_clock::to_time_t(now);
		const int millis = (int)(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000);
		struct tm localTime;
#ifdef _WIN32
		localtime_s(&localTime, &seconds);
#else
		localtime_r(&seconds, &localTime);
#endif
		char timestamp[32];
		strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &localTime);

		// Prefix each physical line, including lines from multi-line assertions.
		// A trailing newline in a message must not create an empty log entry.
		const char *text = message == NULL ? "" : message;
		for(;;){
			const char *end = text;
			while(*end != '\0' && *end != '\n' && *end != '\r')
				end++;

			fprintf(logFile, "[%s.%03d] [%s]", timestamp, millis, level);
			if(file != NULL && file[0] != '\0')
				fprintf(logFile, " [%s:%u %s]", file, line, function == NULL ? "" : function);
			fprintf(logFile, " %.*s\n", (int)(end - text), text);

			if(*end == '\0')
				break;
			text = end + 1;
			if(*end == '\r' && *text == '\n')
				text++;
			if(*text == '\0')
				break;
		}
		// Preserve the last messages even if the game crashes later in this run.
		fflush(logFile);
	}

	void
	WriteV(const char *level, const char *file, unsigned int line,
		const char *function, const char *format, va_list args)
	{
		char message[4096];
		int length = vsnprintf(message, sizeof(message), format, args);
		if(length < 0){
			strcpy(message, "[format error]");
		}else if(length >= (int)sizeof(message)){
			const char suffix[] = " ... [truncated]";
			memcpy(message + sizeof(message) - sizeof(suffix), suffix, sizeof(suffix));
		}
		WriteMessage(level, file, line, function, message);
	}

	void
	Write(const char *level, const char *file, unsigned int line,
		const char *function, const char *format, ...)
	{
		va_list args;
		va_start(args, format);
		WriteV(level, file, line, function, format, args);
		va_end(args);
	}
}
