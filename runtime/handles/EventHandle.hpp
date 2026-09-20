#pragma once

#include "runtime/handles/HandleObject.hpp"

#include <condition_variable>
#include <cstdint>
#include <mutex>

namespace fusionps4::runtime::handles {

// A kernel event object used by sceKernelCreateEqueue / kevent / etc. In
// this runtime it is a first-class object shared between the guest and the
// runtime's own waiters (input thread, audio mixer, ...).
class EventHandle : public HandleObject {
public:
    EventHandle();
    ~EventHandle() override = default;

    HandleType  type() const override { return HandleType::Event; }
    const char* typeName() const override { return "Event"; }

    // Set the event. `ident` is an arbitrary 64-bit token the guest uses
    // to distinguish event sources.
    void signal(std::uint64_t ident);

    // Wait until the event is signalled or `timeoutNs` elapses.
    // timeoutNs < 0 means "wait forever".
    // Returns true if signalled, false on timeout.
    bool wait(std::uint64_t ident, std::int64_t timeoutNs);

    void clear(std::uint64_t ident);

private:
    mutable std::mutex      m_mutex;
    std::condition_variable m_cv;
    std::uint64_t           m_signaled = 0;
    bool                    m_hasAny   = false;
};

} // namespace fusionps4::runtime::handles
