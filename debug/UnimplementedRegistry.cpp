#include "debug/UnimplementedRegistry.hpp"

#include <algorithm>
#include <cstdio>
#include <sstream>

namespace fusionps4::debug {

UnimplementedRegistry& UnimplementedRegistry::instance() {
    static UnimplementedRegistry r;
    return r;
}

void UnimplementedRegistry::report(const std::string& key,
                                   const std::string& args,
                                   const std::string& note) {
    std::lock_guard lock(m_mutex);
    auto& e = m_entries[key];
    if (e.key.empty()) e.key = key;
    e.count++;
    if (!args.empty()) e.lastArgs = args;
    if (!note.empty()) e.lastNote = note;
}

std::vector<UnimplementedRegistry::Entry>
UnimplementedRegistry::snapshot() const {
    std::lock_guard lock(m_mutex);
    std::vector<Entry> out;
    out.reserve(m_entries.size());
    for (const auto& [_, e] : m_entries) out.push_back(e);
    std::sort(out.begin(), out.end(),
              [](const Entry& a, const Entry& b) { return a.count > b.count; });
    return out;
}

std::string UnimplementedRegistry::formatReport() const {
    const auto snap = snapshot();
    std::ostringstream oss;
    oss << "FusionPS4 UNIMPLEMENTED report: " << snap.size()
        << " unique stubs, " << totalCount() << " total calls\n";
    oss << "---------------------------------------------------------------\n";
    for (const auto& e : snap) {
        oss << "  [" << e.count << "x] " << e.key << "\n";
        if (!e.lastArgs.empty()) oss << "        args: " << e.lastArgs << "\n";
        if (!e.lastNote.empty()) oss << "        note: " << e.lastNote << "\n";
    }
    return oss.str();
}

void UnimplementedRegistry::dumpToStderr() const {
    const auto s = formatReport();
    std::fwrite(s.data(), 1, s.size(), stderr);
    std::fflush(stderr);
}

std::size_t UnimplementedRegistry::uniqueCount() const {
    std::lock_guard lock(m_mutex);
    return m_entries.size();
}

std::uint64_t UnimplementedRegistry::totalCount() const {
    std::lock_guard lock(m_mutex);
    std::uint64_t n = 0;
    for (const auto& [_, e] : m_entries) n += e.count;
    return n;
}

} // namespace fusionps4::debug
