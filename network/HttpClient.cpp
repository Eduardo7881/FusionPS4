#include "network/HttpClient.hpp"

#include "debug/Log.hpp"
#include "runtime/RuntimeContext.hpp"

#include <cstring>

#if FUSIONPS4_HAVE_CURL
#  include <curl/curl.h>
#endif

using fusionps4::debug::LogCategory;

namespace fusionps4::network {

#if FUSIONPS4_HAVE_CURL

struct Curl {
    CURL* handle = nullptr;
};

namespace {

size_t writeBody(void* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::vector<std::uint8_t>*>(userdata);
    const auto bytes = size * nmemb;
    const auto* p = static_cast<const std::uint8_t*>(ptr);
    out->insert(out->end(), p, p + bytes);
    return bytes;
}

size_t writeHeader(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::vector<std::pair<std::string, std::string>>*>(userdata);
    std::string line(ptr, size * nmemb);
    // Trim trailing CRLF.
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
        line.pop_back();
    }
    const auto colon = line.find(':');
    if (colon == std::string::npos) return size * nmemb;
    auto key = line.substr(0, colon);
    auto val = line.substr(colon + 1);
    while (!val.empty() && val.front() == ' ') val.erase(val.begin());
    out->emplace_back(std::move(key), std::move(val));
    return size * nmemb;
}

} // namespace

#endif  // FUSIONPS4_HAVE_CURL

HttpClient::HttpClient() = default;
HttpClient::~HttpClient() { shutdown(); }

bool HttpClient::initialize() {
#if FUSIONPS4_HAVE_CURL
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != 0) {
        FP4_ERROR(LogCategory::Network) << "curl_global_init failed";
        return false;
    }
    m_curl = std::make_unique<Curl>();
    m_curl->handle = curl_easy_init();
    if (!m_curl->handle) {
        FP4_ERROR(LogCategory::Network) << "curl_easy_init failed";
        return false;
    }
    m_initialized = true;
    FP4_INFO(LogCategory::Network) << "HttpClient initialized (libcurl)";
    return true;
#else
    FP4_WARN(LogCategory::Network)
        << "HttpClient initialized without libcurl; sceHttp* calls will "
        << "report UNIMPLEMENTED";
    return true;   // not fatal: sceHttp registration still happens
#endif
}

void HttpClient::shutdown() {
#if FUSIONPS4_HAVE_CURL
    if (m_curl && m_curl->handle) {
        curl_easy_cleanup(m_curl->handle);
        m_curl->handle = nullptr;
    }
    m_curl.reset();
    if (m_initialized) curl_global_cleanup();
#endif
    m_initialized = false;
}

bool HttpClient::checkPolicy(const std::string& url, std::string* reason) const {
    // Extract host from url.
    const auto schemeEnd = url.find("://");
    std::string hostPort = url;
    if (schemeEnd != std::string::npos) hostPort = url.substr(schemeEnd + 3);
    const auto slash = hostPort.find('/');
    if (slash != std::string::npos) hostPort = hostPort.substr(0, slash);

    std::string host = hostPort;
    std::uint16_t port = 443;
    const auto colon = hostPort.rfind(':');
    if (colon != std::string::npos) {
        host = hostPort.substr(0, colon);
        try {
            port = static_cast<std::uint16_t>(std::stoi(hostPort.substr(colon + 1)));
        } catch (...) { /* keep default */ }
    }

    auto* policy = runtime::RuntimeContext::instance().network();
    if (!policy) {
        if (reason) *reason = "no NetworkPolicy bound to the runtime";
        return false;
    }
    return policy->allowConnect(host, port, reason);
}

bool HttpClient::perform(const HttpRequest& req, HttpResponse& out) {
    std::string reason;
    if (!checkPolicy(req.url, &reason)) {
        FP4_WARN(LogCategory::Network)
            << "HTTP request to \"" << req.url << "\" denied: " << reason;
        return false;
    }

#if FUSIONPS4_HAVE_CURL
    auto* h = m_curl->handle;
    curl_easy_reset(h);
    curl_easy_setopt(h, CURLOPT_URL, req.url.c_str());
    curl_easy_setopt(h, CURLOPT_TIMEOUT_MS, static_cast<long>(req.timeoutMs));
    curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, writeBody);
    curl_easy_setopt(h, CURLOPT_WRITEDATA, &out.body);
    curl_easy_setopt(h, CURLOPT_HEADERFUNCTION, writeHeader);
    curl_easy_setopt(h, CURLOPT_HEADERDATA, &out.headers);
    curl_easy_setopt(h, CURLOPT_USERAGENT, "FusionPS4/0.1");

    struct curl_slist* headerList = nullptr;
    for (const auto& [k, v] : req.headers) {
        const std::string line = k + ": " + v;
        headerList = curl_slist_append(headerList, line.c_str());
    }
    if (headerList) curl_easy_setopt(h, CURLOPT_HTTPHEADER, headerList);

    if (req.method == "POST") {
        curl_easy_setopt(h, CURLOPT_POST, 1L);
        curl_easy_setopt(h, CURLOPT_POSTFIELDS, req.body.data());
        curl_easy_setopt(h, CURLOPT_POSTFIELDSIZE,
                         static_cast<long>(req.body.size()));
    } else if (!req.method.empty() && req.method != "GET") {
        curl_easy_setopt(h, CURLOPT_CUSTOMREQUEST, req.method.c_str());
    }

    const auto rc = curl_easy_perform(h);
    if (headerList) curl_slist_free_all(headerList);
    if (rc != CURLE_OK) {
        FP4_ERROR(LogCategory::Network)
            << "curl_easy_perform failed: " << curl_easy_strerror(rc);
        return false;
    }
    long code = 0;
    curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &code);
    out.statusCode = code;
    FP4_DEBUG(LogCategory::Network)
        << "HTTP " << req.method << " " << req.url << " -> " << code
        << " (" << out.body.size() << " bytes)";
    return true;
#else
    (void)req; (void)out;
    FP4_UNIMPLEMENTED(LogCategory::Network, "HttpClient::perform");
    FP4_ERROR(LogCategory::Network)
        << "  reason=libcurl not linked into this build";
    return false;
#endif
}

} // namespace fusionps4::network
