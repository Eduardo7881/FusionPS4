#include "sce/ssl/SceSsl.hpp"

#include "assets/CodecRegistry.hpp"
#include "debug/Log.hpp"
#include "sce/SceStubTable.hpp"

using fusionps4::debug::LogCategory;

namespace fusionps4::sce::ssl {

SceSsl& SceSsl::instance() {
    static SceSsl s;
    return s;
}

bool SceSsl::initialize() {
    m_initialized = true;
    const auto& caps = assets::CodecRegistry::instance();
    if (caps.hasCurl) {
        FP4_INFO(LogCategory::Sce)
            << "libSceSsl initialized (TLS provided by libcurl)";
    } else {
        FP4_WARN(LogCategory::Sce)
            << "libSceSsl initialized without a TLS provider; every call "
            << "will report UNIMPLEMENTED";
    }
    return true;
}

void SceSsl::shutdown() { m_initialized = false; }

namespace {

constexpr int kOk            = 0;
constexpr int kErrNotSupport = static_cast<int>(0x80020003u);

extern "C" {

// sceSslInit returns a context handle. Since libcurl owns the SSL context
// internally when the guest uses sceHttp*, the runtime does not need to
// expose a separate SSL context. We return a stable non-zero handle and
// document that all TLS traffic flows through sceHttp*.
int sceSslInit(std::uint32_t /*poolSize*/) { return 1; }
int sceSslTerm(int /*context*/) { return kOk; }

// Direct TLS sockets (sceSslGetSsl*, sceSslConnect) are not implemented
// because the guest has no way to obtain a raw socket that the runtime
// has not already routed through NetworkPolicy. All TLS-enabled traffic
// must go through sceHttp*.
int sceSslConnect(int /*context*/, void* /*socket*/) {
    FP4_UNIMPLEMENTED(LogCategory::Sce, "sceSslConnect");
    FP4_ERROR(LogCategory::Sce)
        << "  reason=direct TLS sockets are not supported; use sceHttp* "
        << "which routes through NetworkPolicy and libcurl";
    return kErrNotSupport;
}

int sceSslGetIssuerName(int /*context*/, void* /*out*/) { return kErrNotSupport; }
int sceSslGetSubjectName(int /*context*/, void* /*out*/) { return kErrNotSupport; }
int sceSslGetNotBefore(int /*context*/, void* /*out*/) { return kErrNotSupport; }
int sceSslGetNotAfter(int /*context*/, void* /*out*/) { return kErrNotSupport; }

} // extern "C"

} // namespace

void SceSsl::registerExports(SceStubTable& t) {
    t.registerStub("libSceSsl", "sceSslInit",
                   reinterpret_cast<void*>(&sceSslInit));
    t.registerStub("libSceSsl", "sceSslTerm",
                   reinterpret_cast<void*>(&sceSslTerm));
    t.registerStub("libSceSsl", "sceSslConnect",
                   reinterpret_cast<void*>(&sceSslConnect));
    t.registerStub("libSceSsl", "sceSslGetIssuerName",
                   reinterpret_cast<void*>(&sceSslGetIssuerName));
    t.registerStub("libSceSsl", "sceSslGetSubjectName",
                   reinterpret_cast<void*>(&sceSslGetSubjectName));
    t.registerStub("libSceSsl", "sceSslGetNotBefore",
                   reinterpret_cast<void*>(&sceSslGetNotBefore));
    t.registerStub("libSceSsl", "sceSslGetNotAfter",
                   reinterpret_cast<void*>(&sceSslGetNotAfter));
}

} // namespace fusionps4::sce::ssl
