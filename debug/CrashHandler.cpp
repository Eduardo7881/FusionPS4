#include "debug/CrashHandler.hpp"

#include "debug/Log.hpp"
#include "debug/UnimplementedRegistry.hpp"

#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ucontext.h>
#include <unistd.h>

namespace fusionps4::debug {

namespace {

std::atomic<bool>           g_inHandler{false};
Symbolizer                  g_symbolizer;
CrashHandler::PreReportHook g_preHook = nullptr;
char                        g_reportPath[512] = {0};

const char* signalName(int sig) {
    switch (sig) {
        case SIGSEGV: return "SIGSEGV";
        case SIGBUS:  return "SIGBUS";
        case SIGILL:  return "SIGILL";
        case SIGFPE:  return "SIGFPE";
        case SIGABRT: return "SIGABRT";
        default:      return "SIG?";
    }
}

// Async-signal-safe writes.
void safeWrite(int fd, const char* s, std::size_t n) {
    while (n) {
        const ssize_t w = ::write(fd, s, n);
        if (w <= 0) return;
        s += w;
        n -= static_cast<std::size_t>(w);
    }
}

void safeWriteCStr(int fd, const char* s) {
    safeWrite(fd, s, std::strlen(s));
}

void safeWriteHex(int fd, std::uint64_t v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "0x%016llx", (unsigned long long)v);
    safeWriteCStr(fd, buf);
}

void reportLine(int fd, const char* tag, std::uint64_t v) {
    safeWriteCStr(fd, tag);
    safeWriteHex(fd, v);
    safeWriteCStr(fd, "\n");
}

void emitBacktrace(int fd, const ucontext_t* uc) {
    // Best-effort walk using the frame pointer chain. The guest may be
    // compiled without frame pointers, in which case the walk terminates
    // early — that is acceptable: we still emit the faulting PC and the
    // module it belongs to.
    safeWriteCStr(fd, "-- backtrace --\n");
    const std::uintptr_t pc =
        static_cast<std::uintptr_t>(uc->uc_mcontext.gregs[REG_RIP]);
    const auto desc = g_symbolizer.describe(pc);
    safeWriteCStr(fd, "  #0 ");
    safeWriteHex(fd, pc);
    if (!desc.empty()) {
        safeWriteCStr(fd, "  ");
        safeWriteCStr(fd, desc.c_str());
    }
    safeWriteCStr(fd, "\n");

    std::uintptr_t rbp =
        static_cast<std::uintptr_t>(uc->uc_mcontext.gregs[REG_RBP]);
    for (int i = 1; i < 32 && rbp; ++i) {
        // Bounds are sanity-checked so a corrupted rbp does not hang us.
        if (rbp < 0x1000 || rbp > 0x00007fffffffffffULL) break;
        const auto* frame = reinterpret_cast<const std::uintptr_t*>(rbp);
        const std::uintptr_t nextRbp = frame[0];
        const std::uintptr_t retAddr = frame[1];
        if (retAddr == 0) break;
        safeWriteCStr(fd, "  #");
        char num[8];
        std::snprintf(num, sizeof(num), "%d ", i);
        safeWriteCStr(fd, num);
        safeWriteHex(fd, retAddr);
        const auto d = g_symbolizer.describe(retAddr);
        if (!d.empty()) {
            safeWriteCStr(fd, "  ");
            safeWriteCStr(fd, d.c_str());
        }
        safeWriteCStr(fd, "\n");
        if (nextRbp <= rbp) break;
        rbp = nextRbp;
    }
}

void emitCrashReport(int sig, siginfo_t* info, void* ucontextVoid) {
    const int fd = STDERR_FILENO;

    safeWriteCStr(fd, "\n==== FusionPS4 crash ====\n");
    safeWriteCStr(fd, "signal: ");
    safeWriteCStr(fd, signalName(sig));
    safeWriteCStr(fd, "\n");

    if (info) {
        reportLine(fd, "fault address: ", (std::uint64_t)(uintptr_t)info->si_addr);
        safeWriteCStr(fd, "si_code: ");
        safeWriteHex(fd, (std::uint64_t)info->si_code);
        safeWriteCStr(fd, "\n");
    }

    const auto* uc = static_cast<const ucontext_t*>(ucontextVoid);
    if (uc) {
        reportLine(fd, "RIP: ", uc->uc_mcontext.gregs[REG_RIP]);
        reportLine(fd, "RSP: ", uc->uc_mcontext.gregs[REG_RSP]);
        reportLine(fd, "RBP: ", uc->uc_mcontext.gregs[REG_RBP]);
        reportLine(fd, "RAX: ", uc->uc_mcontext.gregs[REG_RAX]);
        reportLine(fd, "RBX: ", uc->uc_mcontext.gregs[REG_RBX]);
        reportLine(fd, "RCX: ", uc->uc_mcontext.gregs[REG_RCX]);
        reportLine(fd, "RDX: ", uc->uc_mcontext.gregs[REG_RDX]);
        reportLine(fd, "RSI: ", uc->uc_mcontext.gregs[REG_RSI]);
        reportLine(fd, "RDI: ", uc->uc_mcontext.gregs[REG_RDI]);
        reportLine(fd, "R8 : ", uc->uc_mcontext.gregs[REG_R8]);
        reportLine(fd, "R9 : ", uc->uc_mcontext.gregs[REG_R9]);
        reportLine(fd, "R10: ", uc->uc_mcontext.gregs[REG_R10]);
        reportLine(fd, "R11: ", uc->uc_mcontext.gregs[REG_R11]);
        reportLine(fd, "R12: ", uc->uc_mcontext.gregs[REG_R12]);
        reportLine(fd, "R13: ", uc->uc_mcontext.gregs[REG_R13]);
        reportLine(fd, "R14: ", uc->uc_mcontext.gregs[REG_R14]);
        reportLine(fd, "R15: ", uc->uc_mcontext.gregs[REG_R15]);
        emitBacktrace(fd, uc);
    }

    // The most common cause of a crash in a compatibility runtime is a
    // stub that returned garbage. Including the report makes diagnosis
    // possible without re-running under a debugger.
    const auto unimpl = UnimplementedRegistry::instance().formatReport();
    safeWriteCStr(fd, "-- UNIMPLEMENTED so far --\n");
    safeWrite(fd, unimpl.data(), unimpl.size());
    safeWriteCStr(fd, "=========================\n");
}

void handler(int sig, siginfo_t* info, void* ucontextVoid) {
    // Guard against recursion: a crash inside the crash handler cannot be
    // recovered from, so we immediately re-raise.
    bool expected = false;
    if (!g_inHandler.compare_exchange_strong(expected, true)) {
        ::signal(sig, SIG_DFL);
        ::raise(sig);
        return;
    }

    if (g_preHook) {
        // Pre-hook is not async-signal-safe by contract; callers are
        // expected to keep it minimal.
        g_preHook();
    }

    emitCrashReport(sig, info, ucontextVoid);

    // Re-raise with the default handler so the kernel dumps core.
    ::signal(sig, SIG_DFL);
    ::raise(sig);
}

} // namespace

void CrashHandler::install(Symbolizer sym) {
    g_symbolizer = sym;

    struct sigaction sa{};
    sa.sa_sigaction = handler;
    sa.sa_flags     = SA_SIGINFO | SA_RESTART;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGSEGV);
    sigaddset(&sa.sa_mask, SIGBUS);
    sigaddset(&sa.sa_mask, SIGILL);
    sigaddset(&sa.sa_mask, SIGFPE);
    sigaddset(&sa.sa_mask, SIGABRT);

    for (int sig : { SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT }) {
        sigaction(sig, &sa, nullptr);
    }
}

void CrashHandler::setPreReportHook(PreReportHook h) { g_preHook = h; }

void CrashHandler::setReportFile(const char* path) {
    if (!path) { g_reportPath[0] = '\0'; return; }
    std::strncpy(g_reportPath, path, sizeof(g_reportPath) - 1);
    g_reportPath[sizeof(g_reportPath) - 1] = '\0';
}

} // namespace fusionps4::debug
