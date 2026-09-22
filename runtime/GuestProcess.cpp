#include "runtime/GuestProcess.hpp"

#include "debug/Log.hpp"
#include "isolation/Capabilities.hpp"
#include "isolation/NamespaceSetup.hpp"
#include "isolation/SeccompFilter.hpp"
#include "syscall/trap/TrapGate.hpp"

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <sys/wait.h>
#include <unistd.h>

using fusionps4::debug::LogCategory;

namespace fusionps4::runtime {

GuestProcess::GuestProcess() = default;
GuestProcess::~GuestProcess() {
    kill();
}

bool GuestProcess::configure(const isolation::IsolationConfig& iso,
                             std::uintptr_t                     arenaBase,
                             std::size_t                        arenaSize) {
    m_iso = iso;
    if (!m_arena.init(arenaBase, arenaSize)) return false;
    m_ring.initialize();
    return true;
}

void GuestProcess::bindServer(syscall::trap::TrapServer* server) {
    m_server = server;
}

bool GuestProcess::spawn(GuestEntryFn guestEntry, void* arg) {
    if (m_pid > 0) {
        FP4_WARN(LogCategory::Process) << "GuestProcess already spawned";
        return false;
    }

    const pid_t pid = ::fork();
    if (pid < 0) {
        FP4_ERROR(LogCategory::Process)
            << "fork() failed: " << std::strerror(errno);
        return false;
    }

    if (pid == 0) {
        // ---- child ----------------------------------------------------
        childMain(guestEntry, arg, &m_ring, m_iso);
        _exit(127);   // unreachable
    }

    m_pid = pid;
    FP4_INFO(LogCategory::Process)
        << "guest process spawned: pid=" << pid;
    return true;
}

void GuestProcess::childMain(GuestEntryFn guestEntry, void* arg,
                             syscall::trap::TrapRing* ring,
                             isolation::IsolationConfig iso) {
    // ---- 1. Namespaces ----------------------------------------------
    if (!isolation::NamespaceSetup::unshareAll(iso) && iso.abort_on_isolation_failure) {
        FP4_FATAL(LogCategory::Process) << "namespace setup failed; aborting child";
        _exit(1);
    }
    if (!isolation::NamespaceSetup::setHostname(iso) && iso.abort_on_isolation_failure) {
        FP4_FATAL(LogCategory::Process) << "hostname setup failed; aborting child";
        _exit(1);
    }
    if (!isolation::NamespaceSetup::pivotIntoJail(iso) && iso.abort_on_isolation_failure) {
        FP4_FATAL(LogCategory::Process) << "pivot into jail failed; aborting child";
        _exit(1);
    }

    // ---- 2. Trap gate (must happen before seccomp) -------------------
    syscall::trap::TrapGateConfig gateCfg;
    gateCfg.ring     = ring;
    gateCfg.guestTid = 1;
    gateCfg.trace    = iso.trace_syscalls;
    if (!syscall::trap::TrapGate::install(gateCfg)) {
        FP4_FATAL(LogCategory::Process) << "TrapGate install failed";
        _exit(1);
    }

    // ---- 3. Capabilities --------------------------------------------
    if (iso.no_new_privs && !isolation::Capabilities::setNoNewPrivs()
        && iso.abort_on_isolation_failure) {
        FP4_FATAL(LogCategory::Process) << "PR_SET_NO_NEW_PRIVS failed";
        _exit(1);
    }
    if (iso.drop_capabilities && !isolation::Capabilities::dropAll()
        && iso.abort_on_isolation_failure) {
        FP4_FATAL(LogCategory::Process) << "capability drop failed";
        _exit(1);
    }

    // ---- 4. Seccomp -------------------------------------------------
    if (iso.trap_all_syscalls) {
        if (!isolation::SeccompFilter::install(iso)) {
            FP4_FATAL(LogCategory::Process) << "seccomp install failed";
            _exit(1);
        }
    }

    FP4_INFO(LogCategory::Process)
        << "guest child fully isolated; jumping to entry point";

    // ---- 5. Jump to the guest entry point ---------------------------
    guestEntry(arg);
    _exit(0);
}

int GuestProcess::wait() {
    if (m_pid <= 0) return 0;
    int status = 0;
    while (::waitpid(m_pid, &status, 0) < 0) {
        if (errno == EINTR) continue;
        FP4_ERROR(LogCategory::Process)
            << "waitpid(" << m_pid << ") failed: " << std::strerror(errno);
        return -1;
    }
    m_exited.store(true);
    if (WIFEXITED(status)) {
        FP4_INFO(LogCategory::Process)
            << "guest pid=" << m_pid << " exited code=" << WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        FP4_WARN(LogCategory::Process)
            << "guest pid=" << m_pid << " killed by signal "
            << WTERMSIG(status);
    }
    m_pid = -1;
    return status;
}

void GuestProcess::kill() {
    if (m_pid <= 0) return;
    ::kill(m_pid, SIGKILL);
    int status = 0;
    ::waitpid(m_pid, &status, 0);
    m_exited.store(true);
    m_pid = -1;
}

} // namespace fusionps4::runtime
