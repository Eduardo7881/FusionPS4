#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace fusionps4::audio {

// A single audio voice within an NGS2 graph. Voices are PCM buffers with
// a gain, pan and playback state. The graph mixes them at a fixed sample
// rate (48 kHz, stereo) into the runtime's AudioManager.
struct Ngs2Voice {
    std::uint32_t id = 0;
    std::vector<std::int16_t> pcm;      // interleaved stereo, 48 kHz
    std::size_t   cursor    = 0;        // current playback position (frames)
    float         gain      = 1.0f;     // [0, 4] typical
    float         panLeft   = 1.0f;
    float         panRight  = 1.0f;
    bool          playing   = false;
    bool          loop      = false;
};

// NGS2 mixer graph. Owns a dedicated audio port in the AudioManager, a
// worker thread that produces mixed frames at a rate matching SDL's queue
// drain, and a set of voices. This is a real mixer, not a passthrough.
//
// NGS2's public API exposes dozens of handle types (racks, nodes, ports,
// voices). We map them all onto this single class: whatever the guest
// creates, ends up as a voice or as a parameter change on the mixer.
class Ngs2Graph {
public:
    Ngs2Graph();
    ~Ngs2Graph();

    Ngs2Graph(const Ngs2Graph&) = delete;
    Ngs2Graph& operator=(const Ngs2Graph&) = delete;

    bool initialize(int sampleRate = 48000, int channels = 2);
    void shutdown();

    // ---- voices ---------------------------------------------------------
    std::uint32_t addVoice();
    bool          removeVoice(std::uint32_t id);
    Ngs2Voice*    voice(std::uint32_t id);

    // Starts/stops playback of a voice. When `loop` is true the cursor
    // wraps to 0 on end-of-buffer instead of stopping.
    bool playVoice(std::uint32_t id, bool loop);
    bool stopVoice(std::uint32_t id);

    // Queue PCM into a voice. Data replaces the previous buffer; the
    // voice's playback rate is fixed at the graph's sample rate.
    bool setVoicePcm(std::uint32_t id, const void* data, std::size_t frames);

    bool setVoiceGain(std::uint32_t id, float gain);
    bool setVoicePan(std::uint32_t id, float pan);   // [-1, 1]

    // ---- global ---------------------------------------------------------
    void setMasterGain(float gain);
    float masterGain() const;

    std::size_t activeVoiceCount() const;

private:
    void mixerThread();

    mutable std::mutex             m_mutex;
    std::vector<Ngs2Voice>         m_voices;
    std::uint32_t                  m_nextVoiceId = 1;

    int                            m_sampleRate = 48000;
    int                            m_channels   = 2;
    int                            m_portId     = -1;

    std::atomic<float>             m_masterGain{1.0f};
    std::atomic<bool>              m_running{false};
    std::thread                    m_thread;
};

} // namespace fusionps4::audio
