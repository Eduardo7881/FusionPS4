#include "sce/dialog/SceSaveDataDialog.hpp"
#include "debug/Log.hpp"
#include "dialogs/DialogManager.hpp"
#include "sce/SceStubTable.hpp"
#include <cstring>
using fusionps4::debug::LogCategory;
using fusionps4::dialogs::DialogManager;
using fusionps4::dialogs::DialogRequest;
using fusionps4::dialogs::DialogKind;
using fusionps4::dialogs::DialogResult;

namespace fusionps4::sce::dialog {

SceSaveDataDialog& SceSaveDataDialog::instance() { static SceSaveDataDialog s; return s; }
bool SceSaveDataDialog::initialize() {
    m_initialized = true;
    FP4_INFO(LogCategory::Sce) << "libSceSaveDataDialog initialized";
    return true;
}
void SceSaveDataDialog::shutdown() { m_initialized = false; }

namespace {
constexpr int kOk = 0;
constexpr int kErrInvalidArg = static_cast<int>(0x80020005u);

// Result reported to the guest callback.
constexpr std::int32_t kResultOk     = 0;
constexpr std::int32_t kResultCancel = 1;

struct SaveDialogParam {
    std::uint32_t dialogType;   // 0=save, 1=load, 2=delete
    std::uint32_t reserved;
    char          title[128];
    char          subtitle[128];
    char          detail[256];
    char          dirName[64];
};

int g_lastResult = kResultCancel;

extern "C" {

int sceSaveDataDialogInitialize() { return kOk; }
int sceSaveDataDialogTerminate() { return kOk; }

int sceSaveDataDialogOpen(const SaveDialogParam* param) {
    if (!param) return kErrInvalidArg;

    DialogRequest req;
    req.kind = DialogKind::SaveData;
    switch (param->dialogType) {
        case 0: req.title = "Save Data";     break;
        case 1: req.title = "Load Save Data";break;
        case 2: req.title = "Delete Save";   break;
        default: req.title = "Save Data";    break;
    }
    req.message      = param->subtitle;
    req.saveDirName  = param->dirName;
    req.saveTitle    = param->title;
    req.saveSubtitle = param->subtitle;
    req.saveDetail   = param->detail;
    req.buttons      = { "OK", "Cancel" };
    req.defaultButton = 0;
    req.onComplete = [](DialogResult r) {
        g_lastResult = (r == DialogResult::Ok) ? kResultOk : kResultCancel;
    };

    DialogManager::instance().submit(std::move(req));
    return 1;
}

int sceSaveDataDialogGetStatus(std::uint32_t* outStatus) {
    if (outStatus) *outStatus = DialogManager::instance().hasActiveDialog() ? 0u : 1u;
    return kOk;
}

int sceSaveDataDialogUpdateStatus(std::uint32_t* outStatus) {
    return sceSaveDataDialogGetStatus(outStatus);
}

int sceSaveDataDialogGetResult(std::int32_t* outResult) {
    if (outResult) *outResult = g_lastResult;
    return kOk;
}

int sceSaveDataDialogClose() { return kOk; }
int sceSaveDataDialogAbort() {
    DialogManager::instance().cancelActive();
    return kOk;
}

int sceSaveDataDialogIsReadyToDisplay(int* outReady) {
    if (outReady) *outReady = DialogManager::instance().hasActiveDialog() ? 0 : 1;
    return kOk;
}

} // extern "C"
} // namespace

void SceSaveDataDialog::registerExports(SceStubTable& t) {
    t.registerStub("libSceSaveDataDialog", "sceSaveDataDialogInitialize",
                   reinterpret_cast<void*>(&sceSaveDataDialogInitialize));
    t.registerStub("libSceSaveDataDialog", "sceSaveDataDialogTerminate",
                   reinterpret_cast<void*>(&sceSaveDataDialogTerminate));
    t.registerStub("libSceSaveDataDialog", "sceSaveDataDialogOpen",
                   reinterpret_cast<void*>(&sceSaveDataDialogOpen));
    t.registerStub("libSceSaveDataDialog", "sceSaveDataDialogGetStatus",
                   reinterpret_cast<void*>(&sceSaveDataDialogGetStatus));
    t.registerStub("libSceSaveDataDialog", "sceSaveDataDialogUpdateStatus",
                   reinterpret_cast<void*>(&sceSaveDataDialogUpdateStatus));
    t.registerStub("libSceSaveDataDialog", "sceSaveDataDialogGetResult",
                   reinterpret_cast<void*>(&sceSaveDataDialogGetResult));
    t.registerStub("libSceSaveDataDialog", "sceSaveDataDialogClose",
                   reinterpret_cast<void*>(&sceSaveDataDialogClose));
    t.registerStub("libSceSaveDataDialog", "sceSaveDataDialogAbort",
                   reinterpret_cast<void*>(&sceSaveDataDialogAbort));
    t.registerStub("libSceSaveDataDialog", "sceSaveDataDialogIsReadyToDisplay",
                   reinterpret_cast<void*>(&sceSaveDataDialogIsReadyToDisplay));
}

} // namespace fusionps4::sce::dialog
