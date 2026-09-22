#include "sce/dialog/SceCommonDialog.hpp"
#include "debug/Log.hpp"
#include "dialogs/DialogManager.hpp"
#include "sce/SceStubTable.hpp"
using fusionps4::debug::LogCategory;
namespace fusionps4::sce::dialog {

SceCommonDialog& SceCommonDialog::instance() { static SceCommonDialog s; return s; }
bool SceCommonDialog::initialize() {
    m_initialized = true;
    FP4_INFO(LogCategory::Sce) << "libSceCommonDialog initialized";
    return true;
}
void SceCommonDialog::shutdown() { m_initialized = false; }

namespace {
constexpr int kOk = 0;
extern "C" {
int sceCommonDialogInitialize() { return kOk; }
int sceCommonDialogTerminate() { return kOk; }
int sceCommonDialogIsUsed(int* outUsed) {
    if (outUsed)
        *outUsed = fusionps4::dialogs::DialogManager::instance()
                       .hasActiveDialog() ? 1 : 0;
    return kOk;
}
int sceCommonDialogUpdate() {
    // Runtime ticks the DialogManager itself, so nothing to do here.
    return kOk;
}
} // extern "C"
} // namespace

void SceCommonDialog::registerExports(SceStubTable& t) {
    t.registerStub("libSceCommonDialog", "sceCommonDialogInitialize",
                   reinterpret_cast<void*>(&sceCommonDialogInitialize));
    t.registerStub("libSceCommonDialog", "sceCommonDialogTerminate",
                   reinterpret_cast<void*>(&sceCommonDialogTerminate));
    t.registerStub("libSceCommonDialog", "sceCommonDialogIsUsed",
                   reinterpret_cast<void*>(&sceCommonDialogIsUsed));
    t.registerStub("libSceCommonDialog", "sceCommonDialogUpdate",
                   reinterpret_cast<void*>(&sceCommonDialogUpdate));
}

} // namespace fusionps4::sce::dialog
