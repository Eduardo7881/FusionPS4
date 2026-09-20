#include "sce/pad/ScePad.hpp"

#include "debug/Log.hpp"
#include "input/InputManager.hpp"
#include "runtime/RuntimeContext.hpp"
#include "runtime/process/PS4Process.hpp"
#include "sce/SceStubTable.hpp"

#include <cstring>

using fusionps4::debug::LogCategory;
using fusionps4::runtime::RuntimeContext;

namespace fusionps4::sce::pad {

ScePad& ScePad::instance() {
    static ScePad s;
    return s;
}

bool ScePad::initialize() {
    m_initialized = true;
    FP4_INFO(LogCategory::Sce) << "ScePad initialized";
    return true;
}

void ScePad::shutdown() { m_initialized = false; }

namespace {

constexpr int kSceOk              = 0;
constexpr int kSceErrorInvalidArg = static_cast<int>(0x80020005u);
constexpr int kSceErrorNotFound   = static_cast<int>(0x80020004u);

// PS4 libScePad "user" ids, one per slot.
constexpr int kUserPadCount = 4;

// Handle table entry: a PadHandle maps (portId) into InputManager.
struct PadHandleInfo {
    int index = 0;   // 0..3
    bool open = false;
};

// The registered handle value for the guest is the HandleTable handle; the
// input index is tracked alongside. We use a small fixed table indexed by
// the guest-visible handle for simplicity — one process, at most four pads.
PadHandleInfo g_openPads[kUserPadCount];

extern "C" {

int scePadInit() {
    FP4_DEBUG(LogCategory::Sce) << "scePadInit";
    return kSceOk;
}

int scePadOpen(int userId, int /*type*/, int /*index*/, const void* /*param*/) {
    if (userId < 0 || userId >= kUserPadCount) return kSceErrorInvalidArg;

    auto& proc = RuntimeContext::instance().requireProcess();

    struct PadHandleObj : runtime::handles::HandleObject {
        int index;
        explicit PadHandleObj(int i) : index(i) {}
        HandleType  type() const override { return HandleType::Device; }
        const char* typeName() const override { return "Pad"; }
        std::string describe() const override {
            return "Pad{index=" + std::to_string(index) + "}";
        }
    };

    auto obj = std::make_shared<PadHandleObj>(userId);
    const auto h = proc.handleTable().registerObject(std::move(obj));

    FP4_INFO(LogCategory::Sce)
        << "scePadOpen(user=" << userId << ") -> handle=" << h;
    return h;
}

int scePadClose(int handle) {
    auto& proc = RuntimeContext::instance().requireProcess();
    if (!proc.handleTable().get(static_cast<runtime::handles::Handle>(handle)))
        return kSceErrorNotFound;
    proc.handleTable().close(static_cast<runtime::handles::Handle>(handle));
    return kSceOk;
}

// ScePadData on the guest side is exactly what InputManager::PadData
// mirrors, so we copy 0x78 bytes.
int scePadReadState(int handle, void* outPadData) {
    if (!outPadData) return kSceErrorInvalidArg;
    auto& proc = RuntimeContext::instance().requireProcess();
    auto base  = proc.handleTable().get(
        static_cast<runtime::handles::Handle>(handle));
    if (!base) return kSceErrorNotFound;

    // Extract the pad index from the handle object's describe(); this is
    // less brittle than RTTI across a shared_ptr dynamic_cast, but we do
    // have the concrete type as PadHandleObj (local). Since PadHandleObj
    // is only defined locally, we recover the index by parsing describe.
    const auto d = base->describe();
    const auto pos = d.find("index=");
    if (pos == std::string::npos) return kSceErrorInvalidArg;
    int idx = std::atoi(d.c_str() + pos + 6);
    if (idx < 0 || idx >= input::InputManager::kMaxPads)
        return kSceErrorInvalidArg;

    const auto pad = RuntimeContext::instance().requireInput().padState(idx);
    std::memcpy(outPadData, &pad, sizeof(pad));
    return kSceOk;
}

int scePadRead(int handle, void* outPadData, int num) {
    if (num < 1) return kSceErrorInvalidArg;
    const int rc = scePadReadState(handle, outPadData);
    if (rc != kSceOk) return rc;
    return num;
}

struct ScePadVibrationParam {
    std::uint8_t largeMotor;
    std::uint8_t smallMotor;
};

int scePadSetVibration(int handle, const ScePadVibrationParam* param) {
    if (!param) return kSceErrorInvalidArg;
    auto& proc = RuntimeContext::instance().requireProcess();
    auto base  = proc.handleTable().get(
        static_cast<runtime::handles::Handle>(handle));
    if (!base) return kSceErrorNotFound;

    const auto d = base->describe();
    const auto pos = d.find("index=");
    if (pos == std::string::npos) return kSceErrorInvalidArg;
    const int idx = std::atoi(d.c_str() + pos + 6);

    RuntimeContext::instance().requireInput().setVibration(
        idx, param->largeMotor, param->smallMotor);
    return kSceOk;
}

} // extern "C"

} // namespace

void ScePad::registerExports(SceStubTable& t) {
    t.registerStub("libScePad", "scePadInit",
                   reinterpret_cast<void*>(&scePadInit));
    t.registerStub("libScePad", "scePadOpen",
                   reinterpret_cast<void*>(&scePadOpen));
    t.registerStub("libScePad", "scePadClose",
                   reinterpret_cast<void*>(&scePadClose));
    t.registerStub("libScePad", "scePadReadState",
                   reinterpret_cast<void*>(&scePadReadState));
    t.registerStub("libScePad", "scePadRead",
                   reinterpret_cast<void*>(&scePadRead));
    t.registerStub("libScePad", "scePadSetVibration",
                   reinterpret_cast<void*>(&scePadSetVibration));
}

} // namespace fusionps4::sce::pad
