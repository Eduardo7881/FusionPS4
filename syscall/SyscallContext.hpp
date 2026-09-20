#pragma once

#include "syscall/freebsd/FreeBsd.hpp"

#include <cstdint>
#include <string>

namespace fusionps4::runtime::process { class PS4Process; }
namespace fusionps4::runtime::thread  { class ThreadObject; }

namespace fusionps4::syscall {

// Snapshot of the guest CPU state as seen by the dispatcher. Only the
// fields actually needed to implement Linux-hosted syscalls are present;
// SIMD and FPU state are intentionally excluded, since a user-space syscall
// dispatcher must not touch them.
struct SyscallContext {
    std::int64_t  number = 0;

    // System V x86_64 syscall convention: rdi, rsi, rdx, r10, r8, r9.
    std::uint64_t args[6] = {0, 0, 0, 0, 0, 0};

    // Return value written to rax. When `isError` is true the runtime
    // treats the low bits of retval as a FreeBSD errno, matching the
    // carry-flag semantics used by PS4 user space.
    std::int64_t  retval  = 0;
    bool          isError = false;

    // Convenience accessors with the semantics expected by the SysV ABI.
    std::uint64_t arg(int i) const { return (i >= 0 && i < 6) ? args[i] : 0; }

    void ok(std::int64_t v = 0) {
        retval  = v;
        isError = false;
    }

    void fail(std::int64_t errnoValue) {
        retval  = errnoValue;
        isError = true;
    }

    // Populated by the dispatcher before invoking a handler; never null.
    runtime::process::PS4Process*  process = nullptr;
    runtime::thread::ThreadObject* thread  = nullptr;

    // Human-readable syscall name (e.g. "read") for logging.
    const char* name = "unknown";
};

} // namespace fusionps4::syscall
