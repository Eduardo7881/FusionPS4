#include "sce/ajm/SceAjm.hpp"

#include "audio/AjmDecoder.hpp"
#include "debug/Log.hpp"
#include "runtime/RuntimeContext.hpp"
#include "sce/SceStubTable.hpp"

#include <atomic>
#include <condition_variable>
#include <cstring>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>

using fusionps4::debug::LogCategory;
using fusionps4::audio::AjmCodec;
using fusionps4::audio::AjmDecoder;
using fusionps4::audio::AjmPcmResult;

namespace fusionps4::sce::ajm {

SceAjm& SceAjm::instance() {
    static SceAjm s;
    return s;
}

bool SceAjm::initialize() {
    m_initialized = true;
    FP4_INFO(LogCategory::Sce) << "libSceAjm initialized";
    return true;
}

void SceAjm::shutdown() { m_initialized = false; }

namespace {

constexpr int kOk             = 0;
constexpr int kErrInvalidArg  = static_cast<int>(0x80020005u);
constexpr int kErrNotFound    = static_cast<int>(0x80020004u);
constexpr int kErrNotSupport  = static_cast<int>(0x80020003u);
constexpr int kErrNoMemory    = static_cast<int>(0x80020002u);

// ---- job queue --------------------------------------------------------

struct DecodeJob {
    std::uint64_t jobId = 0;
    AjmCodec      codec = AjmCodec::Unknown;
    const void*   input = nullptr;
    std::size_t   inputSize = 0;
    void*         output = nullptr;         // guest PCM buffer (int16)
    std::size_t   outputCapacity = 0;       // in samples
    std::size_t   produced = 0;             // out: samples written
    int           status = 0;               // out: 0 = ok, negative = error
    std::uint64_t callback = 0;             // guest callback pointer, if any
    std::uint64_t callbackArg = 0;
    bool          done = false;
};

std::mutex                              g_mutex;
std::condition_variable                 g_cv;
std::queue<std::shared_ptr<DecodeJob>>  g_pending;
std::thread                             g_worker;
std::atomic<bool>                       g_running{false};
std::atomic<std::uint64_t>              g_nextJobId{1};
std::unordered_map<std::uint64_t, std::shared_ptr<DecodeJob>> g_jobs;

void workerLoop() {
    while (g_running.load(std::memory_order_acquire)) {
        std::shared_ptr<DecodeJob> job;
        {
            std::unique_lock lock(g_mutex);
            g_cv.wait_for(lock, std::chrono::milliseconds(50), [] {
                return !g_pending.empty() ||
                       !g_running.load(std::memory_order_acquire);
            });
            if (!g_running.load(std::memory_order_acquire)) break;
            if (g_pending.empty()) continue;
            job = g_pending.front();
            g_pending.pop();
        }
        if (!job) continue;

        auto decoder = AjmDecoder::create(job->codec);
        if (!decoder) {
            job->status = kErrNotSupport;
            job->done = true;
            FP4_WARN(LogCategory::Audio)
                << "Ajm job " << job->jobId << ": codec unavailable";
            continue;
        }

        AjmPcmResult result;
        if (!decoder->decode(job->input, job->inputSize, result)) {
            job->status = kErrInvalidArg;
            job->done = true;
            continue;
        }

        // Copy into the guest buffer, truncated to capacity.
        const auto samples = std::min(result.samples.size(), job->outputCapacity);
        if (samples > 0 && job->output) {
            std::memcpy(job->output, result.samples.data(),
                        samples * sizeof(std::int16_t));
        }
        job->produced = samples;
        job->status   = 0;
        job->done     = true;

        FP4_DEBUG(LogCategory::Audio)
            << "Ajm job " << job->jobId << " complete: " << samples
            << " samples";
    }
}

} // namespace

namespace {

extern "C" {

int sceAjmInitialize(std::uint64_t /*reserved*/, std::uint64_t* outContext) {
    if (!outContext) return kErrInvalidArg;
    if (!g_running.exchange(true)) {
        g_worker = std::thread(workerLoop);
    }
    *outContext = 1;
    FP4_INFO(LogCategory::Sce) << "sceAjmInitialize";
    return kOk;
}

int sceAjmFinalize(std::uint64_t /*context*/) {
    if (g_running.exchange(false)) {
        g_cv.notify_all();
        if (g_worker.joinable()) g_worker.join();
    }
    return kOk;
}

// sceAjmModuleRegister(context, codecType, flags):
//   codecType values (from OpenOrbis):
//     1 = MP3, 2 = AAC, 3 = ATRAC9, 4 = CELP ...
int sceAjmModuleRegister(std::uint64_t /*context*/, std::uint32_t codecType,
                         std::uint32_t /*flags*/) {
    switch (codecType) {
        case 1: {   // MP3
            auto d = AjmDecoder::create(AjmCodec::Mp3);
            if (!d) {
                FP4_ERROR(LogCategory::Sce)
                    << "sceAjmModuleRegister(MP3): decoder unavailable";
                return kErrNotSupport;
            }
            return kOk;
        }
        case 3: {   // ATRAC9
            FP4_UNIMPLEMENTED(LogCategory::Sce, "sceAjmModuleRegister(AT9)");
            FP4_ERROR(LogCategory::Sce)
                << "  reason=AT9 decoder not implemented";
            return kErrNotSupport;
        }
        default:
            FP4_UNIMPLEMENTED(LogCategory::Sce, "sceAjmModuleRegister");
            FP4_ERROR(LogCategory::Sce)
                << "  reason=codec type " << codecType
                << " is not supported by this runtime";
            return kErrNotSupport;
    }
}

// sceAjmBatchStartBuffer(context, batchPtr, batchSize, outputPtr,
//                        outputSize, callback, callbackArg, outJobId)
//
// The batch's internal structure is complex; we look for the codec tag
// in the first 32 bytes and the input/output pointers in fixed offsets.
// If those don't match our expectation we refuse rather than guess.
int sceAjmBatchStartBuffer(std::uint64_t  /*context*/,
                           const void*    batch,
                           std::size_t    batchSize,
                           std::uint64_t* outJobId,
                           void*          /*callback*/,
                           std::uint64_t  /*callbackArg*/) {
    if (!batch || batchSize < 32 || !outJobId) return kErrInvalidArg;

    // Parse a minimal batch layout: [codec u32][reserved u32]
    // [inputPtr u64][inputSize u64][outputPtr u64][outputCapacity u64]
    struct AjmBatchHeader {
        std::uint32_t codec;
        std::uint32_t reserved;
        std::uint64_t inputPtr;
        std::uint64_t inputSize;
        std::uint64_t outputPtr;
        std::uint64_t outputCapacity;
    };
    if (batchSize < sizeof(AjmBatchHeader)) return kErrInvalidArg;
    AjmBatchHeader h{};
    std::memcpy(&h, batch, sizeof(h));

    AjmCodec codec = AjmCodec::Unknown;
    switch (h.codec) {
        case 1: codec = AjmCodec::Mp3; break;
        case 3: codec = AjmCodec::At9; break;
        default: return kErrNotSupport;
    }

    auto job = std::make_shared<DecodeJob>();
    job->jobId          = g_nextJobId.fetch_add(1);
    job->codec          = codec;
    job->input          = reinterpret_cast<const void*>(
        static_cast<std::uintptr_t>(h.inputPtr));
    job->inputSize      = static_cast<std::size_t>(h.inputSize);
    job->output         = reinterpret_cast<void*>(
        static_cast<std::uintptr_t>(h.outputPtr));
    job->outputCapacity = static_cast<std::size_t>(h.outputCapacity);

    {
        std::lock_guard lock(g_mutex);
        g_jobs[job->jobId] = job;
        g_pending.push(job);
    }
    g_cv.notify_one();

    *outJobId = job->jobId;
    return kOk;
}

int sceAjmBatchWait(std::uint64_t jobId, std::uint32_t /*timeoutUs*/,
                    std::uint32_t* outStatus) {
    std::shared_ptr<DecodeJob> job;
    {
        std::lock_guard lock(g_mutex);
        auto it = g_jobs.find(jobId);
        if (it == g_jobs.end()) return kErrNotFound;
        job = it->second;
    }

    // Busy-wait with a small sleep; AJM jobs are short.
    while (!job->done) {
        std::this_thread::sleep_for(std::chrono::microseconds(200));
    }

    if (outStatus) *outStatus = static_cast<std::uint32_t>(job->status);

    std::lock_guard lock(g_mutex);
    g_jobs.erase(jobId);
    return kOk;
}

int sceAjmBatchJobDecode(std::uint64_t /*context*/, void* /*batch*/) {
    // Some titles build a batch by hand and submit through this entry.
    // We do not decode the batch layout: it is version-dependent and
    // rejecting it is safer than silently misparsing it.
    FP4_UNIMPLEMENTED(LogCategory::Sce, "sceAjmBatchJobDecode");
    FP4_ERROR(LogCategory::Sce)
        << "  reason=hand-built AJM batches use a version-dependent layout; "
        << "use sceAjmBatchStartBuffer with the runtime-provided header instead";
    return kErrNotSupport;
}

} // extern "C"

} // namespace

void SceAjm::registerExports(SceStubTable& t) {
    t.registerStub("libSceAjm", "sceAjmInitialize",
                   reinterpret_cast<void*>(&sceAjmInitialize));
    t.registerStub("libSceAjm", "sceAjmFinalize",
                   reinterpret_cast<void*>(&sceAjmFinalize));
    t.registerStub("libSceAjm", "sceAjmModuleRegister",
                   reinterpret_cast<void*>(&sceAjmModuleRegister));
    t.registerStub("libSceAjm", "sceAjmBatchStartBuffer",
                   reinterpret_cast<void*>(&sceAjmBatchStartBuffer));
    t.registerStub("libSceAjm", "sceAjmBatchWait",
                   reinterpret_cast<void*>(&sceAjmBatchWait));
    t.registerStub("libSceAjm", "sceAjmBatchJobDecode",
                   reinterpret_cast<void*>(&sceAjmBatchJobDecode));
}

} // namespace fusionps4::sce::ajm
