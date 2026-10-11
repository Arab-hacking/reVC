#include "common.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "DebugLog.h"

#ifdef CUSTOM_MODELS

#include <fcntl.h>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <signal.h>
#else
#include <unistd.h>
#include <signal.h>
#ifdef __GLIBC__
#include <execinfo.h>
#endif
#endif

#define DEBUGLOG_NAME	"debug.log"
#define DEBUGLOG_MAX	(16*1024*1024)

static int debugLogFd = -1;

// opened without a FILE*, so the crash handlers can still write into it
static int
DebugLogOpen(void)
{
	if(debugLogFd >= 0)
		return debugLogFd;

	// keep the file from growing without bounds over many sessions
	FILE *probe = fopen(DEBUGLOG_NAME, "rb");
	if(probe){
		fseek(probe, 0, SEEK_END);
		if(ftell(probe) > DEBUGLOG_MAX)
			debugLogFd = open(DEBUGLOG_NAME, O_WRONLY | O_CREAT | O_TRUNC, 0644);
		fclose(probe);
	}
	if(debugLogFd < 0)
		debugLogFd = open(DEBUGLOG_NAME, O_WRONLY | O_CREAT | O_APPEND, 0644);
	return debugLogFd;
}

static bool debugLogInited = false;

void
DebugLogEnsure(void)
{
	// the full init (banner + crash handlers) - idempotent; called from the
	// first log line, so the traps are on no matter which entry point ran
	DebugLogInit();
}

const char *
DebugLogPath(void)
{
	return DEBUGLOG_NAME;
}

void
DebugLogPrintf(const char *fmt, ...)
{
	char buf[2048];
	DebugLogEnsure();
	int fd = debugLogFd;
	if(fd < 0)
		return;

	va_list ap;
	va_start(ap, fmt);
	int n = vsnprintf(buf, sizeof(buf)-2, fmt, ap);
	va_end(ap);
	if(n < 0)
		return;
	if(n > (int)sizeof(buf)-2)
		n = sizeof(buf)-2;
	if(n > 0 && buf[n-1] != '\n')
		buf[n++] = '\n';
	if(write(fd, buf, n) < 0)
		{}	// nothing sensible to do about a full disk here
}

// ---------------------------------------------------------------------------
// crash reporting
// ---------------------------------------------------------------------------

// safe to call from a signal/exception context: no malloc, no stdio
static void
DebugLogCrashLine(const char *line)
{
	int fd = debugLogFd >= 0 ? debugLogFd : open(DEBUGLOG_NAME, O_WRONLY | O_CREAT | O_APPEND, 0644);
	if(fd >= 0 && line){
		write(fd, line, strlen(line));
		write(fd, "\n", 1);
	}
}

#ifdef _WIN32

// A Windows access violation is a structured exception. The CRT SIGSEGV
// handler below loses the original CONTEXT, so catch first-chance AVs here and
// record the faulting instruction before the CRT translates them to signals.
static LONG CALLBACK
DebugLogVectoredException(EXCEPTION_POINTERS *info)
{
	EXCEPTION_RECORD *record;
	CONTEXT *context;
	uintptr_t instruction, imageBase;
	void *stackPointer = nil;
	void *framePointer = nil;
	char line[256];

	if(info == nil || info->ExceptionRecord == nil || info->ContextRecord == nil)
		return EXCEPTION_CONTINUE_SEARCH;
	record = info->ExceptionRecord;
	if(record->ExceptionCode != EXCEPTION_ACCESS_VIOLATION &&
	   record->ExceptionCode != EXCEPTION_IN_PAGE_ERROR &&
	   record->ExceptionCode != EXCEPTION_STACK_OVERFLOW)
		return EXCEPTION_CONTINUE_SEARCH;

	context = info->ContextRecord;
	instruction = (uintptr_t)record->ExceptionAddress;
	imageBase = (uintptr_t)GetModuleHandleA(nil);
#if defined(_M_X64) || defined(__x86_64__)
	stackPointer = (void*)(uintptr_t)context->Rsp;
	framePointer = (void*)(uintptr_t)context->Rbp;
#elif defined(_M_IX86) || defined(__i386__)
	stackPointer = (void*)(uintptr_t)context->Esp;
	framePointer = (void*)(uintptr_t)context->Ebp;
#endif

	snprintf(line, sizeof(line),
		"FIRST-CHANCE: exception 0x%08lX at %p; exe-base=%p exe-rva=0x%llX; SP=%p BP=%p",
		(unsigned long)record->ExceptionCode, record->ExceptionAddress,
		(void*)imageBase,
		(unsigned long long)(instruction >= imageBase ? instruction - imageBase : instruction),
		stackPointer, framePointer);
	DebugLogCrashLine(line);
	if(record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record->NumberParameters >= 2){
		const char *operation = record->ExceptionInformation[0] == 0 ? "read" :
			record->ExceptionInformation[0] == 1 ? "write" :
			record->ExceptionInformation[0] == 8 ? "execute" : "unknown";
		snprintf(line, sizeof(line), "FIRST-CHANCE: access=%s target=%p",
			operation, (void*)(uintptr_t)record->ExceptionInformation[1]);
		DebugLogCrashLine(line);
	}
	return EXCEPTION_CONTINUE_SEARCH;
}

