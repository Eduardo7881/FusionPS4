#include "syscall/trap/TrapRing.hpp"

#include "debug/Log.hpp"

#include <cerrno>
#include <chrono>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>

using fusionps4::debug::LogCategory;

namespace fusionps4::syscall::trap {

namespace {

int futexWait(std::atomic<std::uint32_t>* addr, std::uint32_t expected,
              std::int64_t timeoutNs) {
    struct timespec ts{};
    struct timespec* tsp = nullptr;
    if (timeoutNs >= 0) {
        ts.tv_sec  = timeoutNs / 1'000'000'000LL;
        ts.tv_nsec = timeoutNs % 1'000'000'000LL;
        tsp = &ts;
    }
    return static_cast<int>(::syscall(
        SYS_futex, reinterpret_cast<std::uint32_t*>(addr),
        FUTEX_WAIT | FUTEX_PRIVATE_FLAG, expected, tsp, nullptr, 0));
}

void futexWake(std::atomic<std::uint32_t>* addr, int count) {
    ::syscall(SYS_futex, reinterpret_cast<std::uint32_t*>(addr),
              FUTEX_WAKE | FUTEX_PRIVATE_FLAG, count, nullptr, nullptr, 0);
}

} // namespace

void TrapRing::initialize() {
    for (auto& s : m_header.slots) {
        s.state.store(static_cast<std::uint32_t>(SlotState::Free),
                      std::memory_order_relaxed);
        s.guestWaitFutex.store(0, std::memory_order_relaxed);
    }
    m_header.serverFutex.store(0, std::memory_order_relaxed);
    m_header.shutdown.store(0, std::memory_order_relaxed);
}

std::uint32_t TrapRing::claimSlot() {
    // Linear scan with CAS; the ring is small and contention is low.
    for (std::uint32_t i = 0; i < kTrapSlotCount; ++i) {
        std::uint32_t expected =
            static_cast<std::uint32_t>(SlotState::Free);
        if (m_header.slots[i].state.compare_exchange_strong(
                expected, static_cast<std::uint32_t>(SlotState::Pending),
                std::memory_order_acq_rel,
                std::memory_order_relaxed)) {
            return i;
        }
    }
    return UINT32_MAX;
}

void TrapRing::releaseSlot(std::uint32_t index) {
    if (index >= kTrapSlotCount) return;
    m_header.slots[index].state.store(
        static_cast<std::uint32_t>(SlotState::Free),
        std::memory_order_release);
}

TrapResponse TrapRing::submit(const TrapRequest& req) {
    std::uint32_t slot;
    for (;;) {
        slot = claimSlot();
        if (slot != UINT32_MAX) break;
        // All slots busy. Yield briefly and retry.
        ::usleep(50);
    }

    auto& s = m_header.slots[slot];
    s.req = req;
    s.resp = TrapResponse{};
    s.guestWaitFutex.store(0, std::memory_order_relaxed);

    // Wake the server thread.
    m_header.serverFutex.fetch_add(1, std::memory_order_release);
    futexWake(&m_header.serverFutex, 1);

    // Block until the server has written the response.
    for (;;) {
        const std::uint32_t state = s.state.load(std::memory_order_acquire);
        if (state == static_cast<std::uint32_t>(SlotState::Done)) break;
        futexWait(&s.guestWaitFutex, 0, -1);
    }

    TrapResponse r = s.resp;
    releaseSlot(slot);
    return r;
}

bool TrapRing::tryConsume(TrapRequest& outReq, std::uint32_t& outSlotIndex) {
    for (std::uint32_t i = 0; i < kTrapSlotCount; ++i) {
        auto& s = m_header.slots[i];
        if (s.state.load(std::memory_order_acquire) ==
            static_cast<std::uint32_t>(SlotState::Pending)) {
            outReq = s.req;
            outSlotIndex = i;
            return true;
        }
    }
    return false;
}

void TrapRing::complete(std::uint32_t slotIndex, const TrapResponse& resp) {
    if (slotIndex >= kTrapSlotCount) return;
    auto& s = m_header.slots[slotIndex];
    s.resp = resp;
    s.state.store(static_cast<std::uint32_t>(SlotState::Done),
                  std::memory_order_release);
    // Wake the guest thread waiting on this slot.
    s.guestWaitFutex.fetch_add(1, std::memory_order_release);
    futexWake(&s.guestWaitFutex, 1);
}

void TrapRing::wakeServer() {
    m_header.serverFutex.fetch_add(1, std::memory_order_release);
    futexWake(&m_header.serverFutex, 1);
}

bool TrapRing::waitForWork(std::int64_t timeoutNs) {
    if (m_header.shutdown.load(std::memory_order_acquire)) return false;

    const std::uint32_t current =
        m_header.serverFutex.load(std::memory_order_acquire);
    futexWait(&m_header.serverFutex, current, timeoutNs);

    return !m_header.shutdown.load(std::memory_order_acquire);
}

void TrapRing::requestShutdown() {
    m_header.shutdown.store(1, std::memory_order_release);
    futexWake(&m_header.serverFutex, INT32_MAX);
}

} // namespace fusionps4::syscall::trap
