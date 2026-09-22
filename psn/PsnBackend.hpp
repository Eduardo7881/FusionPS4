#pragma once

#include "psn/PsnTypes.hpp"

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace fusionps4::psn {

// Local PSN simulation. Everything is persisted under a per-account
// directory:
//
//     <persistRoot>/<onlineId>/
//         account.dat      # binary AccountInfo
//         trophies/<titleId>.dat
//         scores/<boardId>.dat
//
// The file formats are private to this runtime and evolve with it; a
// version field is written at the head of each file so a future format
// change can migrate rather than silently misparse.
//
// No network call is ever made. The point of this backend is to give
// titles that expect a signed-in user a consistent, persistent identity
// and per-title trophy state.
class PsnBackend {
public:
    static PsnBackend& instance();

    // Loads or creates the account directory. Called once at runtime init.
    // `persistRoot` typically resolves to "<cwd>/persist".
    bool initialize(const std::string& persistRoot,
                    const std::string& onlineId);

    void shutdown();

    // ---- account --------------------------------------------------------
    const AccountInfo& account() const { return m_account; }
    bool isSignedIn() const { return m_signedIn; }

    // Simulated sign-in is immediate: there is no real account to
    // authenticate against, so this just flags state and updates the log.
    bool signIn();
    void signOut();

    // ---- trophies -------------------------------------------------------
    // Registering a set of trophy definitions for a title (normally called
    // from sceNpTrophyRegisterContext). Persisted so that title identities
    // remain stable across runs.
    void registerTitle(std::uint64_t titleId,
                       const std::vector<TrophyDef>& defs);

    // Unlock state.
    bool isTrophyUnlocked(std::uint64_t titleId, std::uint32_t trophyId) const;
    bool unlockTrophy(std::uint64_t titleId, std::uint32_t trophyId);

    std::vector<TrophyState> trophyStates(std::uint64_t titleId) const;
    std::vector<TrophyDef>   trophyDefs(std::uint64_t titleId) const;

    // Aggregated counts by grade.
    struct TrophyCounts {
        std::uint32_t platinum = 0;
        std::uint32_t gold     = 0;
        std::uint32_t silver   = 0;
        std::uint32_t bronze   = 0;
    };
    TrophyCounts trophyCounts(std::uint64_t titleId) const;

    // ---- scores ---------------------------------------------------------
    // Score submission (leaderboard). The runtime keeps a local board per
    // board id; ranks are recomputed on each submit.
    void submitScore(std::uint32_t boardId, std::uint64_t score,
                     const std::string& comment);

    std::vector<ScoreEntry> scoreboard(std::uint32_t boardId) const;

    // ---- paths ----------------------------------------------------------
    const std::string& accountDir()  const { return m_accountDir;  }
    const std::string& persistRoot() const { return m_persistRoot; }

private:
    PsnBackend() = default;

    bool loadAccount();
    void saveAccount();

    bool loadTrophies(std::uint64_t titleId);
    void saveTrophies(std::uint64_t titleId);

    bool loadScores(std::uint32_t boardId);
    void saveScores(std::uint32_t boardId);

    static std::uint64_t hashTitleId(const std::string& s);
    static bool          ensureDirectory(const std::string& path);

    mutable std::mutex                                       m_mutex;
    std::string                                              m_persistRoot;
    std::string                                              m_accountDir;

    AccountInfo                                              m_account{};
    bool                                                     m_signedIn = false;

    struct TitleState {
        std::vector<TrophyDef>   defs;
        std::vector<TrophyState> states;
        bool                     loaded = false;
    };
    std::unordered_map<std::uint64_t, TitleState>            m_titles;

    struct BoardState {
        std::vector<ScoreEntry> entries;
        bool                    loaded = false;
    };
    std::unordered_map<std::uint32_t, BoardState>            m_boards;
};

} // namespace fusionps4::psn
