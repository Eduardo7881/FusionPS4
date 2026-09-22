#include "sce/share/SceShare.hpp"
#include "debug/Log.hpp"
#include "sce/SceStubTable.hpp"
using fusionps4::debug::LogCategory;

namespace fusionps4::sce::share {

SceShare& SceShare::instance() { static SceShare s; return s; }
bool SceShare::initialize() {
    m_initialized = true;
    FP4_WARN(LogCategory::Sce)
        << "libSceShare initialized; PSN upload is not available because "
        << "this runtime simulates PSN locally. Every share operation will "
        << "return SCE_SHARE_ERROR_NOT_SIGNED_IN with a reason in the log.";
    return true;
}
void SceShare::shutdown() { m_initialized = false; }

namespace {
constexpr int kErrNotSignedIn = static_cast<int>(0x80550008);
constexpr int kErrNotSupport  = static_cast<int>(0x80020003u);

extern "C" {

int sceShareInitialize() {
    FP4_UNIMPLEMENTED(LogCategory::Sce, "sceShareInitialize");
    FP4_ERROR(LogCategory::Sce)
        << "  reason=PSN share upload requires a real PSN account and "
        << "network access; this runtime simulates PSN and does not upload. "
        << "Screenshots still work through libSceScreenShot (local file).";
    return kErrNotSignedIn;
}

int sceShareTerminate() { return kErrNotSignedIn; }
int sceShareOpen(std::uint32_t /*mode*/) { return kErrNotSignedIn; }
int sceShareClose() { return kErrNotSignedIn; }
int sceShareGetStatus(std::uint32_t* out) {
    if (out) *out = 0;
    return kErrNotSignedIn;
}
int sceShareRegisterPostDialog(void* /*p*/) { return kErrNotSignedIn; }
int sceShareSetVideoToPng(const void* /*p*/) { return kErrNotSignedIn; }
int sceShareCaptureScreen() { return kErrNotSignedIn; }

} // extern "C"
} // namespace

void SceShare::registerExports(SceStubTable& t) {
    t.registerStub("libSceShare", "sceShareInitialize",
                   reinterpret_cast<void*>(&sceShareInitialize));
    t.registerStub("libSceShare", "sceShareTerminate",
                   reinterpret_cast<void*>(&sceShareTerminate));
    t.registerStub("libSceShare", "sceShareOpen",
                   reinterpret_cast<void*>(&sceShareOpen));
    t.registerStub("libSceShare", "sceShareClose",
                   reinterpret_cast<void*>(&sceShareClose));
    t.registerStub("libSceShare", "sceShareGetStatus",
                   reinterpret_cast<void*>(&sceShareGetStatus));
    t.registerStub("libSceShare", "sceShareRegisterPostDialog",
                   reinterpret_cast<void*>(&sceShareRegisterPostDialog));
    t.registerStub("libSceShare", "sceShareSetVideoToPng",
                   reinterpret_cast<void*>(&sceShareSetVideoToPng));
    t.registerStub("libSceShare", "sceShareCaptureScreen",
                   reinterpret_cast<void*>(&sceShareCaptureScreen));
}

} // namespace fusionps4::sce::share
