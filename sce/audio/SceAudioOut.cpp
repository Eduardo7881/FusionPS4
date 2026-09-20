#include "sce/audio/SceAudioOut.hpp"

#include "audio/AudioManager.hpp"
#include "debug/Log.hpp"
#include "runtime/RuntimeContext.hpp"
#include "runtime/process/PS4Process.hpp"
#include "sce/SceStubTable.hpp"

#include <cstring>

using fusionps4::debug::LogCategory;
using fusionps4::runtime::RuntimeContext;

namespace fusionps4::sce::audio {

SceAudioOut& SceAudioOut::instance() {
    static SceAudioOut s;
    return s;
}

bool SceAudioOut::initialize() {
    m_initialized = true;
    FP4_INFO(LogCategory::Sce) << "SceAudioOut initialized";
    return true;
}

void SceAudioOut::shutdown() { m_initialized = false; }

namespace {

constexpr int kSceOk              = 0;
constexpr int kSceErrorInvalidArg = static_cast<int>(0x80020005u);
constexpr int kSceErrorNotFound   = static_cast<int>(0x80020004u);
constexpr int kSceErrorNoMemory   = static_cast<int>(0x80020002u);

// A port handle maps a guest handle to an AudioManager port id.
struct PortHandleObj : runtime::handles::HandleObject {
    int portId;
    explicit PortHandleObj(int p) : portId(p) {}
    HandleType  type() const override { return HandleType::Device; }
    const char* typeName() const override { return "AudioPort"; }
    std::string describe() const override {
        return "AudioPort{portId=" + std::to_string(portId) + "}";
    }
};

int lookupPortId(runtime::handles::Handle h) {
    auto& proc = RuntimeContext::instance().requireProcess();
    auto obj = proc.handleTable().get(h);
    if (!obj) return -1;
    auto p = std::dynamic_pointer_cast<PortHandleObj>(obj);
    return p ? p->portId : -1;
}

extern "C" {

int sceAudioOutInit() {
    FP4_DEBUG(LogCategory::Sce) << "sceAudioOutInit";
    return kSceOk;
}

int sceAudioOutOpen(int /*userId*/, int /*type*/, int /*index*/,
                    unsigned int len, unsigned int freq, unsigned int /*param*/) {
    // len is the number of samples per output block, freq the sample rate.
    if (len == 0 || freq == 0) return kSceErrorInvalidArg;

    auto& am = RuntimeContext::instance().requireAudio();
    const int portId = am.openPort(/*channels*/ 2,
                                   static_cast<int>(freq),
                                   static_cast<int>(len));
    if (portId < 0) return kSceErrorNoMemory;

    auto& proc = RuntimeContext::instance().requireProcess();
    auto obj = std::make_shared<PortHandleObj>(portId);
    const auto h = proc.handleTable().registerObject(std::move(obj));
    FP4_INFO(LogCategory::Sce)
        << "sceAudioOutOpen(len=" << len << ", freq=" << freq
        << ") -> handle=" << h << " port=" << portId;
    return h;
}

int sceAudioOutClose(int handle) {
    auto& proc = RuntimeContext::instance().requireProcess();
    const auto portId = lookupPortId(static_cast<runtime::handles::Handle>(handle));
    if (portId < 0) return kSceErrorNotFound;
    RuntimeContext::instance().requireAudio().closePort(portId);
    proc.handleTable().close(static_cast<runtime::handles::Handle>(handle));
    return kSceOk;
}

int sceAudioOutOutput(int handle, const void* ptr) {
    if (!ptr) return kSceErrorInvalidArg;
    const auto portId = lookupPortId(static_cast<runtime::handles::Handle>(handle));
    if (portId < 0) return kSceErrorNotFound;

    // The block size is implicit in the port configuration. We pass it as
    // an approximate number of samples: the AudioManager only needs to
    // know how many int16 pairs to enqueue, and it trusts the guest's
    // granularity contract.
    constexpr std::size_t kDefaultSamples = 512;
    if (!RuntimeContext::instance().requireAudio().output(
            portId, ptr, kDefaultSamples)) {
        return kSceErrorInvalidArg;
    }
    return kSceOk;
}

int sceAudioOutSetVolume(int handle, int /*flag*/, int* vol) {
    if (!vol) return kSceErrorInvalidArg;
    const auto portId = lookupPortId(static_cast<runtime::handles::Handle>(handle));
    if (portId < 0) return kSceErrorNotFound;
    RuntimeContext::instance().requireAudio().setMasterVolume(vol[0], vol[1]);
    return kSceOk;
}

} // extern "C"

} // namespace

void SceAudioOut::registerExports(SceStubTable& t) {
    t.registerStub("libSceAudioOut", "sceAudioOutInit",
                   reinterpret_cast<void*>(&sceAudioOutInit));
    t.registerStub("libSceAudioOut", "sceAudioOutOpen",
                   reinterpret_cast<void*>(&sceAudioOutOpen));
    t.registerStub("libSceAudioOut", "sceAudioOutClose",
                   reinterpret_cast<void*>(&sceAudioOutClose));
    t.registerStub("libSceAudioOut", "sceAudioOutOutput",
                   reinterpret_cast<void*>(&sceAudioOutOutput));
    t.registerStub("libSceAudioOut", "sceAudioOutSetVolume",
                   reinterpret_cast<void*>(&sceAudioOutSetVolume));
}

} // namespace fusionps4::sce::audio
