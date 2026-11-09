//
// Created by mrxenginner on 13/07/2025.
//

#ifndef REVC_STACKTRACE_H
#define REVC_STACKTRACE_H

#if defined ANDROID

#include <dlfcn.h>
#include <stdint.h>
#include <unwind.h>

extern uintptr_t g_libREVC;

#if defined(__arm__)
#define PRINT_CRASH_STATES(context) \
	Logger::CrashLog("register states:"); \
	Logger::CrashLog("r0: 0x%lx, r1: 0x%lx, r2: 0x%lx, r3: 0x%lx", (unsigned long)(context)->uc_mcontext.arm_r0, (unsigned long)(context)->uc_mcontext.arm_r1, (unsigned long)(context)->uc_mcontext.arm_r2, (unsigned long)(context)->uc_mcontext.arm_r3); \
	Logger::CrashLog("r4: 0x%lx, r5: 0x%lx, r6: 0x%lx, r7: 0x%lx", (unsigned long)(context)->uc_mcontext.arm_r4, (unsigned long)(context)->uc_mcontext.arm_r5, (unsigned long)(context)->uc_mcontext.arm_r6, (unsigned long)(context)->uc_mcontext.arm_r7); \
	Logger::CrashLog("r8: 0x%lx, r9: 0x%lx, sl: 0x%lx, fp: 0x%lx", (unsigned long)(context)->uc_mcontext.arm_r8, (unsigned long)(context)->uc_mcontext.arm_r9, (unsigned long)(context)->uc_mcontext.arm_r10, (unsigned long)(context)->uc_mcontext.arm_fp); \
	Logger::CrashLog("ip: 0x%lx, sp: 0x%lx, lr: 0x%lx, pc: 0x%lx", (unsigned long)(context)->uc_mcontext.arm_ip, (unsigned long)(context)->uc_mcontext.arm_sp, (unsigned long)(context)->uc_mcontext.arm_lr, (unsigned long)(context)->uc_mcontext.arm_pc);
#elif defined(__aarch64__)
#define PRINT_CRASH_STATES(context) \
	Logger::CrashLog("sp: 0x%lx, pc: 0x%lx, lr: 0x%lx", (unsigned long)(context)->uc_mcontext.sp, (unsigned long)(context)->uc_mcontext.pc, (unsigned long)(context)->uc_mcontext.regs[30]);
#else
#define PRINT_CRASH_STATES(context) \
	(void)(context);
#endif

class CStackTrace
{
public:
	static void printBacktrace()
	{
		Logger::CrashLog("------------ START BACKTRACE ------------");
		Logger::CrashLog(" ");
		PrintStackTrace();
		Logger::CrashLog("------------  END BACKTRACE  ------------");
	}

private:
	static _Unwind_Reason_Code TraceFunction(_Unwind_Context* context, void* arg) {
		uintptr_t pc = _Unwind_GetIP(context);
		if (pc == 0)
			return _URC_NO_REASON;

		// Offsets are printed relative to the game library so they stay usable
		// with a different load address; dli_fname is deliberately not printed
		// because dladdr is not guaranteed to be async-signal-safe.
		if (g_libREVC != 0 && pc > g_libREVC)
			Logger::CrashLog("librevc.so + 0x%lx (abs 0x%lx)",
			                 (unsigned long)(pc - g_libREVC), (unsigned long)pc);
		else
			Logger::CrashLog("abs 0x%lx", (unsigned long)pc);

		return _URC_NO_REASON;
	}

	static void PrintStackTrace() {
		_Unwind_Backtrace(TraceFunction, nullptr);
	}
};
#endif

#endif
