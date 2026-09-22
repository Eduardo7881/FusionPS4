#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace fusionps4::isolation {

// Guest isolation policy. All flags default to the most restrictive values;
// the runtime only relaxes them if the environment explicitly asks.
struct IsolationConfig {
    // ---- namespace isolation --------------------------------------------
    bool use_user_ns  = true;    // CLONE_NEWUSER
    bool use_pid_ns   = true;    // CLONE_NEWPID
    bool use_mount_ns = true;    // CLONE_NEWNS
    bool use_net_ns   = true;    // CLONE_NEWNET
    bool use_ipc_ns   = true;    // CLONE_NEWIPC
    bool use_uts_ns   = true;    // CLONE_NEWUTS

    std::string guest_hostname = "PS4";

    // Path to a directory the runtime can use as the new root for the guest.
    // If empty, a tmpfs is mounted automatically at runtime.
    std::string jail_root;

    // ---- capability isolation -------------------------------------------
    bool  drop_capabilities = true;
    bool  no_new_privs      = true;

    // ---- seccomp --------------------------------------------------------
    bool trap_all_syscalls  = true;

    // Syscalls explicitly allowed to pass through to the Linux kernel. These
    // are the ones the SIGSYS handler itself needs to function; none of them
    // give the guest access to host resources.
    std::vector<std::int32_t> syscall_allowlist = {
        // Required by the trap mechanism itself.
        /* futex          */ 202,
        /* rt_sigreturn   */ 15,
        /* sigaltstack    */ 131,
        /* restart_syscall*/ 219,
        /* rseq            */ 334,
        // Required by the guest to manipulate its own view of the arena.
        // The guest does not know Linux syscall numbers, so these cannot be
        // reached by mistake.
        /* mmap            */ 9,
        /* mprotect        */ 10,
        /* munmap          */ 11,
        // Clean shutdown.
        /* exit            */ 60,
        /* exit_group      */ 231,
    };

    // ---- logging --------------------------------------------------------
    // When true, every syscall forwarded to the runtime is traced at
    // LogLevel::Trace. Useful for debugging isolation failures.
    bool trace_syscalls = false;

    // When true, every SCE call forwarded to the runtime is traced.
    bool trace_sce_calls = false;

    // ---- fail-safe ------------------------------------------------------
    // If a privileged setup step fails (namespace creation, pivot_root,
    // etc.), abort instead of silently degrading. Defaults to true because
    // a silently degraded sandbox is worse than a loud failure.
    bool abort_on_isolation_failure = true;
};

IsolationConfig loadIsolationConfigFromEnvironment();

} // namespace fusionps4::isolation
