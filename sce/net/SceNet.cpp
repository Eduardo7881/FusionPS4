#include "sce/net/SceNet.hpp"

#include "debug/Log.hpp"
#include "network/NetworkPolicy.hpp"
#include "runtime/RuntimeContext.hpp"
#include "runtime/handles/SocketHandle.hpp"
#include "runtime/process/PS4Process.hpp"
#include "sce/SceStubTable.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

using fusionps4::debug::LogCategory;
using fusionps4::runtime::RuntimeContext;

namespace fusionps4::sce::net {

SceNet& SceNet::instance() {
    static SceNet s;
    return s;
}

bool SceNet::initialize() {
    m_initialized = true;
    FP4_INFO(LogCategory::Sce) << "SceNet initialized";
    return true;
}

void SceNet::shutdown() { m_initialized = false; }

namespace {

constexpr int kSceOk              = 0;
constexpr int kSceErrorInvalidArg = static_cast<int>(0x80020005u);
constexpr int kSceErrorNotFound   = static_cast<int>(0x80020004u);
constexpr int kSceErrorConnect    = static_cast<int>(0x8041010Cu);   // EACCES-equivalent
constexpr int kSceErrorGeneric    = static_cast<int>(0x80410101u);

// Minimal SceNetSockaddrIn. PS4 userland passes a sockaddr-like struct;
// we accept the Linux layout to avoid a second translation step.
struct SceNetSockaddrIn {
    std::uint8_t  len;
    std::uint8_t  family;
    std::uint16_t port;   // network order
    std::uint32_t addr;   // network order
    std::uint8_t  zero[8];
};
static_assert(sizeof(SceNetSockaddrIn) == 16, "SceNetSockaddrIn must be 16");

// SCE socket() type bits differ slightly from Linux; the mapping below
// covers SOCK_STREAM, SOCK_DGRAM and SOCK_RAW, which is what every PS4
// game uses.
int convertSocketType(int sceType) {
    int type = sceType & 0x0F;
    switch (type) {
        case 1: return SOCK_STREAM;
        case 2: return SOCK_DGRAM;
        case 3: return SOCK_RAW;
        default: return sceType;
    }
}

int convertSocketProtocol(int sceProto) {
    // SCE protocols 0/6/17 map 1:1 to Linux.
    return sceProto;
}

} // namespace

namespace {

extern "C" {

int sceNetInit() {
    FP4_DEBUG(LogCategory::Sce) << "sceNetInit";
    return kSceOk;
}

int sceNetTerm() {
    return kSceOk;
}

int sceNetSocket(const char* /*name*/, int sceFamily,
                 int sceType, int sceProto) {
    const int domain   = sceFamily;
    const int type     = convertSocketType(sceType);
    const int protocol = convertSocketProtocol(sceProto);

    const int fd = ::socket(domain, type, protocol);
    if (fd < 0) {
        FP4_ERROR(LogCategory::Network)
            << "socket() failed: " << std::strerror(errno);
        return kSceErrorGeneric;
    }

    auto& proc = RuntimeContext::instance().requireProcess();
    auto obj = std::make_shared<runtime::handles::SocketHandle>(
        fd, domain, type, protocol);
    return proc.handleTable().registerObject(std::move(obj));
}

int sceNetClose(int handle) {
    auto& proc = RuntimeContext::instance().requireProcess();
    if (!proc.handleTable().get(static_cast<runtime::handles::Handle>(handle)))
        return kSceErrorNotFound;
    proc.handleTable().close(static_cast<runtime::handles::Handle>(handle));
    return kSceOk;
}

int sceNetConnect(int handle, const SceNetSockaddrIn* addr, unsigned int addrLen) {
    if (!addr || addrLen < sizeof(SceNetSockaddrIn)) return kSceErrorInvalidArg;
    auto& proc = RuntimeContext::instance().requireProcess();
    auto base = proc.handleTable().get(static_cast<runtime::handles::Handle>(handle));
    auto sk = std::dynamic_pointer_cast<runtime::handles::SocketHandle>(base);
    if (!sk) return kSceErrorNotFound;

    // Extract host and port for the policy check.
    char ipStr[INET_ADDRSTRLEN] = {0};
    struct in_addr in{};
    in.s_addr = addr->addr;
    ::inet_ntop(AF_INET, &in, ipStr, sizeof(ipStr));
    const std::uint16_t port = ntohs(addr->port);

    std::string reason;
    if (!RuntimeContext::instance().requireNetwork().allowConnect(ipStr, port, &reason)) {
        FP4_WARN(LogCategory::Network)
            << "connect to " << ipStr << ":" << port << " denied: " << reason;
        return kSceErrorConnect;
    }

    struct sockaddr_in sa{};
    sa.sin_family = AF_INET;
    sa.sin_port   = addr->port;
    sa.sin_addr.s_addr = addr->addr;

    if (::connect(sk->hostFd(), reinterpret_cast<struct sockaddr*>(&sa),
                  sizeof(sa)) != 0) {
        FP4_ERROR(LogCategory::Network)
            << "connect(" << ipStr << ":" << port << ") failed: "
            << std::strerror(errno);
        return kSceErrorGeneric;
    }
    FP4_INFO(LogCategory::Network)
        << "socket " << handle << " connected to " << ipStr << ":" << port;
    return kSceOk;
}

int sceNetSend(int handle, const void* buf, std::size_t len, int flags) {
    auto& proc = RuntimeContext::instance().requireProcess();
    auto base = proc.handleTable().get(static_cast<runtime::handles::Handle>(handle));
    auto sk = std::dynamic_pointer_cast<runtime::handles::SocketHandle>(base);
    if (!sk) return kSceErrorNotFound;
    const ssize_t n = ::send(sk->hostFd(), buf, len, flags);
    if (n < 0) return kSceErrorGeneric;
    return static_cast<int>(n);
}

int sceNetRecv(int handle, void* buf, std::size_t len, int flags) {
    auto& proc = RuntimeContext::instance().requireProcess();
    auto base = proc.handleTable().get(static_cast<runtime::handles::Handle>(handle));
    auto sk = std::dynamic_pointer_cast<runtime::handles::SocketHandle>(base);
    if (!sk) return kSceErrorNotFound;
    const ssize_t n = ::recv(sk->hostFd(), buf, len, flags);
    if (n < 0) return kSceErrorGeneric;
    return static_cast<int>(n);
}

} // extern "C"

} // namespace

void SceNet::registerExports(SceStubTable& t) {
    t.registerStub("libSceNet", "sceNetInit",
                   reinterpret_cast<void*>(&sceNetInit));
    t.registerStub("libSceNet", "sceNetTerm",
                   reinterpret_cast<void*>(&sceNetTerm));
    t.registerStub("libSceNet", "sceNetSocket",
                   reinterpret_cast<void*>(&sceNetSocket));
    t.registerStub("libSceNet", "sceNetClose",
                   reinterpret_cast<void*>(&sceNetClose));
    t.registerStub("libSceNet", "sceNetConnect",
                   reinterpret_cast<void*>(&sceNetConnect));
    t.registerStub("libSceNet", "sceNetSend",
                   reinterpret_cast<void*>(&sceNetSend));
    t.registerStub("libSceNet", "sceNetRecv",
                   reinterpret_cast<void*>(&sceNetRecv));
}

} // namespace fusionps4::sce::net
