#pragma once

#include <cstdint>
#include <cstdlib>
#include <string>

namespace fusionps4::config {

enum class WindowMode {
    Windowed,
    Fullscreen,
    FullscreenExclusive,
};

struct WindowConfig {
    std::string title   = "FusionPS4";
    int         width   = 1280;
    int         height  = 720;
    WindowMode  mode    = WindowMode::Windowed;
    bool        vsync   = true;
    bool        resizable = true;
};

struct RuntimeConfig {
    WindowConfig window;
    std::string  vfsRoot      = "virtual_fs";
    std::string  appsRoot     = "apps";
    std::string  configRoot   = "config";
    std::string  logFile;               // vazio => apenas stderr
    uint64_t     guestVaBase  = 0;
    uint64_t     guestVaSize  = 0;
    bool         debugHandles = false;
};

inline RuntimeConfig loadFromEnvironment() {
    RuntimeConfig cfg;

    if (const char* t = std::getenv("FUSIONPS4_TITLE"))  cfg.window.title  = t;
    if (const char* w = std::getenv("FUSIONPS4_WIDTH"))  cfg.window.width  = std::atoi(w);
    if (const char* h = std::getenv("FUSIONPS4_HEIGHT")) cfg.window.height = std::atoi(h);
    if (const char* l = std::getenv("FUSIONPS4_LOGFILE")) cfg.logFile     = l;
    if (const char* v = std::getenv("FUSIONPS4_VFS"))     cfg.vfsRoot     = v;

    if (const char* fs = std::getenv("FUSIONPS4_FULLSCREEN")) {
        const std::string s = fs;
        if (s == "1" || s == "true" || s == "yes") {
            cfg.window.mode = WindowMode::Fullscreen;
        }
    }
    return cfg;
}

} // namespace fusionps4::config
