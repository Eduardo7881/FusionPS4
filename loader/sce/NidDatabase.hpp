#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

namespace fusionps4::loader::sce {

struct NidEntry {
    std::uint64_t nid = 0;
    std::string   library;
    std::string   function;
};

// NID → (library, function) mapping. Loaded from a text file; falls back
// to a built-in table covering the functions FusionPS4 already implements.
class NidDatabase {
public:
    static NidDatabase& instance();

    // Loads entries from `path`. Returns false on IO error.
    // Silently ignores duplicate keys (last wins).
    bool loadFromFile(const std::string& path);

    // Loads the built-in fallback, covering only the functions whose NID
    // we can compute with computeLegacy(). Called automatically on first
    // access; explicit calls are idempotent.
    void loadBuiltinFallback();

    // Returns nullptr if the NID is unknown.
    const NidEntry* lookup(std::uint64_t nid) const;

    std::size_t size() const;

private:
    NidDatabase();
    void insert(std::uint64_t nid, std::string lib, std::string fn);

    mutable std::mutex                    m_mutex;
    std::unordered_map<std::uint64_t, NidEntry> m_entries;
    bool                                  m_builtinLoaded = false;
};

} // namespace fusionps4::loader::sce
