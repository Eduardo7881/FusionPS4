#include "syscall/trap/TrapGate.hpp"

#include "debug/Log.hpp"
#include "syscall/trap/SceCallIds.hpp"
#include "syscall/trap/TrapRegisters.hpp"

#include <cerrno>
#include <csignal>
#include <cstring>
#include <sys/mman.h>
#include <ucontext.h>

using fusionps4::debug::LogCategory;

namespace fusionps4::syscall::trap {

namespace {

TrapGateConfig g_config;
thread_local std::uint32_t g_currentTid = 0;

// Alternate signal stack. Reserved before seccomp so sigaltstack() is
// allowed by the kernel; the signal handler runs on this stack, which lets
// it make the (allowlisted) futex calls the trap ring requires.
constexpr std::size_t kAltStackSize = 64 * 1024;
alignas(16) std::uint8_t g_altStack[kAltStackSize];

extern "C" void fusionps4_sigsys_handler(int sig, siginfo_t* info, void* uctxVoid) {
    if (sig != SIGSYS) return;

    auto* uc = static_cast<ucontext_t*>(uctxVoid);
    TrapRegisters regs{uc};

    const std::uint64_t number = regs.syscallNumber();
    TrapRequest req;
    req.number   = number;
    req.threadId = g_currentTid;

    if (isSceCall(number)) {
        req.kind = 1;
        const std::uint32_t id = sceCallId(number);
        // Args 0..5 are read in the *syscall* convention. Trampolines
        // shuffle RCX -> R10 before the syscall instruction so this works.
        for (int i = 0; i < 6; ++i) req.args[i] = regs.arg(i);
        (void)id;
    } else {
        req.kind = 0;
        for (int i = 0; i < 6; ++i) req.args[i] = regs.arg(i);
    }

    if (g_config.trace) {
        char buf[160];
        std::snprintf(buf, sizeof(buf),
                      "trap: number=0x%llx kind=%u tid=%u",
                      (unsigned long long)number,
                      (unsigned)req.kind, (unsigned)req.threadId);
        ::write(STDERR_FILENO, buf, std::strlen(buf));
        ::write(STDERR_FILENO, "\n", 1);
    }

    const TrapResponse resp = g_config.ring->submit(req);

    regs.setResult(resp.retval);
    regs.setCarryFlag(resp.isError != 0);
    (void)info;
}

} // namespace

bool TrapGate::install(const TrapGateConfig& cfg) {
    g_config = cfg;

    // Alternate stack must be set before seccomp; the SIGSYS handler will
    // run on it once the trap goes live.
    stack_t ss{};
    ss.ss_sp    = g_altStack;
    ss.ss_size  = kAltStackSize;
    ss.ss_flags = 0;
    if (::sigaltstack(&ss, nullptr) != 0) {
        FP4_ERROR(LogCategory::Process)
            << "sigaltstack failed: " << std::strerror(errno);
        return false;
    }

    struct sigaction sa{};
    sa.sa_sigaction = fusionps4_sigsys_handler;
    sa.sa_flags     = SA_SIGINFO | SA_ONSTACK | SA_NODEFER;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGSYS);

    if (::sigaction(SIGSYS, &sa, nullptr) != 0) {
        FP4_ERROR(LogCategory::Process)
            << "sigaction(SIGSYS) failed: " << std::strerror(errno);
        return false;
    }

    FP4_INFO(LogCategory::Process)
        << "TrapGate installed: SIGSYS handler + alternate stack ready";
    return true;
}

void TrapGate::setCurrentThreadId(std::uint32_t tid) {
    g_currentTid = tid;
}

} // namespace fusionps4::syscall::trap
