#include "sce/signin/SceSigninDialog.hpp"

#include "debug/Log.hpp"
#include "dialogs/DialogManager.hpp"
#include "psn/PsnBackend.hpp"
#include "sce/SceStubTable.hpp"

#include <mutex>

using fusionps4::debug::LogCategory;
using fusionps4::dialogs::DialogManager;
using fusionps4::dialogs::DialogRequest;
using fusionps4::dialogs::DialogKind;
using fusionps4::psn::PsnBackend;

namespace fusionps4::sce::signin {

SceSigninDialog& SceSigninDialog::instance() { static SceSigninDialog s; return s; }
bool SceSigninDialog::initialize() {
    m_initialized = true;
    FP4_INFO(LogCategory::Sce) << "libSceSigninDialog initialized";
    return true;
}
void SceSigninDialog::shutdown() { m_initialized = false; }

namespace {
constexpr int kOk = 0;
constexpr int kErrNotFound = static_cast<int>(0x80020004u);

std::mutex  g_mutex;
bool        g_active = false;
int         g_result = 0;

extern "C" {

int sceSigninDialogInitialize() { return kOk; }
int sceSigninDialogTerminate() { return kOk; }

int sceSigninDialogOpen(std::uint32_t /*mode*/) {
    auto& psn = PsnBackend::instance();
    if (psn.isSignedIn()) {
        // The user is already signed in (our simulated account). Present a
        // non-interactive confirmation message so the guest's UI flow
        // completes; the guest sees success on the next status poll.
        DialogRequest req;
        req.kind    = DialogKind::Signin;
        req.title   = "Signed In";
        req.message = "Signed in as " + psn.account().onlineId;
        req.buttons = { "OK" };
        req.timeoutMs = 1500;
        req.onComplete = [](auto) {
            std::lock_guard lock(g_mutex);
            g_active = false;
            g_result = 0;
        };
        {
            std::lock_guard lock(g_mutex);
            g_active = true;
        }
        DialogManager::instance().submit(std::move(req));
    }
    return kOk;
}

int sceSigninDialogGetStatus(std::uint32_t* outStatus) {
    if (!outStatus) return kErrNotFound;
    std::lock_guard lock(g_mutex);
    *outStatus = g_active ? 0u : 1u;
    return kOk;
}

int sceSigninDialogUpdateStatus(std::uint32_t* outStatus) {
    return sceSigninDialogGetStatus(outStatus);
}

int sceSigninDialogGetResult(std::int32_t* outResult) {
    if (!outResult) return kErrNotFound;
    std::lock_guard lock(g_mutex);
    *outResult = g_result;
    return kOk;
}

int sceSigninDialogClose() {
    std::lock_guard lock(g_mutex);
    g_active = false;
    return kOk;
}

} // extern "C"
} // namespace

void SceSigninDialog::registerExports(SceStubTable& t) {
    t.registerStub("libSceSigninDialog", "sceSigninDialogInitialize",
                   reinterpret_cast<void*>(&sceSigninDialogInitialize));
    t.registerStub("libSceSigninDialog", "sceSigninDialogTerminate",
                   reinterpret_cast<void*>(&sceSigninDialogTerminate));
    t.registerStub("libSceSigninDialog", "sceSigninDialogOpen",
                   reinterpret_cast<void*>(&sceSigninDialogOpen));
    t.registerStub("libSceSigninDialog", "sceSigninDialogGetStatus",
                   reinterpret_cast<void*>(&sceSigninDialogGetStatus));
    t.registerStub("libSceSigninDialog", "sceSigninDialogUpdateStatus",
                   reinterpret_cast<void*>(&sceSigninDialogUpdateStatus));
    t.registerStub("libSceSigninDialog", "sceSigninDialogGetResult",
                   reinterpret_cast<void*>(&sceSigninDialogGetResult));
    t.registerStub("libSceSigninDialog", "sceSigninDialogClose",
                   reinterpret_cast<void*>(&sceSigninDialogClose));
}

} // namespace fusionps4::sce::signin
