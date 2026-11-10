//
// Created by mrxenginner on 11/05/2025.
//

#if defined ANDROID

#include "SignalHandler.h"
#include "../logger/log.h"
#include "StackTrace.h"

#include <csignal>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <ucontext.h>

extern int16 g_usLastProcessedModelIndexAutomobile;
extern int g_iLastProcessedModelIndexAutoEnt;

extern int g_iLastProcessedSkinCollision;
extern int g_iLastProcessedEntityCollision;
extern char lastFile[123];
extern int g_iLastRenderedObject;
extern int lastNvEvent;
extern CVector lastPos;
char g_iLastBlock[123];
char streamimgState[255];

namespace CrashHandler {
    namespace {
        enum {
            SEGV_INDEX = 0,
            ABRT_INDEX,
            FPE_INDEX,
            BUS_INDEX,
            NUM_SIGNALS
        };

        struct sigaction g_oldHandlers[NUM_SIGNALS];
        bool g_installed = false;

        // Dedicated signal stack: the crash report must run even when the
        // fault is a stack overflow.
        const size_t SIGNAL_STACK_SIZE = 64 * 1024;
        char g_signalStack[SIGNAL_STACK_SIZE];

        // 0 if the previous disposition was not installed by us (or did not
        // exist at all), otherwise the address that should run after our log.
        void* PreviousHandler(int index)
        {
            struct sigaction& old = g_oldHandlers[index];
            if (old.sa_flags & SA_SIGINFO) {
                if (old.sa_sigaction != nullptr)
                    return reinterpret_cast<void*>(old.sa_sigaction);
                return nullptr;
            }
            // Plain handler installed: SIG_DFL (0) and SIG_IGN (1) must not be
            // called as functions, that was the SIG_IGN -> jump to address 1
            // crash this handler used to have.
            if (reinterpret_cast<uintptr_t>(old.sa_handler) > 1)
                return reinterpret_cast<void*>(old.sa_handler);
            return nullptr;
        }

        void PrintBuildCrashInfo()
        {
            time_t currentTime = time(nullptr);
            struct tm timeInfo;
            if (localtime_r(&currentTime, &timeInfo) == nullptr)
                return;

            // tm_mon is 0-based and tm_year counts from 1900.
            Logger::CrashLog("Crash time: %02d:%02d:%02d %04d-%02d-%02d",
                             timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec,
                             timeInfo.tm_year + 1900, timeInfo.tm_mon + 1, timeInfo.tm_mday);
#if defined(__arm__)
            const char *abi = "armeabi-v7a";
#else
            const char *abi = "arm64-v8a";
#endif
            Logger::CrashLog("Build times: %s %s. ABI: %s", __TIME__, __DATE__, abi);
            Logger::CrashLog("Last processed auto and entity: %d %d",
                             (int)g_usLastProcessedModelIndexAutomobile,
                             g_iLastProcessedModelIndexAutoEnt);
            Logger::CrashLog("Last rendered object: %d", g_iLastRenderedObject);
            Logger::CrashLog("Game library base: 0x%lx", (unsigned long)g_libREVC);
        }

        void SignalHandler(int signum, siginfo_t* info, void* contextPtr)
        {
            // Block every signal we care about while the report is written, so
            // a second fault (e.g. in the backtrace) cannot rewrite the log.
            sigset_t mask;
            sigfillset(&mask);
            sigprocmask(SIG_BLOCK, &mask, nullptr);

            auto* context = static_cast<ucontext_t*>(contextPtr);

            int index = -1;
            const char* signalName = nullptr;

            switch (signum) {
                case SIGSEGV: index = SEGV_INDEX; signalName = "SIGSEGV"; break;
                case SIGABRT: index = ABRT_INDEX; signalName = "SIGABRT"; break;
                case SIGFPE:  index = FPE_INDEX;  signalName = "SIGFPE";  break;
                case SIGBUS:  index = BUS_INDEX;  signalName = "SIGBUS";  break;
                default:
                    Logger::CrashLog("Unhandled signal: %d", signum);
                    return;
            }

            PrintBuildCrashInfo();
            Logger::CrashLog("%s | Fault address: %p | si_code: %d", signalName,
                             info != nullptr ? info->si_addr : nullptr,
                             info != nullptr ? info->si_code : 0);
            if (context != nullptr)
                PRINT_CRASH_STATES(context);
            CStackTrace::printBacktrace();

            // Chain to the previous handler so the platform can still write its
            // own tombstone (otherwise the process would keep running in a
            // broken state and Android would report a hang instead of a crash).
            void* previous = PreviousHandler(index);
            if (previous != nullptr) {
                sigset_t oldMask;
                sigprocmask(SIG_SETMASK, &g_oldHandlers[index].sa_mask, &oldMask);
                if (g_oldHandlers[index].sa_flags & SA_SIGINFO)
                    reinterpret_cast<void (*)(int, siginfo_t*, void*)>(previous)(signum, info, contextPtr);
                else
                    reinterpret_cast<void (*)(int)>(previous)(signum);
                return;
            }

            // Restore the default disposition and re-raise: the default action
            // terminates the process, which is what the OS expects.
            signal(signum, SIG_DFL);
            raise(signum);
        }
    }

    void SetupSignalHandlers()
    {
        if (g_installed)
            return;

        stack_t stack;
        memset(&stack, 0, sizeof(stack));
        stack.ss_sp = g_signalStack;
        stack.ss_size = SIGNAL_STACK_SIZE;
        stack.ss_flags = 0;
        sigaltstack(&stack, nullptr);

        struct {
            int signal;
            int index;
        } signals[] = {
                { SIGSEGV, SEGV_INDEX },
                { SIGABRT, ABRT_INDEX },
                { SIGFPE,  FPE_INDEX  },
                { SIGBUS,  BUS_INDEX  },
        };

        for (const auto& s : signals) {
            struct sigaction act;
            memset(&act, 0, sizeof(act));
            act.sa_sigaction = SignalHandler;
            sigemptyset(&act.sa_mask);
            // Alternate signal stack: a stack overflow (very common in this
            // engine) otherwise faults inside the handler itself.
            act.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESTART;
            sigaction(s.signal, &act, &g_oldHandlers[s.index]);
        }
        g_installed = true;
    }
}
#endif
