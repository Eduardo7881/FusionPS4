#include "sce/playgo/ScePlayGo.hpp"

#include "debug/Log.hpp"
#include "sce/SceStubTable.hpp"

#include <cstring>

using fusionps4::debug::LogCategory;

namespace fusionps4::sce::playgo {

ScePlayGo& ScePlayGo::instance() {
    static ScePlayGo s;
    return s;
}

bool ScePlayGo::initialize() {
    m_initialized = true;
    FP4_INFO(LogCategory::Sce)
        << "libScePlayGo initialized (all content reported as installed)";
    return true;
}

void ScePlayGo::shutdown() { m_initialized = false; }

namespace {

constexpr int kOk = 0;
constexpr int kErrInvalidArg = static_cast<int>(0x80020005u);

// scePlayGo state values.
constexpr std::uint32_t kStateInstalled     = 3;   // fully installed
constexpr std::uint32_t kStateNotInstalled  = 0;
constexpr std::uint32_t kStatePartial       = 1;

// "Chunk" info: the PS4 splits a title into chunks that become available
// independently. We report every queried chunk as fully installed, because
// in our model the runtime already has all the files.

extern "C" {

int scePlayGoInitialize(std::uint32_t /*poolSize*/, std::uint32_t /*flags*/) {
    return kOk;
}
int scePlayGoTerminate() { return kOk; }

int scePlayGoGetLocus(void* /*locus*/, void* /*chunkIds*/,
                      std::uint32_t /*n*/, std::uint32_t* outCount) {
    if (outCount) *outCount = 0;
    return kOk;
}

int scePlayGoGetChunkId(std::uint32_t /*chunkIndex*/,
                        std::uint64_t* outChunkId) {
    if (!outChunkId) return kErrInvalidArg;
    // Single-chunk model: the title is one chunk with a fixed id.
    *outChunkId = 0x46503400000001ull;   // "FP4" + 1
    return kOk;
}

int scePlayGoGetInstallSpeed(std::uint32_t* outSpeed) {
    if (outSpeed) *outSpeed = 0;
    return kOk;
}

int scePlayGoGetProgress(std::uint64_t /*chunkId*/,
                         std::uint32_t* outProgress) {
    if (outProgress) *outProgress = 100;
    return kOk;
}

int scePlayGoGetLocusInfo(std::uint64_t /*chunkId*/, void* outInfo) {
    // ScePlayGoLocusInfo: { uint32_t state; uint32_t progressPercent; ... }
    struct Info {
        std::uint32_t state;
        std::uint32_t progressPercent;
        std::uint8_t  reserved[24];
    };
    static_assert(sizeof(Info) == 32, "Info");
    if (!outInfo) return kErrInvalidArg;
    auto* i = static_cast<Info*>(outInfo);
    i->state           = kStateInstalled;
    i->progressPercent = 100;
    std::memset(i->reserved, 0, sizeof(i->reserved));
    return kOk;
}

int scePlayGoGetEta(std::uint64_t /*chunkId*/, std::uint32_t* outEta) {
    if (outEta) *outEta = 0;
    return kOk;
}

int scePlayGoPrefetch(std::uint64_t /*chunkId*/, std::uint64_t /*size*/,
                      std::uint32_t /*flags*/) {
    // Nothing to prefetch: content is already local.
    return kOk;
}

int scePlayGoOpen(void** outHandle) {
    if (outHandle) *outHandle = nullptr;
    return kOk;
}
int scePlayGoClose(void* /*handle*/) { return kOk; }

int scePlayGoSetWakeupCallback(void* /*cb*/, void* /*arg*/) { return kOk; }

} // extern "C"

} // namespace

void ScePlayGo::registerExports(SceStubTable& t) {
    t.registerStub("libScePlayGo", "scePlayGoInitialize",
                   reinterpret_cast<void*>(&scePlayGoInitialize));
    t.registerStub("libScePlayGo", "scePlayGoTerminate",
                   reinterpret_cast<void*>(&scePlayGoTerminate));
    t.registerStub("libScePlayGo", "scePlayGoGetLocus",
                   reinterpret_cast<void*>(&scePlayGoGetLocus));
    t.registerStub("libScePlayGo", "scePlayGoGetChunkId",
                   reinterpret_cast<void*>(&scePlayGoGetChunkId));
    t.registerStub("libScePlayGo", "scePlayGoGetInstallSpeed",
                   reinterpret_cast<void*>(&scePlayGoGetInstallSpeed));
    t.registerStub("libScePlayGo", "scePlayGoGetProgress",
                   reinterpret_cast<void*>(&scePlayGoGetProgress));
    t.registerStub("libScePlayGo", "scePlayGoGetLocusInfo",
                   reinterpret_cast<void*>(&scePlayGoGetLocusInfo));
    t.registerStub("libScePlayGo", "scePlayGoGetEta",
                   reinterpret_cast<void*>(&scePlayGoGetEta));
    t.registerStub("libScePlayGo", "scePlayGoPrefetch",
                   reinterpret_cast<void*>(&scePlayGoPrefetch));
    t.registerStub("libScePlayGo", "scePlayGoOpen",
                   reinterpret_cast<void*>(&scePlayGoOpen));
    t.registerStub("libScePlayGo", "scePlayGoClose",
                   reinterpret_cast<void*>(&scePlayGoClose));
    t.registerStub("libScePlayGo", "scePlayGoSetWakeupCallback",
                   reinterpret_cast<void*>(&scePlayGoSetWakeupCallback));
}

} // namespace fusionps4::sce::playgo
