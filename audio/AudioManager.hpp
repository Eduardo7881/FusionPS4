#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace fusionps4::audio {

// One SCE audio port. The guest writes PCM frames into sceAudioOutOutput;
// we push them onto the SDL audio device via SDL_QueueAudio. Ports are
// independent of channels: several ports can be mixed because the SDL
// device has a single stream and SDL_QueueAudio simply appends.
struct Port {
    int            id        = 0;
    int            channels  = 2;     // 1 or 2
    int            sampleRate= 48000;
    int            granularity = 0;   // bytes per output block
    bool           open      = true;
};

class AudioManager {
public:
    static constexpr int kMaxPorts = 32;

    AudioManager() = default;
    ~AudioManager();

    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    bool init(int sampleRate = 48000);
    void shutdown();

    // Open a new port. `channels` 1 (mono) or 2 (stereo). `granularity`
    // is the number of samples per output block, as PS4 expects.
    // Returns a port id >= 0 on success, -1 on error.
    int openPort(int channels, int sampleRate, int granularity);

    bool closePort(int portId);

    // Push a block of interleaved PCM samples (int16). `numSamples` counts
    // frames (i.e. per-channel samples). Returns true on success.
    bool output(int portId, const void* data, std::size_t numSamples);

    // Master volume in [0, 255]; 128 is unity.
    void setMasterVolume(int left, int right);

private:
    std::mutex             m_mutex;
    std::vector<Port>      m_ports;
    int                    m_nextPortId = 1;
    std::atomic<int>       m_leftVolume  {128};
    std::atomic<int>       m_rightVolume {128};

    struct SdlAudio;
    std::unique_ptr<SdlAudio> m_sdl;
};

} // namespace fusionps4::audio
