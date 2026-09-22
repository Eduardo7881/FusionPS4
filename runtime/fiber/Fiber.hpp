#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <ucontext.h>

namespace fusionps4::runtime::fiber {

// A single guest fiber. The guest allocates the fiber object and its
// stack; we keep a side table entry that holds the ucontext and metadata,
// so we never have to know the SceFiber struct layout used by a specific
// SDK version.
class Fiber {
public:
    using EntryFn = int(*)(void*);   // PS4 fiber entry: int entry(void* arg)

    Fiber();
    ~Fiber();

    Fiber(const Fiber&) = delete;
    Fiber& operator=(const Fiber&) = delete;

    // Configure the fiber with a guest-provided stack. `stackBase` and
    // `stackSize` must be inside the shared arena (the guest's memory),
    // which is what makes it possible for the runtime to run the fiber's
    // code directly.
    bool init(std::string    name,
              EntryFn        entry,
              void*          entryArg,
              void*          stackBase,
              std::size_t    stackSize);

    bool valid() const { return m_initialized; }

    const std::string& name() const { return m_name; }
    bool finished() const { return m_finished; }

    // Trampoline body — invoked by ucontext as the fiber's entry point.
    void run();

    ucontext_t& context() { return m_ctx; }

private:
    std::string  m_name;
    EntryFn      m_entry    = nullptr;
    void*        m_entryArg = nullptr;
    void*        m_stackBase= nullptr;
    std::size_t  m_stackSize= 0;
    ucontext_t   m_ctx{};
    bool         m_initialized = false;
    bool         m_finished    = false;
};

// Per-thread state: the fiber that the current guest thread is running, if
// any, and the context to return to when the fiber yields back to the
// thread.
struct ThreadFiberState {
    ucontext_t   threadCtx{};
    Fiber*       running = nullptr;
    bool         inFiber = false;
};

ThreadFiberState& currentThreadState();

} // namespace fusionps4::runtime::fiber
