#include "sce/bgft/SceBgft.hpp"

#include "debug/Log.hpp"
#include "sce/SceStubTable.hpp"
#include "sce/bgft/BgftTransfer.hpp"

#include <cstring>
#include <string>

using fusionps4::debug::LogCategory;

namespace fusionps4::sce::bgft {

SceBgft& SceBgft::instance() { static SceBgft s; return s; }

bool SceBgft::initialize() {
    m_initialized = true;
    BgftTransfer::instance().start();
    FP4_INFO(LogCategory::Sce)
        << "libSceBgft initialized (local VFS-to-VFS transfer backend)";
    return true;
}
void SceBgft::shutdown() {
    BgftTransfer::instance().stop();
    m_initialized = false;
}

namespace {

constexpr int kOk             = 0;
constexpr int kErrInvalidArg  = static_cast<int>(0x80020005u);
constexpr int kErrNotFound    = static_cast<int>(0x80020004u);

// sceBgft* param structure for Install:
//     header(16) + strings; we parse a simplified layout with src/dst
//     embedded at known offsets.
struct InstallParam {
    std::uint32_t reserved[4];
    char          contentUrl[512];       // src, interpreted as a guest path
    char          contentName[64];       // dst directory name
    char          dstPath[512];          // dst dir under /user/
    std::uint32_t flags;
    std::uint32_t reserved2;
};
static_assert(sizeof(InstallParam) == 16 + 512 + 64 + 512 + 8, "InstallParam");

extern "C" {

int sceBgftInitialize(const void* /*init*/, const void* /*opt*/) { return kOk; }
int sceBgftTerminate() { return kOk; }

int sceBgftServiceDownloadRegisterTask(const InstallParam* p,
                                       std::int32_t* outTaskId) {
    if (!p || !outTaskId) return kErrInvalidArg;
    // Interpret contentUrl as a local guest source path and dstPath +
    // contentName as the destination.
    const std::string src = p->contentUrl;
    std::string dst = p->dstPath;
    if (!dst.empty() && dst.back() != '/') dst.push_back('/');
    dst += p->contentName;

    auto id = BgftTransfer::instance().submit(src, dst);
    *outTaskId = static_cast<std::int32_t>(id);
    FP4_INFO(LogCategory::Sce)
        << "sceBgftServiceDownloadRegisterTask: " << src << " -> " << dst
        << " (task=" << id << ")";
    return kOk;
}

int sceBgftServiceDownloadStartTask(std::int32_t /*taskId*/) { return kOk; }
int sceBgftServiceDownloadPauseTask(std::int32_t /*taskId*/) { return kOk; }
int sceBgftServiceDownloadResumeTask(std::int32_t /*taskId*/) { return kOk; }
int sceBgftServiceDownloadStopTask(std::int32_t /*taskId*/) { return kOk; }
int sceBgftServiceDownloadUnregisterTask(std::int32_t /*taskId*/) { return kOk; }

int sceBgftServiceDownloadGetTaskInfo(std::int32_t taskId, void* outInfo) {
    if (!outInfo) return kErrInvalidArg;
    std::uint64_t total = 0, copied = 0;
    std::uint32_t state = 0, err = 0;
    if (!BgftTransfer::instance().query(
            static_cast<std::uint32_t>(taskId), &total, &copied, &state, &err)) {
        return kErrNotFound;
    }
    // SceBgftTask: 64 bytes layout, only the first fields matter to guests.
    struct Task {
        std::uint32_t taskId;
        std::uint32_t status;   // 0=pending, 1=running, 2=done, 3=failed
        std::uint64_t totalSize;
        std::uint64_t downloadedSize;
        std::uint32_t errorCode;
        std::uint32_t reserved[9];
    };
    static_assert(sizeof(Task) == 64, "Task");
    auto* t = static_cast<Task*>(outInfo);
    std::memset(t, 0, sizeof(*t));
    t->taskId          = static_cast<std::uint32_t>(taskId);
    t->status          = state;
    t->totalSize       = total;
    t->downloadedSize  = copied;
    t->errorCode       = err;
    return kOk;
}

int sceBgftServiceIntDownloadRegisterTaskByStorageEx(const void* /*p*/,
                                                     std::int32_t* outTaskId) {
    if (outTaskId) *outTaskId = -1;
    return kOk;
}

} // extern "C"
} // namespace

void SceBgft::registerExports(SceStubTable& t) {
    t.registerStub("libSceBgft", "sceBgftInitialize",
                   reinterpret_cast<void*>(&sceBgftInitialize));
    t.registerStub("libSceBgft", "sceBgftTerminate",
                   reinterpret_cast<void*>(&sceBgftTerminate));
    t.registerStub("libSceBgft", "sceBgftServiceDownloadRegisterTask",
                   reinterpret_cast<void*>(&sceBgftServiceDownloadRegisterTask));
    t.registerStub("libSceBgft", "sceBgftServiceDownloadStartTask",
                   reinterpret_cast<void*>(&sceBgftServiceDownloadStartTask));
    t.registerStub("libSceBgft", "sceBgftServiceDownloadPauseTask",
                   reinterpret_cast<void*>(&sceBgftServiceDownloadPauseTask));
    t.registerStub("libSceBgft", "sceBgftServiceDownloadResumeTask",
                   reinterpret_cast<void*>(&sceBgftServiceDownloadResumeTask));
    t.registerStub("libSceBgft", "sceBgftServiceDownloadStopTask",
                   reinterpret_cast<void*>(&sceBgftServiceDownloadStopTask));
    t.registerStub("libSceBgft", "sceBgftServiceDownloadUnregisterTask",
                   reinterpret_cast<void*>(&sceBgftServiceDownloadUnregisterTask));
    t.registerStub("libSceBgft", "sceBgftServiceDownloadGetTaskInfo",
                   reinterpret_cast<void*>(&sceBgftServiceDownloadGetTaskInfo));
    t.registerStub("libSceBgft", "sceBgftServiceIntDownloadRegisterTaskByStorageEx",
                   reinterpret_cast<void*>(&sceBgftServiceIntDownloadRegisterTaskByStorageEx));
}

} // namespace fusionps4::sce::bgft
