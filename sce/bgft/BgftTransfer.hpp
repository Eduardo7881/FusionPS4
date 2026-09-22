#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>

namespace fusionps4::sce::bgft {

// A single file-copy job that the runtime performs on behalf of the guest.
// Source and destination are guest paths; the runtime resolves them
// through the process VFS, so the operation cannot escape the sandbox.
//
// This gives us a realistic implementation of sceBgft* and
// sceDownload* without needing any network: the guest asks the runtime to
// copy a file it already has into a location it owns, and the runtime does
// so with progress reporting.
struct Transfer {
    std::uint32_t id       = 0;
    std::string   guestSrc;
    std::string   guestDst;
    std::uint64_t totalBytes = 0;
    std::atomic<std::uint64_t> copiedBytes{0};
    std::atomic<std::uint32_t> state{0};   // 0=pending, 1=running, 2=done, 3=failed
    std::atomic<std::uint32_t> errorCode{0};
    bool          cancelRequested = false;
};

class BgftTransfer {
public:
    static BgftTransfer& instance();

    bool start();
    void stop();

    std::uint32_t submit(const std::string& guestSrc,
                         const std::string& guestDst);

    // Fills the progress for `id`. Returns false if the id is unknown.
    bool query(std::uint32_t id, std::uint64_t* outTotal,
               std::uint64_t* outCopied, std::uint32_t* outState,
               std::uint32_t* outError) const;

    bool cancel(std::uint32_t id);

private:
    BgftTransfer() = default;

    void worker();

    mutable std::mutex                                  m_mutex;
    std::unordered_map<std::uint32_t, std::shared_ptr<Transfer>> m_transfers;
    std::queue<std::shared_ptr<Transfer>>               m_pending;
    std::condition_variable                             m_cv;
    std::thread                                         m_thread;
    std::atomic<bool>                                   m_running{false};
    std::atomic<std::uint32_t>                          m_nextId{1};
};

} // namespace fusionps4::sce::bgft
