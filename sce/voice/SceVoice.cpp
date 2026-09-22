#include "sce/voice/SceVoice.hpp"

#include "debug/Log.hpp"
#include "sce/SceStubTable.hpp"

using fusionps4::debug::LogCategory;

namespace fusionps4::sce::voice {

SceVoice& SceVoice::instance() {
    static SceVoice s;
    return s;
}

bool SceVoice::initialize() {
    m_initialized = true;
    FP4_INFO(LogCategory::Sce)
        << "libSceVoice initialized (no capture device wired)";
    return true;
}

void SceVoice::shutdown() { m_initialized = false; }

namespace {

constexpr int kErrNoInput = static_cast<int>(0x80020004u);
constexpr int kErrNotSupport = static_cast<int>(0x80020003u);

extern "C" {

int sceVoiceInit(std::uint64_t* /*outCtx*/) {
    FP4_UNIMPLEMENTED(LogCategory::Sce, "sceVoiceInit");
    FP4_ERROR(LogCategory::Sce)
        << "  reason=no microphone/capture device is bound to the runtime; "
        << "voice chat is unavailable. This is expected when no input "
        << "device is configured by the operator.";
    return kErrNoInput;
}

int sceVoiceQuit() { return kErrNoInput; }
int sceVoiceCreatePort(std::uint64_t* /*outPort*/, std::uint32_t /*flags*/) {
    return kErrNoInput;
}
int sceVoiceDeletePort(std::uint64_t /*port*/) { return kErrNoInput; }
int sceVoiceReadFromServer(std::uint64_t /*port*/, void* /*data*/,
                           std::size_t* /*size*/) { return kErrNoInput; }
int sceVoiceWriteToServer(std::uint64_t /*port*/, const void* /*data*/,
                          std::size_t /*size*/) { return kErrNoInput; }
int sceVoiceGetPortInfo(std::uint64_t /*port*/, void* /*info*/) {
    return kErrNoInput;
}
int sceVoiceGetBitRate(std::uint64_t /*port*/, std::uint32_t* outRate) {
    if (outRate) *outRate = 0;
    return kErrNoInput;
}

} // extern "C"

} // namespace

void SceVoice::registerExports(SceStubTable& t) {
    t.registerStub("libSceVoice", "sceVoiceInit",
                   reinterpret_cast<void*>(&sceVoiceInit));
    t.registerStub("libSceVoice", "sceVoiceQuit",
                   reinterpret_cast<void*>(&sceVoiceQuit));
    t.registerStub("libSceVoice", "sceVoiceCreatePort",
                   reinterpret_cast<void*>(&sceVoiceCreatePort));
    t.registerStub("libSceVoice", "sceVoiceDeletePort",
                   reinterpret_cast<void*>(&sceVoiceDeletePort));
    t.registerStub("libSceVoice", "sceVoiceReadFromServer",
                   reinterpret_cast<void*>(&sceVoiceReadFromServer));
    t.registerStub("libSceVoice", "sceVoiceWriteToServer",
                   reinterpret_cast<void*>(&sceVoiceWriteToServer));
    t.registerStub("libSceVoice", "sceVoiceGetPortInfo",
                   reinterpret_cast<void*>(&sceVoiceGetPortInfo));
    t.registerStub("libSceVoice", "sceVoiceGetBitRate",
                   reinterpret_cast<void*>(&sceVoiceGetBitRate));
}

} // namespace fusionps4::sce::voice
