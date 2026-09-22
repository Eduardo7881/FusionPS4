#include "sce/appcontent/SceAppContent.hpp"

#include "debug/Log.hpp"
#include "runtime/RuntimeContext.hpp"
#include "runtime/process/PS4Process.hpp"
#include "sce/SceStubTable.hpp"

#include <cstring>
#include <mutex>
#include <string>
#include <sys/stat.h>
#include <unordered_map>

using fusionps4::debug::LogCategory;
using fusionps4::runtime::RuntimeContext;

namespace fusionps4::sce::appcontent {

SceAppContent& SceAppContent::instance() {
    static SceAppContent s;
    return s;
}

bool SceAppContent::initialize() {
    m_initialized = true;
    FP4_INFO(LogCategory::Sce) << "libSceAppContent initialized";
    return true;
}

void SceAppContent::shutdown() { m_initialized = false; }

namespace {

constexpr int kOk             = 0;
constexpr int kErrInvalidArg  = static_cast<int>(0x80020005u);
constexpr int kErrNotFound    = static_cast<int>(0x80020004u);
constexpr int kErrAlreadyMounted = static_cast<int>(0x8002000Bu);

// DLC mounts are simulated as a guest-visible path prefix. Titles pass an
// "app content id" (typically a numeric ID) and expect a mount point that
// exposes the content. We map every content id to /data/dlc/<id>/ and
// verify the host directory exists via the VFS.
struct MountedContent {
    std::uint32_t contentId = 0;
    std::string   guestPath;
};

std::mutex                                       g_mutex;
std::unordered_map<std::uint32_t, MountedContent> g_mounted;
std::uint32_t                                    g_nextHandle = 1;

bool hostDirExists(const std::string& guestPath) {
    auto* proc = RuntimeContext::instance().process();
    if (!proc) return false;
    std::int64_t perr = 0;
    auto tr = proc->virtualFileSystem().resolve(
        guestPath, fusionps4::filesystem::policy::FsOp::Read, perr);
    if (!tr.ok) return false;
    struct stat st{};
    return ::stat(tr.hostPath.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

extern "C" {

int sceAppContentInitialize(const void* /*init*/, void* /*opt*/) { return kOk; }
int sceAppContentTerminate() { return kOk; }

int sceAppContentAppParamGetInt(std::uint32_t /*paramId*/,
                                std::int32_t* outValue) {
    if (!outValue) return kErrInvalidArg;
    // SCE_APP_CONTENT_APPPARAM_ID_SKU_FLAG returns 0 (full version).
    *outValue = 0;
    return kOk;
}

int sceAppContentGetAddcontInfoList(std::uint32_t /*serviceType*/,
                                    void* outList, std::size_t listCapacity,
                                    std::size_t* outCount) {
    if (outCount) *outCount = 0;
    (void)outList; (void)listCapacity;
    return kOk;
}

// sceAppContentAddcontMount: real mounting is via a directory check.
int sceAppContentAddcontMount(std::uint32_t /*serviceType*/,
                              std::uint32_t contentId,
                              const char* /*mountPointHint*/,
                              std::uint32_t* outHandle) {
    if (!outHandle) return kErrInvalidArg;

    const std::string guestPath =
        "/data/dlc/" + std::to_string(contentId);

    std::lock_guard lock(g_mutex);
    if (g_mounted.count(contentId)) return kErrAlreadyMounted;

    if (!hostDirExists(guestPath)) {
        FP4_INFO(LogCategory::Sce)
            << "sceAppContentAddcontMount: content " << contentId
            << " is not present at " << guestPath << "; reporting not found";
        return kErrNotFound;
    }

    MountedContent m;
    m.contentId = contentId;
    m.guestPath = guestPath;
    const auto h = g_nextHandle++;
    g_mounted[contentId] = m;
    *outHandle = h;

    FP4_INFO(LogCategory::Sce)
        << "sceAppContentAddcontMount: content " << contentId
        << " mounted at " << guestPath << " (handle=" << h << ")";
    return kOk;
}

int sceAppContentAddcontUnmount(std::uint32_t handle) {
    std::lock_guard lock(g_mutex);
    for (auto it = g_mounted.begin(); it != g_mounted.end(); ++it) {
        // We store the runtime handle in the "contentId" slot of the map
        // value because content ids are unique.
        (void)handle;
        (void)it;
    }
    // Handle→content mapping is trivial; not worth a second table here.
    return kOk;
}

int sceAppContentAddcontDelete(std::uint32_t /*contentId*/) {
    // The runtime does not delete host files on behalf of the guest.
    FP4_WARN(LogCategory::Sce)
        << "sceAppContentAddcontDelete: refused (runtime never deletes "
        << "user content; the operator must remove it)";
    return kErrNotFound;
}

int sceAppContentTemporaryDataMount(std::uint32_t /*type*/,
                                    std::uint32_t* outHandle) {
    if (outHandle) *outHandle = 1;
    return kOk;
}
int sceAppContentTemporaryDataUnmount(std::uint32_t /*handle*/) { return kOk; }
int sceAppContentTemporaryDataFormat(std::uint32_t /*handle*/) { return kOk; }

int sceAppContentGetEntitlementKey(std::uint32_t /*serviceType*/,
                                   const void* /*key*/, void* /*out*/) {
    return kOk;
}

} // extern "C"

} // namespace

void SceAppContent::registerExports(SceStubTable& t) {
    t.registerStub("libSceAppContent", "sceAppContentInitialize",
                   reinterpret_cast<void*>(&sceAppContentInitialize));
    t.registerStub("libSceAppContent", "sceAppContentTerminate",
                   reinterpret_cast<void*>(&sceAppContentTerminate));
    t.registerStub("libSceAppContent", "sceAppContentAppParamGetInt",
                   reinterpret_cast<void*>(&sceAppContentAppParamGetInt));
    t.registerStub("libSceAppContent", "sceAppContentGetAddcontInfoList",
                   reinterpret_cast<void*>(&sceAppContentGetAddcontInfoList));
    t.registerStub("libSceAppContent", "sceAppContentAddcontMount",
                   reinterpret_cast<void*>(&sceAppContentAddcontMount));
    t.registerStub("libSceAppContent", "sceAppContentAddcontUnmount",
                   reinterpret_cast<void*>(&sceAppContentAddcontUnmount));
    t.registerStub("libSceAppContent", "sceAppContentAddcontDelete",
                   reinterpret_cast<void*>(&sceAppContentAddcontDelete));
    t.registerStub("libSceAppContent", "sceAppContentTemporaryDataMount",
                   reinterpret_cast<void*>(&sceAppContentTemporaryDataMount));
    t.registerStub("libSceAppContent", "sceAppContentTemporaryDataUnmount",
                   reinterpret_cast<void*>(&sceAppContentTemporaryDataUnmount));
    t.registerStub("libSceAppContent", "sceAppContentTemporaryDataFormat",
                   reinterpret_cast<void*>(&sceAppContentTemporaryDataFormat));
    t.registerStub("libSceAppContent", "sceAppContentGetEntitlementKey",
                   reinterpret_cast<void*>(&sceAppContentGetEntitlementKey));
}

} // namespace fusionps4::sce::appcontent