static void
DebugLogSignalHandler(int sig)
{
	const char *what =
		sig == SIGSEGV ? "SIGSEGV (invalid memory access)" :
		sig == SIGABRT ? "SIGABRT (abort - failed assert or out of memory)" :
		sig == SIGFPE  ? "SIGFPE (arithmetic error)" :
		sig == SIGILL  ? "SIGILL (illegal instruction)" :
		"fatal signal";
	char line[128];
	snprintf(line, sizeof(line), "CRASH: %s - see the lines above for what was loaded last", what);
	DebugLogCrashLine(line);
	DebugLogCrashLine("---- session ended abnormally ----");
	signal(sig, SIG_DFL);
	raise(sig);
}

static LONG WINAPI
DebugLogUnhandledException(EXCEPTION_POINTERS *info)
{
	char line[128];
	snprintf(line, sizeof(line), "CRASH: exception 0x%08lX at %p - see the lines above for what was loaded last",
		(unsigned long)info->ExceptionRecord->ExceptionCode,
		info->ExceptionRecord->ExceptionAddress);
	DebugLogCrashLine(line);
	DebugLogCrashLine("---- session ended abnormally ----");
	return EXCEPTION_CONTINUE_SEARCH;
}

#else

static void
DebugLogSignalHandler(int sig)
{
	const char *what =
		sig == SIGSEGV ? "SIGSEGV (invalid memory access)" :
		sig == SIGABRT ? "SIGABRT (abort - failed assert or out of memory)" :
		sig == SIGFPE  ? "SIGFPE (arithmetic error)" :
		sig == SIGILL  ? "SIGILL (illegal instruction)" :
		sig == SIGBUS  ? "SIGBUS (bus error)" :
		"fatal signal";
	char line[128];
	snprintf(line, sizeof(line), "CRASH: %s - see the lines above for what was loaded last", what);
	DebugLogCrashLine(line);
#ifdef __GLIBC__
	void *bt[32];
	int n = backtrace(bt, 32);
	if(n > 0){
		DebugLogCrashLine("backtrace:");
		backtrace_symbols_fd(bt, n, debugLogFd >= 0 ? debugLogFd : -1);
	}
#endif
	DebugLogCrashLine("---- session ended abnormally ----");
	// let the default handling produce a core dump / normal termination
	signal(sig, SIG_DFL);
	raise(sig);
}

#endif // _WIN32

static void
DebugLogExitMarker(void)
{
	DebugLogCrashLine("---- session ended ----");
}

void
DebugLogInit(void)
{
	if(debugLogInited)
		return;
	debugLogInited = true;

	int fd = DebugLogOpen();
	if(fd < 0)
		return;

	time_t now = time(nil);
	char banner[128];
	snprintf(banner, sizeof(banner), "==== reVC debug log, started %s", ctime(&now));
	// ctime adds the newline already
	DebugLogCrashLine(banner);

#ifdef _WIN32
	AddVectoredExceptionHandler(1, DebugLogVectoredException);
	SetUnhandledExceptionFilter(DebugLogUnhandledException);
	// assert() and abort() go through the CRT, not through the unhandled
	// exception filter - catch them with the signal handlers as well
	signal(SIGABRT, DebugLogSignalHandler);
	signal(SIGSEGV, DebugLogSignalHandler);
	signal(SIGILL, DebugLogSignalHandler);
	signal(SIGFPE, DebugLogSignalHandler);
#else
	signal(SIGSEGV, DebugLogSignalHandler);
	signal(SIGABRT, DebugLogSignalHandler);
	signal(SIGFPE, DebugLogSignalHandler);
	signal(SIGILL, DebugLogSignalHandler);
	signal(SIGBUS, DebugLogSignalHandler);
#endif
	atexit(DebugLogExitMarker);
}

#else // CUSTOM_MODELS

void DebugLogInit(void) {}
void DebugLogEnsure(void) {}
void DebugLogPrintf(const char*, ...) {}
const char *DebugLogPath(void) { return ""; }

#endif // CUSTOM_MODELS
