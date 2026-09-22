#pragma once

#include <array>
#include <atomic>
#include <cstdint>

// A shared ring used to forward syscall/SCE requests from a guest thread
// (in the child process) to the runtime (in the parent). The ring lives in
// the SharedArena and is reachable at the same VA from both processes.
//
// Design choices:
//   * Fixed number of slots, one request per slot.
//   * Producers (guest threads) claim a free slot by CAS'ing the slot's
//     state from Free to Pending. If no slot is free, the producer spins.
//   * The runtime's TrapServer thread scans slots, processes Pending ones,
//     writes the response, and sets state to Done.
//   * Communication uses two futexes: one for waking the server, one for
//     waking a waiting guest thread per slot.
//
// Memory ordering is enforced with std::atomic: state is the synchronization
// point, and the request/response payloads are written before the state
// store (release) and read after the state load (acquire).

namespace fusionps4::syscall::trap {

struct TrapRequest {
    std::uint64_t number = 0;
    std::uint64_t args[6] = {0, 0, 0, 0, 0, 0};
    std::uint32_t threadId = 0;   // guest thread id, 0 if unknown
    std::uint32_t kind = 0;       // 0 = syscall, 1 = SCE
};

struct TrapResponse {
    std::int64_t  retval  = 0;
    std::uint32_t isError = 0;    // 1 if retval is a FreeBSD errno
    std::uint32_t pad     = 0;
};

enum class SlotState : std::uint32_t {
    Free    = 0,
    Pending = 1,
    Done    = 2,
};

struct alignas(64) TrapSlot {
    std::atomic<std::uint32_t> state{static_cast<std::uint32_t>(SlotState::Free)};
    std::uint32_t              pad0 = 0;
    TrapRequest                req;
    TrapResponse               resp;
    std::atomic<std::uint32_t> guestWaitFutex{0};
    std::uint32_t              pad1 = 0;
};

inline constexpr std::size_t kTrapSlotCount = 256;

struct alignas(64) TrapRingHeader {
    std::atomic<std::uint32_t> serverFutex{0};
    std::uint32_t              pad0 = 0;
    std::atomic<std::uint32_t> shutdown{0};
    std::uint32_t              pad1 = 0;
    TrapSlot                   slots[kTrapSlotCount];
};

class TrapRing {
public:
    TrapRing() = default;

    void initialize();

    // Called by guest threads. Blocks until the runtime has produced a
    // response; returns the response.
    TrapResponse submit(const TrapRequest& req);

    // Called by the runtime's TrapServer thread. Returns true and populates
    // *out if a slot was pending; otherwise returns false immediately.
    bool tryConsume(TrapRequest& outReq, std::uint32_t& outSlotIndex);

    // Called by the runtime after processing a request.
    void complete(std::uint32_t slotIndex, const TrapResponse& resp);

    // Wake the server thread (called by guest producers).
    void wakeServer();

    // Wait for the server to have work. Called by the TrapServer thread.
    // Returns false if shutdown was requested.
    bool waitForWork(std::int64_t timeoutNs);

    // Ask the server to stop.
    void requestShutdown();

    TrapRingHeader& header() { return m_header; }

private:
    TrapRingHeader m_header;

    std::uint32_t claimSlot();
    void releaseSlot(std::uint32_t index);
};

} // namespace fusionps4::syscall::trap
