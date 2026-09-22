#pragma once

#include "network/NetworkPolicy.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace fusionps4::network {

struct HttpRequest {
    std::string method;         // "GET", "POST", ...
    std::string url;
    std::vector<std::pair<std::string, std::string>> headers;
    std::vector<std::uint8_t> body;
    std::uint32_t timeoutMs = 30000;
};

struct HttpResponse {
    long statusCode = 0;
    std::vector<std::uint8_t> body;
    std::vector<std::pair<std::string, std::string>> headers;
};

// HTTP client that routes every request through the NetworkPolicy. The
// guest never sees a raw socket; every HTTP call it makes goes through
// sceHttp* → here → libcurl → NetworkPolicy → host sockets.
class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;

    bool initialize();
    void shutdown();

    // Perform a synchronous request. Returns false if the policy denies
    // the host or the underlying transport fails.
    bool perform(const HttpRequest& req, HttpResponse& out);

    // Called from the guest's sceHttp* stubs; keeps policy checks in one
    // place. `reason` is filled on denial for the log.
    bool checkPolicy(const std::string& url, std::string* reason) const;

private:
    bool                           m_initialized = false;
    std::unique_ptr<struct Curl>   m_curl;
};

} // namespace fusionps4::network
