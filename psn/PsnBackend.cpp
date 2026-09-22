#include "psn/PsnBackend.hpp"

#include "debug/Log.hpp"

#include <algorithm>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using fusionps4::debug::LogCategory;

namespace fusionps4::psn {

namespace {

// File magic and version.
constexpr std::uint32_t kAccountMagic  = 0x4E435041;  // "APCN"
constexpr std::uint32_t kTrophyMagic   = 0x50485254;  // "TRHP"
constexpr std::uint32_t kScoreMagic    = 0x52435350;  // "PSCR"
constexpr std::uint32_t kFormatVersion = 1;

#pragma pack(push, 1)
struct AccountHeader {
    std::uint32_t magic;
    std::uint32_t version;
    std::uint64_t accountId;
    std::uint32_t age;
    std::uint32_t flags;
    char          onlineId[32];
    char          region[8];
    char          language[8];
    std::uint32_t crc;
    std::uint32_t reserved;
};

struct TrophyHeader {
    std::uint32_t magic;
    std::uint32_t version;
    std::uint64_t titleId;
    std::uint32_t defCount;
    std::uint32_t stateCount;
    std::uint32_t reserved[4];
};

struct TrophyDefRecord {
    std::uint32_t trophyId;
    std::uint32_t grade;
    std::uint32_t hidden;
    std::uint32_t reserved;
    char          title[64];
    char          description[128];
};

struct TrophyStateRecord {
    std::uint32_t trophyId;
    std::uint32_t unlocked;
    std::uint64_t unlockedAt;
};

struct ScoreHeader {
    std::uint32_t magic;
    std::uint32_t version;
    std::uint32_t boardId;
    std::uint32_t entryCount;
    std::uint32_t reserved[4];
};

struct ScoreRecord {
    std::uint64_t score;
    std::uint64_t timestamp;
    std::uint32_t rank;
    std::uint32_t reserved;
    char          comment[64];
};
#pragma pack(pop)

// A tiny checksum used to reject a truncated account file. The intent is
// not cryptographic integrity, only "did we read what we wrote".
std::uint32_t fnv1a(const void* data, std::size_t n) {
    const auto* p = static_cast<const std::uint8_t*>(data);
    std::uint32_t h = 0x811C9DC5u;
    for (std::size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 0x01000193u;
    }
    return h;
}

bool writeAll(int fd, const void* data, std::size_t n) {
    const auto* p = static_cast<const std::uint8_t*>(data);
    while (n > 0) {
        const ssize_t w = ::write(fd, p, n);
        if (w <= 0) return false;
        p += w;
        n -= static_cast<std::size_t>(w);
    }
    return true;
}

bool readAll(int fd, void* data, std::size_t n) {
    auto* p = static_cast<std::uint8_t*>(data);
    while (n > 0) {
        const ssize_t r = ::read(fd, p, n);
        if (r <= 0) return false;
        p += r;
        n -= static_cast<std::size_t>(r);
    }
    return true;
}

void safeCopy(char* dst, std::size_t cap, const std::string& src) {
    const auto n = std::min(cap - 1, src.size());
    std::memcpy(dst, src.data(), n);
    dst[n] = '\0';
}

std::string safeRead(const char* src, std::size_t cap) {
    std::size_t n = 0;
    while (n < cap && src[n] != '\0') ++n;
    return std::string(src, n);
}

} // namespace

PsnBackend& PsnBackend::instance() {
    static PsnBackend b;
    return b;
}

std::uint64_t PsnBackend::hashTitleId(const std::string& s) {
    // Deterministic 64-bit hash of a title string. Not cryptographic.
    std::uint64_t h = 0xCBF29CE484222325ull;
    for (char c : s) {
        h ^= static_cast<std::uint8_t>(c);
        h *= 0x100000001B3ull;
    }
    return h;
}

bool PsnBackend::ensureDirectory(const std::string& path) {
    struct stat st{};
    if (::stat(path.c_str(), &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    // Create parents iteratively.
    std::string acc;
    for (std::size_t i = 0; i < path.size(); ++i) {
        acc.push_back(path[i]);
        if (path[i] == '/' && acc.size() > 1) {
            ::mkdir(acc.substr(0, acc.size() - 1).c_str(), 0700);
        }
    }
    return ::mkdir(path.c_str(), 0700) == 0 || errno == EEXIST;
}

bool PsnBackend::initialize(const std::string& persistRoot,
                            const std::string& onlineId) {
    std::lock_guard lock(m_mutex);
    if (!m_persistRoot.empty()) return true;

    m_persistRoot = persistRoot;
    m_accountDir  = persistRoot + "/" + onlineId;

    if (!ensureDirectory(persistRoot) ||
        !ensureDirectory(m_accountDir) ||
        !ensureDirectory(m_accountDir + "/trophies") ||
        !ensureDirectory(m_accountDir + "/scores")) {
        FP4_ERROR(LogCategory::Sce)
            << "PsnBackend: cannot create account directory \""
            << m_accountDir << "\"";
        return false;
    }

    m_account.onlineId = onlineId;

    if (!loadAccount()) {
        // First run: fabricate a stable account id from the onlineId so
        // subsequent runs see the same identity.
        m_account.accountId = hashTitleId(onlineId) & 0x7FFFFFFFull;
        m_account.onlineId  = onlineId;
        m_account.region    = "us";
        m_account.language  = "en";
        m_account.age       = 25;
        m_account.flags     = 0;
        saveAccount();
        FP4_INFO(LogCategory::Sce)
            << "PSN simulated account created: \"" << onlineId
            << "\" (accountId=" << m_account.accountId << ")";
    } else {
        FP4_INFO(LogCategory::Sce)
            << "PSN simulated account loaded: \"" << m_account.onlineId
            << "\" (accountId=" << m_account.accountId << ")";
    }

    m_signedIn = true;
    return true;
}

void PsnBackend::shutdown() {
    std::lock_guard lock(m_mutex);
    for (auto& [id, t] : m_titles) if (t.loaded) saveTrophies(id);
    for (auto& [id, b] : m_boards) if (b.loaded) saveScores(id);
    m_titles.clear();
    m_boards.clear();
    m_signedIn = false;
}

bool PsnBackend::signIn() {
    std::lock_guard lock(m_mutex);
    if (m_signedIn) return true;
    m_signedIn = true;
    FP4_INFO(LogCategory::Sce)
        << "PSN simulated sign-in: \"" << m_account.onlineId << "\"";
    return true;
}

void PsnBackend::signOut() {
    std::lock_guard lock(m_mutex);
    m_signedIn = false;
}

bool PsnBackend::loadAccount() {
    const auto path = m_accountDir + "/account.dat";
    const int fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0) return false;

    AccountHeader h{};
    if (!readAll(fd, &h, sizeof(h))) { ::close(fd); return false; }
    ::close(fd);

    if (h.magic != kAccountMagic || h.version != kFormatVersion) return false;

    m_account.accountId = h.accountId;
    m_account.onlineId  = safeRead(h.onlineId, sizeof(h.onlineId));
    m_account.region    = safeRead(h.region, sizeof(h.region));
    m_account.language  = safeRead(h.language, sizeof(h.language));
    m_account.age       = h.age;
    m_account.flags     = h.flags;
    return true;
}

void PsnBackend::saveAccount() {
    const auto path = m_accountDir + "/account.dat";
    const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) return;

    AccountHeader h{};
    h.magic     = kAccountMagic;
    h.version   = kFormatVersion;
    h.accountId = m_account.accountId;
    h.age       = m_account.age;
    h.flags     = m_account.flags;
    safeCopy(h.onlineId, sizeof(h.onlineId), m_account.onlineId);
    safeCopy(h.region,   sizeof(h.region),   m_account.region);
    safeCopy(h.language, sizeof(h.language), m_account.language);
    h.crc = fnv1a(h.onlineId, sizeof(h.onlineId)) ^
            fnv1a(h.region,   sizeof(h.region))   ^
            fnv1a(h.language, sizeof(h.language));

    writeAll(fd, &h, sizeof(h));
    ::close(fd);
}

bool PsnBackend::loadTrophies(std::uint64_t titleId) {
    auto& ts = m_titles[titleId];
    if (ts.loaded) return true;

    char nameBuf[32];
    std::snprintf(nameBuf, sizeof(nameBuf), "%016llx.dat",
                  (unsigned long long)titleId);
    const auto path = m_accountDir + "/trophies/" + nameBuf;

    const int fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0) { ts.loaded = true; return true; }

