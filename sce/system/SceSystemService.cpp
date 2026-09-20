#include "sce/system/SceSystemService.hpp"

#include "debug/Log.hpp"
#include "runtime/RuntimeContext.hpp"
#include "runtime/process/PS4Process.hpp"
#include "sce/SceStubTable.hpp"

#include <cstring>

using fusionps4::debug::LogCategory;

namespace fusionps4::sce::system {

SceSystemService& SceSystemService::instance() {
    static SceSystemService s;
    return s;
}

bool SceSystemService::initialize() {
    m_initialized = true;
    FP4_INFO(LogCategory::Sce) << "SceSystemService initialized";
    return true;
}

void SceSystemService::shutdown() { m_initialized = false; }

namespace {

constexpr int kSceOk = 0;

// SceSystemServiceStatus
struct Status {
    int32_t  eventNum;
    int32_t  padNum;
    int32_t  moveNum;
    int32_t  cameraNum;
    uint32_t unk1;
    uint32_t unk2;
};
static_assert(sizeof(Status) == 0x18, "SceSystemServiceStatus must be 0x18");

extern "C" {

int sceSystemServiceGetStatus(Status* out) {
    if (!out) return static_cast<int>(0x80020005u);
    std::memset(out, 0, sizeof(*out));
    out->padNum   = 1;
    out->eventNum = 1;
    return kSceOk;
}

int sceSystemServiceReceiveEvent(void* /*out*/) {
    // No system events are delivered to the guest in Phase 4. Real event
    // delivery (e.g. "return to dashboard") arrives in Phase 7 when we
    // wire the host window into the system service.
    return kSceOk;
}

int sceSystemServiceHideSplashScreen() { return kSceOk; }
int sceSystemServiceLaunchApp(const char* /*uri*/, const void* /*args*/) {
    FP4_WARN(LogCategory::Sce)
        << "sceSystemServiceLaunchApp: application launching not supported";
    return static_cast<int>(0x80020003u);   // SCE_ERROR_NOT_SUPPORTED
}

int sceKernelGetProcessTime() {
    using clock = std::chrono::steady_clock;
    static const auto t0 = clock::now();
    const auto d = std::chrono::duration_cast<std::chrono::microseconds>(
        clock::now() - t0);
    return static_cast<int>(d.count());
}

} // extern "C"

} // namespace

void SceSystemService::registerExports(SceStubTable& t) {
    t.registerStub("libSceSystemService", "sceSystemServiceGetStatus",
                   reinterpret_cast<void*>(&sceSystemServiceGetStatus));
    t.registerStub("libSceSystemService", "sceSystemServiceReceiveEvent",
                   reinterpret_cast<void*>(&sceSystemServiceReceiveEvent));
    t.registerStub("libSceSystemService", "sceSystemServiceHideSplashScreen",
                   reinterpret_cast<void*>(&sceSystemServiceHideSplashScreen));
    t.registerStub("libSceSystemService", "sceSystemServiceLaunchApp",
                   reinterpret_cast<void*>(&sceSystemServiceLaunchApp));
    t.registerStub("libSceKernel", "sceKernelGetProcessTime",
                   reinterpret_cast<void*>(&sceKernelGetProcessTime));
}

} // namespace fusionps4::sce::system
