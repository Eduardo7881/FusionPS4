#include "audio/AudioManager.hpp"

#include "debug/Log.hpp"

#include <SDL.h>
#include <algorithm>
#include <cstring>

using fusionps4::debug::LogCategory;

namespace fusionps4::audio {

struct AudioManager::SdlAudio {
    SDL_AudioDeviceID  device = 0;
    int                sampleRate = 48000;
};

AudioManager::~AudioManager() {
    shutdown();
}

bool AudioManager::init(int sampleRate) {
    m_sdl = std::make_unique<SdlAudio>();

    SDL_AudioSpec want{};
    want.freq     = sampleRate;
    want.format   = AUDIO_S16SYS;
    want.channels = 2;
    want.samples  = 1024;
    want.callback = nullptr;   // we use SDL_QueueAudio

    SDL_AudioSpec have{};
    m_sdl->device = SDL_OpenAudioDevice(nullptr, 0, &want, &have,
                                        SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (m_sdl->device == 0) {
        FP4_ERROR(LogCategory::Audio)
            << "SDL_OpenAudioDevice failed: " << SDL_GetError();
        m_sdl.reset();
        return false;
    }
    m_sdl->sampleRate = have.freq;

    SDL_PauseAudioDevice(m_sdl->device, 0);
    FP4_INFO(LogCategory::Audio)
        << "SDL audio device opened: " << have.freq << " Hz, "
        << static_cast<int>(have.channels) << " ch";
    return true;
}

void AudioManager::shutdown() {
    std::lock_guard lock(m_mutex);
    m_ports.clear();
    if (m_sdl && m_sdl->device) {
        SDL_CloseAudioDevice(m_sdl->device);
        m_sdl->device = 0;
    }
    m_sdl.reset();
}

int AudioManager::openPort(int channels, int sampleRate, int granularity) {
    if (channels != 1 && channels != 2) {
        FP4_ERROR(LogCategory::Audio)
            << "unsupported channel count " << channels;
        return -1;
    }
    if (sampleRate <= 0) sampleRate = m_sdl ? m_sdl->sampleRate : 48000;
    if (granularity <= 0) {
        FP4_ERROR(LogCategory::Audio)
            << "invalid granularity " << granularity;
        return -1;
    }

    std::lock_guard lock(m_mutex);
    for (const auto& p : m_ports) {
        if (p.open) {
            FP4_ERROR(LogCategory::Audio)
                << "port already open (id=" << p.id << "); only one port per "
                << "process is supported in this phase";
            return -1;
        }
    }

    Port p;
    p.id          = m_nextPortId++;
    p.channels    = channels;
    p.sampleRate  = sampleRate;
    p.granularity = granularity;
    p.open        = true;
    m_ports.push_back(p);

    FP4_INFO(LogCategory::Audio)
        << "port " << p.id << " opened: ch=" << channels
        << " rate=" << sampleRate << " gran=" << granularity;
    return p.id;
}

bool AudioManager::closePort(int portId) {
    std::lock_guard lock(m_mutex);
    for (auto& p : m_ports) {
        if (p.id == portId) {
            p.open = false;
            FP4_INFO(LogCategory::Audio) << "port " << portId << " closed";
            return true;
        }
    }
    return false;
}

bool AudioManager::output(int portId, const void* data, std::size_t numSamples) {
    if (!data || numSamples == 0) return false;
    if (!m_sdl || !m_sdl->device) return false;

    int channels = 2;
    {
        std::lock_guard lock(m_mutex);
        bool found = false;
        for (const auto& p : m_ports) {
            if (p.id == portId && p.open) {
                channels = p.channels;
                found = true;
                break;
            }
        }
        if (!found) return false;
    }

    // Convert mono -> stereo in a scratch buffer when needed.
    if (channels == 2) {
        const auto bytes = numSamples * 2 * sizeof(std::int16_t);
        SDL_QueueAudio(m_sdl->device, data, static_cast<Uint32>(bytes));
    } else {
        std::vector<std::int16_t> stereo(numSamples * 2);
        const auto* mono = static_cast<const std::int16_t*>(data);
        for (std::size_t i = 0; i < numSamples; ++i) {
            stereo[i * 2]     = mono[i];
            stereo[i * 2 + 1] = mono[i];
        }
        SDL_QueueAudio(m_sdl->device, stereo.data(),
                       static_cast<Uint32>(stereo.size() * sizeof(std::int16_t)));
    }
    return true;
}

void AudioManager::setMasterVolume(int left, int right) {
    m_leftVolume.store(std::clamp(left, 0, 255));
    m_rightVolume.store(std::clamp(right, 0, 255));
}

} // namespace fusionps4::audio
