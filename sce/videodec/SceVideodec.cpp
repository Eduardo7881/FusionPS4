#include "sce/videodec/SceVideodec.hpp"

#include "assets/CodecRegistry.hpp"
#include "debug/Log.hpp"
#include "sce/SceStubTable.hpp"

using fusionps4::debug::LogCategory;

namespace fusionps4::sce::videodec {

SceVideodec& SceVideodec::instance() {
    static SceVideodec s;
    return s;
}

bool SceVideodec::initialize() {
    m_initialized = true;
    const auto& caps = assets::CodecRegistry::instance();
    FP4_INFO(LogCategory::Sce)
        << "libSceVideodec initialized (h264=" << caps.hasH264
        << ", vp9=" << caps.hasVpx << ")";
    return true;
}

void SceVideodec::shutdown() { m_initialized = false; }

namespace {

constexpr int kOk            = 0;
constexpr int kErrNotSupport = static_cast<int>(0x80020003u);
constexpr int kErrInvalidArg = static_cast<int>(0x80020005u);

extern "C" {

int sceVideodecInitLibrary(std::uint32_t /*version*/) { return kOk; }
int sceVideodecTermLibrary() { return kOk; }

int sceVideodecCreateDecoder(const void* param, std::uint64_t* outHandle) {
    (void)param;
    (void)outHandle;
    FP4_UNIMPLEMENTED(LogCategory::Sce, "sceVideodecCreateDecoder");
    FP4_ERROR(LogCategory::Sce)
        << "  reason=video decoding is not implemented in this runtime. "
        << "VP9 and H.264 hardware decode are absent on PS4 hardware "
        << "targets and no software path is provided. Titles that play "
        << "video (cutscenes, intros) will see NOT_SUPPORTED here and "
        << "should fall back to their alternate path.";
    return kErrNotSupport;
}

int sceVideodecDeleteDecoder(std::uint64_t /*handle*/) { return kErrNotSupport; }
int sceVideodecDecode(std::uint64_t /*handle*/, const void* /*input*/,
                      std::size_t /*inSize*/, void* /*output*/,
                      std::size_t* /*produced*/) { return kErrNotSupport; }
int sceVideodecFlush(std::uint64_t /*handle*/) { return kErrNotSupport; }

} // extern "C"

} // namespace

void SceVideodec::registerExports(SceStubTable& t) {
    t.registerStub("libSceVideodec", "sceVideodecInitLibrary",
                   reinterpret_cast<void*>(&sceVideodecInitLibrary));
    t.registerStub("libSceVideodec", "sceVideodecTermLibrary",
                   reinterpret_cast<void*>(&sceVideodecTermLibrary));
    t.registerStub("libSceVideodec", "sceVideodecCreateDecoder",
                   reinterpret_cast<void*>(&sceVideodecCreateDecoder));
    t.registerStub("libSceVideodec", "sceVideodecDeleteDecoder",
                   reinterpret_cast<void*>(&sceVideodecDeleteDecoder));
    t.registerStub("libSceVideodec", "sceVideodecDecode",
                   reinterpret_cast<void*>(&sceVideodecDecode));
    t.registerStub("libSceVideodec", "sceVideodecFlush",
                   reinterpret_cast<void*>(&sceVideodecFlush));
}

} // namespace fusionps4::sce::videodec
