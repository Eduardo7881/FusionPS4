#include "sce/np/SceNpCommon.hpp"
#include "debug/Log.hpp"
#include "psn/PsnBackend.hpp"
#include "sce/SceStubTable.hpp"
#include <cstring>
using fusionps4::debug::LogCategory;
namespace fusionps4::sce::np {

SceNpCommon& SceNpCommon::instance() { static SceNpCommon s; return s; }
bool SceNpCommon::initialize() {
    m_initialized = true;
    FP4_INFO(LogCategory::Sce) << "libSceNpCommon initialized";
    return true;
}
void SceNpCommon::shutdown() { m_initialized = false; }

namespace {
constexpr int kOk             = 0;
constexpr int kErrInvalidArg  = static_cast<int>(0x80020005u);

extern "C" {

// sceNpCreateRequest returns a handle that titles pass to other NP calls.
// We allocate stable small integers; no real request object is required.
std::uint32_t g_nextRequestId = 1;

int sceNpCreateRequest(std::uint32_t* outReqId) {
    if (!outReqId) return kErrInvalidArg;
    *outReqId = g_nextRequestId++;
    return kOk;
}

int sceNpDeleteRequest(std::uint32_t /*reqId*/) { return kOk; }

int sceNpAbortRequest(std::uint32_t /*reqId*/) { return kOk; }

int sceNpGetAccountLanguageA(char* outLanguage) {
    if (!outLanguage) return kErrInvalidArg;
    std::memcpy(outLanguage, "en", 2);
    outLanguage[2] = '\0';
    return kOk;
}

} // extern "C"
} // namespace

void SceNpCommon::registerExports(SceStubTable& t) {
    t.registerStub("libSceNpCommon", "sceNpCreateRequest",
                   reinterpret_cast<void*>(&sceNpCreateRequest));
    t.registerStub("libSceNpCommon", "sceNpDeleteRequest",
                   reinterpret_cast<void*>(&sceNpDeleteRequest));
    t.registerStub("libSceNpCommon", "sceNpAbortRequest",
                   reinterpret_cast<void*>(&sceNpAbortRequest));
    t.registerStub("libSceNpCommon", "sceNpGetAccountLanguageA",
                   reinterpret_cast<void*>(&sceNpGetAccountLanguageA));
}

} // namespace fusionps4::sce::np
