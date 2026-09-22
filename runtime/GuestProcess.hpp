#pragma once

#include "isolation/IsolationConfig.hpp"
#include "isolation/SharedArena.hpp"
#include "syscall/trap/TrapRing.hpp"

#include <atomic>
#include <cstdint>
#include <sys/types.h>

namespace fusionps4::syscall { class SyscallDispatcher; }
namespace fusionps4::syscall::trap { class TrapServer; }

namespace fusionps4::runtime {

// Owns the forked guest process: the child that actually executes PS4 code.
//
// Lifetime:
//   * configure() — reserves the arena and installs the trap gate. Called
//     in the parent, before fork.
//   * spawn()     — forks; the child runs the isolation setup and then
//     jumps into the guest entry point. The parent returns from spawn()
//     and retains the pid.
//   * wait()      — blocks until the child exits and returns its status.
//   * kill()      — forcibly terminates the child (used during shutdown).
class GuestProcess {
public:
    GuestProcess();
    ~GuestProcess();

    GuestProcess(const GuestProcess&) = delete;
    GuestProcess& operator=(const GuestProcess&) = delete;

    // Allocates the shared arena and prepares the trap ring. Must be called
    // before spawn(). Returns false on failure.
    bool configure(const isolation::IsolationConfig& iso,
                   std::uintptr_t                     arenaBase,
                   std::size_t                        arenaSize);

    // Forks the guest. `guestEntry` is the address the child jumps to
    // after isolation is applied; it must be a function pointer valid in
    // the child (inherited from fork, so any parent function qualifies).
    using GuestEntryFn = void(*)(void*);
    bool spawn(GuestEntryFn guestEntry, void* arg);

    // Bind the runtime-side server that services the ring.
    void bindServer(syscall::trap::TrapServer* server);

    // Returns the trap ring that both processes share.
    syscall::trap::TrapRing& ring() { return m_ring; }

    // Shared arena accessor.
    isolation::SharedArena& arena() { return m_arena; }

    pid_t pid() const { return m_pid; }
    bool  running() const { return m_pid > 0 && !m_exited.load(); }

    // Block until the child exits; returns its exit status.
    int wait();

    // Force-kill and reap.
    void kill();

private:
    // Runs in the child after fork, before isolation and before jumping to
    // guestEntry. Returns the (unused) exit code path.
    static void childMain(GuestEntryFn guestEntry, void* arg,
                          syscall::trap::TrapRing* ring,
                          isolation::IsolationConfig iso);

    isolation::IsolationConfig       m_iso;
    isolation::SharedArena           m_arena;
    syscall::trap::TrapRing          m_ring;
    syscall::trap::TrapServer*       m_server = nullptr;

    pid_t                            m_pid = -1;
    std::atomic<bool>                m_exited{false};
};

} // namespace fusionps4::runtime
