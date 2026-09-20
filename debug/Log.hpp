#pragma once

#include <cstdint>
#include <cstdio>
#include <mutex>
#include <sstream>
#include <string>

namespace fusionps4::debug {

enum class LogCategory : std::uint32_t {
    Host = 0,
    Loader,
    Syscall,
    Sce,
    Fs,
    Input,
    Audio,
    Network,
    Memory,
    Graphics,
    Vulkan,
    OpenGL,
    Thread,
    Process,
    Handle,
    Error,
    Count
};

enum class LogLevel : std::uint32_t {
    Trace = 0,
    Debug,
    Info,
    Warn,
    Error,
    Fatal
};

const char* logCategoryName(LogCategory c);
const char* logLevelName(LogLevel l);

class Log {
public:
    static void init();
    static void shutdown();

    static void setMinLevel(LogLevel lvl);
    static LogLevel minLevel();

    static void setOutputFile(const std::string& path);
    static void setToStderr(bool enabled);

    static void setCategoryEnabled(LogCategory c, bool enabled);

    static bool isEnabled(LogLevel lvl, LogCategory c);
    static void write(LogLevel lvl, LogCategory c, const std::string& msg);

private:
    static std::mutex  s_mutex;
    static LogLevel    s_minLevel;
    static bool        s_catEnabled[static_cast<std::size_t>(LogCategory::Count)];
    static FILE*       s_file;
    static bool        s_ownFile;
    static bool        s_toStderr;
};

class LogStream {
public:
    LogStream(LogLevel lvl, LogCategory cat)
        : m_level(lvl), m_cat(cat) {}

    ~LogStream() {
        Log::write(m_level, m_cat, m_ss.str());
    }

    template <typename T>
    LogStream& operator<<(const T& v) {
        m_ss << v;
        return *this;
    }

private:
    LogLevel          m_level;
    LogCategory       m_cat;
    std::ostringstream m_ss;
};

} // namespace fusionps4::debug

#define FP4_LOG(cat, lvl)                                                     \
    if (!::fusionps4::debug::Log::isEnabled((lvl), (cat))) {                  \
    } else                                                                    \
        ::fusionps4::debug::LogStream((lvl), (cat))

#define FP4_TRACE(cat) FP4_LOG((cat), ::fusionps4::debug::LogLevel::Trace)
#define FP4_DEBUG(cat) FP4_LOG((cat), ::fusionps4::debug::LogLevel::Debug)
#define FP4_INFO(cat)  FP4_LOG((cat), ::fusionps4::debug::LogLevel::Info)
#define FP4_WARN(cat)  FP4_LOG((cat), ::fusionps4::debug::LogLevel::Warn)
#define FP4_ERROR(cat) FP4_LOG((cat), ::fusionps4::debug::LogLevel::Error)
#define FP4_FATAL(cat) FP4_LOG((cat), ::fusionps4::debug::LogLevel::Fatal)

#define FP4_UNIMPLEMENTED(cat, fn)                                            \
    ::fusionps4::debug::Log::write(                                           \
        ::fusionps4::debug::LogLevel::Error, (cat),                           \
        std::string("UNIMPLEMENTED: ") + (fn))