    TrophyHeader h{};
    if (!readAll(fd, &h, sizeof(h)) ||
        h.magic != kTrophyMagic || h.version != kFormatVersion ||
        h.titleId != titleId) {
        ::close(fd);
        ts.loaded = true;
        return false;
    }

    ts.defs.resize(h.defCount);
    for (std::size_t i = 0; i < h.defCount; ++i) {
        TrophyDefRecord r{};
        if (!readAll(fd, &r, sizeof(r))) { ::close(fd); return false; }
        ts.defs[i].trophyId    = r.trophyId;
        ts.defs[i].grade       = static_cast<TrophyGrade>(r.grade);
        ts.defs[i].hidden      = r.hidden != 0;
        ts.defs[i].title       = safeRead(r.title, sizeof(r.title));
        ts.defs[i].description = safeRead(r.description, sizeof(r.description));
    }

    ts.states.resize(h.stateCount);
    for (std::size_t i = 0; i < h.stateCount; ++i) {
        TrophyStateRecord r{};
        if (!readAll(fd, &r, sizeof(r))) { ::close(fd); return false; }
        ts.states[i].trophyId   = r.trophyId;
        ts.states[i].unlocked   = r.unlocked != 0;
        ts.states[i].unlockedAt = r.unlockedAt;
    }

