#include "sce/fiber/SceFiber.hpp"

#include "debug/Log.hpp"
#include "runtime/fiber/Fiber.hpp"
#include "sce/SceStubTable.hpp"

#include <cstring>
#include <memory>
#include <mutex>
#include <unordered_map>

using fusionps4::debug::LogCategory;
using fusionps4::runtime::fiber::Fiber;
using fusionps4::runtime::fiber::ThreadFiberState;
using fusionps4::runtime::fiber::currentThreadState;

namespace fusionps4::sce::fiber {

SceFiber& SceFiber::instance() {
    static SceFiber s;
    return s;
}

bool SceFiber::initialize() {
    m_initialized = true;
    FP4_INFO(LogCategory::Sce) << "libSceFiber initialized";
    return true;
}

void SceFiber::shutdown() {
    // Fiber instances are per-guest-object and released by sceFiberFinalize.
    m_initialized = false;
}

namespace {

constexpr int kOk              = 0;
constexpr int kErrInvalidArg   = static_cast<int>(0x80020005u);
constexpr int kErrNotFound     = static_cast<int>(0x80020004u);
constexpr int kErrBusy         = static_cast<int>(0x8002000Bu);
constexpr int kErrFault        = static_cast<int>(0x8002000Du);

// Side table: guest SceFiber pointer → runtime Fiber instance.
struct FiberEntry {
    std::unique_ptr<Fiber> impl;
    void*                  guestPtr = nullptr;
};

std::mutex                                       g_mutex;
std::unordered_map<void*, FiberEntry>            g_fibers;

Fiber* lookup(void* guestPtr) {
    std::lock_guard lock(g_mutex);
    auto it = g_fibers.find(guestPtr);
    return it == g_fibers.end() ? nullptr : it->second.impl.get();
}

// Guest entry point for a fiber. On PS4 the signature is:
//     void entry(uint64_t arg);
// though the actual calling convention is that the argument is passed in
// %rdi and the entry may return (in which case the fiber terminates).
using GuestFiberEntry = void(*)(std::uint64_t);

extern "C" {

int sceFiberInitializeImpl(void*              fiber,
                           const char*        name,
                           GuestFiberEntry    entry,
                           std::uint64_t      arg,
                           void*              addr,
                           std::uint32_t      size,
                           void*              /*opt*/) {
    if (!fiber || !entry || !addr || size < 4096) return kErrInvalidArg;

    std::lock_guard lock(g_mutex);
    if (g_fibers.count(fiber)) return kErrBusy;

    auto impl = std::make_unique<Fiber>();

    // The Fiber::EntryFn signature is `int(void*)`. The guest's real
    // signature is `void(uint64_t)`. We bridge with a closure-like shim:
    // the shim takes the 64-bit arg as a pointer value and calls the guest
    // entry with it. This is ABI-correct on x86_64 SysV.
    struct Shim {
        GuestFiberEntry guestEntry;
        std::uint64_t   arg;
    };
    // We cannot return a closure from a C linkage function, so we store
    // the shim inside the Fiber's user data via a heap allocation the
    // Fiber owns. Simpler: capture in a static map keyed by Fiber*.
    //
    // Because we already hold g_mutex and the fiber will not run until
    // sceFiberRun, we can use a second table for shims.
    static std::unordered_map<Fiber*, Shim> g_shims;
    g_shims[impl.get()] = { entry, arg };

    auto trampoline = [](void* ctx) -> int {
        auto* self = static_cast<Fiber*>(ctx);
        // Read the shim.
        auto& shim = g_shims[self];
        shim.guestEntry(shim.arg);
        return 0;
    };
    // Fiber::EntryFn is int(*)(void*); a non-capturing lambda converts.
    // The lambda above is non-capturing (it uses static tables), so this
    // conversion is well-formed.

    const std::string nm = name ? name : "";
    if (!impl->init(nm, trampoline, impl.get(),
                    addr, static_cast<std::size_t>(size))) {
        return kErrInvalidArg;
    }

    FiberEntry fe;
    fe.guestPtr = fiber;
    fe.impl     = std::move(impl);
    g_fibers.emplace(fiber, std::move(fe));

    FP4_DEBUG(LogCategory::Sce)
        << "sceFiberInitializeImpl(\"" << nm << "\") fiber=" << fiber;
    return kOk;
}

int sceFiberFinalize(void* fiber) {
    if (!fiber) return kErrInvalidArg;
    std::lock_guard lock(g_mutex);
    auto it = g_fibers.find(fiber);
    if (it == g_fibers.end()) return kErrNotFound;
    g_fibers.erase(it);
    return kOk;
}

int sceFiberRun(void* fiber, std::uint64_t arg, std::uint64_t* out) {
    Fiber* impl = lookup(fiber);
    if (!impl) return kErrNotFound;

    ThreadFiberState& ts = currentThreadState();
    if (ts.inFiber) return kErrBusy;

    ts.running = impl;
    ts.inFiber = true;

    // Switch to the fiber. When the fiber yields, control returns here.
    ::swapcontext(&ts.threadCtx, &impl->context());

    ts.inFiber = false;
    ts.running = nullptr;

    if (out) *out = arg;   // PS4 passes the yield arg back through out
    return kOk;
}

int sceFiberSwitch(void* fiber, std::uint64_t arg, std::uint64_t* out) {
    Fiber* impl = lookup(fiber);
    if (!impl) return kErrNotFound;

    ThreadFiberState& ts = currentThreadState();
    if (!ts.inFiber || !ts.running) {
        // Switching from the thread context: equivalent to sceFiberRun.
        return sceFiberRun(fiber, arg, out);
    }

    Fiber* prev = ts.running;
    ts.running = impl;
    // Swap directly between fibers; the thread context is not involved.
    ::swapcontext(&prev->context(), &impl->context());
    if (out) *out = arg;
    return kOk;
}

int sceFiberReturnToThread(std::uint64_t arg, std::uint64_t* out) {
    ThreadFiberState& ts = currentThreadState();
    if (!ts.inFiber || !ts.running) return kErrFault;

    Fiber* current = ts.running;
    ts.inFiber = false;
    ts.running = nullptr;
    ::swapcontext(&current->context(), &ts.threadCtx);

    if (out) *out = arg;
    return kOk;
}

int sceFiberGetSelf(void** out) {
    if (!out) return kErrInvalidArg;
    ThreadFiberState& ts = currentThreadState();
    if (!ts.inFiber) { *out = nullptr; return kOk; }
    *out = ts.running;   // não é o ponteiro guest, mas o runtime usa por baixo
    return kOk;
}

} // extern "C"

} // namespace

void SceFiber::registerExports(SceStubTable& t) {
    t.registerStub("libSceFiber", "sceFiberInitializeImpl",
                   reinterpret_cast<void*>(&sceFiberInitializeImpl));
    t.registerStub("libSceFiber", "sceFiberInitialize",
                   reinterpret_cast<void*>(&sceFiberInitializeImpl));
    t.registerStub("libSceFiber", "sceFiberFinalize",
                   reinterpret_cast<void*>(&sceFiberFinalize));
    t.registerStub("libSceFiber", "sceFiberRun",
                   reinterpret_cast<void*>(&sceFiberRun));
    t.registerStub("libSceFiber", "sceFiberSwitch",
                   reinterpret_cast<void*>(&sceFiberSwitch));
    t.registerStub("libSceFiber", "sceFiberReturnToThread",
                   reinterpret_cast<void*>(&sceFiberReturnToThread));
    t.registerStub("libSceFiber", "sceFiberGetSelf",
                   reinterpret_cast<void*>(&sceFiberGetSelf));
}

} // namespace fusionps4::sce::fiber
