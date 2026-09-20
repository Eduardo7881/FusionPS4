#pragma once

#include <cstdint>
#include <mutex>
#include <string>

namespace fusionps4::network {

enum class PolicyMode {
    Disabled,       // every socket operation fails
    LocalhostOnly,  // connect() only to 127.0.0.1 / ::1
    Restricted,     // connect() only to an allowlist
    FullAccess,     // no restrictions
};

struct Endpoint {
    std::string host;
    std::uint16_t port = 0;
};

class NetworkPolicy {
public:
    NetworkPolicy();

    void setMode(PolicyMode mode);
    PolicyMode mode() const;

    // Add an endpoint to the Restricted allowlist. Ignored in other modes.
    void allowEndpoint(const std::string& host, std::uint16_t port);
    void clearAllowlist();

    // Decide whether a connect() to (host, port) is allowed. `outReason`
    // is filled on denial for logging.
    bool allowConnect(const std::string& host,
                      std::uint16_t      port,
                      std::string*       outReason = nullptr) const;

    // Decide whether bind/listen on `port` is allowed. Only relevant for
    // the guest hosting a server; the default policy permits binds on
    // 127.0.0.1 in all modes except Disabled.
    bool allowBind(std::uint16_t port, std::string* outReason = nullptr) const;

private:
    mutable std::mutex        m_mutex;
    PolicyMode                m_mode = PolicyMode::LocalhostOnly;
    std::vector<Endpoint>     m_allowlist;
};

} // namespace fusionps4::network
