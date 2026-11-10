#include "common.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "DebugLog.h"
#include "../../core/DebugLog.h"

#ifdef CUSTOM_MODELS

#ifdef _WIN32
#include <windows.h>
#include <signal.h>
#else
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#ifdef __linux__
#include <sys/syscall.h>
#include <ucontext.h>
#endif
#ifdef __GLIBC__
#include <execinfo.h>
#elif defined(__ANDROID__)
#include <unwind.h>
#include <dlfcn.h>
#endif
#endif

#define DEBUGLOG_NAME "debug.log"

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
	DebugLogEnsure();
	va_list ap;
	va_start(ap, fmt);
	DebugLog::WriteV("CUSTOM", nil, 0, nil, fmt, ap);
	va_end(ap);
}

// ---------------------------------------------------------------------------
// crash reporting
// ---------------------------------------------------------------------------

// safe to call from a signal/exception context: no malloc, no stdio
static void
DebugLogCrashLine(const char *line)
{
	DebugLog::WriteCrashLine(line);
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
	// Windows CRT signals: keep this to async-signal-safe work only, so no
	// snprintf allocation and no variable-length formatting inside the handler.
	const char *what =
		sig == SIGSEGV ? "CRASH: SIGSEGV (invalid memory access)\n" :
		sig == SIGABRT ? "CRASH: SIGABRT (abort - failed assert or out of memory)\n" :
		sig == SIGFPE  ? "CRASH: SIGFPE (arithmetic error)\n" :
		sig == SIGILL  ? "CRASH: SIGILL (illegal instruction)\n" :
		"CRASH: fatal signal\n";
	DebugLogCrashLine(what);
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

#ifdef __linux__
static bool
DebugLogParseHexRange(const char *line, uintptr_t &lo, uintptr_t &hi, const char *&pathOut)
{
	lo = 0;
	hi = 0;
	pathOut = "";
	const char *p = line;
	while((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F')){
		uintptr_t d = (*p <= '9') ? (uintptr_t)(*p - '0') : (uintptr_t)((*p | 0x20) - 'a' + 10);
		lo = (lo << 4) | d;
		p++;
	}
	if(*p != '-') return false;
	p++;
	while((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F')){
		uintptr_t d = (*p <= '9') ? (uintptr_t)(*p - '0') : (uintptr_t)((*p | 0x20) - 'a' + 10);
		hi = (hi << 4) | d;
		p++;
	}
	// Jump to 6th column (pathname) in /proc/self/maps: perms offset dev inode pathname
	int spaces = 0;
	while(*p && *p != '\n'){
		if(*p == ' '){
			while(*p == ' ') p++;
			spaces++;
			if(spaces >= 5){
				pathOut = p;
				break;
			}
		}else{
			p++;
		}
	}
	return hi > lo;
}

static bool
DebugLogLookupProcMap(uintptr_t addr, uintptr_t &baseOut, char *nameBuf, size_t nameCap)
{
	baseOut = 0;
	if(nameCap > 0) nameBuf[0] = '\0';
	if(addr == 0) return false;
	int fd = open("/proc/self/maps", O_RDONLY);
	if(fd < 0) return false;
	char buf[2048];
	char line[512];
	size_t lineLen = 0;
	bool found = false;
	ssize_t n;
	while(!found && (n = read(fd, buf, sizeof(buf))) > 0){
		for(ssize_t i = 0; i < n; i++){
			char c = buf[i];
			if(c == '\n'){
				line[lineLen] = '\0';
				uintptr_t lo = 0, hi = 0;
				const char *path = "";
				if(DebugLogParseHexRange(line, lo, hi, path) && addr >= lo && addr < hi){
					baseOut = lo;
					const char *leaf = path;
					for(const char *s = path; *s && *s != '\n'; s++)
						if(*s == '/') leaf = s + 1;
					size_t k = 0;
					while(leaf[k] && leaf[k] != '\n' && k + 1 < nameCap){
						nameBuf[k] = leaf[k];
						k++;
					}
					if(nameCap > 0) nameBuf[k] = '\0';
					found = true;
					break;
				}
				lineLen = 0;
			}else if(lineLen + 1 < sizeof(line)){
				line[lineLen++] = c;
			}
		}
	}
	close(fd);
	return found;
}
#endif

static void
DebugLogSignalAction(int sig, siginfo_t *info, void *ucontextRaw)
{
	static volatile sig_atomic_t s_inCrashHandler = 0;
	if (s_inCrashHandler) {
		signal(sig, SIG_DFL);
		raise(sig);
		return;
	}
	s_inCrashHandler = 1;

	const char *what =
		sig == SIGSEGV ? "SIGSEGV (invalid memory access)" :
		sig == SIGABRT ? "SIGABRT (abort - failed assert or out of memory)" :
		sig == SIGFPE  ? "SIGFPE (arithmetic error)" :
		sig == SIGILL  ? "SIGILL (illegal instruction)" :
		sig == SIGBUS  ? "SIGBUS (bus error)" :
		"fatal signal";
	char line[256];
	snprintf(line, sizeof(line), "CRASH: %s - see the lines above for what was loaded last", what);
	DebugLogCrashLine(line);

#ifdef __linux__
	long pid = (long)getpid();
	long tid = (long)syscall(SYS_gettid);
	uintptr_t faultAddr = info ? (uintptr_t)info->si_addr : 0;
	int siCode = info ? info->si_code : 0;
	uintptr_t regPc = 0, regLr = 0, regSp = 0, regFp = 0;
	if(ucontextRaw){
		ucontext_t *uc = (ucontext_t*)ucontextRaw;
#if defined(__aarch64__)
		regPc = (uintptr_t)uc->uc_mcontext.pc;
		regSp = (uintptr_t)uc->uc_mcontext.sp;
		regFp = (uintptr_t)uc->uc_mcontext.regs[29];
		regLr = (uintptr_t)uc->uc_mcontext.regs[30];
#elif defined(__arm__)
		regPc = (uintptr_t)uc->uc_mcontext.arm_pc;
		regSp = (uintptr_t)uc->uc_mcontext.arm_sp;
		regFp = (uintptr_t)uc->uc_mcontext.arm_fp;
		regLr = (uintptr_t)uc->uc_mcontext.arm_lr;
#elif defined(__x86_64__)
		regPc = (uintptr_t)uc->uc_mcontext.gregs[REG_RIP];
		regSp = (uintptr_t)uc->uc_mcontext.gregs[REG_RSP];
		regFp = (uintptr_t)uc->uc_mcontext.gregs[REG_RBP];
#endif
	}
	snprintf(line, sizeof(line),
		"CRASH-CTX: pid=%ld tid=%ld code=%d fault_addr=%p pc=%p lr=%p sp=%p fp=%p",
		pid, tid, siCode, (void*)faultAddr, (void*)regPc, (void*)regLr, (void*)regSp, (void*)regFp);
	DebugLogCrashLine(line);
	if(regPc){
		uintptr_t modBase = 0;
		char modName[96];
		if(DebugLogLookupProcMap(regPc, modBase, modName, sizeof(modName))){
			snprintf(line, sizeof(line), "CRASH-PC: %s+0x%llx (%p)",
				modName[0] ? modName : "[anon]",
				(unsigned long long)(regPc - modBase), (void*)regPc);
			DebugLogCrashLine(line);
		}
	}
	if(regLr){
		uintptr_t modBase = 0;
		char modName[96];
		if(DebugLogLookupProcMap(regLr, modBase, modName, sizeof(modName))){
			snprintf(line, sizeof(line), "CRASH-LR: %s+0x%llx (%p)",
				modName[0] ? modName : "[anon]",
				(unsigned long long)(regLr - modBase), (void*)regLr);
			DebugLogCrashLine(line);
		}
	}
#else
	(void)info;
	(void)ucontextRaw;
#endif

#ifdef __GLIBC__
	void *bt[32];
	int n = backtrace(bt, 32);
	if(n > 0){
		DebugLogCrashLine("backtrace:");
		if(DebugLog::CrashFd() >= 0)
			backtrace_symbols_fd(bt, n, DebugLog::CrashFd());
	}
#elif defined(__ANDROID__)
	extern uintptr_t g_libREVC;
	struct AndroidBtState {
		int depth;
	};
	struct AndroidBtHelper {
		static void formatAddr(int idx, uintptr_t pc) {
			char frameLine[200];
			uintptr_t modBase = 0;
			char modName[80];
			if (g_libREVC != 0 && pc >= g_libREVC && pc < g_libREVC + 0x08000000ULL) {
				snprintf(frameLine, sizeof(frameLine), "  #%02d librevc.so+0x%llx (%p)",
					idx, (unsigned long long)(pc - g_libREVC), (void *)pc);
			} else if (DebugLogLookupProcMap(pc, modBase, modName, sizeof(modName))) {
				snprintf(frameLine, sizeof(frameLine), "  #%02d %s+0x%llx (%p)",
					idx, modName[0] ? modName : "[anon]",
					(unsigned long long)(pc - modBase), (void *)pc);
			} else {
				snprintf(frameLine, sizeof(frameLine), "  #%02d pc %p", idx, (void *)pc);
			}
			DebugLogCrashLine(frameLine);
		}
		static _Unwind_Reason_Code cb(struct _Unwind_Context *ctx, void *arg) {
			AndroidBtState *st = (AndroidBtState *)arg;
			if (st->depth >= 16)
				return _URC_END_OF_STACK;
			uintptr_t pc = _Unwind_GetIP(ctx);
			if (pc) {
				formatAddr(st->depth, pc);
				st->depth++;
			}
			return _URC_NO_REASON;
		}
	};
	DebugLogCrashLine("backtrace:");
	AndroidBtState st = { 0 };
	_Unwind_Backtrace(AndroidBtHelper::cb, &st);
#if defined(__aarch64__)
	// The FP-chain and stack scans that used to run here dereferenced raw stack
	// words without checking that they are mapped. After a stack overflow (the
	// most common crash in this engine) the scanner itself faulted, and that
	// second SIGSEGV replaced the real report. _Unwind_Backtrace above already
	// walks the frames safely, so only its output is kept.
	(void)regFp;
#endif
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

	DebugLog::Write("INFO", nil, 0, nil, "Custom-folder diagnostics and crash handlers enabled");

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
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = DebugLogSignalAction;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
	sigaction(SIGSEGV, &sa, nil);
	sigaction(SIGABRT, &sa, nil);
	sigaction(SIGFPE, &sa, nil);
	sigaction(SIGILL, &sa, nil);
	sigaction(SIGBUS, &sa, nil);
#endif
	atexit(DebugLogExitMarker);
}

#else // CUSTOM_MODELS

void DebugLogInit(void) {}
void DebugLogEnsure(void) {}
void DebugLogPrintf(const char*, ...) {}
const char *DebugLogPath(void) { return ""; }

#endif // CUSTOM_MODELS
