#include "host/main/Application.hpp"

#include "debug/Log.hpp"

#include <SDL.h>
#include <chrono>
#include <cstring>

using fusionps4::debug::LogCategory;

namespace fusionps4::host {

Application::Application()  = default;

Application::~Application() {
    shutdown();
}

bool Application::bootstrap(int argc, char** argv) {
    debug::Log::init();
    m_config = config::loadFromEnvironment();

    // Allow a positional argument to override the window title for debugging.
    if (argc > 1) {
        m_config.window.title = argv[1];
    }
    if (!m_config.logFile.empty()) {
        debug::Log::setOutputFile(m_config.logFile);
    }

    FP4_INFO(LogCategory::Host)
        << "FusionPS4 " << "0.1.0"
        << " starting; title=\"" << m_config.window.title << "\"";

    // SDL subsystems that the runtime needs across the whole app.
    if (SDL_Init(SDL_INIT_EVENTS) != 0) {
        FP4_FATAL(LogCategory::Host)
            << "SDL_Init(EVENTS) failed: " << SDL_GetError();
        return false;
    }

    m_runtime = std::make_unique<runtime::Runtime>(m_config);
    if (!m_runtime->init()) {
        FP4_FATAL(LogCategory::Host) << "Runtime init failed";
        return false;
    }

    m_initialized = true;
    return true;
}

void Application::shutdown() {
    if (!m_initialized) return;

    if (m_runtime) {
        m_runtime->shutdown();
        m_runtime.reset();
    }
    SDL_Quit();
    FP4_INFO(LogCategory::Host) << "FusionPS4 stopped";
    debug::Log::shutdown();
    m_initialized = false;
}

void Application::mainLoop() {
    using clock = std::chrono::steady_clock;
    auto last = clock::now();

    while (m_runtime->isRunning()) {
        const auto now = clock::now();
        const double dt = std::chrono::duration<double>(now - last).count();
        last = now;

        if (!m_runtime->tick(dt)) break;
    }
}

int Application::run(int argc, char** argv) {
    if (!bootstrap(argc, argv)) {
        shutdown();
        return 1;
    }
    mainLoop();
    shutdown();
    return 0;
}

} // namespace fusionps4::host
