#include "network/NetworkPolicy.hpp"

#include "debug/Log.hpp"

#include <algorithm>

using fusionps4::debug::LogCategory;

namespace fusionps4::network {

NetworkPolicy::NetworkPolicy() = default;

void NetworkPolicy::setMode(PolicyMode mode) {
    std::lock_guard lock(m_mutex);
    m_mode = mode;
    const char* name = "?";
    switch (mode) {
        case PolicyMode::Disabled:      name = "Disabled"; break;
        case PolicyMode::LocalhostOnly: name = "LocalhostOnly"; break;
        case PolicyMode::Restricted:    name = "Restricted"; break;
        case PolicyMode::FullAccess:    name = "FullAccess"; break;
    }
    FP4_INFO(LogCategory::Network) << "policy mode = " << name;
}

PolicyMode NetworkPolicy::mode() const {
    std::lock_guard lock(m_mutex);
    return m_mode;
}

void NetworkPolicy::allowEndpoint(const std::string& host, std::uint16_t port) {
    std::lock_guard lock(m_mutex);
    m_allowlist.push_back({host, port});
    FP4_INFO(LogCategory::Network)
        << "allowlist += " << host << ":" << port;
}

void NetworkPolicy::clearAllowlist() {
    std::lock_guard lock(m_mutex);
    m_allowlist.clear();
}

namespace {

bool isLocalhost(const std::string& host) {
    return host == "127.0.0.1" || host == "::1" || host == "localhost";
}

} // namespace

bool NetworkPolicy::allowConnect(const std::string& host,
                                 std::uint16_t      port,
                                 std::string*       outReason) const {
    std::lock_guard lock(m_mutex);
    switch (m_mode) {
        case PolicyMode::Disabled:
            if (outReason) *outReason = "network disabled by policy";
            return false;

        case PolicyMode::LocalhostOnly:
            if (!isLocalhost(host)) {
                if (outReason)
                    *outReason = "only localhost connections permitted";
                return false;
            }
            return true;

        case PolicyMode::Restricted: {
            if (isLocalhost(host)) return true;
            for (const auto& ep : m_allowlist) {
                if (ep.host == host && ep.port == port) return true;
            }
            if (outReason)
                *outReason = "destination not in allowlist";
            return false;
        }

        case PolicyMode::FullAccess:
            return true;
    }
    return false;
}

bool NetworkPolicy::allowBind(std::uint16_t port, std::string* outReason) const {
    std::lock_guard lock(m_mutex);
    if (m_mode == PolicyMode::Disabled) {
        if (outReason) *outReason = "network disabled by policy";
        return false;
    }
    (void)port;
    return true;
}

} // namespace fusionps4::network
