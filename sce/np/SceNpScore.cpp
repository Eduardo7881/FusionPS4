#include "sce/np/SceNpScore.hpp"
#include "debug/Log.hpp"
#include "psn/PsnBackend.hpp"
#include "sce/SceStubTable.hpp"
#include <cstring>
using fusionps4::debug::LogCategory;
using fusionps4::psn::PsnBackend;

namespace fusionps4::sce::np {

SceNpScore& SceNpScore::instance() { static SceNpScore s; return s; }
bool SceNpScore::initialize() {
    m_initialized = true;
    FP4_INFO(LogCategory::Sce) << "libSceNpScore initialized";
    return true;
}
void SceNpScore::shutdown() { m_initialized = false; }

namespace {

constexpr int kOk             = 0;
constexpr int kErrInvalidArg  = static_cast<int>(0x80020005u);
constexpr int kErrNotSignedIn = static_cast<int>(0x80550008);

extern "C" {

int sceNpScoreInit() { return kOk; }
int sceNpScoreTerm() { return kOk; }

int sceNpScoreCreateTitleCtx(const char* /*titleId*/, void* /*ctx*/,
                             void* /*opt*/) {
    return kOk;
}

int sceNpScoreCreateRequest() {
    // Score requests use ephemeral integer handles.
    static std::uint32_t next = 1;
    return static_cast<int>(next++);
}

int sceNpScoreDeleteRequest(int /*reqId*/) { return kOk; }

int sceNpScoreSetScore(std::uint32_t boardId, std::uint64_t score,
                       const char* comment, void* /*opt*/) {
    auto& psn = PsnBackend::instance();
    if (!psn.isSignedIn()) return kErrNotSignedIn;
    psn.submitScore(boardId, score, comment ? comment : "");
    return kOk;
}

int sceNpScoreGetRankingByRange(std::uint32_t boardId,
                                std::uint32_t startRank,
                                std::uint32_t /*numRanks*/,
                                void* outEntries, std::size_t outEntrySize,
                                std::size_t* outCount) {
    if (!outEntries || !outCount) return kErrInvalidArg;
    auto& psn = PsnBackend::instance();
    if (!psn.isSignedIn()) return kErrNotSignedIn;

    // SceNpScoreRankData: rank u32, score u64, padding, accountId u64,
    // comment[32], reserved.
    struct RankEntry {
        std::uint32_t rank;
        std::uint32_t reserved;
        std::uint64_t score;
        std::uint64_t accountId;
        char          comment[32];
        std::uint8_t  reserved2[64];
    };
    static_assert(sizeof(RankEntry) == 128, "RankEntry");

    if (outEntrySize < sizeof(RankEntry)) return kErrInvalidArg;

    const auto entries = psn.scoreboard(boardId);
    std::size_t n = 0;
    for (std::size_t i = startRank - 1; i < entries.size(); ++i) {
        if ((n + 1) * sizeof(RankEntry) > outEntrySize) break;
        auto* dst = reinterpret_cast<RankEntry*>(
            static_cast<std::uint8_t*>(outEntries) + n * sizeof(RankEntry));
        std::memset(dst, 0, sizeof(*dst));
        dst->rank      = entries[i].rank;
        dst->score     = entries[i].score;
        dst->accountId = psn.account().accountId;
        std::snprintf(dst->comment, sizeof(dst->comment), "%s",
                      entries[i].comment.c_str());
        ++n;
    }
    *outCount = n;
    return kOk;
}

int sceNpScoreGetRankingByNpId(std::uint32_t /*boardId*/,
                               const void* /*npId*/,
                               void* outEntries, std::size_t /*outEntrySize*/,
                               std::size_t* outCount) {
    if (outEntries) std::memset(outEntries, 0, 128);
    if (outCount) *outCount = 0;
    return kOk;
}

} // extern "C"

} // namespace

void SceNpScore::registerExports(SceStubTable& t) {
    t.registerStub("libSceNpScore", "sceNpScoreInit",
                   reinterpret_cast<void*>(&sceNpScoreInit));
    t.registerStub("libSceNpScore", "sceNpScoreTerm",
                   reinterpret_cast<void*>(&sceNpScoreTerm));
    t.registerStub("libSceNpScore", "sceNpScoreCreateTitleCtx",
                   reinterpret_cast<void*>(&sceNpScoreCreateTitleCtx));
    t.registerStub("libSceNpScore", "sceNpScoreCreateRequest",
                   reinterpret_cast<void*>(&sceNpScoreCreateRequest));
    t.registerStub("libSceNpScore", "sceNpScoreDeleteRequest",
                   reinterpret_cast<void*>(&sceNpScoreDeleteRequest));
    t.registerStub("libSceNpScore", "sceNpScoreSetScore",
                   reinterpret_cast<void*>(&sceNpScoreSetScore));
    t.registerStub("libSceNpScore", "sceNpScoreGetRankingByRange",
                   reinterpret_cast<void*>(&sceNpScoreGetRankingByRange));
    t.registerStub("libSceNpScore", "sceNpScoreGetRankingByNpId",
                   reinterpret_cast<void*>(&sceNpScoreGetRankingByNpId));
}

} // namespace fusionps4::sce::np
