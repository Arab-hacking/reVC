#include "common.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <exception>

#include "DebugLog.h"
#include "../../core/DebugLog.h"

#ifdef CUSTOM_MODELS

#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
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

// Resolve DbgHelp and the absolute output path before a crash (CWD changes
// during startup). Never link against a new import library or load it in SEH.
typedef BOOL (WINAPI *WriteDumpFn)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE,
    PMINIDUMP_EXCEPTION_INFORMATION, PMINIDUMP_USER_STREAM_INFORMATION,
    PMINIDUMP_CALLBACK_INFORMATION);
static WriteDumpFn writeDump;
static char dumpPath[MAX_PATH];

// ---- stack walking -------------------------------------------------------
//
// A raw exe RVA in the log is not actionable: the address only means something
// once it is matched against the PDB of the exact build that produced the log,
// and that PDB is usually not at hand when the log is being read. The handlers
// therefore resolve the crashing stack here, through the same DbgHelp that
// writes the dump, and print module!function+offset [file:line] for every
// frame. Everything is loaded through GetProcAddress in DebugLogInit: a crash
// handler must not depend on an import directory the linker may have pruned.
typedef BOOL (WINAPI *SymInitializeFn)(HANDLE, PCSTR, BOOL);
typedef DWORD (WINAPI *SymSetOptionsFn)(DWORD);
typedef BOOL (WINAPI *SymFromAddrFn)(HANDLE, DWORD64, PDWORD64, PSYMBOL_INFO);
typedef BOOL (WINAPI *SymGetLineFromAddr64Fn)(HANDLE, DWORD64, PDWORD, PIMAGEHLP_LINE64);
typedef BOOL (WINAPI *SymGetModuleInfo64Fn)(HANDLE, DWORD64, PIMAGEHLP_MODULE64);
typedef BOOL (WINAPI *StackWalk64Fn)(DWORD, HANDLE, HANDLE, LPSTACKFRAME64, PVOID,
    PREAD_PROCESS_MEMORY_ROUTINE64, PFUNCTION_TABLE_ACCESS_ROUTINE64,
    PGET_MODULE_BASE_ROUTINE64, PTRANSLATE_ADDRESS_ROUTINE64);
typedef PVOID (WINAPI *SymFunctionTableAccess64Fn)(HANDLE, DWORD64);
typedef DWORD64 (WINAPI *SymGetModuleBase64Fn)(HANDLE, DWORD64);
typedef BOOL (WINAPI *SymSetSearchPathFn)(HANDLE, PCSTR);
typedef DWORD (WINAPI *SymGetOptionsFn)(void);

static SymInitializeFn symInitialize;
static SymSetOptionsFn symSetOptions;
static SymFromAddrFn symFromAddr;
static SymGetLineFromAddr64Fn symGetLineFromAddr64;
static SymGetModuleInfo64Fn symGetModuleInfo64;
static StackWalk64Fn stackWalk64;
static SymFunctionTableAccess64Fn symFunctionTableAccess64;
static SymGetModuleBase64Fn symGetModuleBase64;
static SymSetSearchPathFn symSetSearchPath;
static SymGetOptionsFn symGetOptions;
static bool symbolsReady;
static volatile LONG stackLogCount;

// A static buffer for the symbol: the handler runs while the heap may already
// be corrupt, so it must not allocate.
static char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR) + 64];

static const char *
DebugLogBaseName(const char *path)
{
    const char *leaf = path;
    if(path == nil)
        return "?";
    for(const char *p = path; *p; p++)
        if(*p == '\\' || *p == '/')
            leaf = p + 1;
    return leaf;
}

