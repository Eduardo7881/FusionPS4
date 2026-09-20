#include "debug/Log.hpp"

#include <array>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>

namespace fusionps4::debug {

namespace {

constexpr std::array<const char*, static_cast<std::size_t>(LogCategory::Count)>
    kCategoryNames = {
        "HOST", "LOADER", "SYSCALL", "SCE",  "FS",     "INPUT",
        "AUDIO", "NETWORK", "MEMORY", "GRAPHICS", "VULKAN", "OPENGL",
        "THREAD", "PROCESS", "HANDLE", "ERROR"};

constexpr std::array<const char*, 6> kLevelNames = {
    "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};

std::string nowTimestamp() {
    using clock = std::chrono::system_clock;
    const auto tp = clock::now();
    const auto t  = clock::to_time_t(tp);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        tp.time_since_epoch()) %
                    1000;

    std::tm tm{};
    localtime_r(&t, &tm);

    char buf[64];
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03d",
                  tm.tm_hour, tm.tm_min, tm.tm_sec,
                  static_cast<int>(ms.count()));
    return buf;
}

} // namespace

const char* logCategoryName(LogCategory c) {
    const auto idx = static_cast<std::size_t>(c);
    return idx < kCategoryNames.size() ? kCategoryNames[idx] : "?";
}

const char* logLevelName(LogLevel l) {
    const auto idx = static_cast<std::size_t>(l);
    return idx < kLevelNames.size() ? kLevelNames[idx] : "?";
}

std::mutex  Log::s_mutex;
LogLevel    Log::s_minLevel = LogLevel::Debug;
bool        Log::s_catEnabled[static_cast<std::size_t>(LogCategory::Count)] = {};
FILE*       Log::s_file = nullptr;
bool        Log::s_ownFile = false;
bool        Log::s_toStderr = true;

void Log::init() {
    std::lock_guard lock(s_mutex);
    for (auto& v : s_catEnabled) v = true;
    s_file     = nullptr;
    s_ownFile  = false;
    s_toStderr = true;
    s_minLevel = LogLevel::Debug;

    if (const char* env = std::getenv("FUSIONPS4_LOG")) {
        std::string s = env;
        for (auto& ch : s) ch = static_cast<char>(std::toupper(ch));
        if (s == "TRACE") s_minLevel = LogLevel::Trace;
        else if (s == "DEBUG") s_minLevel = LogLevel::Debug;
        else if (s == "INFO") s_minLevel = LogLevel::Info;
        else if (s == "WARN") s_minLevel = LogLevel::Warn;
        else if (s == "ERROR") s_minLevel = LogLevel::Error;
    }
}

void Log::shutdown() {
    std::lock_guard lock(s_mutex);
    if (s_ownFile && s_file) {
        std::fclose(s_file);
    }
    s_file = nullptr;
    s_ownFile = false;
}

void Log::setMinLevel(LogLevel lvl) {
    std::lock_guard lock(s_mutex);
    s_minLevel = lvl;
}

LogLevel Log::minLevel() {
    std::lock_guard lock(s_mutex);
    return s_minLevel;
}

void Log::setOutputFile(const std::string& path) {
    std::lock_guard lock(s_mutex);
    if (s_ownFile && s_file) {
        std::fclose(s_file);
        s_file = nullptr;
        s_ownFile = false;
    }
    s_file = std::fopen(path.c_str(), "w");
    if (s_file) s_ownFile = true;
}

void Log::setToStderr(bool enabled) {
    std::lock_guard lock(s_mutex);
    s_toStderr = enabled;
}

void Log::setCategoryEnabled(LogCategory c, bool enabled) {
    std::lock_guard lock(s_mutex);
    s_catEnabled[static_cast<std::size_t>(c)] = enabled;
}

bool Log::isEnabled(LogLevel lvl, LogCategory c) {
    std::lock_guard lock(s_mutex);
    if (static_cast<std::uint32_t>(lvl) <
        static_cast<std::uint32_t>(s_minLevel)) {
        return false;
    }
    return s_catEnabled[static_cast<std::size_t>(c)];
}

void Log::write(LogLevel lvl, LogCategory c, const std::string& msg) {
    std::lock_guard lock(s_mutex);
    if (static_cast<std::uint32_t>(lvl) <
        static_cast<std::uint32_t>(s_minLevel)) {
        return;
    }
    if (!s_catEnabled[static_cast<std::size_t>(c)]) return;

    const std::string line = "[" + nowTimestamp() + "][" +
                             logLevelName(lvl) + "][" + logCategoryName(c) +
                             "] " + msg + "\n";

    if (s_toStderr) {
        std::fwrite(line.data(), 1, line.size(), stderr);
    }
    if (s_file) {
        std::fwrite(line.data(), 1, line.size(), s_file);
        std::fflush(s_file);
    }
}

} // namespace fusionps4::debug
