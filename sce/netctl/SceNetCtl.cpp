#include "sce/netctl/SceNetCtl.hpp"
#include "debug/Log.hpp"
#include "network/NetworkPolicy.hpp"
#include "runtime/RuntimeContext.hpp"
#include "sce/SceStubTable.hpp"
#include <cstring>
using fusionps4::debug::LogCategory;
using fusionps4::runtime::RuntimeContext;

namespace fusionps4::sce::netctl {

SceNetCtl& SceNetCtl::instance() { static SceNetCtl s; return s; }
bool SceNetCtl::initialize() {
    m_initialized = true;
    FP4_INFO(LogCategory::Sce) << "libSceNetCtl initialized";
    return true;
}
void SceNetCtl::shutdown() { m_initialized = false; }

namespace {

constexpr int kOk              = 0;
constexpr int kErrInvalidArg   = static_cast<int>(0x80020005u);
constexpr int kErrNotFound     = static_cast<int>(0x80020004u);

// SCE_NETCTL_STATE_*
constexpr std::uint32_t kStateDisconnected = 0;
constexpr std::uint32_t kStateConnecting   = 1;
constexpr std::uint32_t kStateObtainingIp  = 2;
constexpr std::uint32_t kStateConnected    = 3;

// SCE_NETCTL_INFO_PARAM_STATE and friends.
constexpr std::uint32_t kInfoState      = 0x01;
constexpr std::uint32_t kInfoNat        = 0x02;
constexpr std::uint32_t kInfoIpAddress  = 0x0A;
constexpr std::uint32_t kInfoMacAddress = 0x0B;
constexpr std::uint32_t kInfoMtu        = 0x0C;
constexpr std::uint32_t kInfoLink       = 0x0D;
constexpr std::uint32_t kInfoBssid      = 0x10;
constexpr std::uint32_t kInfoSsid       = 0x11;
constexpr std::uint32_t kInfoWlan       = 0x12;

// sceNetCtlInit registers for state changes; state comes from the runtime's
// NetworkPolicy.
std::uint32_t currentState() {
    auto* pol = RuntimeContext::instance().network();
    if (!pol) return kStateDisconnected;
    using Mode = fusionps4::network::PolicyMode;
    switch (pol->mode()) {
        case Mode::Disabled:      return kStateDisconnected;
        case Mode::LocalhostOnly: return kStateConnected;
        case Mode::Restricted:    return kStateConnected;
        case Mode::FullAccess:    return kStateConnected;
    }
    return kStateDisconnected;
}

extern "C" {

int sceNetCtlInit() { return kOk; }
int sceNetCtlTerm() { return kOk; }

int sceNetCtlGetState(int* outState) {
    if (!outState) return kErrInvalidArg;
    *outState = static_cast<int>(currentState());
    return kOk;
}

int sceNetCtlGetInfo(int code, void* outInfo) {
    if (!outInfo) return kErrInvalidArg;
    switch (code) {
        case kInfoState:
            *static_cast<std::uint32_t*>(outInfo) = currentState();
            return kOk;
        case kInfoNat:
            // NAT type 1 = open, 2 = moderate, 3 = strict. We report 2.
            *static_cast<std::uint32_t*>(outInfo) = 2;
            return kOk;
        case kInfoLink:
            // 1 = WiFi, 2 = Ethernet. Report Ethernet.
            *static_cast<std::uint32_t*>(outInfo) = 2;
            return kOk;
        case kInfoMtu:
            *static_cast<std::uint32_t*>(outInfo) = 1500;
            return kOk;
        case kInfoIpAddress: {
            // SceNetCtlInfo for IP is a 16-byte ASCII string.
            std::memset(outInfo, 0, 16);
            const char* ip = "127.0.0.1";
            std::memcpy(outInfo, ip, std::strlen(ip));
            return kOk;
        }
        case kInfoMacAddress: {
            std::memset(outInfo, 0, 16);
            std::uint8_t mac[6] = {0x02, 0x46, 0x50, 0x34, 0x00, 0x01};
            std::memcpy(outInfo, mac, 6);
            return kOk;
        }
        case kInfoWlan:
            // 0 = not WiFi.
            *static_cast<std::uint32_t*>(outInfo) = 0;
            return kOk;
        case kInfoBssid:
        case kInfoSsid:
            std::memset(outInfo, 0, 32);
            return kOk;
        default:
            FP4_UNIMPLEMENTED(LogCategory::Sce, "sceNetCtlGetInfo");
            FP4_ERROR(LogCategory::Sce)
                << "  reason=unsupported netctl info code 0x"
                << std::hex << code << std::dec;
            return kErrNotFound;
    }
}

int sceNetCtlRegisterCallback(void (* /*cb*/)(int, void*), void* /*arg*/,
                              int* outId) {
    if (outId) *outId = 1;
    return kOk;
}
int sceNetCtlUnregisterCallback(int /*id*/) { return kOk; }

int sceNetCtlCheckCallback() { return kOk; }

} // extern "C"

} // namespace

void SceNetCtl::registerExports(SceStubTable& t) {
    t.registerStub("libSceNetCtl", "sceNetCtlInit",
                   reinterpret_cast<void*>(&sceNetCtlInit));
    t.registerStub("libSceNetCtl", "sceNetCtlTerm",
                   reinterpret_cast<void*>(&sceNetCtlTerm));
    t.registerStub("libSceNetCtl", "sceNetCtlGetState",
                   reinterpret_cast<void*>(&sceNetCtlGetState));
    t.registerStub("libSceNetCtl", "sceNetCtlGetInfo",
                   reinterpret_cast<void*>(&sceNetCtlGetInfo));
    t.registerStub("libSceNetCtl", "sceNetCtlRegisterCallback",
                   reinterpret_cast<void*>(&sceNetCtlRegisterCallback));
    t.registerStub("libSceNetCtl", "sceNetCtlUnregisterCallback",
                   reinterpret_cast<void*>(&sceNetCtlUnregisterCallback));
    t.registerStub("libSceNetCtl", "sceNetCtlCheckCallback",
                   reinterpret_cast<void*>(&sceNetCtlCheckCallback));
}

} // namespace fusionps4::sce::netctl
