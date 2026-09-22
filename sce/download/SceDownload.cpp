#include "sce/download/SceDownload.hpp"
#include "debug/Log.hpp"
#include "sce/SceStubTable.hpp"
#include "sce/bgft/BgftTransfer.hpp"
#include <cstring>
using fusionps4::debug::LogCategory;

namespace fusionps4::sce::download {

SceDownload& SceDownload::instance() { static SceDownload s; return s; }
bool SceDownload::initialize() {
    m_initialized = true;
    FP4_INFO(LogCategory::Sce) << "libSceDownload initialized";
    return true;
}
void SceDownload::shutdown() { m_initialized = false; }

namespace {
constexpr int kOk = 0;
constexpr int kErrInvalidArg = static_cast<int>(0x80020005u);

extern "C" {

int sceDownloadInitialize() { return kOk; }
int sceDownloadTerminate() { return kOk; }

// sceDownloadInit: some SDK versions expose this alias.
int sceDownloadInit() { return kOk; }
int sceDownloadDone() { return kOk; }

int sceDownloadCreateTask(const char* /*url*/, void* /*param*/,
                          std::int32_t* outTaskId) {
    if (outTaskId) *outTaskId = -1;
    return kOk;
}
int sceDownloadStartTask(std::int32_t /*taskId*/) { return kOk; }
int sceDownloadStopTask(std::int32_t /*taskId*/) { return kOk; }
int sceDownloadDeleteTask(std::int32_t /*taskId*/) { return kOk; }
int sceDownloadGetTaskInfo(std::int32_t /*taskId*/, void* outInfo) {
    if (outInfo) std::memset(outInfo, 0, 64);
    return kOk;
}

} // extern "C"
} // namespace

void SceDownload::registerExports(SceStubTable& t) {
    t.registerStub("libSceDownload", "sceDownloadInitialize",
                   reinterpret_cast<void*>(&sceDownloadInitialize));
    t.registerStub("libSceDownload", "sceDownloadTerminate",
                   reinterpret_cast<void*>(&sceDownloadTerminate));
    t.registerStub("libSceDownload", "sceDownloadInit",
                   reinterpret_cast<void*>(&sceDownloadInit));
    t.registerStub("libSceDownload", "sceDownloadDone",
                   reinterpret_cast<void*>(&sceDownloadDone));
    t.registerStub("libSceDownload", "sceDownloadCreateTask",
                   reinterpret_cast<void*>(&sceDownloadCreateTask));
    t.registerStub("libSceDownload", "sceDownloadStartTask",
                   reinterpret_cast<void*>(&sceDownloadStartTask));
    t.registerStub("libSceDownload", "sceDownloadStopTask",
                   reinterpret_cast<void*>(&sceDownloadStopTask));
    t.registerStub("libSceDownload", "sceDownloadDeleteTask",
                   reinterpret_cast<void*>(&sceDownloadDeleteTask));
    t.registerStub("libSceDownload", "sceDownloadGetTaskInfo",
                   reinterpret_cast<void*>(&sceDownloadGetTaskInfo));
}

} // namespace fusionps4::sce::download
