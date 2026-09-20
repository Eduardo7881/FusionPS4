#include "runtime/Runtime.hpp"

#include "debug/Log.hpp"

#include <SDL.h>

using fusionps4::debug::LogCategory;

namespace fusionps4::runtime {

Runtime::Runtime(config::RuntimeConfig cfg)
    : m_config(std::move(cfg)) {}

Runtime::~Runtime() {
    shutdown();
}

bool Runtime::init() {
    std::lock_guard lock(m_mutex);
    if (m_running) return true;

    FP4_INFO(LogCategory::Host) << "Runtime init";

    m_window = std::make_unique<host::window::HostWindow>();
    if (!m_window->create(m_config.window)) {
        FP4_FATAL(LogCategory::Host) << "HostWindow creation failed";
        return false;
    }

    host::window::WindowEventHandlers handlers;
    handlers.onClose  = [this] { requestQuit(); };
    handlers.onResize = [](int w, int h) {
        FP4_DEBUG(LogCategory::Host) << "window resize " << w << "x" << h;
    };
    handlers.onFocus = [](bool f) {
        FP4_TRACE(LogCategory::Host) << "focus=" << f;
    };
    handlers.onKey = [](int sc, bool down) {
        FP4_TRACE(LogCategory::Input)
            << "key scancode=" << sc << " down=" << down;
    };
    handlers.onTextInput = [](const std::string& t) {
        FP4_TRACE(LogCategory::Input) << "text=\"" << t << "\"";
    };
    m_window->setEventHandlers(std::move(handlers));

    if (!createMainProcess()) {
        return false;
    }

    m_running = true;
    FP4_INFO(LogCategory::Host) << "Runtime ready";
    return true;
}

bool Runtime::createMainProcess() {
    m_mainProcess = std::make_unique<process::PS4Process>(m_nextPid++,
                                                          "fusionps4-guest");
    if (!m_mainProcess->init()) {
        FP4_FATAL(LogCategory::Process) << "main process init failed";
        return false;
    }
    return true;
}

void Runtime::shutdown() {
    std::lock_guard lock(m_mutex);
    if (!m_running && !m_window && !m_mainProcess) return;

    FP4_INFO(LogCategory::Host) << "Runtime shutdown";

    if (m_mainProcess) {
        m_mainProcess->shutdown();
        m_mainProcess.reset();
    }
    if (m_window) {
        m_window->destroy();
        m_window.reset();
    }
    m_running = false;
}

bool Runtime::tick(double deltaSeconds) {
    (void)deltaSeconds;
    if (!m_running) return false;

    // 1) Host events (window resize / close / input).
    if (!m_window->processEvents()) {
        requestQuit();
    }

    // 2) Future: process PS4 graphics command buffers here and submit GPU
    //    work. Phase 5 wires this up through the Graphics Abstraction Layer.

    // 3) Present the frame.
    m_window->present();

    return m_running;
}

void Runtime::requestQuit() {
    m_running = false;
}

} // namespace fusionps4::runtime
