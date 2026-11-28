#pragma once
#include <stdarg.h>

enum android_LogPriority {
	ANDROID_LOG_UNKNOWN = 0,
	ANDROID_LOG_DEFAULT,
	ANDROID_LOG_VERBOSE,
	ANDROID_LOG_DEBUG,
	ANDROID_LOG_INFO,
	ANDROID_LOG_WARN,
	ANDROID_LOG_ERROR,
	ANDROID_LOG_FATAL
};

static inline int __android_log_print(int, const char *, const char *, ...) { return 0; }
static inline int __android_log_vprint(int, const char *, const char *, va_list) { return 0; }
static inline int __android_log_write(int, const char *, const char *) { return 0; }
