#pragma once

#include "syscall/SyscallDispatcher.hpp"
#include "syscall/trap/TrapRing.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>

namespace fusionps4::sce { class SceStubTable; }

namespace fusionps4::syscall::trap {

// Runtime-side server. Runs on a dedicated thread, consumes requests from
// the shared TrapRing, and dispatches them to either the SyscallDispatcher
// (for real syscalls) or a caller-provided SCE dispatcher.
//
// All SCE calls are serialized with a mutex because they may touch the
// GraphicsDevice, HostWindow, InputManager, etc., which are also used by
// the runtime's main thread. Real syscalls are dispatched without this
// mutex because they go through their own synchronization (AddressSpace,
// HandleTable) which is already thread-safe.
class TrapServer {
public:
    using SceInvoker = std::function<std::int64_t(
        std::uint32_t id, const std::uint64_t args[6], bool* outIsError)>;

    TrapServer(SyscallDispatcher&  syscalls,
               SceInvoker          sceInvoker);
    ~TrapServer();

    TrapServer(const TrapServer&) = delete;
    TrapServer& operator=(const TrapServer&) = delete;

    // Bind the ring that lives in the shared arena. Must be called before
    // start().
    void bindRing(TrapRing* ring);

    bool start();
    void stop();

private:
    void run();

    SyscallDispatcher& m_syscalls;
    SceInvoker         m_sceInvoker;
    TrapRing*          m_ring = nullptr;

    std::thread        m_thread;
    std::atomic<bool>  m_running{false};

    std::mutex         m_sceMutex;
};

} // namespace fusionps4::syscall::trap
