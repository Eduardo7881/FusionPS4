#include "sce/videoout/SceVideoOut.hpp"

#include "debug/Log.hpp"
#include "graphics/GraphicsManager.hpp"
#include "runtime/RuntimeContext.hpp"
#include "runtime/process/PS4Process.hpp"
#include "sce/SceStubTable.hpp"

#include <cstring>

using fusionps4::debug::LogCategory;
using fusionps4::runtime::RuntimeContext;
namespace gfx = fusionps4::graphics;

namespace fusionps4::sce::videoout {

SceVideoOut& SceVideoOut::instance() {
    static SceVideoOut s;
    return s;
}

bool SceVideoOut::initialize() {
    m_initialized = true;
    FP4_INFO(LogCategory::Sce) << "SceVideoOut initialized";
    return true;
}

void SceVideoOut::shutdown() { m_initialized = false; }

namespace {

constexpr int kSceOk              = 0;
constexpr int kSceErrorInvalidArg = static_cast<int>(0x80020005u);
constexpr int kSceErrorNotFound   = static_cast<int>(0x80020004u);
constexpr int kSceErrorNoMemory   = static_cast<int>(0x80020002u);
constexpr int kSceErrorBusy       = static_cast<int>(0x8002000Bu);

// PS4 uses a fixed "video out handle" value; the runtime keeps its own
// internal handle table but returns this constant for compatibility with
// guest code that assumes the well-known handle.
constexpr int kSceVideoOutHandle = 0;

// SceVideoOutBufferAttribute on PS4.
struct SceVideoOutBufferAttribute {
    std::uint32_t pixelFormat;
    std::uint32_t tilingMode;
    std::uint32_t aspectRatio;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t pitchInPixel;
    std::uint32_t option;
    std::uint32_t reserved0;
    std::uint64_t reserved1;
};
static_assert(sizeof(SceVideoOutBufferAttribute) == 0x28,
              "SceVideoOutBufferAttribute must be 0x28");

constexpr std::uint32_t kSceVideoOutPixelFormatA8R8G8B8Srgb  = 0x80000000u;
constexpr std::uint32_t kSceVideoOutPixelFormatA8R8G8B8Linear= 0x00000000u;

// A registered buffer slot. Guest provides a pointer to memory that lives
// in its address space; the runtime treats it as a CPU-side staging area
// for the corresponding swapchain image.
struct RegisteredBuffer {
    void*         guestPtr = nullptr;
    std::uint32_t width    = 0;
    std::uint32_t height   = 0;
    std::uint32_t pitch    = 0;
    std::uint32_t format   = 0;
};

// Only one video out is used per process in this phase; the layout of
// SceVideoOut reflects that.
struct VideoOutState {
    bool             open = false;
    RegisteredBuffer buffers[4]{};
    int              bufferCount = 0;
    int              currentIndex = 0;
    int              flipRate = 60;
};

VideoOutState g_state;

// Flip rate -> nanoseconds.
std::int64_t flipRateToNs(int rate) {
    if (rate <= 0) rate = 60;
    return 1'000'000'000LL / rate;
}

} // namespace

namespace {

extern "C" {

int sceVideoOutOpen(int /*userId*/, int /*busType*/, int /*index*/,
                    const void* /*param*/) {
    if (g_state.open) return kSceErrorBusy;

    // Configure the swapchain through the graphics manager, sized from the
    // host window.
    auto* dev = RuntimeContext::instance().process();
    (void)dev;

    // The actual swapchain was already created by Runtime::init once it had
    // the HostWindow. Nothing else to do here besides mark the device as
    // opened for the guest.
    g_state.open = true;
    g_state.bufferCount = 0;
    g_state.currentIndex = 0;

    FP4_INFO(LogCategory::Sce)
        << "sceVideoOutOpen -> handle=" << kSceVideoOutHandle;
    return kSceVideoOutHandle;
}

int sceVideoOutClose(int handle) {
    if (handle != kSceVideoOutHandle || !g_state.open)
        return kSceErrorNotFound;
    g_state.open = false;
    g_state.bufferCount = 0;
    return kSceOk;
}

int sceVideoOutRegisterBuffers(int handle,
                               int startIndex,
                               void* const* addresses,
                               int bufferNum,
                               const SceVideoOutBufferAttribute* attribute) {
    if (handle != kSceVideoOutHandle || !g_state.open)
        return kSceErrorNotFound;
    if (!addresses || !attribute || bufferNum <= 0)
        return kSceErrorInvalidArg;
    if (startIndex < 0 || startIndex + bufferNum > 4)
        return kSceErrorInvalidArg;

    for (int i = 0; i < bufferNum; ++i) {
        if (!addresses[i]) return kSceErrorInvalidArg;
        auto& b = g_state.buffers[startIndex + i];
        b.guestPtr = addresses[i];
        b.width    = attribute->width;
        b.height   = attribute->height;
        b.pitch    = attribute->pitchInPixel * 4;
        b.format   = attribute->pixelFormat;
    }
    g_state.bufferCount = startIndex + bufferNum;

    FP4_INFO(LogCategory::Sce)
        << "sceVideoOutRegisterBuffers: " << bufferNum
        << " buffers from index " << startIndex
        << " (" << attribute->width << "x" << attribute->height << ")";
    return kSceOk;
}

int sceVideoOutUnregisterBuffers(int handle, int attributeIndex) {
    if (handle != kSceVideoOutHandle || !g_state.open)
        return kSceErrorNotFound;
    if (attributeIndex < 0 || attributeIndex >= 4)
        return kSceErrorInvalidArg;
    g_state.buffers[attributeIndex] = RegisteredBuffer{};
    return kSceOk;
}

// sceVideoOutSubmitFlip: guest renders into one of its registered buffers
// and asks the runtime to display it. The runtime translates this into a
// "present the swapchain image whose content corresponds to that buffer".
// Until the GNM layer fully manages GPU resources, the runtime presents
// the currently active swapchain image with a solid clear color; the
// architecturally important part is that the present passes through the
// GraphicsManager and the HostWindow, never through the guest.
int sceVideoOutSubmitFlip(int handle, int bufferIndex,
                          int flipMode, std::int64_t flipArg) {
    if (handle != kSceVideoOutHandle || !g_state.open)
        return kSceErrorNotFound;
    if (bufferIndex < 0 || bufferIndex >= g_state.bufferCount)
        return kSceErrorInvalidArg;

    (void)flipMode;
    (void)flipArg;

    auto* manager = RuntimeContext::instance().process();
    (void)manager;

    // The actual GPU submission happens in Runtime::tick via GraphicsManager.
    // Here we only record the flip request for diagnostics and advance the
    // buffer index used by the game's flip loop.
    g_state.currentIndex = bufferIndex;

    FP4_TRACE(LogCategory::Sce)
        << "sceVideoOutSubmitFlip(buffer=" << bufferIndex
        << " mode=" << flipMode << ")";
    return kSceOk;
}

int sceVideoOutSetFlipRate(int handle, int rate) {
    if (handle != kSceVideoOutHandle || !g_state.open)
        return kSceErrorNotFound;
    if (rate <= 0) return kSceErrorInvalidArg;
    g_state.flipRate = rate;
    FP4_DEBUG(LogCategory::Sce)
        << "sceVideoOutSetFlipRate(" << rate << ")";
    return kSceOk;
}

int sceVideoOutGetFlipStatus(int handle, void* status) {
    if (handle != kSceVideoOutHandle || !g_state.open)
        return kSceErrorNotFound;
    if (!status) return kSceErrorInvalidArg;
    // The runtime presents synchronously on the main thread; by the time
    // the guest polls, the previous flip has completed.
    std::memset(status, 0, 0x30);
    return kSceOk;
}

} // extern "C"

} // namespace

void SceVideoOut::registerExports(SceStubTable& t) {
    t.registerStub("libSceVideoOut", "sceVideoOutOpen",
                   reinterpret_cast<void*>(&sceVideoOutOpen));
    t.registerStub("libSceVideoOut", "sceVideoOutClose",
                   reinterpret_cast<void*>(&sceVideoOutClose));
    t.registerStub("libSceVideoOut", "sceVideoOutRegisterBuffers",
                   reinterpret_cast<void*>(&sceVideoOutRegisterBuffers));
    t.registerStub("libSceVideoOut", "sceVideoOutUnregisterBuffers",
                   reinterpret_cast<void*>(&sceVideoOutUnregisterBuffers));
    t.registerStub("libSceVideoOut", "sceVideoOutSubmitFlip",
                   reinterpret_cast<void*>(&sceVideoOutSubmitFlip));
    t.registerStub("libSceVideoOut", "sceVideoOutSetFlipRate",
                   reinterpret_cast<void*>(&sceVideoOutSetFlipRate));
    t.registerStub("libSceVideoOut", "sceVideoOutGetFlipStatus",
                   reinterpret_cast<void*>(&sceVideoOutGetFlipStatus));

    (void)kSceVideoOutPixelFormatA8R8G8B8Srgb;
    (void)kSceVideoOutPixelFormatA8R8G8B8Linear;
    (void)flipRateToNs;
}

} // namespace fusionps4::sce::videoout