// Prints the frames of a crash. `context` is the state at the fault, so frame 0
// is the instruction that faulted - the one whose address used to be the only
// thing the log had.
static void
DebugLogWindowsStack(HANDLE thread, CONTEXT *context)
{
    if(!symbolsReady || context == nil || thread == nil)
        return;
    // A first-chance fault that something else handles (a probe, a guard
    // hit inside a library) is not the crash, so the walk is not one-shot:
    // it gives up after a few so that a fault loop cannot fill the log, and
    // the frames of the fault that actually kills the process are in it.
    if(InterlockedIncrement(&stackLogCount) > 4)
        return;

    // StackWalk64 advances the context it is given, so it gets a copy: the
    // real one has to stay untouched for the handler chain and the dump.
    CONTEXT walkContext = *context;
    context = &walkContext;

    STACKFRAME64 frame = {};
    DWORD machine;
#if defined(_M_X64) || defined(__x86_64__)
    machine = IMAGE_FILE_MACHINE_AMD64;
    frame.AddrPC.Offset = context->Rip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = context->Rbp;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = context->Rsp;
    frame.AddrStack.Mode = AddrModeFlat;
#elif defined(_M_IX86) || defined(__i386__)
    machine = IMAGE_FILE_MACHINE_I386;
    frame.AddrPC.Offset = context->Eip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = context->Ebp;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = context->Esp;
    frame.AddrStack.Mode = AddrModeFlat;
#else
    return;
#endif

    DebugLogCrashLine("STACK:");
    for(int depth = 0; depth < 24; depth++){
        if(!stackWalk64(machine, GetCurrentProcess(), thread, &frame, context,
		        nil, symFunctionTableAccess64, symGetModuleBase64, nil))
            break;
        if(frame.AddrPC.Offset == 0)
            break;

        // The module the frame is in: an image address is resolved to its own
        // module, so a fault inside the renderer or a vendor DLL says so
        // instead of looking like (or not looking like) an offset in reVC.exe.
        char module[64];
        module[0] = '\0';
        IMAGEHLP_MODULE64 moduleInfo = {};
        moduleInfo.SizeOfStruct = sizeof(moduleInfo);
        if(symGetModuleInfo64(GetCurrentProcess(), frame.AddrPC.Offset, &moduleInfo)){
            snprintf(module, sizeof(module), "%s", DebugLogBaseName(moduleInfo.ImageName));
        }else{
            HMODULE owner = nil;
            if(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
			        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			        (LPCSTR)(uintptr_t)frame.AddrPC.Offset, &owner) && owner){
                char full[MAX_PATH] = {};
                if(GetModuleFileNameA(owner, full, sizeof(full)))
                    snprintf(module, sizeof(module), "%s", DebugLogBaseName(full));
            }
        }

        // Symbol and line. A build without a PDB still yields the exported
        // names DbgHelp can see, and the plain address when it has nothing.
        char name[MAX_SYM_NAME + 1] = {};
        DWORD64 displacement = 0;
        SYMBOL_INFO *symbol = (SYMBOL_INFO*)symbolBuffer;
        memset(symbol, 0, sizeof(symbolBuffer));
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;
        if(symFromAddr(GetCurrentProcess(), frame.AddrPC.Offset, &displacement, symbol))
            snprintf(name, sizeof(name), "%s", symbol->Name);

        char where[160] = {};
        DWORD lineDisplacement = 0;
        IMAGEHLP_LINE64 line = {};
        line.SizeOfStruct = sizeof(line);
        if(symGetLineFromAddr64(GetCurrentProcess(), frame.AddrPC.Offset, &lineDisplacement, &line) && line.FileName)
            snprintf(where, sizeof(where), " [%s:%lu]", DebugLogBaseName(line.FileName), (unsigned long)line.LineNumber);

        char text[512];
        if(name[0])
            snprintf(text, sizeof(text), "  #%02d %s!%s+0x%llX%s (pc=%p)",
			    depth, module[0] ? module : "?", name,
			    (unsigned long long)displacement, where, (void*)(uintptr_t)frame.AddrPC.Offset);
        else
            snprintf(text, sizeof(text), "  #%02d %s+0x%llX%s (pc=%p)",
			    depth, module[0] ? module : "?", (unsigned long long)frame.AddrPC.Offset,
			    where, (void*)(uintptr_t)frame.AddrPC.Offset);
        DebugLogCrashLine(text);
        if(frame.AddrPC.Offset == frame.RetAddr.Offset)
            break;
    }
}
static volatile LONG reportingCrash;

