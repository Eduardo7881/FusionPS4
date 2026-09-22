#include "sce/audiodec/SceAudiodec.hpp"

#include "audio/AjmDecoder.hpp"
#include "debug/Log.hpp"
#include "sce/SceStubTable.hpp"

#include <cstring>
#include <memory>
#include <mutex>
#include <unordered_map>

using fusionps4::debug::LogCategory;
using fusionps4::audio::AjmCodec;
using fusionps4::audio::AjmDecoder;
using fusionps4::audio::AjmPcmResult;

namespace fusionps4::sce::audiodec {

SceAudiodec& SceAudiodec::instance() {
    static SceAudiodec s;
    return s;
}

bool SceAudiodec::initialize() {
    m_initialized = true;
    FP4_INFO(LogCategory::Sce) << "libSceAudiodec initialized";
    return true;
}

void SceAudiodec::shutdown() { m_initialized = false; }

namespace {

constexpr int kOk            = 0;
constexpr int kErrInvalidArg = static_cast<int>(0x80020005u);
constexpr int kErrNotFound   = static_cast<int>(0x80020004u);
constexpr int kErrNotSupport = static_cast<int>(0x80020003u);

struct DecoderEntry {
    AjmCodec                       codec;
    std::unique_ptr<AjmDecoder>    impl;
};

std::mutex                                       g_mutex;
std::unordered_map<std::uint64_t, DecoderEntry>  g_decoders;
std::uint64_t                                    g_nextHandle = 1;

extern "C" {

int sceAudiodecInitLibrary(std::uint32_t /*version*/) { return kOk; }
int sceAudiodecTermLibrary() { return kOk; }

// sceAudiodecCreateDecoder(param*, decoderHandle*)
// param->codecType: 0x02 = MP3, 0x04 = AAC, 0x0F = AT9
int sceAudiodecCreateDecoder(const void* param, std::uint64_t* outHandle) {
    if (!param || !outHandle) return kErrInvalidArg;

    struct Param {
        std::uint32_t codecType;
        std::uint32_t reserved;
    };
    Param p{};
    std::memcpy(&p, param, sizeof(p));

    AjmCodec codec;
    switch (p.codecType) {
        case 0x02: codec = AjmCodec::Mp3; break;
        case 0x0F: codec = AjmCodec::At9; break;
        default:
            FP4_UNIMPLEMENTED(LogCategory::Sce, "sceAudiodecCreateDecoder");
            FP4_ERROR(LogCategory::Sce)
                << "  reason=codec type 0x" << std::hex << p.codecType
                << std::dec << " is not supported by this runtime";
            return kErrNotSupport;
    }

    auto impl = AjmDecoder::create(codec);
    if (!impl) return kErrNotSupport;

    std::lock_guard lock(g_mutex);
    const auto h = g_nextHandle++;
    g_decoders[h] = DecoderEntry{ codec, std::move(impl) };
    *outHandle = h;
    return kOk;
}

int sceAudiodecDeleteDecoder(std::uint64_t handle) {
    std::lock_guard lock(g_mutex);
    auto it = g_decoders.find(handle);
    if (it == g_decoders.end()) return kErrNotFound;
    g_decoders.erase(it);
    return kOk;
}

int sceAudiodecDecode(std::uint64_t handle, const void* input,
                      std::size_t inputSize, void* output,
                      std::size_t outputCapacity, std::size_t* outProduced) {
    std::lock_guard lock(g_mutex);
    auto it = g_decoders.find(handle);
    if (it == g_decoders.end()) return kErrNotFound;
    if (!input || !output || inputSize == 0) return kErrInvalidArg;

    AjmPcmResult result;
    if (!it->second.impl->decode(input, inputSize, result)) {
        return kErrInvalidArg;
    }
    const auto samples = std::min(result.samples.size(),
                                  outputCapacity / sizeof(std::int16_t));
    std::memcpy(output, result.samples.data(),
                samples * sizeof(std::int16_t));
    if (outProduced) *outProduced = samples * sizeof(std::int16_t);
    return kOk;
}

int sceAudiodecClearContext(std::uint64_t /*handle*/) { return kOk; }

} // extern "C"

} // namespace

void SceAudiodec::registerExports(SceStubTable& t) {
    t.registerStub("libSceAudiodec", "sceAudiodecInitLibrary",
                   reinterpret_cast<void*>(&sceAudiodecInitLibrary));
    t.registerStub("libSceAudiodec", "sceAudiodecTermLibrary",
                   reinterpret_cast<void*>(&sceAudiodecTermLibrary));
    t.registerStub("libSceAudiodec", "sceAudiodecCreateDecoder",
                   reinterpret_cast<void*>(&sceAudiodecCreateDecoder));
    t.registerStub("libSceAudiodec", "sceAudiodecDeleteDecoder",
                   reinterpret_cast<void*>(&sceAudiodecDeleteDecoder));
    t.registerStub("libSceAudiodec", "sceAudiodecDecode",
                   reinterpret_cast<void*>(&sceAudiodecDecode));
    t.registerStub("libSceAudiodec", "sceAudiodecClearContext",
                   reinterpret_cast<void*>(&sceAudiodecClearContext));
}

} // namespace fusionps4::sce::audiodec
