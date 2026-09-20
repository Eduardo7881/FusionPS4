#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

namespace fusionps4::debug {

// Frame-scoped timing. The Runtime marks a frame begin/end; subsystems
// open small scopes inside a frame. At end of frame the accumulated
// durations are stored in a rolling window that can be queried for a
// breakdown report.
class Profiler {
public:
    static Profiler& instance();

    void beginFrame();
    void endFrame();

    // Named scope; RAII.
    class Scope {
    public:
        Scope(Profiler& p, const char* label);
        ~Scope();
    private:
        Profiler&   m_p;
        const char* m_label;
        std::uint64_t m_startNs;
    };

    // Snapshot in the form: {label, avgMs, lastMs}. `count` is the number
    // of frames that contributed.
    struct Entry {
        std::string  label;
        double       avgMs  = 0.0;
        double       lastMs = 0.0;
    };
    std::vector<Entry> snapshot() const;

    std::string formatReport() const;

    // Enable/disable collection. When disabled, begin/end are cheap
    // no-ops. Disabled by default; enable with FUSIONPS4_PROFILE=1.
    void setEnabled(bool on);
    bool enabled() const;

private:
    Profiler() = default;

    struct Sample {
        std::uint64_t totalNs = 0;
        std::uint64_t lastNs  = 0;
    };

    mutable std::mutex                    m_mutex;
    std::unordered_map<std::string, Sample> m_samples;
    std::atomic<bool>                     m_enabled{false};
    std::uint64_t                         m_frameStartNs = 0;
};

} // namespace fusionps4::debug
