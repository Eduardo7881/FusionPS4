#pragma once

#include "isolation/IsolationConfig.hpp"

#include <cstdint>
#include <vector>

namespace fusionps4::isolation {

// Installs a seccomp-BPF filter that traps every syscall as SIGSYS, except
// for a small allowlist of syscalls required by the trap handler itself.
//
// The trap is delivered to the calling thread; the SIGSYS handler
// (see syscall/trap/TrapGate) reads the syscall number and arguments from
// the ucontext, forwards the request to the runtime via a shared ring, and
// writes the response back into RAX before returning.
class SeccompFilter {
public:
    // Installs the filter on the calling thread. Returns false on any
    // failure; the errno is logged.
    //
    // SECCOMP_FILTER_FLAG_TSYNC is used so that any threads the process
    // already has (they should be none at this point) also receive the
    // filter. This makes it impossible for a guest thread to escape the
    // trap by creating a new thread.
    static bool install(const IsolationConfig& cfg);

    // Installs the filter with SECCOMP_FILTER_FLAG_TSYNC | LOG, which
    // additionally reports trapped syscalls to the kernel audit log. Only
    // for debugging.
    static bool installWithAudit(const IsolationConfig& cfg);

private:
    static bool installImpl(const IsolationConfig& cfg, bool audit);
};

} // namespace fusionps4::isolation
