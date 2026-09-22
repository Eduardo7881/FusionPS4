#include "sce/rtc/SceRtc.hpp"

#include "debug/Log.hpp"
#include "sce/SceStubTable.hpp"

#include <chrono>
#include <cstring>
#include <ctime>
#include <sys/time.h>

using fusionps4::debug::LogCategory;

namespace fusionps4::sce::rtc {

SceRtc& SceRtc::instance() {
    static SceRtc s;
    return s;
}

bool SceRtc::initialize() {
    m_initialized = true;
    FP4_INFO(LogCategory::Sce) << "libSceRtc initialized";
    return true;
}

void SceRtc::shutdown() { m_initialized = false; }

namespace {

constexpr int kOk              = 0;
constexpr int kErrInvalidArg   = static_cast<int>(0x80020005u);
constexpr int kErrOverflow     = static_cast<int>(0x80020002u);

// Seconds between 0001-01-01 and 1970-01-01, in the proleptic Gregorian
// calendar as required by the PS4 RTC.
constexpr std::uint64_t kUnixToSceEpochSeconds = 62135596800ULL;

// A SceRtcDateTime on PS4 is a 16-byte struct of BCD fields:
//   [0]  second    BCD
//   [1]  minute    BCD
//   [2]  hour      BCD
//   [3]  day       BCD
//   [4]  month     BCD
//   [5]  year (2-digit, BCD)
//   [6..7]  reserved
//   [8..11] microseconds (uint32, native)
//   [12..15] reserved
struct SceRtcDateTime {
    std::uint8_t  second_bcd;
    std::uint8_t  minute_bcd;
    std::uint8_t  hour_bcd;
    std::uint8_t  day_bcd;
    std::uint8_t  month_bcd;
    std::uint8_t  year_bcd;
    std::uint8_t  reserved0[2];
    std::uint32_t microsecond;
    std::uint8_t  reserved1[4];
};
static_assert(sizeof(SceRtcDateTime) == 0x18,
              "SceRtcDateTime must be 0x18 bytes");

std::uint8_t toBcd(int v) {
    return static_cast<std::uint8_t>(((v / 10) << 4) | (v % 10));
}
int fromBcd(std::uint8_t b) {
    return ((b >> 4) & 0xF) * 10 + (b & 0xF);
}

// Convert a PS4 tick value to broken-down UTC. Uses timegm() semantics
// via days-from-civil for determinism.
void tickToCivil(std::uint64_t ticks, SceRtcDateTime& out) {
    const std::uint64_t totalSeconds = ticks / 1000000ULL;
    const std::uint32_t micros = static_cast<std::uint32_t>(ticks % 1000000ULL);

    // Seconds since Unix epoch.
    const std::int64_t unixSec =
        static_cast<std::int64_t>(totalSeconds) -
        static_cast<std::int64_t>(kUnixToSceEpochSeconds);

    std::time_t t = static_cast<std::time_t>(unixSec);
    std::tm tm{};
    gmtime_r(&t, &tm);

    std::memset(&out, 0, sizeof(out));
    out.second_bcd    = toBcd(tm.tm_sec);
    out.minute_bcd    = toBcd(tm.tm_min);
    out.hour_bcd      = toBcd(tm.tm_hour);
    out.day_bcd       = toBcd(tm.tm_mday);
    out.month_bcd     = toBcd(tm.tm_mon + 1);
    out.year_bcd      = toBcd((tm.tm_year + 1900) % 100);
    out.microsecond   = micros;
}

// Inverse: broken-down UTC to ticks.
std::uint64_t civilToTick(const SceRtcDateTime& in) {
    std::tm tm{};
    tm.tm_sec  = fromBcd(in.second_bcd);
    tm.tm_min  = fromBcd(in.minute_bcd);
    tm.tm_hour = fromBcd(in.hour_bcd);
    tm.tm_mday = fromBcd(in.day_bcd);
    tm.tm_mon  = fromBcd(in.month_bcd) - 1;
    tm.tm_year = fromBcd(in.year_bcd) + 100;   // 2000-2099 por convenção PS4
    tm.tm_isdst = 0;

    const std::time_t t = timegm(&tm);
    const std::uint64_t seconds =
        static_cast<std::uint64_t>(t) + kUnixToSceEpochSeconds;
    return seconds * 1000000ULL + in.microsecond;
}

extern "C" {

int sceRtcGetCurrentTick(std::uint64_t* tick) {
    if (!tick) return kErrInvalidArg;
    struct ::timeval tv{};
    if (::gettimeofday(&tv, nullptr) != 0) return kErrOverflow;
    const std::uint64_t s =
        static_cast<std::uint64_t>(tv.tv_sec) + kUnixToSceEpochSeconds;
    *tick = s * 1000000ULL + static_cast<std::uint64_t>(tv.tv_usec);
    return kOk;
}

int sceRtcGetCurrentClock(SceRtcDateTime* dt, int tzMinutes) {
    if (!dt) return kErrInvalidArg;
    std::uint64_t ticks = 0;
    sceRtcGetCurrentTick(&ticks);
    if (tzMinutes != 0) {
        ticks += static_cast<std::uint64_t>(tzMinutes) * 60ULL * 1000000ULL;
    }
    tickToCivil(ticks, *dt);
    return kOk;
}

int sceRtcGetCurrentClockLocalTime(SceRtcDateTime* dt) {
    return sceRtcGetCurrentClock(dt, 0);
}

int sceRtcSetTick(SceRtcDateTime* dt, const std::uint64_t* tick) {
    if (!dt || !tick) return kErrInvalidArg;
    tickToCivil(*tick, *dt);
    return kOk;
}

int sceRtcGetTick(const SceRtcDateTime* dt, std::uint64_t* tick) {
    if (!dt || !tick) return kErrInvalidArg;
    *tick = civilToTick(*dt);
    return kOk;
}

int sceRtcGetTickResolution(std::uint64_t* resolution) {
    if (!resolution) return kErrInvalidArg;
    *resolution = 1000000ULL;
    return kOk;
}

int sceRtcConvertUtcToLocalTime(const std::uint64_t* utcTick,
                                std::uint64_t* outTick) {
    if (!utcTick || !outTick) return kErrInvalidArg;
    *outTick = *utcTick;   // runtime keeps UTC; guest applies its own offset
    return kOk;
}

int sceRtcConvertLocalTimeToUtc(const std::uint64_t* localTick,
                                std::uint64_t* outTick) {
    return sceRtcConvertUtcToLocalTime(localTick, outTick);
}

// sceRtcGetCurrentNetworkTick is used by games that need a monotonic
// tick whose delta is used for animations. We return the same value as
// the local tick: both are UTC-based on PS4.
int sceRtcGetCurrentNetworkTick(std::uint64_t* tick) {
    return sceRtcGetCurrentTick(tick);
}

} // extern "C"

} // namespace

void SceRtc::registerExports(SceStubTable& t) {
    t.registerStub("libSceRtc", "sceRtcGetCurrentTick",
                   reinterpret_cast<void*>(&sceRtcGetCurrentTick));
    t.registerStub("libSceRtc", "sceRtcGetCurrentClock",
                   reinterpret_cast<void*>(&sceRtcGetCurrentClock));
    t.registerStub("libSceRtc", "sceRtcGetCurrentClockLocalTime",
                   reinterpret_cast<void*>(&sceRtcGetCurrentClockLocalTime));
    t.registerStub("libSceRtc", "sceRtcSetTick",
                   reinterpret_cast<void*>(&sceRtcSetTick));
    t.registerStub("libSceRtc", "sceRtcGetTick",
                   reinterpret_cast<void*>(&sceRtcGetTick));
    t.registerStub("libSceRtc", "sceRtcGetTickResolution",
                   reinterpret_cast<void*>(&sceRtcGetTickResolution));
    t.registerStub("libSceRtc", "sceRtcConvertUtcToLocalTime",
                   reinterpret_cast<void*>(&sceRtcConvertUtcToLocalTime));
    t.registerStub("libSceRtc", "sceRtcConvertLocalTimeToUtc",
                   reinterpret_cast<void*>(&sceRtcConvertLocalTimeToUtc));
    t.registerStub("libSceRtc", "sceRtcGetCurrentNetworkTick",
                   reinterpret_cast<void*>(&sceRtcGetCurrentNetworkTick));
}

} // namespace fusionps4::sce::rtc
