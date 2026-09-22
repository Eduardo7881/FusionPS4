#include "sce/input/SceKeyboard.hpp"

#include "debug/Log.hpp"
#include "input/InputManager.hpp"
#include "runtime/RuntimeContext.hpp"
#include "runtime/process/PS4Process.hpp"
#include "sce/SceStubTable.hpp"

#include <cstring>

using fusionps4::debug::LogCategory;
using fusionps4::runtime::RuntimeContext;

namespace fusionps4::sce::input {

SceKeyboard& SceKeyboard::instance() {
    static SceKeyboard s;
    return s;
}

bool SceKeyboard::initialize() {
    m_initialized = true;
    FP4_INFO(LogCategory::Sce) << "libSceKeyboard initialized";
    return true;
}

void SceKeyboard::shutdown() { m_initialized = false; }

namespace {

constexpr int kOk             = 0;
constexpr int kErrInvalidArg  = static_cast<int>(0x80020005u);
constexpr int kErrNotFound    = static_cast<int>(0x80020004u);

// SceKeyboardData on PS4 is a 0x18-byte struct:
//   uint32_t keyCode;
//   uint32_t scanCode;
//   uint8_t  status;     // 0 = up, 1 = down
//   uint8_t  reserved[3];
//   uint64_t timestamp;  // microseconds
struct SceKeyboardData {
    std::uint32_t keyCode;
    std::uint32_t scanCode;
    std::uint8_t  status;
    std::uint8_t  reserved[3];
    std::uint64_t timestamp;
};
static_assert(sizeof(SceKeyboardData) == 0x18, "SceKeyboardData must be 0x18");

// A keyboard handle is a plain integer. Only one keyboard device exists;
// sceKeyboardOpen returns the constant 1, and Close is a no-op.
constexpr int kKeyboardHandle = 1;

extern "C" {

int sceKeyboardInit() { return kOk; }
int sceKeyboardClose() { return kOk; }

int sceKeyboardOpen(int /*userId*/, int /*type*/, int /*index*/,
                    const void* /*param*/) {
    return kKeyboardHandle;
}

int sceKeyboardCloseHandle(int handle) {
    return handle == kKeyboardHandle ? kOk : kErrNotFound;
}

int sceKeyboardReadState(int handle, SceKeyboardData* out) {
    if (handle != kKeyboardHandle) return kErrNotFound;
    if (!out) return kErrInvalidArg;

    const auto k = RuntimeContext::instance().requireInput().keyboardState();
    std::memset(out, 0, sizeof(*out));
    out->keyCode   = k.keyCode;
    out->scanCode  = k.scanCode;
    out->status    = k.pressed ? 1 : 0;
    out->timestamp = k.timestamp;
    return kOk;
}

} // extern "C"

} // namespace

void SceKeyboard::registerExports(SceStubTable& t) {
    t.registerStub("libSceKeyboard", "sceKeyboardInit",
                   reinterpret_cast<void*>(&sceKeyboardInit));
    t.registerStub("libSceKeyboard", "sceKeyboardClose",
                   reinterpret_cast<void*>(&sceKeyboardClose));
    t.registerStub("libSceKeyboard", "sceKeyboardOpen",
                   reinterpret_cast<void*>(&sceKeyboardOpen));
    t.registerStub("libSceKeyboard", "sceKeyboardCloseHandle",
                   reinterpret_cast<void*>(&sceKeyboardCloseHandle));
    t.registerStub("libSceKeyboard", "sceKeyboardReadState",
                   reinterpret_cast<void*>(&sceKeyboardReadState));
}

} // namespace fusionps4::sce::input
