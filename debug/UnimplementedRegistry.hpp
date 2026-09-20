#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace fusionps4::debug {

// Records every UNIMPLEMENTED report produced by the runtime. The point is
// to make missing functionality *visible*: a game that hits twenty stubs
// should produce a report listing them, not silently misbehave.
//
// The key is "<library>::<function>"; the value is the number of times it
// was reported plus the last argument summary the caller supplied.
class UnimplementedRegistry {
public:
    struct Entry {
        std::string   key;
        std::uint64_t count    = 0;
        std::string   lastArgs;
        std::string   lastNote;
    };

    static UnimplementedRegistry& instance();

    // Record one call. `args` and `note` are optional but strongly
    // recommended: without them the report is just a name.
    void report(const std::string& key,
                const std::string& args,
                const std::string& note);

    std::vector<Entry> snapshot() const;

    // Multi-line textual report, sorted by descending count.
    std::string formatReport() const;

    // Writes the report to stderr and, if set, to a log file. Clears
    // nothing.
    void dumpToStderr() const;

    std::size_t uniqueCount() const;
    std::uint64_t totalCount() const;

private:
    UnimplementedRegistry() = default;

    mutable std::mutex                    m_mutex;
    std::unordered_map<std::string, Entry> m_entries;
};

} // namespace fusionps4::debug
