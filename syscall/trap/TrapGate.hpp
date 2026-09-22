#pragma once

#include "syscall/trap/TrapRing.hpp"

#include <cstdint>

namespace fusionps4::syscall::trap {

// Child-side state that the SIGSYS handler needs. Set up before seccomp
// is installed, never modified afterwards (except for fields that are
// per-thread, which live in the handler's own storage).
struct TrapGateConfig {
    TrapRing*          ring       = nullptr;
    std::uint32_t      guestTid   = 0;
    bool               trace      = false;
};

// Installs the SIGSYS handler and configures the alternate signal stack.
// Must be called in the child process, after fork() and before
// SeccompFilter::install().
//
// After this call, any syscall from this process that is not on the
// seccomp allowlist will deliver SIGSYS and be forwarded through the ring.
class TrapGate {
public:
    static bool install(const TrapGateConfig& cfg);

    // Called from the guest thread right before the seccomp filter goes
    // live. Records the current tid so the server can associate requests
    // with guest threads.
    static void setCurrentThreadId(std::uint32_t tid);
};

} // namespace fusionps4::syscall::trap
