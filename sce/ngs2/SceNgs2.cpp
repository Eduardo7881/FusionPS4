#include "sce/ngs2/SceNgs2.hpp"

#include "audio/Ngs2Graph.hpp"
#include "debug/Log.hpp"
#include "runtime/RuntimeContext.hpp"
#include "runtime/process/PS4Process.hpp"
#include "sce/SceStubTable.hpp"

#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>

using fusionps4::debug::LogCategory;
using fusionps4::runtime::RuntimeContext;

namespace fusionps4::sce::ngs2 {

SceNgs2& SceNgs2::instance() {
    static SceNgs2 s;
    return s;
}

bool SceNgs2::initialize() {
    m_graph = std::make_unique<audio::Ngs2Graph>();
    if (!m_graph->initialize()) {
        FP4_ERROR(LogCategory::Sce) << "Ngs2Graph initialization failed";
        m_graph.reset();
        return false;
    }
    m_initialized = true;
    FP4_INFO(LogCategory::Sce) << "libSceNgs2 initialized";
    return true;
}

void SceNgs2::shutdown() {
    if (m_graph) m_graph->shutdown();
    m_graph.reset();
    m_initialized = false;
}

namespace {

constexpr int kOk             = 0;
constexpr int kErrInvalidArg  = static_cast<int>(0x80020005u);
constexpr int kErrNotFound    = static_cast<int>(0x80020004u);
constexpr int kErrNoMemory    = static_cast<int>(0x80020002u);
constexpr int kErrNotSupport  = static_cast<int>(0x80020003u);

// Guest-visible handles. NGS2 uses a tree of objects (system → rack →
// node → voice). The runtime flattens this: every created object that has
// audio data is a "voice" in the mixer. Handles are the addresses of
// small runtime-owned bookkeeping records.

struct ObjectKind {
    enum E { Rack, Voice, Unknown };
};

struct ObjectEntry {
    ObjectKind::E kind;
    std::uint32_t voiceId;
};

std::mutex                                     g_mutex;
std::unordered_map<std::uint64_t, ObjectEntry> g_objects;
std::uint64_t                                  g_nextHandle = 1;

std::uint64_t allocateHandle(ObjectKind::E k, std::uint32_t voiceId) {
    std::lock_guard lock(g_mutex);
    const auto h = g_nextHandle++;
    g_objects[h] = { k, voiceId };
    return h;
}

bool lookupHandle(std::uint64_t h, ObjectEntry& out) {
    std::lock_guard lock(g_mutex);
    auto it = g_objects.find(h);
    if (it == g_objects.end()) return false;
    out = it->second;
    return true;
}

audio::Ngs2Graph& graph() {
    return SceNgs2::instance().graph();
}

extern "C" {

int sceNgs2SystemCreateWithAllocator(std::uint64_t* outHandle,
                                     const void* /*config*/,
                                     std::uint64_t /*allocator*/) {
    if (!outHandle) return kErrInvalidArg;
    *outHandle = allocateHandle(ObjectKind::Rack, 0);
    FP4_DEBUG(LogCategory::Sce)
        << "sceNgs2SystemCreateWithAllocator -> " << *outHandle;
    return kOk;
}

int sceNgs2SystemDestroy(std::uint64_t handle) {
    std::lock_guard lock(g_mutex);
    g_objects.erase(handle);
    return kOk;
}

int sceNgs2RackCreate(std::uint64_t /*system*/, std::uint64_t* outRack,
                      const void* /*config*/) {
    if (!outRack) return kErrInvalidArg;
    *outRack = allocateHandle(ObjectKind::Rack, 0);
    return kOk;
}

int sceNgs2RackDestroy(std::uint64_t rack, std::uint64_t /*system*/) {
    std::lock_guard lock(g_mutex);
    g_objects.erase(rack);
    return kOk;
}

// sceNgs2VoiceCreate: the crucial call. The guest supplies a config that
// tells us whether it is a PCM source or a streaming one. We accept both
// and create a runtime voice.
int sceNgs2VoiceCreate(std::uint64_t /*rack*/, std::uint64_t* outVoice,
                       const void* /*config*/) {
    if (!outVoice) return kErrInvalidArg;
    const auto vid = graph().addVoice();
    if (vid == 0) return kErrNoMemory;
    *outVoice = allocateHandle(ObjectKind::Voice, vid);
    FP4_DEBUG(LogCategory::Sce)
        << "sceNgs2VoiceCreate -> " << *outVoice << " (voiceId=" << vid << ")";
    return kOk;
}

int sceNgs2VoiceDestroy(std::uint64_t voice) {
    ObjectEntry e;
    if (!lookupHandle(voice, e)) return kErrNotFound;
    if (e.kind == ObjectKind::Voice) graph().removeVoice(e.voiceId);
    std::lock_guard lock(g_mutex);
    g_objects.erase(voice);
    return kOk;
}

// sceNgs2VoiceControl with SCE_NGS2_VOICE_CONTROL_* parameters. Only the
// gain and pan controls are implemented; anything else returns
// NOT_SUPPORTED with a specific reason.
int sceNgs2VoiceControl(std::uint64_t voice, std::uint32_t controlId,
                        const void* value, std::size_t valueSize) {
    ObjectEntry e;
    if (!lookupHandle(voice, e) || e.kind != ObjectKind::Voice)
        return kErrNotFound;
    if (!value) return kErrInvalidArg;

    // Control ids for common parameters (from public OpenOrbis headers):
    //   0x0000 = gain (float)
    //   0x0001 = pan  (float, -1..1)
    //   0x0010 = PCM buffer (pointer + size in the value buffer)
    switch (controlId) {
        case 0x0000: {
            if (valueSize < sizeof(float)) return kErrInvalidArg;
            float g = 0.0f;
            std::memcpy(&g, value, sizeof(g));
            graph().setVoiceGain(e.voiceId, g);
            return kOk;
        }
        case 0x0001: {
            if (valueSize < sizeof(float)) return kErrInvalidArg;
            float p = 0.0f;
            std::memcpy(&p, value, sizeof(p));
            graph().setVoicePan(e.voiceId, p);
            return kOk;
        }
        case 0x0010: {
            struct PcmParam { const void* ptr; std::size_t frames; };
            if (valueSize < sizeof(PcmParam)) return kErrInvalidArg;
            PcmParam pp{};
            std::memcpy(&pp, value, sizeof(pp));
            if (!pp.ptr || pp.frames == 0) return kErrInvalidArg;
            graph().setVoicePcm(e.voiceId, pp.ptr, pp.frames);
            return kOk;
        }
        default:
            FP4_UNIMPLEMENTED(LogCategory::Sce, "sceNgs2VoiceControl");
            FP4_ERROR(LogCategory::Sce)
                << "  reason=unsupported NGS2 voice control id 0x"
                << std::hex << controlId << std::dec
                << " voiceSize=" << valueSize;
            return kErrNotSupport;
    }
}

int sceNgs2VoicePlay(std::uint64_t voice, bool loop) {
    ObjectEntry e;
    if (!lookupHandle(voice, e) || e.kind != ObjectKind::Voice)
        return kErrNotFound;
    return graph().playVoice(e.voiceId, loop) ? kOk : kErrInvalidArg;
}

int sceNgs2VoiceStop(std::uint64_t voice) {
    ObjectEntry e;
    if (!lookupHandle(voice, e) || e.kind != ObjectKind::Voice)
        return kErrNotFound;
    return graph().stopVoice(e.voiceId) ? kOk : kErrInvalidArg;
}

int sceNgs2RackSetGain(std::uint64_t /*rack*/, float gain) {
    graph().setMasterGain(gain);
    return kOk;
}

int sceNgs2SystemRender(std::uint64_t /*system*/, const void* /*config*/) {
    // The graph mixes continuously on its own thread; there is no
    // per-frame render step required from the guest. Report success.
    return kOk;
}

} // extern "C"

} // namespace

void SceNgs2::registerExports(SceStubTable& t) {
    t.registerStub("libSceNgs2", "sceNgs2SystemCreateWithAllocator",
                   reinterpret_cast<void*>(&sceNgs2SystemCreateWithAllocator));
    t.registerStub("libSceNgs2", "sceNgs2SystemDestroy",
                   reinterpret_cast<void*>(&sceNgs2SystemDestroy));
    t.registerStub("libSceNgs2", "sceNgs2RackCreate",
                   reinterpret_cast<void*>(&sceNgs2RackCreate));
    t.registerStub("libSceNgs2", "sceNgs2RackDestroy",
                   reinterpret_cast<void*>(&sceNgs2RackDestroy));
    t.registerStub("libSceNgs2", "sceNgs2VoiceCreate",
                   reinterpret_cast<void*>(&sceNgs2VoiceCreate));
    t.registerStub("libSceNgs2", "sceNgs2VoiceDestroy",
                   reinterpret_cast<void*>(&sceNgs2VoiceDestroy));
    t.registerStub("libSceNgs2", "sceNgs2VoiceControl",
                   reinterpret_cast<void*>(&sceNgs2VoiceControl));
    t.registerStub("libSceNgs2", "sceNgs2VoicePlay",
                   reinterpret_cast<void*>(&sceNgs2VoicePlay));
    t.registerStub("libSceNgs2", "sceNgs2VoiceStop",
                   reinterpret_cast<void*>(&sceNgs2VoiceStop));
    t.registerStub("libSceNgs2", "sceNgs2RackSetGain",
                   reinterpret_cast<void*>(&sceNgs2RackSetGain));
    t.registerStub("libSceNgs2", "sceNgs2SystemRender",
                   reinterpret_cast<void*>(&sceNgs2SystemRender));
}

} // namespace fusionps4::sce::ngs2
