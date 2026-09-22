#include "runtime/fiber/Fiber.hpp"

#include "debug/Log.hpp"

#include <cstring>

using fusionps4::debug::LogCategory;

namespace fusionps4::runtime::fiber {

namespace {

// The trampoline needs a way to find the Fiber instance it belongs to.
// makecontext() only passes integers, so we stash the current fiber in a
// thread-local before swapcontext'ing to it. This is safe because the
// swap happens synchronously from Fiber::startOnCurrentThread below.
thread_local Fiber* g_pendingFiber = nullptr;

void fiberTrampoline() {
    Fiber* f = g_pendingFiber;
    g_pendingFiber = nullptr;
    if (!f) return;
    f->run();
    // run() never returns; the fiber yields back to its owner.
}

} // namespace

Fiber::Fiber() = default;

Fiber::~Fiber() = default;

bool Fiber::init(std::string name, EntryFn entry, void* entryArg,
                 void* stackBase, std::size_t stackSize) {
    if (m_initialized) return false;
    if (!entry || !stackBase || stackSize < 4096) return false;

    m_name      = std::move(name);
    m_entry     = entry;
    m_entryArg  = entryArg;
    m_stackBase = stackBase;
    m_stackSize = stackSize;

    if (::getcontext(&m_ctx) != 0) {
        FP4_ERROR(LogCategory::Thread)
            << "getcontext failed for fiber \"" << m_name << "\"";
        return false;
    }
    m_ctx.uc_stack.ss_sp   = stackBase;
    m_ctx.uc_stack.ss_size = stackSize;
    m_ctx.uc_link          = nullptr;   // fibers always yield explicitly

    // Attach the trampoline. makecontext requires int args on this ABI;
    // we pass none and rely on the thread-local above.
    ::makecontext(&m_ctx, reinterpret_cast<void(*)()>(&fiberTrampoline), 0);

    m_initialized = true;
    m_finished    = false;
    FP4_DEBUG(LogCategory::Thread)
        << "fiber \"" << m_name << "\" initialized with stack "
        << stackBase << " size " << stackSize;
    return true;
}

void Fiber::run() {
    // Executed on the fiber's own stack. Calls into the guest's entry
    // point directly; the guest's code is in the shared arena and its
    // address space is identical to ours, so no marshalling is needed.
    FP4_TRACE(LogCategory::Thread) << "fiber \"" << m_name << "\" entered";

    const int rc = m_entry(m_entryArg);
    (void)rc;

    m_finished = true;
    FP4_TRACE(LogCategory::Thread) << "fiber \"" << m_name << "\" returned";

    // Yield back to the owning thread. Because uc_link is null, we cannot
    // rely on the kernel to jump; we must swap explicitly.
    ThreadFiberState& ts = currentThreadState();
    if (ts.inFiber) {
        ts.inFiber = false;
        ts.running = nullptr;
        ::swapcontext(&m_ctx, &ts.threadCtx);
    }
    // Unreachable.
}

ThreadFiberState& currentThreadState() {
    thread_local ThreadFiberState s;
    return s;
}

} // namespace fusionps4::runtime::fiber
