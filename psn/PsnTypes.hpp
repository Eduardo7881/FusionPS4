#pragma once

#include <cstdint>
#include <string>

namespace fusionps4::psn {

// PS4 trophy grades. Values match SceNpTrophyGrade.
enum class TrophyGrade : std::uint32_t {
    Platinum = 0,
    Gold     = 1,
    Silver   = 2,
    Bronze   = 3,
};

struct AccountInfo {
    std::uint64_t accountId  = 0;             // decimal "PSN account id"
    std::string   onlineId   = "Player1";     // the visible name
    std::string   region     = "us";
    std::string   language   = "en";
    std::uint32_t age        = 25;
    std::uint32_t flags      = 0;             // bit0 = PS Plus (simulado)
};

struct TrophyDef {
    std::uint32_t trophyId   = 0;
    TrophyGrade   grade      = TrophyGrade::Bronze;
    std::string   title;
    std::string   description;
    bool          hidden     = false;
};

struct TrophyState {
    std::uint32_t trophyId   = 0;
    bool          unlocked   = false;
    std::uint64_t unlockedAt = 0;   // microseconds since Unix epoch
};

struct ScoreEntry {
    std::uint32_t boardId   = 0;
    std::uint64_t score     = 0;
    std::string   comment;
    std::uint64_t timestamp = 0;
    std::uint32_t rank      = 0;
};

} // namespace fusionps4::psn
