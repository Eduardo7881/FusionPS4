#include "sce/move/SceMove.hpp"

#include "debug/Log.hpp"
#include "sce/SceStubTable.hpp"

using fusionps4::debug::LogCategory;

namespace fusionps4::sce::move {

SceMove& SceMove::instance() {
    static SceMove s;
    return s;
}

bool SceMove::initialize() {
    m_initialized = true;
    FP4_INFO(LogCategory::Sce)
        << "libSceMove initialized (no PlayStation Move controller "
        << "available on this host)";
    return true;
}

void SceMove::shutdown() { m_initialized = false; }

namespace {

constexpr int kErrNoDevice = static_cast<int>(0x80020004u);

extern "C" {

// Move controllers are motion-tracked light-sphere devices. The runtime
// cannot satisfy their input without the physical hardware. Every entry
// point returns NOT_FOUND; the log records the call so the UNIMPLEMENTED
// report at shutdown lists them.

int sceMoveInit() {
    FP4_DEBUG(LogCategory::Sce) << "sceMoveInit";
    return 0;
}

int sceMoveOpen(int /*userId*/, int /*type*/, int /*index*/,
                const void* /*param*/) {
    FP4_UNIMPLEMENTED(LogCategory::Sce, "sceMoveOpen");
    FP4_ERROR(LogCategory::Sce)
        << "  reason=no PlayStation Move controller is connected";
    return kErrNoDevice;
}

int sceMoveClose(int /*handle*/)   { return kErrNoDevice; }
int sceMoveReadState(int /*handle*/, void* /*data*/) { return kErrNoDevice; }
int sceMoveGetDeviceInfo(int /*handle*/, void* /*info*/) { return kErrNoDevice; }
int sceMoveIsMoveConnected(int /*index*/) { return 0; }

} // extern "C"

} // namespace

void SceMove::registerExports(SceStubTable& t) {
    t.registerStub("libSceMove", "sceMoveInit",
                   reinterpret_cast<void*>(&sceMoveInit));
    t.registerStub("libSceMove", "sceMoveOpen",
                   reinterpret_cast<void*>(&sceMoveOpen));
    t.registerStub("libSceMove", "sceMoveClose",
                   reinterpret_cast<void*>(&sceMoveClose));
    t.registerStub("libSceMove", "sceMoveReadState",
                   reinterpret_cast<void*>(&sceMoveReadState));
    t.registerStub("libSceMove", "sceMoveGetDeviceInfo",
                   reinterpret_cast<void*>(&sceMoveGetDeviceInfo));
    t.registerStub("libSceMove", "sceMoveIsMoveConnected",
                   reinterpret_cast<void*>(&sceMoveIsMoveConnected));
}

} // namespace fusionps4::sce::move
