#include "sce/camera/SceCamera.hpp"

#include "debug/Log.hpp"
#include "sce/SceStubTable.hpp"

using fusionps4::debug::LogCategory;

namespace fusionps4::sce::camera {

SceCamera& SceCamera::instance() {
    static SceCamera s;
    return s;
}

bool SceCamera::initialize() {
    m_initialized = true;
    FP4_INFO(LogCategory::Sce)
        << "libSceCamera initialized (no physical device available)";
    return true;
}

void SceCamera::shutdown() { m_initialized = false; }

namespace {

constexpr int kErrNotConnected = static_cast<int>(0x802A0001);
constexpr int kErrNotOpen      = static_cast<int>(0x802A0002);
constexpr int kErrInvalidArg   = static_cast<int>(0x80020005u);

extern "C" {

// Every sceCamera* entry point reports NOT_CONNECTED, because the runtime
// does not have a camera device. This is the honest behaviour: a title
// that requires a camera cannot be satisfied, and the runtime says so.

int sceCameraOpen(int /*userId*/, int /*type*/, int /*index*/,
                  const void* /*param*/) {
    FP4_UNIMPLEMENTED(LogCategory::Sce, "sceCameraOpen");
    FP4_ERROR(LogCategory::Sce)
        << "  reason=no camera device is available on this host; "
        << "the runtime does not emulate the PS4 camera";
    return kErrNotConnected;
}

int sceCameraClose(int /*handle*/)  { return kErrNotOpen; }
int sceCameraStart(int /*handle*/)  { return kErrNotOpen; }
int sceCameraStop(int /*handle*/)   { return kErrNotOpen; }
int sceCameraIsAttached(int index) {
    FP4_TRACE(LogCategory::Sce) << "sceCameraIsAttached(" << index << ")";
    return 0;   // 0 = não conectada
}
int sceCameraGetDeviceInfo(int /*handle*/, void* /*info*/) {
    return kErrNotOpen;
}
int sceCameraGetStateInfo(int /*handle*/, void* /*info*/) {
    return kErrNotOpen;
}
int sceCameraSetConfig(int /*handle*/, int /*config*/, const void* /*p*/) {
    return kErrInvalidArg;
}
int sceCameraRead(int /*handle*/, void* /*data*/) {
    return kErrNotOpen;
}

} // extern "C"

} // namespace

void SceCamera::registerExports(SceStubTable& t) {
    t.registerStub("libSceCamera", "sceCameraOpen",
                   reinterpret_cast<void*>(&sceCameraOpen));
    t.registerStub("libSceCamera", "sceCameraClose",
                   reinterpret_cast<void*>(&sceCameraClose));
    t.registerStub("libSceCamera", "sceCameraStart",
                   reinterpret_cast<void*>(&sceCameraStart));
    t.registerStub("libSceCamera", "sceCameraStop",
                   reinterpret_cast<void*>(&sceCameraStop));
    t.registerStub("libSceCamera", "sceCameraIsAttached",
                   reinterpret_cast<void*>(&sceCameraIsAttached));
    t.registerStub("libSceCamera", "sceCameraGetDeviceInfo",
                   reinterpret_cast<void*>(&sceCameraGetDeviceInfo));
    t.registerStub("libSceCamera", "sceCameraGetStateInfo",
                   reinterpret_cast<void*>(&sceCameraGetStateInfo));
    t.registerStub("libSceCamera", "sceCameraSetConfig",
                   reinterpret_cast<void*>(&sceCameraSetConfig));
    t.registerStub("libSceCamera", "sceCameraRead",
                   reinterpret_cast<void*>(&sceCameraRead));
}

} // namespace fusionps4::sce::camera
