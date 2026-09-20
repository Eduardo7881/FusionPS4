#include "runtime/RuntimeContext.hpp"

#include "debug/Log.hpp"

using fusionps4::debug::LogCategory;

namespace fusionps4::runtime {

RuntimeContext& RuntimeContext::instance() {
    static RuntimeContext ctx;
    return ctx;
}

void RuntimeContext::bind(process::PS4Process*   process,
                          input::InputManager*   input,
                          audio::AudioManager*   audio,
                          network::NetworkPolicy* network) {
    std::lock_guard lock(m_mutex);
    m_process = process;
    m_input   = input;
    m_audio   = audio;
    m_network = network;
    FP4_INFO(LogCategory::Host) << "RuntimeContext bound";
}

void RuntimeContext::unbind() {
    std::lock_guard lock(m_mutex);
    m_process = nullptr;
    m_input   = nullptr;
    m_audio   = nullptr;
    m_network = nullptr;
}

process::PS4Process* RuntimeContext::process() const {
    std::lock_guard lock(m_mutex);
    return m_process;
}

input::InputManager* RuntimeContext::input() const {
    std::lock_guard lock(m_mutex);
    return m_input;
}

audio::AudioManager* RuntimeContext::audio() const {
    std::lock_guard lock(m_mutex);
    return m_audio;
}

network::NetworkPolicy* RuntimeContext::network() const {
    std::lock_guard lock(m_mutex);
    return m_network;
}

process::PS4Process& RuntimeContext::requireProcess() const {
    auto* p = process();
    if (!p) {
        FP4_FATAL(LogCategory::Sce)
            << "SCE stub executed without a bound PS4Process";
        std::abort();
    }
    return *p;
}

input::InputManager& RuntimeContext::requireInput() const {
    auto* p = input();
    if (!p) {
        FP4_FATAL(LogCategory::Sce)
            << "SCE stub executed without a bound InputManager";
        std::abort();
    }
    return *p;
}

audio::AudioManager& RuntimeContext::requireAudio() const {
    auto* p = audio();
    if (!p) {
        FP4_FATAL(LogCategory::Sce)
            << "SCE stub executed without a bound AudioManager";
        std::abort();
    }
    return *p;
}

network::NetworkPolicy& RuntimeContext::requireNetwork() const {
    auto* p = network();
    if (!p) {
        FP4_FATAL(LogCategory::Sce)
            << "SCE stub executed without a bound NetworkPolicy";
        std::abort();
    }
    return *p;
}

} // namespace fusionps4::runtime
