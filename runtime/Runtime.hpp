#pragma once

#include "config/RuntimeConfig.hpp"
#include "host/window/HostWindow.hpp"
#include "runtime/process/PS4Process.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

namespace fusionps4::runtime {

// The Runtime is the central "owned by the host" object.
// It owns the HostWindow, the resource managers, and the PS4Process.
// The guest application never talks to SDL, filesystem, sockets or audio
// backends directly; every path funnels through here.
class Runtime {
public:
    explicit Runtime(config::RuntimeConfig cfg);
    ~Runtime();

    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;

    bool init();
    void shutdown();

    // Called once per host frame.
    // Order: host events -> runtime tick -> PS4 threads are already running
    // as native pthreads -> renderer present is called by the graphics layer.
    bool tick(double deltaSeconds);

    void requestQuit();

    bool isRunning() const { return m_running; }

    host::window::HostWindow&      window()      { return *m_window; }
    process::PS4Process&           mainProcess() { return *m_mainProcess; }

    const config::RuntimeConfig& config() const { return m_config; }
    // Optional: called from Application after the process is loaded. If
    // FUSIONPS4_ISOLATED=1 is set in the environment, Runtime::init()
    // invokes this automatically.
    bool startGuestIfIsolated();

private:
    bool createMainProcess();

    config::RuntimeConfig                            m_config;
    std::unique_ptr<host::window::HostWindow>        m_window;
    std::unique_ptr<process::PS4Process>             m_mainProcess;
    process::ProcessId                               m_nextPid = 1;
    bool                                             m_running = false;
    std::mutex                                       m_mutex;
};

} // namespace fusionps4::runtime