static void
DebugLogWindowsContext(EXCEPTION_POINTERS *info)
{
    if(!info || !info->ExceptionRecord || !info->ContextRecord) return;
    char line[1024];
    EXCEPTION_RECORD *r = info->ExceptionRecord;
    MEMORY_BASIC_INFORMATION memory = {};
    if(VirtualQuery(r->ExceptionAddress, &memory, sizeof(memory))){
        char module[MAX_PATH] = {};
        GetModuleFileNameA((HMODULE)memory.AllocationBase, module, sizeof(module));
        snprintf(line, sizeof(line), "CRASH context: pid=%lu tid=%lu module=%s base=%p RVA=0x%llX",
            (unsigned long)GetCurrentProcessId(), (unsigned long)GetCurrentThreadId(),
            module, memory.AllocationBase,
            (unsigned long long)((uintptr_t)r->ExceptionAddress - (uintptr_t)memory.AllocationBase));
        DebugLogCrashLine(line);
    }
    CONTEXT *c = info->ContextRecord;
#if defined(_M_X64) || defined(__x86_64__)
    snprintf(line, sizeof(line),
        "REGISTERS RIP=%llX RSP=%llX RBP=%llX RAX=%llX RBX=%llX RCX=%llX RDX=%llX RSI=%llX RDI=%llX FLAGS=%lX",
        (unsigned long long)c->Rip, (unsigned long long)c->Rsp, (unsigned long long)c->Rbp,
        (unsigned long long)c->Rax, (unsigned long long)c->Rbx, (unsigned long long)c->Rcx,
        (unsigned long long)c->Rdx, (unsigned long long)c->Rsi, (unsigned long long)c->Rdi,
        (unsigned long)c->EFlags);
    DebugLogCrashLine(line);
    snprintf(line, sizeof(line), "REGISTERS R8=%llX R9=%llX R10=%llX R11=%llX R12=%llX R13=%llX R14=%llX R15=%llX",
        (unsigned long long)c->R8, (unsigned long long)c->R9, (unsigned long long)c->R10,
        (unsigned long long)c->R11, (unsigned long long)c->R12, (unsigned long long)c->R13,
        (unsigned long long)c->R14, (unsigned long long)c->R15);
    DebugLogCrashLine(line);
#elif defined(_M_IX86) || defined(__i386__)
    snprintf(line, sizeof(line), "REGISTERS EIP=%lX ESP=%lX EBP=%lX EAX=%lX EBX=%lX ECX=%lX EDX=%lX",
        (unsigned long)c->Eip, (unsigned long)c->Esp, (unsigned long)c->Ebp,
        (unsigned long)c->Eax, (unsigned long)c->Ebx, (unsigned long)c->Ecx, (unsigned long)c->Edx);
    DebugLogCrashLine(line);
#endif
}

static void
DebugLogSaveDump(EXCEPTION_POINTERS *info)
{
    if(InterlockedCompareExchange(&reportingCrash, 1, 0) != 0) return;
    if(!writeDump || !dumpPath[0]){
        DebugLogCrashLine("CRASH dump unavailable: DbgHelp was not initialized");
        return;
    }
    HANDLE file = CreateFileA(dumpPath, GENERIC_WRITE, 0, nil, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nil);
    if(file == INVALID_HANDLE_VALUE){
        DebugLogCrashLine("CRASH dump: cannot create output file");
        return;
    }
    MINIDUMP_EXCEPTION_INFORMATION exception = {};
    exception.ThreadId = GetCurrentThreadId();
    exception.ExceptionPointers = info;
    exception.ClientPointers = FALSE;
    BOOL ok = writeDump(GetCurrentProcess(), GetCurrentProcessId(), file,
        (MINIDUMP_TYPE)(MiniDumpNormal | MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules),
        info ? &exception : nil, nil, nil);
    DWORD error = ok ? ERROR_SUCCESS : GetLastError();
    FlushFileBuffers(file);
    CloseHandle(file);
    char line[512];
    snprintf(line, sizeof(line), "CRASH dump: %s (error=%lu) path=%s; retain matching EXE and PDB",
        ok ? "saved" : "FAILED", (unsigned long)error, dumpPath);
    DebugLogCrashLine(line);
}

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
	   record->ExceptionCode != EXCEPTION_STACK_OVERFLOW &&
	   record->ExceptionCode != 0xC0000374UL && // STATUS_HEAP_CORRUPTION
	   record->ExceptionCode != 0xC0000409UL)   // STATUS_STACK_BUFFER_OVERRUN / fail-fast
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
	// The faulting stack, symbolised. A fault that Windows recovers from only
	// reaches the unhandled filter if nothing handles it, so the walk happens
	// here, at the moment the fault is real; the one-shot guard keeps it to the
	// first one, which is the one that matters.
	if(record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION ||
	   record->ExceptionCode == EXCEPTION_IN_PAGE_ERROR)
		DebugLogWindowsStack(GetCurrentThread(), context);
	// Heap termination may bypass the unhandled filter. Record these when
	// Windows delivers them to VEH; true fast-fail can still bypass all handlers.
	if(record->ExceptionCode == 0xC0000374UL || record->ExceptionCode == 0xC0000409UL){
		DebugLogWindowsContext(info);
		DebugLogSaveDump(info);
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
	DebugLogSaveDump(nil);
	DebugLogCrashLine("---- session ended abnormally ----");
	signal(sig, SIG_DFL);
	raise(sig);
}

