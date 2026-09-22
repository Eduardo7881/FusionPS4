#include "sce/http/SceHttp.hpp"

#include "debug/Log.hpp"
#include "network/HttpClient.hpp"
#include "runtime/RuntimeContext.hpp"
#include "runtime/process/PS4Process.hpp"
#include "sce/SceStubTable.hpp"

#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

using fusionps4::debug::LogCategory;
using fusionps4::network::HttpClient;
using fusionps4::network::HttpRequest;
using fusionps4::network::HttpResponse;
using fusionps4::runtime::RuntimeContext;

namespace fusionps4::sce::http {

SceHttp& SceHttp::instance() {
    static SceHttp s;
    return s;
}

namespace {
std::unique_ptr<HttpClient> g_client;
}

bool SceHttp::initialize() {
    g_client = std::make_unique<HttpClient>();
    g_client->initialize();
    m_initialized = true;
    FP4_INFO(LogCategory::Sce) << "libSceHttp initialized";
    return true;
}

void SceHttp::shutdown() {
    if (g_client) g_client->shutdown();
    g_client.reset();
    m_initialized = false;
}

namespace {

constexpr int kOk             = 0;
constexpr int kErrInvalidArg  = static_cast<int>(0x80020005u);
constexpr int kErrNotFound    = static_cast<int>(0x80020004u);
constexpr int kErrNotSupport  = static_cast<int>(0x80020003u);
constexpr int kErrDenied      = static_cast<int>(0x80431077u);   // permission denied

struct HttpHandle {
    enum class Kind { Template, Connection, Request } kind;
    std::string url;
    std::string method;
    std::vector<std::uint8_t> responseBody;
    std::size_t readCursor = 0;
    long        statusCode = 0;
    bool        requestSent = false;
};

std::mutex                                 g_mutex;
std::unordered_map<std::uint64_t, std::shared_ptr<HttpHandle>> g_handles;
std::uint64_t                              g_nextHandle = 1;

std::shared_ptr<HttpHandle> lookup(std::uint64_t h, HttpHandle::Kind want) {
    std::lock_guard lock(g_mutex);
    auto it = g_handles.find(h);
    if (it == g_handles.end() || it->second->kind != want) return nullptr;
    return it->second;
}

extern "C" {

int sceHttpInit(std::uint32_t /*poolSize*/, std::uint32_t /*flags*/) {
    return kOk;
}

int sceHttpTerm() { return kOk; }

int sceHttpCreateTemplate(const char* /*userAgent*/, int /*httpVer*/,
                          int /*autoProxyConf*/, std::uint64_t* outTemplate) {
    if (!outTemplate) return kErrInvalidArg;
    auto h = std::make_shared<HttpHandle>();
    h->kind = HttpHandle::Kind::Template;
    std::lock_guard lock(g_mutex);
    const auto id = g_nextHandle++;
    g_handles[id] = h;
    *outTemplate = id;
    return kOk;
}

int sceHttpDeleteTemplate(std::uint64_t tpl) {
    std::lock_guard lock(g_mutex);
    g_handles.erase(tpl);
    return kOk;
}

int sceHttpCreateConnectionWithURL(std::uint64_t tpl, const char* url,
                                   std::uint64_t* outConn) {
    if (!url || !outConn) return kErrInvalidArg;
    if (!lookup(tpl, HttpHandle::Kind::Template)) return kErrNotFound;

    auto h = std::make_shared<HttpHandle>();
    h->kind = HttpHandle::Kind::Connection;
    h->url  = url;

    std::lock_guard lock(g_mutex);
    const auto id = g_nextHandle++;
    g_handles[id] = h;
    *outConn = id;
    FP4_DEBUG(LogCategory::Sce)
        << "sceHttpCreateConnectionWithURL: \"" << url << "\" -> " << id;
    return kOk;
}

int sceHttpDeleteConnection(std::uint64_t conn) {
    std::lock_guard lock(g_mutex);
    g_handles.erase(conn);
    return kOk;
}

int sceHttpCreateRequestWithURL(std::uint64_t conn, int method,
                                const char* url, std::uint64_t contentLength,
                                std::uint64_t* outReq) {
    if (!url || !outReq) return kErrInvalidArg;
    if (!lookup(conn, HttpHandle::Kind::Connection)) return kErrNotFound;
    (void)contentLength;

    const char* methodStr = "GET";
    switch (method) {
        case 0: methodStr = "GET";    break;
        case 1: methodStr = "POST";   break;
        case 2: methodStr = "HEAD";   break;
        case 3: methodStr = "OPTIONS";break;
        case 4: methodStr = "PUT";    break;
        case 5: methodStr = "DELETE"; break;
        case 6: methodStr = "TRACE";  break;
        case 7: methodStr = "CONNECT";break;
        default: return kErrInvalidArg;
    }

    auto h = std::make_shared<HttpHandle>();
    h->kind   = HttpHandle::Kind::Request;
    h->url    = url;
    h->method = methodStr;

    std::lock_guard lock(g_mutex);
    const auto id = g_nextHandle++;
    g_handles[id] = h;
    *outReq = id;
    return kOk;
}

int sceHttpDeleteRequest(std::uint64_t req) {
    std::lock_guard lock(g_mutex);
    g_handles.erase(req);
    return kOk;
}

int sceHttpSendRequest(std::uint64_t req, const void* postData,
                       std::size_t postDataSize) {
    auto h = lookup(req, HttpHandle::Kind::Request);
    if (!h) return kErrNotFound;

    if (!g_client) {
        FP4_ERROR(LogCategory::Sce)
            << "sceHttpSendRequest: HttpClient not initialized";
        return kErrNotSupport;
    }

    HttpRequest r;
    r.method = h->method;
    r.url    = h->url;
    if (postData && postDataSize > 0) {
        r.body.assign(static_cast<const std::uint8_t*>(postData),
                      static_cast<const std::uint8_t*>(postData) + postDataSize);
    }

    HttpResponse resp;
    if (!g_client->perform(r, resp)) {
        FP4_WARN(LogCategory::Sce)
            << "sceHttpSendRequest failed for \"" << h->url << "\" "
            << "(check NetworkPolicy)";
        return kErrDenied;
    }

    h->responseBody = std::move(resp.body);
    h->readCursor   = 0;
    h->statusCode   = resp.statusCode;
    h->requestSent  = true;

    FP4_INFO(LogCategory::Sce)
        << "sceHttpSendRequest \"" << h->url << "\" -> " << h->statusCode
        << " (" << h->responseBody.size() << " bytes)";
    return kOk;
}

int sceHttpGetStatusCode(std::uint64_t req, int* outCode) {
    if (!outCode) return kErrInvalidArg;
    auto h = lookup(req, HttpHandle::Kind::Request);
    if (!h) return kErrNotFound;
    *outCode = static_cast<int>(h->statusCode);
    return kOk;
}

int sceHttpReadData(std::uint64_t req, void* out, std::size_t outSize) {
    if (!out || outSize == 0) return kErrInvalidArg;
    auto h = lookup(req, HttpHandle::Kind::Request);
    if (!h) return kErrNotFound;
    if (!h->requestSent) return kErrInvalidArg;

    const auto remaining = h->responseBody.size() - h->readCursor;
    const auto take = std::min<std::size_t>(remaining, outSize);
    if (take > 0) {
        std::memcpy(out, h->responseBody.data() + h->readCursor, take);
        h->readCursor += take;
    }
    return static_cast<int>(take);
}

int sceHttpGetContentLength(std::uint64_t req, std::uint64_t* outLen) {
    if (!outLen) return kErrInvalidArg;
    auto h = lookup(req, HttpHandle::Kind::Request);
    if (!h) return kErrNotFound;
    *outLen = h->responseBody.size();
    return kOk;
}

int sceHttpSetRequestContentLength(std::uint64_t req, std::uint64_t length) {
    auto h = lookup(req, HttpHandle::Kind::Request);
    if (!h) return kErrNotFound;
    (void)length;
    return kOk;
}

int sceHttpAddRequestHeader(std::uint64_t req, const char* name,
                            const char* value, std::uint32_t /*mode*/) {
    (void)req; (void)name; (void)value;
    // Headers are accepted but not yet plumbed through to the request.
    // Adding them silently would lie about the outgoing HTTP request, so
    // we do not do that; instead we record the omission in the log.
    FP4_UNIMPLEMENTED(LogCategory::Sce, "sceHttpAddRequestHeader");
    FP4_ERROR(LogCategory::Sce)
        << "  reason=custom HTTP request headers are not yet forwarded to "
        << "libcurl (name=\"" << (name ? name : "") << "\")";
    return kErrNotSupport;
}

} // extern "C"

} // namespace

void SceHttp::registerExports(SceStubTable& t) {
    t.registerStub("libSceHttp", "sceHttpInit",
                   reinterpret_cast<void*>(&sceHttpInit));
    t.registerStub("libSceHttp", "sceHttpTerm",
                   reinterpret_cast<void*>(&sceHttpTerm));
    t.registerStub("libSceHttp", "sceHttpCreateTemplate",
                   reinterpret_cast<void*>(&sceHttpCreateTemplate));
    t.registerStub("libSceHttp", "sceHttpDeleteTemplate",
                   reinterpret_cast<void*>(&sceHttpDeleteTemplate));
    t.registerStub("libSceHttp", "sceHttpCreateConnectionWithURL",
                   reinterpret_cast<void*>(&sceHttpCreateConnectionWithURL));
    t.registerStub("libSceHttp", "sceHttpDeleteConnection",
                   reinterpret_cast<void*>(&sceHttpDeleteConnection));
    t.registerStub("libSceHttp", "sceHttpCreateRequestWithURL",
                   reinterpret_cast<void*>(&sceHttpCreateRequestWithURL));
    t.registerStub("libSceHttp", "sceHttpDeleteRequest",
                   reinterpret_cast<void*>(&sceHttpDeleteRequest));
    t.registerStub("libSceHttp", "sceHttpSendRequest",
                   reinterpret_cast<void*>(&sceHttpSendRequest));
    t.registerStub("libSceHttp", "sceHttpGetStatusCode",
                   reinterpret_cast<void*>(&sceHttpGetStatusCode));
    t.registerStub("libSceHttp", "sceHttpReadData",
                   reinterpret_cast<void*>(&sceHttpReadData));
    t.registerStub("libSceHttp", "sceHttpGetContentLength",
                   reinterpret_cast<void*>(&sceHttpGetContentLength));
    t.registerStub("libSceHttp", "sceHttpSetRequestContentLength",
                   reinterpret_cast<void*>(&sceHttpSetRequestContentLength));
    t.registerStub("libSceHttp", "sceHttpAddRequestHeader",
                   reinterpret_cast<void*>(&sceHttpAddRequestHeader));
}

} // namespace fusionps4::sce::http
