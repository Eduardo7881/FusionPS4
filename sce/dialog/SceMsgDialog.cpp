#include "sce/dialog/SceMsgDialog.hpp"

#include "debug/Log.hpp"
#include "dialogs/DialogManager.hpp"
#include "sce/SceStubTable.hpp"

#include <atomic>
#include <cstring>
#include <mutex>
#include <unordered_map>

using fusionps4::debug::LogCategory;
using fusionps4::dialogs::DialogManager;
using fusionps4::dialogs::DialogKind;
using fusionps4::dialogs::DialogRequest;
using fusionps4::dialogs::DialogResult;

namespace fusionps4::sce::dialog {

SceMsgDialog& SceMsgDialog::instance() { static SceMsgDialog s; return s; }
bool SceMsgDialog::initialize() {
    m_initialized = true;
    FP4_INFO(LogCategory::Sce) << "libSceMsgDialog initialized";
    return true;
}
void SceMsgDialog::shutdown() { m_initialized = false; }

namespace {

constexpr int kOk             = 0;
constexpr int kErrInvalidArg  = static_cast<int>(0x80020005u);
constexpr int kErrNotFound    = static_cast<int>(0x80020004u);
constexpr int kErrBusy        = static_cast<int>(0x8002000Bu);

// sceMsgDialog result codes sent to the callback.
constexpr std::int32_t kResultOk     = 0;
constexpr std::int32_t kResultCancel = 1;

std::mutex                                        g_mutex;
std::unordered_map<std::uint32_t, void*>          g_callbacks;   // dialId -> cb
std::unordered_map<std::uint32_t, void*>          g_userdata;
std::unordered_map<std::uint32_t, std::uint32_t>  g_results;     // dialId -> code
std::uint32_t                                     g_nextDial = 1;

extern "C" {

int sceMsgDialogInit() { return kOk; }
int sceMsgDialogTerm() { return kOk; }

// sceMsgDialogOpen(param*) — param contains the message type, text, and
// button list. We accept the common layout and forward to DialogManager.
struct MsgParamHeader {
    std::uint32_t mode;         // 0=ok, 1=yesno, 2=none, ...
    std::uint32_t buttonType;   // 0=ok, 1=yesno, ...
    std::uint32_t reserved;
    std::uint64_t userId;
    // Followed by variable-length strings; we only parse the common
    // single-string form via the well-known offset.
    char          message[256];
};
static_assert(sizeof(MsgParamHeader) == 0x114, "MsgParamHeader");

int sceMsgDialogOpen(const MsgParamHeader* param) {
    if (!param) return kErrInvalidArg;

    DialogRequest req;
    req.kind    = DialogKind::Message;
    req.title   = "System";
    req.message = param->message;
    if (param->buttonType == 1) {
        req.buttons = { "Yes", "No" };
    } else {
        req.buttons = { "OK" };
    }
    req.defaultButton = 0;

    // Capture nothing: the callback is invoked by DialogManager::tick
    // through onComplete.
    const auto dialId = g_nextDial++;
    req.onComplete = [dialId](DialogResult r) {
        std::lock_guard lock(g_mutex);
        g_results[dialId] = (r == DialogResult::Ok) ? kResultOk : kResultCancel;
    };

    if (!DialogManager::instance().submit(std::move(req))) {
        return kErrBusy;
    }
    return static_cast<int>(dialId);
}

int sceMsgDialogGetStatus(std::uint32_t dialId, std::uint32_t* outStatus) {
    if (!outStatus) return kErrInvalidArg;
    std::lock_guard lock(g_mutex);
    auto it = g_results.find(dialId);
    *outStatus = (it == g_results.end()) ? 0u : 1u;   // 0=running, 1=finished
    return kOk;
}

int sceMsgDialogUpdateStatus(std::uint32_t dialId, std::uint32_t* outStatus) {
    return sceMsgDialogGetStatus(dialId, outStatus);
}

int sceMsgDialogGetResult(std::uint32_t dialId, std::int32_t* outResult) {
    if (!outResult) return kErrInvalidArg;
    std::lock_guard lock(g_mutex);
    auto it = g_results.find(dialId);
    if (it == g_results.end()) {
        *outResult = -1;
        return kOk;
    }
    *outResult = static_cast<std::int32_t>(it->second);
    return kOk;
}

int sceMsgDialogClose(std::uint32_t dialId) {
    std::lock_guard lock(g_mutex);
    g_results.erase(dialId);
    return kOk;
}

int sceMsgDialogAbort(std::uint32_t /*dialId*/) {
    DialogManager::instance().cancelActive();
    return kOk;
}

int sceMsgDialogRegisterCallback(void (*cb)(std::uint32_t, std::int32_t, void*),
                                 void* userdata) {
    // A single global callback is registered by the title; we store it and
    // invoke it in a future revision when a richer callback convention is
    // needed. For now, results are polled via GetResult.
    (void)cb; (void)userdata;
    return kOk;
}

int sceMsgDialogUnregisterCallback() { return kOk; }

} // extern "C"

} // namespace

void SceMsgDialog::registerExports(SceStubTable& t) {
    t.registerStub("libSceMsgDialog", "sceMsgDialogInit",
                   reinterpret_cast<void*>(&sceMsgDialogInit));
    t.registerStub("libSceMsgDialog", "sceMsgDialogTerm",
                   reinterpret_cast<void*>(&sceMsgDialogTerm));
    t.registerStub("libSceMsgDialog", "sceMsgDialogOpen",
                   reinterpret_cast<void*>(&sceMsgDialogOpen));
    t.registerStub("libSceMsgDialog", "sceMsgDialogGetStatus",
                   reinterpret_cast<void*>(&sceMsgDialogGetStatus));
    t.registerStub("libSceMsgDialog", "sceMsgDialogUpdateStatus",
                   reinterpret_cast<void*>(&sceMsgDialogUpdateStatus));
    t.registerStub("libSceMsgDialog", "sceMsgDialogGetResult",
                   reinterpret_cast<void*>(&sceMsgDialogGetResult));
    t.registerStub("libSceMsgDialog", "sceMsgDialogClose",
                   reinterpret_cast<void*>(&sceMsgDialogClose));
    t.registerStub("libSceMsgDialog", "sceMsgDialogAbort",
                   reinterpret_cast<void*>(&sceMsgDialogAbort));
    t.registerStub("libSceMsgDialog", "sceMsgDialogRegisterCallback",
                   reinterpret_cast<void*>(&sceMsgDialogRegisterCallback));
    t.registerStub("libSceMsgDialog", "sceMsgDialogUnregisterCallback",
                   reinterpret_cast<void*>(&sceMsgDialogUnregisterCallback));
}

} // namespace fusionps4::sce::dialog
