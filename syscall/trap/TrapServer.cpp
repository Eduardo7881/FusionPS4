#include "syscall/trap/TrapServer.hpp"

#include "debug/Log.hpp"
#include "runtime/RuntimeContext.hpp"
#include "runtime/process/PS4Process.hpp"
#include "runtime/thread/ThreadObject.hpp"

using fusionps4::debug::LogCategory;

namespace fusionps4::syscall::trap {

TrapServer::TrapServer(SyscallDispatcher& syscalls, SceInvoker sceInvoker)
    : m_syscalls(syscalls), m_sceInvoker(std::move(sceInvoker)) {}

TrapServer::~TrapServer() {
    stop();
}

void TrapServer::bindRing(TrapRing* ring) {
    m_ring = ring;
}

bool TrapServer::start() {
    if (m_thread.joinable()) return true;
    if (!m_ring) {
        FP4_ERROR(LogCategory::Syscall)
            << "TrapServer::start without a bound ring";
        return false;
    }
    m_running.store(true);
    m_thread = std::thread([this] { run(); });
    FP4_INFO(LogCategory::Syscall) << "TrapServer started";
    return true;
}

void TrapServer::stop() {
    if (!m_running.exchange(false)) {
        if (m_thread.joinable()) m_thread.join();
        return;
    }
    if (m_ring) m_ring->requestShutdown();
    if (m_thread.joinable()) m_thread.join();
    FP4_INFO(LogCategory::Syscall) << "TrapServer stopped";
}

void TrapServer::run() {
    while (m_running.load(std::memory_order_acquire)) {
        // Wait for work with a short timeout so we can observe shutdown.
        if (!m_ring->waitForWork(50 * 1000 * 1000)) continue;

        // Drain whatever is pending.
        TrapRequest req;
        std::uint32_t slot = 0;
        while (m_ring->tryConsume(req, slot)) {
            TrapResponse resp{};

            auto* proc = runtime::RuntimeContext::instance().process();
            if (req.kind == 1) {
                // SCE call, serialized with the main thread.
                std::lock_guard lock(m_sceMutex);
                bool isError = false;
                const auto v = m_sceInvoker(
                    static_cast<std::uint32_t>(req.number),
                    req.args, &isError);
                resp.retval  = v;
                resp.isError = isError ? 1u : 0u;
            } else {
                // Real syscall. Build a SyscallContext and dispatch.
                syscall::SyscallContext ctx;
                ctx.number  = static_cast<std::int64_t>(req.number);
                for (int i = 0; i < 6; ++i) ctx.args[i] = req.args[i];
                ctx.process = proc;
                ctx.thread  = nullptr;   // resolved below if needed
                if (proc && req.threadId) {
                    ctx.thread = proc->threadManager().get(req.threadId).get();
                }
                m_syscalls.dispatch(ctx);
                resp.retval  = ctx.retval;
                resp.isError = ctx.isError ? 1u : 0u;
            }

            m_ring->complete(slot, resp);
        }
    }
}

} // namespace fusionps4::syscall::trap
