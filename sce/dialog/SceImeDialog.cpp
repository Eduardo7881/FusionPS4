#include "sce/dialog/SceImeDialog.hpp"
#include "debug/Log.hpp"
#include "dialogs/DialogManager.hpp"
#include "sce/SceStubTable.hpp"
#include <cstring>
#include <mutex>
using fusionps4::debug::LogCategory;
using fusionps4::dialogs::DialogManager;
using fusionps4::dialogs::DialogRequest;
using fusionps4::dialogs::DialogKind;
using fusionps4::dialogs::DialogResult;

namespace fusionps4::sce::dialog {

SceImeDialog& SceImeDialog::instance() { static SceImeDialog s; return s; }
bool SceImeDialog::initialize() {
    m_initialized = true;
    FP4_INFO(LogCategory::Sce) << "libSceImeDialog initialized";
    return true;
}
void SceImeDialog::shutdown() { m_initialized = false; }

namespace {
constexpr int kOk = 0;
constexpr int kErrInvalidArg = static_cast<int>(0x80020005u);

struct ImeDialogParam {
    std::uint32_t userId;
    std::uint32_t dialogMode;
    std::uint32_t maxTextLength;
    std::uint32_t reserved;
    char          title[128];
    char          initialText[512];
};

std::mutex  g_mutex;
std::string g_resultText;
std::int32_t g_resultCode = -1;

extern "C" {

int sceImeDialogInit(const ImeDialogParam* param, const void* /*ext*/) {
    if (!param) return kErrInvalidArg;

    DialogRequest req;
    req.kind            = DialogKind::Ime;
    req.title           = param->title;
    req.message         = "Enter text";
    req.imeInitialText  = param->initialText;
    req.imeMaxLength    = param->maxTextLength;
    req.buttons         = { "OK", "Cancel" };
    req.onComplete = [](DialogResult r) {
        std::lock_guard lock(g_mutex);
        g_resultCode = (r == DialogResult::Ok) ? 0 : 1;
    };

    DialogManager::instance().submit(std::move(req));
    return kOk;
}

int sceImeDialogGetStatus(std::uint32_t* outStatus) {
    if (!outStatus) return kErrInvalidArg;
    std::lock_guard lock(g_mutex);
    *outStatus = (g_resultCode < 0) ? 0u : 1u;
    return kOk;
}

int sceImeDialogGetResult(void* outResult) {
    // SceImeDialogResult: { int32_t result; char text[512]; }
    struct Result { std::int32_t code; char text[512]; };
    static_assert(sizeof(Result) == 516, "ImeDialogResult");
    if (!outResult) return kErrInvalidArg;
    auto* r = static_cast<Result*>(outResult);
    std::lock_guard lock(g_mutex);
    r->code = (g_resultCode < 0) ? 1 : g_resultCode;

    // Copy the current IME buffer if the dialog is still active.
    fusionps4::dialogs::DialogManager::ActiveDialogView v;
    if (DialogManager::instance().current(v)) {
        std::snprintf(r->text, sizeof(r->text), "%s", v.imeText.c_str());
    } else {
        std::snprintf(r->text, sizeof(r->text), "%s", g_resultText.c_str());
    }
    return kOk;
}

int sceImeDialogAbort() {
    DialogManager::instance().cancelActive();
    return kOk;
}

} // extern "C"
} // namespace

void SceImeDialog::registerExports(SceStubTable& t) {
    t.registerStub("libSceImeDialog", "sceImeDialogInit",
                   reinterpret_cast<void*>(&sceImeDialogInit));
    t.registerStub("libSceImeDialog", "sceImeDialogGetStatus",
                   reinterpret_cast<void*>(&sceImeDialogGetStatus));
    t.registerStub("libSceImeDialog", "sceImeDialogGetResult",
                   reinterpret_cast<void*>(&sceImeDialogGetResult));
    t.registerStub("libSceImeDialog", "sceImeDialogAbort",
                   reinterpret_cast<void*>(&sceImeDialogAbort));
}

} // namespace fusionps4::sce::dialog