    ::close(fd);
    ts.loaded = true;
    return true;
}

void PsnBackend::saveTrophies(std::uint64_t titleId) {
    auto it = m_titles.find(titleId);
    if (it == m_titles.end()) return;
    const auto& ts = it->second;

    char nameBuf[32];
    std::snprintf(nameBuf, sizeof(nameBuf), "%016llx.dat",
                  (unsigned long long)titleId);
    const auto path = m_accountDir + "/trophies/" + nameBuf;

    const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) return;

    TrophyHeader h{};
    h.magic      = kTrophyMagic;
    h.version    = kFormatVersion;
    h.titleId    = titleId;
    h.defCount   = static_cast<std::uint32_t>(ts.defs.size());
    h.stateCount = static_cast<std::uint32_t>(ts.states.size());
    writeAll(fd, &h, sizeof(h));

    for (const auto& d : ts.defs) {
        TrophyDefRecord r{};
        r.trophyId = d.trophyId;
        r.grade    = static_cast<std::uint32_t>(d.grade);
        r.hidden   = d.hidden ? 1 : 0;
        safeCopy(r.title, sizeof(r.title), d.title);
        safeCopy(r.description, sizeof(r.description), d.description);
        writeAll(fd, &r, sizeof(r));
    }
    for (const auto& s : ts.states) {
        TrophyStateRecord r{};
        r.trophyId   = s.trophyId;
        r.unlocked   = s.unlocked ? 1 : 0;
        r.unlockedAt = s.unlockedAt;
        writeAll(fd, &r, sizeof(r));
    }
    ::close(fd);
}

void PsnBackend::registerTitle(std::uint64_t titleId,
                               const std::vector<TrophyDef>& defs) {
    std::lock_guard lock(m_mutex);
    loadTrophies(titleId);
    auto& ts = m_titles[titleId];
    ts.defs = defs;

    // Reconcile state: preserve unlock flags from a previous run, add an
    // entry for each new trophy definition.
    std::unordered_map<std::uint32_t, bool> prior;
    for (const auto& s : ts.states) prior[s.trophyId] = s.unlocked;

    ts.states.clear();
    for (const auto& d : defs) {
        TrophyState s;
        s.trophyId = d.trophyId;
        auto pit = prior.find(d.trophyId);
        if (pit != prior.end()) s.unlocked = pit->second;
        ts.states.push_back(s);
    }
    saveTrophies(titleId);
    FP4_INFO(LogCategory::Sce)
        << "PSN trophy title " << titleId << " registered: "
        << defs.size() << " trophies";
}

bool PsnBackend::isTrophyUnlocked(std::uint64_t titleId,
                                  std::uint32_t trophyId) const {
    std::lock_guard lock(m_mutex);
    auto it = m_titles.find(titleId);
    if (it == m_titles.end()) return false;
    for (const auto& s : it->second.states) {
        if (s.trophyId == trophyId) return s.unlocked;
    }
    return false;
}

bool PsnBackend::unlockTrophy(std::uint64_t titleId, std::uint32_t trophyId) {
    std::lock_guard lock(m_mutex);
    auto it = m_titles.find(titleId);
    if (it == m_titles.end()) return false;

    for (auto& s : it->second.states) {
        if (s.trophyId == trophyId) {
            if (s.unlocked) return false;   // already unlocked
            s.unlocked = true;
            s.unlockedAt = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count());
            saveTrophies(titleId);
            FP4_INFO(LogCategory::Sce)
                << "PSN trophy unlocked: title=" << titleId
                << " trophy=" << trophyId;
            return true;
        }
    }
    return false;
}

