#include "sce/gameupdate/SceGameUpdate.hpp"

#include "debug/Log.hpp"
#include "sce/SceStubTable.hpp"

#include <cstring>

using fusionps4::debug::LogCategory;

namespace fusionps4::sce::gameupdate {

SceGameUpdate& SceGameUpdate::instance() {
    static SceGameUpdate s;
    return s;
}

bool SceGameUpdate::initialize() {
    m_initialized = true;
    FP4_INFO(LogCategory::Sce)
        << "libSceGameUpdate initialized (no update server; "
        << "all queries report \"no update available\")";
    return true;
}

void SceGameUpdate::shutdown() { m_initialized = false; }

namespace {

constexpr int kOk = 0;
constexpr int kErrInvalidArg = static_cast<int>(0x80020005u);

// sceGameUpdate status values.
constexpr std::uint32_t kStatusIdle       = 0;
constexpr std::uint32_t kStatusChecking   = 1;
constexpr std::uint32_t kStatusNoUpdate   = 2;
constexpr std::uint32_t kStatusUpdateFound = 3;

extern "C" {

int sceGameUpdateInitialize() { return kOk; }
int sceGameUpdateTerminate() { return kOk; }

int sceGameUpdateCreateRequest(std::uint32_t* outReqId) {
    if (!outReqId) return kErrInvalidArg;
    static std::uint32_t next = 1;
    *outReqId = next++;
    return kOk;
}

int sceGameUpdateDeleteRequest(std::uint32_t /*reqId*/) { return kOk; }

int sceGameUpdateCheckUpdate(std::uint32_t /*reqId*/,
                             std::uint32_t /*flags*/,
                             std::uint32_t* outStatus) {
    if (outStatus) *outStatus = kStatusNoUpdate;
    FP4_DEBUG(LogCategory::Sce)
        << "sceGameUpdateCheckUpdate: no update available (offline runtime)";
    return kOk;
}

int sceGameUpdateGetStatus(std::uint32_t /*reqId*/,
                           std::uint32_t* outStatus) {
    if (outStatus) *outStatus = kStatusIdle;
    return kOk;
}

int sceGameUpdateAbort(std::uint32_t /*reqId*/) { return kOk; }

int sceGameUpdateGetUpdateInfo(std::uint32_t /*reqId*/, void* outInfo) {
    if (!outInfo) return kErrInvalidArg;
    // SceGameUpdateInfo: 32 bytes of zeros (no update).
    std::memset(outInfo, 0, 32);
    return kOk;
}

} // extern "C"

} // namespace

void SceGameUpdate::registerExports(SceStubTable& t) {
    t.registerStub("libSceGameUpdate", "sceGameUpdateInitialize",
                   reinterpret_cast<void*>(&sceGameUpdateInitialize));
    t.registerStub("libSceGameUpdate", "sceGameUpdateTerminate",
                   reinterpret_cast<void*>(&sceGameUpdateTerminate));
    t.registerStub("libSceGameUpdate", "sceGameUpdateCreateRequest",
                   reinterpret_cast<void*>(&sceGameUpdateCreateRequest));
    t.registerStub("libSceGameUpdate", "sceGameUpdateDeleteRequest",
                   reinterpret_cast<void*>(&sceGameUpdateDeleteRequest));
    t.registerStub("libSceGameUpdate", "sceGameUpdateCheckUpdate",
                   reinterpret_cast<void*>(&sceGameUpdateCheckUpdate));
    t.registerStub("libSceGameUpdate", "sceGameUpdateGetStatus",
                   reinterpret_cast<void*>(&sceGameUpdateGetStatus));
    t.registerStub("libSceGameUpdate", "sceGameUpdateAbort",
                   reinterpret_cast<void*>(&sceGameUpdateAbort));
    t.registerStub("libSceGameUpdate", "sceGameUpdateGetUpdateInfo",
                   reinterpret_cast<void*>(&sceGameUpdateGetUpdateInfo));
}

} // namespace fusionps4::sce::gameupdate