static LONG WINAPI
DebugLogUnhandledException(EXCEPTION_POINTERS *info)
{
	if(!info || !info->ExceptionRecord) return EXCEPTION_CONTINUE_SEARCH;
	char line[128];
	snprintf(line, sizeof(line), "CRASH: exception 0x%08lX at %p - see the lines above for what was loaded last",
		(unsigned long)info->ExceptionRecord->ExceptionCode,
		info->ExceptionRecord->ExceptionAddress);
	DebugLogCrashLine(line);
	DebugLogWindowsContext(info);
	// The vectored handler walks the stack for the faults it recognises. One
	// that only reaches the unhandled filter - an abort raised as an exception,
	// or a fault the filter sees after a handler declined it - still gets a
	// stack here. The walk is bounded, so this cannot flood the log.
	if(info->ContextRecord)
		DebugLogWindowsStack(GetCurrentThread(), info->ContextRecord);
	DebugLogSaveDump(info);
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
	// If DWARF unwind stopped inside a stripped leaf function in libc/GLES, also log FP chain
	if (st.depth <= 4 && regFp != 0 && regSp != 0 && (regFp & 0x7u) == 0 &&
	    regFp >= regSp && regFp - regSp < (1u << 20)) {
		DebugLogCrashLine("fp-chain:");
		uintptr_t curFp = regFp;
		for (int f = 0; f < 12 && curFp >= regSp && curFp - regSp < (1u << 20) && (curFp & 0x7u) == 0; f++) {
			const uintptr_t *pair = (const uintptr_t *)curFp;
			uintptr_t nextFp = pair[0];
			uintptr_t savedLr = pair[1];
			if (savedLr == 0)
				break;
			AndroidBtHelper::formatAddr(f, savedLr);
			if (nextFp <= curFp)
				break;
			curFp = nextFp;
		}
	}
	if (st.depth <= 4 && regSp != 0 && g_libREVC != 0) {
		uintptr_t spAligned = (regSp + 7u) & ~(uintptr_t)7u;
		uintptr_t pageEnd = (regSp & ~(uintptr_t)0xfffu) + 0x1000u;
		int found = 0;
		for (uintptr_t cur = spAligned; cur + sizeof(uintptr_t) <= pageEnd && cur - spAligned < 1024u && found < 8; cur += sizeof(uintptr_t)) {
			uintptr_t val = *(const uintptr_t *)cur;
			if (val >= g_libREVC && val < g_libREVC + 0x01000000ULL && (val & 0x3u) == 0) {
				if (found == 0)
					DebugLogCrashLine("sp-scan:");
				AndroidBtHelper::formatAddr(found++, val);
			}
		}
	}
#endif
#endif
	DebugLogCrashLine("---- session ended abnormally ----");
	// let the default handling produce a core dump / normal termination
	signal(sig, SIG_DFL);
	raise(sig);
}

#endif // _WIN32

