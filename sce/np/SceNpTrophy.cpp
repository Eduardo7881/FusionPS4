#include "sce/np/SceNpTrophy.hpp"
#include "debug/Log.hpp"
#include "psn/PsnBackend.hpp"
#include "sce/SceStubTable.hpp"
#include <cstring>
#include <mutex>
#include <unordered_map>
using fusionps4::debug::LogCategory;
using fusionps4::psn::PsnBackend;
using fusionps4::psn::TrophyDef;
using fusionps4::psn::TrophyGrade;

namespace fusionps4::sce::np {

SceNpTrophy& SceNpTrophy::instance() { static SceNpTrophy s; return s; }
bool SceNpTrophy::initialize() {
    m_initialized = true;
    FP4_INFO(LogCategory::Sce) << "libSceNpTrophy initialized";
    return true;
}
void SceNpTrophy::shutdown() { m_initialized = false; }

namespace {

constexpr int kOk                 = 0;
constexpr int kErrInvalidArg      = static_cast<int>(0x80020005u);
constexpr int kErrNotFound        = static_cast<int>(0x80020004u);
constexpr int kErrAlreadyUnlocked = static_cast<int>(0x80551604);
constexpr int kErrNotSignedIn     = static_cast<int>(0x80550008);

std::mutex                                          g_mutex;
std::unordered_map<std::uint32_t, std::uint64_t>    g_contexts;      // ctxId -> titleId
std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> g_handles; // ctxId -> trophy handles
std::unordered_map<std::uint32_t, std::uint64_t>    g_handleToTrophy; // handle -> trophyId
std::uint32_t                                       g_nextCtx   = 1;
std::uint32_t                                       g_nextHandle= 1;

// SceNpTrophyDetails as received from the guest (a variable-length struct;
// we accept the common form and copy what we need).
struct GuestTrophyDetails {
    std::uint32_t trophyId;
    std::uint32_t grade;
    std::uint8_t  hidden;
    std::uint8_t  reserved[3];
    std::uint32_t reserved2;
};
static_assert(sizeof(GuestTrophyDetails) == 16, "GuestTrophyDetails");

extern "C" {

int sceNpTrophyCreateContext(std::uint32_t* outContext,
                             std::uint32_t /*commId*/,
                             std::uint64_t /*options*/) {
    if (!outContext) return kErrInvalidArg;
    std::lock_guard lock(g_mutex);
    *outContext = g_nextCtx++;
    return kOk;
}

int sceNpTrophyDestroyContext(std::uint32_t context) {
    std::lock_guard lock(g_mutex);
    g_contexts.erase(context);
    g_handles.erase(context);
    return kOk;
}

// sceNpTrophyRegisterContext(ctx, handle, callback, userdata, options)
// We treat `handle` as the title id. Real titles pass a small integer
// unique per title; our runtime uses that number to namespace the trophy
// store. On success the callback (if provided) is invoked with status OK;
// calling a guest callback from within the trap is acceptable because the
// runtime owns the process.
int sceNpTrophyRegisterContext(std::uint32_t context,
                               std::uint64_t handle,
                               void (*callback)(std::uint32_t, void*),
                               void* userdata,
                               std::uint64_t /*options*/) {
    if (context == 0) return kErrInvalidArg;
    {
        std::lock_guard lock(g_mutex);
        g_contexts[context] = handle;
    }
    if (callback) {
        // Synchronous completion is legal per the SDK contract.
        callback(context, userdata);
    }
    FP4_INFO(LogCategory::Sce)
        << "sceNpTrophyRegisterContext: ctx=" << context
        << " title=" << handle;
    return kOk;
}

// sceNpTrophyCreateHandle(ctx, details, outHandle)
int sceNpTrophyCreateHandle(std::uint32_t context,
                            const GuestTrophyDetails* details,
                            std::uint32_t* outHandle) {
    if (!details || !outHandle) return kErrInvalidArg;
    std::lock_guard lock(g_mutex);
    auto it = g_contexts.find(context);
    if (it == g_contexts.end()) return kErrInvalidArg;

    const std::uint32_t h = g_nextHandle++;
    g_handles[context].push_back(h);
    g_handleToTrophy[h] = details->trophyId;
    *outHandle = h;
    return kOk;
}

int sceNpTrophyGetTrophyUnlockState(std::uint32_t context,
                                    std::uint32_t handle,
                                    std::uint32_t* outFlags) {
    if (!outFlags) return kErrInvalidArg;
    std::uint64_t titleId = 0;
    std::uint64_t trophyId = 0;
    {
        std::lock_guard lock(g_mutex);
        auto it = g_contexts.find(context);
        if (it == g_contexts.end()) return kErrInvalidArg;
        titleId = it->second;
        auto hit = g_handleToTrophy.find(handle);
        if (hit == g_handleToTrophy.end()) return kErrNotFound;
        trophyId = hit->second;
    }

    const bool unlocked =
        PsnBackend::instance().isTrophyUnlocked(titleId, trophyId);
    *outFlags = unlocked ? 1u : 0u;
    return kOk;
}

int sceNpTrophyUnlockTrophy(std::uint32_t context,
                            std::uint32_t handle,
                            std::uint32_t* outPlatinumId) {
    std::uint64_t titleId = 0;
    std::uint64_t trophyId = 0;
    {
        std::lock_guard lock(g_mutex);
        auto it = g_contexts.find(context);
        if (it == g_contexts.end()) return kErrInvalidArg;
        titleId = it->second;
        auto hit = g_handleToTrophy.find(handle);
        if (hit == g_handleToTrophy.end()) return kErrNotFound;
        trophyId = hit->second;
    }
    if (outPlatinumId) *outPlatinumId = 0xFFFFFFFFu;

    auto& psn = PsnBackend::instance();
    if (!psn.isSignedIn()) return kErrNotSignedIn;
    if (psn.isTrophyUnlocked(titleId, trophyId)) return kErrAlreadyUnlocked;

    psn.unlockTrophy(titleId, trophyId);
    FP4_INFO(LogCategory::Sce)
        << "Trophy unlocked: title=" << titleId << " trophy=" << trophyId;
    return kOk;
}

int sceNpTrophyGetGameInfo(std::uint32_t context, std::uint64_t /*handle*/,
                           void* outInfo) {
    if (!outInfo) return kErrInvalidArg;
    std::uint64_t titleId = 0;
    {
        std::lock_guard lock(g_mutex);
        auto it = g_contexts.find(context);
        if (it == g_contexts.end()) return kErrInvalidArg;
        titleId = it->second;
    }
    // SceNpTrophyGameDetails: 4 u32 counts + 16 reserved bytes.
    struct GameInfo {
        std::uint32_t platinum;
        std::uint32_t gold;
        std::uint32_t silver;
        std::uint32_t bronze;
        std::uint8_t  reserved[16];
    };
    static_assert(sizeof(GameInfo) == 32, "GameInfo");
    auto* gi = static_cast<GameInfo*>(outInfo);
    const auto counts = PsnBackend::instance().trophyCounts(titleId);
    gi->platinum = counts.platinum;
    gi->gold     = counts.gold;
    gi->silver   = counts.silver;
    gi->bronze   = counts.bronze;
    std::memset(gi->reserved, 0, sizeof(gi->reserved));
    return kOk;
}

int sceNpTrophyGetTrophyInfo(std::uint32_t context, std::uint32_t handle,
                             void* outInfo) {
    if (!outInfo) return kErrInvalidArg;
    std::uint64_t titleId = 0;
    std::uint32_t trophyId = 0;
    {
        std::lock_guard lock(g_mutex);
        auto it = g_contexts.find(context);
        if (it == g_contexts.end()) return kErrInvalidArg;
        titleId = it->second;
        auto hit = g_handleToTrophy.find(handle);
        if (hit == g_handleToTrophy.end()) return kErrNotFound;
        trophyId = static_cast<std::uint32_t>(hit->second);
    }

    // SceNpTrophyDetails output: grade, hidden, unlock state, name[128],
    // description[512], reserved.
    struct TrophyInfo {
        std::uint32_t grade;
        std::uint32_t hidden;
        std::uint32_t unlocked;
        std::uint32_t reserved;
        char          name[128];
        char          description[512];
        std::uint8_t  reserved2[64];
    };
    static_assert(sizeof(TrophyInfo) == 720, "TrophyInfo");

    auto* ti = static_cast<TrophyInfo*>(outInfo);
    std::memset(ti, 0, sizeof(*ti));

    const auto defs = PsnBackend::instance().trophyDefs(titleId);
    for (const auto& d : defs) {
        if (d.trophyId != trophyId) continue;
        ti->grade   = static_cast<std::uint32_t>(d.grade);
        ti->hidden  = d.hidden ? 1 : 0;
        std::snprintf(ti->name, sizeof(ti->name), "%s", d.title.c_str());
        std::snprintf(ti->description, sizeof(ti->description),
                      "%s", d.description.c_str());
        break;
    }
    ti->unlocked = PsnBackend::instance()
                       .isTrophyUnlocked(titleId, trophyId) ? 1 : 0;
    return kOk;
}

int sceNpTrophyDestroyHandle(std::uint32_t context, std::uint32_t handle) {
    std::lock_guard lock(g_mutex);
    auto it = g_handles.find(context);
    if (it != g_handles.end()) {
        auto& v = it->second;
        v.erase(std::remove(v.begin(), v.end(), handle), v.end());
    }
    g_handleToTrophy.erase(handle);
    return kOk;
}

} // extern "C"

} // namespace

void SceNpTrophy::registerExports(SceStubTable& t) {
    t.registerStub("libSceNpTrophy", "sceNpTrophyCreateContext",
                   reinterpret_cast<void*>(&sceNpTrophyCreateContext));
    t.registerStub("libSceNpTrophy", "sceNpTrophyDestroyContext",
                   reinterpret_cast<void*>(&sceNpTrophyDestroyContext));
    t.registerStub("libSceNpTrophy", "sceNpTrophyRegisterContext",
                   reinterpret_cast<void*>(&sceNpTrophyRegisterContext));
    t.registerStub("libSceNpTrophy", "sceNpTrophyCreateHandle",
                   reinterpret_cast<void*>(&sceNpTrophyCreateHandle));
    t.registerStub("libSceNpTrophy", "sceNpTrophyDestroyHandle",
                   reinterpret_cast<void*>(&sceNpTrophyDestroyHandle));
    t.registerStub("libSceNpTrophy", "sceNpTrophyGetTrophyUnlockState",
                   reinterpret_cast<void*>(&sceNpTrophyGetTrophyUnlockState));
    t.registerStub("libSceNpTrophy", "sceNpTrophyUnlockTrophy",
                   reinterpret_cast<void*>(&sceNpTrophyUnlockTrophy));
    t.registerStub("libSceNpTrophy", "sceNpTrophyGetGameInfo",
                   reinterpret_cast<void*>(&sceNpTrophyGetGameInfo));
    t.registerStub("libSceNpTrophy", "sceNpTrophyGetTrophyInfo",
                   reinterpret_cast<void*>(&sceNpTrophyGetTrophyInfo));
}

} // namespace fusionps4::sce::np
