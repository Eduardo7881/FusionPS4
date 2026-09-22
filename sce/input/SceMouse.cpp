#include "sce/input/SceMouse.hpp"

#include "debug/Log.hpp"
#include "input/InputManager.hpp"
#include "runtime/RuntimeContext.hpp"
#include "sce/SceStubTable.hpp"

#include <cstring>

using fusionps4::debug::LogCategory;
using fusionps4::runtime::RuntimeContext;

namespace fusionps4::sce::input {

SceMouse& SceMouse::instance() {
    static SceMouse s;
    return s;
}

bool SceMouse::initialize() {
    m_initialized = true;
    FP4_INFO(LogCategory::Sce) << "libSceMouse initialized";
    return true;
}

void SceMouse::shutdown() { m_initialized = false; }

namespace {

constexpr int kOk            = 0;
constexpr int kErrInvalidArg = static_cast<int>(0x80020005u);
constexpr int kErrNotFound   = static_cast<int>(0x80020004u);

// SceMouseData on PS4 is 0x18 bytes:
//   int32_t  x, y;
//   int32_t  deltaX, deltaY;
//   uint32_t buttons;   // bit 0 = left, 1 = right, 2 = middle
//   uint32_t reserved;
//   uint64_t timestamp;
struct SceMouseData {
    std::int32_t  x;
    std::int32_t  y;
    std::int32_t  deltaX;
    std::int32_t  deltaY;
    std::uint32_t buttons;
    std::uint32_t reserved;
    std::uint64_t timestamp;
};
static_assert(sizeof(SceMouseData) == 0x20, "SceMouseData must be 0x20");

constexpr int kMouseHandle = 1;

extern "C" {

int sceMouseInit() { return kOk; }
int sceMouseClose() { return kOk; }

int sceMouseOpen(int /*userId*/, int /*type*/, int /*index*/,
                 const void* /*param*/) {
    return kMouseHandle;
}

int sceMouseCloseHandle(int handle) {
    return handle == kMouseHandle ? kOk : kErrNotFound;
}

int sceMouseRead(int handle, SceMouseData* out) {
    if (handle != kMouseHandle) return kErrNotFound;
    if (!out) return kErrInvalidArg;

    const auto m = RuntimeContext::instance().requireInput().mouseState();
    std::memset(out, 0, sizeof(*out));
    out->x         = m.x;
    out->y         = m.y;
    out->deltaX    = m.deltaX;
    out->deltaY    = m.deltaY;
    out->buttons   = m.buttons;
    out->timestamp = m.timestamp;
    return kOk;
}

// PS4 exposes these to let a title capture the cursor.
int sceMouseSetPointerSpeed(int handle, int /*speed*/) {
    return handle == kMouseHandle ? kOk : kErrNotFound;
}

} // extern "C"

} // namespace

void SceMouse::registerExports(SceStubTable& t) {
    t.registerStub("libSceMouse", "sceMouseInit",
                   reinterpret_cast<void*>(&sceMouseInit));
    t.registerStub("libSceMouse", "sceMouseClose",
                   reinterpret_cast<void*>(&sceMouseClose));
    t.registerStub("libSceMouse", "sceMouseOpen",
                   reinterpret_cast<void*>(&sceMouseOpen));
    t.registerStub("libSceMouse", "sceMouseCloseHandle",
                   reinterpret_cast<void*>(&sceMouseCloseHandle));
    t.registerStub("libSceMouse", "sceMouseRead",
                   reinterpret_cast<void*>(&sceMouseRead));
    t.registerStub("libSceMouse", "sceMouseSetPointerSpeed",
                   reinterpret_cast<void*>(&sceMouseSetPointerSpeed));
}

} // namespace fusionps4::sce::input
