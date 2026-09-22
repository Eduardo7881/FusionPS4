#include "sce/dialog/SceErrorDialog.hpp"
#include "debug/Log.hpp"
#include "dialogs/DialogManager.hpp"
#include "sce/SceStubTable.hpp"
#include <cstring>
using fusionps4::debug::LogCategory;
namespace fusionps4::sce::dialog {

SceErrorDialog& SceErrorDialog::instance() { static SceErrorDialog s; return s; }
bool SceErrorDialog::initialize() {
    m_initialized = true;
    FP4_INFO(LogCategory::Sce) << "libSceErrorDialog initialized";
    return true;
}
void SceErrorDialog::shutdown() { m_initialized = false; }

namespace {
constexpr int kOk = 0;
constexpr int kErrInvalidArg = static_cast<int>(0x80020005u);

extern "C" {

int sceErrorDialogInitialize() { return kOk; }
int sceErrorDialogTerminate() { return kOk; }

int sceErrorDialogOpen(std::uint32_t errorCode) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "Error 0x%08X", errorCode);
    fusionps4::dialogs::DialogRequest req;
    req.kind    = fusionps4::dialogs::DialogKind::Error;
    req.title   = "Error";
    req.message = buf;
    req.buttons = { "OK" };
    fusionps4::dialogs::DialogManager::instance().submit(std::move(req));
    return 1;
}

int sceErrorDialogGetStatus(std::uint32_t* outStatus) {
    if (outStatus) *outStatus = 1;
    return kOk;
}
int sceErrorDialogUpdateStatus(std::uint32_t* outStatus) {
    return sceErrorDialogGetStatus(outStatus);
}
int sceErrorDialogClose() { return kOk; }

} // extern "C"
} // namespace

void SceErrorDialog::registerExports(SceStubTable& t) {
    t.registerStub("libSceErrorDialog", "sceErrorDialogInitialize",
                   reinterpret_cast<void*>(&sceErrorDialogInitialize));
    t.registerStub("libSceErrorDialog", "sceErrorDialogTerminate",
                   reinterpret_cast<void*>(&sceErrorDialogTerminate));
    t.registerStub("libSceErrorDialog", "sceErrorDialogOpen",
                   reinterpret_cast<void*>(&sceErrorDialogOpen));
    t.registerStub("libSceErrorDialog", "sceErrorDialogGetStatus",
                   reinterpret_cast<void*>(&sceErrorDialogGetStatus));
    t.registerStub("libSceErrorDialog", "sceErrorDialogUpdateStatus",
                   reinterpret_cast<void*>(&sceErrorDialogUpdateStatus));
    t.registerStub("libSceErrorDialog", "sceErrorDialogClose",
                   reinterpret_cast<void*>(&sceErrorDialogClose));
}

} // namespace fusionps4::sce::dialog