// An uncaught C++ exception can otherwise terminate without reaching SEH.
static void
DebugLogTerminate(void)
{
    DebugLogCrashLine("CRASH: std::terminate (uncaught exception or noexcept violation)");
    std::exception_ptr exception = std::current_exception();
    if(exception){
        try { std::rethrow_exception(exception); }
        catch(const std::exception &e) { DebugLogCrashLine(e.what()); }
        catch(...) { DebugLogCrashLine("CRASH: non-standard C++ exception"); }
    }
    abort(); // existing SIGABRT handler records abnormal exit and attempts a dump
}

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
	DWORD pathLength = GetFullPathNameA("crash.dmp", sizeof(dumpPath), dumpPath, nil);
	if(pathLength == 0 || pathLength >= sizeof(dumpPath)) dumpPath[0] = '\0';
	HMODULE dbghelp = LoadLibraryA("dbghelp.dll");
	if(dbghelp) writeDump = (WriteDumpFn)GetProcAddress(dbghelp, "MiniDumpWriteDump");
	// Symbolising the crash stack needs the same DLL. Everything is resolved
	// now, at startup: a crash handler must not call GetProcAddress or load a
	// DLL while the process is in an unknown state.
	if(dbghelp){
		symInitialize = (SymInitializeFn)GetProcAddress(dbghelp, "SymInitialize");
		symSetOptions = (SymSetOptionsFn)GetProcAddress(dbghelp, "SymSetOptions");
		symGetOptions = (SymGetOptionsFn)GetProcAddress(dbghelp, "SymGetOptions");
		symFromAddr = (SymFromAddrFn)GetProcAddress(dbghelp, "SymFromAddr");
		symGetLineFromAddr64 = (SymGetLineFromAddr64Fn)GetProcAddress(dbghelp, "SymGetLineFromAddr64");
		symGetModuleInfo64 = (SymGetModuleInfo64Fn)GetProcAddress(dbghelp, "SymGetModuleInfo64");
		stackWalk64 = (StackWalk64Fn)GetProcAddress(dbghelp, "StackWalk64");
		symFunctionTableAccess64 = (SymFunctionTableAccess64Fn)GetProcAddress(dbghelp, "SymFunctionTableAccess64");
		symGetModuleBase64 = (SymGetModuleBase64Fn)GetProcAddress(dbghelp, "SymGetModuleBase64");
		symSetSearchPath = (SymSetSearchPathFn)GetProcAddress(dbghelp, "SymSetSearchPath");
		if(symInitialize && symSetOptions && symFromAddr && symGetLineFromAddr64 &&
		   symGetModuleInfo64 && stackWalk64 && symFunctionTableAccess64 && symGetModuleBase64){
			// UNDNAME so the frames read as C++ names. The PDB is searched next
			// to the EXE, which is where the build puts it; a run without one
			// still resolves exports and module+offset.
			symSetOptions((symGetOptions ? symGetOptions() : 0) |
				SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
			// The working directory changes during startup, so the search path is
			// pinned to the executable's own folder at init time.
			char exePath[MAX_PATH] = {};
			DWORD exeLen = GetModuleFileNameA(nil, exePath, sizeof(exePath));
			if(exeLen > 0 && exeLen < sizeof(exePath)){
				char *slash = strrchr(exePath, '\\');
				if(slash)
					slash[1] = '\0';
				else
					exePath[0] = '\0';
				if(symSetSearchPath)
					symSetSearchPath(GetCurrentProcess(), exePath);
			}
			symbolsReady = symInitialize(GetCurrentProcess(), nil, TRUE) != FALSE;
			DebugLog::Write("INFO", nil, 0, nil, "Crash stacks: %s",
				symbolsReady ? "symbolised (keep the matching PDB next to the EXE)" : "not symbolised");
		}
	}
	DebugLog::Write("INFO", nil, 0, nil, "Crash dump %s: %s (keep matching EXE/PDB)",
		writeDump ? "enabled" : "unavailable", dumpPath);
	ULONG stackReserve = 64 * 1024;
	SetThreadStackGuarantee(&stackReserve); // main-thread stack overflow reporting
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
	std::set_terminate(DebugLogTerminate);
	atexit(DebugLogExitMarker);
}

#else // CUSTOM_MODELS

void DebugLogInit(void) {}
void DebugLogEnsure(void) {}
void DebugLogPrintf(const char*, ...) {}
const char *DebugLogPath(void) { return ""; }

#endif // CUSTOM_MODELS