std::vector<TrophyState> PsnBackend::trophyStates(std::uint64_t titleId) const {
    std::lock_guard lock(m_mutex);
    auto it = m_titles.find(titleId);
    return it == m_titles.end() ? std::vector<TrophyState>{}
                                : it->second.states;
}

std::vector<TrophyDef> PsnBackend::trophyDefs(std::uint64_t titleId) const {
    std::lock_guard lock(m_mutex);
    auto it = m_titles.find(titleId);
    return it == m_titles.end() ? std::vector<TrophyDef>{}
                                : it->second.defs;
}

PsnBackend::TrophyCounts PsnBackend::trophyCounts(std::uint64_t titleId) const {
    std::lock_guard lock(m_mutex);
    TrophyCounts c{};
    auto it = m_titles.find(titleId);
    if (it == m_titles.end()) return c;
    for (const auto& s : it->second.states) {
        if (!s.unlocked) continue;
        for (const auto& d : it->second.defs) {
            if (d.trophyId != s.trophyId) continue;
            switch (d.grade) {
                case TrophyGrade::Platinum: c.platinum++; break;
                case TrophyGrade::Gold:     c.gold++;     break;
                case TrophyGrade::Silver:   c.silver++;   break;
                case TrophyGrade::Bronze:   c.bronze++;   break;
            }
            break;
        }
    }
    return c;
}

bool PsnBackend::loadScores(std::uint32_t boardId) {
    auto& bs = m_boards[boardId];
    if (bs.loaded) return true;

    char nameBuf[32];
    std::snprintf(nameBuf, sizeof(nameBuf), "%08x.dat", boardId);
    const auto path = m_accountDir + "/scores/" + nameBuf;

    const int fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0) { bs.loaded = true; return true; }

    ScoreHeader h{};
    if (!readAll(fd, &h, sizeof(h)) ||
        h.magic != kScoreMagic || h.version != kFormatVersion) {
        ::close(fd);
        bs.loaded = true;
        return false;
    }

    bs.entries.resize(h.entryCount);
    for (std::size_t i = 0; i < h.entryCount; ++i) {
        ScoreRecord r{};
        if (!readAll(fd, &r, sizeof(r))) { ::close(fd); return false; }
        bs.entries[i].boardId   = boardId;
        bs.entries[i].score     = r.score;
        bs.entries[i].timestamp = r.timestamp;
        bs.entries[i].rank      = r.rank;
        bs.entries[i].comment   = safeRead(r.comment, sizeof(r.comment));
    }
    ::close(fd);
    bs.loaded = true;
    return true;
}

void PsnBackend::saveScores(std::uint32_t boardId) {
    auto it = m_boards.find(boardId);
    if (it == m_boards.end()) return;
    const auto& bs = it->second;

    char nameBuf[32];
    std::snprintf(nameBuf, sizeof(nameBuf), "%08x.dat", boardId);
    const auto path = m_accountDir + "/scores/" + nameBuf;

    const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) return;

    ScoreHeader h{};
    h.magic      = kScoreMagic;
    h.version    = kFormatVersion;
    h.boardId    = boardId;
    h.entryCount = static_cast<std::uint32_t>(bs.entries.size());
    writeAll(fd, &h, sizeof(h));

    for (const auto& e : bs.entries) {
        ScoreRecord r{};
        r.score     = e.score;
        r.timestamp = e.timestamp;
        r.rank      = e.rank;
        safeCopy(r.comment, sizeof(r.comment), e.comment);
        writeAll(fd, &r, sizeof(r));
    }
    ::close(fd);
}

void PsnBackend::submitScore(std::uint32_t boardId, std::uint64_t score,
                             const std::string& comment) {
    std::lock_guard lock(m_mutex);
    loadScores(boardId);
    auto& bs = m_boards[boardId];

    ScoreEntry e;
    e.boardId   = boardId;
    e.score     = score;
    e.comment   = comment;
    e.timestamp = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
    bs.entries.push_back(e);

    // Sort descending and recompute ranks.
    std::sort(bs.entries.begin(), bs.entries.end(),
              [](const ScoreEntry& a, const ScoreEntry& b) {
                  return a.score > b.score;
              });
    for (std::size_t i = 0; i < bs.entries.size(); ++i) {
        bs.entries[i].rank = static_cast<std::uint32_t>(i + 1);
    }
    saveScores(boardId);
    FP4_DEBUG(LogCategory::Sce)
        << "PSN score submitted: board=" << boardId << " score=" << score;
}

std::vector<ScoreEntry> PsnBackend::scorescoreboard(std::uint32_t boardId) const
