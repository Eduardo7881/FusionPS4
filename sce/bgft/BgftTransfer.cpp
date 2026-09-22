#include "sce/bgft/BgftTransfer.hpp"

#include "debug/Log.hpp"
#include "runtime/RuntimeContext.hpp"
#include "runtime/process/PS4Process.hpp"

#include <chrono>
#include <fstream>

using fusionps4::debug::LogCategory;
using fusionps4::runtime::RuntimeContext;
namespace pol = fusionps4::filesystem::policy;

namespace fusionps4::sce::bgft {

BgftTransfer& BgftTransfer::instance() {
    static BgftTransfer t;
    return t;
}

bool BgftTransfer::start() {
    if (m_running.exchange(true)) return true;
    m_thread = std::thread([this] { worker(); });
    FP4_INFO(LogCategory::Fs) << "BgftTransfer worker started";
    return true;
}

void BgftTransfer::stop() {
    if (!m_running.exchange(false)) return;
    m_cv.notify_all();
    if (m_thread.joinable()) m_thread.join();
}

std::uint32_t BgftTransfer::submit(const std::string& guestSrc,
                                   const std::string& guestDst) {
    auto t = std::make_shared<Transfer>();
    t->id       = m_nextId.fetch_add(1);
    t->guestSrc = guestSrc;
    t->guestDst = guestDst;

    {
        std::lock_guard lock(m_mutex);
        m_transfers[t->id] = t;
        m_pending.push(t);
    }
    m_cv.notify_one();
    return t->id;
}

bool BgftTransfer::query(std::uint32_t id, std::uint64_t* outTotal,
                         std::uint64_t* outCopied, std::uint32_t* outState,
                         std::uint32_t* outError) const {
    std::lock_guard lock(m_mutex);
    auto it = m_transfers.find(id);
    if (it == m_transfers.end()) return false;
    const auto& t = it->second;
    if (outTotal)  *outTotal  = t->totalBytes;
    if (outCopied) *outCopied = t->copiedBytes.load();
    if (outState)  *outState  = t->state.load();
    if (outError)  *outError  = t->errorCode.load();
    return true;
}

bool BgftTransfer::cancel(std::uint32_t id) {
    std::lock_guard lock(m_mutex);
    auto it = m_transfers.find(id);
    if (it == m_transfers.end()) return false;
    it->second->cancelRequested = true;
    return true;
}

void BgftTransfer::worker() {
    while (m_running.load(std::memory_order_acquire)) {
        std::shared_ptr<Transfer> job;
        {
            std::unique_lock lock(m_mutex);
            m_cv.wait_for(lock, std::chrono::milliseconds(50), [this] {
                return !m_pending.empty() ||
                       !m_running.load(std::memory_order_acquire);
            });
            if (!m_running.load(std::memory_order_acquire)) break;
            if (m_pending.empty()) continue;
            job = m_pending.front();
            m_pending.pop();
        }
        if (!job) continue;

        job->state.store(1);   // running

        auto* proc = RuntimeContext::instance().process();
        if (!proc) {
            job->state.store(3);
            job->errorCode.store(0x80020005u);
            continue;
        }

        std::int64_t perr = 0;
        const auto src = proc->virtualFileSystem().resolve(
            job->guestSrc, pol::FsOp::Read, perr);
        if (!src.ok) {
            FP4_WARN(LogCategory::Fs)
                << "BgftTransfer: source rejected \"" << job->guestSrc << "\"";
            job->state.store(3);
            job->errorCode.store(0x80020004u);
            continue;
        }

        const auto dst = proc->virtualFileSystem().resolve(
            job->guestDst, pol::FsOp::Create, perr);
        if (!dst.ok) {
            FP4_WARN(LogCategory::Fs)
                << "BgftTransfer: destination rejected \"" << job->guestDst << "\"";
            job->state.store(3);
            job->errorCode.store(0x80020004u);
            continue;
        }

        // Ensure parent directory exists.
        std::string acc;
        for (std::size_t i = 0; i < dst.hostPath.size(); ++i) {
            acc.push_back(dst.hostPath[i]);
            if (dst.hostPath[i] == '/') ::mkdir(acc.c_str(), 0755);
        }

        std::ifstream in(src.hostPath, std::ios::binary | std::ios::ate);
        if (!in) {
            job->state.store(3);
            job->errorCode.store(0x80020004u);
            continue;
        }
        job->totalBytes = static_cast<std::uint64_t>(in.tellg());
        in.seekg(0);

        std::ofstream out(dst.hostPath, std::ios::binary | std::ios::trunc);
        if (!out) {
            job->state.store(3);
            job->errorCode.store(0x80020002u);
            continue;
        }

        constexpr std::size_t kChunk = 256 * 1024;
        std::vector<char> buf(kChunk);
        while (in) {
            if (job->cancelRequested) {
                job->state.store(3);
                job->errorCode.store(0x8002000Cu);   // cancelled
                break;
            }
            in.read(buf.data(), static_cast<std::streamsize>(buf.size()));
            const auto got = in.gcount();
            if (got > 0) out.write(buf.data(), got);
            job->copiedBytes.fetch_add(static_cast<std::uint64_t>(got));
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        out.close();

        if (!job->cancelRequested) {
            job->state.store(2);
            FP4_INFO(LogCategory::Fs)
                << "BgftTransfer " << job->id << " complete: "
                << job->guestSrc << " -> " << job->guestDst
                << " (" << job->totalBytes << " bytes)";
        }
    }
}

} // namespace fusionps4::sce::bgft
