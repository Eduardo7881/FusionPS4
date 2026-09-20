#include "debug/Profiler.hpp"

#include <chrono>
#include <cstdlib>
#include <sstream>

namespace fusionps4::debug {

namespace {

std::uint64_t nowNs() {
    using namespace std::chrono;
    return static_cast<std::uint64_t>(
        duration_cast<nanoseconds>(steady_clock::now().time_since_epoch())
            .count());
}

} // namespace

Profiler& Profiler::instance() {
    static Profiler p;
    if (const char* env = std::getenv("FUSIONPS4_PROFILE")) {
        if (*env == '1') p.m_enabled.store(true);
    }
    return p;
}

void Profiler::setEnabled(bool on) { m_enabled.store(on); }
bool Profiler::enabled() const      { return m_enabled.load(); }

void Profiler::beginFrame() {
    if (!m_enabled.load()) return;
    m_frameStartNs = nowNs();
}

void Profiler::endFrame() {
    if (!m_enabled.load()) return;
    const auto dt = nowNs() - m_frameStartNs;
    std::lock_guard lock(m_mutex);
    auto& s = m_samples["frame"];
    s.totalNs += dt;
    s.lastNs   = dt;
}

Profiler::Scope::Scope(Profiler& p, const char* label)
    : m_p(p), m_label(label), m_startNs(0) {
    if (p.enabled()) m_startNs = nowNs();
}

Profiler::Scope::~Scope() {
    if (!m_p.enabled() || m_startNs == 0) return;
    const auto dt = nowNs() - m_startNs;
    std::lock_guard lock(m_p.m_mutex);
    auto& s = m_p.m_samples[m_label];
    s.totalNs += dt;
    s.lastNs   = dt;
}

std::vector<Profiler::Entry> Profiler::snapshot() const {
    std::lock_guard lock(m_mutex);
    // No frame counter in the profiler: we approximate "avg" as total /
    // (total / last) — which reduces to lastNs when only one sample
    // contributed. Instead, we track the frame count in m_samples["frame"]
    // when present.
    std::uint64_t frames = 0;
    if (auto it = m_samples.find("frame"); it != m_samples.end()) {
        frames = it->second.lastNs ? it->second.totalNs / it->second.lastNs : 0;
    }
    if (frames == 0) frames = 1;

    std::vector<Entry> out;
    out.reserve(m_samples.size());
    for (const auto& [k, v] : m_samples) {
        Entry e;
        e.label  = k;
        e.lastMs = v.lastNs / 1e6;
        e.avgMs  = (v.totalNs / static_cast<double>(frames)) / 1e6;
        out.push_back(std::move(e));
    }
    return out;
}

std::string Profiler::formatReport() const {
    auto snap = snapshot();
    std::ostringstream oss;
    oss << "FusionPS4 profile:\n";
    for (const auto& e : snap) {
        oss << "  " << e.label << ": avg=" << e.avgMs << " ms last="
            << e.lastMs << " ms\n";
    }
    return oss.str();
}

} // namespace fusionps4::debug
