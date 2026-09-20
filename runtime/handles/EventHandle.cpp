#include "runtime/handles/EventHandle.hpp"

#include <chrono>

namespace fusionps4::runtime::handles {

EventHandle::EventHandle() = default;

void EventHandle::signal(std::uint64_t ident) {
    {
        std::lock_guard lock(m_mutex);
        m_signaled |= (1ull << (ident & 63));
        m_hasAny    = true;
    }
    m_cv.notify_all();
}

bool EventHandle::wait(std::uint64_t ident, std::int64_t timeoutNs) {
    std::unique_lock lock(m_mutex);
    const std::uint64_t bit = 1ull << (ident & 63);

    auto pred = [&] {
        return m_hasAny && (m_signaled & bit);
    };

    if (timeoutNs < 0) {
        m_cv.wait(lock, pred);
        return true;
    }

    using namespace std::chrono;
    return m_cv.wait_for(lock, nanoseconds(timeoutNs), pred);
}

void EventHandle::clear(std::uint64_t ident) {
    std::lock_guard lock(m_mutex);
    m_signaled &= ~(1ull << (ident & 63));
    if (m_signaled == 0) m_hasAny = false;
}

} // namespace fusionps4::runtime::handles
