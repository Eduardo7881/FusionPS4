#include "sce/np/SceNpManager.hpp"

#include "debug/Log.hpp"
#include "psn/PsnBackend.hpp"
#include "sce/SceStubTable.hpp"

#include <cstring>
#include <string>

using fusionps4::debug::LogCategory;
using fusionps4::psn::PsnBackend;

namespace fusionps4::sce::np {

SceNpManager& SceNpManager::instance() {
    static SceNpManager s;
    return s;
}

bool SceNpManager::initialize() {
    m_initialized = true;
    FP4_INFO(LogCategory::Sce) << "libSceNpManager initialized";
    return true;
}

void SceNpManager::shutdown() { m_initialized = false; }

namespace {

constexpr int kOk                 = 0;
constexpr int kErrNotSignedIn     = static_cast<int>(0x80550008);
constexpr int kErrInvalidArg      = static_cast<int>(0x80020005u);

// SceNpId on PS4: a struct with 16-byte handle + onlineId + reserved.
struct SceNpId {
    std::uint8_t  handle[16];
    std::uint8_t  reserved[8];
    char          onlineId[16];
    std::uint8_t  reserved2[6];
    std::uint8_t  reserved3[8];
};
static_assert(sizeof(SceNpId) == 54, "SceNpId size");

extern "C" {

int sceNpInit(std::uint32_t /*poolSize*/, std::uint32_t /*flags*/,
              void* /*ctx*/) {
    return kOk;
}
int sceNpTerm() { return kOk; }

int sceNpCheckNpReachability() {
    return PsnBackend::instance().isSignedIn() ? kOk : kErrNotSignedIn;
}

int sceNpGetAccountIdA(std::uint64_t* outAccountId) {
    if (!outAccountId) return kErrInvalidArg;
    auto& psn = PsnBackend::instance();
    if (!psn.isSignedIn()) return kErrNotSignedIn;
    *outAccountId = psn.account().accountId;
    return kOk;
}

int sceNpGetAccountId(std::uint64_t* outAccountId) {
    return sceNpGetAccountIdA(outAccountId);
}

int sceNpGetOnlineId(SceNpId* outId) {
    if (!outId) return kErrInvalidArg;
    auto& psn = PsnBackend::instance();
    if (!psn.isSignedIn()) return kErrNotSignedIn;

    std::memset(outId, 0, sizeof(*outId));
    const auto& a = psn.account();
    // Fill handle with a deterministic byte pattern derived from the
    // account id, so titles that hash the handle for logging see a stable
    // value across runs.
    for (int i = 0; i < 16; ++i) {
        outId->handle[i] = static_cast<std::uint8_t>(
            (a.accountId >> ((i % 8) * 8)) ^ (i * 17));
    }
    const auto n = std::min<std::size_t>(15, a.onlineId.size());
    std::memcpy(outId->onlineId, a.onlineId.data(), n);
    return kOk;
}

int sceNpGetAccountCountry(const char* /*onlineId*/, char* outCountry) {
    if (!outCountry) return kErrInvalidArg;
    // Return the region in the 2-letter form the guest expects.
    std::memcpy(outCountry, "US", 2);
    outCountry[2] = '\0';
    return kOk;
}

int sceNpGetAccountLanguage(char* outLanguage) {
    if (!outLanguage) return kErrInvalidArg;
    std::memcpy(outLanguage, "en", 2);
    outLanguage[2] = '\0';
    return kOk;
}

int sceNpIsPlusMember(int /*slot*/) {
    // bit 0 in flags indicates PS Plus in our simulation. Default off.
    return (PsnBackend::instance().account().flags & 1) ? 1 : 0;
}

int sceNpSetNpTitleId(const char* /*titleId*/, const void* /*opt*/) {
    return kOk;
}

int sceNpRegisterStateCallback(void* /*callback*/, void* /*userdata*/) {
    return kOk;
}

int sceNpUnregisterStateCallback() { return kOk; }

} // extern "C"

} // namespace

void SceNpManager::registerExports(SceStubTable& t) {
    t.registerStub("libSceNpManager", "sceNpInit",
                   reinterpret_cast<void*>(&sceNpInit));
    t.registerStub("libSceNpManager", "sceNpTerm",
                   reinterpret_cast<void*>(&sceNpTerm));
    t.registerStub("libSceNpManager", "sceNpCheckNpReachability",
                   reinterpret_cast<void*>(&sceNpCheckNpReachability));
    t.registerStub("libSceNpManager", "sceNpGetAccountIdA",
                   reinterpret_cast<void*>(&sceNpGetAccountIdA));
    t.registerStub("libSceNpManager", "sceNpGetAccountId",
                   reinterpret_cast<void*>(&sceNpGetAccountId));
    t.registerStub("libSceNpManager", "sceNpGetOnlineId",
                   reinterpret_cast<void*>(&sceNpGetOnlineId));
    t.registerStub("libSceNpManager", "sceNpGetAccountCountry",
                   reinterpret_cast<void*>(&sceNpGetAccountCountry));
    t.registerStub("libSceNpManager", "sceNpGetAccountLanguage",
                   reinterpret_cast<void*>(&sceNpGetAccountLanguage));
    t.registerStub("libSceNpManager", "sceNpIsPlusMember",
                   reinterpret_cast<void*>(&sceNpIsPlusMember));
    t.registerStub("libSceNpManager", "sceNpSetNpTitleId",
                   reinterpret_cast<void*>(&sceNpSetNpTitleId));
    t.registerStub("libSceNpManager", "sceNpRegisterStateCallback",
                   reinterpret_cast<void*>(&sceNpRegisterStateCallback));
    t.registerStub("libSceNpManager", "sceNpUnregisterStateCallback",
                   reinterpret_cast<void*>(&sceNpUnregisterStateCallback));
}

} // namespace fusionps4::sce::np
