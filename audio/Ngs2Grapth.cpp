#include "audio/Ngs2Graph.hpp"

#include "audio/AudioManager.hpp"
#include "debug/Log.hpp"
#include "runtime/RuntimeContext.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

using fusionps4::debug::LogCategory;

namespace fusionps4::audio {

Ngs2Graph::Ngs2Graph() = default;

Ngs2Graph::~Ngs2Graph() {
    shutdown();
}

bool Ngs2Graph::initialize(int sampleRate, int channels) {
    std::lock_guard lock(m_mutex);
    if (m_running.load()) return true;

    auto* am = runtime::RuntimeContext::instance().audio();
    if (!am) {
        FP4_ERROR(LogCategory::Audio)
            << "Ngs2Graph: no AudioManager bound to RuntimeContext";
        return false;
    }

    // Open a single mono-per-source port at the graph's rate. The guest
    // is unaware of this port; it is the graph's private output.
    m_portId = am->openPort(channels, sampleRate, /*granularity*/ 512);
    if (m_portId < 0) {
        FP4_ERROR(LogCategory::Audio)
            << "Ngs2Graph: AudioManager::openPort failed";
        return false;
    }

    m_sampleRate = sampleRate;
    m_channels   = channels;
    m_running.store(true);
    m_thread = std::thread([this] { mixerThread(); });

    FP4_INFO(LogCategory::Audio)
        << "Ngs2Graph initialized: " << sampleRate << " Hz, " << channels
        << " ch, port=" << m_portId;
    return true;
}

void Ngs2Graph::shutdown() {
    if (!m_running.exchange(false)) {
        if (m_thread.joinable()) m_thread.join();
        return;
    }
    if (m_thread.joinable()) m_thread.join();
    std::lock_guard lock(m_mutex);
    if (m_portId >= 0) {
        auto* am = runtime::RuntimeContext::instance().audio();
        if (am) am->closePort(m_portId);
        m_portId = -1;
    }
    m_voices.clear();
}

std::uint32_t Ngs2Graph::addVoice() {
    std::lock_guard lock(m_mutex);
    Ngs2Voice v;
    v.id = m_nextVoiceId++;
    m_voices.push_back(std::move(v));
    return m_voices.back().id;
}

bool Ngs2Graph::removeVoice(std::uint32_t id) {
    std::lock_guard lock(m_mutex);
    for (auto it = m_voices.begin(); it != m_voices.end(); ++it) {
        if (it->id == id) { m_voices.erase(it); return true; }
    }
    return false;
}

Ngs2Voice* Ngs2Graph::voice(std::uint32_t id) {
    for (auto& v : m_voices) if (v.id == id) return &v;
    return nullptr;
}

bool Ngs2Graph::playVoice(std::uint32_t id, bool loop) {
    std::lock_guard lock(m_mutex);
    auto* v = voice(id);
    if (!v) return false;
    if (v->pcm.empty()) return false;
    v->playing = true;
    v->loop    = loop;
    if (v->cursor >= v->pcm.size() / 2) v->cursor = 0;
    return true;
}

bool Ngs2Graph::stopVoice(std::uint32_t id) {
    std::lock_guard lock(m_mutex);
    auto* v = voice(id);
    if (!v) return false;
    v->playing = false;
    v->cursor  = 0;
    return true;
}

bool Ngs2Graph::setVoicePcm(std::uint32_t id, const void* data,
                            std::size_t frames) {
    std::lock_guard lock(m_mutex);
    auto* v = voice(id);
    if (!v || !data) return false;
    const auto samples = frames * static_cast<std::size_t>(m_channels);
    v->pcm.resize(samples);
    std::memcpy(v->pcm.data(), data, samples * sizeof(std::int16_t));
    v->cursor = 0;
    return true;
}

bool Ngs2Graph::setVoiceGain(std::uint32_t id, float gain) {
    std::lock_guard lock(m_mutex);
    auto* v = voice(id);
    if (!v) return false;
    v->gain = std::clamp(gain, 0.0f, 8.0f);
    return true;
}

bool Ngs2Graph::setVoicePan(std::uint32_t id, float pan) {
    std::lock_guard lock(m_mutex);
    auto* v = voice(id);
    if (!v) return false;
    pan = std::clamp(pan, -1.0f, 1.0f);
    // Constant-power pan law.
    const float theta = (pan + 1.0f) * 0.25f * 3.14159265f;
    v->panLeft  = std::cos(theta);
    v->panRight = std::sin(theta);
    return true;
}

void Ngs2Graph::setMasterGain(float gain) {
    m_masterGain.store(std::clamp(gain, 0.0f, 4.0f));
}

float Ngs2Graph::masterGain() const {
    return m_masterGain.load();
}

std::size_t Ngs2Graph::activeVoiceCount() const {
    std::lock_guard lock(m_mutex);
    std::size_t n = 0;
    for (const auto& v : m_voices) if (v.playing) ++n;
    return n;
}

void Ngs2Graph::mixerThread() {
    constexpr std::size_t kBlockFrames = 512;

    std::vector<std::int16_t> mixBuf(kBlockFrames * 2, 0);
    std::vector<std::int16_t> tmpVoices(kBlockFrames * 2, 0);

    while (m_running.load(std::memory_order_acquire)) {
        std::fill(mixBuf.begin(), mixBuf.end(), static_cast<std::int16_t>(0));

        int portId = -1;
        {
            std::lock_guard lock(m_mutex);
            portId = m_portId;

            for (auto& v : m_voices) {
                if (!v.playing || v.pcm.empty()) continue;

                const auto totalFrames = v.pcm.size() / 2;
                std::fill(tmpVoices.begin(), tmpVoices.end(),
                          static_cast<std::int16_t>(0));

                for (std::size_t i = 0; i < kBlockFrames; ++i) {
                    if (v.cursor >= totalFrames) {
                        if (v.loop) v.cursor = 0;
                        else { v.playing = false; break; }
                    }
                    tmpVoices[i * 2]     = v.pcm[v.cursor * 2];
                    tmpVoices[i * 2 + 1] = v.pcm[v.cursor * 2 + 1];
                    ++v.cursor;
                }

                for (std::size_t i = 0; i < kBlockFrames; ++i) {
                    const float gl = v.gain * v.panLeft;
                    const float gr = v.gain * v.panRight;
                    mixBuf[i * 2] = static_cast<std::int16_t>(std::clamp(
                        static_cast<int>(mixBuf[i * 2]) +
                        static_cast<int>(tmpVoices[i * 2] * gl),
                        -32768, 32767));
                    mixBuf[i * 2 + 1] = static_cast<std::int16_t>(std::clamp(
                        static_cast<int>(mixBuf[i * 2 + 1]) +
                        static_cast<int>(tmpVoices[i * 2 + 1] * gr),
                        -32768, 32767));
                }
            }

            const float mg = m_masterGain.load();
            if (mg != 1.0f) {
                for (auto& s : mixBuf) {
                    s = static_cast<std::int16_t>(
                        std::clamp(static_cast<int>(s * mg), -32768, 32767));
                }
            }
        }

        auto* am = runtime::RuntimeContext::instance().audio();
        if (am && portId >= 0) {
            am->output(portId, mixBuf.data(), kBlockFrames);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
} // namespace fusionps4::audio
