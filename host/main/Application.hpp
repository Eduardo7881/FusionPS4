#pragma once

#include "config/RuntimeConfig.hpp"
#include "runtime/Runtime.hpp"

#include <memory>

namespace fusionps4::host {

class Application {
public:
    Application();
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    int run(int argc, char** argv);

private:
    bool bootstrap(int argc, char** argv);
    void shutdown();
    void mainLoop();

    config::RuntimeConfig             m_config;
    std::unique_ptr<runtime::Runtime> m_runtime;
    bool                              m_initialized = false;
};

} // namespace fusionps4::host
