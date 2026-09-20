#pragma once

#include "runtime/handles/HandleObject.hpp"

#include <cstdint>

namespace fusionps4::runtime::handles {

// SocketHandle in Phase 3 only tracks the lifetime of the Linux fd and the
// socket's metadata. The network *policy* layer (Phase 4) will add
// connect/bind/listen interception. Until then, socket() / connect() are
// dispatched to raw Linux calls, but the fd never leaves the runtime.
class SocketHandle : public HandleObject {
public:
    SocketHandle(int          hostFd,
                 int          domain,
                 int          type,
                 int          protocol);
    ~SocketHandle() override;

    HandleType  type() const override { return HandleType::Socket; }
    const char* typeName() const override { return "Socket"; }
    std::string describe() const override;

    int hostFd()  const { return m_hostFd; }
    int domain()  const { return m_domain; }
    int type()    const { return m_type; }
    int protocol() const { return m_protocol; }

private:
    int m_hostFd;
    int m_domain;
    int m_type;
    int m_protocol;
};

} // namespace fusionps4::runtime::handles
