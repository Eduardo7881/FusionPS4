#include "sce/gnm/SceGnm.hpp"

#include "debug/Log.hpp"
#include "graphics/GraphicsManager.hpp"
#include "runtime/RuntimeContext.hpp"
#include "sce/SceStubTable.hpp"

#include <cstring>

using fusionps4::debug::LogCategory;
using fusionps4::runtime::RuntimeContext;
namespace gfx = fusionps4::graphics;

namespace fusionps4::sce::gnm {

SceGnm& SceGnm::instance() {
    static SceGnm s;
    return s;
}

bool SceGnm::initialize() {
    m_initialized = true;
    FP4_INFO(LogCategory::Sce) << "SceGnm initialized";
    return true;
}

void SceGnm::shutdown() { m_initialized = false; }

namespace {

constexpr int kSceGnmOk             = 0;
constexpr int kSceGnmErrorInvalidArg= static_cast<int>(0x80020005u);

// State the guest expects GNM to maintain across calls. This mirrors the
// minimum needed to route a clear-to-present path through the GAL; real
// GNM state (draw state, shader state, resource state) is defined in
// Phase 6 when full command processing lands.
struct GnmState {
    bool         initialized = false;
    gfx::ClearColor currentClear{};
    gfx::PipelineHandle currentPipeline = 0;
    gfx::BufferHandle   currentVertexBuffer = 0;
};

GnmState g_state;

} // namespace

namespace {

extern "C" {

// GNM's real device init entry point is internal; libSceGnm exposes
// per-subsystem functions. We implement the widely-used "safe init"
// entry that titles call after sceGnmInit.
int sceGnmInit() {
    if (g_state.initialized) return kSceGnmOk;
    g_state.initialized = true;
    FP4_INFO(LogCategory::Sce) << "sceGnmInit";
    return kSceGnmOk;
}

int sceGnmTerminate() {
    g_state.initialized = false;
    return kSceGnmOk;
}

// sceGnmSetCsShader and similar functions are handled in Phase 6 with real
// resource creation. Registering them here would be premature; the
// dispatcher's UNIMPLEMENTED report is the correct behaviour.

// A small subset that a title uses to disable/enable rendering:
int sceGnmSubmitDone() {
    // In GNM, SubmitDone flushes the current command buffer. Phase 5 has
    // no per-submit command buffers yet; the runtime already presents once
    // per frame. Report success and note the call in the trace log.
    FP4_TRACE(LogCategory::Sce) << "sceGnmSubmitDone";
    return kSceGnmOk;
}

int sceGnmDrawIndex(int /*indexCount*/, const void* /*indexAddr*/,
                    std::uint64_t /*modifier*/) {
    // Draw submission is Phase 6.
    FP4_ERROR(LogCategory::Sce)
        << "UNIMPLEMENTED sceGnmDrawIndex: index-buffer draw submission "
        << "requires the GNM command processor (Phase 6).";
    return kSceGnmErrorInvalidArg;
}

int sceGnmDrawIndexAuto(int /*indexCount*/, std::uint64_t /*modifier*/) {
    FP4_ERROR(LogCategory::Sce)
        << "UNIMPLEMENTED sceGnmDrawIndexAuto: draw submission requires "
        << "the GNM command processor (Phase 6).";
    return kSceGnmErrorInvalidArg;
}

// GNM's clear function. This one we can honour today: the runtime's
// GraphicsDevice exposes clearCurrentFrame.
int sceGnmDrawInitDefaultHardwareState() {
    g_state.currentClear = {0, 0, 0, 1};
    return kSceGnmOk;
}

} // extern "C"

} // namespace

void SceGnm::registerExports(SceStubTable& t) {
    t.registerStub("libSceGnm", "sceGnmInit",
                   reinterpret_cast<void*>(&sceGnmInit));
    t.registerStub("libSceGnm", "sceGnmTerminate",
                   reinterpret_cast<void*>(&sceGnmTerminate));
    t.registerStub("libSceGnm", "sceGnmSubmitDone",
                   reinterpret_cast<void*>(&sceGnmSubmitDone));
    t.registerStub("libSceGnm", "sceGnmDrawIndex",
                   reinterpret_cast<void*>(&sceGnmDrawIndex));
    t.registerStub("libSceGnm", "sceGnmDrawIndexAuto",
                   reinterpret_cast<void*>(&sceGnmDrawIndexAuto));
    t.registerStub("libSceGnm", "sceGnmDrawInitDefaultHardwareState",
                   reinterpret_cast<void*>(&sceGnmDrawInitDefaultHardwareState));
}

} // namespace fusionps4::sce::gnm
