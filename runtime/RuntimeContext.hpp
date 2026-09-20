#pragma once

#include <mutex>

namespace fusionps4::runtime::process { class PS4Process; }
namespace fusionps4::input                { class InputManager; }
namespace fusionps4::audio                { class AudioManager; }
namespace fusionps4::network              { class NetworkPolicy; }

namespace fusionps4::runtime {

// Process-global access point used by SCE stubs to reach the runtime's
// managers without threading them through every function signature. Because
// FusionPS4 hosts a single guest process at a time, a singleton is a correct
// model — matching how real kernels expose per-process state via CPU-local
// pointers.
//
// Set once during Runtime::init(); cleared on shutdown.
class RuntimeContext {
public:
    static RuntimeContext& instance();

    void bind(process::PS4Process*  process,
              input::InputManager*  input,
              audio::AudioManager*  audio,
              network::NetworkPolicy* network);
    void unbind();

    process::PS4Process*    process() const;
    input::InputManager*    input()   const;
    audio::AudioManager*    audio()   const;
    network::NetworkPolicy* network() const;

    // Convenience accessors that assert non-null; call only from stubs that
    // are guaranteed to run inside a live process.
    process::PS4Process&    requireProcess() const;
    input::InputManager&    requireInput()   const;
    audio::AudioManager&    requireAudio()   const;
    network::NetworkPolicy& requireNetwork() const;

private:
    RuntimeContext() = default;

    mutable std::mutex      m_mutex;
    process::PS4Process*    m_process = nullptr;
    input::InputManager*    m_input   = nullptr;
    audio::AudioManager*    m_audio   = nullptr;
    network::NetworkPolicy* m_network = nullptr;
};

} // namespace fusionps4::runtime
