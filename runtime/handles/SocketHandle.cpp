#include "runtime/handles/SocketHandle.hpp"

#include "debug/Log.hpp"

#include <unistd.h>

using fusionps4::debug::LogCategory;

namespace fusionps4::runtime::handles {

SocketHandle::SocketHandle(int hostFd, int domain, int type, int protocol)
    : m_hostFd(hostFd), m_domain(domain), m_type(type), m_protocol(protocol) {}

SocketHandle::~SocketHandle() {
    if (m_hostFd >= 0) {
        ::close(m_hostFd);
        m_hostFd = -1;
    }
}

std::string SocketHandle::describe() const {
    return "Socket{fd=" + std::to_string(m_hostFd) + ", domain=" +
           std::to_string(m_domain) + "}";
}

} // namespace fusionps4::runtime::handles
